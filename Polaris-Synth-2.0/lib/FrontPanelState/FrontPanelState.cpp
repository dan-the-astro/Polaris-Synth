#include <FrontPanelState.h>
#include "PatchManager.h"

// 74HC393 counter control pins driving the two analog muxes
static constexpr gpio_num_t ADC0_CLK = static_cast<gpio_num_t>(32);
static constexpr gpio_num_t ADC0_RES = static_cast<gpio_num_t>(33);
static constexpr gpio_num_t ADC1_CLK = static_cast<gpio_num_t>(25);
static constexpr gpio_num_t ADC1_RES = static_cast<gpio_num_t>(26);

// A filtered knob reading must move more than this many ADC counts (out of the
// ~1650 full-scale range) before it is pushed to a parameter. This deadband
// keeps a stationary knob from dithering on residual ADC noise, the same role
// the hysteresis in the 1.x firmware played.
static constexpr int kAdcDeadband = 8;

// Initial scan of the front panel to set initial states
void FrontPanelState::initialScan() {
    resyncSwitches();

    // Seed the button states from the current pin levels
    if (io_expander_2) {
        last_button_bits = (io_expander_2->readGPIOPacked() >> 8) & 0x0F;
    }

    // Read initial ADC values to set continuous parameters. The ADS1015 needs
    // ~303us per conversion, so pace the priming loop accordingly.
    for (int i = 0; i < 16; i++) {
        checkADCs();
        delayMicroseconds(400);
    }
}

// Re-read the toggle switch states from both expanders
void FrontPanelState::resyncSwitches() {
    if (io_expander) {
        uint16_t states = io_expander->readGPIOPacked();
        LFO_saw_wave = states & (1 << 0);
        LFO_triangle_wave = states & (1 << 1);
        LFO_square_wave = states & (1 << 2);
        LFO_sine_wave = states & (1 << 3);
        OscA_saw_wave = states & (1 << 4);
        OscA_triangle_wave = states & (1 << 5);
        OscA_square_wave = states & (1 << 6);
        OscA_sine_wave = states & (1 << 7);
        OscB_saw_wave = states & (1 << 8);
        OscB_triangle_wave = states & (1 << 9);
        OscB_square_wave = states & (1 << 10);
        OscB_sine_wave = states & (1 << 11);
        OscB_lo_freq = states & (1 << 12);
        OscB_keyboard = states & (1 << 13);
        Wheel_freq_A = states & (1 << 14);
        Wheel_freq_B = states & (1 << 15);
    }
    if (io_expander_2) {
        uint16_t states = io_expander_2->readGPIOPacked();
        Wheel_pw_A = states & (1 << 0);
        Wheel_pw_B = states & (1 << 1);
        Wheel_filter_cutoff = states & (1 << 2);
        Poly_freq_A = states & (1 << 3);
        Poly_pw_A = states & (1 << 4);
        Poly_filter_cutoff = states & (1 << 5);
        Filter_keytrack = states & (1 << 6);
        unison = states & (1 << 7);
    }
}

void FrontPanelState::setLiveMode(bool live) {
    if (live && !live_mode) {
        // Knobs take over at their current physical positions on the next ADC
        // scans; switches are re-read immediately.
        live_mode = true;
        resyncSwitches();
    } else {
        live_mode = live;
    }
}

// Detect presses (1 -> 0 transitions) on expander 2 GPB0-3 with debouncing
void FrontPanelState::detectButtonPresses(uint16_t packedStates) {
    uint8_t buttons = (packedStates >> 8) & 0x0F;
    uint32_t now = millis();
    for (uint8_t i = 0; i < 4; i++) {
        bool wasHigh = last_button_bits & (1 << i);
        bool isLow = !(buttons & (1 << i));
        if (wasHigh && isLow && (now - last_press_ms[i]) > 120) {
            last_press_ms[i] = now;
            pushButtonEvent(i);
        }
    }
    last_button_bits = buttons;
}

