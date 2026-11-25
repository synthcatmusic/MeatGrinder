#include "PluginEditor.h"

using namespace juce;
namespace ui = mg::ui;

namespace
{
    constexpr int kKnobRowY = 224;                      // vertical centre of the knob row
    const Rectangle<float> kRecess { 78.0f, 90.0f, 764.0f, 262.0f };
    const Rectangle<float> kFooter { 22.0f, 372.0f, 876.0f, 108.0f };
}

// ---------------------------------------------------------------------------
MeatGrinderPanel::MeatGrinderPanel (MeatGrinderAudioProcessor& p)
    : proc (p),
      inMeter (p.engine.meters.inPeak[0], p.engine.meters.inPeak[1], p.engine.meters.inClip, "IN"),
      outMeter (p.engine.meters.outPeak[0], p.engine.meters.outPeak[1], p.engine.meters.outClip, "OUT"),
      oversampleChoice (*p.apvts.getParameter (mg::id::oversample), StringArray { "1x", "2x", "4x" }),
      clipChoice (*p.apvts.getParameter (mg::id::hardClip), StringArray { "SOFT", "HARD" })
{
    setLookAndFeel (&laf);

    setupKnob (drive,   mg::id::drive,   "DRIVE",   false);
    setupKnob (color,   mg::id::color,   "COLOR",   false);
    setupKnob (punch,   mg::id::punch,   "PUNCH",   false);
    setupKnob (fatness, mg::id::fatness, "FATNESS", true);
    setupKnob (mix,     mg::id::mix,     "MIX",     false);
    setupKnob (output,  mg::id::output,  "OUTPUT",  false);

    curveName.setText ("CURVE", dontSendNotification);
    curveName.setJustificationType (Justification::centred);
    curveName.setFont (ui::Fonts::body (19.0f));
    curveName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (curveName);
    addAndMakeVisible (curve);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // presets
    for (int i = 0; i < mg::numFactoryPresets; ++i)
        presetBox.addItem (mg::factoryPresets[i].name, i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);
    presetBox.setJustificationType (Justification::centred);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // toggles
    bypassButton.getProperties().set ("accent", (int) ui::colour::amber.getARGB());
    gainCompButton.getProperties().set ("accent", (int) ui::colour::pink.getARGB());
    bypassAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, mg::id::bypass, bypassButton);
    gainCompAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, mg::id::gainComp, gainCompButton);
    bypassButton.onStateChange = [this] { repaint(); };
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (gainCompButton);

    addAndMakeVisible (oversampleChoice);
    addAndMakeVisible (clipChoice);

    setSize (kWidth, kHeight);
}

MeatGrinderPanel::~MeatGrinderPanel()
{
    setLookAndFeel (nullptr);
}

void MeatGrinderPanel::setupKnob (KnobBlock& k, const char* paramId, const String& title, bool hero)
{
    auto& s = k.slider;
    s.getProperties().set ("hero", hero);      // must be set before the text box is created
    addAndMakeVisible (s);                     // parent first, so the text box is built by our look-and-feel
    s.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, hero ? 160 : 84, hero ? 26 : 20);
    s.setMouseDragSensitivity (hero ? 220 : 170);

    k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    if (auto* prm = proc.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));

    k.name.setText (title, dontSendNotification);
    k.name.setJustificationType (Justification::centred);
    k.name.setFont (ui::Fonts::body (hero ? 30.0f : 19.0f));
    k.name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.name);
}

void MeatGrinderPanel::stepPreset (int delta)
{
    const int n = mg::numFactoryPresets;
    const int idx = (proc.getCurrentProgram() + delta + n) % n;
    proc.setCurrentProgram (idx);
    presetBox.setSelectedItemIndex (idx, dontSendNotification);
}

// ---------------------------------------------------------------------------
void MeatGrinderPanel::resized()
{
    // header
    prevButton.setBounds (506, 30, 22, 30);
    presetBox.setBounds (532, 28, 240, 34);
    nextButton.setBounds (776, 30, 22, 30);
    bypassButton.setBounds (812, 28, 92, 34);

    // meters
    inMeter.setBounds (24, 100, 46, 250);
    outMeter.setBounds (850, 100, 46, 250);

    // knob row
    constexpr int smallW = 82, smallH = 98;
    auto small = [&] (KnobBlock& k, int x)
    {
        k.slider.setBounds (x, kKnobRowY - 39, smallW, smallH);
        k.name.setBounds (x, kKnobRowY - 39 - 26, smallW, 24);
    };
    small (drive, 82);
    small (color, 172);
    small (punch, 262);
    small (mix, 666);
    small (output, 756);

    fatness.slider.setBounds (360, kKnobRowY - 96, 200, 218);
    fatness.name.setBounds (360, kKnobRowY - 96 - 34, 200, 32);

    curve.setBounds (579, kKnobRowY - 38, 76, 76);
    curveName.setBounds (576, kKnobRowY - 39 - 26, smallW, 24);

    // footer
    oversampleChoice.setBounds (44, 404, 206, 34);
    clipChoice.setBounds (290, 404, 150, 34);
    gainCompButton.setBounds (480, 404, 140, 34);
}

