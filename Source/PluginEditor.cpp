#include "PluginEditor.h"

//==============================================================================
// Palette: deep navy HUD background, electric blue glow, warm amber warning accent.
namespace
{
    const juce::Colour kBg        { 0xff0a1120 };   // background gradient top
    const juce::Colour kBgSoft    { 0xff16233b };   // background gradient bottom
    const juce::Colour kInk       { 0xff060a13 };   // deep shadow / panel accents
    const juce::Colour kPanel     { 0xff121b30 };   // display / chip background
    const juce::Colour kPanelEdge { 0xff2b3a56 };   // panel / chip border
    const juce::Colour kTrack     { 0xff222e46 };   // knob track
    const juce::Colour kText      { 0xffeaf1fb };   // primary text
    const juce::Colour kTextDim   { 0xff7d8ba3 };   // secondary text
    const juce::Colour kBlue      { 0xff2f8fff };   // electric blue
    const juce::Colour kBlueDeep  { 0xff1652c4 };   // deep blue (gradient end)
    const juce::Colour kBlueGlow  { 0xff8fcaff };   // bright glow / highlight
    const juce::Colour kWarn      { 0xffffa23f };   // amber, used only for "off pitch"

    // A soft glowing dot: halo + solid core + bright highlight. Used for the LED,
    // the knob thumb and the cents-needle marker so the HUD motif stays consistent.
    void drawGlowDot (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float glowAlpha = 0.55f)
    {
        juce::ColourGradient halo (colour.withAlpha (glowAlpha), c.x, c.y,
                                   colour.withAlpha (0.f), c.x + r * 2.6f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (c.x - r * 2.6f, c.y - r * 2.6f, r * 5.2f, r * 5.2f);

        g.setColour (colour);
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);

        const float hi = r * 0.42f;
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (c.x - hi, c.y - hi, hi * 2.f, hi * 2.f);
    }
}

//==============================================================================
PitchSnapLookAndFeel::PitchSnapLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBg);

    setColour (juce::Slider::textBoxTextColourId, kBlueGlow);
    setColour (juce::Slider::textBoxOutlineColourId, kPanelEdge);
    setColour (juce::Slider::textBoxBackgroundColourId, kPanel);

    setColour (juce::Label::textColourId, kText);

    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::outlineColourId, kPanelEdge);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kBlue);

    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kBlue.withAlpha (0.85f));
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

    // recessed disc
    g.setColour (kInk.withAlpha (0.55f));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: blue glow gradient
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, rotaryStartAngle, angle, true);
    juce::ColourGradient grad (kBlueGlow, centre.x, centre.y - arcR, kBlueDeep, centre.x, centre.y + arcR, false);
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (kPanelEdge);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f, 1.2f);

    // thumb: a small glowing dot at the tip
    const float tipR = radius - thickness * 0.3f;
    const juce::Point<float> tip (centre.x + tipR * std::cos (angle - juce::MathConstants<float>::halfPi),
                                  centre.y + tipR * std::sin (angle - juce::MathConstants<float>::halfPi));
    drawGlowDot (g, tip, radius * 0.14f, kBlueGlow, 0.7f);
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
    g.setColour (box.isPopupActive() ? kBlueGlow : kBlue);
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
    g.setColour (kPanelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.2f);

    auto area = r.reduced (16.f, 12.f);
    auto left = area.removeFromLeft (185.f);

    auto noteText = [] (float midi)
    {
        const int n = (int) std::lround (midi);
        return juce::String (autotune::noteName (n)) + juce::String (n / 12 - 1);
    };

    const bool voiced = detected >= 0.f;
    const float badgeD = 58.f;
    const auto badge = juce::Rectangle<float> (left.getX(), left.getY() + 2.f, badgeD, badgeD);

    g.setColour (kInk);
    g.fillEllipse (badge);
    if (voiced)
    {
        g.setColour (kBlue.withAlpha (0.28f));
        g.drawEllipse (badge.expanded (3.f), 3.f);
        g.setColour (kBlueGlow);
        g.drawEllipse (badge, 2.2f);
        g.setColour (juce::Colours::white);
    }
    else
    {
        g.setColour (kPanelEdge);
        g.drawEllipse (badge, 1.6f);
        g.setColour (kTextDim);
    }

    g.setFont (juce::FontOptions (22.f, juce::Font::bold));
    g.drawText (voiced ? noteText (detected) : "--", badge, juce::Justification::centred);

    auto info = left.withTrimmedLeft (badgeD + 12.f);
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

    g.setColour (kTrack);
    g.fillRoundedRectangle (x0, cy - 3.f, w, 6.f, 3.f);

    for (int c = -100; c <= 100; c += 50)
    {
        const float x = x0 + w * (c + 100) / 200.f;
        g.setColour (c == 0 ? kBlue : kTextDim.withAlpha (0.5f));
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
        drawGlowDot (g, { nx, cy }, 6.5f, close ? kBlueGlow : kWarn, 0.65f);

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

    buildBackground();
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
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
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
    const float det = proc.getDetectedMidi();
    display.setValues (det, proc.getTargetMidi());

    ledVoiced = det >= 0.f;
    ledPhase += 0.12f;
    if (ledPhase > juce::MathConstants<float>::twoPi) ledPhase -= juce::MathConstants<float>::twoPi;
    repaint();
}

