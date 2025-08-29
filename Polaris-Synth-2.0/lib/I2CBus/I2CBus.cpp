#include "I2CBus.h"

I2CBus::I2CBus(i2c_port_t port)
    : _port(port), _initialised(false)
{
}

esp_err_t I2CBus::init(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t clk_speed)
{
    if (_initialised) {
        return ESP_OK;
    }

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_pin;
    conf.scl_io_num = scl_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = clk_speed;

    esp_err_t err = i2c_param_config(_port, &conf);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_driver_install(_port, conf.mode, 0, 0, 0);
    if (err == ESP_OK) {
        _initialised = true;
    }
    return err;
}

