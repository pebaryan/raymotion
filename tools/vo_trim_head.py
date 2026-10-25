"""Cut the reference-prompt leak from the start of voice-over takes.

Qwen3-TTS zero-shot clones often begin with the last syllables of the reference clip (with
the default prompt "...co simulation", every take opens with "-mulation") before the line
itself. Transcribing a whole take hides it: ASR drops the fragment and reports the first
real word late. Transcribing only the head (the first seconds, padded with silence) does
hear it, so this:

  1. transcribes the head of the original take with word timings;
  2. finds the line's first word; anything heard before it is the leak;
  3. cuts at the quietest moment between the leak's last word and the first real word,
     with a short fade-in;
  4. checks the head again (it must now start with the line's first word) and the whole
     take (its wording score against the script must not drop), or leaves the take alone.

    C:/Users/aryan/miniconda3/envs/qwen3tts/python.exe tools/vo_trim_head.py \
        assets/vo/sparql/lines.json assets/vo/sparql

Originals are kept as <name>.untrimmed.wav under --backup (default out/vo-untrimmed); an
existing backup is never overwritten, so a rerun always starts from the originals. Delete a
line's backup after regenerating that line.
"""
import argparse
import json
import os
import shutil
import sys

import numpy as np

sys.path.insert(0, os.environ.get("QWEN_TTS_DIR", r"D:/code/qwen3tts"))
import audio_guard as ag  # noqa: E402

HEADS, PAD = (1.5, 1.0, 2.0), 0.3   # seconds transcribed (ASR hears the leak in some, not all), padding


def head_words(arr, sr, tmp, head=HEADS[0]):
    """Words heard in the first `head` seconds, with times in the take's own seconds."""
    pad = np.zeros(int(PAD * sr), dtype=arr.dtype)
    ag.write_wav(tmp, np.concatenate([pad, arr[:int(head * sr)], pad]), sr)
    ws, _ = ag.words(tmp)
    return [(ag.norm_words(w["w"]), float(w["start"]) - PAD, float(w["end"]) - PAD) for w in ws]


def quietest(arr, sr, a, b):
    """Sample of the quietest 10 ms window in [a, b] seconds."""
    win = max(1, int(0.01 * sr))
    lo, hi = max(0, int(a * sr)), max(int(a * sr) + win, int(b * sr))
    best, at = None, lo
    for s in range(lo, min(hi, len(arr) - win), win):
        e = float(np.mean(arr[s:s + win] ** 2))
        if best is None or e < best:
            best, at = e, s
    return at


def dips(arr, sr, a, b, floor_db=-26.0):
    """Local minima of 10 ms energy in [a, b] seconds quieter than floor_db, latest first."""
    win = max(1, int(0.01 * sr))
    lo, hi = int(a * sr) // win, int(b * sr) // win
    db = [20 * np.log10(np.sqrt(np.mean(arr[i * win:(i + 1) * win] ** 2)) + 1e-9) for i in range(lo, hi + 1)]
    out = [(lo + i) * win for i in range(1, len(db) - 1) if db[i] <= db[i - 1] and db[i] <= db[i + 1] and db[i] < floor_db]
    return out[::-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lines", help="the lines.json the takes were made from")
    ap.add_argument("dir", help="folder with the takes")
    ap.add_argument("--backup", default="out/vo-untrimmed")
    args = ap.parse_args()
    with open(args.lines, encoding="utf-8") as fh:
        spec = json.load(fh)
    os.makedirs(args.backup, exist_ok=True)
    tmp = os.path.join(args.backup, "_head.wav")
    for line in spec["lines"]:
        name = line["file"]
        f = os.path.join(args.dir, name)
        first = ag.norm_words(line["text"])[0]
        backup = os.path.join(args.backup, os.path.splitext(name)[0] + ".untrimmed.wav")
        if not os.path.exists(backup):
            shutil.copyfile(f, backup)
        arr, sr = ag.read_wav(backup)
        for head in HEADS:
            heard = head_words(arr, sr, tmp, head)
            k = next((i for i, (w, _, _) in enumerate(heard) if w and w[0] == first), None)
            if k:
                break
        leak = " ".join(" ".join(w) for w, _, _ in heard[:k or 0])
        if k is None or k == 0:
            shutil.copyfile(backup, f)
            print(f"{name}: {'no leak' if k == 0 else 'first word not found'}, left as generated")
            continue
        # The quietest moment between the leak and the first word, then earlier dips (ASR can
        # place the leak's end late, so the best cut may come before it).
        cuts = [quietest(arr, sr, heard[k - 1][2], max(heard[k][1], heard[k - 1][2] + 0.02))]
        cuts += [c for c in dips(arr, sr, 0.1, heard[k - 1][2]) if c < cuts[0]]
        _, whole0 = ag.words(backup)
        s0 = ag.wording_score(whole0, line["text"])
        done = False
        for start in cuts:
            out = arr[start:].copy()
            fade = min(len(out), int(0.012 * sr))
            out[:fade] *= np.linspace(0, 1, fade)
            after = head_words(out, sr, tmp)
            for head in HEADS[1:]:          # it must be gone at every head length
                again = head_words(out, sr, tmp, head)
                if again and again[0][0] and again[0][0][0] != first:
                    after = again
                    break
            if not after or not after[0][0] or after[0][0][0] != first:
                print(f"{name}: cut at {start / sr:.2f}s rejected (head now: {' '.join(' '.join(w) for w, _, _ in after)})")
                continue
            ag.write_wav(tmp, out, sr)
            _, whole = ag.words(tmp)
            s1 = ag.wording_score(whole, line["text"])
            if s1 < s0 - 0.02:
                print(f"{name}: cut at {start / sr:.2f}s rejected (score {s0:.3f} -> {s1:.3f})")
                continue
            ag.write_wav(f, out, sr)
            print(f"{name}: removed '{leak}' ({start / sr:.2f}s), now starts '{' '.join(' '.join(w) for w, _, _ in after[:3])}', score {s0:.3f} -> {s1:.3f}")
            done = True
            break
        if not done:
            shutil.copyfile(backup, f)
            print(f"{name}: no clean cut, left as generated")
    if os.path.exists(tmp):
        os.remove(tmp)


if __name__ == "__main__":
    main()
