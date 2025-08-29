#include <cstdint>

class FrontPanelState {
    
    // All On/Off states are stored as bools
    // All continuous values are stored as Q16.16 fixed point values unless otherwise noted
    public:

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
};