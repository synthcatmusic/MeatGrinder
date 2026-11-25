#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MeatGrinderEngine.h"
#include "Parameters.h"

class MeatGrinderAudioProcessor : public juce::AudioProcessor
{
public:
    MeatGrinderAudioProcessor();
    ~MeatGrinderAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // Factory presets are exposed as host "programs" too.
    int getNumPrograms() override { return mg::numFactoryPresets; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    mg::MeatGrinderEngine engine;

private:
    mg::Settings readSettings() const;

    std::atomic<float>* pFatness = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pPunch = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pGainComp = nullptr;
    std::atomic<float>* pHardClip = nullptr;
    std::atomic<float>* pOversample = nullptr;

    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeatGrinderAudioProcessor)
};
