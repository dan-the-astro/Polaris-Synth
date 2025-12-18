#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

// Class for managing the OLED display of the synthesizer
#include <U8g2lib.h>

class DisplayManager {
public:
    DisplayManager() {
        oled = new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0);
        init_display();
    }
    
    ~DisplayManager() {
        delete oled;
    }
    
private:
    U8G2_SSD1309_128X64_NONAME0_F_HW_I2C* oled = nullptr;
    
    void init_display();
};

#endif