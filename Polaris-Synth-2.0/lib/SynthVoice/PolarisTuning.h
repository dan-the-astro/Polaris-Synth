#pragma once

#include <cstdint>

// Central tuning constants for the synth engine.
//
// Pitch index domain: all pitch-related modulation is additive in "pitch
// index units" where 128 units = 1 semitone, exactly matching the resolution
// of frac_table in LookupTables. A pitch index i maps to the frequency of
// MIDI note (i / 128) plus (i % 128)/128 of a semitone, so converting an
// index to an oscillator phase increment is two table lookups and a multiply.

// Audio buffer layout: 60 stereo samples per DMA buffer at 44100Hz means the
// control-rate code (LFO, envelopes, filter coefficients) runs at 735Hz.
static constexpr int32_t kSamplesPerBuffer = 60;
static constexpr int32_t kControlRate = 735;

static constexpr int32_t kUnitsPerSemitone = 128;
// Highest usable pitch index: keeps (idx >> 7) within the MIDI note table and
// the fractional part within the +1 semitone span of frac_table
static constexpr int32_t kMaxPitchIndex = 16255;

// Modulation depth scaling. Each value is the effect of a full-scale (1.0)
// modulation signal on its destination.
static constexpr int32_t kWheelPitchRangeUnits  = 512;    // +/-4 semitones of vibrato
static constexpr int32_t kWheelFilterRangeUnits = 3072;   // +/-24 semitones of cutoff
static constexpr int32_t kWheelPWRangeQ16       = 26214;  // +/-0.4 duty cycle
static constexpr int32_t kPolyPitchRangeUnits   = 2048;   // +/-16 semitones on Osc A
static constexpr int32_t kPolyFilterRangeUnits  = 6144;   // +/-48 semitones of cutoff
static constexpr int64_t kPolyPWRange           = 1932735283LL; // 0.45 * 2^32 duty shift
static constexpr int32_t kFilterEnvRangeUnits   = 7680;   // +60 semitones at full env amount

// Square wave duty cycle clamps (5%..95% of the 2^32 phase range) so the
// pulse can never modulate itself into silence
static constexpr uint32_t kPWThreshMin = 214748365u;
static constexpr uint32_t kPWThreshMax = 4080218931u;

// Glide knob values below this (Q16.16 seconds, ~50ms) mean "glide off"
static constexpr int32_t kGlideOffThresholdQ16 = 3277;

// Unison detune per voice in pitch index units (a few cents each way)
static constexpr int32_t kUnisonDetuneUnits[8] = { 0, 5, -5, 9, -9, 13, -13, 17 };
