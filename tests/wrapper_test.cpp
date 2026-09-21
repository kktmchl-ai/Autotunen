// Headless integration test: drives the real PitchSnapProcessor through its parameters and processBlock().
#include "../Source/PluginProcessor.h"
#include <cstdio>

static const double SR = 48000.0;
static int failures = 0;
static void check (const char* name, bool ok, const juce::String& info = {})
{
    std::printf ("%s  %s  %s\n", ok ? "PASS" : "FAIL", name, info.toRawUTF8());
    if (! ok) ++failures;
}

static void setParam (PitchSnapProcessor& p, const char* id, float realValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

// harmonic tone (sawtooth-ish) so pitch detection has something to chew on
static std::vector<float> tone (double f0, double seconds)
{
    std::vector<float> x ((size_t) (seconds * SR));
    for (size_t i = 0; i < x.size(); ++i)
    {
        double s = 0;  for (int k = 1; k <= 12; ++k) s += std::sin (2 * juce::MathConstants<double>::pi * k * f0 * (double) i / SR) / k;
        x[i] = (float) (0.3 * s);
    }
    return x;
}

// autocorrelation pitch estimate on a segment
static double measureF0 (const float* x, int n)
{
    int lo = (int) (SR / 500), hi = (int) (SR / 100);
    std::vector<double> ac ((size_t) hi + 2, 0.0);
    for (int L = lo - 1; L <= hi + 1; ++L) { double s = 0; for (int i = 0; i + L < n; ++i) s += (double) x[i] * x[i + L]; ac[(size_t) L] = s; }
    int best = lo; for (int L = lo; L <= hi; ++L) if (ac[(size_t) L] > ac[(size_t) best]) best = L;
    double a = ac[(size_t) best - 1], b = ac[(size_t) best], c = ac[(size_t) best + 1];
    double d = a - 2 * b + c;  return SR / (best + (d != 0 ? 0.5 * (a - c) / d : 0));
}
static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

// run `input` through the processor in 512-sample blocks (stereo), return left channel
static std::vector<float> render (PitchSnapProcessor& p, const std::vector<float>& in)
{
    std::vector<float> out (in.size());
    juce::AudioBuffer<float> buf (2, 512);  juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in.size(); pos += 512)
    {
        const int n = (int) std::min<size_t> (512, in.size() - pos);
        for (int c = 0; c < 2; ++c) std::copy (in.begin() + (long) pos, in.begin() + (long) pos + n, buf.getWritePointer (c));
        buf.setSize (2, n, true, false, true);
        p.processBlock (buf, midi);
        std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, out.begin() + (long) pos);
    }
    return out;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    PitchSnapProcessor p;
    p.setPlayConfigDetails (2, 2, SR, 512);
    p.prepareToPlay (SR, 512);
    std::printf ("reported latency: %d samples (%.1f ms)\n", p.getLatencySamples(), 1000.0 * p.getLatencySamples() / SR);
    check ("latency reported to host", p.getLatencySamples() > 0);

    // 1. chromatic, retune 0, input 30 cents sharp of A3 -> 220 Hz
    setParam (p, "retune", 0.f); setParam (p, "amount", 100.f); setParam (p, "mix", 100.f); setParam (p, "output", 0.f);
    auto in = tone (220.0 * std::pow (2.0, 30.0 / 1200.0), 3.0);
    auto out = render (p, in);
    double f = measureF0 (out.data() + (int) (1.5 * SR), (int) (1.0 * SR));
    check ("chromatic: +30c input lands on A3", std::abs (cents (f, 220.0)) < 6.0, juce::String::formatted ("(%.2f Hz, %+.1f c)", f, cents (f, 220.0)));

    // 2. key/scale wiring: C Major, input just under A#3 (233.08 Hz) must go to A3 or B3, never stay on A#3
    p.reset(); p.prepareToPlay (SR, 512);
    setParam (p, "key", 0.f); setParam (p, "scale", 1.f);          // C, Major
    in = tone (233.08 * std::pow (2.0, -8.0 / 1200.0), 3.0);  out = render (p, in);
    f = measureF0 (out.data() + (int) (1.5 * SR), (int) (1.0 * SR));
    const bool onA = std::abs (cents (f, 220.0)) < 8.0, onB = std::abs (cents (f, 246.94)) < 8.0;
    check ("C major: A#3 pulled onto A3/B3", onA || onB, juce::String::formatted ("(%.2f Hz)", f));

    // 3. key change: same input with key = A# (index 10) Major has A#3 in scale -> stays put
    p.reset(); p.prepareToPlay (SR, 512);
    setParam (p, "key", 10.f); setParam (p, "scale", 1.f);
    in = tone (233.08 * std::pow (2.0, -8.0 / 1200.0), 3.0);  out = render (p, in);
    f = measureF0 (out.data() + (int) (1.5 * SR), (int) (1.0 * SR));
    check ("A# major: A#3 stays A#3", std::abs (cents (f, 233.08)) < 8.0, juce::String::formatted ("(%.2f Hz, %+.1f c)", f, cents (f, 233.08)));

    // 4. Mix = 0  ->  bit-exact delayed dry
    p.reset(); p.prepareToPlay (SR, 512);
    setParam (p, "key", 0.f); setParam (p, "scale", 0.f); setParam (p, "mix", 0.f);
    in = tone (220.0 * std::pow (2.0, 30.0 / 1200.0), 1.0);  out = render (p, in);
    const int lat = p.getLatencySamples();
    float maxErr = 0;  for (size_t i = (size_t) lat + 4000; i < in.size(); ++i) maxErr = std::max (maxErr, std::abs (out[i] - in[i - (size_t) lat]));
    check ("mix=0: dry, delayed by exactly the reported latency", maxErr < 1e-5f, juce::String::formatted ("(max err %.2e)", maxErr));

    // 5. output gain -6 dB
    p.reset(); p.prepareToPlay (SR, 512);
    setParam (p, "mix", 0.f); setParam (p, "output", -6.f);
    out = render (p, in);
    double rIn = 0, rOut = 0;  for (size_t i = (size_t) lat + 6000; i < in.size(); ++i) { rIn += in[i - (size_t) lat] * in[i - (size_t) lat]; rOut += out[i] * out[i]; }
    check ("output -6 dB", std::abs (10 * std::log10 (rOut / rIn) + 6.0) < 0.1, juce::String::formatted ("(%.2f dB)", 10 * std::log10 (rOut / rIn)));

    // 6. state save / restore
    setParam (p, "retune", 123.f); setParam (p, "key", 7.f);
    juce::MemoryBlock mb;  p.getStateInformation (mb);
    setParam (p, "retune", 5.f);  setParam (p, "key", 2.f);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    check ("state round-trip", std::abs (p.apvts.getRawParameterValue ("retune")->load() - 123.f) < 0.2f
                               && (int) p.apvts.getRawParameterValue ("key")->load() == 7);

    // 7. mono layout
    PitchSnapProcessor m;  m.setPlayConfigDetails (1, 1, SR, 512);
    juce::AudioProcessor::BusesLayout lay;  lay.inputBuses.add (juce::AudioChannelSet::mono()); lay.outputBuses.add (juce::AudioChannelSet::mono());
    check ("mono/mono layout accepted", m.isBusesLayoutSupported (lay));
    lay.inputBuses.set (0, juce::AudioChannelSet::stereo());
    check ("mono in / stereo out rejected", ! m.isBusesLayoutSupported (lay));

    // 8. editor can be created (needs a GUI-capable JUCE build, but not a display for construction)
    { std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
      check ("editor constructs", ed != nullptr && ed->getWidth() > 0); }

    std::printf ("\n%s\n", failures == 0 ? "ALL PASS" : "SOME FAILED");
    return failures;
}
