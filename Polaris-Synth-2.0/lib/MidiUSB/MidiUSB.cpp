#include "MidiUSB.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

MidiUSB::MidiUSB(int intPin, int taskCore, UBaseType_t taskPrio)
: intPin_(intPin), taskCore_(taskCore), taskPrio_(taskPrio) {}

bool MidiUSB::begin(MidiHandler* handler) {
  handler_ = handler;

  // Initialize USB host core
  if (usb_.Init() != 0) {
    return false;
  }

  // INT# pin
  pinMode(intPin_, INPUT_PULLUP);

  // Binary semaphore for ISR -> task wakeups
  sem_ = xSemaphoreCreateBinary();
  if (!sem_) return false;

  // Background task (pinned core optional)
  BaseType_t ok = xTaskCreatePinnedToCore(
      taskThunk, "uhs-midi", 4096, this, taskPrio_, &task_, taskCore_);
  if (ok != pdPASS) return false;

  // Attach interrupt after task exists
  attachInterruptArg(intPin_, isrThunk, this, FALLING);

  return true;
}

void MidiUSB::end() {
  detachInterrupt(intPin_);
  if (task_) {
    vTaskDelete(task_);
    task_ = nullptr;
  }
  if (sem_) {
    vSemaphoreDelete(sem_);
    sem_ = nullptr;
  }
}

void IRAM_ATTR MidiUSB::isrThunk(void* arg) {
  static_cast<MidiUSB*>(arg)->isr();
}

void MidiUSB::isr() {
  intPending_ = true;
  BaseType_t hpw = pdFALSE;
  if (sem_) xSemaphoreGiveFromISR(sem_, &hpw);
  if (hpw == pdTRUE) portYIELD_FROM_ISR();
}

void MidiUSB::taskThunk(void* arg) {
  static_cast<MidiUSB*>(arg)->taskLoop();
}

void MidiUSB::taskLoop() {
  for (;;) {
    // Sleep until an INT# edge arrives
    if (xSemaphoreTake(sem_, portMAX_DELAY) == pdTRUE) {
      // Drain all pending conditions while INT may remain asserted
      // (MAX3421E can hold INT low until serviced).
      do {
        handleUsbOnce();
        drainMidi();
        // Keep looping while INT is still low or more pending work flagged.
      } while ((digitalRead(intPin_) == LOW) || (intPending_ && (intPending_ = false, false)));
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

void MidiUSB::drainMidi() {
  if (!handler_) return;

  // USBH_MIDI::RecvData returns USB-MIDI event packets in multiples of 4 bytes.
  // We'll read until empty.
  for (;;) {
    uint8_t pkt[64];
    uint16_t rcvd = sizeof(pkt);
    uint8_t rc = midi_.RecvData(&rcvd, pkt);
    if (rc != 0 || rcvd == 0) break;

    // Parse 4-byte USB-MIDI Event Packets
    for (uint16_t i = 0; i + 3 < rcvd; i += 4) {
      uint8_t cin  = pkt[i + 0] & 0x0F;
      uint8_t b1   = pkt[i + 1];
      uint8_t b2   = pkt[i + 2];
      uint8_t b3   = pkt[i + 3];

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

    // If more is ready, RecvData will deliver on next call; loop continues.
  }
}