// Builds the static navy HUD background (gradient, halftone dots, speed lines) once
// into a cached image, so every 30 Hz repaint (driven by the LED pulse) stays cheap.
void PitchSnapEditor::buildBackground()
{
    const int w = getWidth(), h = getHeight();
    backgroundImage = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (backgroundImage);

    juce::ColourGradient bgGrad (kBg, 0.f, 0.f, kBgSoft, 0.f, (float) h, false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // angled dark panel behind the header, for a graphic-novel panel feel
    juce::Path header;
    header.startNewSubPath (0.f, 0.f);
    header.lineTo ((float) w, 0.f);
    header.lineTo ((float) w, 58.f);
    header.lineTo (0.f, 74.f);
    header.closeSubPath();
    g.setColour (kInk.withAlpha (0.55f));
    g.fillPath (header);

    // halftone screentone dots, fading out from the top-right corner
    const juce::Point<float> origin ((float) w - 10.f, 6.f);
    for (float yy = -20.f; yy < 230.f; yy += 9.f)
    {
        for (float xx = (float) w - 260.f; xx < (float) w + 20.f; xx += 9.f)
        {
            const float d = origin.getDistanceFrom ({ xx, yy });
            const float t = juce::jlimit (0.f, 1.f, 1.f - d / 230.f);
            if (t <= 0.02f) continue;
            const float rr = 0.9f + 1.5f * t;
            g.setColour (kBlueGlow.withAlpha (0.11f * t));
            g.fillEllipse (xx - rr, yy - rr, rr * 2.f, rr * 2.f);
        }
    }

    // manga-style speed lines radiating from the same corner
    const float angles[] = { 200.f, 214.f, 227.f, 241.f, 255.f, 269.f, 283.f };
    for (float ang : angles)
    {
        const float rad = juce::degreesToRadians (ang);
        const float len = 150.f;
        const juce::Point<float> p2 (origin.x + len * std::cos (rad), origin.y + len * std::sin (rad));
        juce::ColourGradient sg (kBlueGlow.withAlpha (0.32f), origin.x, origin.y,
                                 kBlueGlow.withAlpha (0.f), p2.x, p2.y, false);
        g.setGradientFill (sg);
        g.drawLine (origin.x, origin.y, p2.x, p2.y, 1.3f);
    }

    // thin panel-edge highlight under the header
    g.setColour (kBlue.withAlpha (0.18f));
    g.drawLine (0.f, 74.f, (float) w, 58.f, 1.f);

    // soft vignette at the very bottom for depth
    juce::ColourGradient vgrad (juce::Colours::transparentBlack, 0.f, (float) h - 60.f,
                                kInk.withAlpha (0.35f), 0.f, (float) h, false);
    g.setGradientFill (vgrad);
    g.fillRect (0, h - 60, w, 60);
}

void PitchSnapEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (backgroundImage, 0, 0);

    g.setColour (kText);
    g.setFont (juce::FontOptions (27.f, juce::Font::bold));
    g.drawText ("FocusQ", 20, 10, 220, 36, juce::Justification::centredLeft);

    // glowing blue LED: brighter and steady when a pitch is detected, a slow
    // standby pulse when idle.
    const float pulse  = 0.5f + 0.5f * std::sin (ledPhase);
    const float glowA  = ledVoiced ? 0.85f : 0.22f + 0.16f * pulse;
    const float coreA  = ledVoiced ? 1.0f  : 0.55f + 0.25f * pulse;
    const juce::Point<float> ledPos (176.f, 27.f);

    juce::ColourGradient halo (kBlueGlow.withAlpha (glowA * 0.5f), ledPos.x, ledPos.y,
                               kBlueGlow.withAlpha (0.f), ledPos.x + 15.f, ledPos.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (ledPos.x - 15.f, ledPos.y - 15.f, 30.f, 30.f);
    g.setColour (kBlueGlow.withAlpha (coreA));
    g.fillEllipse (ledPos.x - 4.f, ledPos.y - 4.f, 8.f, 8.f);
    g.setColour (juce::Colours::white.withAlpha (coreA * 0.9f));
    g.fillEllipse (ledPos.x - 1.6f, ledPos.y - 1.6f, 3.2f, 3.2f);

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
    auto knobs = r.removeFromTop (152);
    const int w = knobs.getWidth() / 4;
    for (auto* k : { &retune, &amount, &mix, &output })
    {
        auto cell = knobs.removeFromLeft (w);
        k->label.setBounds  (cell.removeFromTop (18));
        k->slider.setBounds (cell.reduced (8, 0));
    }
}
