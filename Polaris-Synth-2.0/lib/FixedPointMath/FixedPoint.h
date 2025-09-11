#include <cstdint>
// Utility class for Q16.16 fixed-point arithmetic

class FixedPoint {
public:
    static constexpr int FRACTIONAL_BITS = 16;
    static constexpr int32_t SCALING_FACTOR = 1 << FRACTIONAL_BITS;

    // Convert float to Q16.16 fixed-point
    static int32_t fromFloat(float value) {
        return static_cast<int32_t>(value * SCALING_FACTOR);
    }

    // Convert Q16.16 fixed-point to float
    static float toFloat(int32_t fixedValue) {
        return static_cast<float>(fixedValue) / SCALING_FACTOR;
    }

    // Multiply two Q16.16 fixed-point numbers
    static int32_t multiply(int32_t a, int32_t b) {
        return (static_cast<int64_t>(a) * b) >> FRACTIONAL_BITS;
    }

    // Divide two Q16.16 fixed-point numbers
    static int32_t divide(int32_t a, int32_t b) {
        return (static_cast<int64_t>(a) << FRACTIONAL_BITS) / b;
    }

    // Add two Q16.16 fixed-point numbers
    static int32_t add(int32_t a, int32_t b) {
        return a + b;
    }
    
    // Subtract two Q16.16 fixed-point numbers
    static int32_t subtract(int32_t a, int32_t b) {
        return a - b;
    }

};