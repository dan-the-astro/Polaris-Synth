// Voice lifecycle: initialization on MIDI note-on, release on note-off.
// Everything initialized here is per-voice runtime state; all patch
// parameters stay in FrontPanelState and are read each control tick.

#include "SynthVoice.h"

void SynthVoice::noteOn(uint8_t note, uint32_t tick, int32_t detune) {
    bool fresh = !isPlaying;

    midiNote = note;
    startTick = tick;
    gateOn = true;
    detuneUnits = detune;
    targetIdx = static_cast<int32_t>(note) * kUnitsPerSemitone;

    if (fresh) {
        // New voice: deterministic oscillator start and silent envelopes.
        // (A stolen voice keeps its phases and current envelope levels so the
        // retrigger doesn't click.)
        OscAPhase = 0;
        OscBPhase = 0;
        AmpADSRLevel = 0;
        FilterADSRLevel = 0;
        ampRenderLevel = 0;
        ampRenderDelta = 0;

        // Start the filter from rest
        fltIn1 = fltIn2 = fltOut1 = fltOut2 = 0;
        flt2In1 = flt2In2 = flt2Out1 = flt2Out2 = 0;
        fltErr1 = fltErr2 = 0;

        // Glide starts from wherever this voice last was (glideIdx keeps its
        // previous value); updateGlide() jumps instantly when glide is off.
    }

    ampADSRStage = EnvStage::Attack;
    filterADSRStage = EnvStage::Attack;
    isPlaying = true;
}

void SynthVoice::noteOff() {
    gateOn = false;
    ampADSRStage = EnvStage::Release;
    filterADSRStage = EnvStage::Release;
}
