// I2S output. The driver is configured for 32-bit stereo Philips format at
// 44.1kHz with 4 x 60-sample DMA buffers; i2s_write() blocks until DMA space
// is free, which is what paces the whole engine loop.

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SynthEngine.h"

void SynthEngine::initI2S() {
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("SynthEngine: i2s_driver_install failed: %d\n", err);
    }
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void SynthEngine::writeAudioBuffer() {
    size_t bytes_written = 0;
    // 60 samples * 2 channels * 4 bytes = 480 bytes per buffer
    esp_err_t err = i2s_write(I2S_NUM_0, sampleBuffer, sizeof(sampleBuffer), &bytes_written, portMAX_DELAY);

    // The blocking write paces the whole engine. If I2S is broken it returns
    // immediately, and the engine loop must not be allowed to spin: IDLE0 is
    // task-watchdog-watched and this framework aborts on its starvation.
    if (err != ESP_OK || bytes_written == 0) {
        vTaskDelay(1);
    }
}
