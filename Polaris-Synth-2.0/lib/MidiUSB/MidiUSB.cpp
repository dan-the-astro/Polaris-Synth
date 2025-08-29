#include "MidiUSB.h"

#include <SPI.h>

MidiUSB *MidiUSB::s_instance = nullptr;

MidiUSB::MidiUSB(int interruptPin, MidiHandler &parser)
    : _intPin(interruptPin),
      _usb(),
      _midi(&_usb),
      _parser(parser),
      _irqFlag(false) {
    s_instance = this;
}

bool MidiUSB::begin() {
    // Initialise SPI with default pins (SCK=18, MISO=19, MOSI=23, SS=5)
    SPI.begin();

    pinMode(_intPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_intPin), MidiUSB::usbInterrupt, FALLING);

    if (_usb.Init() == -1) {
        return false; // initialisation failed
    }

    delay(200); // give devices time to enumerate
    return true;
}

void MidiUSB::task() {
    _usb.Task();

    if (!_irqFlag) {
        return; // nothing to process
    }

    _irqFlag = false;

    uint8_t buf[3];
    uint8_t size;
    while ((size = _midi.RecvData(buf)) > 0) {
        _parser.parse(buf, size);
    }
}

void IRAM_ATTR MidiUSB::usbInterrupt() {
    if (s_instance) {
        s_instance->_usb.ISR();
        s_instance->_irqFlag = true;
    }
}

