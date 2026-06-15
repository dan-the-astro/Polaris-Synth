#include "MidiUSB.h"
#include "PolarisShared.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

MidiUSB::MidiUSB(int intPin, int taskCore, UBaseType_t taskPrio)
: intPin_(intPin), taskCore_(taskCore), taskPrio_(taskPrio) {}

bool MidiUSB::begin(MidiHandler* handler) {
  handler_ = handler;

  // Initialize USB host core
  if (usb_.Init() != 0) {
    return false;
  }

  // INT# pin, observed as a level from the polling task. Deliberately NOT
  // attached as a GPIO interrupt - see the class comment in MidiUSB.h.
  pinMode(intPin_, INPUT_PULLUP);

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
  uint32_t boot = millis();
  lastDiagMs_ = boot;
  for (;;) {
    diagLoops_++;
    // 1kHz service rate, like the proven polled design of the 1.x firmware.
    // The delay also guarantees this (priority 4) task can never starve
    // core 1, whatever the INT# line or the controller do.
    vTaskDelay(pdMS_TO_TICKS(1));

    if (digitalRead(intPin_) == LOW) {
      // Service and clear the MAX3421E IRQ flags. (With the GPIO2 INT patch the
      // library's Task() also reads the correct pin; this is belt-and-braces.)
      usb_.IntHandler();
    }

    handleUsbOnce();   // advance the USB state machine
    drainMidi();       // forward received MIDI packets

    uint32_t now = millis();
    // Heartbeat: one line per second. When the keyboard locks up, compare this
    // against a healthy line. Healthy looks like loops~1000 with msgs>0 while
    // playing; a wedge shows loops~1000, msgs=0, and either errs>0 (transfer
    // errors) or lastrc=0x04 (device gone silent / NAK). loops<<1000 means the
    // task is blocked in a transfer; qfree=0 means the event queue backed up.
    if ((now - lastDiagMs_) >= 1000) {
      lastDiagMs_ = now;
      int qfree = PolarisShared::midiEventQueue
                      ? (int)uxQueueSpacesAvailable(PolarisShared::midiEventQueue)
                      : -1;
      Serial.printf("[usbmidi] st=0x%02X loops=%u msgs=%u errs=%u lastrc=0x%02X qfree=%d\n",
                    usb_.getUsbTaskState(), diagLoops_, diagMsgs_, diagErrs_,
                    diagLastRc_, qfree);
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
