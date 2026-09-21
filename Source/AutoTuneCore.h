// AutoTuneCore.h
// ---------------------------------------------------------------------------
// Real-time monophonic auto-tune DSP core. No JUCE dependency, so it can be
// unit-tested standalone.
//
//   1. Pitch detection : YIN (de Cheveigne & Kawahara, 2002) on a decimated
//                        copy of the input, run every few ms.
//   2. Note snapping   : nearest note of the chosen key/scale (with hysteresis).
//   3. Retune speed    : one-pole glide of the correction (0 ms = hard "T-Pain").
//   4. Pitch shifting  : pitch-synchronous overlap-add (PSOLA-style). Grains of
//                        two periods are copied from the input and re-laid on
//                        the output at a spacing of T0/ratio. Because the grain
//                        waveform itself is never stretched, formants stay put.
//   5. Unvoiced parts  : when no pitch is found the grains run at ratio 1
//                        (transparent pass-through), so "s", "t", "k" and
//                        breaths are left alone.
// ---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace autotune
{

enum ScaleType
{
    Chromatic = 0,
    Major,
    Minor,
    HarmonicMinor,
    Dorian,
    PentatonicMajor,
    PentatonicMinor,
    Blues,
    NumScales
};

inline const char* scaleName (int i)
{
    static const char* names[] = { "Chromatic", "Major", "Minor", "Harmonic Minor",
                                   "Dorian", "Pentatonic Major", "Pentatonic Minor", "Blues" };
    return names[std::clamp (i, 0, (int) NumScales - 1)];
}

inline const char* noteName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pc % 12) + 12) % 12];
}

// Bit i set  =>  interval i (in semitones above the root) is allowed.
inline uint16_t scaleMask (int type)
{
    auto m = [] (std::initializer_list<int> iv) { uint16_t r = 0; for (int i : iv) r |= (uint16_t) (1u << i); return r; };
    switch (type)
    {
        case Major:           return m ({ 0, 2, 4, 5, 7, 9, 11 });
        case Minor:           return m ({ 0, 2, 3, 5, 7, 8, 10 });
        case HarmonicMinor:   return m ({ 0, 2, 3, 5, 7, 8, 11 });
        case Dorian:          return m ({ 0, 2, 3, 5, 7, 9, 10 });
        case PentatonicMajor: return m ({ 0, 2, 4, 7, 9 });
        case PentatonicMinor: return m ({ 0, 3, 5, 7, 10 });
        case Blues:           return m ({ 0, 3, 5, 6, 7, 10 });
        default:              return 0x0FFF;
    }
}

class Core
{
public:
    static constexpr double kMinF0 = 70.0;   // lowest tracked pitch  (Hz)
    static constexpr double kMaxF0 = 900.0;  // highest tracked pitch (Hz)

    // ---- setup ------------------------------------------------------------
    void prepare (double sampleRate, int channels)
    {
        sr     = sampleRate;
        numCh  = std::max (1, std::min (channels, kMaxChannels));

        t0Max  = (int) std::ceil (sr / kMinF0);
        t0Min  = std::max (8, (int) std::floor (sr / kMaxF0));
        dOut   = 2 * t0Max;                         // reported latency

        int size = 1;
        while (size < 8 * t0Max) size <<= 1;
        bufMask = size - 1;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            inBuf[c].assign ((size_t) size, 0.f);
            outBuf[c].assign ((size_t) size, 0.f);
        }
        monoBuf.assign ((size_t) size, 0.f);

        // Decimated domain used for pitch detection (~12 kHz).
        decFactor = std::max (1, (int) std::lround (sr / 12000.0));
        fsDec     = sr / decFactor;
        tauMin    = std::max (2, (int) std::floor (fsDec / kMaxF0));
        tauMax    = (int) std::ceil (fsDec / kMinF0) + 2;
        detW      = (int) (tauMax * 1.25);
        int dsz = 1;
        while (dsz < 2 * (detW + tauMax)) dsz <<= 1;
        decMask = dsz - 1;
        decBuf.assign ((size_t) dsz, 0.f);
        seg.assign ((size_t) (detW + tauMax + 2), 0.f);
        dfn.assign ((size_t) (tauMax + 2), 0.0);
        cmn.assign ((size_t) (tauMax + 2), 0.0);

