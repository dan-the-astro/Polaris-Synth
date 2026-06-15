#pragma once

#include <cstdint>
#include "LookupTables.h"
#include "PolarisTuning.h"

class FrontPanelState;

// Snapshot of the front panel values the audio-rate code needs, taken once
// per control tick so the hot loop never touches shared state and all 60
// samples of a buffer see consistent settings.
struct RenderParams {
    // Waveform enables
    bool aSaw = false, aTri = false, aSqr = false, aSin = false;
    bool bSaw = false, bTri = false, bSqr = false, bSin = false;
    // Poly mod routing and amounts (Q16.16)
    bool polyFreqA = false, polyPwA = false;
    int32_t polyOscBAmt = 0;
    int32_t polyEnvAmt = 0;
    // Mixer levels (Q16.16)
    int32_t lvlA = 0, lvlB = 0, lvlN = 0;
    int32_t masterLevel = 0;
};

// One polyphonic voice. Audio-rate processing is implemented inline in this
// header so the engine's render loop can inline everything; control-rate
// processing (envelopes, pitch assembly, filter coefficients) lives in the
// SynthVoice*.cpp files.
//
// Signal levels are Q16.16: a single full-scale waveform spans -65536..65536.
class SynthVoice {
    public:

        enum class EnvStage : uint8_t { Idle, Attack, Decay, Sustain, Release };

        // ---------------- Voice state ----------------

        // Fixed for lifetime of voice
        bool isPlaying = false;   // envelope still sounding (includes release tail)
        bool gateOn = false;      // key currently held
        uint8_t midiNote = 0;
        uint32_t startTick = 0;   // control tick of note-on, for voice stealing

        // Pitch state in pitch index units (128 per semitone)
        int32_t glideIdx = 60 * kUnitsPerSemitone;  // current (slewed) note pitch
        int32_t targetIdx = 60 * kUnitsPerSemitone; // pitch the glide moves toward
        int32_t detuneUnits = 0;                    // unison detune offset

        // Control-rate cached oscillator settings
        int32_t oscA_baseIdx = 60 * kUnitsPerSemitone; // pre-poly-mod pitch index
        uint32_t OscAPhaseIncrement = 0;
        uint32_t OscBPhaseIncrement = 0;
        uint32_t oscA_pwBase = 0x80000000u;   // square duty thresholds (vs phase)
        uint32_t oscB_pwThresh = 0x80000000u;

        // Per sample changing values (integers)
        uint32_t OscAPhase = 0;
        uint32_t OscBPhase = 0;

        // Per sample changing values (Q16.16 fixed point)
        int32_t OscALevel = 0;
        int32_t OscBLevel = 0;
        int32_t PostMixerLevel = 0;
        int32_t PostFilterLevel = 0;
        int32_t PostAmpLevel = 0;
        int32_t FilterADSRLevel = 0;
        int32_t AmpADSRLevel = 0;

        // Amp level interpolation across one buffer (removes 735Hz zipper)
        int32_t ampRenderLevel = 0;
        int32_t ampRenderDelta = 0;

        // ADSR stages
        EnvStage ampADSRStage = EnvStage::Idle;
        EnvStage filterADSRStage = EnvStage::Idle;

        // Filter: Q2.30 coefficients (b0 b1 b2 a1 a2) and two cascaded
        // direct-form-I biquad stages (24dB/oct total). fltErr* carry the
        // sub-LSB truncation residue into the next sample (first-order error
        // feedback); without it the tiny low-cutoff coefficients quantize the
        // filter output to silence.
        int32_t filterCoef[5] = {0, 0, 0, 0, 0};
        int32_t fltIn1 = 0, fltIn2 = 0, fltOut1 = 0, fltOut2 = 0;
        int32_t flt2In1 = 0, flt2In2 = 0, flt2Out1 = 0, flt2Out2 = 0;
        int32_t fltErr1 = 0, fltErr2 = 0;

        // Control-rate poly mod snapshot used for the filter destination
        // (audio-rate cutoff modulation would require recomputing the
        // coefficients per sample, which the ESP32 can't afford)
        int32_t polyFiltSnapshot = 0;

