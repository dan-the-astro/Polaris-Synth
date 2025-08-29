#pragma once

// Simple driver for the ADS1015 external ADC.
// Assumes the I2C bus has been initialised elsewhere and
// provides helpers to read/write 16‑bit registers on the device.

#include <cstdint>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2CBus.h"

class ExternalADC {
public:
    // Construct with an initialised I2C bus and device address.
    ExternalADC(I2CBus &bus, uint8_t i2c_addr);

    // Write a 16‑bit value to an ADS1015 register.
    esp_err_t writeReg(uint8_t reg, uint16_t value);

    // Read a 16‑bit value from an ADS1015 register.
    esp_err_t readReg(uint8_t reg, uint16_t *value);

    // Convenience method to read a single ended conversion from
    // the given channel [0‑3]. Returns raw 12‑bit signed value.
    int16_t readChannel(uint8_t channel);

private:
    I2CBus &_bus;
    uint8_t _i2c_addr;
};

