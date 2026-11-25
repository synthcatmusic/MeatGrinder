// MEATGRINDER - offline DSP verification.
// Build target: MeatGrinderDspTests   (cmake --build build --target MeatGrinderDspTests)
// Exit code 0 = all checks passed.

#include "../Source/MeatGrinderEngine.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <random>

using mg::MeatGrinderEngine;
using mg::Settings;

namespace
{
constexpr double kFs = 44100.0;
constexpr int kOrder = 15;                 // FFT 32768
constexpr int kN = 1 << kOrder;
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("   [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

float db (double x) { return (float) (20.0 * std::log10 (std::max (x, 1.0e-12))); }

using Signal = std::vector<float>;

// Runs the engine over L/R in blocks.  Settings can change per block through `perBlock`.
void run (const Signal& inL, const Signal& inR, Signal& outL, Signal& outR, Settings s,
          int block = 512, const std::function<void (int, Settings&)>& perBlock = {},
          MeatGrinderEngine* external = nullptr)
{
    MeatGrinderEngine local;
    MeatGrinderEngine& e = external ? *external : local;
    if (! external) e.prepare (kFs, block);

    const int n = (int) inL.size();
    outL.assign ((size_t) n, 0.0f);
    outR.assign ((size_t) n, 0.0f);
    juce::AudioBuffer<float> buf (2, block);

    for (int pos = 0, b = 0; pos < n; pos += block, ++b)
    {
        const int m = std::min (block, n - pos);
        buf.setSize (2, m, false, false, true);
        for (int i = 0; i < m; ++i) { buf.setSample (0, i, inL[(size_t) (pos + i)]); buf.setSample (1, i, inR[(size_t) (pos + i)]); }
        if (perBlock) perBlock (b, s);
        e.process (buf, s);
        for (int i = 0; i < m; ++i) { outL[(size_t) (pos + i)] = buf.getSample (0, i); outR[(size_t) (pos + i)] = buf.getSample (1, i); }
    }
}

Signal sine (int n, double f, double amp)
{
    Signal x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / kFs));
    return x;
}

Signal noise (int n, double amp, unsigned seed = 1)
{
    std::mt19937 g (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    Signal x ((size_t) n);
    for (auto& v : x) v = (float) amp * d (g);
    return x;
}

Signal pink (int n, double amp, unsigned seed = 1)
{
    std::mt19937 g (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    Signal x ((size_t) n);
    float b0 = 0, b1 = 0, b2 = 0;
    for (auto& v : x)
    {
        const float w = d (g);
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        v = (float) amp * 0.25f * (b0 + b1 + b2 + w * 0.1848f);
    }
    return x;
}

Signal saw (int n, double f, double amp)   // band-limited, 1/k harmonics
{
    Signal x ((size_t) n, 0.0f);
    for (int k = 1; k * f < 18000.0; ++k)
        for (int i = 0; i < n; ++i)
            x[(size_t) i] += (float) (amp * (2.0 / juce::MathConstants<double>::pi) / k * std::sin (2.0 * juce::MathConstants<double>::pi * f * k * i / kFs));
    return x;
}

double rms (const Signal& x, int from, int count)
{
    double a = 0;
    for (int i = 0; i < count; ++i) a += (double) x[(size_t) (from + i)] * x[(size_t) (from + i)];
    return std::sqrt (a / count);
}

Signal power (const Signal& x, int from)
{
    juce::dsp::FFT fft (kOrder);
    std::vector<float> buf ((size_t) (2 * kN), 0.0f);
    for (int i = 0; i < kN; ++i)
        buf[(size_t) i] = x[(size_t) (from + i)] * (0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / kN));
    fft.performRealOnlyForwardTransform (buf.data(), true);
    Signal p ((size_t) (kN / 2 + 1));
    for (int k = 0; k <= kN / 2; ++k) p[(size_t) k] = buf[(size_t) (2 * k)] * buf[(size_t) (2 * k)] + buf[(size_t) (2 * k + 1)] * buf[(size_t) (2 * k + 1)];
    return p;
}

double bandPower (const Signal& p, int centre, int half)
{
    double a = 0;
    for (int k = std::max (0, centre - half); k <= std::min (kN / 2, centre + half); ++k) a += p[(size_t) k];
    return a;
}

// THD (%) of a coherent sine at bin b0
double thd (const Signal& y, int b0)
{
    auto p = power (y, (int) y.size() - kN);
    const double fund = bandPower (p, b0, 2);
    double h = 0;
    for (int k = 2; k * b0 < kN / 2 - 4 && k <= 14; ++k) h += bandPower (p, k * b0, 2);
    return 100.0 * std::sqrt (h / fund);
}

// Energy in [f0,f1) relative to the total (5 Hz..22 kHz), in dB
double bandFractionDb (const Signal& y, double f0, double f1)
{
    auto p = power (y, (int) y.size() - kN);
    double band = 0, total = 0;
    for (int k = 0; k <= kN / 2; ++k)
    {
        const double f = k * kFs / kN;
        if (f >= 5.0 && f < 22000.0) total += p[(size_t) k];
        if (f >= f0 && f < f1) band += p[(size_t) k];
    }
    return 10.0 * std::log10 (std::max (band / total, 1.0e-18));
}

double centroid (const Signal& y)
{
    auto p = power (y, (int) y.size() - kN);
    double num = 0, den = 0;
    for (int k = 4; k <= kN / 2; ++k) { const double f = k * kFs / kN; num += f * p[(size_t) k]; den += p[(size_t) k]; }
    return num / den;
}

Settings base()
{
    Settings s;
    s.fatness = 0.5f; s.color = 0.5f; s.driveDb = 6.0f; s.punch = 0.5f;
    s.mix = 1.0f; s.outputDb = 0.0f; s.gainComp = true; s.oversampling = 1;
    return s;
}

double binFreq (int b) { return b * kFs / kN; }
} // namespace

