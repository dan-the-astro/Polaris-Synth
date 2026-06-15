#include "DisplayManager.h"
#include "PolarisLogoBitmap.h"
#include "FrontPanelState.h"
#include "FixedPoint.h"
#include "SynthVoice.h"   // phaseIncFromIndex for the Bode plot
#include "FilterMath.h"
#include <U8g2lib.h>
#include <cstring>
#include <cstdio>
#include <cmath>


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

void DisplayManager::attach(FrontPanelState* fp, PatchManager* pm) {
    panel = fp;
    patches = pm;
    // Leave the splash logo up for a moment before the first screen draw

    // Not needed anymore since the manager loop calls update() immediately after attach(), but if we wanted to show the splash for a fixed time after attach() we could set nextDraw here:
    // nextDraw = millis() + 1500;
}

void DisplayManager::update() {
    if (!panel) return;

    // Handle queued button presses immediately
    uint8_t btn;
    bool pressed = false;
    while (panel->getButtonEvent(btn)) {
        handleButton(btn);
        pressed = true;
    }

    // Periodic redraw so knob movements show up while browsing
    if (pressed || (int32_t)(millis() - nextDraw) >= 0) {
        draw();
        nextDraw = millis() + 150;
    }
}

void DisplayManager::handleButton(uint8_t btn) {
    if (screen == Screen::FileSelect) {
        switch (btn) {
            case 0: // UP
                if (fileCursor > 0) fileCursor--;
                break;
            case 1: // DOWN
                if (fileCursor < fileCount - 1) fileCursor++;
                break;
            case 2: // SELECT
                doLoad();
                screen = Screen::Info;
                break;
            case 3: // EXIT
                screen = Screen::Info;
                break;
        }
        return;
    }

    switch (btn) {
        case 0: // cycle INFO -> BODE -> AMP ADSR -> FIL ADSR -> RAW ADC -> INFO
            switch (screen) {
                case Screen::Info:       screen = Screen::FilterBode; break;
                case Screen::FilterBode: screen = Screen::AmpAdsr;    break;
                case Screen::AmpAdsr:    screen = Screen::FilterAdsr; break;
                case Screen::FilterAdsr: screen = Screen::RawAdc;     break;
                default:                 screen = Screen::Info;       break;
            }
            break;
        case 1:
            toggleLive();
            break;
        case 2:
            doSave();
            break;
        case 3:
            enterFileSelect();
            break;
    }
}

void DisplayManager::toggleLive() {
    panel->setLiveMode(!panel->isLive());
    showStatus(panel->isLive() ? "LIVE" : "PATCH");
}

void DisplayManager::enterFileSelect() {
    // Retry the SD card in case it was inserted after boot
    if (!patches->ready() && !patches->begin()) {
        showStatus("NO SD");
        return;
    }
    fileCount = patches->listPatches(fileNames, PatchManager::kMaxPatches);
    fileCursor = 0;
    fileScroll = 0;
    screen = Screen::FileSelect;
}

void DisplayManager::doSave() {
    if (!patches->ready() && !patches->begin()) {
        showStatus("NO SD");
        return;
    }
    char name[PatchManager::kNameLen];
    if (loadedPatch[0]) {
        // Overwrite the patch that is currently loaded
        strcpy(name, loadedPatch);
    } else {
        patches->makeNextPatchName(name, sizeof(name));
    }

    PatchData d;
    panel->capturePatch(d);
    if (patches->save(name, d)) {
        strcpy(loadedPatch, name);
        showStatus("SAVED");
    } else {
        showStatus("SAVE ERR");
    }
}

void DisplayManager::doLoad() {
    if (fileCount == 0) {
        showStatus("NO FILES");
        return;
    }
    PatchData d;
    if (patches->load(fileNames[fileCursor], d)) {
        panel->applyPatch(d); // drops the panel into PATCH mode
        strcpy(loadedPatch, fileNames[fileCursor]);
        showStatus("LOADED");
    } else {
        showStatus("LOAD ERR");
    }
}

