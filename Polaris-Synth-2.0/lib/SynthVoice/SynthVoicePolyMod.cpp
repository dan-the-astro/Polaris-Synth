// Control-rate poly mod snapshot.
//
// Poly mod is evaluated per sample for the Osc A destinations (its sources -
// Osc B and the filter envelope - live at audio rate), but the filter cutoff
// destination can only consume it at control rate because the biquad
// coefficients are regenerated once per buffer. This snapshot uses the most
// recent Osc B output sample together with the current filter envelope level.

#include "SynthVoice.h"

void SynthVoice::updatePolyModSnapshot(const RenderParams& p) {
    polyFiltSnapshot = polyModOffset(p);
}