void FrontPanelState::checkIOExpanders() {
    // The MCP23017 INT outputs are active low and stay latched until the
    // expander is read, so polling the line level here (1kHz) loses no
    // events. A periodic forced resync covers the case of a broken or
    // floating INT line: the panel then still tracks switches, just with up
    // to 250ms latency, instead of being dead (or storming an ISR).
    uint32_t now = millis();
    bool periodic = (now - last_switch_resync_ms) >= 250;
    if (periodic) {
        last_switch_resync_ms = now;
    }

    bool int1 = periodic ||
        (int1_pin != static_cast<gpio_num_t>(-1) && gpio_get_level(int1_pin) == 0);
    bool int2 = periodic ||
        (int2_pin != static_cast<gpio_num_t>(-1) && gpio_get_level(int2_pin) == 0);

    if (int1) {
        // Acknowledge the interrupt to clear it
        io_expander->acknowledgeInterrupt();
        if (live_mode) {
            // Read the current states of GPIOA and GPIOB
            uint16_t states = io_expander->readGPIOPacked();

            // Update internal state variables based on pin states
            LFO_saw_wave = states & (1 << 0);
            LFO_triangle_wave = states & (1 << 1);
            LFO_square_wave = states & (1 << 2);
            LFO_sine_wave = states & (1 << 3);
            OscA_saw_wave = states & (1 << 4);
            OscA_triangle_wave = states & (1 << 5);
            OscA_square_wave = states & (1 << 6);
            OscA_sine_wave = states & (1 << 7);
            OscB_saw_wave = states & (1 << 8);
            OscB_triangle_wave = states & (1 << 9);
            OscB_square_wave = states & (1 << 10);
            OscB_sine_wave = states & (1 << 11);
            OscB_lo_freq = states & (1 << 12);
            OscB_keyboard = states & (1 << 13);
            Wheel_freq_A = states & (1 << 14);
            Wheel_freq_B = states & (1 << 15);
        }
    }

    if (int2) {
        // The captured snapshot holds the pin states latched at the moment of
        // the interrupt (and clears it); use it so short button presses are
        // not missed between the interrupt and this poll.
        uint16_t captured = io_expander_2->readCapturedPacked();
        detectButtonPresses(captured);

        // Read the current states of GPIOA and GPIOB for the switches.
        // Run press detection on the live state too: INTCAP only latches the
        // first change, so a press right after a switch flip would otherwise
        // be missed (the debounce window stops double counting).
        uint16_t states = io_expander_2->readGPIOPacked();
        detectButtonPresses(states);
        if (live_mode) {
            // Update internal state variables based on pin states
            Wheel_pw_A = states & (1 << 0);
            Wheel_pw_B = states & (1 << 1);
            Wheel_filter_cutoff = states & (1 << 2);
            Poly_freq_A = states & (1 << 3);
            Poly_pw_A = states & (1 << 4);
            Poly_filter_cutoff = states & (1 << 5);
            Filter_keytrack = states & (1 << 6);
            unison = states & (1 << 7);
        }
    }
}

void FrontPanelState::checkADCs() {
    ADC0StateMachine();
    ADC1StateMachine();
}

void FrontPanelState::ADC0StateMachine() {

    // If this is the first iteration, start the conversion and return
    if (!adc0_first_conversion_started) {
        adc0_first_conversion_started = true;
        adc0->startConversion(0);
        return;
    }

    // Read the conversion started last call (the mux is parked on adc0_channel)
    int16_t raw = adc0->readConversionResult();
    adc0_raw[adc0_channel] = raw;
    int16_t filtered = applyBiquadFilter(adc0_biquad_state[adc0_channel], raw);

    // Only drive parameters in LIVE mode, and only once the knob has moved
    // past the deadband (kills idle dithering). PATCH mode keeps scanning so
    // the filters and the raw diagnostic view stay live.
    if (live_mode) {
        int delta = static_cast<int>(filtered) - static_cast<int>(adc0_applied[adc0_channel]);
        if (delta < 0) delta = -delta;
        if (delta > kAdcDeadband) {
            adc0_applied[adc0_channel] = filtered;
            applyADC0(adc0_channel, filtered);
        }
    }

    // Advance the analog mux. On wrap, RESET the 74HC393 instead of clocking
    // past the last channel: this ADC uses all 16 mux positions, but resetting
    // (rather than relying on the 4-bit counter's own rollover) guarantees the
    // hardware counter and adc0_channel can never drift out of sync.
    if (adc0_channel >= 15) {
        adc0_channel = 0;
        gpio_set_level(ADC0_RES, 1); // pulse master reset -> counter = 0
        gpio_set_level(ADC0_RES, 0);
    } else {
        adc0_channel++;
        gpio_set_level(ADC0_CLK, 1); // pulse clock -> next channel
        gpio_set_level(ADC0_CLK, 0);
    }

    // Start the conversion for the next iteration (pipelined approach)
    adc0->startConversion(0);
}

