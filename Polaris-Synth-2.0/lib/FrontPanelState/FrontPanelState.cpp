#include <FrontPanelState.h>

// Initial scan of the front panel to set initial states
void FrontPanelState::initialScan() {
    // Read initial states from both IO expanders
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
    // Read initial ADC values to set continuous parameters
    for (int i = 0; i < 16; i++) {
        checkADCs();
    }
}

void FrontPanelState::checkIOExpanders() {
    // Check if either IO Expander has an interrupt pending
    bool int1 = io_expander->consumeInterruptFlag();
    bool int2 = io_expander_2->consumeInterruptFlag();

    if (int1) {
        // Acknowledge the interrupt to clear it
        io_expander->acknowledgeInterrupt();
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

    if (int2) {
        // Acknowledge the interrupt to clear it
        io_expander_2->acknowledgeInterrupt();
        // Read the current states of GPIOA and GPIOB
        uint16_t states = io_expander_2->readGPIOPacked();
        // Update internal state variables based on pin states
        Wheel_pw_A = states & (1 << 0);
        Wheel_pw_B = states & (1 << 1);
        Wheel_filter_cutoff = states & (1 << 2);
        Poly_freq_A = states & (1 << 3);
        Poly_pw_A = states & (1 << 4);
        Poly_filter_cutoff = states & (1 << 5);
        Filter_keytrack = states & (1 << 6);
        unison = states & (1 << 7);

        // TODO: Add event triggers for OLED action buttons
    }
}

void FrontPanelState::checkADCs() {
    ADC0StateMachine();
    ADC1StateMachine();  // Comment out to test
}

void FrontPanelState::ADC0StateMachine() {

    int16_t reading = adc0->readChannel(0); // Read channel 0

    //Serial.printf("ADC0 Channel %d Reading: %d\n", adc0_channel, reading);

    switch (adc0_channel) {
        case 0:
            setLFOInitialAmount(reading);
            //Serial.println(reading);
            adc0_channel = 1;
            break;
        case 1:
            setLFOFrequency(reading);
            adc0_channel = 2;
            break;
        case 2:
            setOscAFrequency(reading);
            adc0_channel = 3;
            break;
        case 3:
            setOscAPulseWidth(reading);
            adc0_channel = 4;
            break;
        case 4:
            setOscBFrequency(reading);
            adc0_channel = 5;
            break;
        case 5:
            setOscBFine(reading);
            adc0_channel = 6;
            break;
        case 6:
            setOscBPulseWidth(reading);
            adc0_channel = 7;
            break;
        case 7:
            setWheelSourceMix(reading);
            adc0_channel = 8;
            break;
        case 8:
            setPolyFiltEnvAmt(reading);
            adc0_channel = 9;
            break;
        case 9:
            setPolyOscBAmount(reading);
            adc0_channel = 10;
            break;
        case 10:
            setMixerOscALevel(reading);
            adc0_channel = 11;
            break;
        case 11:
            setMixerOscBLevel(reading);
            adc0_channel = 12;
            break;
        case 12:
            setMixerNoiseLevel(reading);
            adc0_channel = 13;
            break;
        case 13:
            setFilterCutoff(reading);
            adc0_channel = 14;
            break;
        case 14:
            setFilterResonance(reading);
            adc0_channel = 15;
            break;
        case 15:
            setFilterEnvAmt(reading);
            adc0_channel = 0;
            break;
    }

    // Advance 74HC393 counter to select next ADC channel
    gpio_set_level(static_cast<gpio_num_t>(32), 1); // Set CLK high
    gpio_set_level(static_cast<gpio_num_t>(32), 0); // Set CLK low
    
}


void FrontPanelState::ADC1StateMachine() {

    int16_t reading = adc1->readChannel(0); // Read channel 0

    switch (adc1_channel) {
        case 0:
            setFilterAttack(reading);
            adc1_channel = 1;
            break;
        case 1:
            setFilterDecay(reading);
            adc1_channel = 2;
            break;
        case 2:
            setFilterSustain(reading);
            adc1_channel = 3;
            break;
        case 3:
            setFilterRelease(reading);
            adc1_channel = 4;
            break;
        case 4:
            setAmpAttack(reading);
            adc1_channel = 5;
            break;
        case 5:
            setAmpDecay(reading);
            adc1_channel = 6;
            break;
        case 6:
            setAmpSustain(reading);
            adc1_channel = 7;
            break;
        case 7:
            setAmpRelease(reading);
            adc1_channel = 8;
            break;
        case 8:
            setMasterLevel(reading);
            adc1_channel = 9;
            break;
        case 9:
            setGlideRate(reading);
            adc1_channel = 0;
            break;
    }

    // Advance 74HC393 counter to select next ADC channel
    gpio_set_level(static_cast<gpio_num_t>(25), 1); // Set CLK high
    gpio_set_level(static_cast<gpio_num_t>(25), 0); // Set CLK low

}

// Functions to map raw ADC readings to internal state variables (0-1650 = 0-3.3V)


void FrontPanelState::setLFOInitialAmount(int16_t value) { 
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    LFO_initial_amount = (static_cast<uint32_t>(value) << 16) / 1650;
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
    LFO_frequency = static_cast<uint32_t>(freq * 65536.0f); // Q16.16 fixed point
}

void FrontPanelState::setOscAFrequency(int16_t value) { 
    // Map 0-1650 to -7 to +7 octaves (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    int32_t freq = ((static_cast<int32_t>(value) - 825) * 1000000) / 825;
    OscA_frequency = freq << 16;
}

void FrontPanelState::setOscAPulseWidth(int16_t value) { 
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscA_pulse_width = (static_cast<uint32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setOscBFrequency(int16_t value) { 
    // Map 0-1650 to -7 to +7 octaves (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    int32_t freq = ((static_cast<int32_t>(value) - 825) * 1000000) / 825;
    OscB_frequency = freq << 16;
 }

void FrontPanelState::setOscBPulseWidth(int16_t value) { 
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscB_pulse_width = (static_cast<uint32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setOscBFine(int16_t value) { 
    // Map 0-1650 to 0.0 - 1.0 (Q16.16 fixed point)
    if (value < 0) value = 0;
    if (value > 1650) value = 1650;
    OscB_fine = (static_cast<uint32_t>(value) << 16) / 1650;
 }

void FrontPanelState::setWheelSourceMix(int16_t value) { (void)value; }

void FrontPanelState::setPolyFiltEnvAmt(int16_t value) { (void)value; }

void FrontPanelState::setPolyOscBAmount(int16_t value) { (void)value; }

void FrontPanelState::setMixerOscALevel(int16_t value) { (void)value; }

void FrontPanelState::setMixerOscBLevel(int16_t value) { (void)value; }

void FrontPanelState::setMixerNoiseLevel(int16_t value) { (void)value; }

void FrontPanelState::setFilterCutoff(int16_t value) { (void)value; }

void FrontPanelState::setFilterResonance(int16_t value) { (void)value; }

void FrontPanelState::setFilterEnvAmt(int16_t value) { (void)value; }

void FrontPanelState::setFilterAttack(int16_t value) { (void)value; }

void FrontPanelState::setFilterDecay(int16_t value) { (void)value; }

void FrontPanelState::setFilterSustain(int16_t value) { (void)value; }

void FrontPanelState::setFilterRelease(int16_t value) { (void)value; }

void FrontPanelState::setAmpAttack(int16_t value) { (void)value; }

void FrontPanelState::setAmpDecay(int16_t value) { (void)value; }

void FrontPanelState::setAmpSustain(int16_t value) { (void)value; }

void FrontPanelState::setAmpRelease(int16_t value) { (void)value; }

void FrontPanelState::setMasterLevel(int16_t value) { (void)value; }

void FrontPanelState::setGlideRate(int16_t value) { (void)value; }

