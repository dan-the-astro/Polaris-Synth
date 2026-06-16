#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SynthEngine.h"
#include "FrontPanelState.h"
#include "PolarisShared.h"
#if SYNTH_PROFILE
#include "xtensa/core-macros.h"
#endif

SynthEngine::SynthEngine(uint8_t numVoices) {
    // Initialize voices
    voiceCount = numVoices;
    voices = new SynthVoice[voiceCount];

    initI2S();

    // Success message
    Serial.println("SynthEngine initialized with " + String(voiceCount) + " voices.");
}

// Build the per-buffer snapshot of everything the audio-rate code reads
static RenderParams makeRenderParams(const FrontPanelState& fp) {
    RenderParams p;
    p.aSaw = fp.OscA_saw_wave;
    p.aTri = fp.OscA_triangle_wave;
    p.aSqr = fp.OscA_square_wave;
    p.aSin = fp.OscA_sine_wave;
    p.bSaw = fp.OscB_saw_wave;
    p.bTri = fp.OscB_triangle_wave;
    p.bSqr = fp.OscB_square_wave;
    p.bSin = fp.OscB_sine_wave;
    p.polyFreqA = fp.Poly_freq_A;
    p.polyPwA = fp.Poly_pw_A;
    p.polyOscBAmt = fp.Poly_oscB_amount;
    p.polyEnvAmt = fp.Poly_filt_env_amt;
    p.lvlA = fp.Mixer_oscA_level;
    p.lvlB = fp.Mixer_oscB_level;
    p.lvlN = fp.Mixer_noise_level;
    p.masterLevel = fp.Master_level;
    return p;
}

void SynthEngine::run() {
    // Wait for the PolarisManager (core 1) to finish bringing up the front
    // panel hardware
    while (!PolarisShared::frontPanel || !PolarisShared::midiEventQueue) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const FrontPanelState& fp = *PolarisShared::frontPanel;

    Serial.println("SynthEngine running.");

    for (;;) {
#if SYNTH_PROFILE
        profStartCcount = xthal_get_ccount();
#endif
        tickCounter++;

        // 1. Voice allocation from queued MIDI events
        drainMidiEvents(fp);

        // Release every held voice if the USB host recovered a wedged link,
        // so a note that was down during the glitch doesn't stay stuck on.
        if (PolarisShared::allNotesOff) {
            PolarisShared::allNotesOff = false;
            for (uint8_t i = 0; i < voiceCount; i++) {
                if (voices[i].isPlaying) {
                    voices[i].noteOff();
                }
            }
        }

        // 2. Global modulation: LFO, then wheel mod
        lfoTick(fp);
        wheelModTick(fp);

        // 3. Per-voice control-rate processing
        RenderParams p = makeRenderParams(fp);
        int32_t bend = PolarisShared::pitchBendUnits;
        for (uint8_t i = 0; i < voiceCount; i++) {
            SynthVoice& v = voices[i];
            if (!v.isPlaying) continue;
            v.updateFilterEnvelope(fp);
            v.updateAmpEnvelope(fp);
            v.updateGlide(fp.Glide_rate);
            v.updatePitchAndPulseWidth(fp, wheelPitchUnits, wheelPWQ16, bend);
            v.updatePolyModSnapshot(p);
            v.updateFilterCoefficients(fp, wheelFilterUnits);
        }

        // 4. Render 60 samples and push them to the I2S DMA (blocking write
        // paces this loop at the 735Hz control rate)
        renderBuffer(p);

#if SYNTH_PROFILE
        profileReport();
#endif
    }
}

#if SYNTH_PROFILE
// Capture the compute cost of this buffer (everything since the top of the loop
// up to here), excluding the blocking i2s_write that follows. Tracks the
// worst-case buffer and the number of voices that were sounding for it.
void IRAM_ATTR SynthEngine::profileBufferEnd() {
    uint32_t elapsed = xthal_get_ccount() - profStartCcount;
    profSumCycles += elapsed;
    profBufferCount++;
    if (elapsed > profWorstCycles) {
        profWorstCycles = elapsed;
        uint8_t playing = 0;
        for (uint8_t v = 0; v < voiceCount; v++) {
            if (voices[v].isPlaying) playing++;
        }
        profWorstVoices = playing;
    }
}

// Log worst-case and average compute load once per ~second. "load" is the
// worst buffer as a percentage of the per-buffer cycle budget at the current
// CPU clock and sample rate, so it reads directly as headroom used.
void SynthEngine::profileReport() {
    const uint32_t buffersPerSecond = (uint32_t)SAMPLE_RATE / kSamplesPerBuffer;
    if (profBufferCount < buffersPerSecond) return;

    uint64_t cpuHz = (uint64_t)getCpuFrequencyMhz() * 1000000ull;
    uint32_t budget = (uint32_t)(cpuHz * kSamplesPerBuffer / (uint64_t)SAMPLE_RATE);
    uint32_t avg = (uint32_t)(profSumCycles / profBufferCount);
    uint32_t loadPct = (uint32_t)((uint64_t)profWorstCycles * 100 / budget);

    Serial.printf(
        "[synthprof] worst=%u avg=%u budget=%u cy/buf  load=%u%% (worst, %u voices sounding)\n",
        profWorstCycles, avg, budget, loadPct, profWorstVoices);

    profWorstCycles = 0;
    profSumCycles = 0;
    profBufferCount = 0;
    profWorstVoices = 0;
}
#endif

void SynthEngine::drainMidiEvents(const FrontPanelState& fp) {
    MidiEvent e;
    while (xQueueReceive(PolarisShared::midiEventQueue, &e, 0) == pdTRUE) {
        if (e.type == 1) {
            noteOnEvent(e.note, fp.unison);
        } else {
            noteOffEvent(e.note, fp.unison);
        }
    }
}

void SynthEngine::noteOnEvent(uint8_t note, bool unisonOn) {
    if (unisonOn) {
        // Unison stacks every voice on the played note with a slight detune
        for (uint8_t i = 0; i < voiceCount; i++) {
            voices[i].noteOn(note, tickCounter, kUnisonDetuneUnits[i & 7]);
        }
        return;
    }
    SynthVoice* v = allocateVoice(note);
    v->noteOn(note, tickCounter, 0);
}

void SynthEngine::noteOffEvent(uint8_t note, bool unisonOn) {
    // Release every held voice playing this note (covers unison too)
    for (uint8_t i = 0; i < voiceCount; i++) {
        if (voices[i].gateOn && voices[i].midiNote == note) {
            voices[i].noteOff();
        }
    }
    (void)unisonOn;
}

SynthVoice* SynthEngine::allocateVoice(uint8_t note) {
    // Reuse a voice already playing this note (prevents layering)
    for (uint8_t i = 0; i < voiceCount; i++) {
        if (voices[i].isPlaying && voices[i].midiNote == note) {
            return &voices[i];
        }
    }
    // Otherwise take a free voice
    for (uint8_t i = 0; i < voiceCount; i++) {
        if (!voices[i].isPlaying) {
            return &voices[i];
        }
    }
    // All busy: steal the oldest. Prefer released (gate off) voices so held
    // chords survive as long as possible.
    SynthVoice* oldest = &voices[0];
    bool oldestHeld = oldest->gateOn;
    for (uint8_t i = 1; i < voiceCount; i++) {
        SynthVoice& v = voices[i];
        bool better = (!v.gateOn && oldestHeld) ||
                      ((v.gateOn == oldestHeld) && (v.startTick < oldest->startTick));
        if (better) {
            oldest = &v;
            oldestHeld = v.gateOn;
        }
    }
    return oldest;
}
