#include <cstdint>
#include "MidiHandler.h"

void MidiHandler::parse(const uint8_t* buf, uint8_t size) {
    if (size == 0) {
        return;
    }
    uint8_t status = buf[0] & 0xF0;
    uint8_t channel = buf[0] & 0x0F;
    switch (status) {
        case 0x90: // Note On
            if (size >= 3) {
                uint8_t note = buf[1];
                uint8_t velocity = buf[2];
                if (velocity > 0) {
                    handleNoteOn(channel, note, velocity);
                } else {
                    handleNoteOff(channel, note, velocity);
                }
            }
            break;
        case 0x80: // Note Off
            if (size >= 3) {
                uint8_t note = buf[1];
                uint8_t velocity = buf[2];
                handleNoteOff(channel, note, velocity);
            }
            break;
        case 0xB0: // Control Change
            if (size >= 3) {
                uint8_t controller = buf[1];
                uint8_t value = buf[2];
                handleControlChange(channel, controller, value);
            }
            break;
        case 0xE0: // Pitch Bend
            if (size >= 3) {
                int16_t value = (static_cast<int16_t>(buf[2]) << 7) | buf[1];
                handlePitchBend(channel, value);
            }
            break;
        // Add more MIDI event types as needed
        default:
            break;
    }
}