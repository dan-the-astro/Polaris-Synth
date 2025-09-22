#include <FrontPanelState.h>

// Initial scan of the front panel to set initial states
void FrontPanelState::initialScan() {
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
        // Process the states as needed (placeholder)
        // For example, update internal state variables based on pin states
    }

    if (int2) {
        // Acknowledge the interrupt to clear it
        io_expander_2->acknowledgeInterrupt();
        // Read the current states of GPIOA and GPIOB
        uint16_t states = io_expander_2->readGPIOPacked();
        // Process the states as needed (placeholder)
        // For example, update internal state variables based on pin states
    }
}

// Empty setter implementations (TODO: apply proper transformations / scaling)
void FrontPanelState::setLFOInitialAmount(int value) { (void)value; }
void FrontPanelState::setLFOFrequency(int value) { (void)value; }
void FrontPanelState::setOscAFrequency(int value) { (void)value; }
void FrontPanelState::setOscAPulseWidth(int value) { (void)value; }
void FrontPanelState::setOscBFrequency(int value) { (void)value; }
void FrontPanelState::setOscBPulseWidth(int value) { (void)value; }
void FrontPanelState::setOscBFine(int value) { (void)value; }
void FrontPanelState::setWheelSourceMix(int value) { (void)value; }
void FrontPanelState::setPolyFiltEnvAmt(int value) { (void)value; }
void FrontPanelState::setPolyOscBAmount(int value) { (void)value; }
void FrontPanelState::setMixerOscALevel(int value) { (void)value; }
void FrontPanelState::setMixerOscBLevel(int value) { (void)value; }
void FrontPanelState::setMixerNoiseLevel(int value) { (void)value; }
void FrontPanelState::setFilterCutoff(int value) { (void)value; }
void FrontPanelState::setFilterResonance(int value) { (void)value; }
void FrontPanelState::setFilterEnvAmt(int value) { (void)value; }
void FrontPanelState::setFilterAttack(int value) { (void)value; }
void FrontPanelState::setFilterDecay(int value) { (void)value; }
void FrontPanelState::setFilterSustain(int value) { (void)value; }
void FrontPanelState::setFilterRelease(int value) { (void)value; }
void FrontPanelState::setAmpAttack(int value) { (void)value; }
void FrontPanelState::setAmpDecay(int value) { (void)value; }
void FrontPanelState::setAmpSustain(int value) { (void)value; }
void FrontPanelState::setAmpRelease(int value) { (void)value; }
void FrontPanelState::setMasterLevel(int value) { (void)value; }
void FrontPanelState::setGlideRate(int value) { (void)value; }

