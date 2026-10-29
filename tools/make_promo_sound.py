"""make_promo_sound.py -- the "social critic" pass: a driving news-investigation bed, a
loopable flight wind, landing thuds and a takeoff lift.

The lullaby bed (assets/sfx/bgm.wav) is left alone; this writes new files beside it.
Synthesised with numpy, 44.1 kHz, 16-bit stereo.

    python tools/make_promo_sound.py
    -> assets/sfx/bed_news.wav   (~202 s, 118 BPM, D minor pulse)
       assets/sfx/wind.wav       (12 s seamless loop, for while the plane is flying)
       assets/sfx/thud.wav       (landing)
       assets/sfx/lift.wav       (takeoff)
"""
import os
import wave

import numpy as np

RATE = 44100
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sfx")
rng = np.random.default_rng(2026)


def write(path, stereo, peak_db=None):
    x = np.asarray(stereo, dtype=np.float64)
    if peak_db is not None:
        peak = max(np.max(np.abs(x)), 1e-9)
        x = x * (10 ** (peak_db / 20.0)) / peak
    x = np.clip(x, -1.0, 1.0)
    pcm = (x * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"{os.path.basename(path):16s} {len(pcm) / RATE:7.2f} s")


# ---------------------------------------------------------------- voices

def t_of(n):
    return np.arange(n) / RATE


def sine(freq, dur, amp=1.0):
    n = int(dur * RATE)
    return amp * np.sin(2 * np.pi * freq * t_of(n))


def saw(freq, dur, harmonic_rolloff=0.75, harmonics=9):
    """Band-limited-ish saw: a stack of sines, the read of a synth brass/string stab."""
    n = int(dur * RATE)
    t = t_of(n)
    x = np.zeros(n)
    for k in range(1, harmonics + 1):
        if k * freq > RATE / 2.2:
            break
        x += (harmonic_rolloff ** (k - 1)) * np.sin(2 * np.pi * k * freq * t + 0.3 * k) / k
    return x


def decay_env(n, attack, tau):
    t = t_of(n)
    return np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-np.maximum(t - attack, 0) / tau)


def band_noise(n, lo, hi, seed=1):
    """White noise shaped to [lo, hi] in the frequency domain (fast, no per-sample loop)."""
    r = np.random.default_rng(seed)
    x = r.normal(0, 1, n)
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1 / RATE)
    mask = ((f >= lo) & (f <= hi)).astype(float)
    # soft shoulders so the band does not ring
    mask = np.convolve(mask, np.ones(9) / 9, mode="same")
    y = np.fft.irfft(X * mask, n)
    return y / (np.max(np.abs(y)) + 1e-9)


def kick(dur=0.34):
    n = int(dur * RATE)
    t = t_of(n)
    f = 120 * np.exp(-t / 0.045) + 46          # pitch drop: the "news sting" thump
    body = np.sin(2 * np.pi * np.cumsum(f) / RATE)
    click = band_noise(n, 1500, 7000, seed=int(dur * 1000)) * np.exp(-t / 0.006) * 0.5
    return np.tanh(1.8 * (body * decay_env(n, 0.002, 0.16) + click))


def hat(dur=0.05, seed=3):
    n = int(dur * RATE)
    return band_noise(n, 6000, 15000, seed=seed) * decay_env(n, 0.001, 0.014)


def stab(freqs, dur=0.5):
    n = int(dur * RATE)
    x = np.zeros(n)
    for f in freqs:
        x += saw(f, dur, 0.62, 7)
    x = np.tanh(1.3 * x / len(freqs))
    return x * decay_env(n, 0.004, 0.19)


def bass_note(freq, dur=0.42):
    n = int(dur * RATE)
    x = sine(freq, dur) + 0.45 * sine(freq * 2, dur) + 0.2 * sine(freq * 3, dur)
    return np.tanh(2.2 * x) * decay_env(n, 0.004, 0.20)


# ---------------------------------------------------------------- bed

