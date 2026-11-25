#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mg::ui
{

// Palette: cast iron, stainless steel, raw meat, bone, hazard amber.
namespace colour
{
    inline const juce::Colour iron     { 0xff1b1d21 };
    inline const juce::Colour ironLift { 0xff23262b };
    inline const juce::Colour plate    { 0xff2a2d33 };
    inline const juce::Colour groove   { 0xff0f1114 };
    inline const juce::Colour steelHi  { 0xffd3d8de };
    inline const juce::Colour steelLo  { 0xff555c66 };
    inline const juce::Colour bone     { 0xffe9e0cc };
    inline const juce::Colour boneDim  { 0xff9a9486 };
    inline const juce::Colour pink     { 0xffff6a84 };
    inline const juce::Colour crimson  { 0xffc01a3c };
    inline const juce::Colour amber    { 0xffffb23e };
}

struct Fonts
{
    static juce::Typeface::Ptr bodyFace();
    static juce::Typeface::Ptr logoFace();
    static juce::Font body (float height);
    static juce::Font logo (float height);
};

// Draws a "grinder plate": concentric rings of holes, rotated by `rotation` radians.
void drawGrinderPlate (juce::Graphics& g, juce::Point<float> centre, float radius, float rotation,
                       bool dense, juce::Colour holeColour);

class MeatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MeatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

} // namespace mg::ui
