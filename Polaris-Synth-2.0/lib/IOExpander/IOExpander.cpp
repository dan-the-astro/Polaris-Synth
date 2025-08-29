#include <IOExpander.h>

// Constructor: i2c_port, device i2c address
MCP23017::MCP23017(i2c_port_t i2c_port, uint8_t i2c_addr)
    : _i2c_port(i2c_port), _i2c_addr(i2c_addr)
{
    // 1. Set all pins on both banks as inputs.
    writeReg(0x00, 0xFF); // IODIRA all inputs
    writeReg(0x01, 0xFF); // IODIRB all inputs
    // 2. Enable interrupt-on-change for all pins.
    writeReg(0x04, 0xFF); // GPINTENA
    writeReg(0x05, 0xFF); // GPINTENB
    // 3. Configure the MCP23017 to mirror interrupts on INTA and INTB.
    // Set the MIRROR bit (bit 6) in IOCON. Write the value to IOCON.
    writeReg(0x0A, 0x40);
    // 4. Optionally configure default compare behavior here if needed.
}

// Interrupt handler to be attached to the ESP32 interrupt pin.
// This clears the interrupt state by reading the INTCAP registers.
void MCP23017::handleInterrupt()
{
    uint8_t dummyA = 0, dummyB = 0;
    // Read captured values from both Port A & B.
    readReg(0x10, &dummyA);
    readReg(0x11, &dummyB);
    // TODO: Notify task that additional processing is needed.
}
// Write a single byte 'data' to register 'reg'
void MCP23017::writeReg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}
// Read from register 'reg' into data pointer
void MCP23017::readReg(uint8_t reg, uint8_t* data)
{   
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

uint8_t MCP23017::readGPIOA()
{
    uint8_t data = 0;
    readReg(0x12, &data); // GPIOA register address is 0x12
    return data;
}
uint8_t MCP23017::readGPIOB()
{
    uint8_t data = 0;
    readReg(0x13, &data); // GPIOB register address is 0x13
    return data;
}