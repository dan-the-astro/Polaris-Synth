#include <Arduino.h>
#include "HardwareInit.h"
#include "I2CBus.h"

void init_hardware() {

    // Initialize serial for debugging
    Serial.begin(115200);

    // Initialize i2C bus
    I2CBus i2c_bus(I2C_NUM_0);

    esp_err_t err = i2c_bus.init(GPIO_NUM_21, GPIO_NUM_22, 400000);
    if (err != ESP_OK) {
        Serial.printf("Failed to initialize I2C bus: %s\n", esp_err_to_name(err));
    } else {
        Serial.println("I2C bus initialized successfully.");
    }

    
    
}