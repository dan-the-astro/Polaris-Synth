// Filter envelope and cutoff assembly.
//
// The cutoff is assembled in the pitch index domain (128 units per semitone),
// so every modulation source is a simple additive offset and the final
// frequency conversion reuses the oscillator pitch tables:
//
//   cutoff index = knob + keyboard tracking + wheel mod + ADSR*env amount + poly mod
//
// The index is then converted to a phase-increment style frequency value and
// run through the RBJ low-pass coefficient calculation. Coefficients are
// recomputed at control rate (735Hz per voice), which is the practical limit
// for the trig involved.

#include "SynthVoice.h"
#include "FrontPanelState.h"
#include "FilterMath.h"

void SynthVoice::updateFilterEnvelope(const FrontPanelState& fp) {
    stepEnvelope(filterADSRStage, FilterADSRLevel,
                 fp.Filter_attack, fp.Filter_decay, fp.Filter_sustain, fp.Filter_release);
}

void SynthVoice::updateFilterCoefficients(const FrontPanelState& fp, int32_t wheelFilterUnits) {
    int32_t idx = fp.Filter_cutoff;

    // Keyboard tracking: a key-based offset around the cutoff knob's
    // frequency, full tracking centered on middle C
    if (fp.Filter_keytrack) {
        idx += (static_cast<int32_t>(midiNote) - 60) * kUnitsPerSemitone;
    }

    // Wheel mod offset (already scaled to index units by the engine)
    if (fp.Wheel_filter_cutoff) {
        idx += wheelFilterUnits;
    }

    // ADSR contribution: envelope level scaled by the env amount knob
    int32_t envQ16 = static_cast<int32_t>(
        (static_cast<int64_t>(FilterADSRLevel) * fp.Filter_env_amt) >> 16);
    idx += static_cast<int32_t>(
        (static_cast<int64_t>(envQ16) * kFilterEnvRangeUnits) >> 16);

    // Poly mod contribution (control-rate snapshot)
    if (fp.Poly_filter_cutoff) {
        idx += static_cast<int32_t>(
            (static_cast<int64_t>(polyFiltSnapshot) * kPolyFilterRangeUnits) >> 16);
    }

    uint32_t cutoffPhaseInc = phaseIncFromIndex(idx); // clamps internally

    // Resonance knob is the biquad Q factor (0.5 .. 10)
    float q = static_cast<float>(fp.Filter_resonance) * (1.0f / 65536.0f);
    if (q < 0.5f) q = 0.5f;
    if (q > 10.0f) q = 10.0f;

    // The trig stays at control rate; the render loop interpolates between the
    // previous coefficients and this target across the buffer so the cutoff
    // moves smoothly at audio rate. Difference is taken in 64-bit because a1
    // spans nearly the full int32 range (Q2.30, |a1| can approach 2.0).
    int32_t target[5];
    polarisLowpassCoeffsQ30(cutoffPhaseInc, q, target);

    if (filterCoefPrimed) {
        for (int k = 0; k < 5; k++) {
            filterCoefDelta[k] = static_cast<int32_t>(
                (static_cast<int64_t>(target[k]) - filterCoef[k]) / kSamplesPerBuffer);
        }
    } else {
        // First set after a fresh note-on: snap, don't ramp from stale state
        for (int k = 0; k < 5; k++) {
            filterCoef[k] = target[k];
            filterCoefDelta[k] = 0;
        }
        filterCoefPrimed = true;
    }
}