// Route a filtered ADC0 reading to its parameter setter
void FrontPanelState::applyADC0(int channel, int16_t value) {
    switch (channel) {
        case 0:  setLFOInitialAmount(value); break;
        case 1:  setLFOFrequency(value);     break;
        case 2:  setOscAFrequency(value);    break;
        case 3:  setOscAPulseWidth(value);   break;
        case 4:  setOscBFrequency(value);    break;
        case 5:  setOscBFine(value);         break;
        case 6:  setOscBPulseWidth(value);   break;
        case 7:  setWheelSourceMix(value);   break;
        case 8:  setPolyFiltEnvAmt(value);   break;
        case 9:  setPolyOscBAmount(value);   break;
        case 10: setMixerOscALevel(value);   break;
        case 11: setMixerOscBLevel(value);   break;
        case 12: setMixerNoiseLevel(value);  break;
        case 13: setFilterCutoff(value);     break;
        case 14: setFilterResonance(value);  break;
        case 15: setFilterEnvAmt(value);     break;
        default: break;
    }
}


void FrontPanelState::ADC1StateMachine() {

    int16_t reading = 0;

    // If this is the first iteration, start the conversion and return
    if (!adc1_first_conversion_started) {
        adc1_first_conversion_started = true;
        adc1->startConversion(0);
        return;
    }

    // Read the conversion started last call (the mux is parked on adc1_channel)
    int16_t raw = adc1->readConversionResult();
    adc1_raw[adc1_channel] = raw;
    int16_t filtered = applyBiquadFilter(adc1_biquad_state[adc1_channel], raw);

    if (live_mode) {
        int delta = static_cast<int>(filtered) - static_cast<int>(adc1_applied[adc1_channel]);
        if (delta < 0) delta = -delta;
        if (delta > kAdcDeadband) {
            adc1_applied[adc1_channel] = filtered;
            applyADC1(adc1_channel, filtered);
        }
    }

    // This ADC only uses 10 of the mux's positions, so on channel 9 RESET the
    // counter back to 0 rather than clocking it to position 10. This is the
    // fix for the channel/mux desync: clocking past channel 9 left the
    // free-running 4-bit counter at 10-15 while the software index was back at
    // 0-5, so every ADSR knob ended up reading a floating, unused mux input.
    if (adc1_channel >= 9) {
        adc1_channel = 0;
        gpio_set_level(ADC1_RES, 1); // pulse master reset -> counter = 0
        gpio_set_level(ADC1_RES, 0);
    } else {
        adc1_channel++;
        gpio_set_level(ADC1_CLK, 1); // pulse clock -> next channel
        gpio_set_level(ADC1_CLK, 0);
    }

    // Start the conversion for the next iteration (pipelined approach)
    adc1->startConversion(0);
}

// Route a filtered ADC1 reading to its parameter setter
void FrontPanelState::applyADC1(int channel, int16_t value) {
    switch (channel) {
        case 0: setFilterAttack(value);  break;
        case 1: setFilterDecay(value);   break;
        case 2: setFilterSustain(value); break;
        case 3: setFilterRelease(value); break;
        case 4: setAmpAttack(value);     break;
        case 5: setAmpDecay(value);      break;
        case 6: setAmpSustain(value);    break;
        case 7: setAmpRelease(value);    break;
        case 8: setMasterLevel(value);   break;
        case 9: setGlideRate(value);     break;
        default: break;
    }
}

// ---------- Patch capture / apply ----------

