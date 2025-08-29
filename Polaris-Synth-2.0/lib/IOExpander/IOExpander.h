// Class for the MCP23017 I2C IO Expander

#pragma once

#include <cstdint>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include "I2CBus.h"

class MCP23017
{
public:
    // Constructor: I2C bus and device i2c address
    MCP23017(I2CBus &bus, uint8_t i2c_addr);

    // Interrupt handler to be attached to the ESP32 interrupt pin.
    // This clears the interrupt state by reading the INTCAP registers.
    void handleInterrupt();

    // Read the current state of GPIOA and GPIOB
    uint8_t readGPIOA();
    uint8_t readGPIOB();

private:
    I2CBus &_bus;
    uint8_t _i2c_addr;

    // Write a single byte 'data' to register 'reg'
    void writeReg(uint8_t reg, uint8_t data);

    // Read from register 'reg' into data pointer
    void readReg(uint8_t reg, uint8_t* data);

};
