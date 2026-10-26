"""Generate the original sound effects for the flag showcase (44.1 kHz mono WAV)."""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
OUT = Path(__file__).resolve().parents[1] / "assets" / "audio" / "flags"
OUT.mkdir(parents=True, exist_ok=True)


def write(name, duration, synth):
    rng = random.Random(4926)
    samples = []
    for i in range(round(duration * RATE)):
        t = i / RATE
        v = max(-1.0, min(1.0, synth(t, duration, rng)))
        samples.append(round(v * 32767))
    with wave.open(str(OUT / f"{name}.wav"), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(struct.pack("<" + "h" * len(samples), *samples))


def pop(t, duration, rng):
    env = math.exp(-27 * t)
    pitch = 670 - 360 * t / duration
    tone = math.sin(2 * math.pi * (pitch * t + 30 * t * t))
    return 0.62 * env * tone + 0.12 * math.exp(-95 * t) * rng.uniform(-1, 1)


def whoosh(t, duration, rng):
    u = t / duration
    env = math.sin(math.pi * u) ** 1.5
    flutter = math.sin(2 * math.pi * (170 * t + 110 * t * t))
    return env * (0.26 * rng.uniform(-1, 1) + 0.13 * flutter)


def impact(t, duration, rng):
    thud = math.sin(2 * math.pi * (85 * t - 24 * t * t)) * math.exp(-10 * t)
    tick = rng.uniform(-1, 1) * math.exp(-45 * t)
    return 0.56 * thud + 0.26 * tick


write("pop", 0.22, pop)
write("whoosh", 0.48, whoosh)
write("impact", 0.62, impact)
