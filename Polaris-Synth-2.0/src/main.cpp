#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "PolarisManager.h"


void createPolarisManager(void* pvParameters) {
    static PolarisManager* polaris_manager = nullptr;
    if (!polaris_manager) {
        polaris_manager = new PolarisManager();
    }
}


void setup() {

    // Create a task to run PolarisManager on core 1    
    xTaskCreatePinnedToCore(
        createPolarisManager,
        "PolarisManager",
        8192, // Stack size
        NULL,
        configMAX_PRIORITIES - 1, // Highest priority
        NULL,
        1 // Run on core 1
    );


}




void loop() {

}
