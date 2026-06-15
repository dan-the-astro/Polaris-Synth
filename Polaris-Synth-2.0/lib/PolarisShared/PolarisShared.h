// Shared state between the PolarisManager (core 1) and the SynthEngine (core 0).
// All cross-core communication goes through this header:
//  - MIDI note events travel through a FreeRTOS queue
//  - Continuous controllers (pitch bend, mod wheel) are plain 32-bit globals,
//    which are atomic on the ESP32 (aligned 32-bit reads/writes)

#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class FrontPanelState;

// A note on/off event passed from the MIDI handlers to the synth engine
struct MidiEvent {
    uint8_t type;     // 0 = note off, 1 = note on
    uint8_t note;     // MIDI note number 0-127
    uint8_t velocity; // 0-127 (currently unused, P5 style)
};

namespace PolarisShared {

    // Queue of MidiEvent, created in main() before any task starts
    extern QueueHandle_t midiEventQueue;

    // Front panel state object, set by PolarisManager once hardware init is done.
    // The SynthEngine waits for this to become non-null before starting.
    extern FrontPanelState* volatile frontPanel;

    // Pitch bend as a pitch table offset (128 units per semitone, +/-2 semitones)
    extern volatile int32_t pitchBendUnits;

    // Mod wheel position as Q16.16 (0 - 65536)
    extern volatile int32_t modWheelQ16;

    // Set by the USB host when it recovers a wedged MIDI link; the synth
    // engine releases all held voices on the next control tick so a note that
    // was held at the moment of the glitch doesn't stay stuck on.
    extern volatile bool allNotesOff;

    // Create the queue. Call once from setup() before tasks exist.
    void init();
}
