#pragma once

#include <cstdint>
#include <cmath>

// RBJ audio-cookbook low-pass biquad coefficient calculation, shared by the
// synth voices (fixed point Q2.30) and the display Bode plot (float).
//
// F is the cutoff expressed as a 2^32-scaled phase increment
// (F = 2^32 * fc / fs), which is what the pitch lookup tables produce, so
// w0 = 2*pi*fc/fs = F * (2*pi / 2^32).

inline void polarisLowpassCoeffsFloat(uint32_t F, float q, float out[5]) {
    float w0 = static_cast<float>(F) * 1.46291808e-9f; // 2*pi / 2^32
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2.0f * q);

    float a0inv = 1.0f / (1.0f + alpha);
    float b1 = 1.0f - c;

    out[0] = (b1 * 0.5f) * a0inv;    // b0
    out[1] = b1 * a0inv;             // b1
    out[2] = (b1 * 0.5f) * a0inv;    // b2
    out[3] = (-2.0f * c) * a0inv;    // a1
    out[4] = (1.0f - alpha) * a0inv; // a2
}

// Same coefficients scaled to Q2.30 for the fixed-point voice filter
inline void polarisLowpassCoeffsQ30(uint32_t F, float q, int32_t out[5]) {
    float f[5];
    polarisLowpassCoeffsFloat(F, q, f);
    for (int i = 0; i < 5; i++) {
        out[i] = static_cast<int32_t>(f[i] * 1073741824.0f); // * 2^30
    }
}
