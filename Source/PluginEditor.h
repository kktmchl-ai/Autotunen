#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Dark, modern "HUD" look: deep navy background, halftone/speed-line accents,
// glowing blue LED indicator and blue-glow rotary knobs.
class PitchSnapLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PitchSnapLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};

//==============================================================================
// Shows the detected note as a glowing HUD badge, the note it is being pulled
// to, and a cents needle rendered as a small glow marker.
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
    void buildBackground();

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

    juce::Image backgroundImage;
    bool  ledVoiced = false;
    float ledPhase  = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSnapEditor)
};
