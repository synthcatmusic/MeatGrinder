#pragma once

// MEATGRINDER - DSP engine
// ---------------------------------------------------------------------------
// Self-contained (only juce_audio_basics + juce_dsp), so it can be unit-tested
// without any GUI / plugin code.  Signal flow per channel (controls are linked
// across channels so the stereo image is preserved):
//
//   in -> DRIVE -> [density compressor] -> [transient drive-duck]
//      -> COLOR pre-emphasis shelf
//      -> upsample (1x/2x/4x linear-phase FIR)
//           A: tanh "warmth" + even-harmonic injection
//           B: symmetric density saturation        (FATNESS 10-70 %)
//           C: soft / hard clipper                 (FATNESS 55-100 %)
//           D: ceiling (soft knee or hard)
//           + harmonic exciter (presence)
//      -> downsample -> DC blocker -> COLOR de-emphasis + tone low-pass
//      -> RMS-matched gain compensation -> safety ceiling
//      -> MIX (latency-aligned dry) -> OUTPUT -> bypass crossfade -> guard

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace mg
{

struct Settings
{
    float fatness = 0.35f;   // 0..1
    float color = 0.5f;      // 0..1   (0 = warm/dark, 0.5 = neutral, 1 = bright/edgy)
    float driveDb = 6.0f;    // 0..36 dB
    float punch = 0.5f;      // 0..1   (0 = dense/compressed, 1 = transients preserved)
    float mix = 1.0f;        // 0..1
    float outputDb = 0.0f;   // -24..+12 dB
    bool bypass = false;
    bool gainComp = true;
    bool hardClip = false;
    int oversampling = 1;    // 0 = 1x, 1 = 2x, 2 = 4x
};

// --------------------------------------------------------------------------
// Waveshaping primitives (header-inline so the editor can draw the real curve)
// --------------------------------------------------------------------------

// Rational tanh approximation.  C1-continuous, exactly +-1 for |x| >= 3.
inline float fastTanh (float x) noexcept
{
    x = x < -3.0f ? -3.0f : (x > 3.0f ? 3.0f : x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Transparent up to `knee`, then smoothly approaches exactly `ceiling` (at knee + 3*(ceiling-knee)).
inline float softCeiling (float x, float knee = 0.8f, float ceiling = 1.0f) noexcept
{
    const float range = ceiling - knee;
    const float a = std::fabs (x);
    if (a <= knee)
        return x;
    const float y = knee + range * fastTanh ((a - knee) / range);
    return x < 0.0f ? -y : y;
}

// The shaper's own ceiling sits just under the final 0 dBFS ceiling, so the base-rate safety ceiling
// only ever touches filter overshoot and never re-compresses the (oversampled) plateau -> no new aliasing.
inline constexpr float kShaperCeiling = 0.80f;   // -1.9 dBFS: leaves room for ~15 % oversampling-filter overshoot
inline constexpr float kOuterKnee = 0.95f;

struct ShaperParams
{
    float gA = 1.0f, invTanhA = 1.0f;     // stage A
    float even = 0.0f;                    // even-harmonic injection
    float aB = 0.0f, gB = 1.0f, invTanhB = 1.0f;   // stage B
    float aC = 0.0f, gC = 1.0f;           // stage C
    float exciter = 0.0f;                 // harmonic exciter amount
    bool hard = false;
};

class MeatGrinderEngine
{
public:
    static constexpr int kChunk = 32;     // control-rate block (samples)

    MeatGrinderEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // In-place.  Handles 1 or 2 channels (extra channels are left untouched).
    void process (juce::AudioBuffer<float>& buffer, const Settings& settings);

    // Select oversampling (0 = 1x, 1 = 2x, 2 = 4x).  Also done automatically by process().
    void setOversamplingMode (int index);

    // Latency (samples) of the currently selected oversampling mode.
    int getLatencySamples() const noexcept { return latency; }

    // Peak meters, written by the audio thread, read-and-cleared by the UI.
    struct Meters
    {
        std::atomic<float> inPeak[2] {};
        std::atomic<float> outPeak[2] {};
        std::atomic<bool> inClip { false };
        std::atomic<bool> outClip { false };
    };
    Meters meters;

    // Static helpers (shared with the UI + tests)
    static ShaperParams makeShaperParams (float fatness, float color, bool hard) noexcept;
    static float shape (float u, const ShaperParams& p) noexcept;        // full curve: core + ceiling
    static float shapeCore (float u, const ShaperParams& p) noexcept;    // stages A-C
    static float applyCeiling (float s, const ShaperParams& p) noexcept; // stage D
    static float transferCurve (float x, float fatness, float color, float driveDb, bool hard) noexcept;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        float process (float x) noexcept
        {
            const double in = (double) x;
            const double out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return (float) out;
        }

        void reset() noexcept { z1 = z2 = 0; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept;
        void makeHighShelf (double fs, double f0, double gainDb, double q) noexcept;
        void makeLowPass (double fs, double f0, double q) noexcept;
        void makeHighPass (double fs, double f0, double q) noexcept;
    };

    struct Channel
    {
        Biquad preShelf, postShelf, tone;       // base rate
        Biquad exHp1, exHp2;                    // oversampled rate
        double dcX = 0, dcY = 0;
        float exEnv = 0.0f;                     // exciter band envelope
        double osDcX = 0, osDcY = 0;            // DC blocker inside the oversampled stage
        void reset() noexcept
        {
            preShelf.reset(); postShelf.reset(); tone.reset(); exHp1.reset(); exHp2.reset();
            dcX = dcY = 0;
            exEnv = 0.0f;
            osDcX = osDcY = 0;
        }
    };

    void setOversampling (int index);
    void applySettings (const Settings& s);
    void resetDsp();
    void updateColor (float color);
    void updateDynamicsCoefficients (float punch);
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int m, int numCh);
    void runShaper (int ch, float* data, int num) noexcept;

    bool prepared = false;
    bool firstBlock = true;
    double fs = 44100.0;

    // oversampling
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int osIndex = 1, osFactor = 2, latency = 0;

    // dry path delay (latency compensation)
    static constexpr int kDelayCap = 2048;
    std::array<std::vector<float>, 2> delayBuf;
    int delayPos = 0, delaySamples = 0;

    // work buffers
    std::array<std::array<float, kChunk>, 2> inBuf {}, work {}, dryAl {};
    std::array<float, kChunk> duckBuf {};

    // smoothed parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fatSm, colSm, punchSm, mixSm, outSm, bypassSm, compSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveSm;

    Settings cur;
    ShaperParams shaper;
    std::array<Channel, 2> ch;
    float lastColor = -1.0f;
    bool toneActive = false;
    bool wasBypassed = false;
    bool nanSeen = false;

    // dynamics state / coefficients
    float envF = 0, envS = 0, pk = 0, rmsMs = 0;
    float tfAtt = 0, tfRel = 0, tsAtt = 0, pkAtt = 0, pkRel = 0, rmsCoef = 0;
    float densAmt = 0, densRatio = 1, duckAmt = 0;
    static constexpr float kCrestThreshDb = 4.0f;

    // gain compensation
    float msDry = 0, msWet = 0, compGain = 1.0f, msCoef = 0, compCoef = 0;

    double dcR = 0.999, osDcR = 0.9999;
    float exAtt = 0.1f, exRel = 0.001f;   // exciter envelope coefficients (oversampled rate)
};

} // namespace mg
