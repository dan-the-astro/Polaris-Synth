#include <cstdint>
class SynthVoice {
    public:

        SynthVoice() {
            // Constructor
        }
    
    private:

        // Fixed for lifetime of voice
        bool isPlaying = false;
        uint8_t midiNote = 0;

        // Per sample changing values (Q16.16 fixed point)
        int32_t OscALevel = 0;
        int32_t OscBLevel = 0;
        int32_t NoiseLevel = 0;
        int32_t PostMixerLevel = 0;
        int32_t PostFilterLevel = 0;
        int32_t PostAmpLevel = 0;
        int32_t FilterADSRLevel = 0;
        int32_t AmpADSRLevel = 0;

        // Per sample changing values (integers)
        int32_t OscAPhase = 0;
        int32_t OscBPhase = 0;

        int32_t OscAPhaseIncrement = 0;
        int32_t OscBPhaseIncrement = 0;

        // Per sample changing values

    

};