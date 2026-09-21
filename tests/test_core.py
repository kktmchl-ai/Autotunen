import numpy as np, subprocess, sys
from scipy.signal import lfilter
import os
SR = int(os.environ.get('SR', 48000))
rng = np.random.default_rng(1)

def run(x, key=0, scale=0, ret=0, amt=1, mix=1, ch=1):
    x = np.asarray(x, dtype=np.float32)
    x.tofile("/tmp/in.f32")
    out = subprocess.run(["./runcore", "/tmp/in.f32", "/tmp/out.f32", str(SR), str(key), str(scale),
                          str(ret), str(amt), str(mix), str(ch)], capture_output=True, text=True)
    lat = int(out.stdout.strip()); print("   ", out.stderr.strip())
    return np.fromfile("/tmp/out.f32", dtype=np.float32), lat

def voice(f0_of_t, dur, formants=((700, 90), (1200, 110), (2600, 200))):
    """Glottal-like pulse train (phase accumulated, so glides are clean) through formant resonators."""
    n = int(dur * SR); f = f0_of_t(np.arange(n) / SR)
    ph = np.cumsum(f / SR); src = np.zeros(n)
    idx = np.where(np.diff(np.floor(ph)) > 0)[0]; src[idx] = 1.0
    src = lfilter([1], [1, -0.97], src)            # rough glottal tilt
    y = src
    for fc, bw in formants:
        r = np.exp(-np.pi * bw / SR); th = 2 * np.pi * fc / SR
        y = lfilter([1 - r], [1, -2 * r * np.cos(th), r * r], y)
    return (y / np.max(np.abs(y)) * 0.5).astype(np.float32)

def f0_track(y, lo=70, hi=900, frame=None, hop=None):
    frame = frame or int(2048 * SR / 48000); hop = hop or frame // 4
    """Independent autocorrelation pitch tracker (FFT based) -> Hz per frame."""
    res = []
    for s in range(0, len(y) - frame, hop):
        w = y[s:s+frame] * np.hanning(frame)
        ac = np.fft.irfft(np.abs(np.fft.rfft(w, 2*frame))**2)[:frame]
        lo_l, hi_l = int(SR/hi), int(SR/lo)
        seg = ac[lo_l:hi_l]; k = np.argmax(seg) + lo_l
        # parabolic
        a, b, c = ac[k-1], ac[k], ac[k+1]; d = a - 2*b + c
        k = k + (0.5*(a-c)/d if d != 0 else 0)
        res.append(SR / k)
    return np.array(res)

def cents(f, ref): return 1200 * np.log2(f / ref)
ok = True
def check(name, cond, info=""):
    global ok; ok &= bool(cond); print(("PASS" if cond else "FAIL"), name, info)

# ---- 1. static detuned note -> snaps to A3 (220 Hz) -------------------------------------------
print("1) static +30 cents sharp A3, chromatic, retune 0 ms")
x = voice(lambda t: 220 * 2 ** (30/1200) + 0*t, 3.0)
y, lat = run(x)
tr = f0_track(y[int(1.0*SR):int(2.8*SR)])
check("output pitch within 5 cents of 220 Hz", np.max(np.abs(cents(tr, 220))) < 5, f"(max err {np.max(np.abs(cents(tr,220))):.2f} c, median {np.median(cents(tr,220)):.2f} c)")
tr_in = f0_track(x[int(1.0*SR):int(2.8*SR)]); print(f"    input was {np.median(cents(tr_in,220)):.1f} cents above 220")

# ---- 2. vibrato flattened by fast retune ---------------------------------------------------------
print("2) +/-35 cent vibrato around A3, retune 0 ms  -> should go flat")
x = voice(lambda t: 220 * 2 ** ((35*np.sin(2*np.pi*5.5*t))/1200), 3.0)
y, lat = run(x)
tr = f0_track(y[int(1.0*SR):int(2.8*SR)]); tr_in = f0_track(x[int(1.0*SR):int(2.8*SR)])
print(f"    input spread {cents(tr_in,220).std():.1f} c   output spread {cents(tr,220).std():.1f} c   output mean {cents(tr,220).mean():.1f} c")
check("vibrato reduced by >75%", cents(tr,220).std() < 0.25 * cents(tr_in,220).std())
check("output centred on 220 Hz (|mean|<6c)", abs(cents(tr,220).mean()) < 6)

# ---- 3. slow retune keeps more of the natural movement -----------------------------------------
print("3) same vibrato, retune 150 ms -> should keep more vibrato than case 2")
y3, _ = run(x, ret=150)
tr3 = f0_track(y3[int(1.0*SR):int(2.8*SR)])
print(f"    output spread {cents(tr3,220).std():.1f} c")
check("slow retune preserves more movement than fast", cents(tr3,220).std() > cents(tr,220).std())

