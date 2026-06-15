#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

// Class for managing the OLED display of the synthesizer.
//
// Screen flow (button 0 cycles, per the menu bar label):
//   INFO -> FILTER BODE -> AMP ADSR -> FILTER ADSR -> INFO
// Button 1 toggles LIVE/PATCH mode, button 2 saves the current patch,
// button 3 opens the patch file selector (UP/DOWN/SELECT/EXIT).
//
// update() must be called regularly from the PolarisManager task; it
// consumes UI button events from the FrontPanelState and periodically
// redraws the active screen.

#include <U8g2lib.h>
#include <cstdint>
#include "PatchManager.h"

class FrontPanelState;

class DisplayManager {
public:
    DisplayManager() {
        oled = new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0);
        init_display();
    }

    ~DisplayManager() {
        delete oled;
    }

    // Hook up the front panel (button source, parameter values) and the
    // patch manager. Until this is called update() does nothing and the
    // splash screen stays up.
    void attach(FrontPanelState* fp, PatchManager* pm);

    // Service buttons and redraw; call from the manager loop
    void update();

private:
    enum class Screen : uint8_t { Info, FilterBode, AmpAdsr, FilterAdsr, RawAdc, FileSelect };

    U8G2_SSD1309_128X64_NONAME0_F_HW_I2C* oled = nullptr;
    FrontPanelState* panel = nullptr;
    PatchManager* patches = nullptr;

    Screen screen = Screen::Info;
    uint32_t nextDraw = 0;

    // Brief status overlay ("SAVED", "NO SD", ...)
    char statusMsg[16] = {0};
    uint32_t statusUntil = 0;

    // File selector state
    char fileNames[PatchManager::kMaxPatches][PatchManager::kNameLen];
    int fileCount = 0;
    int fileCursor = 0;
    int fileScroll = 0;

    // Name of the most recently loaded/saved patch file ("" = none)
    char loadedPatch[PatchManager::kNameLen] = {0};

    void init_display();

    void handleButton(uint8_t btn);
    void draw();
    void drawMenuBar(const char* l0, const char* l1, const char* l2, const char* l3,
                     bool invertSlot1);
    void drawInfoScreen();
    void drawAdsrScreen(bool filterEnv);
    void drawBodeScreen();
    void drawRawAdc();
    void drawFileSelect();
    void drawStatusOverlay();

    void enterFileSelect();
    void doSave();
    void doLoad();
    void toggleLive();
    void showStatus(const char* msg);
};

#endif