void FrontPanelState::capturePatch(PatchData& p) const {
    p.magic = PatchData::kMagic;
    p.version = PatchData::kVersion;

    p.lfoInitialAmount = LFO_initial_amount;
    p.lfoFrequency = LFO_frequency;
    p.oscAFrequency = OscA_frequency;
    p.oscAPulseWidth = OscA_pulse_width;
    p.oscBFrequency = OscB_frequency;
    p.oscBPulseWidth = OscB_pulse_width;
    p.oscBFine = OscB_fine;
    p.wheelSourceMix = Wheel_source_mix;
    p.polyFiltEnvAmt = Poly_filt_env_amt;
    p.polyOscBAmount = Poly_oscB_amount;
    p.mixerOscALevel = Mixer_oscA_level;
    p.mixerOscBLevel = Mixer_oscB_level;
    p.mixerNoiseLevel = Mixer_noise_level;
    p.filterCutoff = Filter_cutoff;
    p.filterResonance = Filter_resonance;
    p.filterEnvAmt = Filter_env_amt;
    p.filterAttack = Filter_attack;
    p.filterDecay = Filter_decay;
    p.filterSustain = Filter_sustain;
    p.filterRelease = Filter_release;
    p.ampAttack = Amp_attack;
    p.ampDecay = Amp_decay;
    p.ampSustain = Amp_sustain;
    p.ampRelease = Amp_release;
    p.masterLevel = Master_level;
    p.glideRate = Glide_rate;

    p.lfoSaw = LFO_saw_wave;
    p.lfoTri = LFO_triangle_wave;
    p.lfoSquare = LFO_square_wave;
    p.lfoSine = LFO_sine_wave;
    p.oscASaw = OscA_saw_wave;
    p.oscATri = OscA_triangle_wave;
    p.oscASquare = OscA_square_wave;
    p.oscASine = OscA_sine_wave;
    p.oscBSaw = OscB_saw_wave;
    p.oscBTri = OscB_triangle_wave;
    p.oscBSquare = OscB_square_wave;
    p.oscBSine = OscB_sine_wave;
    p.oscBLoFreq = OscB_lo_freq;
    p.oscBKeyboard = OscB_keyboard;
    p.wheelFreqA = Wheel_freq_A;
    p.wheelFreqB = Wheel_freq_B;
    p.wheelPwA = Wheel_pw_A;
    p.wheelPwB = Wheel_pw_B;
    p.wheelFilter = Wheel_filter_cutoff;
    p.polyFreqA = Poly_freq_A;
    p.polyPwA = Poly_pw_A;
    p.polyFilter = Poly_filter_cutoff;
    p.filterKeytrack = Filter_keytrack;
    p.unisonOn = unison;
}

void FrontPanelState::applyPatch(const PatchData& p) {
    // Detach the physical panel first so a concurrent ADC scan can't fight
    // the values we are about to load
    live_mode = false;

    LFO_initial_amount = p.lfoInitialAmount;
    LFO_frequency = p.lfoFrequency;
    OscA_frequency = p.oscAFrequency;
    OscA_pulse_width = p.oscAPulseWidth;
    OscB_frequency = p.oscBFrequency;
    OscB_pulse_width = p.oscBPulseWidth;
    OscB_fine = p.oscBFine;
    Wheel_source_mix = p.wheelSourceMix;
    Poly_filt_env_amt = p.polyFiltEnvAmt;
    Poly_oscB_amount = p.polyOscBAmount;
    Mixer_oscA_level = p.mixerOscALevel;
    Mixer_oscB_level = p.mixerOscBLevel;
    Mixer_noise_level = p.mixerNoiseLevel;
    Filter_cutoff = p.filterCutoff;
    Filter_resonance = p.filterResonance;
    Filter_env_amt = p.filterEnvAmt;
    Filter_attack = p.filterAttack;
    Filter_decay = p.filterDecay;
    Filter_sustain = p.filterSustain;
    Filter_release = p.filterRelease;
    Amp_attack = p.ampAttack;
    Amp_decay = p.ampDecay;
    Amp_sustain = p.ampSustain;
    Amp_release = p.ampRelease;
    // Master_level is intentionally NOT applied from patches: the output
    // volume always follows the physical knob to avoid surprise jumps.
    Glide_rate = p.glideRate;

    LFO_saw_wave = p.lfoSaw;
    LFO_triangle_wave = p.lfoTri;
    LFO_square_wave = p.lfoSquare;
    LFO_sine_wave = p.lfoSine;
    OscA_saw_wave = p.oscASaw;
    OscA_triangle_wave = p.oscATri;
    OscA_square_wave = p.oscASquare;
    OscA_sine_wave = p.oscASine;
    OscB_saw_wave = p.oscBSaw;
    OscB_triangle_wave = p.oscBTri;
    OscB_square_wave = p.oscBSquare;
    OscB_sine_wave = p.oscBSine;
    OscB_lo_freq = p.oscBLoFreq;
    OscB_keyboard = p.oscBKeyboard;
    Wheel_freq_A = p.wheelFreqA;
    Wheel_freq_B = p.wheelFreqB;
    Wheel_pw_A = p.wheelPwA;
    Wheel_pw_B = p.wheelPwB;
    Wheel_filter_cutoff = p.wheelFilter;
    Poly_freq_A = p.polyFreqA;
    Poly_pw_A = p.polyPwA;
    Poly_filter_cutoff = p.polyFilter;
    Filter_keytrack = p.filterKeytrack;
    unison = p.unisonOn;
}

// Functions to map raw ADC readings to internal state variables (0-1650 = 0-3.3V)


void FrontPanelState::setLFOInitialAmount(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    LFO_initial_amount = (static_cast<int32_t>(value) << 16) / 1650;
}

