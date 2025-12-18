#include <Arduino.h>
#include "PolarisManager.h"
#include "I2CBus.h"

void PolarisManager::init_hardware() {
    // Initialize serial for debugging
    Serial.begin(115200);

    // Initialize Display Manager
    if (!display_manager) {
        display_manager = new DisplayManager();
    }

    // Allocate I2C bus (if not already)
    if (!i2c_bus) {
        i2c_bus = new I2CBus(I2C_NUM_0);
        Serial.println("I2C bus object created.");
    }

    // Initialize I2C bus pins and frequency
    // esp_err_t err = i2c_bus->init(GPIO_NUM_21, GPIO_NUM_22, 400000);
    // if (err != ESP_OK) {
    //     Serial.printf("Failed to initialize I2C bus: %s\n", esp_err_to_name(err));
    //     return; // Abort further hardware init if bus failed
    // }
    Serial.println("I2C bus initialized successfully.");

    // Allocate dependent devices if not already allocated (note: correct mapping of addresses)
    if (!io_expander_0) io_expander_0 = new IOExpander(*i2c_bus, IOEXPANDER0_I2C_ADDR);
    if (!io_expander_1) io_expander_1 = new IOExpander(*i2c_bus, IOEXPANDER1_I2C_ADDR);
    if (!adc_0) adc_0 = new ExternalADC(*i2c_bus, ADC0_I2C_ADDR);
    if (!adc_1) adc_1 = new ExternalADC(*i2c_bus, ADC1_I2C_ADDR);

    // Setting up peripheral devices (ADCs, IO expanders, SD card, etc.)


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
    if (!front_panel_state) {
        front_panel_state = new FrontPanelState(adc_0, adc_1, io_expander_0, io_expander_1);
    }

    Serial.println("Hardware objects allocated and initialized.");
}