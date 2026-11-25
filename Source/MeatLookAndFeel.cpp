#include "MeatLookAndFeel.h"

#include <BinaryData.h>

namespace mg::ui
{

using namespace juce;

// ---------------------------------------------------------------------------
// Fonts (Barlow Condensed, SIL Open Font License - see Assets/Fonts/OFL.txt)
// ---------------------------------------------------------------------------
Typeface::Ptr Fonts::bodyFace()
{
    static Typeface::Ptr face = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf,
                                                                   (size_t) BinaryData::BarlowCondensedSemiBold_ttfSize);
    return face;
}

Typeface::Ptr Fonts::logoFace()
{
    static Typeface::Ptr face = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedExtraBold_ttf,
                                                                   (size_t) BinaryData::BarlowCondensedExtraBold_ttfSize);
    return face;
}

Font Fonts::body (float height)
{
    if (auto f = bodyFace())
        return Font (FontOptions (f).withHeight (height));
    return Font (FontOptions (height));
}

Font Fonts::logo (float height)
{
    if (auto f = logoFace())
        return Font (FontOptions (f).withHeight (height));
    return Font (FontOptions (height, Font::bold));
}

// ---------------------------------------------------------------------------
void drawGrinderPlate (Graphics& g, Point<float> c, float radius, float rotation, bool dense, Colour holeColour)
{
    struct Ring { float r; int n; };
    static constexpr Ring denseRings[] = { { 0.0f, 1 }, { 0.28f, 6 }, { 0.50f, 12 }, { 0.70f, 18 } };
    static constexpr Ring sparseRings[] = { { 0.0f, 1 }, { 0.36f, 6 }, { 0.66f, 12 } };

    const float holeR = radius * (dense ? 0.062f : 0.085f);

    auto drawRing = [&] (const Ring& ring)
    {
        for (int j = 0; j < ring.n; ++j)
        {
            const float phi = (ring.n == 1 ? 0.0f : (float) j * MathConstants<float>::twoPi / (float) ring.n) + rotation;
            const float px = c.x + ring.r * radius * std::sin (phi);
            const float py = c.y - ring.r * radius * std::cos (phi);

            g.setColour (Colours::white.withAlpha (0.20f));            // lower lip catches light
            g.fillEllipse (px - holeR, py - holeR + 0.9f, holeR * 2.0f, holeR * 2.0f);
            g.setColour (holeColour);
            g.fillEllipse (px - holeR, py - holeR, holeR * 2.0f, holeR * 2.0f);
        }
    };

    if (dense) for (auto& r : denseRings) drawRing (r);
    else       for (auto& r : sparseRings) drawRing (r);
}

// ---------------------------------------------------------------------------
MeatLookAndFeel::MeatLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, colour::bone);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, colour::crimson);
    setColour (Label::textColourId, colour::bone);
    setColour (TextEditor::backgroundColourId, colour::groove);
    setColour (TextEditor::textColourId, colour::bone);
    setColour (TextEditor::highlightColourId, colour::crimson);
    setColour (TextEditor::outlineColourId, colour::crimson);
    setColour (TextEditor::focusedOutlineColourId, colour::pink);
    setColour (CaretComponent::caretColourId, colour::pink);

    setColour (ComboBox::textColourId, colour::bone);
    setColour (PopupMenu::backgroundColourId, colour::plate);
    setColour (PopupMenu::textColourId, colour::bone);
    setColour (PopupMenu::highlightedBackgroundColourId, colour::crimson);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
}

Font MeatLookAndFeel::getComboBoxFont (ComboBox&)   { return Fonts::body (20.0f); }
Font MeatLookAndFeel::getPopupMenuFont()            { return Fonts::body (18.0f); }
Font MeatLookAndFeel::getLabelFont (Label& l)       { return Fonts::body (l.getFont().getHeight()); }

Label* MeatLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool hero = (bool) s.getProperties().getWithDefault ("hero", false);
    l->setFont (Fonts::body (hero ? 22.0f : 16.0f));
    l->setJustificationType (Justification::centred);
    return l;
}

