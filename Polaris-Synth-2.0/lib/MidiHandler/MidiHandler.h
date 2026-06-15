#pragma once

#include <cstdint>

// Parses MIDI messages and forwards them to the synth engine:
//  - Note on/off events are pushed onto PolarisShared::midiEventQueue
//  - Pitch bend / mod wheel update the shared globals directly
//
// parse() takes complete messages (as delivered by USB-MIDI event packets).
// feedByte() assembles messages from a raw serial byte stream (DIN MIDI),
// including running status, and calls parse() internally. Use one
// MidiHandler instance per stream so running status state stays separate.
class MidiHandler {
    public:
        // MIDI channel to respond to, 0-based (0 = MIDI channel 1)
        static constexpr uint8_t kMidiChannel = 0;

        void parse(const uint8_t* buf, uint8_t size);
        void feedByte(uint8_t b);

    private:
        // Serial stream assembly state (DIN MIDI)
        uint8_t runningStatus = 0;
        uint8_t dataBytes[2] = {0, 0};
        uint8_t dataCount = 0;
        bool inSysEx = false;

        void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
        void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
        void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value);
        void handlePitchBend(uint8_t channel, int16_t value);
        // Add more MIDI event handlers as needed
};
