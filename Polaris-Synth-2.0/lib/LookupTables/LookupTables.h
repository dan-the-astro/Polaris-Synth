// Header for look up tables

#include "esp_attr.h"
#include <cstdint>

#pragma once

// -------------------------------------------------------
// Configuration
// -------------------------------------------------------

#define SINE_TABLE_SIZE     256

#define MIDI_NOTES          128
#define SAMPLE_RATE         44100.0f
#define A4_FREQUENCY        440.0f  // standard reference
#define A4_MIDI_NOTE        69      // standard reference

// If you want fraction coverage from -8 semitones to +8 semitones:
// Regenerate the frac table if these are changed
#define SEMITONE_RANGE_LOW  -8.0f
#define SEMITONE_RANGE_HIGH  8.0f
#define FRAC_TABLE_SIZE     2048


// Lookup table for sine function

static const int32_t DRAM_ATTR sine_table[SINE_TABLE_SIZE];

// Lookup table for MIDI notes to phase increments

static const float DRAM_ATTR midi_note_table[MIDI_NOTES];

// Lookup table for fractional part of phase increment

static const float DRAM_ATTR frac_table[FRAC_TABLE_SIZE];