// ---------------------------------------------------------------------------
void MeatGrinderPanel::paint (Graphics& g)
{
    using namespace ui;
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (ColourGradient (colour::ironLift, 0.0f, 0.0f, colour::iron, 0.0f, bounds.getHeight(), false));
    g.fillAll();

    g.setColour (Colours::white.withAlpha (0.016f));                 // brushed-metal grain
    for (int y = 0; y < kHeight; y += 3)
        g.fillRect (0, y, kWidth, 1);

    // header
    g.setColour (colour::groove.withAlpha (0.55f));
    g.fillRect (0, 0, kWidth, 80);
    g.setGradientFill (ColourGradient (colour::crimson, 0.0f, 80.0f, colour::pink, (float) kWidth, 80.0f, false));
    g.fillRect (0, 79, kWidth, 2);

    const Point<float> mc (50.0f, 40.0f);                            // logo mark: a grinder plate
    g.setGradientFill (ColourGradient (colour::pink, mc.x - 20.0f, mc.y - 24.0f, colour::crimson, mc.x + 18.0f, mc.y + 24.0f, false));
    g.fillEllipse (mc.x - 26.0f, mc.y - 26.0f, 52.0f, 52.0f);
    drawGrinderPlate (g, mc, 24.0f, 0.0f, false, colour::iron);

    g.setColour (colour::bone);
    g.setFont (Fonts::logo (50.0f));
    g.drawText ("MEATGRINDER", 92, 6, 400, 52, Justification::centredLeft, false);
    g.setColour (colour::boneDim);
    g.setFont (Fonts::body (18.0f));
    g.drawText ("Analog Saturation & Harmonic Enhancer", 94, 54, 400, 20, Justification::centredLeft, false);

    // recessed panels
    g.setColour (colour::groove.withAlpha (0.42f));
    g.fillRoundedRectangle (kRecess, 10.0f);
    g.setColour (colour::plate.brighter (0.1f));
    g.drawRoundedRectangle (kRecess, 10.0f, 1.2f);

    g.setColour (colour::groove.withAlpha (0.36f));
    g.fillRoundedRectangle (kFooter, 10.0f);
    g.setColour (colour::plate.brighter (0.1f));
    g.drawRoundedRectangle (kFooter, 10.0f, 1.2f);

    // footer captions
    g.setFont (Fonts::body (18.0f));
    g.setColour (colour::bone);
    g.drawText ("Oversampling", 44, 380, 206, 22, Justification::centredLeft, false);
    g.drawText ("Clipping", 290, 380, 150, 22, Justification::centredLeft, false);
    g.drawText ("Loudness", 480, 380, 150, 22, Justification::centredLeft, false);

    g.setFont (Fonts::body (16.0f));
    g.setColour (colour::boneDim);
    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const int lat = proc.getLatencySamples();
    g.drawText (lat == 0 ? String ("Zero latency")
                         : String::formatted ("Latency %d samples (%.1f ms)", lat, 1000.0 * lat / sr),
                44, 444, 230, 20, Justification::centredLeft, false);
    g.drawText (rawParam (mg::id::hardClip) > 0.5f ? "Flat-top clipper, ceiling -1.9 dBFS" : "Soft-knee ceiling at -1.9 dBFS",
                290, 444, 190, 20, Justification::centredLeft, false);
    g.drawText ("Output RMS follows the input", 480, 444, 190, 20, Justification::centredLeft, false);

    g.drawText ("Double-click a knob to reset it", 676, 380, 200, 20, Justification::centredRight, false);
    g.drawText ("Click a clip lamp to clear it", 676, 402, 200, 20, Justification::centredRight, false);
    g.setColour (colour::boneDim.withAlpha (0.7f));
    g.drawText ("MEATGRINDER 1.0", 676, 452, 200, 20, Justification::centredRight, false);
}

void MeatGrinderPanel::paintOverChildren (Graphics& g)
{
    if (bypassButton.getToggleState())
    {
        g.setColour (ui::colour::iron.withAlpha (0.58f));
        g.fillRoundedRectangle (kRecess, 10.0f);
    }
}

// ---------------------------------------------------------------------------
void MeatGrinderPanel::tick()
{
    inMeter.pull();
    outMeter.pull();

    const bool hard = rawParam (mg::id::hardClip) > 0.5f;
    curve.setParams (rawParam (mg::id::fatness), rawParam (mg::id::color), rawParam (mg::id::drive), hard);

    const int lat = proc.getLatencySamples();
    if (lat != shownLatency || hard != shownHard)
    {
        shownLatency = lat;
        shownHard = hard;
        repaint (kFooter.toNearestInt());
    }

    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);
}

// ---------------------------------------------------------------------------
MeatGrinderEditor::MeatGrinderEditor (MeatGrinderAudioProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (MeatGrinderPanel::kWidth * 3 / 4, MeatGrinderPanel::kHeight * 3 / 4,
                     MeatGrinderPanel::kWidth * 2, MeatGrinderPanel::kHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) MeatGrinderPanel::kWidth / (double) MeatGrinderPanel::kHeight);
    setSize (MeatGrinderPanel::kWidth, MeatGrinderPanel::kHeight);
    startTimerHz (30);
}

MeatGrinderEditor::~MeatGrinderEditor()
{
    stopTimer();
}

void MeatGrinderEditor::resized()
{
    panel.setBounds (0, 0, MeatGrinderPanel::kWidth, MeatGrinderPanel::kHeight);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) MeatGrinderPanel::kWidth));
}