void DisplayManager::showStatus(const char* msg) {
    strncpy(statusMsg, msg, sizeof(statusMsg) - 1);
    statusMsg[sizeof(statusMsg) - 1] = '\0';
    statusUntil = millis() + 1200;
}

// ---------------------------------------------------------------- drawing

void DisplayManager::draw() {
    oled->clearBuffer();
    oled->setFont(u8g2_font_micro_mr);
    oled->setDrawColor(1);

    bool invertLive = !panel->isLive(); // highlight when knobs are detached

    switch (screen) {
        case Screen::Info:
            drawInfoScreen();
            drawMenuBar("BODE", "LIVE", "SAVE", "LOAD", invertLive);
            break;
        case Screen::FilterBode:
            drawBodeScreen();
            drawMenuBar("AMP", "LIVE", "SAVE", "LOAD", invertLive);
            break;
        case Screen::AmpAdsr:
            drawAdsrScreen(false);
            drawMenuBar("FIL", "LIVE", "SAVE", "LOAD", invertLive);
            break;
        case Screen::FilterAdsr:
            drawAdsrScreen(true);
            drawMenuBar("RAW", "LIVE", "SAVE", "LOAD", invertLive);
            break;
        case Screen::RawAdc:
            drawRawAdc();
            drawMenuBar("INFO", "LIVE", "SAVE", "LOAD", invertLive);
            break;
        case Screen::FileSelect:
            drawFileSelect();
            drawMenuBar("UP", "DOWN", "SELECT", "EXIT", false);
            break;
    }

    drawStatusOverlay();
    oled->sendBuffer();
}

// Menu bar at the bottom of the screen common to all OLED screens:
// 4 almost equally sized boxes with their action labels
void DisplayManager::drawMenuBar(const char* l0, const char* l1, const char* l2, const char* l3,
                                 bool invertSlot1) {
    oled->drawFrame(1, 56, 31, 9);
    oled->drawFrame(32, 56, 32, 9);
    oled->drawFrame(64, 56, 32, 9);
    oled->drawFrame(96, 56, 31, 9);

    oled->setCursor(3, 63);
    oled->print(l0);
    if (invertSlot1) {
        // Filled box + inverted text marks PATCH mode on the LIVE button
        oled->drawBox(32, 56, 32, 9);
        oled->setDrawColor(0);
        oled->setCursor(34, 63);
        oled->print(l1);
        oled->setDrawColor(1);
    } else {
        oled->setCursor(34, 63);
        oled->print(l1);
    }
    oled->setCursor(66, 63);
    oled->print(l2);
    oled->setCursor(98, 63);
    oled->print(l3);
}

// Parameter overview: envelope/LFO values on the left, mixer bars on the right
void DisplayManager::drawInfoScreen() {
    char buf[26];

    snprintf(buf, sizeof(buf), "AMP ATK:%.2f", FixedPoint::toFloat(panel->Amp_attack));
    oled->setCursor(1, 5);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "AMP DEC:%.2f", FixedPoint::toFloat(panel->Amp_decay));
    oled->setCursor(1, 11);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "AMP SUS:%.2f", FixedPoint::toFloat(panel->Amp_sustain));
    oled->setCursor(1, 17);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "AMP REL:%.2f", FixedPoint::toFloat(panel->Amp_release));
    oled->setCursor(1, 23);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "FIL ATK:%.2f", FixedPoint::toFloat(panel->Filter_attack));
    oled->setCursor(1, 29);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "FIL DEC:%.2f", FixedPoint::toFloat(panel->Filter_decay));
    oled->setCursor(1, 35);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "FIL SUS:%.2f", FixedPoint::toFloat(panel->Filter_sustain));
    oled->setCursor(1, 41);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "FIL REL:%.2f", FixedPoint::toFloat(panel->Filter_release));
    oled->setCursor(1, 47);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "LFO FREQ:%.1fHZ", FixedPoint::toFloat(panel->LFO_frequency));
    oled->setCursor(1, 53);
    oled->print(buf);

    // Mixer level bars (Osc A / Osc B / Noise)
    oled->setCursor(44, 5);
    oled->print("MIX");

    static const int barX[3] = { 60, 84, 108 };
    static const char* barLabel[3] = { "A", "B", "N" };
    const int32_t levels[3] = {
        panel->Mixer_oscA_level, panel->Mixer_oscB_level, panel->Mixer_noise_level
    };

    for (int i = 0; i < 3; i++) {
        int x = barX[i];
        oled->setCursor(x + 4, 5);
        oled->print(barLabel[i]);

        oled->drawFrame(x, 8, 12, 46);
        // Tick marks at the quarter points
        for (int t = 0; t < 5; t++) {
            oled->drawPixel(x - 2, 9 + t * 11);
        }
        // Fill from the bottom proportional to the Q16.16 level
        int h = (int)(((int64_t)levels[i] * 44) >> 16);
        if (h > 44) h = 44;
        if (h > 0) {
            oled->drawBox(x + 1, 9 + (44 - h), 10, h);
        }
    }
}

