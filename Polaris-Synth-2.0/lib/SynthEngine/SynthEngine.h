#include <cstdint>
#include "SynthVoice.h"
#include "driver/i2s.h"
#include "LookupTables.h"

class SynthEngine {
    public:
    
        SynthEngine(uint8_t numVoices);
    
    private:

        SynthVoice* voices = nullptr;
        uint8_t voiceCount = 0;

        // LFO parameters

        int32_t LFO_Level = 0; // Q16.16 fixed point

        int32_t LFO_Phase = 0; // 32-bit integer phase accumulator

        // Other synth engine parameters and state variables

        int32_t sampleBuffer[120]; // Buffer for audio samples

        // Config struct for i2s peripheral
        const i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Master mode transmit
            .sample_rate = SAMPLE_RATE, // 44.1k
            .bits_per_sample = (i2s_bits_per_sample_t)(32), // 32b per sample
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo format
            .communication_format = I2S_COMM_FORMAT_STAND_I2S, // Standard Philips format
            .intr_alloc_flags = 0, // default interrupt priority
            .dma_buf_count = 4, // 4 DMA buffers
            .dma_buf_len = 60, // 60 samples in buffer
            .use_apll = false // No idea what this does, works with default value
        };

        // Pin config for i2s peripheral
        const i2s_pin_config_t pin_config = { // i2s pins
            .bck_io_num = 27,
            .ws_io_num = 14,
            .data_out_num = 12,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
};