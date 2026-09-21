#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PitchSnapProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    StringArray keys, scales;
    for (int i = 0; i < 12; ++i) keys.add (autotune::noteName (i));
    for (int i = 0; i < autotune::NumScales; ++i) scales.add (autotune::scaleName (i));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 },   "Key",   keys, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 0));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "retune", 1 }, "Retune Speed",
        NormalisableRange<float> (0.f, 400.f, 0.1f, 0.4f), 20.f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "Amount",
        NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f,
        AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f,
        AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return { p.begin(), p.end() };
}

PitchSnapProcessor::PitchSnapProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pKey    = apvts.getRawParameterValue ("key");
    pScale  = apvts.getRawParameterValue ("scale");
    pRetune = apvts.getRawParameterValue ("retune");
    pAmount = apvts.getRawParameterValue ("amount");
    pMix    = apvts.getRawParameterValue ("mix");
    pOutput = apvts.getRawParameterValue ("output");
}

bool PitchSnapProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in  = l.getMainInputChannelSet();
    const auto& out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PitchSnapProcessor::prepareToPlay (double sampleRate, int)
{
    preparedChannels = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));
    core.prepare (sampleRate, preparedChannels);
    setLatencySamples (core.latencySamples());          // the host compensates for this
    lastGain = juce::Decibels::decibelsToGain (pOutput->load());
    firstBlock = true;
}

void PitchSnapProcessor::releaseResources() {}

void PitchSnapProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin (buffer.getNumChannels(), preparedChannels);
    if (numCh < 1 || numSamples < 1) return;

    core.setKey      ((int) pKey->load());
    core.setScale    ((int) pScale->load());
    core.setRetuneMs (pRetune->load());
    core.setAmount   (pAmount->load() * 0.01f);
    core.setMix      (pMix->load() * 0.01f);
    if (firstBlock) { core.snapMix(); firstBlock = false; }

    core.process (buffer.getArrayOfWritePointers(), numSamples);

    const float g = juce::Decibels::decibelsToGain (pOutput->load());
    if (std::abs (g - lastGain) > 1.0e-6f)
    {
        buffer.applyGainRamp (0, numSamples, lastGain, g);
        lastGain = g;
    }
    else if (std::abs (g - 1.f) > 1.0e-6f)
        buffer.applyGain (g);
}

juce::AudioProcessorEditor* PitchSnapProcessor::createEditor() { return new PitchSnapEditor (*this); }

void PitchSnapProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PitchSnapProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchSnapProcessor(); }
