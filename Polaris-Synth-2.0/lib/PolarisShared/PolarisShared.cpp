#include "PolarisShared.h"

namespace PolarisShared {

    QueueHandle_t midiEventQueue = nullptr;

    FrontPanelState* volatile frontPanel = nullptr;

    volatile int32_t pitchBendUnits = 0;

    volatile int32_t modWheelQ16 = 0;

    volatile bool allNotesOff = false;

    void init() {
        if (!midiEventQueue) {
            midiEventQueue = xQueueCreate(64, sizeof(MidiEvent));
        }
    }
}
