#include "MidiUSB.h"
#include "PolarisShared.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
// Wedge watchdog tuning. The detector must never punish normal playing:
// silence only starts *probing*; only proof of a dead device forces a replug.
constexpr uint32_t kNudgeSilenceMs        = 1200;  // silence before the one-shot clear-halt nudge
constexpr uint32_t kNudgeBurstMsgs        = 24;    // burst size that makes the stop "suspicious"
constexpr uint32_t kProbeSilenceMs        = 1500;  // silence before EP0 liveness probing starts
constexpr uint32_t kProbeFastMs           = 500;   // probe interval right after the stream stops
constexpr uint32_t kProbeSlowMs           = 5000;  // relaxed interval once the device proves alive
constexpr uint16_t kProbeFastCount        = 6;     // successes before relaxing the interval
constexpr uint8_t  kProbeFailLimit        = 2;     // consecutive failures = device firmware dead
// EP0 NAK budget while probing: 2^6-1 = 63 NAKs (a few ms). The library
// default is 32K-1 NAKs / 5s timeout, which would stall this task (and
// starve core 1) when the device is wedged. 63 NAKs is still far more than
// a healthy device ever needs for GET_STATUS.
constexpr uint8_t  kProbeNakPower         = 6;
constexpr uint32_t kPostRecoveryDiscardMs = 400;   // flush the stale-packet burst after replug
constexpr uint32_t kReEnumRetryMs         = 6000;  // re-enum stalled -> retry replug
constexpr uint32_t kReEnumRetrySlowMs     = 30000; // back off after repeated stalls
constexpr uint8_t  kReEnumFastRetries     = 5;
}  // namespace

MidiUSB::MidiUSB(int intPin, int taskCore, UBaseType_t taskPrio)
: intPin_(intPin), taskCore_(taskCore), taskPrio_(taskPrio) {}

bool MidiUSB::begin(MidiHandler* handler) {
  handler_ = handler;

  // INT# pin, observed as a level from the polling task. Deliberately NOT
  // attached as a GPIO interrupt - see the class comment in MidiUSB.h.
  pinMode(intPin_, INPUT_PULLUP);

  // NOTE: usb_.Init() is deliberately NOT called here. On a cold power-up the
  // MAX3421E crystal oscillator often hasn't stabilised yet (OSCOKIRQ not
  // asserted), so Init() returns -1; doing it on the boot path meant a single
  // early failure left USB MIDI dead until a manual reboot (warm reboots
  // "worked" only because the chip was already running). It also blocked the
  // boot sequence. Init now runs inside the polling task with retries.
  //
  // Background polling task (pinned core optional). UHS2 device enumeration
  // runs on this stack; 4096 bytes is marginal with 32-bit frames.
  BaseType_t ok = xTaskCreatePinnedToCore(
      taskThunk, "uhs-midi", 6144, this, taskPrio_, &task_, taskCore_);
  return ok == pdPASS;
}

void MidiUSB::end() {
  if (task_) {
    vTaskDelete(task_);
    task_ = nullptr;
  }
}

void MidiUSB::taskThunk(void* arg) {
  static_cast<MidiUSB*>(arg)->taskLoop();
}

