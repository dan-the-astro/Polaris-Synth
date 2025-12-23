// Header for look up tables

#pragma once

#include "esp_attr.h"
#include <cstdint>

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

extern const int32_t sine_table[SINE_TABLE_SIZE];

// Lookup table for MIDI notes to phase increments

extern const int32_t midi_note_table[MIDI_NOTES];
// Lookup table for fractional part of phase increment

extern const int32_t frac_table[FRAC_TABLE_SIZE];


