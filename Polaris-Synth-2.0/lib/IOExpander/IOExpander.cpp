#include <IOExpander.h>

// Constructor: I2C bus, device i2c address
IOExpander::IOExpander(I2CBus &bus, uint8_t i2c_addr)
    : _bus(bus), _i2c_addr(i2c_addr)
{
    // 1. Set all pins on both banks as inputs (default safe state).
    writeReg(0x00, 0xFF); // IODIRA all inputs
    writeReg(0x01, 0xFF); // IODIRB all inputs

    // 2. Disable interrupts until user calls attachInterruptLine().
    writeReg(0x04, 0x00); // GPINTENA
    writeReg(0x05, 0x00); // GPINTENB

    // 3. Configure IOCON: MIRROR (bit6) set so a single INT line can be used.
    writeReg(0x0A, 0x40); // IOCON

    // 4. Interrupt-on-change vs DEFVAL comparison: use change (INTCON=0).
    writeReg(0x08, 0x00); // INTCONA
    writeReg(0x09, 0x00); // INTCONB
}

// Enable interrupt-on-change for all pins and mirror onto single INT line.
void IOExpander::attachInterruptLine()
{
    // Enable all 8 bits on each port.
    writeReg(0x04, 0xFF); // GPINTENA
    writeReg(0x05, 0xFF); // GPINTENB
    // IOCON already had MIRROR set in constructor; ensure it still is.
    writeReg(0x0A, 0x40);
}

// Clear MCP23017 latched interrupt by reading INTCAP registers.
void IOExpander::acknowledgeInterrupt()
{
    uint8_t dummy;
    readReg(0x10, &dummy); // INTCAPA
    readReg(0x11, &dummy); // INTCAPB
}

bool IOExpander::consumeInterruptFlag(bool clear)
{
    bool wasSet = _interruptPending;
    if (wasSet && clear) {
        _interruptPending = false;
    }
    return wasSet;
}
// Write a single byte 'data' to register 'reg'
void IOExpander::writeReg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_bus.port(), cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}
// Read from register 'reg' into data pointer
void IOExpander::readReg(uint8_t reg, uint8_t* data)
{   
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_bus.port(), cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

uint8_t IOExpander::readGPIOA()
{
    uint8_t data = 0;
    readReg(0x12, &data); // GPIOA register address is 0x12
    return data;
}
uint8_t IOExpander::readGPIOB()
{
    uint8_t data = 0;
    readReg(0x13, &data); // GPIOB register address is 0x13
    return data;
}

uint16_t IOExpander::readGPIOPacked()
{
    uint8_t a = 0, b = 0;
    readReg(0x12, &a); // GPIOA
    readReg(0x13, &b); // GPIOB
    return static_cast<uint16_t>(a) | (static_cast<uint16_t>(b) << 8);
}

uint16_t IOExpander::readCapturedPacked()
{
    uint8_t a = 0, b = 0;
    readReg(0x10, &a); // INTCAPA (reading clears)
    readReg(0x11, &b); // INTCAPB
    return static_cast<uint16_t>(a) | (static_cast<uint16_t>(b) << 8);
}
