#pragma once

#include <Arduino.h>
#include "MidiHandler.h"

// DIN MIDI input on UART2. The RX pin defaults to GPIO 3 (the same pin the
// 1.x firmware used for serial MIDI). The ESP32 GPIO matrix routes the pin to
// UART2, leaving UART0 TX (GPIO 1) free for debug output. No TX is configured
// as MIDI out is not wired.
class MidiDin {
    public:
        explicit MidiDin(int8_t rxPin = 3, int8_t txPin = -1)
            : _rx(rxPin), _tx(txPin) {}

        void begin() {
            Serial2.begin(31250, SERIAL_8N1, _rx, _tx);
        }

        // Drain pending UART bytes into the MIDI parser. Call regularly from
        // the manager loop.
        void poll() {
            while (Serial2.available() > 0) {
                _parser.feedByte(static_cast<uint8_t>(Serial2.read()));
            }
        }

    private:
        int8_t _rx;
        int8_t _tx;
        // Dedicated parser so DIN running status never mixes with USB MIDI
        MidiHandler _parser;
};