int main()
{
    juce::ScopedNoDenormals noDenormals;
    const int b110 = 82;                       // coherent 110.35 Hz
    const int bHi = 3715;                      // coherent ~5.0 kHz

    // ------------------------------------------------------------------ 1
    std::printf ("\n1. Latency reporting + dry alignment (MIX=0 and BYPASS must equal delayed input)\n");
    for (int os = 0; os < 3; ++os)
    {
        auto in = noise (20000, 0.5);
        Signal oL, oR;
        MeatGrinderEngine e;
        e.prepare (kFs, 512);
        Settings s = base(); s.mix = 0.0f; s.oversampling = os;
        run (in, in, oL, oR, s, 512, {}, &e);
        const int lat = e.getLatencySamples();
        double err = 0;
        for (int i = lat; i < (int) in.size(); ++i) err = std::max (err, (double) std::fabs (oL[(size_t) i] - in[(size_t) (i - lat)]));
        char msg[160]; std::snprintf (msg, sizeof msg, "%dx: latency %d samples (%.2f ms), MIX=0 error %.1e", 1 << os, lat, 1000.0 * lat / kFs, err);
        check (err < 1.0e-6, msg);

        MeatGrinderEngine e2; e2.prepare (kFs, 512);
        s.mix = 1.0f; s.bypass = true;
        run (in, in, oL, oR, s, 512, {}, &e2);
        err = 0;
        for (int i = lat; i < (int) in.size(); ++i) err = std::max (err, (double) std::fabs (oL[(size_t) i] - in[(size_t) (i - lat)]));
        std::snprintf (msg, sizeof msg, "%dx: BYPASS error %.1e", 1 << os, err);
        check (err < 1.0e-6, msg);
    }

    // ------------------------------------------------------------------ 2
    std::printf ("\n2. Stability + output ceiling: +12 dBFS noise, FATNESS 100 %%, DRIVE +36 dB, all modes\n");
    {
        bool finite = true, bounded = true; int runs = 0; float worst = 0;
        for (int os = 0; os < 3; ++os)
            for (float col : { 0.0f, 0.5f, 1.0f })
                for (float pun : { 0.0f, 0.5f, 1.0f })
                    for (bool hard : { false, true })
                    {
                        auto in = noise (22050, 4.0, (unsigned) (os * 100 + runs));
                        Signal oL, oR;
                        Settings s = base(); s.fatness = 1.0f; s.driveDb = 36.0f; s.color = col; s.punch = pun; s.hardClip = hard; s.oversampling = os; s.mix = 1.0f;
                        run (in, in, oL, oR, s);
                        for (size_t i = 0; i < oL.size(); ++i)
                        {
                            if (! std::isfinite (oL[i]) || ! std::isfinite (oR[i])) finite = false;
                            worst = std::max (worst, std::fabs (oL[i]));
                            if (std::fabs (oL[i]) > 1.0f + 1.0e-4f) bounded = false;
                        }
                        ++runs;
                    }
        char msg[160];
        std::snprintf (msg, sizeof msg, "%d configurations, all finite", runs);
        check (finite, msg);
        std::snprintf (msg, sizeof msg, "wet output never exceeds 0 dBFS (worst %.4f)", worst);
        check (bounded, msg);

        auto in = noise (22050, 4.0, 7);
        Signal oL, oR;
        Settings s = base(); s.fatness = 1.0f; s.driveDb = 36.0f; s.outputDb = 12.0f; s.hardClip = true;
        run (in, in, oL, oR, s);
        float pkv = 0; for (auto v : oL) pkv = std::max (pkv, std::fabs (v));
        std::snprintf (msg, sizeof msg, "OUTPUT +12 dB on a clipped signal peaks at %.2f (%.1f dBFS) - bounded by guard", pkv, db (pkv));
        check (pkv <= 8.0f, msg);
    }

    // ------------------------------------------------------------------ 3
    std::printf ("\n3. DC offset: asymmetric (even-harmonic) settings must not leak DC\n");
    for (float F : { 0.3f, 0.6f, 1.0f })
    {
        auto in = sine (kN * 2, 100.0, 0.5);
        Signal oL, oR;
        Settings s = base(); s.fatness = F; s.color = 0.0f; s.driveDb = 12.0f; s.gainComp = false;
        run (in, in, oL, oR, s);
        double mean = 0; for (int i = kN; i < 2 * kN; ++i) mean += oL[(size_t) i]; mean /= kN;
        const double r = rms (oL, kN, kN);
        char msg[120]; std::snprintf (msg, sizeof msg, "FATNESS %.0f %%: DC / RMS = %.4f", F * 100, std::fabs (mean) / r);
        check (std::fabs (mean) / r < 0.01, msg);
    }

    // ------------------------------------------------------------------ 4
    std::printf ("\n4. FATNESS changes the signal (harmonic content), not just the level (GAIN COMP on)\n");
    {
        double prev = -1; bool mono = true; double rmsMin = 1e9, rmsMax = -1e9;
        auto in = sine (kN * 2, binFreq (b110), 0.125);   // -18 dBFS
        const double rmsIn = rms (in, kN, kN);
        for (float F : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            Signal oL, oR;
            Settings s = base(); s.fatness = F; s.driveDb = 6.0f;
            run (in, in, oL, oR, s);
            const double t = thd (oL, b110);
            const double r = db (rms (oL, kN, kN) / rmsIn);
            rmsMin = std::min (rmsMin, r); rmsMax = std::max (rmsMax, r);
            std::printf ("   FATNESS %3.0f %%  THD %6.2f %%   level vs input %+5.2f dB\n", F * 100, t, r);
            if (t <= prev) mono = false;
            prev = t;
        }
        check (mono, "THD rises monotonically with FATNESS");
        check (prev > 15.0, "FATNESS 100 % is heavily distorted (THD > 15 %)");
        check (rmsMax - rmsMin < 2.0, "loudness stays within 2 dB across the FATNESS range (gain comp) -> change is timbral");
    }

    // ------------------------------------------------------------------ 5
    std::printf ("\n5. COLOR changes the spectral character (FATNESS 60 %%, GAIN COMP on)\n");
    {
        auto pinkIn = pink (kN * 2, 0.4, 3);
        auto sawIn = saw (kN * 2, 82.5, 0.3);
        auto sineIn = sine (kN * 2, binFreq (b110), 0.125);
        double hfPink[5], hfSaw[5], h2h3[5];
        int i = 0;
        std::printf ("   COLOR | pink noise >2k | saw bass >2k | sine110 H2-H3\n");
        for (float col : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            Settings s = base(); s.fatness = 0.6f; s.color = col;
            Signal o, dummy;
            run (pinkIn, pinkIn, o, dummy, s);  hfPink[i] = bandFractionDb (o, 2000.0, 22000.0);
            run (sawIn, sawIn, o, dummy, s);    hfSaw[i] = bandFractionDb (o, 2000.0, 22000.0);
            run (sineIn, sineIn, o, dummy, s);
            auto p = power (o, (int) o.size() - kN);
            h2h3[i] = 10.0 * std::log10 (bandPower (p, 2 * b110, 2) / bandPower (p, 3 * b110, 2));
            std::printf ("   %4.0f %% | %9.1f dB | %9.1f dB | %9.1f dB\n", col * 100, hfPink[i], hfSaw[i], h2h3[i]);
            ++i;
        }
        bool mono = true, monoEO = true;
        for (int k = 1; k < 5; ++k) { if (hfPink[k] <= hfPink[k - 1]) mono = false; if (h2h3[k] >= h2h3[k - 1]) monoEO = false; }
        check (mono && hfPink[4] - hfPink[0] > 6.0, "high-frequency share rises monotonically warm -> bright (> 6 dB range, pink noise)");
        check (hfSaw[4] > hfSaw[2] && hfSaw[2] > hfSaw[0] && hfSaw[4] - hfSaw[0] > 6.0, "bass synth gets more upper harmonics when bright (> 6 dB range)");
        check (monoEO, "warm is even-harmonic rich, bright is odd-harmonic dominated (H2-H3 falls monotonically)");
    }

    // ------------------------------------------------------------------ 6
    std::printf ("\n6. DRIVE controls how hard the stages are driven (THD, -18 dBFS sine)\n");
    {
        double prev = -1; bool mono = true;
        auto in = sine (kN * 2, binFreq (b110), 0.125);
        for (float d : { 0.0f, 6.0f, 12.0f, 24.0f })
        {
            Signal oL, oR;
            Settings s = base(); s.fatness = 0.5f; s.driveDb = d;
            run (in, in, oL, oR, s);
            const double t = thd (oL, b110);
            std::printf ("   DRIVE %+5.1f dB  THD %6.2f %%\n", d, t);
            if (t <= prev) mono = false;
            prev = t;
        }
        check (mono, "THD rises monotonically with DRIVE");
    }

    // ------------------------------------------------------------------ 7
    std::printf ("\n7. Oversampling reduces aliasing (5 kHz sine, alias = all non-harmonic energy)\n");
    {
        auto in = sine (kN * 2, binFreq (bHi), 0.5);
        auto aliasDb = [&] (int os, float fat, float drv, bool hard)
        {
            Signal oL, oR;
            Settings s = base(); s.fatness = fat; s.driveDb = drv; s.hardClip = hard; s.oversampling = os; s.gainComp = false;
            run (in, in, oL, oR, s);
            auto p = power (oL, kN);
            const double fund = bandPower (p, bHi, 3);
            double harm = 0, total = 0;
            for (int k = 1; k * bHi <= kN / 2; ++k) harm += bandPower (p, k * bHi, 3);
            for (int k = 10; k <= kN / 2; ++k) total += p[(size_t) k];
            return (double) db (std::sqrt ((total - harm) / fund));
        };

        double typ[3];
        for (int os = 0; os < 3; ++os) typ[os] = aliasDb (os, 0.5f, 12.0f, false);
        std::printf ("   typical (FATNESS 50 %%, DRIVE +12, soft):  1x %6.1f dB   2x %6.1f dB   4x %6.1f dB\n", typ[0], typ[1], typ[2]);
        std::printf ("   extreme (FATNESS 80 %%, DRIVE +12, hard):  1x %6.1f dB   2x %6.1f dB   4x %6.1f dB\n",
                     aliasDb (0, 0.8f, 12.0f, true), aliasDb (1, 0.8f, 12.0f, true), aliasDb (2, 0.8f, 12.0f, true));
        std::printf ("   (a hard-clipped sine has harmonics falling only ~1/k, so each doubling of the rate buys ~3 dB there)\n");
        check (typ[1] < typ[0] - 6.0, "typical settings: 2x lowers aliasing by > 6 dB vs 1x");
        check (typ[2] < typ[1] - 3.0, "typical settings: 4x lowers aliasing by > 3 dB vs 2x");
        check (aliasDb (1, 0.8f, 12.0f, true) < aliasDb (0, 0.8f, 12.0f, true) - 6.0, "extreme hard clip: 2x still lowers aliasing by > 6 dB vs 1x");
    }

    // ------------------------------------------------------------------ 8
    std::printf ("\n8. Stereo integrity\n");
    {
        auto in = noise (30000, 0.6, 3);
        Signal zero (in.size(), 0.0f), oL, oR;
        Settings s = base(); s.fatness = 0.9f; s.driveDb = 18.0f;
        run (in, zero, oL, oR, s);
        float mr = 0; for (auto v : oR) mr = std::max (mr, std::fabs (v));
        char msg[160]; std::snprintf (msg, sizeof msg, "hard-panned input: silent channel stays silent (max %.1e)", mr);
        check (mr < 1.0e-6f, msg);

        run (in, in, oL, oR, s);
        float md = 0; for (size_t i = 0; i < oL.size(); ++i) md = std::max (md, std::fabs (oL[i] - oR[i]));
        std::snprintf (msg, sizeof msg, "identical L/R input -> identical L/R output (max diff %.1e)", md);
        check (md < 1.0e-7f, msg);

        // linked controls: quiet side gets the same gain reduction as loud side
        auto loud = sine (kN, 110.0, 0.8), quiet = sine (kN, 110.0, 0.2);
        s.punch = 0.0f; s.fatness = 0.0f; s.gainComp = false; s.driveDb = 0.0f;
        run (loud, quiet, oL, oR, s);
        const float ratio = db (rms (oL, kN / 2, kN / 2) / rms (oR, kN / 2, kN / 2));
        std::snprintf (msg, sizeof msg, "L/R level ratio preserved at low FATNESS (in 12.0 dB, out %.1f dB)", ratio);
        check (std::fabs (ratio - 12.0f) < 2.5f, msg);
    }

    // ------------------------------------------------------------------ 9
    std::printf ("\n9. Mono (1-channel) buffers\n");
    {
        auto in = noise (8192, 0.5, 9);
        Signal oL, oR;
        Settings s = base(); s.fatness = 0.7f;
        run (in, in, oL, oR, s);
        MeatGrinderEngine e; e.prepare (kFs, 512);
        juce::AudioBuffer<float> mono (1, 512);
        float maxErr = 0;
        for (int pos = 0; pos + 512 <= (int) in.size(); pos += 512)
        {
            for (int i = 0; i < 512; ++i) mono.setSample (0, i, in[(size_t) (pos + i)]);
            e.process (mono, s);
            for (int i = 0; i < 512; ++i) maxErr = std::max (maxErr, std::fabs (mono.getSample (0, i) - oL[(size_t) (pos + i)]));
        }
        char msg[100]; std::snprintf (msg, sizeof msg, "mono result equals stereo result (max diff %.1e)", maxErr);
        check (maxErr < 1.0e-6f, msg);
    }

    // ------------------------------------------------------------------ 10
    std::printf ("\n10. Gain compensation (noise at -30 dBFS, FATNESS 70 %%, DRIVE +18 dB)\n");
    {
        auto in = noise (kN * 3, 0.0316, 5);
        Signal oL, oR;
        Settings s = base(); s.fatness = 0.7f; s.driveDb = 18.0f;
        run (in, in, oL, oR, s);
        const double d = db (rms (oL, 2 * kN, kN) / rms (in, 2 * kN, kN));
        char msg[100]; std::snprintf (msg, sizeof msg, "output RMS vs input RMS with GAIN COMP: %+.2f dB", d);
        check (std::fabs (d) < 1.5, msg);
        s.gainComp = false;
        run (in, in, oL, oR, s);
        std::printf ("   (without GAIN COMP the same patch is %+.1f dB louder)\n", db (rms (oL, 2 * kN, kN) / rms (in, 2 * kN, kN)));
    }

    // ------------------------------------------------------------------ 11
    std::printf ("\n11. No clicks on parameter jumps and bypass toggling\n");
    {
        auto in = sine (44100, 110.0, 0.5);
        Signal oL, oR;
        Settings s = base(); s.fatness = 0.3f; s.driveDb = 0.0f; s.gainComp = false;
        run (in, in, oL, oR, s, 256, [] (int b, Settings& st) { if (b >= 80) { st.driveDb = 24.0f; st.fatness = 0.6f; st.color = 0.9f; st.outputDb = -6.0f; } });
        float maxStep = 0; for (size_t i = 1; i < oL.size(); ++i) maxStep = std::max (maxStep, std::fabs (oL[i] - oL[i - 1]));
        Settings fin = s; fin.driveDb = 24.0f; fin.fatness = 0.6f; fin.color = 0.9f; fin.outputDb = -6.0f;
        run (in, in, oL, oR, fin, 256);   // the end state on its own: a driven sine has steep edges by itself
        float finalStep = 0; for (size_t i = 1; i < oL.size(); ++i) finalStep = std::max (finalStep, std::fabs (oL[i] - oL[i - 1]));
        char msg[160]; std::snprintf (msg, sizeof msg, "instant DRIVE 0 -> +24 dB, FATNESS/COLOR jump: max step %.3f vs %.3f for the end state alone", maxStep, finalStep);
        check (maxStep <= finalStep * 1.1f + 0.02f, msg);

        s = base(); s.fatness = 0.8f; s.driveDb = 12.0f;
        run (in, in, oL, oR, s, 256);   // baseline: no toggling (a heavily driven sine has steep edges of its own)
        float baseStep = 0; for (size_t i = 1; i < oL.size(); ++i) baseStep = std::max (baseStep, std::fabs (oL[i] - oL[i - 1]));
        run (in, in, oL, oR, s, 256, [] (int b, Settings& st) { st.bypass = (b >= 60 && b < 100); });
        maxStep = 0; for (size_t i = 1; i < oL.size(); ++i) maxStep = std::max (maxStep, std::fabs (oL[i] - oL[i - 1]));
        std::snprintf (msg, sizeof msg, "bypass on/off while playing: max step %.3f vs %.3f steady-state baseline", maxStep, baseStep);
        check (maxStep <= baseStep * 1.1f + 0.02f, msg);
    }

    // ------------------------------------------------------------------ 12
    std::printf ("\n12. Low-level usefulness (-36 dBFS sine)\n");
    {
        auto in = sine (kN * 2, binFreq (b110), 0.0158);
        Signal oL, oR;
        Settings s = base(); s.fatness = 0.35f; s.driveDb = 6.0f;
        run (in, in, oL, oR, s);
        const double t1 = thd (oL, b110);
        s.driveDb = 24.0f; s.fatness = 0.6f;
        run (in, in, oL, oR, s);
        const double t2 = thd (oL, b110);
        std::printf ("   default settings: THD %.2f %%   DRIVE +24 / FATNESS 60 %%: THD %.2f %%\n", t1, t2);
        check (t2 > 5.0 * t1 && t2 > 5.0, "quiet material can still be driven into rich saturation");
    }

    // ------------------------------------------------------------------ 13
    std::printf ("\n13. CPU load (stereo, 44.1 kHz, 512-sample blocks, 10 s of audio)\n");
    for (int os = 0; os < 3; ++os)
    {
        auto in = noise (441000, 0.5, 11);
        Signal oL, oR;
        Settings s = base(); s.fatness = 1.0f; s.driveDb = 18.0f; s.oversampling = os; s.hardClip = true;
        const auto t0 = std::chrono::steady_clock::now();
        run (in, in, oL, oR, s);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("   %dx: %.2f %% of one core (worst-case settings) - %.0fx realtime\n", 1 << os, 100.0 * sec / 10.0, 10.0 / sec);
        if (os == 2) check (sec / 10.0 < 0.15, "4x oversampled worst case stays below 15 % of one core");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
