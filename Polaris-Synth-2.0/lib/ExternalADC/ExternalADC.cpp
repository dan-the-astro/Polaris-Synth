#include "ExternalADC.h"

// ADS1015 register addresses
static constexpr uint8_t ADS1015_REG_CONVERSION = 0x00;
static constexpr uint8_t ADS1015_REG_CONFIG     = 0x01;

// Bits for configuration register
static constexpr uint16_t ADS1015_CFG_OS_SINGLE    = 0x8000;
static constexpr uint16_t ADS1015_CFG_MUX_OFFSET   = 12;      // MUX bits shift
static constexpr uint16_t ADS1015_CFG_PGA_4_096V   = 0x0200;  // +/-4.096V range (LSB = 2 mV)
static constexpr uint16_t ADS1015_CFG_PGA_2_048V   = 0x0400;  // +/-2.048V range (LSB = 1 mV)
static constexpr uint16_t ADS1015_CFG_MODE_SINGLE  = 0x0100;  // single-shot mode
static constexpr uint16_t ADS1015_CFG_DR_1600SPS   = 0x0080;  // 1600 samples per second
static constexpr uint16_t ADS1015_CFG_DR_3300SPS   = 0x00C0;  // 3300 samples per second (max)
static constexpr uint16_t ADS1015_CFG_CQUE_NONE    = 0x0003;  // disable comparator

ExternalADC::ExternalADC(I2CBus &bus, uint8_t i2c_addr)
    : _bus(bus), _i2c_addr(i2c_addr)
{
}

esp_err_t ExternalADC::writeReg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = static_cast<uint8_t>(value >> 8);
    buf[2] = static_cast<uint8_t>(value & 0xFF);
    return i2c_master_write_to_device(_bus.port(), _i2c_addr, buf, sizeof(buf), pdMS_TO_TICKS(1000));
}

esp_err_t ExternalADC::readReg(uint8_t reg, uint16_t *value)
{
    uint8_t data[2] = {0, 0};
    esp_err_t err = i2c_master_write_read_device(_bus.port(), _i2c_addr, &reg, 1, data, 2, pdMS_TO_TICKS(1000));
    if (err == ESP_OK && value) {
        *value = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    }
    return err;
}

int16_t ExternalADC::readChannel(uint8_t channel)
{
    if (channel > 3) {
        return 0;
    }

    // Build configuration word
    uint16_t config = ADS1015_CFG_OS_SINGLE |
                      0x4000 | // Set multiplexer to use GND as negative reference
                      (static_cast<uint16_t>(channel) << ADS1015_CFG_MUX_OFFSET) |
                      ADS1015_CFG_PGA_4_096V |
                      ADS1015_CFG_MODE_SINGLE |
                      ADS1015_CFG_DR_3300SPS |   // use max data rate
                      ADS1015_CFG_CQUE_NONE;

    // Start single conversion
    writeReg(ADS1015_REG_CONFIG, config);

    // Busy-wait ~320 us (conversion period ≈303 us @3300 SPS)
    // Add slight margin for oscillator / I2C command latency.
    esp_rom_delay_us(320); // or ets_delay_us(320) on older IDF

    uint16_t raw = 0;
    readReg(ADS1015_REG_CONVERSION, &raw);

    return static_cast<int16_t>(raw) >> 4;
}

