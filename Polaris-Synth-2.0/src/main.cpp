#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "LookupTables.h"
#include "PolarisShared.h"
#include "PolarisManager.h"
#include "SynthEngine.h"

#define NUM_VOICES 5


// Core 1: hardware manager (front panel, display, MIDI, patches)
void polarisManagerTask(void* pvParameters) {
    PolarisManager* polaris_manager = new PolarisManager();
    polaris_manager->run(); // never returns
}

// Core 0: synth engine (all audio). Waits inside run() until the manager has
// published the front panel state.
void synthEngineTask(void* pvParameters) {
    SynthEngine* synth_engine = new SynthEngine(NUM_VOICES);
    synth_engine->run(); // never returns
}


void setup() {

    // Cross-core queue/globals must exist before either task starts
    PolarisShared::init();

    // Create a task to run PolarisManager on core 1
    xTaskCreatePinnedToCore(
        polarisManagerTask,
        "PolarisManager",
        12288, // Stack size: SD/FATFS mount, u8g2 and float printf all run here
        NULL,
        3, // Above the idle/loop tasks, below USB MIDI (prio 4)
        NULL,
        1 // Run on core 1
    );

    // Create a task to run SynthEngine on core 0
    xTaskCreatePinnedToCore(
        synthEngineTask,
        "SynthEngine",
        16384, // Stack size
        NULL,
        configMAX_PRIORITIES - 2, // Audio gets (almost) top priority
        NULL,
        0 // Run on core 0
    );


}




void loop() {
    // Everything runs in the two pinned tasks
    vTaskDelay(portMAX_DELAY);
}
