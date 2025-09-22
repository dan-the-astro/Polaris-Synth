#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2CBus.h"
#include "IOExpander.h"
#include "ExternalADC.h"
#include "MidiUSB.h"
#include "FrontPanelState.h"

#define ADC0_I2C_ADDR 0x49
#define ADC1_I2C_ADDR 0x48
#define IOEXPANDER0_I2C_ADDR 0x26   
#define IOEXPANDER1_I2C_ADDR 0x24

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

// Number of polyphonic voices to create
#define NUM_VOICES 5

// Default MIDI channel
#define MIDI_CHANNEL 1

// Samples per second
#define SAMPLERATE 44100

class PolarisManager {

public:
    PolarisManager() { init_hardware(); }

private:

    // These hardware-related members are allocated during init_hardware().
    I2CBus* i2c_bus = nullptr;
    IOExpander* io_expander_0 = nullptr;
    IOExpander* io_expander_1 = nullptr;
    ExternalADC* adc_0 = nullptr;
    ExternalADC* adc_1 = nullptr;
    FrontPanelState* front_panel_state = nullptr;
    MidiUSB* midi_usb = nullptr;

    // IO Expander interrupts now managed inside FrontPanelState.


    void init_hardware();
    ~PolarisManager() {
        // Free in reverse dependency order
        if (front_panel_state) { delete front_panel_state; front_panel_state = nullptr; }
        if (adc_1) { delete adc_1; adc_1 = nullptr; }
        if (adc_0) { delete adc_0; adc_0 = nullptr; }
        if (io_expander_1) { delete io_expander_1; io_expander_1 = nullptr; }
        if (io_expander_0) { delete io_expander_0; io_expander_0 = nullptr; }
        if (i2c_bus) { delete i2c_bus; i2c_bus = nullptr; }
    }

};