# ---- 4. scale snapping (C major): input A#3 (233.08 Hz)+10c must go to A3 or B3 ------------------
print("4) C major: A#3 is out of scale")
x = voice(lambda t: 233.08 * 2 ** (-8/1200) + 0*t, 3.0)   # a bit under A#3
y, _ = run(x, key=0, scale=1)
tr = f0_track(y[int(1.0*SR):int(2.8*SR)]); m = np.median(tr)
cands = {"A3": 220.0, "B3": 246.94}
name = min(cands, key=lambda k: abs(cents(m, cands[k])))
check("snapped onto an in-scale note", abs(cents(m, cands[name])) < 6, f"(-> {name}, {cents(m,cands[name]):.1f} c off)")

# ---- 5. formants preserved when shifting down 3 semitones -------------------------------------------
print("5) formant preservation: input 262 Hz (C4) forced to A#3-ish via key/scale; compare spectral envelope")
x = voice(lambda t: 261.63 * 2 ** (0.0*t) * 2 ** (-40/1200), 3.0)   # 40c flat of C4 -> should go UP 40c only
y, _ = run(x)
def envelope_peak(sig, lo, hi):
    seg = sig[int(1.0*SR):int(2.8*SR)]; w = seg * np.hanning(len(seg))
    S = np.abs(np.fft.rfft(w)); fr = np.fft.rfftfreq(len(w), 1/SR)
    # smooth over ~ 2 harmonics with a wide moving average
    k = int(400 / (fr[1]))
    Ssm = np.convolve(S, np.ones(k)/k, mode='same')
    m = (fr > lo) & (fr < hi); return fr[m][np.argmax(Ssm[m])]
print(f"    F1 region peak: in {envelope_peak(x,400,1000):.0f} Hz / out {envelope_peak(y,400,1000):.0f} Hz")
# bigger shift: 5 semitone: place input at F# below and pick pentatonic? use key mismatch instead
x2 = voice(lambda t: 311.13 + 0*t, 3.0)        # D#4 ; key C, Major -> D#4 is out of scale -> D4 or E4 (approx 1 st)
y2, _ = run(x2, key=0, scale=1)
pin, pout = envelope_peak(x2,400,1000), envelope_peak(y2,400,1000)
print(f"    F1 peak (1 st shift): in {pin:.0f} Hz / out {pout:.0f} Hz")
check("F1 peak stays within 120 Hz", abs(pin - pout) < 120)

# ---- 6. harmonic purity: how clean is the output? ------------------------------------------------------
print("6) artefact measurement (energy outside harmonic comb)")
x = voice(lambda t: 220 * 2 ** (45/1200) + 0*t, 4.0)
y, _ = run(x)
seg = y[int(1.5*SR):int(3.5*SR)]; S = np.abs(np.fft.rfft(seg*np.hanning(len(seg))))**2; fr = np.fft.rfftfreq(len(seg), 1/SR)
d = np.abs(fr[:,None] - 220*np.arange(1,40)[None,:]).min(axis=1); inb = d < 8
ratio = S[inb & (fr<6000)].sum() / S[fr<6000].sum()
print(f"    {100*ratio:.1f}% of energy on the 220 Hz harmonic comb")
check("clean harmonic output (>93%)", ratio > 0.93)

# ---- 7. unvoiced noise passes through at same level ---------------------------------------------------------
print("7) white noise passes (unvoiced)")
x = (rng.standard_normal(3*SR) * 0.1).astype(np.float32)
y, lat = run(x)
r_in, r_out = x[SR:].std(), y[SR:].std()
check("noise RMS within 1 dB", abs(20*np.log10(r_out/r_in)) < 1, f"({20*np.log10(r_out/r_in):+.2f} dB)")

# ---- 8. mix = 0 -> exact delayed dry -------------------------------------------------------------------------
print("8) mix=0 is a bit-exact delay by the reported latency")
x = voice(lambda t: 200 + 0*t, 1.0)
y, lat = run(x, mix=0)
err = np.max(np.abs(y[lat:] - x[:len(x)-lat]))
check("dry path exact", err < 1e-6, f"(max err {err:.2e}, latency {lat} = {1000*lat/SR:.1f} ms)")

# ---- 9. amount = 0 acts as transparent passthrough -----------------------------------------------------------------
print("9) amount=0 -> output ~ input (delayed)")
x = voice(lambda t: 220 * 2 ** (30/1200) + 0*t, 3.0)
y, lat = run(x, amt=0)
a = y[lat+SR:lat+2*SR]; b = x[SR:2*SR]
snr = 10*np.log10(np.sum(b**2)/np.sum((a-b)**2))
check("amount=0 near-transparent (SNR>25 dB)", snr > 25, f"(SNR {snr:.1f} dB)")

# ---- 10. stereo --------------------------------------------------------------------------------------------------------
print("10) stereo, identical channels stay identical")
x = voice(lambda t: 220 * 2 ** (30/1200) + 0*t, 2.0)
st = np.stack([x, x], axis=1).reshape(-1)
y, lat = run(st, ch=2)
L, R = y[0::2], y[1::2]
check("L == R", np.max(np.abs(L-R)) < 1e-7)
tr = f0_track(L[SR:]); check("stereo pitch corrected", np.max(np.abs(cents(tr,220))) < 5)

print("\nALL PASS" if ok else "\nSOME FAILED"); sys.exit(0 if ok else 1)
