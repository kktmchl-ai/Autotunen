#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Small dark look with a teal accent.
class PitchSnapLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PitchSnapLookAndFeel();
};

//==============================================================================
// Shows the detected note, the note it is being pulled to and a cents needle.
class PitchDisplay final : public juce::Component
{
public:
    void setValues (float detectedMidi, float targetMidi);
    void paint (juce::Graphics&) override;

private:
    float detected = -1.f, target = -1.f;
    float needle = 0.f;          // smoothed deviation in cents
};

//==============================================================================
class PitchSnapEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit PitchSnapEditor (PitchSnapProcessor&);
    ~PitchSnapEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& title, const juce::String& suffix, int decimals);

    PitchSnapProcessor& proc;
    PitchSnapLookAndFeel laf;

    juce::ComboBox keyBox, scaleBox;
    juce::Label    keyLabel, scaleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttach, scaleAttach;

    PitchDisplay display;
    Knob retune, amount, mix, output;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSnapEditor)
};
