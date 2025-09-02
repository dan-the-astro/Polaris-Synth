class MidiHandler {
    public:
        void parse(const uint8_t* buf, uint8_t size);

    private:
        void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
        void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
        void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value);
        void handlePitchBend(uint8_t channel, int16_t value);
        // Add more MIDI event handlers as needed
};