// Control-rate oscillator pitch and pulse width assembly.
//
// Pitch is assembled in the pitch index domain (128 units per semitone):
// the glided note pitch plus knob offsets, pitch bend, and wheel mod. The
// index is converted to a phase increment with the MIDI note table and the
// fractional semitone table. Per-sample poly mod (Osc A only) is applied on
// top of oscA_baseIdx in the render loop.

#include "SynthVoice.h"
#include "FrontPanelState.h"

void SynthVoice::updateGlide(int32_t glideRateQ16) {
    if (glideIdx == targetIdx) return;

    // Small knob values mean glide off: jump immediately
    if (glideRateQ16 < kGlideOffThresholdQ16) {
        glideIdx = targetIdx;
        return;
    }

    // Constant-rate glide: the knob sets the time for a full-range sweep
    int32_t ticks = static_cast<int32_t>(
        (static_cast<int64_t>(glideRateQ16) * kControlRate) >> 16);
    if (ticks < 1) ticks = 1;
    int32_t step = kMaxPitchIndex / ticks;
    if (step < 1) step = 1;

    if (glideIdx < targetIdx) {
        glideIdx += step;
        if (glideIdx > targetIdx) glideIdx = targetIdx;
    } else {
        glideIdx -= step;
        if (glideIdx < targetIdx) glideIdx = targetIdx;
    }
}

void SynthVoice::updatePitchAndPulseWidth(const FrontPanelState& fp,
                                          int32_t wheelPitchUnits,
                                          int32_t wheelPWQ16,
                                          int32_t pitchBendUnits) {
    int32_t noteIdx = glideIdx + detuneUnits;

    // ----- Oscillator A pitch -----
    int32_t idxA = noteIdx
                 + fp.OscA_frequency * kUnitsPerSemitone  // knob, whole semitones
                 + pitchBendUnits;
    if (fp.Wheel_freq_A) {
        idxA += wheelPitchUnits;
    }
    oscA_baseIdx = clampPitchIndex(idxA);
    OscAPhaseIncrement = phaseIncFromIndex(oscA_baseIdx);

    // ----- Oscillator A pulse width -----
    int32_t pwA = fp.OscA_pulse_width;
    if (fp.Wheel_pw_A) {
        pwA += wheelPWQ16;
    }
    oscA_pwBase = pwThresholdFromQ16(pwA);

    // ----- Oscillator B pitch -----
    int32_t idxB;
    if (fp.OscB_keyboard) {
        // Keyboard mode: the frequency knob is a -12..+12 semitone offset
        int32_t semis = (fp.OscB_frequency * 24 + 63) / 127 - 12;
        idxB = noteIdx + semis * kUnitsPerSemitone;
    } else {
        // Free mode: the knob picks an absolute pitch (MIDI note index)
        idxB = fp.OscB_frequency * kUnitsPerSemitone;
    }
    idxB += fp.OscB_fine + pitchBendUnits;
    if (fp.Wheel_freq_B) {
        idxB += wheelPitchUnits;
    }
    uint32_t incB = phaseIncFromIndex(clampPitchIndex(idxB));

    // Lo Freq switch drops Osc B about 7.6 octaves down into LFO territory
    if (fp.OscB_lo_freq) {
        incB /= 200;
    }
    OscBPhaseIncrement = incB;

    // ----- Oscillator B pulse width -----
    int32_t pwB = fp.OscB_pulse_width;
    if (fp.Wheel_pw_B) {
        pwB += wheelPWQ16;
    }
    oscB_pwThresh = pwThresholdFromQ16(pwB);
}
