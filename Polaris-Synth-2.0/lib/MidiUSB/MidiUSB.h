// MidiUSB.h - interface for USB MIDI host using MAX3421E on ESP32
#pragma once

#include <Arduino.h>
#include <USB.h>
#include <usbh_midi.h>

#include "MidiHandler.h"

/**
 * MidiUSB sets up the MAX3421E USB host controller and forwards any MIDI
 * messages from attached USB MIDI devices to a MidiHandler instance. The class
 * itself performs no interpretation of the MIDI data.
 */
class MidiUSB {
public:
    /**
     * Create a new MidiUSB interface.
     * @param interruptPin GPIO pin used for the MAX3421E INT line.
     * @param parser       MidiHandler instance that will parse received data.
     */
    explicit MidiUSB(int interruptPin, MidiHandler &parser);

    /**
     * Initialise the MAX3421E and USB host stack.
     * @return true on success, false otherwise.
     */
    bool begin();

    /**
     * Process pending USB transactions and pass any MIDI messages to the
     * parser. Should be called frequently from the main loop.
     */
    void task();

private:
    static void IRAM_ATTR usbInterrupt();

    static MidiUSB *s_instance;

    int _intPin;
    USB _usb;
    USBH_MIDI _midi;
    MidiHandler &_parser;
    volatile bool _irqFlag;
};