        hop = std::max (64, (int) std::lround (sr / 375.0));
        reset();
    }

    void reset()
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            std::fill (inBuf[c].begin(), inBuf[c].end(), 0.f);
            std::fill (outBuf[c].begin(), outBuf[c].end(), 0.f);
        }
        std::fill (decBuf.begin(), decBuf.end(), 0.f);
        std::fill (monoBuf.begin(), monoBuf.end(), 0.f);
        n = 0;  decWrite = 0;  decAcc = 0;  decCount = 0;  sinceDetect = 0;
        nextSynth = (double) t0Max;
        aPos      = nextSynth;
        voiced = false;  unvoicedCount = 100;  f0 = 0.0;
        hist[0] = hist[1] = hist[2] = 0.0;
        wasVoiced = false;  shiftSm = 0.0;  heldTarget = -1000;
        mixSm = mix;
        detMidi.store (-1.f);  tgtMidi.store (-1.f);
    }

    int latencySamples() const noexcept { return dOut; }

    // ---- parameters (call from the audio thread) ------------------------------
    void setKey (int k)          { key = ((k % 12) + 12) % 12; }
    void setScale (int s)        { mask = scaleMask (s); }
    void setCustomMask (uint16_t m) { mask = m ? m : (uint16_t) 0x0FFF; }   // bit i = note i above the key (root)
    void setRetuneMs (float ms)  { retuneMs = std::max (0.f, ms); }
    void setAmount (float a)     { amount = std::clamp (a, 0.f, 1.f); }
    void setMix (float m)        { mix = std::clamp (m, 0.f, 1.f); }
    void snapMix()               { mixSm = mix; }          // skip the smoothing (first block after prepare)

    // ---- telemetry (safe to read from the GUI thread) --------------------------
    float detectedMidi() const noexcept { return detMidi.load (std::memory_order_relaxed); }  // <0 = unvoiced
    float targetMidi()   const noexcept { return tgtMidi.load (std::memory_order_relaxed); }

    // ---- processing (in place) --------------------------------------------------
    void process (float* const* io, int numSamples)
    {
        const int   nc      = numCh;
        const float invNc   = 1.f / (float) nc;
        const int64_t mask64 = bufMask;

        for (int i = 0; i < numSamples; ++i)
        {
            const int64_t wi = n & mask64;
            float mono = 0.f;
            for (int c = 0; c < nc; ++c)
            {
                const float v = io[c][i];
                inBuf[c][(size_t) wi] = v;
                mono += v;
            }
            mono *= invNc;
            monoBuf[(size_t) wi] = mono;

            // decimate (box-car) for the pitch detector
            decAcc += mono;
            if (++decCount >= decFactor)
            {
                decBuf[(size_t) (decWrite & decMask)] = decAcc / (float) decFactor;
                ++decWrite;
                decAcc = 0.f;  decCount = 0;
            }
            if (++sinceDetect >= hop) { sinceDetect = 0; detect(); }

            // every grain whose input is fully available gets rendered now
            while ((double) (n - t0Max) >= nextSynth)
                makeGrain (nc);

            const int64_t mi = (n - dOut) & mask64;
            mixSm += (mix - mixSm) * 0.002f;
            for (int c = 0; c < nc; ++c)
            {
                const float wet = outBuf[c][(size_t) mi];
                outBuf[c][(size_t) mi] = 0.f;
                const float dry = inBuf[c][(size_t) mi];       // latency-aligned dry
                io[c][i] = dry + (wet - dry) * mixSm;
            }
            ++n;
        }
    }