// ---------------------------------------------------------------------------
void MeatLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, Slider& s)
{
    const bool hero = (bool) s.getProperties().getWithDefault ("hero", false);
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R = jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
    const auto c = area.getCentre();
    const float angle = a0 + pos * (a1 - a0);

    auto pt = [&] (float r, float a) { return Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };

    // glow behind the ring - only on the hero knob, grows with the value
    if (hero && pos > 0.02f)
    {
        ColourGradient glow (colour::crimson.withAlpha (0.34f * pos), c.x, c.y,
                             Colours::transparentBlack, c.x, c.y - R * 1.08f, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - R * 1.08f, c.y - R * 1.08f, R * 2.16f, R * 2.16f);
    }

    // LED tick ring
    const int nTicks = hero ? 45 : 29;
    const float ringIn = R - (hero ? 11.0f : 7.5f);
    const int lastLit = roundToInt (pos * (float) (nTicks - 1));
    for (int i = 0; i < nTicks; ++i)
    {
        const float t = (float) i / (float) (nTicks - 1);
        const float a = a0 + t * (a1 - a0);
        g.setColour (i <= lastLit ? colour::pink.interpolatedWith (colour::crimson, t)
                                  : colour::groove.brighter (0.28f));
        g.drawLine (Line<float> (pt (ringIn, a), pt (R, a)), hero ? 3.2f : 2.4f);
    }

    // stainless cap
    const float capR = ringIn - (hero ? 9.0f : 5.5f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - capR - 1.0f, c.y - capR + 3.0f, (capR + 1.0f) * 2.0f, (capR + 1.0f) * 2.0f);

    ColourGradient body (colour::steelHi, c.x - capR * 0.5f, c.y - capR * 0.6f,
                         colour::steelLo, c.x + capR * 0.9f, c.y + capR * 1.0f, true);
    body.addColour (0.55, colour::steelHi.interpolatedWith (colour::steelLo, 0.55f));
    g.setGradientFill (body);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.4f);
    g.setColour (Colours::white.withAlpha (0.28f));
    g.drawEllipse (c.x - capR + 1.8f, c.y - capR + 1.8f, (capR - 1.8f) * 2.0f, (capR - 1.8f) * 2.0f, 1.0f);

    // knurled rim (hero)
    if (hero)
    {
        g.setColour (Colours::black.withAlpha (0.28f));
        for (int i = 0; i < 72; ++i)
        {
            const float a = angle + (float) i * MathConstants<float>::twoPi / 72.0f;
            g.drawLine (Line<float> (pt (capR - 4.2f, a), pt (capR - 1.6f, a)), 1.0f);
        }
    }

    // the plate turns with the knob
    drawGrinderPlate (g, c, capR, angle - a0, hero, colour::groove);

    // pointer notch
    Path notch;
    notch.startNewSubPath (pt (capR * 0.83f, angle));
    notch.lineTo (pt (capR * 0.96f, angle));
    g.setColour (colour::bone);
    g.strokePath (notch, PathStrokeType (hero ? 3.6f : 2.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ---------------------------------------------------------------------------
void MeatLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const Colour accent ((uint32) (int) b.getProperties().getWithDefault ("accent", (int) colour::pink.getARGB()));

    g.setColour (on ? accent.withAlpha (0.16f) : colour::plate.brighter (highlighted ? 0.12f : 0.0f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? accent : colour::groove.brighter (0.2f));
    g.drawRoundedRectangle (r, 5.0f, 1.5f);

    const float ledR = 5.0f;
    const float ledX = r.getX() + 16.0f, ledY = r.getCentreY();
    if (on)
    {
        g.setColour (accent.withAlpha (0.28f));
        g.fillEllipse (ledX - ledR * 2.0f, ledY - ledR * 2.0f, ledR * 4.0f, ledR * 4.0f);
    }
    g.setColour (on ? accent : colour::groove);
    g.fillEllipse (ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f, 1.0f);

    g.setColour (on ? colour::bone : colour::boneDim);
    g.setFont (Fonts::body (18.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (32.0f).toNearestInt(), Justification::centredLeft, false);
}

// ---------------------------------------------------------------------------
void MeatLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox&)
{
    const auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (colour::plate.brighter (0.08f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colour::groove.brighter (0.2f));
    g.drawRoundedRectangle (r, 5.0f, 1.5f);

    Path chevron;
    const float cx = (float) width - 18.0f, cy = (float) height * 0.5f;
    chevron.startNewSubPath (cx - 5.0f, cy - 2.5f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 5.0f, cy - 2.5f);
    g.setColour (colour::bone);
    g.strokePath (chevron, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

} // namespace mg::ui
