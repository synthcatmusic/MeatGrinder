// Renders the MEATGRINDER editor to a PNG, with the meters filled by real audio.
//   MeatGrinderUiShot <out.png> [preset index] [scale]
// On a headless Linux box run it under Xvfb:  xvfb-run -a ./MeatGrinderUiShot shot.png 2

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::String outPath = argc > 1 ? argv[1] : "meatgrinder.png";
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    MeatGrinderAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    proc.setCurrentProgram (preset);

    std::unique_ptr<MeatGrinderEditor> editor (dynamic_cast<MeatGrinderEditor*> (proc.createEditor()));
    editor->setSize (MeatGrinderPanel::kWidth, MeatGrinderPanel::kHeight);

    // feed the plugin a loud, bass-heavy signal and refresh the editor after each block, so the meters light up
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int frame = 0; frame < 14; ++frame)
    {
        for (int i = 0; i < 512; ++i)
        {
            phase += 2.0 * juce::MathConstants<double>::pi * 82.4 / 44100.0;
            const float s = (float) (0.55 * std::sin (phase) + 0.2 * std::sin (3.0 * phase));
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s * 0.8f);
        }
        proc.processBlock (buffer, midi);
        editor->refreshNow();
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    juce::File file (outPath);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "could not write %s\n", outPath.toRawUTF8());
        return 1;
    }
    std::printf ("wrote %s (%d x %d)\n", outPath.toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
