#include "MidiUSB.h"
#include "PolarisShared.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
    if (!reEnumDone && usb_.getUsbTaskState() == USB_STATE_RUNNING) {
      if (runningSinceMs == 0) {
        runningSinceMs = now;
      } else if ((now - runningSinceMs) >= 2000) {
        Serial.println("[usbmidi] cold-boot settle done, forcing one re-enumeration");
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
    if ((now - lastDiagMs_) >= 1000) {
      lastDiagMs_ = now;
      int qfree = PolarisShared::midiEventQueue
                      ? (int)uxQueueSpacesAvailable(PolarisShared::midiEventQueue)
                      : -1;
      Serial.printf("[usbmidi] st=0x%02X vbus=0x%02X loops=%u msgs=%u errs=%u lastrc=0x%02X qfree=%d\n",
                    usb_.getUsbTaskState(), usb_.getVbusState(), diagLoops_,
                    diagMsgs_, diagErrs_, diagLastRc_, qfree);
      diagLoops_ = diagMsgs_ = diagErrs_ = 0;
    }
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

    // Parse 4-byte USB-MIDI Event Packets
    for (uint16_t i = 0; i + 3 < rcvd; i += 4) {
      uint8_t cin  = pkt[i + 0] & 0x0F;
      uint8_t b1   = pkt[i + 1];
      uint8_t b2   = pkt[i + 2];
      uint8_t b3   = pkt[i + 3];

      const uint8_t msgLen = cinToMsgLen(cin, b1);
      diagMsgs_++;
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

    // If more is ready, RecvData will deliver on next call; loop continues.
  }
}
