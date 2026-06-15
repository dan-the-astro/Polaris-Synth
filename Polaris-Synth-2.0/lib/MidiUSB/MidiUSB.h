// MidiUSB.h - interface for USB MIDI host using MAX3421E on ESP32
#pragma once

#include <Arduino.h>
#include <USB.h>
#include <usbh_midi.h>

#include "MidiHandler.h"

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

private:
  // UHS2 core + MIDI class
  USB usb_;
  USBH_MIDI midi_{&usb_};

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
};