void MidiUSB::taskLoop() {
  // Bring up the USB host core here rather than in begin(): on a cold boot the
  // MAX3421E oscillator can take a moment to stabilise, so the first Init()
  // attempts may return -1. Retry with a short delay so the keyboard still
  // enumerates instead of USB MIDI being dead until reboot. The chip is
  // present, just not ready yet; a handful of 100ms retries covers it. If it
  // never comes up (no MAX3421E fitted) we give up after the budget and exit.
  constexpr int kInitAttempts = 20;     // ~2s worst case before giving up
  bool initialized = false;
  for (int attempt = 1; attempt <= kInitAttempts; attempt++) {
    if (usb_.Init() == 0) {
      initialized = true;
      Serial.printf("[usbmidi] USB host init OK (attempt %d)\n", attempt);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (!initialized) {
    Serial.println("[usbmidi] USB host init failed (no MAX3421E?) - task exiting");
    task_ = nullptr;
    vTaskDelete(nullptr);
    return;  // not reached
  }

  // The MAX3421E INT# line is wired to GPIO2, which is also an ESP32 boot
  // strapping pin. UHS2's Init() enables the 1kHz SOF frame interrupt
  // (bmFRAMEIE) on top of connection detection, and INT# is open-drain
  // active-low level mode, so in host mode the line is re-asserted every frame
  // and effectively sits LOW continuously. The MAX3421E is NOT reset by the
  // ESP32's EN button, so on a warm reset it keeps holding GPIO2 low while the
  // boot ROM samples the strapping pins - and the chip comes up in UART
  // download mode instead of rebooting. (A full power cycle resets the
  // MAX3421E too, releasing GPIO2, which is why only EN was affected.)
  //
  // We poll Task() at 1kHz and never use the frame interrupt - UHS2's
  // IntHandler ignores bmFRAMEIRQ; only connection detection drives anything -
  // so drop bmFRAMEIE and keep only bmCONDETIE. INT# is then deasserted except
  // during brief connect/disconnect events, leaving GPIO2 high at reset so EN
  // reboots normally. (USB::Task() reads the bmFRAMEIRQ status flag directly,
  // which hardware still sets regardless of HIEN, so enumeration timing is
  // unaffected.)
  usb_.regWr(rHIEN, bmCONDETIE);

  uint32_t boot = millis();
  lastDiagMs_ = boot;
  uint32_t detachedSinceMs = boot;  // for the cold-boot re-probe watchdog below
  uint32_t runningSinceMs = 0;      // when the device first reached RUNNING
  bool reEnumDone = false;          // one-shot cold-boot re-enumeration latch
  for (;;) {
    diagLoops_++;
    // 1kHz service rate, like the proven polled design of the 1.x firmware.
    // The delay also guarantees this (priority 4) task can never starve
    // core 1, whatever the INT# line or the controller do.
    vTaskDelay(pdMS_TO_TICKS(1));
    uint32_t now = millis();

    // Service the MAX3421E IRQ flags unconditionally rather than gating on the
    // INT# pin level. vbusState - which is what actually gates enumeration - is
    // refreshed only by busprobe(), and after Init() that runs solely when the
    // connection-detect IRQ (CONDETIRQ) is serviced here. On a cold boot the
    // keyboard's D+ pull-up appears AFTER Init()'s initial busprobe, so
    // detection depends entirely on catching that IRQ. CONDETIRQ is a latched
    // HIRQ status bit, but GPIO2's deasserted "high" sits in the undefined
    // input zone (open-drain + weak pull-up + the devkit LED), so digitalRead()
    // can miss the brief active-low window and the connect is never serviced -
    // vbusState stays SE0 and nothing enumerates. (A warm reboot "works" only
    // because the device is already attached when Init()'s busprobe runs,
    // bypassing this path entirely.) Polling IntHandler() reads the latched
    // HIRQ directly, so the connect is caught regardless of the pin level. This
    // is a poll, not an edge interrupt, so it cannot storm.
    usb_.IntHandler();

    handleUsbOnce();   // advance the USB state machine
    drainMidi();       // forward received MIDI packets

    serviceWedgeWatchdog(now);  // detect and heal a wedged device

    // One-shot cold-boot re-enumeration. The synth and the keyboard power up
    // together, so UHS2 enumerates the keyboard while its own USB stack is
    // still booting. That enumeration "succeeds" (state reaches RUNNING) but
    // the device then NAKs its MIDI IN endpoint forever - no notes ever arrive
    // until the user unplugs and replugs it. A replug is just a fresh
    // enumeration once the keyboard is fully awake, so we reproduce it in
    // firmware: the first time we reach RUNNING, wait until the device has been
    // up long enough to have finished its own boot, then force one clean
    // re-enumeration (chip reset re-probes the bus; pushing the state machine
    // back to INITIALIZE drives a bus reset, re-address and re-configure). The
    // latch makes this happen exactly once so a healthy device is left alone.
    //
    // The driver is released explicitly first. Relying on the INITIALIZE state
    // to do it is a timing accident: Init()'s busprobe reports the attached
    // device (FSHOST), and USB::Task() then overwrites INITIALIZE with SETTLE
    // *before* the INITIALIZE case runs - the release only happens because the
    // subsequent bus reset briefly reads SE0 and bounces the state machine
    // through INITIALIZE again. If that window were ever missed, the driver
    // would stay "consumed" and the device would re-enumerate with no driver
    // bound (a dead keyboard). Releasing here makes it deterministic.
    if (!reEnumDone && usb_.getUsbTaskState() == USB_STATE_RUNNING && !recovering_) {
      if (runningSinceMs == 0) {
        runningSinceMs = now;
      } else if ((now - runningSinceMs) >= 2000) {
        Serial.println("[usbmidi] cold-boot settle done, forcing one re-enumeration");
        if (midi_.GetAddress() != 0) midi_.Release();
        usb_.Init();
        usb_.regWr(rHIEN, bmCONDETIE);  // re-apply: Init() re-enables bmFRAMEIE
        usb_.setUsbTaskState(USB_DETACHED_SUBSTATE_INITIALIZE);
        detachedSinceMs = now;          // don't let the watchdog below double-fire
        reEnumDone = true;
      }
    }

    // Belt-and-braces for the narrow race where the device connects during
    // Init() itself - after its busprobe, before it clears CONDETIRQ - so the
    // IRQ is consumed by Init() and no fresh edge ever follows, leaving the
    // task stuck DETACHED with a stably-attached device. If we sit DETACHED for
    // over a second, re-run Init() to re-sample the bus. Guarded to the
    // detached state so it can never disturb an enumerated device.
    if ((usb_.getUsbTaskState() & USB_STATE_MASK) == USB_STATE_DETACHED) {
      if ((now - detachedSinceMs) >= 1500) {
        Serial.println("[usbmidi] detached >1.5s, re-sampling USB bus");
        usb_.Init();
        usb_.regWr(rHIEN, bmCONDETIE);  // re-apply: Init() re-enables bmFRAMEIE
        detachedSinceMs = now;
      }
    } else {
      detachedSinceMs = now;
    }

    // Heartbeat: one line per second. When the keyboard locks up, compare this
    // against a healthy line. Healthy looks like loops~1000 with msgs>0 while
    // playing; a wedge shows loops~1000, msgs=0, and either errs>0 (transfer
    // errors) or lastrc=0x04 (device gone silent / NAK). loops<<1000 means the
    // task is blocked in a transfer; qfree=0 means the event queue backed up.
    // vbus shows the bus state: 0=SE0/disconnected, 1=SE1/illegal,
    // 2=FSHOST/full-speed attached, 3=LSHOST/low-speed attached.

    // if ((now - lastDiagMs_) >= 1000) {
    //   lastDiagMs_ = now;
    //   int qfree = PolarisShared::midiEventQueue
    //                   ? (int)uxQueueSpacesAvailable(PolarisShared::midiEventQueue)
    //                   : -1;
    //   Serial.printf("[usbmidi] st=0x%02X vbus=0x%02X loops=%u msgs=%u errs=%u lastrc=0x%02X qfree=%d\n",
    //                 usb_.getUsbTaskState(), usb_.getVbusState(), diagLoops_,
    //                 diagMsgs_, diagErrs_, diagLastRc_, qfree);
    //   diagLoops_ = diagMsgs_ = diagErrs_ = 0;
    // }
  }
}

void MidiUSB::handleUsbOnce() {
  // Service the UHS2 state machine once.
  usb_.Task();
}

// Map CIN to number of valid data bytes in pkt[1..3].
// See USB-MIDI 1.0 spec; common cases covered.
uint8_t MidiUSB::cinToMsgLen(uint8_t cin, uint8_t statusByte) {
  switch (cin) {
    case 0x8: // Note Off          (3 bytes)
    case 0x9: // Note On           (3)
    case 0xA: // Poly Aftertouch   (3)
    case 0xB: // Control Change    (3)
    case 0xE: // Pitch Bend        (3)
    case 0x3: // SysCommon 3-byte  (3)
    case 0x4: // SysEx starts/continues (3 data)
      return 3;
    case 0xC: // Program Change    (2)
    case 0xD: // Channel Pressure  (2)
    case 0x2: // SysCommon 2-byte  (2)
    case 0x6: // SysEx ends with 2 (2)
      return 2;
    case 0x5: // Single-byte (e.g., real-time), 0xF? both map to 1
    case 0xF: // Single-byte (realtime)
    case 0x7: // SysEx ends with 1
      return 1;
    default:
      // Fallback: derive from status if possible
      if ((statusByte & 0xF0) == 0xC0 || (statusByte & 0xF0) == 0xD0) return 2;
      if ((statusByte & 0x80) == 0x80) return 3; // most channel voice
      return 1;
  }
}

bool MidiUSB::drainMidi() {
  if (!handler_) return true;

  // USBH_MIDI::RecvData returns USB-MIDI event packets in multiples of 4 bytes.
  // We'll read until empty.
  for (;;) {
    uint8_t pkt[64];
    uint16_t rcvd = sizeof(pkt);
    uint8_t rc = midi_.RecvData(&rcvd, pkt);
    diagLastRc_ = rc;
    // hrNAK (or a zero-length read with no error) just means "no data right
    // now" - a healthy idle poll. Any other non-zero code is a real transfer
    // error (counted for the diagnostics heartbeat).
    if (rc == hrNAK) return true;
    if (rc != 0) { diagErrs_++; return false; }
    if (rcvd == 0) return true;

    uint32_t now = millis();
    // Post-recovery discard window: the device may flush a burst of stale
    // wedge-era packets right after re-enumeration. Keep draining (that empties
    // the queue) but don't play them - the note-offs they pair with are gone,
    // so they would only re-create the stuck notes the recovery just cleared.
    // (0 = inactive; the flag is cleared on expiry so the signed comparison
    // can't misfire once millis() passes 2^31.)
    bool discard = false;
    if (discardUntilMs_ != 0) {
      discard = (int32_t)(discardUntilMs_ - now) > 0;
      if (!discard) discardUntilMs_ = 0;
    }

    // Parse 4-byte USB-MIDI Event Packets
    uint16_t msgCount = 0;
    for (uint16_t i = 0; i + 3 < rcvd; i += 4) {
      uint8_t cin  = pkt[i + 0] & 0x0F;
      uint8_t b1   = pkt[i + 1];
      uint8_t b2   = pkt[i + 2];
      uint8_t b3   = pkt[i + 3];

      // All-zero groups are padding, not MIDI events (same test the library's
      // own RecvData(outBuf) uses). Skip so they don't inflate the counters.
      if (cin == 0 && pkt[i] == 0 && b1 == 0 && b2 == 0 && b3 == 0) continue;

      msgCount++;
      diagMsgs_++;
      if (discard) continue;

      const uint8_t msgLen = cinToMsgLen(cin, b1);
      if (msgLen == 1) {
        const uint8_t m[1] = { b1 };
        handler_->parse(m, 1);
      } else if (msgLen == 2) {
        const uint8_t m[2] = { b1, b2 };
        handler_->parse(m, 2);
      } else /* 3 */ {
        const uint8_t m[3] = { b1, b2, b3 };
        handler_->parse(m, 3);
      }
    }
    if (msgCount) noteMidiActivity(now, msgCount);

    // If more is ready, RecvData will deliver on next call; loop continues.
  }
}

// ---------------------------------------------------------------------------
// Wedge watchdog
//
// Field capture of a lockup: st=0x90 loops~1000 msgs=0 errs=0 lastrc=0x04 -
// the device stays attached and simply NAKs every IN token forever, which at
// the transfer level is identical to "nobody is playing". A silence timeout
// therefore can't distinguish a wedge from a held chord (that approach was
// tried and rejected). What CAN distinguish them is asking the device:
// GET_STATUS on EP0 is mandatory, costs 3 tiny transactions, and touches
// nothing. A keyboard holding a chord answers instantly; a keyboard whose
// firmware crashed (e.g. VBUS sag under a pitch-bend flood - it never happens
// on a PC's stiff 5V rail) does not. Only that proof of death triggers the
// replug, so normal playing can never be interrupted.
// ---------------------------------------------------------------------------

void MidiUSB::noteMidiActivity(uint32_t now, uint16_t msgCount) {
  if ((now - lastMidiMs_) > 1000) burstMsgs_ = 0;  // gap ended the previous burst
  burstMsgs_ += msgCount;
  lastMidiMs_ = now;
  nudgeDone_ = false;
  probeFails_ = 0;
  probeOks_ = 0;
}

uint8_t MidiUSB::probeEp0(uint8_t addr) {
  // Bound EP0's NAK tolerance for the probe so a wedged device (hardware
  // ACKs the SETUP, firmware never serves the data stage) can't hold this
  // task in the library's 5s transfer timeout. Restored afterwards.
  EpInfo* ep0 = midi_.ep0Info();
  uint8_t savedNakPower = ep0->bmNakPower;
  ep0->bmNakPower = kProbeNakPower;
  uint8_t status[2] = {0, 0};
  uint8_t rc = usb_.ctrlReq(addr, 0,
      USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_STANDARD | USB_SETUP_RECIPIENT_DEVICE,
      USB_REQUEST_GET_STATUS, 0, 0, 0, 2, 2, status, NULL);
  ep0->bmNakPower = savedNakPower;
  return rc;
}

uint8_t MidiUSB::clearHaltBulkIn(uint8_t addr) {
  EpInfo* inEp = midi_.inEpInfo();
  if (inEp->epAddr == 0) return 0xFF;
  EpInfo* ep0 = midi_.ep0Info();
  uint8_t savedNakPower = ep0->bmNakPower;
  ep0->bmNakPower = kProbeNakPower;
  // Standard endpoint reset, same as the library's mass-storage driver (and
  // any PC host stack) issues: CLEAR_FEATURE(ENDPOINT_HALT) on the IN pipe.
  uint8_t rc = usb_.ctrlReq(addr, 0,
      USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_STANDARD | USB_SETUP_RECIPIENT_ENDPOINT,
      USB_REQUEST_CLEAR_FEATURE, USB_FEATURE_ENDPOINT_HALT, 0,
      (uint16_t)(0x80 | inEp->epAddr), 0, 0, NULL, NULL);
  ep0->bmNakPower = savedNakPower;
  // Clearing halt resets the device's data toggle to DATA0; mirror it.
  if (rc == 0) inEp->bmRcvToggle = 0;
  return rc;
}

void MidiUSB::startSoftReplug(uint32_t now, const char* reason) {
  recoveryCount_++;
  recoveryRetries_ = recovering_ ? (uint8_t)(recoveryRetries_ + 1) : 0;
  recovering_ = true;
  recoveryStartMs_ = now;
  Serial.printf("[usbmidi] RECOVERY #%u: %s - forcing software replug\n",
                (unsigned)recoveryCount_, reason);

  // The stuck notes' note-offs will never arrive; release them now.
  PolarisShared::allNotesOff = true;

  // Release the class driver FIRST, for the same reason as the cold-boot
  // re-enumeration above: with the device still attached, the state machine
  // never reliably reaches the INITIALIZE case that releases drivers, and a
  // still-"consumed" driver means the device re-enumerates with no driver
  // bound - which is exactly how the previous recovery attempt left the
  // keyboard dead.
  if (midi_.GetAddress() != 0) midi_.Release();

  // Proven cold-boot replug sequence: full MAX3421E chip reset, then run the
  // state machine from scratch (bus reset, re-address, re-configure).
  if (usb_.Init() != 0) {
    Serial.println("[usbmidi] RECOVERY: MAX3421E re-init failed, will retry");
  }
  usb_.regWr(rHIEN, bmCONDETIE);  // re-apply: Init() re-enables bmFRAMEIE
  usb_.setUsbTaskState(USB_DETACHED_SUBSTATE_INITIALIZE);
}

void MidiUSB::serviceWedgeWatchdog(uint32_t now) {
  // --- recovery in flight: wait for re-enumeration, retry if it stalls ---
  if (recovering_) {
    if (usb_.getUsbTaskState() == USB_STATE_RUNNING && midi_.GetAddress() != 0) {
      recovering_ = false;
      recoveryRetries_ = 0;
      discardUntilMs_ = now + kPostRecoveryDiscardMs;
      lastMidiMs_ = now;
      burstMsgs_ = 0;
      nudgeDone_ = false;
      probeFails_ = 0;
      probeOks_ = 0;
      lastProbeMs_ = now;
      wasRunning_ = true;
      Serial.printf("[usbmidi] RECOVERY #%u: device re-enumerated OK\n",
                    (unsigned)recoveryCount_);
      return;
    }
    uint32_t retryAfter =
        (recoveryRetries_ >= kReEnumFastRetries) ? kReEnumRetrySlowMs : kReEnumRetryMs;
    if ((now - recoveryStartMs_) >= retryAfter) {
      if (usb_.getUsbTaskState() == USB_STATE_RUNNING) {
        // Enumerated, but our driver didn't bind (non-MIDI device?). Hand
        // this to the attempt-capped driverless check below instead of
        // replugging forever.
        recovering_ = false;
        wasRunning_ = false;
        Serial.printf("[usbmidi] RECOVERY #%u: enumerated without MIDI driver\n",
                      (unsigned)recoveryCount_);
      } else {
        // Device still not enumerating (a hard-crashed keyboard can stay dead
        // until its own watchdog reboots it). Keep trying; a physical replug
        // is detected through the normal detach path and also ends this.
        startSoftReplug(now, "re-enumeration stalled, retrying");
      }
    }
    return;
  }

  bool running = (usb_.getUsbTaskState() == USB_STATE_RUNNING);
  uint8_t addr = running ? midi_.GetAddress() : 0;

  // Fresh connection: arm the watchdog cleanly so a just-plugged keyboard
  // isn't probed (or blamed) for pre-connection silence.
  if (running && !wasRunning_) {
    lastMidiMs_ = now;
    burstMsgs_ = 0;
    nudgeDone_ = false;
    probeFails_ = 0;
    probeOks_ = 0;
    lastProbeMs_ = now;
    runningNoDriverMs_ = now;
  }
  wasRunning_ = running;
  if (!running) {
    driverlessReplugs_ = 0;
    return;
  }

  // RUNNING with no MIDI driver bound: the library enumerated the device
  // without USBH_MIDI claiming it (the classic dead-keyboard failure mode).
  // Give it 3s to settle, then force a clean replug. Attempt-capped so a
  // genuinely unsupported device (e.g. a mouse) doesn't loop forever.
  if (addr == 0) {
    if ((now - runningNoDriverMs_) >= 3000 && driverlessReplugs_ < 3) {
      driverlessReplugs_++;
      startSoftReplug(now, "device enumerated but MIDI driver not bound");
    }
    return;
  }
  runningNoDriverMs_ = now;

  uint32_t silence = now - lastMidiMs_;

  // --- Tier 1: one-shot bulk-IN endpoint reset ("nudge") ---
  // Fires when a heavy stream (pitch-bend flood signature) stops dead. On a
  // healthy device that just went quiet this is a no-op (idle endpoint, both
  // toggles reset in sync); on a partially-wedged device whose EP0 still
  // works it is the standard way to un-stick the stream endpoint.
  if (!nudgeDone_ && silence >= kNudgeSilenceMs) {
    nudgeDone_ = true;
    if (burstMsgs_ >= kNudgeBurstMsgs) {
      uint8_t rc = clearHaltBulkIn(addr);
      Serial.printf("[usbmidi] %u-msg burst stopped dead; bulk-IN clear-halt rc=0x%02X\n",
                    (unsigned)burstMsgs_, rc);
    }
  }

  // --- Tier 2: EP0 liveness probes ---
  if (silence >= kProbeSilenceMs) {
    uint32_t interval = (probeOks_ >= kProbeFastCount) ? kProbeSlowMs : kProbeFastMs;
    if ((now - lastProbeMs_) >= interval) {
      lastProbeMs_ = now;
      uint8_t rc = probeEp0(addr);
      if (rc == 0) {
        if (probeOks_ < 0xFFFF) probeOks_++;
        probeFails_ = 0;
      } else {
        probeFails_++;
        Serial.printf("[usbmidi] EP0 liveness probe failed rc=0x%02X (%u/%u)\n",
                      rc, (unsigned)probeFails_, (unsigned)kProbeFailLimit);
        if (probeFails_ >= kProbeFailLimit) {
          startSoftReplug(now, "device control endpoint dead (firmware wedge)");
        }
      }
    }
  }
}
