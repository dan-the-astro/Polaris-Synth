#include "DisplayManager.h"
#include "PolarisLogoBitmap.h"
#include <U8g2lib.h>


void DisplayManager::init_display() {
    oled->setBusClock(400000); // Set I2C clock to 400kHz
    oled->begin();
    oled->setFont(u8g2_font_micro_mr);
    oled->setCursor(1, 62);
    oled->print(FIRMWARE_VERSION);
    oled->home();
    oled->drawXBM(0, 19, 128, 25, Polaris_Logo_bitmap_bits);
    oled->sendBuffer();
}