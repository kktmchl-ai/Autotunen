import numpy as np, os, sys
exec(open('test_core.py').read().split("# ---- 1.")[0])
from scipy.linalg import solve_toeplitz
from scipy.signal import freqz
print(f"===== sample rate {SR} =====")
def lpc_peaks(sig, order=None, n=3):
    order = order or int(18 * SR / 48000)
    seg = sig[int(1.0*SR):int(2.8*SR)].astype(np.float64)
    seg = np.append(seg[0], seg[1:] - 0.9*seg[:-1]) * np.hanning(len(seg))
    r = np.correlate(seg, seg, 'full')[len(seg)-1:len(seg)+order]
    a = solve_toeplitz(r[:order], r[1:order+1])
    w, h = freqz([1], np.concatenate([[1], -a]), worN=16384, fs=SR); mag = np.abs(h)
    pk = [i for i in range(1, len(mag)-1) if mag[i] > mag[i-1] and mag[i] > mag[i+1] and w[i] < 4000]
    pk = sorted(pk, key=lambda i: -mag[i])[:n]; return sorted(w[i] for i in pk)
for name, f_in, mask, label in [("down ~5 st", 226.0, 0x010, "only E allowed (E3 = 164.8 Hz)"),
                                ("down ~4.5 st", 226.0, 0x020, "only F allowed (nearest F3 = 174.6 Hz)")]:
    os.environ["MASK"] = hex(mask)[2:]
    x = voice(lambda t: f_in + 0*t, 3.0)
    y, _ = run(x, key=0, scale=0)
    f_out = np.median(f0_track(y[SR:int(2.8*SR)]))
    ratio = f_out / f_in
    pin, pout = lpc_peaks(x), lpc_peaks(y)
    print(f"  {name}: pitch {f_in:.0f} -> {f_out:.1f} Hz (x{ratio:.3f}, {cents(f_out,f_in):+.0f} c)  [{label}]")
    print(f"     LPC formant peaks  input: {np.round(pin).astype(int)}   output: {np.round(pout).astype(int)}   (if formants had moved with pitch: {np.round(np.array(pin)*ratio).astype(int)})")
    # each output peak should be closer to an input peak than to the pitch-shifted position
    good = 0
    for po in pout:
        d_keep = min(abs(po - pi) for pi in pin); d_move = min(abs(po - pi*ratio) for pi in pin)
        good += d_keep < d_move
    check(f"{name}: formants stay put", good >= len(pout) - 1)
sys.exit(0 if ok else 1)
