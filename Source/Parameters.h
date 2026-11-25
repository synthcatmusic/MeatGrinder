#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <iterator>

namespace mg
{
namespace id
{
    inline constexpr const char* fatness  = "fatness";
    inline constexpr const char* color    = "color";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* punch    = "punch";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
    inline constexpr const char* bypass   = "bypass";
    inline constexpr const char* gainComp = "gaincomp";
    inline constexpr const char* hardClip = "hardclip";
    inline constexpr const char* oversample = "oversample";
}

struct Preset
{
    const char* name;
    float fatness, color, drive, punch, mix, output;   // 0..1, 0..1, dB, 0..1, 0..1, dB
    int oversampling;                                  // 0 = 1x, 1 = 2x, 2 = 4x
    bool hardClip, gainComp;
};

inline constexpr Preset factoryPresets[] =
{
    //  name                 fat   col   drive  punch  mix   out   os  hard   comp
    { "Init",                0.35f, 0.50f,  6.0f, 0.50f, 1.00f,  0.0f, 1, false, true  },
    { "Warm Glue",           0.22f, 0.22f,  4.0f, 0.30f, 0.85f,  0.0f, 1, false, true  },
    { "Fat Bass",            0.62f, 0.28f,  9.0f, 0.55f, 1.00f, -1.5f, 2, false, true  },
    { "Punchy Drums",        0.45f, 0.62f,  8.0f, 0.88f, 0.80f, -1.0f, 1, false, true  },
    { "Dirty Synth",         0.70f, 0.66f, 12.0f, 0.40f, 1.00f, -2.0f, 2, false, true  },
    { "Screaming Lead",      0.85f, 0.88f, 15.0f, 0.45f, 0.90f, -3.0f, 2, true,  true  },
    { "EDM Bus",             0.50f, 0.55f,  7.0f, 0.62f, 0.90f, -1.0f, 2, false, true  },
    { "Hardstyle Kick",      0.90f, 0.45f, 18.0f, 0.75f, 1.00f, -4.0f, 2, true,  true  },
    { "Sub Thickener",       0.40f, 0.10f,  6.0f, 0.60f, 0.75f,  0.0f, 1, false, true  },
    { "Vocal Presence",      0.30f, 0.75f,  5.0f, 0.35f, 0.55f,  0.0f, 1, false, true  },
    { "Extreme Destruction", 1.00f, 0.80f, 24.0f, 0.15f, 1.00f, -6.0f, 2, true,  true  },
};
inline constexpr int numFactoryPresets = (int) std::size (factoryPresets);

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;

    const auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    const auto pctIn = [] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue() / 100.0f; };

    const auto colorText = [] (float v, int)
    {
        const int d = roundToInt (std::abs (v - 0.5f) * 200.0f);
        if (d < 2) return String ("Neutral");
        return String (v < 0.5f ? "Warm " : "Bright ") + String (d) + " %";
    };
    const auto colorIn = [] (const String& t)
    {
        const float n = t.retainCharacters ("0123456789.").getFloatValue();
        if (t.containsIgnoreCase ("warm"))    return jlimit (0.0f, 1.0f, 0.5f - n / 200.0f);
        if (t.containsIgnoreCase ("bright"))  return jlimit (0.0f, 1.0f, 0.5f + n / 200.0f);
        if (t.containsIgnoreCase ("neutral")) return 0.5f;
        return jlimit (0.0f, 1.0f, n / 100.0f);
    };
    const auto dbText = [] (float v, int) { return String (v, 1) + " dB"; };
    const auto dbIn = [] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::fatness, 1 }, "Fatness",
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f,
                Attr().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::color, 1 }, "Color",
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
                Attr().withStringFromValueFunction (colorText).withValueFromStringFunction (colorIn)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::drive, 1 }, "Drive",
                NormalisableRange<float> (0.0f, 36.0f, 0.1f), 6.0f,
                Attr().withStringFromValueFunction (dbText).withValueFromStringFunction (dbIn)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::punch, 1 }, "Punch",
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
                Attr().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::mix, 1 }, "Mix",
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
                Attr().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::output, 1 }, "Output",
                NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                Attr().withStringFromValueFunction (dbText).withValueFromStringFunction (dbIn)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::gainComp, 1 }, "Gain Comp", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::hardClip, 1 }, "Hard Clip", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::oversample, 1 }, "Oversampling",
                StringArray { "1x", "2x", "4x" }, 1));

    return layout;
}

} // namespace mg
