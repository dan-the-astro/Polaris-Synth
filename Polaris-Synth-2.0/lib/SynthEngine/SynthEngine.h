#pragma once

#include <cstdint>
#include "LookupTables.h"
#include "SynthVoice.h"
#include "driver/i2s.h"

// Set to 1 to log per-buffer compute cost (control block + render, excluding
// the blocking I2S write) once per second over Serial. Use it to read real
// worst-case CPU headroom before changing NUM_VOICES or SAMPLE_RATE.
#define SYNTH_PROFILE 1

class FrontPanelState;

// The synth engine owns all sound generation. It runs as a dedicated task on
// core 0, paced by the blocking I2S DMA writes:
//
//   once per buffer (735Hz "control rate"):
//     drain MIDI queue -> voice allocation
//     LFO -> wheel mod
//     per voice: envelopes, glide, pitch/PW assembly, filter coefficients
//   once per sample (44.1kHz, per voice):
//     Osc B -> poly mod -> Osc A -> mixer -> filter -> amplifier
//   combine voices -> master level -> I2S
class SynthEngine {
    public:

        explicit SynthEngine(uint8_t numVoices);

        // Main engine loop; waits for the front panel hardware to come up,
        // then renders forever. Never returns.
        void run();

    private:

        SynthVoice* voices = nullptr;
        uint8_t voiceCount = 0;

        // Counts control ticks since boot; used as the voice-stealing timestamp
        uint32_t tickCounter = 0;

        // LFO state
        uint32_t LFO_Phase = 0;   // 32-bit phase accumulator
        int32_t LFO_Level = 0;    // Q16.16 mixed waveform output

        // Wheel mod outputs for the current control tick, pre-scaled per
        // destination (computed in SynthEngineWheelMod.cpp)
        int32_t wheelPitchUnits = 0;   // pitch index units for osc destinations
        int32_t wheelPWQ16 = 0;        // Q16.16 duty cycle offset
        int32_t wheelFilterUnits = 0;  // pitch index units for the filter

        // Shared noise source (xorshift32). One generator feeds the mixer of
        // every voice and the wheel mod noise source, like the single noise
        // circuit of the original instrument.
        uint32_t noiseState = 0x6D5A56F1u;

        int32_t sampleBuffer[2 * kSamplesPerBuffer]; // interleaved stereo

#if SYNTH_PROFILE
        // Cycle-count profiling of the compute path (set in run(), consumed in
        // renderBuffer() just before the blocking write). ccount wraps every
        // ~18s at 240MHz, but unsigned subtraction over one buffer is always
        // valid since a buffer is far shorter than the wrap period.
        uint32_t profStartCcount = 0;
        uint32_t profWorstCycles = 0;
        uint64_t profSumCycles = 0;
        uint32_t profBufferCount = 0;
        uint8_t profWorstVoices = 0;
        void profileBufferEnd();   // called at end of compute, before i2s_write
        void profileReport();      // called once per ~second from run()
#endif

        // Config struct for i2s peripheral
        const i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Master mode transmit
            .sample_rate = (int32_t)SAMPLE_RATE, // 44.1k
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

        // Q16.16 white noise, +/-1.0
        inline int32_t nextNoise() {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            return static_cast<int32_t>(noiseState) >> 15;
        }

        // MIDI event handling / voice allocation (SynthEngine.cpp)
        void drainMidiEvents(const FrontPanelState& fp);
        void noteOnEvent(uint8_t note, bool unisonOn);
        void noteOffEvent(uint8_t note, bool unisonOn);
        SynthVoice* allocateVoice(uint8_t note);

        // Control-rate modulation (SynthEngineLFO.cpp / SynthEngineWheelMod.cpp)
        void lfoTick(const FrontPanelState& fp);
        void wheelModTick(const FrontPanelState& fp);

        // Audio-rate rendering of one buffer (SynthEngineVoiceMix.cpp)
        void renderBuffer(const RenderParams& p);

        // I2S setup and output (SynthEngineI2S.cpp)
        void initI2S();
        void writeAudioBuffer();
};
