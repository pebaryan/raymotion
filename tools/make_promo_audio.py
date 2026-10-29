"""make_promo_audio.py -- BGM bed and hit SFX for "trip_promo".

Everything is synthesised here (numpy, 44.1 kHz, 16-bit stereo WAV) so the promo carries no
third-party audio: a slow Am-F-C-G arpeggio bed for the globe, plus four one-shots
(pop / whoosh / boom / chime) placed on the timeline in trip_promo.cpp.

    python tools/make_promo_audio.py            # writes assets/sfx/*.wav
"""
import os
import wave

import numpy as np

RATE = 44100
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sfx")


def write(path, stereo):
    x = np.clip(stereo, -1.0, 1.0)
    pcm = (x * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"{os.path.basename(path):16s} {len(pcm) / RATE:7.2f} s")


def env(n, attack, decay, hold=0.0):
    """attack (s) linear in, hold, then exponential decay to zero over the rest."""
    t = np.arange(n) / RATE
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    d = np.exp(-np.maximum(t - attack - hold, 0) / max(decay, 1e-4))
    return a * d


def tone(freq, dur, attack=0.01, decay=0.5, partials=((1, 1.0), (2, 0.25), (3, 0.1))):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    x = np.zeros(n)
    for mult, amp in partials:
        x += amp * np.sin(2 * np.pi * freq * mult * t + 0.37 * mult)
    return x * env(n, attack, decay)


def noise_sweep(dur, f0, f1, q=2.5):
    """Noise through a moving one-pole band: cheap but reads as air rather than hiss."""
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    src = np.random.default_rng(7).normal(0, 1, n)
    cut = np.linspace(f0, f1, n) / (RATE / 2)
    y = np.zeros(n)
    z1 = z2 = 0.0
    for i in range(n):
        k = cut[i]
        # one-pole lowpass followed by one-pole highpass at the same cut: a band
        z1 += k * (src[i] - z1)
        z2 += k * (z1 - z2)
        y[i] = z1 - z2
    return y / (np.max(np.abs(y)) + 1e-9) * np.minimum(1.0, t / (dur * 0.35)) * np.exp(-np.maximum(t - dur * 0.4, 0) / (dur * 0.32))


