#include "SynthVoice.h"

// Number of control ticks a Q16.16 time-in-seconds value corresponds to
static inline int32_t envTicks(int32_t secondsQ16) {
    int32_t t = static_cast<int32_t>((static_cast<int64_t>(secondsQ16) * kControlRate) >> 16);
    return t < 1 ? 1 : t;
}

// Shared ADSR state machine, stepped once per control tick (735Hz).
//
// The knob times are treated as desired rates of change rather than promised
// durations: every increment is recomputed from the current knob values each
// tick, so turning a knob immediately affects envelopes already in flight.
// Each rate is full-scale referenced (the time to traverse 0..1).
void SynthVoice::stepEnvelope(EnvStage& stage, int32_t& level,
                              int32_t atkQ16, int32_t decQ16,
                              int32_t susQ16, int32_t relQ16) {
    switch (stage) {
        case EnvStage::Attack: {
            int32_t inc = 65536 / envTicks(atkQ16);
            if (inc < 1) inc = 1;
            level += inc;
            if (level >= 65536) {
                level = 65536;
                stage = EnvStage::Decay;
            }
            break;
        }
        case EnvStage::Decay: {
            int32_t dec = 65536 / envTicks(decQ16);
            if (dec < 1) dec = 1;
            level -= dec;
            if (level <= susQ16) {
                level = susQ16;
                stage = EnvStage::Sustain;
            }
            break;
        }
        case EnvStage::Sustain:
            // Track the sustain knob while the key is held
            level = susQ16;
            break;
        case EnvStage::Release: {
            int32_t dec = 65536 / envTicks(relQ16);
            if (dec < 1) dec = 1;
            level -= dec;
            if (level <= 0) {
                level = 0;
                stage = EnvStage::Idle;
            }
            break;
        }
        case EnvStage::Idle:
            break;
    }
}