        // ---------------- Static helpers ----------------

        static inline int32_t clampPitchIndex(int32_t idx) {
            if (idx < 0) return 0;
            if (idx > kMaxPitchIndex) return kMaxPitchIndex;
            return idx;
        }

        // Convert a pitch index to an oscillator phase increment:
        // base note increment from the MIDI table scaled by the fractional
        // semitone factor (frac_table index 1024 = no offset)
        static inline uint32_t phaseIncFromIndex(int32_t idx) {
            idx = clampPitchIndex(idx);
            uint32_t base = static_cast<uint32_t>(midi_note_table[idx >> 7]);
            uint32_t frac = static_cast<uint32_t>(frac_table[1024 + (idx & 127)]);
            return static_cast<uint32_t>((static_cast<uint64_t>(base) * frac) >> 16);
        }

        // Convert a Q16.16 duty cycle to a phase comparison threshold,
        // clamped to 5%..95%
        static inline uint32_t pwThresholdFromQ16(int32_t pwQ16) {
            if (pwQ16 < 3277) pwQ16 = 3277;       // 5%
            if (pwQ16 > 62259) pwQ16 = 62259;     // 95%
            return static_cast<uint32_t>(pwQ16) << 16;
        }

        // ---------------- Control-rate interface ----------------

        // Voice lifecycle (SynthVoiceMixer.cpp)
        void noteOn(uint8_t note, uint32_t tick, int32_t detune);
        void noteOff();

        // Envelopes (SynthVoiceAmplifier.cpp / SynthVoiceFilter.cpp)
        void updateAmpEnvelope(const FrontPanelState& fp);
        void updateFilterEnvelope(const FrontPanelState& fp);

        // Glide and pitch/pulse-width assembly (SynthVoiceOscillators.cpp)
        void updateGlide(int32_t glideRateQ16);
        void updatePitchAndPulseWidth(const FrontPanelState& fp,
                                      int32_t wheelPitchUnits,
                                      int32_t wheelPWQ16,
                                      int32_t pitchBendUnits);

        // Control-rate poly mod snapshot for the filter (SynthVoicePolyMod.cpp)
        void updatePolyModSnapshot(const RenderParams& p);

        // Cutoff assembly + coefficient generation (SynthVoiceFilter.cpp)
        void updateFilterCoefficients(const FrontPanelState& fp, int32_t wheelFilterUnits);

        // ---------------- Audio-rate processing (hot path) ----------------

        // Oscillator B runs first: it is a poly mod source
        inline void renderOscB(const RenderParams& p) {
            OscBPhase += OscBPhaseIncrement;
            int32_t s = 0;
            if (p.bSaw) {
                s += static_cast<int32_t>(OscBPhase >> 15) - 65536;
            }
            if (p.bTri) {
                s += (OscBPhase < 0x80000000u)
                    ? static_cast<int32_t>(OscBPhase >> 14) - 65536
                    : 196608 - static_cast<int32_t>(OscBPhase >> 14);
            }
            if (p.bSqr) {
                s += (OscBPhase >= oscB_pwThresh) ? -65536 : 65536;
            }
            if (p.bSin) {
                s += sine_table[OscBPhase >> 24] << 1;
            }
            OscBLevel = s;
        }

        // Poly mod value for this sample: Osc B output and filter envelope
        // scaled by their amount knobs (all Q16.16)
        inline int32_t polyModOffset(const RenderParams& p) {
            return static_cast<int32_t>(
                (static_cast<int64_t>(p.polyOscBAmt) * OscBLevel +
                 static_cast<int64_t>(p.polyEnvAmt) * FilterADSRLevel) >> 16);
        }