private:
    static constexpr int kMaxChannels = 2;

    // ---- pitch detection (YIN) ----------------------------------------------------
    void markUnvoiced()
    {
        if (++unvoicedCount >= 4)
        {
            voiced = false;
            detMidi.store (-1.f, std::memory_order_relaxed);
        }
    }

    void detect()
    {
        const int N = detW + tauMax;
        if (decWrite < N) return;

        double energy = 0.0;
        for (int k = 0; k < N; ++k)
        {
            const float v = decBuf[(size_t) ((decWrite - N + k) & decMask)];
            seg[(size_t) k] = v;
            energy += (double) v * v;
        }
        if (energy / N < 1e-6) { markUnvoiced(); return; }        // below about -60 dBFS

        // difference function
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const float* a = seg.data();
            const float* b = seg.data() + tau;
            for (int j = 0; j < detW; ++j) { const double d = (double) a[j] - b[j]; s += d * d; }
            dfn[(size_t) tau] = s;
        }
        // cumulative mean normalised difference
        cmn[0] = 1.0;
        double run = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += dfn[(size_t) tau];
            cmn[(size_t) tau] = run > 0.0 ? dfn[(size_t) tau] * tau / run : 1.0;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmn[(size_t) tau] < 0.20)
            {
                while (tau + 1 < tauMax && cmn[(size_t) tau + 1] < cmn[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            int mt = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cmn[(size_t) tau] < cmn[(size_t) mt]) mt = tau;
            if (cmn[(size_t) mt] < 0.25) best = mt;
        }
        if (best < 0) { markUnvoiced(); return; }

        // parabolic interpolation for sub-sample accuracy
        const double a = cmn[(size_t) best - 1], b = cmn[(size_t) best], c = cmn[(size_t) best + 1];
        const double den = a - 2.0 * b + c;
        double shift = den != 0.0 ? 0.5 * (a - c) / den : 0.0;
        shift = std::clamp (shift, -1.0, 1.0);
        const double coarse = (best + shift) * decFactor;          // period in full-rate samples
        const double fine   = refinePeriod (coarse);
        const double f = sr / fine;
        if (f < kMinF0 || f > kMaxF0) { markUnvoiced(); return; }

        if (! voiced) hist[0] = hist[1] = hist[2] = f;
        hist[0] = hist[1];  hist[1] = hist[2];  hist[2] = f;
        f0 = std::max (std::min (hist[0], hist[1]), std::min (std::max (hist[0], hist[1]), hist[2]));  // median of 3
        voiced = true;
        unvoicedCount = 0;
        detMidi.store ((float) (69.0 + 12.0 * std::log2 (f0 / 440.0)), std::memory_order_relaxed);
    }

    // Refine the period at the full sample rate: evaluate the plain difference
    // function on a handful of lags around the coarse estimate and interpolate
    // its minimum. This removes the few-cent bias of the decimated YIN stage.
    double refinePeriod (double coarse) const
    {
        const int P    = (int) std::lround (coarse);
        const int span = decFactor + 1;
        const int Lmin = P - span, Lmax = P + span;
        const int W    = std::max (256, 2 * P);
        const int64_t start = n - W + 1 - Lmax;
        if (start < 0 || W + Lmax + 4 > bufMask) return coarse;

        double d[64];
        const int cnt = Lmax - Lmin + 1;
        if (cnt > 64) return coarse;
        int64_t idx = start;
        for (int i = 0; i < cnt; ++i)
        {
            const int L = Lmin + i;
            double s = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const double a = monoBuf[(size_t) ((idx + j) & bufMask)];
                const double b = monoBuf[(size_t) ((idx + j + L) & bufMask)];
                s += (a - b) * (a - b);
            }
            d[i] = s;
        }
        int mi = 0;
        for (int i = 1; i < cnt; ++i) if (d[i] < d[mi]) mi = i;
        if (mi == 0 || mi == cnt - 1) return coarse;               // minimum not bracketed
        const double den = d[mi - 1] - 2.0 * d[mi] + d[mi + 1];
        const double sh  = den > 0.0 ? std::clamp (0.5 * (d[mi - 1] - d[mi + 1]) / den, -1.0, 1.0) : 0.0;
        return (double) (Lmin + mi) + sh;
    }

    // ---- note selection -----------------------------------------------------------
    bool allowed (int midiNote) const
    {
        const int pc = (((midiNote - key) % 12) + 12) % 12;
        return (mask >> pc) & 1;
    }

    int pickTarget (double midi)
    {
        const int base = (int) std::floor (midi);
        int best = base;  double bd = 1e9;
        for (int cand = base - 12; cand <= base + 13; ++cand)
        {
            if (! allowed (cand)) continue;
            const double d = std::fabs (cand - midi);
            if (d < bd) { bd = d;  best = cand; }
        }
        // hysteresis: stay on the current note unless the new one is clearly closer
        if (heldTarget > -500 && allowed (heldTarget) && std::fabs (heldTarget - midi) < bd + 0.15)
            return heldTarget;
        heldTarget = best;
        return best;
    }

    // ---- one synthesis grain --------------------------------------------------------
    void makeGrain (int nc)
    {
        double T0    = 0.005 * sr;    // fallback period for unvoiced audio
        double ratio = 1.0;

        if (voiced && f0 > 0.0)
        {
            T0 = std::clamp (sr / f0, (double) t0Min, (double) t0Max);
            const double midi   = 69.0 + 12.0 * std::log2 (f0 / 440.0);
            const int    target = pickTarget (midi);
            tgtMidi.store ((float) target, std::memory_order_relaxed);

            if (! wasVoiced) shiftSm = 0.0;
            const double desired = target - midi;                  // semitones
            const double dtMs    = 1000.0 * (T0 / ratio) / sr;
            const double alpha   = retuneMs < 0.5f ? 1.0 : 1.0 - std::exp (-dtMs / retuneMs);
            shiftSm += (desired - shiftSm) * alpha;
            ratio = std::clamp (std::exp2 (shiftSm * amount / 12.0), 0.5, 2.0);
            wasVoiced = true;
        }
        else
        {
            wasVoiced  = false;
            heldTarget = -1000;
            tgtMidi.store (-1.f, std::memory_order_relaxed);
        }

        const double tS = nextSynth;

        // Analysis mark for this grain. `aPos` has already advanced by one period since the
        // previous grain, so at ratio 1 (tS advances by T0 as well) the offset tS - aPos stays
        // exactly 0 and the wet path is time-aligned with the delayed dry path. When the pitch is
        // shifted the offset drifts, and we jump by whole periods (repeating or skipping one) to
        // keep it within +/- T0/2. Whole-period jumps preserve the waveform phase.
        const int half = std::max (2, (int) std::lround (T0));
        while (tS - aPos >  0.5 * T0) aPos += T0;
        while (tS - aPos < -0.5 * T0) aPos -= T0;
        while (aPos + half > (double) (n - 1)) aPos -= T0;         // never read the future (very low voices)

        const int64_t ia   = (int64_t) std::llround (aPos);
        const int64_t is   = (int64_t) std::llround (tS);
        const float   gain = (float) (1.0 / ratio);                 // Hann OLA at spacing T0/ratio sums to `ratio`
        const double  wStep = 3.14159265358979323846 / half;
        const int64_t m64  = bufMask;

        for (int k = -half; k <= half; ++k)
        {
            const float w = gain * (float) (0.5 + 0.5 * std::cos (wStep * k));
            const size_t oi = (size_t) ((is + k) & m64);
            const size_t ii = (size_t) ((ia + k) & m64);
            for (int c = 0; c < nc; ++c)
                outBuf[c][oi] += w * inBuf[c][ii];
        }
        nextSynth += T0 / ratio;
        aPos      += T0;
    }

    // ---- state ---------------------------------------------------------------------------
    double sr = 44100.0;
    int    numCh = 1;
    int    t0Max = 0, t0Min = 0, dOut = 0, bufMask = 0;
    std::vector<float> inBuf[kMaxChannels], outBuf[kMaxChannels], monoBuf;

    int    decFactor = 4, decMask = 0, tauMin = 0, tauMax = 0, detW = 0, hop = 128;
    double fsDec = 12000.0;
    std::vector<float>  decBuf, seg;
    std::vector<double> dfn, cmn;
    float  decAcc = 0.f;
    int    decCount = 0, sinceDetect = 0;
    int64_t decWrite = 0, n = 0;

    bool   voiced = false;
    int    unvoicedCount = 100;
    double f0 = 0.0, hist[3] = { 0, 0, 0 };

    double nextSynth = 0.0, aPos = 0.0;
    bool   wasVoiced = false;
    double shiftSm = 0.0;
    int    heldTarget = -1000;

    int      key = 0;
    uint16_t mask = 0x0FFF;
    float    retuneMs = 20.f, amount = 1.f, mix = 1.f, mixSm = 1.f;

    std::atomic<float> detMidi { -1.f }, tgtMidi { -1.f };
};

} // namespace autotune
