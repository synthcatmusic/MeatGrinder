#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "MeatLookAndFeel.h"
#include "PluginProcessor.h"
#include "UIComponents.h"

// The whole interface lives in a fixed 920 x 500 panel that the editor scales to the window size.
class MeatGrinderPanel : public juce::Component
{
public:
    static constexpr int kWidth = 920, kHeight = 500;

    explicit MeatGrinderPanel (MeatGrinderAudioProcessor&);
    ~MeatGrinderPanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void tick();   // ~30 Hz from the editor: meters, curve, latency read-out

private:
    struct KnobBlock
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label name;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (KnobBlock&, const char* paramId, const juce::String& title, bool hero);
    void stepPreset (int delta);
    float rawParam (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    MeatGrinderAudioProcessor& proc;
    mg::ui::MeatLookAndFeel laf;   // declared first: outlives every child component

    KnobBlock drive, color, punch, fatness, mix, output;
    mg::ui::CurveView curve;
    juce::Label curveName;

    mg::ui::MeterComponent inMeter, outMeter;

    juce::ComboBox presetBox;
    juce::ArrowButton prevButton { "prev", 0.5f, mg::ui::colour::bone };
    juce::ArrowButton nextButton { "next", 0.0f, mg::ui::colour::bone };

    juce::ToggleButton bypassButton { "BYPASS" }, gainCompButton { "GAIN COMP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, gainCompAttachment;

    mg::ui::SegmentedChoice oversampleChoice, clipChoice;

    int shownLatency = -1;
    bool shownHard = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeatGrinderPanel)
};

class MeatGrinderEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeatGrinderEditor (MeatGrinderAudioProcessor&);
    ~MeatGrinderEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // Pulls meters / curve / read-outs immediately (normally done by the 30 Hz timer).
    void refreshNow() { panel.tick(); }

private:
    void timerCallback() override { panel.tick(); }

    MeatGrinderPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeatGrinderEditor)
};
