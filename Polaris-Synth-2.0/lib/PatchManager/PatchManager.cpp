#include "PatchManager.h"

#include <SPI.h>
#include <SD.h>
#include <driver/gpio.h>
#include <cstring>

// SD card SPI bus pins (HSPI, separate from the VSPI bus used by the
// MAX3421E USB host controller)
#define SDSPI_MISO 17
#define SDSPI_MOSI 16
#define SDSPI_SCLK 4
#define SDSPI_CS   13

static SPIClass sdSpi(HSPI);

bool PatchManager::begin() {
    // Bring the bus up on our custom pins. NOTE: the SPI init handshake the SD
    // library runs always happens at 400kHz regardless of the frequency passed
    // to SD.begin(), so the data-rate argument only affects transfers after a
    // successful mount - it can't fix a handshake failure.
    sdSpi.begin(SDSPI_SCLK, SDSPI_MISO, SDSPI_MOSI, SDSPI_CS);

    // Add internal pull-ups on MISO and CS. During the SPI init handshake the
    // SD library polls MISO for the card's response/ready tokens; the card
    // tri-states the line between bytes, and the ESP32's SPI pin attach leaves
    // MISO with no pull resistor. If the breakout has none either, MISO floats
    // and the library reads 0x00 forever ("Select Failed" / f_mount error 3).
    // The weak internal pull-up (~45k) is enough to settle it for many cards;
    // a hardware 10k on MISO is the robust fix if this isn't sufficient.
    gpio_pullup_en(static_cast<gpio_num_t>(SDSPI_MISO));
    gpio_pullup_en(static_cast<gpio_num_t>(SDSPI_CS));

    // Run the data phase at a conservative 1MHz rather than the 4MHz default.
    // Patches are tiny so speed is irrelevant, and over hand-soldered module
    // pads / jumper wiring a marginal high-speed read of the FAT boot sector
    // can corrupt it and surface as "no valid FAT volume" (error 13) even
    // though the 400kHz init handshake succeeded.
    constexpr uint32_t kSdFrequency = 1000000;

    // A few cards need more than one attempt after a cold power-up.
    for (int attempt = 1; attempt <= 4; attempt++) {
        if (SD.begin(SDSPI_CS, sdSpi, kSdFrequency)) {
            _ready = true;
            Serial.printf("PatchManager: SD mounted (type %d, %llu MB) on attempt %d\n",
                          static_cast<int>(SD.cardType()),
                          SD.cardSize() / (1024ULL * 1024ULL), attempt);
            return true;
        }
        Serial.printf("PatchManager: SD mount attempt %d failed\n", attempt);
        SD.end();
        delay(100);
    }

    _ready = false;
    Serial.println("PatchManager: SD card mount failed");
    return false;
}

int PatchManager::listPatches(char names[][kNameLen], int maxCount) {
    if (!_ready) return 0;

    int count = 0;
    File root = SD.open("/");
    if (!root) return 0;

    for (File entry = root.openNextFile(); entry && count < maxCount; entry = root.openNextFile()) {
        if (!entry.isDirectory()) {
            // entry.name() may include the leading path; strip to base name
            const char* full = entry.name();
            const char* base = strrchr(full, '/');
            base = base ? base + 1 : full;

            size_t len = strlen(base);
            if (len > 4 && len < kNameLen && strcasecmp(base + len - 4, ".PAT") == 0) {
                strncpy(names[count], base, kNameLen - 1);
                names[count][kNameLen - 1] = '\0';
                count++;
            }
        }
        entry.close();
    }
    root.close();

    // Insertion sort for a stable, alphabetical listing
    for (int i = 1; i < count; i++) {
        char key[kNameLen];
        strcpy(key, names[i]);
        int j = i - 1;
        while (j >= 0 && strcasecmp(names[j], key) > 0) {
            strcpy(names[j + 1], names[j]);
            j--;
        }
        strcpy(names[j + 1], key);
    }
    return count;
}

bool PatchManager::save(const char* name, const PatchData& data) {
    if (!_ready) return false;

    char path[kNameLen + 1];
    snprintf(path, sizeof(path), "/%s", name);

    // Remove first: ESP32 FILE_WRITE would otherwise leave stale bytes if the
    // file format ever shrinks
    SD.remove(path);

    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;

    size_t written = f.write(reinterpret_cast<const uint8_t*>(&data), sizeof(PatchData));
    f.close();
    return written == sizeof(PatchData);
}

bool PatchManager::load(const char* name, PatchData& data) {
    if (!_ready) return false;

    char path[kNameLen + 1];
    snprintf(path, sizeof(path), "/%s", name);

    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    size_t got = f.read(reinterpret_cast<uint8_t*>(&data), sizeof(PatchData));
    f.close();

    if (got != sizeof(PatchData)) return false;
    if (data.magic != PatchData::kMagic) return false;
    if (data.version != PatchData::kVersion) return false;
    return true;
}

void PatchManager::makeNextPatchName(char* out, size_t outLen) {
    for (int n = 1; n <= 99; n++) {
        char path[kNameLen + 1];
        snprintf(path, sizeof(path), "/PATCH%d.PAT", n);
        if (!SD.exists(path)) {
            snprintf(out, outLen, "PATCH%d.PAT", n);
            return;
        }
    }
    snprintf(out, outLen, "PATCH99.PAT");
}
