"""Generate the original Austria time-travel effects with only Python's standard library."""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
OUT = Path(__file__).resolve().parents[1] / "assets" / "audio" / "austria"
OUT.mkdir(parents=True, exist_ok=True)


def write(name, duration, fn):
    rng = random.Random(1026)
    pcm = []
    for i in range(round(duration * RATE)):
        t = i / RATE
        sample = max(-1, min(1, fn(t, duration, rng)))
        pcm.append(round(32767 * sample))
    with wave.open(str(OUT / f"{name}.wav"), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(struct.pack("<" + "h" * len(pcm), *pcm))


def whoosh(t, d, rng):
    u = t / d
    envelope = math.sin(math.pi * u) ** 1.7
    whistle = math.sin(2 * math.pi * (145 * t + 180 * t * t))
    return envelope * (0.25 * rng.uniform(-1, 1) + 0.12 * whistle)


def stamp(t, d, rng):
    thump = math.sin(2 * math.pi * (95 * t - 20 * t * t)) * math.exp(-17 * t)
    snap = rng.uniform(-1, 1) * math.exp(-55 * t)
    return 0.62 * thump + 0.22 * snap


def lowhit(t, d, rng):
    tone = math.sin(2 * math.pi * (72 * t - 12 * t * t)) * math.exp(-5 * t)
    air = rng.uniform(-1, 1) * math.exp(-22 * t)
    return 0.5 * tone + 0.1 * air


def chime(t, d, rng):
    env = math.exp(-5 * t) * min(1, t * 120)
    return env * (0.35 * math.sin(2 * math.pi * 880 * t)
                  + 0.18 * math.sin(2 * math.pi * 1320 * t)
                  + 0.08 * math.sin(2 * math.pi * 1760 * t))


write("whoosh", 0.48, whoosh)
write("stamp", 0.34, stamp)
write("lowhit", 0.75, lowhit)
write("chime", 1.15, chime)
