"""Batch render a composition's voiceover with the local Qwen3-TTS clone.

Run inside the qwen3tts conda env, from anywhere:

    C:/Users/aryan/miniconda3/envs/qwen3tts/python.exe tools/vo_batch.py \
        assets/vo/<slug>/lines.json assets/vo/<slug>

One model load, one clone prompt, then one take per line. Every take goes through
audio_guard: trim to the recognised word bounds, drop long stalls, normalise loudness, and
compare the ASR transcript with the requested text; a take below `--min-score` is retried
with another seed. `atempo` slows the delivery without changing pitch, then ffmpeg writes a
48 kHz mono WAV that media::audio can decode.
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np
import torch

QWEN_DIR = os.environ.get("QWEN_TTS_DIR", r"D:/code/qwen3tts")
sys.path.insert(0, QWEN_DIR)
import audio_guard as ag  # noqa: E402
from qwen_tts import Qwen3TTSModel  # noqa: E402

MODEL_DIR = os.environ.get("QWEN_MODEL", r"D:/toolcache/qwen3-tts-1.7b-base")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lines", help="JSON with ref, ref_text, language, atempo and lines[]")
    ap.add_argument("outdir")
    ap.add_argument("--work", default="out/vo-work")
    ap.add_argument("--min-score", type=float, default=0.92)
    ap.add_argument("--attempts", type=int, default=2)
    ap.add_argument("--only", default="", help="comma-separated file names to (re)generate")
    args = ap.parse_args()

    with open(args.lines, encoding="utf-8") as f:
        spec = json.load(f)
    only = {s.strip() for s in args.only.split(",") if s.strip()}
    if only:
        spec["lines"] = [l for l in spec["lines"] if l["file"] in only]
    os.makedirs(args.outdir, exist_ok=True)
    os.makedirs(args.work, exist_ok=True)

    print("loading model ...", flush=True)
    model = Qwen3TTSModel.from_pretrained(
        MODEL_DIR, device_map="cuda:0", dtype=torch.bfloat16, attn_implementation="sdpa")
    ref, ref_sr = ag.read_wav(spec["ref"])
    prompt = model.create_voice_clone_prompt(
        ref_audio=(ref, ref_sr), ref_text=spec["ref_text"], x_vector_only_mode=False)
    language = spec.get("language", "English")
    atempo = float(spec.get("atempo", 1.0))

    total = 0.0
    for i, line in enumerate(spec["lines"]):
        text, name = line["text"], line["file"]
        stem = os.path.splitext(name)[0]
        gen = os.path.join(args.work, stem + ".gen.wav")
        clean = os.path.join(args.work, stem + ".clean.wav")
        best = None
        for attempt in range(args.attempts):
            seed = 7 + 17 * i + 31 * attempt
            torch.manual_seed(seed)
            wavs, sr = model.generate_voice_clone(
                text=text, language=language, voice_clone_prompt=prompt)
            arr = np.asarray(wavs[0]).flatten().astype(np.float32)
            ag.write_wav(gen, np.clip(arr, -1, 1), sr)
            _, asr = ag.words(gen)
            score = ag.wording_score(asr, text)
            trimmed, onset, stalls = ag.tighten(arr, sr, gen, head_pad=1.0)
            print(f"{name}: seed {seed} wording {score} stalls {stalls} "
                  f"spoken {len(ag.norm_words(asr))}/{len(ag.norm_words(text))}", flush=True)
            if best is None or score > best[0]:
                best = (score, trimmed, sr)
            if score >= args.min_score and stalls == 0:
                break
        score, arr, sr = best
        arr, _ = ag.normalise_loudness(arr)
        ag.write_wav(clean, arr, sr)

        out = os.path.join(args.outdir, name)
        subprocess.run([
            "ffmpeg", "-y", "-loglevel", "error", "-i", clean,
            "-filter:a", f"atempo={atempo}", "-ar", "48000", "-ac", "1", out,
        ], check=True)
        dur = len(arr) / sr / atempo
        total += dur
        print(f"  -> {name}: {dur:.2f} s, wording {score}", flush=True)

    print(f"total speech: {total:.1f} s for {len(spec['lines'])} lines", flush=True)


if __name__ == "__main__":
    main()
