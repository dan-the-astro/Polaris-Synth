#include <cstdint>
#include "MidiHandler.h"
#include "PolarisShared.h"

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

// Assemble a raw serial MIDI byte stream into complete messages.
// Handles running status; SysEx and real-time messages are skipped.
void MidiHandler::feedByte(uint8_t b) {
    if (b >= 0xF8) {
        return; // real-time messages: ignore, and never interrupt assembly
    }

    if (b & 0x80) { // status byte
        if (b == 0xF0) {            // SysEx start
            inSysEx = true;
            runningStatus = 0;
        } else if (b == 0xF7) {     // SysEx end
            inSysEx = false;
        } else if (b >= 0xF0) {     // other system common: cancels running status
            inSysEx = false;
            runningStatus = 0;
        } else {                    // channel voice status
            inSysEx = false;
            runningStatus = b;
        }
        dataCount = 0;
        return;
    }

    // data byte
    if (inSysEx || runningStatus == 0) {
        return;
    }

    dataBytes[dataCount++] = b;

    // Program change / channel pressure take one data byte, all other channel
    // voice messages take two
    uint8_t status = runningStatus & 0xF0;
    uint8_t needed = (status == 0xC0 || status == 0xD0) ? 1 : 2;

    if (dataCount >= needed) {
        uint8_t msg[3] = { runningStatus, dataBytes[0], needed > 1 ? dataBytes[1] : (uint8_t)0 };
        parse(msg, needed + 1);
        dataCount = 0; // running status persists for the next message
    }
}

void MidiHandler::handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel != kMidiChannel) return;
    if (!PolarisShared::midiEventQueue) return;
    MidiEvent e = { 1, note, velocity };
    xQueueSend(PolarisShared::midiEventQueue, &e, 0);
}

void MidiHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel != kMidiChannel) return;
    if (!PolarisShared::midiEventQueue) return;
    MidiEvent e = { 0, note, velocity };
    xQueueSend(PolarisShared::midiEventQueue, &e, 0);
}

void MidiHandler::handleControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    if (channel != kMidiChannel) return;
    if (controller == 1) {
        // Mod wheel: 0-127 -> Q16.16 0..1
        PolarisShared::modWheelQ16 = (static_cast<int32_t>(value) << 16) / 127;
    }
}

void MidiHandler::handlePitchBend(uint8_t channel, int16_t value) {
    if (channel != kMidiChannel) return;
    // 0..16383 centered at 8192 -> +/-256 pitch index units (+/-2 semitones)
    PolarisShared::pitchBendUnits = ((static_cast<int32_t>(value) - 8192) * 256) / 8192;
}
