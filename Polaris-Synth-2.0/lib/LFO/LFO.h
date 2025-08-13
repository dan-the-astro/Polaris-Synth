#include <cstdint>
#pragma once

// Header for LFO (Low Frequency Oscillator) class
class LFO {

    public:
    // 
    int LFO_initial_amount_knob;

    int LFO_phase_increment;

    int LFO_wave_level;

    int LFO_initial_amount_level;

    void LFO::genLFO_WaveLevel();

    void LFO::genLFO_InitialAmountLevel();


    private:

    uint32_t LFO_phase;

};