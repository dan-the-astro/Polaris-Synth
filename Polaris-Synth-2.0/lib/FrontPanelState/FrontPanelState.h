#include <Arduino.h>
#include <cstdint>
#include <climits>
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

        // Task that runs in the background to periodically check IO Expanders and ADCs
        void loopTask() {
                checkIOExpanders();
                checkADCs();
                yield(); // yield to other tasks
        }


        FrontPanelState(ExternalADC* adc0_ptr, ExternalADC* adc1_ptr, IOExpander* ioexp_ptr, IOExpander* ioexp2_ptr,
                        gpio_num_t exp1_int_pin = static_cast<gpio_num_t>(34),
                        gpio_num_t exp2_int_pin = static_cast<gpio_num_t>(35)) {
            adc0 = adc0_ptr;
            adc1 = adc1_ptr;
            io_expander = ioexp_ptr;
            io_expander_2 = ioexp2_ptr;

            // Configure INT pins if valid numbers were provided
            if (exp1_int_pin != static_cast<gpio_num_t>(-1)) {
                gpio_config_t cfg = {};
                cfg.intr_type = GPIO_INTR_NEGEDGE; // MCP23017 INT active low
                cfg.mode = GPIO_MODE_INPUT;
                cfg.pin_bit_mask = 1ULL << exp1_int_pin;
                cfg.pull_up_en = GPIO_PULLUP_ENABLE; // internal pull-up (INT is open-drain)
                cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
                gpio_config(&cfg);
            }
            if (exp2_int_pin != static_cast<gpio_num_t>(-1)) {
                gpio_config_t cfg = {};
                cfg.intr_type = GPIO_INTR_NEGEDGE;
                cfg.mode = GPIO_MODE_INPUT;
                cfg.pin_bit_mask = 1ULL << exp2_int_pin;
                cfg.pull_up_en = GPIO_PULLUP_ENABLE;
                cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
                gpio_config(&cfg);
            }

            // Ensure ISR service installed
            static bool isrInstalled = false;
            if (!isrInstalled) {
                if (gpio_install_isr_service(0) == ESP_OK) {
                    isrInstalled = true;
                }
            }

            // Register ISR handlers if pins supplied
            if (exp1_int_pin != static_cast<gpio_num_t>(-1)) {
                gpio_isr_handler_add(exp1_int_pin, &FrontPanelState::expander1Isr, this);
            }
            if (exp2_int_pin != static_cast<gpio_num_t>(-1)) {
                gpio_isr_handler_add(exp2_int_pin, &FrontPanelState::expander2Isr, this);
            }

            // Enable interrupt-on-change inside expanders
            if (io_expander) io_expander->attachInterruptLine();
            if (io_expander_2) io_expander_2->attachInterruptLine();

            // Run initial scan to set initial states
            initialScan();
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
        bool OscB_lo_freq = false; // is Osc B in low-frequency mode
        bool OscB_keyboard = false; // is Osc B keyboard-tracking enabled

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
        // Static ISR trampolines (C-style signature). They set IOExpander flag and our own flag.
        static void IRAM_ATTR expander1Isr(void* arg) {
            auto* self = static_cast<FrontPanelState*>(arg);
            if (self && self->io_expander) {
                self->io_expander->isrFlagSet();
            }
        }
        static void IRAM_ATTR expander2Isr(void* arg) {
            auto* self = static_cast<FrontPanelState*>(arg);
            if (self && self->io_expander_2) {
                self->io_expander_2->isrFlagSet();
            }
        }
        // counters for which ADC channel to read next
        int adc0_channel = 0; // ADC channel for front panel reading
        int adc1_channel = 0; // ADC channel for front panel reading

        // Flags to track if first conversion has been initiated for pipelining
        bool adc0_first_conversion_started = false;
        bool adc1_first_conversion_started = false;

        // Biquad filter state for ADC readings
        // Store previous inputs and outputs for each ADC channel
        struct BiquadState {
            int16_t x1 = 0; // x[n-1]
            int16_t x2 = 0; // x[n-2]
            int16_t y1 = 0; // y[n-1]
            int16_t y2 = 0; // y[n-2]
        };
        
        BiquadState adc0_biquad_state[16]; // Biquad state for ADC0 channels 0-15
        BiquadState adc1_biquad_state[10]; // Biquad state for ADC1 channels 0-9
        
        // Second-order Butterworth low-pass biquad filter
        // Designed for strong noise rejection with more smoothing
        // Cutoff frequency ~5Hz at typical ADC sample rate (~150Hz per channel)
        // Using fixed-point coefficients scaled by 32768 (2^15) for integer math
        inline int16_t applyBiquadFilter(BiquadState& state, int16_t new_reading) const {
            // Biquad coefficients for a Butterworth low-pass filter
            // These values provide stronger noise filtering with more smoothing
            // b0 = 0.0093, b1 = 0.0186, b2 = 0.0093 (scaled by 32768)
            // a1 = -1.7090, a2 = 0.7463 (scaled by 32768)
            constexpr int32_t b0 = 305;    // 0.0093 * 32768
            constexpr int32_t b1 = 611;    // 0.0186 * 32768
            constexpr int32_t b2 = 305;    // 0.0093 * 32768
            constexpr int32_t a1 = -56017; // -1.7090 * 32768
            constexpr int32_t a2 = 24453;  // 0.7463 * 32768
            
            // Direct Form I implementation: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
            int32_t output = b0 * static_cast<int32_t>(new_reading)
                           + b1 * static_cast<int32_t>(state.x1)
                           + b2 * static_cast<int32_t>(state.x2)
                           - a1 * static_cast<int32_t>(state.y1)
                           - a2 * static_cast<int32_t>(state.y2);
            
            // Scale back down (divide by 32768)
            output >>= 15;
            
            // Clamp to valid ADC range (non-negative, max 12-bit value)
            if (output > 4095) output = 4095;
            if (output < 0) output = 0;
            
            int16_t result = static_cast<int16_t>(output);
            
            // Update state
            state.x2 = state.x1;
            state.x1 = new_reading;
            state.y2 = state.y1;
            state.y1 = result;
            
            return result;
        }

        // Initial scan of the front panel to set initial states
        void initialScan();

        // Periodic check of IO Expanders to update states
        void checkIOExpanders(); 

        // Periodic check of ADCs to update states
        void checkADCs(); 

        // State machines for ADC reading (non-blocking)
        void ADC0StateMachine();
        void ADC1StateMachine();

        // Setters (now private; accept a raw int input; implementation will transform and store as Q16.16 where applicable)
        void setLFOInitialAmount(int16_t value);
        void setLFOFrequency(int16_t value);
        void setOscAFrequency(int16_t value);
        void setOscAPulseWidth(int16_t value);
        void setOscBFrequency(int16_t value);
        void setOscBPulseWidth(int16_t value);
        void setOscBFine(int16_t value);
        void setWheelSourceMix(int16_t value);
        void setPolyFiltEnvAmt(int16_t value);
        void setPolyOscBAmount(int16_t value);
        void setMixerOscALevel(int16_t value);
        void setMixerOscBLevel(int16_t value);
        void setMixerNoiseLevel(int16_t value);
        void setFilterCutoff(int16_t value);
        void setFilterResonance(int16_t value);
        void setFilterEnvAmt(int16_t value);
        void setFilterAttack(int16_t value);
        void setFilterDecay(int16_t value);
        void setFilterSustain(int16_t value);
        void setFilterRelease(int16_t value);
        void setAmpAttack(int16_t value);
        void setAmpDecay(int16_t value);
        void setAmpSustain(int16_t value);
        void setAmpRelease(int16_t value);
        void setMasterLevel(int16_t value);
        void setGlideRate(int16_t value);

};