// MidiUSB.h - interface for USB MIDI host using MAX3421E on ESP32
#pragma once

#include <Arduino.h>
#include <USB.h>
#include <usbh_midi.h>

#include "MidiHandler.h"

class MidiUSB {
public:
  explicit MidiUSB(int intPin = 27, int taskCore = 1, UBaseType_t taskPrio = 1);

  // Initialize USB core and start background task. Returns true on success.
  bool begin(MidiHandler* handler);

  // Stop the background task and detach interrupt.
  void end();

private:
  // --- ISR & task glue ---
  static void IRAM_ATTR isrThunk(void* arg);
  static void taskThunk(void* arg);

  void isr();
  void taskLoop();

  void handleUsbOnce();
  void drainMidi();

  // Decode USB-MIDI CIN to number of valid MIDI bytes in pkt[1..3].
  static uint8_t cinToMsgLen(uint8_t cin, uint8_t statusByte);

private:
  // UHS2 core + MIDI class
  USB usb_;
  USBH_MIDI midi_{&usb_};

  MidiHandler* handler_ = nullptr;

  // INT# from MAX3421E
  int intPin_ = -1;

  // Interrupt -> task sync
  SemaphoreHandle_t sem_ = nullptr;
  TaskHandle_t task_ = nullptr;
  int taskCore_ = 1;
  UBaseType_t taskPrio_ = 1;

  // Optional: coalesce multiple INTs before running heavy work
  volatile bool intPending_ = false;
};