        // Oscillator A: poly mod can deflect its pitch and pulse width
        inline void renderOscA(const RenderParams& p, int32_t poly) {
            if (p.polyFreqA) {
                int32_t idx = oscA_baseIdx + static_cast<int32_t>(
                    (static_cast<int64_t>(poly) * kPolyPitchRangeUnits) >> 16);
                OscAPhase += phaseIncFromIndex(idx);
            } else {
                OscAPhase += OscAPhaseIncrement;
            }

            uint32_t pwThresh = oscA_pwBase;
            if (p.polyPwA) {
                int64_t t = static_cast<int64_t>(pwThresh) +
                            ((static_cast<int64_t>(poly) * kPolyPWRange) >> 16);
                if (t < static_cast<int64_t>(kPWThreshMin)) t = kPWThreshMin;
                if (t > static_cast<int64_t>(kPWThreshMax)) t = kPWThreshMax;
                pwThresh = static_cast<uint32_t>(t);
            }

            int32_t s = 0;
            if (p.aSaw) {
                s += static_cast<int32_t>(OscAPhase >> 15) - 65536;
            }
            if (p.aTri) {
                s += (OscAPhase < 0x80000000u)
                    ? static_cast<int32_t>(OscAPhase >> 14) - 65536
                    : 196608 - static_cast<int32_t>(OscAPhase >> 14);
            }
            if (p.aSqr) {
                s += (OscAPhase >= pwThresh) ? -65536 : 65536;
            }
            if (p.aSin) {
                s += sine_table[OscAPhase >> 24] << 1;
            }
            OscALevel = s;
        }

        // Mix oscillators and (shared) noise by their mixer levels
        inline void mixSources(const RenderParams& p, int32_t noise) {
            int64_t acc = static_cast<int64_t>(p.lvlA) * OscALevel +
                          static_cast<int64_t>(p.lvlB) * OscBLevel +
                          static_cast<int64_t>(p.lvlN) * noise;
            PostMixerLevel = static_cast<int32_t>(acc >> 16);
        }

        // Two cascaded fixed-point biquad stages (Q2.30 coefficients) with
        // first-order error feedback on the truncation
        inline void runFilter() {
            int64_t acc = static_cast<int64_t>(filterCoef[0]) * PostMixerLevel
                        + static_cast<int64_t>(filterCoef[1]) * fltIn1
                        + static_cast<int64_t>(filterCoef[2]) * fltIn2
                        - static_cast<int64_t>(filterCoef[3]) * fltOut1
                        - static_cast<int64_t>(filterCoef[4]) * fltOut2
                        + fltErr1;
            int32_t stage1 = static_cast<int32_t>(acc >> 30);
            fltErr1 = static_cast<int32_t>(acc - (static_cast<int64_t>(stage1) << 30));
            fltIn2 = fltIn1;
            fltIn1 = PostMixerLevel;
            fltOut2 = fltOut1;
            fltOut1 = stage1;

            acc = static_cast<int64_t>(filterCoef[0]) * stage1
                + static_cast<int64_t>(filterCoef[1]) * flt2In1
                + static_cast<int64_t>(filterCoef[2]) * flt2In2
                - static_cast<int64_t>(filterCoef[3]) * flt2Out1
                - static_cast<int64_t>(filterCoef[4]) * flt2Out2
                + fltErr2;
            PostFilterLevel = static_cast<int32_t>(acc >> 30);
            fltErr2 = static_cast<int32_t>(acc - (static_cast<int64_t>(PostFilterLevel) << 30));
            flt2In2 = flt2In1;
            flt2In1 = stage1;
            flt2Out2 = flt2Out1;
            flt2Out1 = PostFilterLevel;
        }

        // Apply the (interpolated) amp envelope; returns this voice's output
        inline int32_t applyAmplifier() {
            PostAmpLevel = static_cast<int32_t>(
                (static_cast<int64_t>(PostFilterLevel) * ampRenderLevel) >> 16);
            ampRenderLevel += ampRenderDelta;
            return PostAmpLevel;
        }

    private:
        // Shared ADSR state machine used by both envelopes (SynthVoice.cpp)
        static void stepEnvelope(EnvStage& stage, int32_t& level,
                                 int32_t atkQ16, int32_t decQ16,
                                 int32_t susQ16, int32_t relQ16);

        friend class SynthEngine;
};
