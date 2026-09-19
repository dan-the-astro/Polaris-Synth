// MidiUSB.h - interface for USB MIDI host using MAX3421E on ESP32
#pragma once

#include <Arduino.h>
#include <USB.h>
#include <usbh_midi.h>

#include "MidiHandler.h"

// USBH_MIDI with access to its endpoint records. The wedge watchdog needs
// the EP0 record (to bound control-transfer NAK waits so a dead device can't
// stall the task for the library's 5s timeout) and the bulk-IN record (to
// clear-halt and re-toggle the MIDI stream endpoint).
class PolarisUsbhMidi : public USBH_MIDI {
public:
  explicit PolarisUsbhMidi(USB* p) : USBH_MIDI(p) {}
  EpInfo* ep0Info() { return &epInfo[0]; }
  EpInfo* inEpInfo() { return &epInfo[epDataInIndex]; }
};

// Drives the MAX3421E USB host controller and forwards received USB-MIDI
// packets to a MidiHandler.
//
// The controller is serviced by a dedicated 1kHz polling task; no GPIO
// interrupt is used. The MAX3421E INT# output is open-drain level-mode
// (UHS2 sets INTLEVEL), so against only the weak internal pull-up - and with
// the devkit LED also hanging on GPIO2 - the "high" level sits in the
// undefined input zone and an edge interrupt storms until the interrupt
// watchdog resets the chip. The UHS2 state machine wants continuous polling
// anyway, and 1ms of MIDI latency is inaudible.
//
// Wedge watchdog: the Keystation Mini 32's bulk-IN endpoint can wedge under
// heavy traffic (pitch-bend floods) - the device NAKs every IN token forever
// with no host-visible error, which is indistinguishable from "not playing"
// at the transfer level. Silence alone therefore NEVER triggers recovery
// (a held chord is also silence). Instead, silence only triggers an active
// EP0 liveness probe (GET_STATUS - mandatory, tiny, non-destructive): a
// healthy device holding a chord answers instantly; a crashed one doesn't.
// Only repeated probe failures - proof the device firmware is gone - trigger
// the software replug. A one-shot CLEAR_FEATURE(ENDPOINT_HALT) "nudge" on
// the bulk-IN endpoint (the standard endpoint reset a PC driver issues)
// additionally covers partial wedges where EP0 stays alive.
class MidiUSB {
public:
  explicit MidiUSB(int intPin = 2, int taskCore = 1, UBaseType_t taskPrio = 1);

  // Initialize USB core and start the polling task. Returns true on success.
  bool begin(MidiHandler* handler);

  // Stop the polling task.
  void end();

private:
  static void taskThunk(void* arg);
  void taskLoop();

  void handleUsbOnce();

  // Reads and forwards all pending MIDI packets. Returns false if a real USB
  // transfer error occurred (not just "no data"); the return value is currently
  // only used to feed the diagnostics counters.
  bool drainMidi();

  // Decode USB-MIDI CIN to number of valid MIDI bytes in pkt[1..3].
  static uint8_t cinToMsgLen(uint8_t cin, uint8_t statusByte);

  // ---- wedge watchdog ----
  void serviceWedgeWatchdog(uint32_t now);
  void noteMidiActivity(uint32_t now, uint16_t msgCount);
  uint8_t probeEp0(uint8_t addr);
  uint8_t clearHaltBulkIn(uint8_t addr);
  void startSoftReplug(uint32_t now, const char* reason);

private:
  // UHS2 core + MIDI class
  USB usb_;
  PolarisUsbhMidi midi_{&usb_};

  MidiHandler* handler_ = nullptr;

  // INT# from MAX3421E, read as a plain level only (never an interrupt source)
  int intPin_ = -1;

  TaskHandle_t task_ = nullptr;
  int taskCore_ = 1;
  UBaseType_t taskPrio_ = 1;

  // Once-per-second diagnostics to characterise lockups. The error-based
  // watchdog above never fired in the field, so we need to see what the link
  // is actually doing when it wedges: is the task still looping, is the device
  // sending data / NAKing / erroring, is the event queue backing up.
  uint32_t diagLoops_ = 0;   // taskLoop iterations this second
  uint32_t diagMsgs_ = 0;    // MIDI messages parsed this second
  uint32_t diagErrs_ = 0;    // non-NAK transfer errors this second
  uint8_t  diagLastRc_ = 0;  // last RecvData return code
  uint32_t lastDiagMs_ = 0;

  // ---- wedge watchdog state ----
  bool     wasRunning_ = false;      // previous-loop RUNNING state (edge detect)
  uint32_t lastMidiMs_ = 0;          // last time a MIDI event arrived
  uint32_t burstMsgs_ = 0;           // messages in the current burst (gaps < 1s)
  bool     nudgeDone_ = false;       // one clear-halt nudge per silence episode
  uint32_t lastProbeMs_ = 0;         // last EP0 liveness probe
  uint8_t  probeFails_ = 0;          // consecutive probe failures
  uint16_t probeOks_ = 0;            // consecutive probe successes (for backoff)
  bool     recovering_ = false;      // software replug in flight
  uint32_t recoveryStartMs_ = 0;     // when the current replug was initiated
  uint32_t recoveryCount_ = 0;       // total replugs (diagnostics)
  uint8_t  recoveryRetries_ = 0;     // consecutive stalled re-enum retries
  uint8_t  driverlessReplugs_ = 0;   // replugs triggered by "RUNNING, no driver"
  uint32_t runningNoDriverMs_ = 0;   // when RUNNING-without-driver was first seen
  uint32_t discardUntilMs_ = 0;      // post-recovery window: drain but don't play
};
