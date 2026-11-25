#include "PluginProcessor.h"
#include "PluginEditor.h"

MeatGrinderAudioProcessor::MeatGrinderAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MEATGRINDER", mg::createParameterLayout())
{
    pFatness    = apvts.getRawParameterValue (mg::id::fatness);
    pColor      = apvts.getRawParameterValue (mg::id::color);
    pDrive      = apvts.getRawParameterValue (mg::id::drive);
    pPunch      = apvts.getRawParameterValue (mg::id::punch);
    pMix        = apvts.getRawParameterValue (mg::id::mix);
    pOutput     = apvts.getRawParameterValue (mg::id::output);
    pBypass     = apvts.getRawParameterValue (mg::id::bypass);
    pGainComp   = apvts.getRawParameterValue (mg::id::gainComp);
    pHardClip   = apvts.getRawParameterValue (mg::id::hardClip);
    pOversample = apvts.getRawParameterValue (mg::id::oversample);
}

juce::AudioProcessorParameter* MeatGrinderAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (mg::id::bypass);
}

bool MeatGrinderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == out;
}

mg::Settings MeatGrinderAudioProcessor::readSettings() const
{
    mg::Settings s;
    s.fatness      = pFatness->load();
    s.color        = pColor->load();
    s.driveDb      = pDrive->load();
    s.punch        = pPunch->load();
    s.mix          = pMix->load();
    s.outputDb     = pOutput->load();
    s.bypass       = pBypass->load() > 0.5f;
    s.gainComp     = pGainComp->load() > 0.5f;
    s.hardClip     = pHardClip->load() > 0.5f;
    s.oversampling = juce::jlimit (0, 2, (int) std::lround (pOversample->load()));
    return s;
}

void MeatGrinderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setOversamplingMode (readSettings().oversampling);   // so the reported latency is right from the start
    engine.reset();
    setLatencySamples (engine.getLatencySamples());
}

void MeatGrinderAudioProcessor::reset()
{
    engine.reset();
}

void MeatGrinderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.process (buffer, readSettings());

    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples (engine.getLatencySamples());
}

juce::AudioProcessorEditor* MeatGrinderAudioProcessor::createEditor()
{
    return new MeatGrinderEditor (*this);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
const juce::String MeatGrinderAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, mg::numFactoryPresets) ? juce::String (mg::factoryPresets[index].name)
                                                                    : juce::String();
}

void MeatGrinderAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, mg::numFactoryPresets))
        return;

    currentPreset = index;
    const auto& p = mg::factoryPresets[index];

    auto set = [this] (const char* paramId, float realValue)
    {
        if (auto* prm = apvts.getParameter (paramId))
            prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
    };

    set (mg::id::fatness, p.fatness);
    set (mg::id::color, p.color);
    set (mg::id::drive, p.drive);
    set (mg::id::punch, p.punch);
    set (mg::id::mix, p.mix);
    set (mg::id::output, p.output);
    set (mg::id::oversample, (float) p.oversampling);
    set (mg::id::hardClip, p.hardClip ? 1.0f : 0.0f);
    set (mg::id::gainComp, p.gainComp ? 1.0f : 0.0f);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void MeatGrinderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MeatGrinderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, mg::numFactoryPresets - 1, (int) tree.getProperty ("preset", 0));
            apvts.replaceState (tree);

            // Make the restore exact for every parameter.  A host may leave a boolean / choice parameter at an
            // off-grid value (e.g. 0.15); the saved state holds the snapped value, so JUCE would see "no change"
            // and keep the stale raw value.  Re-applying each stored value explicitly fixes that.
            for (auto* prm : getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    const auto node = tree.getChildWithProperty ("id", ranged->paramID);
                    if (node.isValid())
                    {
                        const float stored = (float) node.getProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()));
                        ranged->setValueNotifyingHost (ranged->convertTo0to1 (stored));
                    }
                }
        }
}

// Required by the plugin client
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeatGrinderAudioProcessor();
}