// ADSR plot: four equal time regions (attack/decay/sustain/release) with the
// knob values printed in each region
void DisplayManager::drawAdsrScreen(bool filterEnv) {
    float atk = FixedPoint::toFloat(filterEnv ? panel->Filter_attack : panel->Amp_attack);
    float dec = FixedPoint::toFloat(filterEnv ? panel->Filter_decay : panel->Amp_decay);
    float sus = FixedPoint::toFloat(filterEnv ? panel->Filter_sustain : panel->Amp_sustain);
    float rel = FixedPoint::toFloat(filterEnv ? panel->Filter_release : panel->Amp_release);

    // Axes
    oled->drawVLine(2, 0, 55);
    oled->drawHLine(2, 54, 125);

    // Title
    oled->setCursor(112, 5);
    oled->print(filterEnv ? "FIL" : "AMP");

    int susY = 2 + (int)((1.0f - sus) * 52.0f);
    if (susY < 2) susY = 2;
    if (susY > 54) susY = 54;

    // Envelope shape
    oled->drawLine(2, 54, 33, 2);        // attack
    oled->drawLine(33, 2, 64, susY);     // decay
    oled->drawLine(64, susY, 95, susY);  // sustain
    oled->drawLine(95, susY, 126, 54);   // release

    // Dotted separators between the regions
    for (int y = 0; y < 55; y += 3) {
        oled->drawPixel(33, y);
        oled->drawPixel(64, y);
        oled->drawPixel(95, y);
    }

    // Region values (times in seconds, sustain as a level)
    char buf[8];
    snprintf(buf, sizeof(buf), "%.2f", atk);
    oled->setCursor(8, 33);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "%.2f", dec);
    oled->setCursor(40, 33);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "%.2f", sus);
    oled->setCursor(71, 33);
    oled->print(buf);
    snprintf(buf, sizeof(buf), "%.2f", rel);
    oled->setCursor(101, 33);
    oled->print(buf);
}

