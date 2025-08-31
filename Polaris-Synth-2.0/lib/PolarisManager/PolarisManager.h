#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2CBus.h"
#include "IOExpander.h"
#include "ExternalADC.h"
#include "MidiUSB.h"


class PolarisManager {

public:

    PolarisManager() {
        init_hardware();
    }

private:

    I2CBus i2c_bus{I2C_NUM_0};
    IOExpander io_expander_0{i2c_bus, 0x20};
    IOExpander io_expander_1{i2c_bus, 0x21};
    ExternalADC adc_0{i2c_bus, 0x48};
    ExternalADC adc_1{i2c_bus, 0x49};
    //MidiUSB midi_usb;



    void init_hardware();

};