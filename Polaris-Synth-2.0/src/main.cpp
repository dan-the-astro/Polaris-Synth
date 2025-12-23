#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "PolarisManager.h"
#include "SynthEngine.h"

#define NUM_VOICES 5


void createPolarisManager(void* pvParameters) {
    static PolarisManager* polaris_manager = nullptr;
    if (!polaris_manager) {
        polaris_manager = new PolarisManager();
    }
}

void createSynthEngine(void* pvParameters) {
    static SynthEngine* synth_engine = nullptr;
    if (!synth_engine) {
        synth_engine = new SynthEngine(NUM_VOICES); 
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

    // Create a task to run SynthEngine on core 0    
    xTaskCreatePinnedToCore(
        createSynthEngine,
        "SynthEngine",
        16384, // Stack size
        NULL,
        configMAX_PRIORITIES, // Highest priority
        NULL,
        0 // Run on core 0
    );


}




void loop() {

}
