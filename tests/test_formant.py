import numpy as np, os, sys
src = open('test_core.py').read().split("# ---- 1.")[0]
exec(src)
print(f"===== sample rate {SR} =====")

def harm_db(sig, f0, K=14):
    seg = sig[int(1.0*SR):int(2.8*SR)].astype(np.float64); w = seg*np.hanning(len(seg))
    S = np.abs(np.fft.rfft(w)); fr = np.fft.rfftfreq(len(w), 1/SR); out = []
    for k in range(1, K+1):
        m = np.abs(fr - k*f0) < 0.25*f0; out.append(S[m].max())
    a = 20*np.log10(np.array(out)); return a - a.max()

# input: 285.6 Hz (D4 -49c) ; C minor pentatonic -> plugin moves it to 311.2 Hz (+149 c) ; reference is a true voice at 311.2 Hz
f_in = 293.66 * 2 ** (-49/1200)
x = voice(lambda t: f_in + 0*t, 3.0)
y, _ = run(x, key=0, scale=6)
f_out = np.median(f0_track(y[SR:int(2.8*SR)]))
ref = voice(lambda t: f_out + 0*t, 3.0)
print(f"    pitch {f_in:.1f} -> {f_out:.1f} Hz ({cents(f_out, f_in):+.0f} c)")
A_out, A_ref, A_in = harm_db(y, f_out), harm_db(ref, f_out), harm_db(x, f_in)
K = 14
d_pres = np.mean(np.abs(A_out - A_ref))          # output vs a real voice at that pitch  (formants preserved => small)
# what a *formant-shifting* pitch shifter would give: input envelope stretched by the pitch ratio
ratio = f_out / f_in
Ain_at_harm = np.interp(np.arange(1, K+1)*f_out/ratio, np.arange(1, K+1)*f_in, A_in)  # envelope sampled at the shifted-back freqs
d_shift = np.mean(np.abs(A_out - (Ain_at_harm - Ain_at_harm.max())))
print(f"    mean |dB diff| vs real voice at target pitch (formants kept)  : {d_pres:.2f} dB")
print(f"    mean |dB diff| vs 'formants moved with pitch' hypothesis      : {d_shift:.2f} dB")
check("output envelope matches a real voice at the new pitch (< 4 dB avg)", d_pres < 4.0)
check("matches 'formants preserved' better than 'formants shifted'", d_pres < d_shift)
sys.exit(0 if ok else 1)
