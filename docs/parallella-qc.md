# parallella — QC review

Film: `out/parallella.mp4` · composition `examples/showcase/parallella.cpp` · brief `docs/parallella-brief.md`
(plus the parts' briefs and QC: `docs/parallella-2012-*.md`, `docs/parallella-sixty-*.md`).

Story mode: **explainer** (vertical). Reviewer: the author agent only. The owner has not watched it.
Verification, Spec-to-screen and Revision are capped at 2, and this review cannot call the film
release-ready.

## 1. Conceit

A $99 board from 2012 runs a real language model, exactly. Its "60 tokens a second" described one matrix
pass, not the model.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | **deviation** | the merge was planned in conversation; `docs/parallella-brief.md` was written after the build |
| 1 Script and cue map | pass | 11 measured WAVs, `words.json` for all 11 (Whisper large-v3) |
| 2 Stills | pass | contact sheet `out/qc/p12/full.png`, then the 85 sweep frames; one fix: the bridge opened on a bare ruler for 0.8 s; `IT RUNS.` now carries the cut |
| 2c Picture sweep | conditional | §5d: 2 flat (2 and 4 frames at cuts), 0 shake, 3 pops (cuts), 12 freezes, all given reasons |
| 3 Draft (muted + sound) | **open** | owner pass pending |
| 4 Final technical QA | pass | §7 |
| 5 Authored test | pass (author) | as the parts; the merge adds one test: chapter 2 starts on chapter 1's result ("IT RUNS.") |
| 6 Craft & coherence | conditional | §3 |
| 7 Packaging | pass | SRT `out/parallella.srt`, cover `out/qc/parallella-cover-180x320.png` (the close), source line on the close, chapter labels in the HUD |

## 3. Scorecard

| # | Criterion | Score | Why |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | does it run? (yes, exactly) -> how fast? (60 was a matrix pass) |
| 2 | Script / voice | 2 | 85 s is long for a reel; line 5 may say "the reference" (caption: "a reference") |
| 3 | Evidence | 3 | every chapter has its object; the twist's bar is labelled as the model that wrote chapter 1's sentence |
| 4 | Timing | 3 | one plan from the WAVs |
| 5 | Sound | 2 | not heard by anyone |
| 6 | Visual system | 3 | one palette; the chapter colour (teal, amber) on the HUD |
| 7 | Chapter differentiation | 3 | 3D, scale, snap, proof, then the ruler's changes |
| 8 | Data & provenance | 3 | the parts' ledgers; the new lines add no claim |
| 9 | Pipeline | 3 | voice fallback; `list` from `/tmp` (85.1 s silent fallback); SRT, score generated |
| 10 | Verification | 2 | cap |
| 11 | Memorability / thumbnail | 2 | the close reads at 180x320; the hook is the stronger cover (owner's call) |
| 12 | Specificity | 3 | |
| | **Authoredness** | **32/36** | |
| 13 | Scenario coherence | 3 | the parts' checks; chapter 2's 15.8 is chapter 1's decoder |
| 14 | Information design | 2 | dense proof panels (as part 1) |
| 15 | Text pacing | 3 | karaoke; headlines for the hook, bridge and close |
| 16 | Mode grammar | 2 | over the reel length default; Gate 0 out of order |
| 17 | Spec-to-screen | 2 | cap |
| 18 | Revision | 2 | cap |
| | **Craft** | **14/18** | |

## 5d. Picture integrity sweep (Gate 2c)

`python tools/qc_sweep.py out/parallella.mp4 --out out/qc/parallella-sweep`: 17 flags.

| Flag | Reason |
| --- | --- |
| flat f956-958 (31.9 s) | the cut into the proof; its chip header fades in |
| flat f1169-1172 (39.0 s) | the cut into the bridge; `IT RUNS.` fades in (was 0.8 s before a fix) |
| pops f127, f372, f1169 | designed cuts: hook -> machine, 3D -> scale, proof -> ruler |
| freezes 16.8-18.6, 18.7-21.1, 23.0-24.4, 29.2-31.8 s | chapter 1 holds, as in `parallella-2012-qc.md` §5d |
| freeze 41.9-44.4 s | `HOW FAST? / 60 TOK/S ON PAPER.` and the line at 60, held to the cut |
| freeze 51.2-53.2 s | 49.4 held, the gap to 60 visible; "The cause was the transfer size" |
| freeze 54.2-55.7 s | 57-59 held; `PROJECTION HELD` |
| freeze 59.8-61.7 s | `ONE MATRIX PASS` on the bar; "not the model" |
| freeze 62.9-65.1 s | 15.8 beside 13.8, the ghost; the whole-model label |
| freezes 70.8-72.5, 72.6-76.0 s | 80.2 held; the "each story 5 tok/s" chip at "waits" |
| freeze 79.1-84.2 s | the close (3 s hold after "ship", then the 10-frame sink) |

Sweep frames, one line each (opened in sheets of 11, `out/qc/p12/fs0-7.png`):

| t | Seen |
| --- | --- |
| 1-4 | sentence typing over the board; `A $99 BOARD / FROM 2012`, then `WROTE THIS.` |
| 5 | cut: the board, caption "It's a Parallella." |
| 6 | dolly; card PARALLELLA · $99 · Kickstarter, 2012 |
| 7-10 | close on the chip; the 16 cores rising, then complete; `16 cores × 32 KB` |
| 11-12 | `= 512 KB of memory, in all` |
| 13-14 | 2D: the cores panel; "A small story model has fifteen million weights:" |
| 15-17 | the square grows; `60.8 MB at full precision (fp32)` |
| 18-22 | `116× too big`, the cores red; the stream pipe |
| 23-27 | the 60.8 MB square; the illustrative weights; the snap to +1 -1 0 +1 0 -1 0 +1 |
| 28-33 | shrunk to `3.8 MB ternary / 16× smaller`; the ternary sample |
| 34-37 | proof: the chip header; panels filling word by word |
| 38-40 | both teal; `24 OF 24 TOKENS IDENTICAL` |
| 41 | cut to chapter 2: `IT RUNS.` over the empty ruler; HUD `02 · HOW FAST?` in amber |
| 42-44 | `HOW FAST?`; `60 TOK/S ON PAPER.`; the dashed line at 60, `PROJECTED 60` |
| 45-49 | ARM bar rising to 13.8 |
| 50-51 | Epiphany bar rising; 49.4 |
| 52-53 | the gauge `8 KB` |
| 54-55 | `1 KB`; the bar at 56.6, 59.3 beside the line |
| 56-57 | band 57-59; `PROJECTION HELD` |
| 58-61 | `ONE MATRIX PASS` on the 57-59 bar |
| 62-64 | the drop: ghost outline, the bar at 27 then 15.8; "the whole model: the one that wrote the sentence" |
| 65-66 | batch: 15.8, then 8 segments stacking |
| 67-68 | 40.2, "8 stories" |
| 69-70 | board two stacking past 60; 80.2 |
| 71-74 | the "each story 5 tok/s · 199 ms" chip |
| 75-76 | close: `SIXTY WAS TRUE. FOR A MATRIX PASS.`; band "matrix pass 57-59"; "16 stories, 2 boards"; "1 story 15.8" |
| 77-82 | `MEASURE WHAT YOU SHIP.`; the source line under the HUD |
| 83-85 | held; the sink in the last 10 frames |

Keyed effects: none. 3D camera: chapter 1 only, the eye at y ≥ 1.0 over a floor at -0.055 (as part 1).

## 5e. Who approved what

| Item | Approved by | Evidence |
| --- | --- | --- |
| One film | owner | "can't it be made as 1 video?" |
| Brief, stills, sweep, technical QA | author | this file |
| Muted watch, sound watch, release | **nobody yet** | — |

## 7. Technical evidence (final render)

- ffprobe: h264 1080x1920, 30 fps, 2535 frames, AAC, 84.50 s.
- Full decode: no errors.
- Loudness -16.8 LUFS; true peak -1.9 dBTP.
- mtimes: source 09:36:03 < exe 09:36:07 < MP4 09:47:12 (after the voice fix; no source change).
- `list` from `/tmp`: works.

## 8. Revision: the shrink line's leak (owner report)

The owner heard "one leaky prompt in voiceover when talking 'so the model was trained...'". In
`04-shrink.wav`, the first 0.5 s was a quieter burst left over from the clone prompt (-18 to -25 dB
against about -12 dB for the speech), then 0.4 s of silence, then "So the model...". Whisper had read the
burst as the word "So", so `tools/vo_trim_head.py` reported "no leak" and kept it.

The fix: the first 0.88 s cut (inside the silence), with a 10 ms fade-in; the original is in
`out/vo-untrimmed/parallella-manual/`. Whisper on the cut take: "So the model was trained with...". The
re-alignment dropped "minus one, zero and one", so those five onsets are the previous alignment's,
minus 0.88 s. `parallella-2012` uses the same take and was re-rendered too.

The sweep frame lines in §5d above were written before this fix. From the shrink scene on, everything
is about 0.9 s earlier; the frames are otherwise the same.

## 9. Decision

**Revise: awaiting the owner's watch** (muted, then with sound). Authoredness 32, Craft 14, on author
evidence only. Open questions for the owner: the length (84.5 s), and whether the separate parts should be
removed now that this film replaces them.
