#include <Arduino.h>
#include "PolarisManager.h"
#include "PolarisShared.h"

void PolarisManager::init_hardware() {
    // Initialize serial for debugging
    Serial.begin(115200);

    // Bring up the shared I2C bus first; everything I2C (display, expanders,
    // ADCs) runs through Wire on this task
    if (!i2c_bus) {
        i2c_bus = new I2CBus(Wire);
        if (!i2c_bus->init(I2C_SDA, I2C_SCL, 400000)) {
            Serial.println("Failed to initialize I2C bus!");
        } else {
            Serial.println("I2C bus initialized successfully.");
        }
    }

    // Initialize Display Manager (splash screen)
    Serial.println("[boot] display");
    if (!display_manager) {
        display_manager = new DisplayManager();
    }

    // Allocate dependent devices if not already allocated (note: correct mapping of addresses)
    Serial.println("[boot] expanders + ADCs");
    if (!io_expander_0) io_expander_0 = new IOExpander(*i2c_bus, IOEXPANDER0_I2C_ADDR);
    if (!io_expander_1) io_expander_1 = new IOExpander(*i2c_bus, IOEXPANDER1_I2C_ADDR);
    if (!adc_0) adc_0 = new ExternalADC(*i2c_bus, ADC0_I2C_ADDR);
    if (!adc_1) adc_1 = new ExternalADC(*i2c_bus, ADC1_I2C_ADDR);

    // Probe every front panel I2C device so wiring/power problems are
    // obvious in the boot log. A non-responding expander also means its INT
    // line to GPIO 34/35 is undriven (those pins have no internal pulls).
    {
        struct { const char* name; uint8_t addr; } devs[] = {
            { "IOExpander0", IOEXPANDER0_I2C_ADDR },
            { "IOExpander1", IOEXPANDER1_I2C_ADDR },
            { "ADC0",        ADC0_I2C_ADDR },
            { "ADC1",        ADC1_I2C_ADDR },
        };
        for (auto& d : devs) {
            uint8_t tmp = 0;
            bool ok = i2c_bus->readReg8(d.addr, 0x00, &tmp);
            Serial.printf("[boot] I2C %s @0x%02X: %s\n", d.name, d.addr,
                          ok ? "OK" : "NOT RESPONDING");
        }
    }

    // Configure ADC interrupt pins as inputs with internal pull-ups
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ADC1_INT) | (1ULL << ADC2_INT);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);


    // Configure the 4-bit counter pins as outputs
    io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CNT_CLK1) | (1ULL << CNT_RES1) | (1ULL << CNT_CLK2) | (1ULL << CNT_RES2);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Set default values
    gpio_set_level(static_cast<gpio_num_t>(CNT_CLK1), 0);
    gpio_set_level(static_cast<gpio_num_t>(CNT_RES1), 1);
    gpio_set_level(static_cast<gpio_num_t>(CNT_CLK2), 0);
    gpio_set_level(static_cast<gpio_num_t>(CNT_RES2), 1);

    // Pulse reset high to trigger reset, then release
    gpio_set_level(static_cast<gpio_num_t>(CNT_RES1), 1);
    gpio_set_level(static_cast<gpio_num_t>(CNT_RES2), 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(static_cast<gpio_num_t>(CNT_RES1), 0);
    gpio_set_level(static_cast<gpio_num_t>(CNT_RES2), 0);

    // Initialize front panel state manager
    Serial.println("[boot] front panel");
    if (!front_panel_state) {
        front_panel_state = new FrontPanelState(adc_0, adc_1, io_expander_0, io_expander_1);
    }

    // Patch storage on the SD card (failure tolerated; UI reports "NO SD")
    Serial.println("[boot] SD card");
    if (!patch_manager) {
        patch_manager = new PatchManager();
        patch_manager->begin();
    }

    // Hand the panel and patch storage to the UI
    display_manager->attach(front_panel_state, patch_manager);

    // MIDI inputs: USB host (MAX3421E on VSPI) and DIN (UART2).
    // The USB task gets priority 4 on this core, above the manager loop.
    Serial.println("[boot] USB MIDI");
    if (!midi_handler) {
        midi_handler = new MidiHandler();
    }
    if (!midi_usb) {
        // The UHS2 USB class never initializes its devConfig[] driver table
        // (its constructor relies on the object living in zero-initialized
        // static storage, like the global `USB Usb;` every Arduino sketch
        // uses). Heap-allocating it handed USB::Task() garbage driver
        // pointers and crashed with LoadProhibited, so the MidiUSB object -
        // which contains the USB object - must live in static storage.
        static MidiUSB midi_usb_instance(USB_INT_PIN, 1 /*core*/, 4 /*prio*/);
        midi_usb = &midi_usb_instance;
        // begin() now only spawns the polling task and returns immediately; the
        // MAX3421E is initialized (with retries) on that task so a slow cold-boot
        // oscillator no longer blocks boot or permanently kills USB MIDI.
        if (!midi_usb->begin(midi_handler)) {
            Serial.println("USB MIDI: failed to start polling task");
        }
    }
    Serial.println("[boot] DIN MIDI");
    if (!midi_din) {
        midi_din = new MidiDin(MIDI_DIN_RX_PIN);
        midi_din->begin();
    }

    // Publish the front panel: this releases the SynthEngine on core 0
    PolarisShared::frontPanel = front_panel_state;

    Serial.println("Hardware objects allocated and initialized.");
}
