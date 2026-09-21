#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AutoTuneCore.h"

class PitchSnapProcessor final : public juce::AudioProcessor
{
public:
    PitchSnapProcessor();
    ~PitchSnapProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // GUI telemetry
    float getDetectedMidi() const noexcept { return core.detectedMidi(); }
    float getTargetMidi()   const noexcept { return core.targetMidi(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    autotune::Core core;
    int   preparedChannels = 2;
    float lastGain = 1.f;
    bool  firstBlock = true;

    std::atomic<float>* pKey     = nullptr;
    std::atomic<float>* pScale   = nullptr;
    std::atomic<float>* pRetune  = nullptr;
    std::atomic<float>* pAmount  = nullptr;
    std::atomic<float>* pMix     = nullptr;
    std::atomic<float>* pOutput  = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSnapProcessor)
};