def pad(freqs, dur):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    x = np.zeros(n)
    for f in freqs:
        x += np.sin(2 * np.pi * f * t) + 0.6 * np.sin(2 * np.pi * (f * 1.003) * t + 1.1)
    a = np.minimum(1.0, t / 0.9)
    rel = np.minimum(1.0, (dur - t) / 0.9)
    return x * a * rel / len(freqs)


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = np.random.default_rng(11)

    # ---------------------------------------------------------------- one-shots
    # pop: a short blip, used per intro word (varying playbackRate in C++)
    pop = tone(760, 0.16, 0.004, 0.055, ((1, 1.0), (2, 0.3), (4, 0.08)))
    write(os.path.join(OUT, "pop.wav"), np.stack([pop, pop], 1))

    # whoosh: air swept up and away, for scene transitions and the logo draw
    wh = noise_sweep(0.85, 300, 5200)
    wh = wh + 0.35 * tone(120, 0.85, 0.02, 0.35)
    write(os.path.join(OUT, "whoosh.wav"), np.stack([wh, np.roll(wh, 260)], 1))

    # boom: the arrival hit when the globe takes over and on the end card
    body = 0.9 * tone(52, 1.4, 0.006, 0.45, ((1, 1.0), (2, 0.22)))
    body[: len(noise_sweep(0.4, 900, 90))] += 0.25 * noise_sweep(0.4, 900, 90)
    bo = body
    write(os.path.join(OUT, "boom.wav"), np.stack([bo, bo], 1))

    # chime: A5 C#6 E6, the end card's three bells
    n = int(2.2 * RATE)
    ch = np.zeros(n)
    for i, f in enumerate((880.0, 1108.73, 1318.51)):
        seg = tone(f, 1.8, 0.005, 0.55 - 0.1 * i, ((1, 1.0), (2.76, 0.4), (5.4, 0.14)))
        off = int(i * 0.16 * RATE)
        ch[off:off + len(seg)] += seg * (0.9 - 0.15 * i)
    write(os.path.join(OUT, "chime.wav"), np.stack([ch, np.roll(ch, 180)], 1))

    # ---------------------------------------------------------------- BGM bed
    # 76 BPM, one bar = 4 beats = 3.1579 s, Am | F | C | G, 8th-note arpeggio, soft pad,
    # bass on 1 and 3. Long enough to sit under 202 s of picture; the mix ducks it in C++.
    bpm, beats = 76.0, 4
    beat = 60.0 / bpm
    bar = beat * beats
    chords = [
        (220.00, [220.00, 261.63, 329.63]),   # Am
        (174.61, [174.61, 220.00, 261.63]),   # F
        (130.81, [261.63, 329.63, 392.00]),   # C
        (196.00, [196.00, 246.94, 293.66]),   # G
    ]
    total = 202.5
    n = int(total * RATE)
    L = np.zeros(n)
    R = np.zeros(n)

    def add(sig, at, pan):
        a = int(at * RATE)
        if a >= n:
            return
        seg = sig[: n - a]
        gl, gr = np.sqrt(0.5 * (1 + pan)), np.sqrt(0.5 * (1 - pan))
        L[a:a + len(seg)] += seg * gl
        R[a:a + len(seg)] += seg * gr

    # pad: one chord per bar, voiced low and wide
    t = 0.0
    while t < total:
        for root, tones in chords:
            if t >= total:
                break
            add(pad([root / 2, tones[1], tones[2]], bar * 1.05), t, 0.0)
            t += bar
        if t >= total:
            break

    # bass: root on beats 1 and 3, an octave down
    t = 0.0
    while t < total:
        for root, tones in chords:
            for b in (0, 2):
                at = t + b * beat
                if at >= total:
                    break
                add(0.55 * tone(root / 4, beat * 1.6, 0.02, 0.55, ((1, 1.0), (2, 0.15))), at, 0.0)
            t += bar

    # arpeggio: chord tones on every 8th, alternating pan, plucked
    t = 0.0
    k = 0
    while t < total:
        for root, tones in chords:
            for i in range(beats * 2):
                at = t + i * beat / 2
                if at >= total:
                    break
                f = tones[[0, 1, 2, 1][i % 4]] * 2
                add(0.22 * tone(f, 0.55, 0.004, 0.16, ((1, 1.0), (3, 0.12))), at, 0.35 if k % 2 else -0.35)
                k += 1
            t += bar

    # air: a very quiet, slowly breathing band of noise for glue
    air = np.zeros(n)
    a_n = int(total * RATE)
    src = np.random.default_rng(3).normal(0, 1, a_n)
    z = 0.0
    for i in range(a_n):
        z += 0.05 * (src[i] - z)
        air[i] = z
    air /= max(np.max(np.abs(air)), 1e-9) / 0.05
    breathe = 0.5 + 0.5 * np.sin(2 * np.pi * np.arange(a_n) / RATE / 21.0)
    L += air * breathe
    R += air * breathe

    # gentle overall shaping: fade in over 1.2 s, out over the last 4 s, then normalise to -14 dBFS
    t = np.arange(n) / RATE
    gain = np.minimum(1.0, t / 1.2) * np.clip((total - t) / 4.0, 0, 1)
    L *= gain
    R *= gain
    peak = max(np.max(np.abs(L)), np.max(np.abs(R)), 1e-9)
    k = (10 ** (-14 / 20)) / peak
    write(os.path.join(OUT, "bgm.wav"), np.stack([L * k, R * k], 1))


if __name__ == "__main__":
    main()
