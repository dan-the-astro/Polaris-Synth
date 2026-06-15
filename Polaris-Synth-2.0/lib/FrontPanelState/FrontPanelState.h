#pragma once

#include <Arduino.h>
#include <cstdint>
#include <climits>
#include <FixedPoint.h>
#include "ExternalADC.h"
#include "IOExpander.h"

struct PatchData;

// Class to hold the current state of all front panel controls and update them from ADC/IOExpander readings
//
// Values are stored in the form the SynthEngine consumes directly:
//  - Levels/amounts are Q16.16 fixed point (65536 = 1.0)
//  - Times are Q16.16 seconds
//  - Pitch-related controls use "pitch index units": 128 units per semitone,
//    matching the resolution of frac_table in LookupTables (index = note*128 + frac)
//
// The object also implements LIVE vs PATCH mode: in LIVE mode the physical
// knobs/switches drive the values; after a patch is loaded the panel is
// detached until LIVE mode is re-enabled (values jump to the current knob
// positions at that moment).

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
        }


        FrontPanelState(ExternalADC* adc0_ptr, ExternalADC* adc1_ptr, IOExpander* ioexp_ptr, IOExpander* ioexp2_ptr,
                        gpio_num_t exp1_int_pin = static_cast<gpio_num_t>(34),
                        gpio_num_t exp2_int_pin = static_cast<gpio_num_t>(35)) {
            adc0 = adc0_ptr;
            adc1 = adc1_ptr;
            io_expander = ioexp_ptr;
            io_expander_2 = ioexp2_ptr;
            int1_pin = exp1_int_pin;
            int2_pin = exp2_int_pin;

            // The expander INT lines are POLLED from the 1kHz manager loop,
            // not interrupt-driven. GPIO 34/35 are input-only pins with no
            // internal pull resistors: if a line is ever left undriven
            // (loose wire, unpowered expander) it floats, and an edge ISR on
            // a floating pin storms the GPIO dispatcher until the interrupt
            // watchdog resets the chip. The MCP23017 latches INT low until
            // serviced, so polling the level loses no events.
            gpio_config_t cfg = {};
            cfg.intr_type = GPIO_INTR_DISABLE;
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pin_bit_mask = 0;
            if (exp1_int_pin != static_cast<gpio_num_t>(-1)) {
                cfg.pin_bit_mask |= 1ULL << exp1_int_pin;
            }
            if (exp2_int_pin != static_cast<gpio_num_t>(-1)) {
                cfg.pin_bit_mask |= 1ULL << exp2_int_pin;
            }
            cfg.pull_up_en = GPIO_PULLUP_DISABLE;   // not available on 34-39
            cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
            if (cfg.pin_bit_mask != 0) {
                gpio_config(&cfg);
            }

            // Enable interrupt-on-change inside expanders so INT latches
            // changes between polls
            if (io_expander) io_expander->attachInterruptLine();
            if (io_expander_2) {
                // UI buttons on GPB0-3 short to ground when pressed
                io_expander_2->enablePullups(0x0F00);
                io_expander_2->attachInterruptLine();
            }

            // Run initial scan to set initial states
            initialScan();
        }

        // LFO parameters
        int32_t LFO_initial_amount = 0; // Q16.16 fixed point
        int32_t LFO_frequency = 0; // Q16.16 fixed point, Hz
        bool LFO_triangle_wave = false; // is triangle wave enabled
        bool LFO_square_wave = false; // is square wave enabled
        bool LFO_saw_wave = false; // is saw wave enabled
        bool LFO_sine_wave = false; // is sine wave enabled

        // Oscillator A parameters
        int32_t OscA_frequency = 0; // plain integer semitone offset, -24..+24
        int32_t OscA_pulse_width = 0; // Q16.16 fixed point, 0..1 duty cycle
        bool OscA_triangle_wave = false; // is triangle wave enabled
        bool OscA_square_wave = false; // is square wave enabled
        bool OscA_saw_wave = false; // is saw wave enabled
        bool OscA_sine_wave = false; // is sine wave enabled

        // Oscillator B parameters
        int32_t OscB_frequency = 0; // plain integer MIDI note index 0..127 (also scaled to +/-12 semitones in keyboard mode)
        int32_t OscB_pulse_width = 0; // Q16.16 fixed point, 0..1 duty cycle
        int32_t OscB_fine = 0; // plain integer pitch index units, 0..128 (one semitone up)
        bool OscB_triangle_wave = false; // is triangle wave enabled
        bool OscB_square_wave = false; // is square wave enabled
        bool OscB_saw_wave = false; // is saw wave enabled
        bool OscB_sine_wave = false; // is sine wave enabled
        bool OscB_lo_freq = false; // is Osc B in low-frequency mode
        bool OscB_keyboard = false; // is Osc B keyboard-tracking enabled

        // Wheel modulation parameters
        int32_t Wheel_source_mix = 0; // Q16.16 fixed point, 0 = LFO ... 1 = noise
        bool Wheel_freq_A = false; // is Wheel modulating Osc A frequency
        bool Wheel_freq_B = false; // is Wheel modulating Osc B frequency
        bool Wheel_pw_A = false; // is Wheel modulating Osc A pulse width
        bool Wheel_pw_B = false; // is Wheel modulating Osc B pulse width
        bool Wheel_filter_cutoff = false; // is Wheel modulating Filter cutoff

        // Poly mod parameters
        int32_t Poly_filt_env_amt = 0; // Q16.16 fixed point
        int32_t Poly_oscB_amount = 0; // Q16.16 fixed point
        bool Poly_freq_A = false; // is Poly modulating Osc A frequency
        bool Poly_pw_A = false; // is Poly modulating Osc A pulse width
        bool Poly_filter_cutoff = false; // is Poly modulating Filter cutoff

        // Mixer parameters
        int32_t Mixer_oscA_level = 0; // Q16.16 fixed point
        int32_t Mixer_oscB_level = 0; // Q16.16 fixed point
        int32_t Mixer_noise_level = 0; // Q16.16 fixed point

        // Filter parameters
        int32_t Filter_cutoff = 0; // plain integer pitch index units 0..16255 (8.18Hz..12.5kHz, exponential)
        int32_t Filter_resonance = 0; // Q16.16 fixed point, Q factor 0.5..10
        int32_t Filter_env_amt = 0; // Q16.16 fixed point
        int32_t Filter_attack = 0; // Q16.16 fixed point, seconds
        int32_t Filter_decay = 0; // Q16.16 fixed point, seconds
        int32_t Filter_sustain = 0; // Q16.16 fixed point, 0..1
        int32_t Filter_release = 0; // Q16.16 fixed point, seconds
        bool Filter_keytrack = false; // is Filter keytracking enabled

        // Amp envelope parameters
        int32_t Amp_attack = 0; // Q16.16 fixed point, seconds
        int32_t Amp_decay = 0; // Q16.16 fixed point, seconds
        int32_t Amp_sustain = 0; // Q16.16 fixed point, 0..1
        int32_t Amp_release = 0; // Q16.16 fixed point, seconds

        // Misc parameters
        int32_t Master_level = 0; // Q16.16 fixed point
        int32_t Glide_rate = 0; // Q16.16 fixed point, seconds for a full-range sweep
        bool unison = false; // is Unison mode enabled

        // ---------- LIVE / PATCH mode ----------

        // True when the physical panel drives the values (LIVE mode)
        bool isLive() const { return live_mode; }

        // Switch between LIVE (true) and PATCH (false) mode. Entering LIVE
        // mode re-reads the switch states from the IO expanders.
        void setLiveMode(bool live);

        // Copy every panel parameter into / out of a patch structure.
        // applyPatch() also drops the panel into PATCH mode.
        void capturePatch(PatchData& patch) const;
        void applyPatch(const PatchData& patch);

        // ---------- UI buttons ----------

        // Fetch the next queued UI button press (0-3, on IO expander 2 GPB0-3).
        // Returns false if no press is pending. Producer and consumer both run
        // on the PolarisManager task, so a simple ring buffer is enough.
        bool getButtonEvent(uint8_t& button) {
            if (btn_head == btn_tail) return false;
            button = btn_events[btn_tail];
            btn_tail = (btn_tail + 1) % sizeof(btn_events);
            return true;
        }

        // ---------- Raw ADC readings (diagnostic screen) ----------

        static constexpr int kNumADC0 = 16; // knobs on ADC0
        static constexpr int kNumADC1 = 10; // knobs on ADC1

        // Latest unfiltered reading for each channel, for the raw-ADC display
        int16_t rawADC0(int ch) const { return (ch >= 0 && ch < kNumADC0) ? adc0_raw[ch] : 0; }
        int16_t rawADC1(int ch) const { return (ch >= 0 && ch < kNumADC1) ? adc1_raw[ch] : 0; }

    private:
        // Expander INT line pins (polled, active low)
        gpio_num_t int1_pin = static_cast<gpio_num_t>(-1);
        gpio_num_t int2_pin = static_cast<gpio_num_t>(-1);

        // Periodic switch resync as insurance against broken INT wiring
        uint32_t last_switch_resync_ms = 0;

        // counters for which ADC channel to read next
        int adc0_channel = 0; // ADC channel for front panel reading
        int adc1_channel = 0; // ADC channel for front panel reading

        // Flags to track if first conversion has been initiated for pipelining
        bool adc0_first_conversion_started = false;
        bool adc1_first_conversion_started = false;

        // LIVE vs PATCH mode flag
        bool live_mode = true;

        // UI button event ring buffer and debounce state
        uint8_t btn_events[8] = {0};
        uint8_t btn_head = 0;
        uint8_t btn_tail = 0;
        uint8_t last_button_bits = 0x0F; // buttons idle high (pull-ups)
        uint32_t last_press_ms[4] = {0, 0, 0, 0};

        void pushButtonEvent(uint8_t button) {
            uint8_t next = (btn_head + 1) % sizeof(btn_events);
            if (next == btn_tail) return; // full, drop
            btn_events[btn_head] = button;
            btn_head = next;
        }

        // Detect presses (1 -> 0 transitions) on expander 2 GPB0-3
        void detectButtonPresses(uint16_t packedStates);

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

        // Latest raw (pre-filter) readings, for the diagnostic raw-ADC screen
        int16_t adc0_raw[16] = {0};
        int16_t adc1_raw[10] = {0};

        // Last filtered value pushed to a parameter; the deadband reference
        int16_t adc0_applied[16] = {0};
        int16_t adc1_applied[10] = {0};

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

        // Re-read the toggle switch states from both expanders (used when
        // returning to LIVE mode)
        void resyncSwitches();

        // Periodic check of IO Expanders to update states
        void checkIOExpanders();

        // Periodic check of ADCs to update states
        void checkADCs();

        // State machines for ADC reading (non-blocking)
        void ADC0StateMachine();
        void ADC1StateMachine();

        // Route a filtered reading to the matching parameter setter
        void applyADC0(int channel, int16_t value);
        void applyADC1(int channel, int16_t value);

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
