// The 44.1kHz hot loop: renders one 60-sample buffer for all voices and
// writes it to the I2S DMA. Placed in IRAM so flash cache misses can never
// stall audio generation. Everything called from here is inline (SynthVoice
// audio methods) and all lookup tables live in DRAM.

#include "SynthEngine.h"
#include "esp_attr.h"
#include <climits>

void IRAM_ATTR SynthEngine::renderBuffer(const RenderParams& p) {
    const uint8_t n = voiceCount;

    for (int i = 0; i < 2 * kSamplesPerBuffer; i += 2) {
        // One shared noise sample per frame, like the P5's single noise source
        int32_t noise = nextNoise();

        int32_t mix = 0;
        for (uint8_t v = 0; v < n; v++) {
            SynthVoice& vo = voices[v];
            if (!vo.isPlaying) continue;

            // Per-voice chain: Osc B first (poly mod source), then poly mod,
            // then Osc A, mixer, filter, amplifier
            vo.renderOscB(p);
            int32_t poly = vo.polyModOffset(p);
            vo.renderOscA(p, poly);
            vo.mixSources(p, noise);
            vo.runFilter();
            mix += vo.applyAmplifier();
        }

        // Master volume, then scale Q16.16 into the 32-bit DAC range with
        // saturation. The << 12 headroom choice puts five full-scale voices
        // just below digital full scale.
        int64_t s = (static_cast<int64_t>(mix) * p.masterLevel) >> 16;
        s <<= 12;
        if (s > INT32_MAX) s = INT32_MAX;
        if (s < INT32_MIN) s = INT32_MIN;

        sampleBuffer[i] = static_cast<int32_t>(s);     // left
        sampleBuffer[i + 1] = static_cast<int32_t>(s); // right
    }

#if SYNTH_PROFILE
    // Measure compute cost before the write, which blocks to pace the loop
    profileBufferEnd();
#endif

    writeAudioBuffer();
}
