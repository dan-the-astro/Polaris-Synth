#pragma once

#include "driver/i2c.h"
#include <cstdint>

// Helper class that initialises and represents an I2C bus.
// Multiple devices can share the same bus instance.
class I2CBus {
public:
    explicit I2CBus(i2c_port_t port);

    // Configure and install the I2C driver if not already initialised.
    esp_err_t init(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t clk_speed = 400000);

    // Return the underlying I2C port number.
    i2c_port_t port() const { return _port; }

private:
    i2c_port_t _port;
    bool _initialised;
};