void FrontPanelState::setLFOFrequency(int16_t value) {
    // Exponential mapping: freq = min_freq * pow(max_freq/min_freq, value/1650.0)
    // min_freq = 0.1Hz, max_freq = 40Hz
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    constexpr float min_freq = 0.1f;
    constexpr float max_freq = 40.0f;
    float norm = static_cast<float>(value) / 1650.0f;
    float freq = min_freq * powf(max_freq / min_freq, norm);
    LFO_frequency = static_cast<int32_t>(freq * 65536.0f); // Q16.16 fixed point
}

void FrontPanelState::setOscAFrequency(int16_t value) {
    // Map 0-1650 to -24..+24 semitones in whole semitone steps (P5 style)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscA_frequency = (static_cast<int32_t>(value) * 48) / 1650 - 24;
}

void FrontPanelState::setOscAPulseWidth(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscA_pulse_width = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setOscBFrequency(int16_t value) {
    // Map 0-1650 to MIDI note index 0..127. The synth engine rescales this to
    // a -12..+12 semitone offset when Osc B keyboard tracking is on.
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscB_frequency = (static_cast<int32_t>(value) * 127) / 1650;
 }

void FrontPanelState::setOscBPulseWidth(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscB_pulse_width = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setOscBFine(int16_t value) {
    // Map 0-1650 to 0..128 pitch index units (0..+1 semitone)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscB_fine = (static_cast<int32_t>(value) * 128) / 1650;
 }

void FrontPanelState::setWheelSourceMix(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Wheel_source_mix = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setPolyFiltEnvAmt(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Poly_filt_env_amt = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setPolyOscBAmount(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Poly_oscB_amount = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setMixerOscALevel(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Mixer_oscA_level = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setMixerOscBLevel(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Mixer_oscB_level = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setMixerNoiseLevel(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Mixer_noise_level = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setFilterCutoff(int16_t value) {
    // Map 0-1650 linearly to pitch index units 0..16255. The pitch index
    // domain is exponential in frequency (128 units per semitone), so this
    // gives the cutoff knob a musical 8.18Hz..12.5kHz sweep.
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Filter_cutoff = (static_cast<int32_t>(value) * 16255) / 1650;
 }

void FrontPanelState::setFilterResonance(int16_t value) {
    // Map 0-1650 to 0.5 - 10.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float reso = 0.5f + (static_cast<float>(value) / 1650.0f) * (10.0f - 0.5f);
    Filter_resonance = static_cast<int32_t>(reso * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setFilterEnvAmt(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Filter_env_amt = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setFilterAttack(int16_t value) {
    // Map 0-1650 to 0.0 - 10.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float attack = (static_cast<float>(value) / 1650.0f) * 10.0f;
    Filter_attack = static_cast<int32_t>(attack * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setFilterDecay(int16_t value) {
    // Map 0-1650 to 0.0 - 10.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float decay = (static_cast<float>(value) / 1650.0f) * 10.0f;
    Filter_decay = static_cast<int32_t>(decay * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setFilterSustain(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float sustain = static_cast<float>(value) / 1650.0f;
    Filter_sustain = static_cast<int32_t>(sustain * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setFilterRelease(int16_t value) {
    // Map 0-1650 to 0.0 - 10.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float release = (static_cast<float>(value) / 1650.0f) * 10.0f;
    Filter_release = static_cast<int32_t>(release * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setAmpAttack(int16_t value) {
    // Map 0-1650 to 0.0 - 10.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float attack = (static_cast<float>(value) / 1650.0f) * 10.0f;
    Amp_attack = static_cast<int32_t>(attack * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setAmpDecay(int16_t value) {
    // Map 0-1650 to 0.0 - 10.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float decay = (static_cast<float>(value) / 1650.0f) * 10.0f;
    Amp_decay = static_cast<int32_t>(decay * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setAmpSustain(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float sustain = static_cast<float>(value) / 1650.0f;
    Amp_sustain = static_cast<int32_t>(sustain * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setAmpRelease(int16_t value) {
    // Map 0-1650 to 0.0 - 10.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float release = (static_cast<float>(value) / 1650.0f) * 10.0f;
    Amp_release = static_cast<int32_t>(release * 65536.0f); // Q16.16 fixed point
 }

void FrontPanelState::setMasterLevel(int16_t value) {
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    Master_level = (static_cast<int32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setGlideRate(int16_t value) {
    // Map 0-1650 to 0.0 - 5.0 seconds (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    float glide = (static_cast<float>(value) / 1650.0f) * 5.0f;
    Glide_rate = static_cast<int32_t>(glide * 65536.0f); // Q16.16 fixed point
 }
