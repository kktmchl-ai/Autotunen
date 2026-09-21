#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      { 0xff101319 };
    const juce::Colour kPanel   { 0xff181c25 };
    const juce::Colour kAccent  { 0xff3fd0c4 };
    const juce::Colour kWarm    { 0xffffb454 };
    const juce::Colour kText    { 0xffdfe6ee };
    const juce::Colour kDim     { 0xff7d8794 };
}

//==============================================================================
PitchSnapLookAndFeel::PitchSnapLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBg);
    setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a303c));
    setColour (juce::Slider::thumbColourId, kText);
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a303c));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.darker (0.4f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

//==============================================================================
void PitchDisplay::setValues (float d, float t)
{
    detected = d;
    target   = t;

    const float dev = (d >= 0.f && t >= 0.f) ? juce::jlimit (-100.f, 100.f, (d - t) * 100.f) : 0.f;
    needle += (dev - needle) * 0.35f;
    repaint();
}

void PitchDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 10.f);

    auto area = r.reduced (16.f, 10.f);
    auto left = area.removeFromLeft (150.f);

    auto noteText = [] (float midi)
    {
        const int n = (int) std::lround (midi);
        return juce::String (autotune::noteName (n)) + juce::String (n / 12 - 1);
    };

    const bool voiced = detected >= 0.f;

    g.setColour (voiced ? kText : kDim);
    g.setFont (juce::FontOptions (46.f, juce::Font::bold));
    g.drawText (voiced ? noteText (detected) : "--", left.removeFromTop (58.f), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (13.f));
    g.setColour (kDim);
    if (voiced && target >= 0.f)
        g.drawText ("tuned to " + noteText (target), left, juce::Justification::topLeft);
    else
        g.drawText (voiced ? "" : "no pitch detected", left, juce::Justification::topLeft);

    // cents needle:  -100 ... 0 ... +100 relative to the target note
    auto bar = area.withTrimmedLeft (10.f).reduced (0.f, 14.f);
    const float cy = bar.getCentreY();
    const float x0 = bar.getX(), x1 = bar.getRight(), w = x1 - x0;

    g.setColour (juce::Colour (0xff2a303c));
    g.fillRoundedRectangle (x0, cy - 3.f, w, 6.f, 3.f);

    for (int c = -100; c <= 100; c += 50)
    {
        const float x = x0 + w * (c + 100) / 200.f;
        g.setColour (c == 0 ? kAccent : kDim.withAlpha (0.6f));
        g.fillRect (x - 1.f, cy - (c == 0 ? 16.f : 10.f), 2.f, c == 0 ? 32.f : 20.f);
    }

    g.setFont (juce::FontOptions (11.f));
    g.setColour (kDim);
    g.drawText ("flat",  juce::Rectangle<float> (x0, cy + 20.f, 40.f, 14.f),  juce::Justification::centredLeft);
    g.drawText ("sharp", juce::Rectangle<float> (x1 - 40.f, cy + 20.f, 40.f, 14.f), juce::Justification::centredRight);

    if (voiced && target >= 0.f)
    {
        const float nx = x0 + w * (needle + 100.f) / 200.f;
        const bool close = std::abs (needle) < 8.f;
        g.setColour (close ? kAccent : kWarm);
        g.fillEllipse (nx - 8.f, cy - 8.f, 16.f, 16.f);
        g.setColour (kText);
        g.setFont (juce::FontOptions (13.f, juce::Font::bold));
        g.drawText (juce::String (needle, 0) + " ct", juce::Rectangle<float> (nx - 30.f, cy - 30.f, 60.f, 14.f),
                    juce::Justification::centred);
    }
}

//==============================================================================
PitchSnapEditor::PitchSnapEditor (PitchSnapProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (620, 380);

    auto& apvts = proc.apvts;

    for (int i = 0; i < 12; ++i)                     keyBox.addItem (autotune::noteName (i), i + 1);
    for (int i = 0; i < autotune::NumScales; ++i)    scaleBox.addItem (autotune::scaleName (i), i + 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "key", keyBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "scale", scaleBox);

    for (auto* l : { &keyLabel, &scaleLabel })
    {
        l->setColour (juce::Label::textColourId, kDim);
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }
    keyLabel.setText ("KEY", juce::dontSendNotification);
    scaleLabel.setText ("SCALE", juce::dontSendNotification);

    addAndMakeVisible (display);

    setupKnob (retune, "retune", "RETUNE SPEED", " ms", 1);
    setupKnob (amount, "amount", "AMOUNT",       " %",  0);
    setupKnob (mix,    "mix",    "MIX",          " %",  0);
    setupKnob (output, "output", "OUTPUT",       " dB", 1);
    retune.slider.setTooltip ("0 ms = instant, hard-tuned robot effect. Higher = more natural.");

    startTimerHz (30);
}

PitchSnapEditor::~PitchSnapEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PitchSnapEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& title,
                                 const juce::String& suffix, int decimals)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (decimals);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void PitchSnapEditor::timerCallback()
{
    display.setValues (proc.getDetectedMidi(), proc.getTargetMidi());
}

void PitchSnapEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kText);
    g.setFont (juce::FontOptions (24.f, juce::Font::bold));
    g.drawText ("PITCHSNAP", 20, 12, 220, 32, juce::Justification::centredLeft);
    g.setColour (kAccent);
    g.fillRoundedRectangle (20.f, 44.f, 42.f, 3.f, 1.5f);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText (juce::String ("latency ") + juce::String (proc.getLatencySamples()) + " smp",
                getWidth() - 160, getHeight() - 22, 140, 14, juce::Justification::centredRight);
}

void PitchSnapEditor::resized()
{
    auto r = getLocalBounds().reduced (20);
    r.removeFromTop (34);                                     // title area

    auto row = r.removeFromTop (30);
    row.removeFromLeft (row.getWidth() - 380);
    keyLabel.setBounds   (row.removeFromLeft (40));
    keyBox.setBounds     (row.removeFromLeft (80).reduced (4, 0));
    row.removeFromLeft (10);
    scaleLabel.setBounds (row.removeFromLeft (50));
    scaleBox.setBounds   (row.reduced (4, 0));

    r.removeFromTop (12);
    display.setBounds (r.removeFromTop (118));

    r.removeFromTop (14);
    auto knobs = r.removeFromTop (150);
    const int w = knobs.getWidth() / 4;
    for (auto* k : { &retune, &amount, &mix, &output })
    {
        auto cell = knobs.removeFromLeft (w);
        k->label.setBounds  (cell.removeFromTop (18));
        k->slider.setBounds (cell.reduced (8, 0));
    }
}
