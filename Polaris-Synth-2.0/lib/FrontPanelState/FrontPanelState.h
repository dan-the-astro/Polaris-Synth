#include <Arduino.h>
#include <cstdint>
#include <FixedPoint.h>
#include "ExternalADC.h"
#include "IOExpander.h"

// Class to hold the current state of all front panel controls and update them from ADC/IOExpander readings

class FrontPanelState {
    
    // All On/Off states are stored as bools
    // All continuous values are stored as Q16.16 fixed point values unless otherwise noted
    public:
        ExternalADC* adc0 = nullptr; // ADC for front panel reading
        ExternalADC* adc1 = nullptr; // ADC for front panel reading
        IOExpander* io_expander = nullptr; // IO Expander for front panel controls
        IOExpander* io_expander_2 = nullptr; // Second IO Expander for front panel controls


        FrontPanelState(ExternalADC* adc0_ptr, ExternalADC* adc1_ptr, IOExpander* ioexp_ptr, IOExpander* ioexp2_ptr) {
            adc0 = adc0_ptr;
            adc1 = adc1_ptr;
            io_expander = ioexp_ptr;
            io_expander_2 = ioexp2_ptr;


        }

        // LFO parameters
        uint32_t LFO_initial_amount = 0; // Q16.16 fixed point
        uint32_t LFO_frequency = 0; // Q16.16 fixed point
        bool LFO_triangle_wave = false; // is triangle wave enabled
        bool LFO_square_wave = false; // is square wave enabled
        bool LFO_saw_wave = false; // is saw wave enabled
        bool LFO_sine_wave = false; // is sine wave enabled

        // Oscillator A parameters
        uint32_t OscA_frequency = 0; // Q16.16 fixed point
        uint32_t OscA_pulse_width = 0; // Q16.16 fixed point
        bool OscA_triangle_wave = false; // is triangle wave enabled
        bool OscA_square_wave = false; // is square wave enabled
        bool OscA_saw_wave = false; // is saw wave enabled
        bool OscA_sine_wave = false; // is sine wave enabled

        // Oscillator B parameters
        uint32_t OscB_frequency = 0; // Q16.16 fixed point
        uint32_t OscB_pulse_width = 0; // Q16.16 fixed point
        uint32_t OscB_fine = 0; // Q16.16 fixed point
        bool OscB_triangle_wave = false; // is triangle wave enabled
        bool OscB_square_wave = false; // is square wave enabled
        bool OscB_saw_wave = false; // is saw wave enabled
        bool OscB_sine_wave = false; // is sine wave enabled

        // Wheel modulation parameters
        uint32_t Wheel_source_mix = 0; // Q16.16 fixed point
        bool Wheel_freq_A = false; // is Wheel modulating Osc A frequency
        bool Wheel_freq_B = false; // is Wheel modulating Osc B frequency
        bool Wheel_pw_A = false; // is Wheel modulating Osc A pulse width
        bool Wheel_pw_B = false; // is Wheel modulating Osc B pulse width
        bool Wheel_filter_cutoff = false; // is Wheel modulating Filter cutoff

        // Poly mod parameters
        uint32_t Poly_filt_env_amt = 0; // Q16.16 fixed point
        uint32_t Poly_oscB_amount = 0; // Q16.16 fixed point
        bool Poly_freq_A = false; // is Poly modulating Osc A frequency
        bool Poly_pw_A = false; // is Poly modulating Osc A pulse width
        bool Poly_filter_cutoff = false; // is Poly modulating Filter cutoff

        // Mixer parameters
        uint32_t Mixer_oscA_level = 0; // Q16.16 fixed point
        uint32_t Mixer_oscB_level = 0; // Q16.16 fixed point
        uint32_t Mixer_noise_level = 0; // Q16.16 fixed point

        // Filter parameters
        uint32_t Filter_cutoff = 0; // Q16.16 fixed point
        uint32_t Filter_resonance = 0; // Q16.16 fixed point
        uint32_t Filter_env_amt = 0; // Q16.16 fixed point
        uint32_t Filter_attack = 0; // Q16.16 fixed point
        uint32_t Filter_decay = 0; // Q16.16 fixed point    
        uint32_t Filter_sustain = 0; // Q16.16 fixed point
        uint32_t Filter_release = 0; // Q16.16 fixed point
        bool Filter_keytrack = false; // is Filter keytracking enabled

        // Amp envelope parameters
        uint32_t Amp_attack = 0; // Q16.16 fixed point
        uint32_t Amp_decay = 0; // Q16.16 fixed point
        uint32_t Amp_sustain = 0; // Q16.16 fixed point
        uint32_t Amp_release = 0; // Q16.16 fixed point

        // Misc parameters
        uint32_t Master_level = 0; // Q16.16 fixed point
        uint32_t Glide_rate = 0; // Q16.16 fixed point
        bool unison = false; // is Unison mode enabled

    private:
    // counters for which ADC channel to read next
    int adc0_channel = 0; // ADC channel for front panel reading
    int adc1_channel = 0; // ADC channel for front panel reading



    // Initial scan of the front panel to set initial states
    void initialScan();

    // Setters (now private; accept a raw int input; implementation will transform and store as Q16.16 where applicable)
    void setLFOInitialAmount(int value);
    void setLFOFrequency(int value);
    void setOscAFrequency(int value);
    void setOscAPulseWidth(int value);
    void setOscBFrequency(int value);
    void setOscBPulseWidth(int value);
    void setOscBFine(int value);
    void setWheelSourceMix(int value);
    void setPolyFiltEnvAmt(int value);
    void setPolyOscBAmount(int value);
    void setMixerOscALevel(int value);
    void setMixerOscBLevel(int value);
    void setMixerNoiseLevel(int value);
    void setFilterCutoff(int value);
    void setFilterResonance(int value);
    void setFilterEnvAmt(int value);
    void setFilterAttack(int value);
    void setFilterDecay(int value);
    void setFilterSustain(int value);
    void setFilterRelease(int value);
    void setAmpAttack(int value);
    void setAmpDecay(int value);
    void setAmpSustain(int value);
    void setAmpRelease(int value);
    void setMasterLevel(int value);
    void setGlideRate(int value);

};