def make_bed(path, total=202.5, bpm=118.0):
    beat = 60.0 / bpm
    bar = beat * 4
    # D minor, one chord per bar: i, i, VI, V -- the tension of a news sting
    chords = [
        (73.42, [220.00, 261.63, 293.66]),     # Dm
        (73.42, [220.00, 261.63, 293.66]),     # Dm
        (116.54, [233.08, 293.66, 349.23]),    # Bb
        (110.00, [220.00, 277.18, 329.63]),    # A
    ]
    n = int(total * RATE)
    L = np.zeros(n)
    R = np.zeros(n)

    def add(sig, at, pan=0.0, gain=1.0):
        a = int(at * RATE)
        if a >= n:
            return
        seg = sig[: n - a] * gain
        L[a:a + len(seg)] += seg * np.sqrt(0.5 * (1 + pan))
        R[a:a + len(seg)] += seg * np.sqrt(0.5 * (1 - pan))

    bars = int(np.ceil(total / bar))
    for b in range(bars):
        t0 = b * bar
        root, tones = chords[b % 4]
        section_air = 1.0 + 0.25 * np.sin(2 * np.pi * b / 16.0)

        # sub/bass: 8th notes on the root, the engine of the thing
        for i in range(8):
            at = t0 + i * beat / 2
            acc = 1.0 if i % 2 == 0 else 0.72
            add(bass_note(root, beat * 0.55), at, 0.0, 0.55 * acc * section_air)

        # kick on 1 and 3, ghost on the "and" of 4
        for at, g in ((0.0, 0.9), (2 * beat, 0.75), (3 * beat + beat / 2, 0.4)):
            add(kick(), t0 + at, 0.0, g)

        # 16th hats, accented on the beat, dropped on the last bar of the loop
        for i in range(16):
            if b % 4 == 3 and i % 4 == 2:
                continue
            at = t0 + i * beat / 4
            acc = 0.85 if i % 4 == 0 else (0.5 if i % 2 == 0 else 0.3)
            add(hat(seed=i + b), at, 0.28 if i % 2 else -0.28, acc * 0.30)

        # brass-ish stab on beat 3: the accusation
        if b % 4 in (1, 3):
            add(stab([f * 2 for f in tones], 0.55), t0 + 2 * beat, 0.0, 0.42)
        # and a longer one that opens each 8-bar phrase
        if b % 8 == 0:
            add(stab([f * 2 for f in tones], 1.1), t0, 0.0, 0.5)
            add(stab([f for f in tones], 1.6), t0, 0.0, 0.22)

        # riser into every 8-bar phrase
        if b % 8 == 7:
            rn = int(bar * RATE)
            rise = band_noise(rn, 700, 9000, seed=b) * np.linspace(0.05, 1.0, rn) ** 2
            add(rise, t0 + bar * 0.35, 0.0, 0.30)

    # a low drone under everything, breathing slowly
    dn = n
    tt = t_of(dn)
    drone = 0.35 * np.sin(2 * np.pi * 36.7 * tt) + 0.2 * np.sin(2 * np.pi * 55.0 * tt + 0.9)
    breathe = 0.55 + 0.45 * np.sin(2 * np.pi * tt / 34.0)
    L += drone * breathe * 0.45
    R += drone * breathe * 0.45

    # shape: fade in, fade out over the last 4 s
    gain = np.minimum(1.0, tt / 1.0) * np.clip((total - tt) / 4.0, 0, 1)
    L *= gain
    R *= gain
    write(path, np.stack([L, R], 1), peak_db=-12.0)


# ---------------------------------------------------------------- flight / landing

def make_wind(path, dur=12.0):
    """Seamless loop: air moving past the fuselage, with a slow swell."""
    n = int(dur * RATE) + int(0.6 * RATE)
    t = t_of(n)
    base = band_noise(n, 180, 2600, seed=41)
    whistle = band_noise(n, 2400, 5200, seed=42) * 0.45
    rumble = band_noise(n, 55, 160, seed=43) * 0.85     # the airframe, felt more than heard
    swell = 0.55 + 0.45 * np.sin(2 * np.pi * t / 3.6)
    gust = 0.75 + 0.25 * np.sin(2 * np.pi * t / 1.15 + 1.2)
    x = (base * 0.8 + whistle + rumble) * swell * gust
    tail = int(0.6 * RATE)
    x[:tail] = x[:tail] * np.linspace(0, 1, tail) + x[-tail:] * np.linspace(1, 0, tail)
    x = x[:-tail]
    x /= max(np.max(np.abs(x)), 1e-9)
    stereo = np.stack([x, np.roll(x, 700)], 1)
    write(path, stereo, peak_db=-9.0)


def make_thud(path):
    """Touchdown: a soft body thump plus the tyre chirp."""
    n = int(0.5 * RATE)
    t = t_of(n)
    body = np.sin(2 * np.pi * np.cumsum(90 * np.exp(-t / 0.09) + 42) / RATE) * decay_env(n, 0.002, 0.14)
    chirp = band_noise(n, 1200, 5000, seed=77) * decay_env(n, 0.008, 0.05) * 0.35
    x = np.tanh(1.6 * (body + chirp))
    write(path, np.stack([x, np.roll(x, 90)], 1), peak_db=-12.0)


def make_lift(path):
    """Takeoff: a short upward air sweep."""
    n = int(0.9 * RATE)
    t = t_of(n)
    sweep = band_noise(n, 250, 6000, seed=88) * (np.linspace(0.15, 1.0, n) ** 1.6) * np.exp(-np.maximum(t - 0.45, 0) / 0.22)
    sub = 0.35 * np.sin(2 * np.pi * np.cumsum(np.linspace(70, 110, n)) / RATE)
    x = np.tanh(1.2 * (sweep * 1.4 + sub * decay_env(n, 0.02, 0.3)))
    write(path, np.stack([x, np.roll(x, 300)], 1), peak_db=-11.0)


def main():
    os.makedirs(OUT, exist_ok=True)
    make_bed(os.path.join(OUT, "bed_news.wav"))
    make_wind(os.path.join(OUT, "wind.wav"))
    make_thud(os.path.join(OUT, "thud.wav"))
    make_lift(os.path.join(OUT, "lift.wav"))


if __name__ == "__main__":
    main()