// Bode magnitude plot of the current filter settings: 20Hz..20kHz log sweep,
// +24dB .. -60dB on screen, two cascaded biquad stages
void DisplayManager::drawBodeScreen() {
    uint32_t F = SynthVoice::phaseIncFromIndex(panel->Filter_cutoff);
    float q = (float)panel->Filter_resonance * (1.0f / 65536.0f);
    if (q < 0.5f) q = 0.5f;
    if (q > 10.0f) q = 10.0f;

    float c[5];
    polarisLowpassCoeffsFloat(F, q, c);
    const float b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];

    oled->drawVLine(0, 0, 55);
    oled->drawHLine(0, 54, 128);

    int prevY = 54;
    for (int x = 0; x < 128; x++) {
        float f = 20.0f * powf(1000.0f, (float)x / 127.0f); // 20Hz..20kHz
        float w = 6.2831853f * f / 44100.0f;

        // |H(e^jw)|^2 in the sin^2(w/2) form rather than the cos(w)/cos(2w) form.
        // At low cutoff the poles sit near DC (a1~=-2, a2~=1) and in the passband
        // cos(w),cos(2w) are all ~1, so the direct sum 1+a1^2+a2^2+...cos terms is
        // ~6-8+2 collapsing to ~1e-10 -- below float32's ~7-digit precision, so the
        // result is pure rounding noise and the passband fell to the floor. Here the
        // leading term is an exact squared sum and the remainder scales with the
        // small quantity p = sin^2(w/2), so it stays accurate near DC.
        float sh = sinf(0.5f * w);
        float p = sh * sh;                       // sin^2(w/2)

        float bSum = b0 + b1 + b2;
        float num = bSum * bSum
                  - 4.0f * (b0 * b1 + b1 * b2 + 4.0f * b0 * b2) * p
                  + 16.0f * b0 * b2 * p * p;
        float aSum = 1.0f + a1 + a2;
        float den = aSum * aSum
                  - 4.0f * (a1 + a1 * a2 + 4.0f * a2) * p
                  + 16.0f * a2 * p * p;
        if (num < 1e-12f) num = 1e-12f;
        if (den < 1e-12f) den = 1e-12f;

        // num/den is |H|^2 for one stage; two identical stages double the dB
        float dB = 20.0f * log10f(num / den); // 10*log10 per stage * 2 stages

        int y = (int)((24.0f - dB) * (54.0f / 84.0f));
        if (y < 0) y = 0;
        if (y > 54) y = 54;

        if (x > 0) {
            oled->drawLine(x - 1, prevY, x, y);
        }
        prevY = y;
    }
}

// Raw ADC diagnostic view: unfiltered readings for all 26 knob channels in a
// 3-column grid. ADC0 channels are labelled 0-15, ADC1 channels b0-b9. Use
// this to tell electrical noise (raw value visibly dances) apart from
// processing issues (raw steady but mapped value wrong).
void DisplayManager::drawRawAdc() {
    char buf[12];
    const int total = 26; // 16 on ADC0 + 10 on ADC1
    for (int idx = 0; idx < total; idx++) {
        int col = idx / 9;          // 3 columns of up to 9 rows
        int row = idx % 9;
        int x = 1 + col * 43;
        int y = 5 + row * 6;        // 9 rows: y = 5,11,...,53

        int value;
        if (idx < 16) {
            value = panel->rawADC0(idx);
            snprintf(buf, sizeof(buf), "%d:%d", idx, value);
        } else {
            int b = idx - 16;
            value = panel->rawADC1(b);
            snprintf(buf, sizeof(buf), "b%d:%d", b, value);
        }
        oled->setCursor(x, y);
        oled->print(buf);
    }
}

// Patch file list with cursor
void DisplayManager::drawFileSelect() {
    if (fileCount == 0) {
        oled->setCursor(1, 5);
        oled->print(patches->ready() ? "NO PATCHES" : "NO SD CARD");
        return;
    }

    // Keep the cursor row visible (6 rows fit above the menu bar)
    const int rows = 6;
    if (fileCursor < fileScroll) fileScroll = fileCursor;
    if (fileCursor >= fileScroll + rows) fileScroll = fileCursor - rows + 1;

    for (int i = 0; i < rows; i++) {
        int idx = fileScroll + i;
        if (idx >= fileCount) break;
        oled->setCursor(1, 5 + i * 8);
        oled->print(fileNames[idx]);
        if (idx == fileCursor) {
            oled->print("<");
        }
    }
}

// Small transient message in the top-right corner
void DisplayManager::drawStatusOverlay() {
    if (!statusMsg[0] || (int32_t)(millis() - statusUntil) >= 0) return;

    int w = strlen(statusMsg) * 4 + 5;
    int x = 127 - w;
    oled->setDrawColor(0);
    oled->drawBox(x, 0, w, 9);
    oled->setDrawColor(1);
    oled->drawFrame(x, 0, w, 9);
    oled->setCursor(x + 3, 7);
    oled->print(statusMsg);
}
