#include "PluginEditor.h"

//==============================================================================
// Palette: warm washi paper, ink-brown bark, sakura pink, vermillion stamp red.
namespace
{
    const juce::Colour kPaper      { 0xfff6ecdc };   // background paper
    const juce::Colour kPaperEdge  { 0xffe7d7bc };   // knob track / card border tint
    const juce::Colour kPanel      { 0xfffbf5e9 };   // display card
    const juce::Colour kBark       { 0xff3e2c22 };   // branch / outlines
    const juce::Colour kBarkSoft   { 0xff8a6a52 };   // dim ink for secondary text
    const juce::Colour kBlossom    { 0xffefb0c4 };   // petal light
    const juce::Colour kBlossomDeep{ 0xffdb84a2 };   // petal shade
    const juce::Colour kSun        { 0xffc23a2e };   // vermillion stamp / sun
    const juce::Colour kSunDeep    { 0xff8c2318 };
    const juce::Colour kText       { 0xff2c2018 };   // ink text
    const juce::Colour kTextDim    { 0xff8a7360 };

    // A simple five-petal blossom, drawn as overlapping teardrop petals around (c).
    void drawBlossom (juce::Graphics& g, juce::Point<float> c, float r,
                      juce::Colour petal, juce::Colour centre, float rot = 0.f, float alpha = 1.f)
    {
        for (int i = 0; i < 5; ++i)
        {
            juce::Path p;
            p.startNewSubPath (0.f, 0.f);
            p.quadraticTo (r * 0.55f, -r * 0.35f, 0.f, -r);
            p.quadraticTo (-r * 0.55f, -r * 0.35f, 0.f, 0.f);
            p.closeSubPath();
            const auto t = juce::AffineTransform::rotation (rot + (float) i * juce::MathConstants<float>::twoPi / 5.f)
                                .translated (c);
            g.setColour (petal.withMultipliedAlpha (alpha));
            g.fillPath (p, t);
        }
        g.setColour (centre.withMultipliedAlpha (alpha));
        g.fillEllipse (c.x - r * 0.18f, c.y - r * 0.18f, r * 0.36f, r * 0.36f);
    }

    void drawBranch (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to,
                     float thickness, juce::Colour c, float bow = -10.f)
    {
        juce::Path p;
        p.startNewSubPath (from);
        const auto mid = from + (to - from) * 0.5f + juce::Point<float> (bow, bow * -0.4f);
        p.quadraticTo (mid, to);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A small corner sprig: a couple of curved twigs with blossom clusters.
    void drawSprig (juce::Graphics& g, juce::Point<float> root, float scale, float mirror)
    {
        const auto p1 = root + juce::Point<float> (-70.f * mirror, 44.f) * scale;
        const auto p2 = root + juce::Point<float> (-30.f * mirror, 18.f) * scale;
        drawBranch (g, root, p1, 2.6f * scale, kBark, 14.f * mirror);
        drawBranch (g, p2, root + juce::Point<float> (-95.f * mirror, 8.f) * scale, 1.6f * scale, kBark, 8.f * mirror);

        struct B { juce::Point<float> pt; float r; juce::Colour c; float rot; };
        const B blossoms[] = {
            { root + juce::Point<float> (-72.f * mirror, 46.f) * scale, 12.f * scale, kBlossom,     0.3f },
            { root + juce::Point<float> (-48.f * mirror, 30.f) * scale, 9.f  * scale, kBlossomDeep, 1.1f },
            { root + juce::Point<float> (-96.f * mirror, 22.f) * scale, 8.f  * scale, kBlossom,     2.0f },
            { root + juce::Point<float> (-20.f * mirror, 10.f) * scale, 7.f  * scale, kBlossomDeep, 0.7f },
            { root + juce::Point<float> (-112.f * mirror, 4.f) * scale, 6.f  * scale, kBlossom,     1.6f },
        };
        for (auto& b : blossoms) drawBlossom (g, b.pt, b.r, b.c, kSun.brighter (0.3f), b.rot);
    }
}

//==============================================================================
PitchSnapLookAndFeel::PitchSnapLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kPaper);

    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::Label::textColourId, kText);

    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::outlineColourId, kBark.withAlpha (0.35f));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kSun);

    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kSun.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void PitchSnapLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                             float pos, float rotaryStartAngle, float rotaryEndAngle,
                                             juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.f;
    const auto  centre = bounds.getCentre();
    const float angle  = rotaryStartAngle + pos * (rotaryEndAngle - rotaryStartAngle);
    const float thickness = radius * 0.20f;
    const float arcR = radius - thickness * 0.5f - 2.f;

    // paper disc behind the arc
    g.setColour (kPaperEdge.withAlpha (0.55f));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (kPaperEdge.darker (0.06f));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: pink to vermillion
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, rotaryStartAngle, angle, true);
    juce::ColourGradient grad (kBlossom, centre.x, centre.y - arcR, kSun, centre.x, centre.y + arcR, false);
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ink outline
    g.setColour (kBark.withAlpha (0.30f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f, 1.2f);

    // thumb: a tiny blossom at the tip
    const float tipR = radius - thickness * 0.3f;
    const juce::Point<float> tip (centre.x + tipR * std::cos (angle - juce::MathConstants<float>::halfPi),
                                  centre.y + tipR * std::sin (angle - juce::MathConstants<float>::halfPi));
    drawBlossom (g, tip, radius * 0.20f, kBlossomDeep, juce::Colours::white);
}

void PitchSnapLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.f, 0.f, (float) width, (float) height).reduced (0.5f);
    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 6.f, 1.1f);

    const auto arrowZone = r.removeFromRight (22.f);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getCentreX() - 5.f, arrowZone.getCentreY() - 3.f,
                       arrowZone.getCentreX() + 5.f, arrowZone.getCentreY() - 3.f,
                       arrowZone.getCentreX(),        arrowZone.getCentreY() + 4.f);
    g.setColour (box.isPopupActive() ? kSunDeep : kSun);
    g.fillPath (arrow);
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
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (kBark.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.2f);

    auto area = r.reduced (16.f, 12.f);
    auto left = area.removeFromLeft (185.f);

    auto noteText = [] (float midi)
    {
        const int n = (int) std::lround (midi);
        return juce::String (autotune::noteName (n)) + juce::String (n / 12 - 1);
    };

    const bool voiced = detected >= 0.f;
    const float stampD = 58.f;
    const auto stamp = juce::Rectangle<float> (left.getX(), left.getY() + 2.f, stampD, stampD);

    if (voiced)
    {
        juce::ColourGradient sg (kSun, stamp.getCentreX(), stamp.getY(),
                                 kSunDeep, stamp.getCentreX(), stamp.getBottom(), false);
        g.setGradientFill (sg);
        g.fillEllipse (stamp);
        g.setColour (juce::Colours::white);
    }
    else
    {
        g.setColour (kBarkSoft.withAlpha (0.35f));
        g.drawEllipse (stamp, 1.6f);
        g.setColour (kBarkSoft);
    }

    g.setFont (juce::FontOptions (23.f, juce::Font::bold));
    g.drawText (voiced ? noteText (detected) : "--", stamp, juce::Justification::centred);

    auto info = left.withTrimmedLeft (stampD + 12.f);
    g.setFont (juce::FontOptions (11.f, juce::Font::bold));
    g.setColour (kTextDim);
    g.drawText ("DETECTED", info.removeFromTop (16.f), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.f));
    g.setColour (voiced ? kText : kTextDim);
    g.drawText (voiced && target >= 0.f ? "tuned to " + noteText (target)
                                        : (voiced ? "" : "no signal"),
               info, juce::Justification::centredLeft);

    // cents needle
    auto bar = area.withTrimmedLeft (10.f).reduced (0.f, 16.f);
    const float cy = bar.getCentreY();
    const float x0 = bar.getX(), x1 = bar.getRight(), w = x1 - x0;

    g.setColour (kPaperEdge);
    g.fillRoundedRectangle (x0, cy - 3.f, w, 6.f, 3.f);

    for (int c = -100; c <= 100; c += 50)
    {
        const float x = x0 + w * (c + 100) / 200.f;
        g.setColour (c == 0 ? kSun : kBarkSoft.withAlpha (0.45f));
        g.fillRect (x - 1.f, cy - (c == 0 ? 16.f : 10.f), 2.f, c == 0 ? 32.f : 20.f);
    }

    g.setFont (juce::FontOptions (11.f));
    g.setColour (kTextDim);
    g.drawText ("flat",  juce::Rectangle<float> (x0, cy + 20.f, 40.f, 14.f),  juce::Justification::centredLeft);
    g.drawText ("sharp", juce::Rectangle<float> (x1 - 40.f, cy + 20.f, 40.f, 14.f), juce::Justification::centredRight);

    if (voiced && target >= 0.f)
    {
        const float nx = x0 + w * (needle + 100.f) / 200.f;
        const bool close = std::abs (needle) < 8.f;
        drawBlossom (g, { nx, cy }, 9.f, close ? kBlossom : kSun, close ? kSunDeep : kBark, needle * 0.01f);

        g.setColour (kText);
        g.setFont (juce::FontOptions (12.f, juce::Font::bold));
        g.drawText (juce::String (needle, 0) + " ct", juce::Rectangle<float> (nx - 30.f, cy - 30.f, 60.f, 14.f),
                   juce::Justification::centred);
    }
}

