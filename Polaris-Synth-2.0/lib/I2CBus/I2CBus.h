#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <cstdint>

// Helper class that wraps the Arduino Wire (TwoWire) driver and represents an
// I2C bus. Multiple devices can share the same bus instance.
//
// Everything in the project (IO expanders, external ADCs, and the U8G2 OLED)
// shares the single hardware I2C bus through Wire. The U8G2 display driver
// uses Wire internally, so routing the other peripherals through Wire as well
// avoids installing two conflicting drivers on the same I2C controller.
//
// NOTE: Wire is not thread safe. All I2C traffic must come from a single task
// (the PolarisManager task on core 1).
class I2CBus {
public:
    explicit I2CBus(TwoWire &wire = Wire);

    // Configure pins and frequency and start the bus if not already started.
    bool init(int sda_pin, int scl_pin, uint32_t clk_speed = 400000);

    // Return the underlying TwoWire object.
    TwoWire &wire() { return _wire; }

    // Write a single byte to an 8-bit register (MCP23017 style).
    bool writeReg8(uint8_t addr, uint8_t reg, uint8_t data);

    // Read a single byte from an 8-bit register.
    bool readReg8(uint8_t addr, uint8_t reg, uint8_t *data);

    // Write a 16-bit big-endian value to a register (ADS1015 style).
    bool writeReg16(uint8_t addr, uint8_t reg, uint16_t value);

    // Read a 16-bit big-endian value from a register.
    bool readReg16(uint8_t addr, uint8_t reg, uint16_t *value);

private:
    TwoWire &_wire;
    bool _initialised;
};
