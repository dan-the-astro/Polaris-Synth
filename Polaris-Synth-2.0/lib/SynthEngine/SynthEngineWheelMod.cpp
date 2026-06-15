// Wheel mod: the Prophet-5 style global modulation bus.
//
//   source     = crossfade(LFO, pink-ish noise) by the source mix knob
//   wheel part = source * mod wheel position
//   plus the initial-amount knob applying the LFO regardless of the wheel
//
// The combined level is pre-scaled here into one offset per destination type
// (oscillator pitch, pulse width, filter cutoff); the per-voice code only
// adds the offset if the corresponding destination switch is on.

#include "SynthEngine.h"
#include "FrontPanelState.h"
#include "PolarisShared.h"

void SynthEngine::wheelModTick(const FrontPanelState& fp) {
    // Crossfade LFO and noise (all Q16.16)
    int32_t mixK = fp.Wheel_source_mix;          // 0 = LFO ... 1.0 = noise
    int32_t noise = nextNoise();
    int32_t source = static_cast<int32_t>(
        (static_cast<int64_t>(LFO_Level) * (65536 - mixK) +
         static_cast<int64_t>(noise) * mixK) >> 16);

    // Scale by the wheel position
    int32_t m = static_cast<int32_t>(
        (static_cast<int64_t>(source) * PolarisShared::modWheelQ16) >> 16);

    // The initial amount knob always applies the LFO to the destinations,
    // regardless of the wheel position
    m += static_cast<int32_t>(
        (static_cast<int64_t>(LFO_Level) * fp.LFO_initial_amount) >> 16);

    // Pre-scale per destination
    wheelPitchUnits = static_cast<int32_t>(
        (static_cast<int64_t>(m) * kWheelPitchRangeUnits) >> 16);
    wheelPWQ16 = static_cast<int32_t>(
        (static_cast<int64_t>(m) * kWheelPWRangeQ16) >> 16);
    wheelFilterUnits = static_cast<int32_t>(
        (static_cast<int64_t>(m) * kWheelFilterRangeUnits) >> 16);
}
