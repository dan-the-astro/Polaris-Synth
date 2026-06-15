// Global LFO, evaluated once per control tick (735Hz).
//
// All enabled waveforms are generated from one shared phase accumulator and
// summed into LFO_Level (Q16.16, +/-1.0 per enabled waveform). LFO_Level is
// kept around for the wheel mod stage, which combines it with noise, the
// initial-amount knob, and the mod wheel position.

#include "SynthEngine.h"
#include "FrontPanelState.h"

void SynthEngine::lfoTick(const FrontPanelState& fp) {
    // Phase increment per control tick from the Q16.16 frequency in Hz:
    // inc = f * 2^32 / 735, with 2^32/735 = 5843543
    uint32_t inc = static_cast<uint32_t>(
        (static_cast<int64_t>(fp.LFO_frequency) * 5843543LL) >> 16);
    LFO_Phase += inc;

    int32_t s = 0;
    if (fp.LFO_saw_wave) {
        s += static_cast<int32_t>(LFO_Phase >> 15) - 65536;
    }
    if (fp.LFO_triangle_wave) {
        s += (LFO_Phase < 0x80000000u)
            ? static_cast<int32_t>(LFO_Phase >> 14) - 65536
            : 196608 - static_cast<int32_t>(LFO_Phase >> 14);
    }
    if (fp.LFO_square_wave) {
        // Fixed 50% duty: just look at the top phase bit
        s += (LFO_Phase & 0x80000000u) ? -65536 : 65536;
    }
    if (fp.LFO_sine_wave) {
        s += sine_table[LFO_Phase >> 24] << 1;
    }
    LFO_Level = s;
}
