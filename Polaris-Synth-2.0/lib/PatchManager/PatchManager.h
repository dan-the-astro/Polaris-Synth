#pragma once

#include <Arduino.h>
#include <cstdint>

// On-disk patch format. Fields are stored exactly in the form FrontPanelState
// holds them (Q16.16 levels/times, pitch index units, etc.) so loading is a
// straight copy.
struct PatchData {
    static constexpr uint32_t kMagic = 0x31544150;  // "PAT1" little-endian
    static constexpr uint16_t kVersion = 1;

    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t reservedPad = 0;

    // Continuous controls
    int32_t lfoInitialAmount = 0;
    int32_t lfoFrequency = 0;
    int32_t oscAFrequency = 0;
    int32_t oscAPulseWidth = 0;
    int32_t oscBFrequency = 0;
    int32_t oscBPulseWidth = 0;
    int32_t oscBFine = 0;
    int32_t wheelSourceMix = 0;
    int32_t polyFiltEnvAmt = 0;
    int32_t polyOscBAmount = 0;
    int32_t mixerOscALevel = 0;
    int32_t mixerOscBLevel = 0;
    int32_t mixerNoiseLevel = 0;
    int32_t filterCutoff = 0;
    int32_t filterResonance = 0;
    int32_t filterEnvAmt = 0;
    int32_t filterAttack = 0;
    int32_t filterDecay = 0;
    int32_t filterSustain = 0;
    int32_t filterRelease = 0;
    int32_t ampAttack = 0;
    int32_t ampDecay = 0;
    int32_t ampSustain = 0;
    int32_t ampRelease = 0;
    int32_t masterLevel = 0;   // captured but not applied on load
    int32_t glideRate = 0;

    // Switches
    uint8_t lfoSaw = 0, lfoTri = 0, lfoSquare = 0, lfoSine = 0;
    uint8_t oscASaw = 0, oscATri = 0, oscASquare = 0, oscASine = 0;
    uint8_t oscBSaw = 0, oscBTri = 0, oscBSquare = 0, oscBSine = 0;
    uint8_t oscBLoFreq = 0, oscBKeyboard = 0;
    uint8_t wheelFreqA = 0, wheelFreqB = 0, wheelPwA = 0, wheelPwB = 0, wheelFilter = 0;
    uint8_t polyFreqA = 0, polyPwA = 0, polyFilter = 0;
    uint8_t filterKeytrack = 0, unisonOn = 0;

    uint8_t reserved[8] = {0};
};

// Saves and loads PatchData structures as .PAT files in the root directory of
// an SD card connected to the HSPI bus.
class PatchManager {
public:
    static constexpr int kMaxPatches = 32;
    static constexpr int kNameLen = 16; // 8.3 name + null, padded

    // Mount the SD card. Returns true on success.
    bool begin();

    bool ready() const { return _ready; }

    // Fill 'names' with up to maxCount .PAT file names (no leading slash),
    // sorted alphabetically. Returns the number of entries found.
    int listPatches(char names[][kNameLen], int maxCount);

    // Save/load a patch. 'name' is the bare file name, e.g. "PATCH1.PAT".
    bool save(const char* name, const PatchData& data);
    bool load(const char* name, PatchData& data);

    // Generate the lowest-numbered unused PATCHn.PAT name.
    void makeNextPatchName(char* out, size_t outLen);

private:
    bool _ready = false;
};
