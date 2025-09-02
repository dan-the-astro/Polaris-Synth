#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2CBus.h"
#include "IOExpander.h"
#include "ExternalADC.h"
#include "MidiUSB.h"

#define ADC0_I2C_ADDR 0x49
#define ADC1_I2C_ADDR 0x48
#define IOEXPANDER0_I2C_ADDR 0x26   
#define IOEXPANDER1_I2C_ADDR 0x24


class PolarisManager {

public:

    PolarisManager() {
        init_hardware();
    }

private:

    I2CBus i2c_bus{I2C_NUM_0};
    IOExpander io_expander_0{i2c_bus, ADC0_I2C_ADDR};
    IOExpander io_expander_1{i2c_bus, ADC1_I2C_ADDR};
    ExternalADC adc_0{i2c_bus, IOEXPANDER0_I2C_ADDR};
    ExternalADC adc_1{i2c_bus, IOEXPANDER1_I2C_ADDR};
    //MidiUSB midi_usb;



    void init_hardware();

};