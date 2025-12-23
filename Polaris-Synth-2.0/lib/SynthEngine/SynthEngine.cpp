#include "Arduino.h"
#include "SynthEngine.h"

SynthEngine::SynthEngine(uint8_t numVoices) {
    // Initialize voices
    voiceCount = numVoices;
    voices = new SynthVoice[voiceCount];

    // Install i2s driver
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);




    // Success message
    Serial.println("SynthEngine initialized with " + String(voiceCount) + " voices.");
}