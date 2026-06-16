#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2CBus.h"
#include "IOExpander.h"
#include "ExternalADC.h"
#include "MidiUSB.h"
#include "MidiDin.h"
#include "MidiHandler.h"
#include "FrontPanelState.h"
#include "DisplayManager.h"
#include "PatchManager.h"

#define ADC0_I2C_ADDR 0x49
#define ADC1_I2C_ADDR 0x48
#define IOEXPANDER0_I2C_ADDR 0x26
#define IOEXPANDER1_I2C_ADDR 0x24

// I2C bus pins
#define I2C_SDA 21
#define I2C_SCL 22

// PINS for counter clock and reset and IO expander and ADC interrupt pins
#define CNT_CLK1 32
#define CNT_RES1 33
#define CNT_CLK2 25
#define CNT_RES2 26
#define ADC1_INT 36
#define ADC2_INT 39
#define EXP1_INT 34
#define EXP2_INT 35

#define SD_CS 13

// MAX3421E USB host controller interrupt pin
#define USB_INT_PIN 2

// DIN MIDI input pin (routed to UART2 through the GPIO matrix)
#define MIDI_DIN_RX_PIN 3

// Number of polyphonic voices to create
#define NUM_VOICES 6

// Default MIDI channel
#define MIDI_CHANNEL 1

// Samples per second
#define SAMPLERATE 44100

// Owns all hardware that is not the synth engine: I2C peripherals, front
// panel scanning, the OLED UI, MIDI inputs, and patch storage. Runs on
// core 1; the synth engine runs on core 0 and communicates through
// PolarisShared.
class PolarisManager {

public:
    PolarisManager() {
        init_hardware();
    }

    // Main manager loop: front panel scanning, DIN MIDI, display.
    // Never returns.
    void run() {
        for (;;) {
            if (front_panel_state) {
                front_panel_state->loopTask();
            }
            if (midi_din) {
                midi_din->poll();
            }
            if (display_manager) {
                display_manager->update();
            }
            // 1ms pace: keeps the ADS1015 conversion pipeline timed (>303us
            // between start and read) and lets lower-priority tasks breathe
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

private:

    // These hardware-related members are allocated during init_hardware().
    I2CBus* i2c_bus = nullptr;
    IOExpander* io_expander_0 = nullptr;
    IOExpander* io_expander_1 = nullptr;
    ExternalADC* adc_0 = nullptr;
    ExternalADC* adc_1 = nullptr;
    FrontPanelState* front_panel_state = nullptr;
    MidiHandler* midi_handler = nullptr;
    MidiUSB* midi_usb = nullptr;
    MidiDin* midi_din = nullptr;
    DisplayManager* display_manager = nullptr;
    PatchManager* patch_manager = nullptr;

    // Hardware initialization function
    void init_hardware();

    ~PolarisManager() {
        // Free in reverse dependency order
        if (patch_manager) { delete patch_manager; patch_manager = nullptr; }
        if (midi_din) { delete midi_din; midi_din = nullptr; }
        // midi_usb points to a static instance (UHS2 requires zeroed
        // storage): stop it but never delete it
        if (midi_usb) { midi_usb->end(); midi_usb = nullptr; }
        if (midi_handler) { delete midi_handler; midi_handler = nullptr; }
        if (front_panel_state) { delete front_panel_state; front_panel_state = nullptr; }
        if (adc_1) { delete adc_1; adc_1 = nullptr; }
        if (adc_0) { delete adc_0; adc_0 = nullptr; }
        if (io_expander_1) { delete io_expander_1; io_expander_1 = nullptr; }
        if (io_expander_0) { delete io_expander_0; io_expander_0 = nullptr; }
        if (display_manager) { delete display_manager; display_manager = nullptr; }
        if (i2c_bus) { delete i2c_bus; i2c_bus = nullptr; }
    }

};