//==============================================================================
PitchSnapEditor::PitchSnapEditor (PitchSnapProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (620, 400);

    auto& apvts = proc.apvts;

    for (int i = 0; i < 12; ++i)                     keyBox.addItem (autotune::noteName (i), i + 1);
    for (int i = 0; i < autotune::NumScales; ++i)    scaleBox.addItem (autotune::scaleName (i), i + 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "key", keyBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "scale", scaleBox);

    for (auto* l : { &keyLabel, &scaleLabel })
    {
        l->setColour (juce::Label::textColourId, kTextDim);
        l->setJustificationType (juce::Justification::centredRight);
        l->setFont (juce::FontOptions (12.f, juce::Font::bold));
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
    k.label.setColour (juce::Label::textColourId, kTextDim);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void PitchSnapEditor::timerCallback()
{
    display.setValues (proc.getDetectedMidi(), proc.getTargetMidi());
}

void PitchSnapEditor::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (kPaper);

    // soft vermillion sun, mostly cropped above the top edge
    {
        juce::ColourGradient grad (kSun.withAlpha (0.45f), w * 0.5f, -100.f, kSun.withAlpha (0.f), w * 0.5f, 260.f, true);
        g.setGradientFill (grad);
        g.fillEllipse (w * 0.5f - 230.f, -300.f, 460.f, 460.f);
    }

    // faint distant mountains along the bottom, behind the controls
    {
        juce::Path mtn;
        mtn.startNewSubPath (0.f, h);
        mtn.lineTo (0.f, h - 12.f);
        mtn.lineTo (w * 0.16f, h - 30.f);
        mtn.lineTo (w * 0.30f, h - 14.f);
        mtn.lineTo (w * 0.50f, h - 42.f);
        mtn.lineTo (w * 0.68f, h - 16.f);
        mtn.lineTo (w * 0.82f, h - 28.f);
        mtn.lineTo (w, h - 10.f);
        mtn.lineTo (w, h);
        mtn.closeSubPath();
        g.setColour (kBark.withAlpha (0.07f));
        g.fillPath (mtn);
    }

    // cherry sprig, top-right corner
    drawSprig (g, { w - 14.f, 4.f }, 1.0f, 1.0f);

    // a couple of drifting petals near the bottom corners
    drawBlossom (g, { 26.f, h - 22.f }, 6.f, kBlossom, kSunDeep, 0.4f, 0.55f);
    drawBlossom (g, { w - 30.f, h - 34.f }, 5.f, kBlossomDeep, kSunDeep, 1.2f, 0.45f);

    // title
    g.setColour (kText);
    g.setFont (juce::FontOptions (27.f, juce::Font::bold));
    g.drawText ("PitchSnap", 20, 10, 260, 36, juce::Justification::centredLeft);

    juce::Path underline;
    underline.startNewSubPath (21.f, 47.f);
    underline.lineTo (148.f, 47.f);
    g.setColour (kSun.withAlpha (0.75f));
    g.strokePath (underline, juce::PathStrokeType (2.4f));
    drawBlossom (g, { 154.f, 47.f }, 6.f, kBlossom, kSunDeep);

    g.setColour (kTextDim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText (juce::String ("latency ") + juce::String (proc.getLatencySamples()) + " smp",
               getWidth() - 160, getHeight() - 22, 140, 14, juce::Justification::centredRight);
}

void PitchSnapEditor::resized()
{
    auto r = getLocalBounds().reduced (20);
    r.removeFromTop (34);

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
