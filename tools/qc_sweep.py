#!/usr/bin/env python3
"""Gate 2c picture-integrity sweep for an encoded film.

Reads the MP4 once at low resolution and reports what a still-at-chapter-points review
misses (see skills/raymotion/references/quality-control.md, Gate 2c):

  * flat frames   - near-uniform pictures (camera inside a wall or under the ground,
                    a missing backdrop, an unintended black);
  * shake         - the whole picture jitters back and forth faster than ~1.5 Hz
                    (typically a sine fed frames where it expected seconds);
  * pops          - one frame differs from its neighbours far more than the motion
                    around it (a hard cut in a one-shot, an object popping in);
  * freezes       - the picture stops changing for longer than --freeze seconds.

It also writes a frame every --step seconds at review size into --out, one file per
time, so every interval can be opened and given a written note. The report is
Markdown to paste into the QC review.

Every flag must be explained in the QC review (intended fade, designed hold, ...) or
fixed. A clean report is necessary, not sufficient: the frames still have to be looked at.

    python tools/qc_sweep.py out/<slug>.mp4 --out out/qc/<slug>-sweep
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np


def probe(path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
         "stream=width,height,r_frame_rate:format=duration", "-of", "json", path],
        capture_output=True, text=True, check=True).stdout
    j = json.loads(out)
    s = j["streams"][0]
    num, den = s["r_frame_rate"].split("/")
    return int(s["width"]), int(s["height"]), float(num) / float(den), float(j["format"]["duration"])


def frames(path, w, h):
    """Yields grey frames, w x h, as float arrays."""
    p = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-i", path, "-vf", f"scale={w}:{h},format=gray",
         "-f", "rawvideo", "-"], stdout=subprocess.PIPE)
    size = w * h
    while True:
        buf = p.stdout.read(size)
        if len(buf) < size:
            break
        yield np.frombuffer(buf, np.uint8).reshape(h, w).astype(np.float32)
    p.wait()


def spectrum(a, win):
    return np.fft.rfft2((a - a.mean()) * win)


def shift(fa, fb, shape):
    """Global (dx, dy) of frame b against frame a (their spectra), by phase correlation."""
    r = fa * np.conj(fb)
    r /= np.abs(r) + 1e-9
    c = np.fft.irfft2(r, s=shape)
    y, x = np.unravel_index(np.argmax(c), c.shape)
    h, w = c.shape
    dy = y - h if y > h // 2 else y
    dx = x - w if x > w // 2 else x
    return -dx, -dy, c.max()


def ranges(flags, fps, min_len=1):
    """Turns per-frame booleans into [(first, last)] frame runs."""
    out, start = [], None
    for i, f in enumerate(list(flags) + [False]):
        if f and start is None:
            start = i
        if not f and start is not None:
            if i - start >= min_len:
                out.append((start, i - 1))
            start = None
    return out


def merge(runs, gap):
    """Joins runs separated by fewer than `gap` frames."""
    out = []
    for s, e in runs:
        if out and s - out[-1][1] <= gap:
            out[-1] = (out[-1][0], e)
        else:
            out.append((s, e))
    return out


def tc(frame, fps):
    s = frame / fps
    return f"{int(s // 60)}:{s % 60:05.2f} (f{frame})"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("video")
    ap.add_argument("--out", help="folder for the review frames (one every --step s)")
    ap.add_argument("--step", type=float, default=1.0, help="seconds between review frames (default 1)")
    ap.add_argument("--review-height", type=int, default=960, help="review frame height (default 960)")
    ap.add_argument("--flat", type=float, default=6.0, help="luma std-dev below which a frame is flat (default 6)")
    ap.add_argument("--freeze", type=float, default=1.5, help="seconds without change that count as a freeze")
    ap.add_argument("--report", help="also write the Markdown report here")
    a = ap.parse_args()

    W, H, fps, dur = probe(a.video)
    # Analysis size: 320 px on the long side keeps phase correlation fast and robust.
    scale = 320 / max(W, H)
    aw, ah = max(16, int(W * scale) // 2 * 2), max(16, int(H * scale) // 2 * 2)
    px = W / aw  # one analysis pixel in output pixels
    win = np.outer(np.hanning(ah), np.hanning(aw)).astype(np.float32)

    std, diff, dx, dy, conf = [], [], [], [], []
    prev = prev_spec = None
    for f in frames(a.video, aw, ah):
        std.append(float(f.std()))
        spec = spectrum(f, win)
        if prev is None:
            diff.append(0.0); dx.append(0); dy.append(0); conf.append(1.0)
        else:
            diff.append(float(np.abs(f - prev).mean()))
            x, y, c = shift(prev_spec, spec, f.shape)
            dx.append(x); dy.append(y); conf.append(float(c))
        prev, prev_spec = f, spec
    n = len(std)
    std, diff = np.array(std), np.array(diff)
    dx, dy, conf = np.array(dx), np.array(dy), np.array(conf)

    lines = [f"## Gate 2c sweep: `{os.path.basename(a.video)}`", "",
             f"{W}x{H} @ {fps:g} fps, {n} frames ({n / fps:.2f} s; container says {dur:.2f} s). "
             f"Analysis at {aw}x{ah}: 1 px = {px:.1f} output px.", ""]
    problems = 0

    # Flat frames.
    flat = merge(ranges(std < a.flat, fps), int(0.5 * fps))
    lines.append(f"### Flat frames (luma std < {a.flat:g})")
    if not flat:
        lines.append("none")
    for s, e in flat:
        problems += 1
        lines.append(f"- {tc(s, fps)} - {tc(e, fps)}: std {std[s:e + 1].min():.1f}; "
                     "explain (designed fade/black) or fix (camera inside geometry, missing backdrop)")
    lines.append("")

    # Shake: in each 1 s window, count direction reversals of the global shift.
    # A slow designed sway reverses at most ~1x a second; a units bug reverses 4-10x.
    lines.append("### Shake (global shift reversing > 3x in 1 s)")
    win_n = max(2, int(round(fps)))
    shaky = np.zeros(n, bool)
    for axis, s in (("x", dx), ("y", dy)):
        ok = conf > 0.08  # ignore frames where the correlation found no clear match
        v = np.where(ok, s, 0)
        sign = np.sign(v)
        for i in range(0, n - win_n):
            seg = sign[i:i + win_n]
            nz = seg[seg != 0]
            flips = int(np.sum(nz[1:] != nz[:-1])) if len(nz) > 1 else 0
            if flips > 3 and np.abs(v[i:i + win_n]).max() >= 1:
                shaky[i:i + win_n] = True
    shake = ranges(shaky, fps)
    if not shake:
        lines.append("none")
    for s, e in shake:
        problems += 1
        seg = np.concatenate([dx[s:e + 1], dy[s:e + 1]])
        lines.append(f"- {tc(s, fps)} - {tc(e, fps)}: up to {np.abs(seg).max() * px:.0f} output px a frame; "
                     "check every sin/cos/noise clock is in seconds (frameTime(t) is frames)")
    lines.append("")

    # Pops: a frame-to-frame change far above the local motion level.
    lines.append("### Pops (a frame change > 4x the local median and > 6 grey levels)")
    pops = []
    for i in range(1, n):
        lo, hi = max(1, i - int(fps)), min(n, i + int(fps) + 1)
        local = np.median(diff[lo:hi])
        if diff[i] > 6 and diff[i] > 4 * max(local, 0.5):
            pops.append(i)
    popf = np.zeros(n, bool)
    popf[pops] = True
    pop_runs = merge(ranges(popf, fps), int(0.25 * fps))
    if not pop_runs:
        lines.append("none")
    for s, e in pop_runs:
        problems += 1
        lines.append(f"- {tc(s, fps)} - {tc(e, fps)}: change up to {diff[s:e + 1].max():.1f}; "
                     "a cut, a pop-in, a camera jump or a parameter jump - explain or fix")
    lines.append("")

    # Freezes.
    lines.append(f"### Freezes (no visible change for > {a.freeze:g} s)")
    frz = ranges(diff < 0.15, fps, min_len=int(a.freeze * fps))
    frz = [(s, e) for s, e in frz if s > 0]
    if not frz:
        lines.append("none")
    for s, e in frz:
        problems += 1
        lines.append(f"- {tc(s, fps)} - {tc(e, fps)}: explain (designed hold) or fix (a stalled key, a dead `up`)")
    lines.append("")

    # Motion profile, per second: lets a reader see stalls, rushes and dead bands.
    lines.append("### Motion per second (mean frame change, grey levels)")
    per = [diff[int(s * fps):int((s + 1) * fps)].mean() for s in range(int(n / fps))]
    lines.append("```")
    for s0 in range(0, len(per), 10):
        lines.append(f"{s0:4d}s " + " ".join(f"{v:5.1f}" for v in per[s0:s0 + 10]))
    lines.append("```")
    lines.append("")

    # Review frames.
    if a.out:
        os.makedirs(a.out, exist_ok=True)
        rh = min(a.review_height, H)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", a.video, "-vf",
                        f"fps=1/{a.step},scale=-2:{rh}", os.path.join(a.out, "t%04d.png")], check=True)
        count = len([f for f in os.listdir(a.out) if f.startswith("t") and f.endswith(".png")])
        lines.append(f"### Review frames")
        lines.append(f"{count} frames in `{a.out}` (t0001 = 0 s, one every {a.step:g} s). Open every one "
                     "and write one line per frame in the QC sweep table; a contact sheet is not a substitute.")
        lines.append("")

    lines.append(f"**{problems} flag(s).** Each needs a fix or a written reason in the QC review.")
    text = "\n".join(lines)
    print(text)
    if a.report:
        with open(a.report, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
