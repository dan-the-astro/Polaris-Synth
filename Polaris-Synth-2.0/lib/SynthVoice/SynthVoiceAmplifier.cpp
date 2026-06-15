// Amplifier envelope: control-rate ADSR with per-sample linear interpolation
// of the level across each audio buffer.

#include "SynthVoice.h"
#include "FrontPanelState.h"

void SynthVoice::updateAmpEnvelope(const FrontPanelState& fp) {
    stepEnvelope(ampADSRStage, AmpADSRLevel,
                 fp.Amp_attack, fp.Amp_decay, fp.Amp_sustain, fp.Amp_release);

    if (ampADSRStage == EnvStage::Idle) {
        // Amp envelope finished: the voice is silent and can be reallocated
        isPlaying = false;
        gateOn = false;
    }

    // The render loop ramps ampRenderLevel from its current value to the new
    // target over the 60 samples of the coming buffer
    ampRenderDelta = (AmpADSRLevel - ampRenderLevel) / kSamplesPerBuffer;
}
