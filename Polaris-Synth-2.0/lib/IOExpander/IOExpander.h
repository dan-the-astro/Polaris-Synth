// Class for the MCP23017 I2C IO Expander

#pragma once

#include <cstdint>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2CBus.h"

class IOExpander
{
public:
    // Constructor: I2C bus and device i2c address
    IOExpander(I2CBus &bus, uint8_t i2c_addr);

    // Configure the MCP23017 to mirror both ports onto a single INT line and
    // enable interrupt-on-change for all 16 pins. This does NOT configure the
    // ESP32 GPIO itself; do that externally and register an ISR which calls
    // isrFlagSet(). Safe to call multiple times.
    void attachInterruptLine();

    // Called from the actual ESP32 GPIO ISR (IRAM safe). Only sets a volatile flag.
    // Marked inline to encourage placement in IRAM if needed.
    inline void isrFlagSet() { _interruptPending = true; }

    // Poll by application task: returns true if an interrupt occurred since last poll
    // and (optionally) clears the flag (default clear=true). If clear=false, caller
    // can peek without consuming.
    bool consumeInterruptFlag(bool clear = true);

    // After seeing consumeInterruptFlag()==true, the application should call
    // readGPIOA()/readGPIOB() (or define a combined method) to fetch current states.
    // We provide a helper to explicitly clear the MCP23017 interrupt by reading
    // INTCAP registers; if you only read GPIO you might miss edge-latched states.
    void acknowledgeInterrupt();

    // Read the current state of GPIOA and GPIOB
    uint8_t readGPIOA();
    uint8_t readGPIOB();

    // Packed read helper: returns combined 16-bit value of current GPIO levels.
    // Lower 8 bits = Port A (GPIOA), upper 8 bits = Port B (GPIOB).
    // Performs two sequential single-byte register reads internally.
    uint16_t readGPIOPacked();

    // Packed captured snapshot read (clears interrupt): reads INTCAPA then INTCAPB
    // and returns them packed like readGPIOPacked(). Use right after detecting
    // an interrupt if you need the latched-at-interrupt snapshot.
    uint16_t readCapturedPacked();

private:
    I2CBus &_bus;
    uint8_t _i2c_addr;
    volatile bool _interruptPending = false; // set in ISR, consumed by task

    // Write a single byte 'data' to register 'reg'
    void writeReg(uint8_t reg, uint8_t data);

    // Read from register 'reg' into data pointer
    void readReg(uint8_t reg, uint8_t* data);

};
