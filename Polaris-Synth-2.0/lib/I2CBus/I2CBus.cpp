#include "I2CBus.h"

I2CBus::I2CBus(TwoWire &wire)
    : _wire(wire), _initialised(false)
{
}

bool I2CBus::init(int sda_pin, int scl_pin, uint32_t clk_speed)
{
    if (_initialised) {
        return true;
    }
    _initialised = _wire.begin(sda_pin, scl_pin, clk_speed);
    return _initialised;
}

bool I2CBus::writeReg8(uint8_t addr, uint8_t reg, uint8_t data)
{
    _wire.beginTransmission(addr);
    _wire.write(reg);
    _wire.write(data);
    return _wire.endTransmission() == 0;
}

bool I2CBus::readReg8(uint8_t addr, uint8_t reg, uint8_t *data)
{
    _wire.beginTransmission(addr);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) { // repeated start
        return false;
    }
    if (_wire.requestFrom(addr, (uint8_t)1) != 1) {
        return false;
    }
    *data = _wire.read();
    return true;
}

bool I2CBus::writeReg16(uint8_t addr, uint8_t reg, uint16_t value)
{
    _wire.beginTransmission(addr);
    _wire.write(reg);
    _wire.write(static_cast<uint8_t>(value >> 8));
    _wire.write(static_cast<uint8_t>(value & 0xFF));
    return _wire.endTransmission() == 0;
}

bool I2CBus::readReg16(uint8_t addr, uint8_t reg, uint16_t *value)
{
    _wire.beginTransmission(addr);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) { // repeated start
        return false;
    }
    if (_wire.requestFrom(addr, (uint8_t)2) != 2) {
        return false;
    }
    uint8_t hi = _wire.read();
    uint8_t lo = _wire.read();
    if (value) {
        *value = (static_cast<uint16_t>(hi) << 8) | lo;
    }
    return true;
}
