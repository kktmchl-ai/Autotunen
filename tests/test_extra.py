import numpy as np, subprocess, os, sys
src = open('test_core.py').read().split("# ---- 1.")[0]
src = src.replace("SR = 48000", f"SR = int(os.environ.get('SR', 48000))")
exec(src)
print(f"===== sample rate {SR} =====")

# A. off-key melody in C major
notes = [60, 62, 64, 65, 67, 69, 71, 72]; det = [+28, -33, +18, -41, +25, -15, +38, -22]; dur = 0.5
def f_of_t(t):
    i = np.minimum((t/dur).astype(int), len(notes)-1)
    return 440 * 2 ** ((np.array(notes)[i] + np.array(det)[i]/100 - 69)/12)
x = voice(f_of_t, dur*len(notes))
y, lat = run(x, key=0, scale=1, ret=10)
tr = f0_track(y); fr_ = int(2048*SR/48000); tf = (np.arange(len(tr))*(fr_//4) + fr_/2)/SR - lat/SR
errs = []
for i, m in enumerate(notes):
    sel = (tf > i*dur + 0.25) & (tf < (i+1)*dur - 0.05)
    errs.append(np.median(cents(tr[sel], 440 * 2 ** ((m-69)/12))))
print("    per-note residual (cents):", np.round(errs, 1))
check("melody: all notes within 8 cents of scale note", np.max(np.abs(errs)) < 8)

# B. extreme registers
for f0, off, lab in [(82.41, 25, "E2 +25c (deep bass)"), (659.26, -30, "E5 -30c (soprano)"), (110.0, 20, "A2 +20c")]:
    x = voice(lambda t: f0 * 2 ** (off/1200) + 0*t, 3.0)
    y, _ = run(x); tr = f0_track(y[SR:int(2.8*SR)], lo=60, hi=1000)
    e = np.max(np.abs(cents(tr, f0)))
    check(f"range {lab}", e < 8, f"(max err {e:.1f} c)")

# C. silence, DC, loud, and abrupt on/off must not blow up / click
z, _ = run(np.zeros(SR)); check("silence -> silence", np.max(np.abs(z)) == 0)
x = voice(lambda t: 220 * 2 ** (30/1200) + 0*t, 2.0); x = np.concatenate([np.zeros(SR//2), x, np.zeros(SR//2)])
y, lat = run(x)
check("no NaN/Inf", np.all(np.isfinite(y)))
check("peak stays sane (<1.3 x input peak)", np.max(np.abs(y)) < 1.3 * np.max(np.abs(x)), f"({np.max(np.abs(y)):.2f} vs {np.max(np.abs(x)):.2f})")
jump = np.max(np.abs(np.diff(y))) / np.max(np.abs(np.diff(x)))
check("no clicks (max sample jump <= 1.5 x input's)", jump < 1.5, f"({jump:.2f})")
tail = y[lat + int(1.5*SR)+SR//4 : ]; check("decays to silence after input stops", np.max(np.abs(tail[-2000:])) < 1e-4 if len(tail) > 2000 else True)
z = (np.random.default_rng(3).standard_normal(SR)*0.9).clip(-1,1).astype(np.float32)
y, _ = run(z); check("loud noise finite & bounded", np.all(np.isfinite(y)) and np.max(np.abs(y)) < 2.5)

# D. formant preservation, big shift (3 semitones): key=C major, input D#4-ish forced by pentatonic? use Blues far away
x = voice(lambda t: 293.66 * 2 ** (-49/1200) + 0*t, 3.0)       # ~ D4 -49c ; C-major pentatonic-like via scale mask below
y, _ = run(x, key=0, scale=6)     # C minor pentatonic: C Eb F G Bb -> D4 goes to Eb4 or C4  (1-2 st)
from scipy.linalg import solve_toeplitz
from scipy.signal import freqz
def peak(sig, lo, hi, order=None):
    order = order or int(16 * SR / 48000)
    seg = sig[int(1.0*SR):int(2.8*SR)].astype(np.float64)
    seg = np.append(seg[0], seg[1:] - 0.9*seg[:-1]) * np.hanning(len(seg))         # pre-emphasis
    r = np.correlate(seg, seg, 'full')[len(seg)-1:len(seg)+order]
    a = solve_toeplitz(r[:order], r[1:order+1])
    w, h = freqz([1], np.concatenate([[1], -a]), worN=8192, fs=SR)
    m = (w > lo) & (w < hi); return w[m][np.argmax(np.abs(h[m]))]
pi_, po_ = peak(x,400,1000), peak(y,400,1000)
tr_in, tr_out = np.median(f0_track(x[SR:int(2.8*SR)])), np.median(f0_track(y[SR:int(2.8*SR)]))
print(f"    pitch {tr_in:.1f} -> {tr_out:.1f} Hz ({cents(tr_out,tr_in):+.0f} c);  F1 {pi_:.0f} -> {po_:.0f} Hz")
check("formant F1 moved much less than pitch did", abs(cents(po_, pi_)) < 0.5*abs(cents(tr_out,tr_in)) + 40)
sys.exit(0 if ok else 1)
