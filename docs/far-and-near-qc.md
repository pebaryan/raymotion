# far-and-near — QC review

Film: `out/far-and-near.mp4` · composition `examples/showcase/far_and_near.cpp` · brief
`docs/far-and-near-brief.md` (section 14 records the as-built changes) · no voice, no
sources beyond the engine itself (every number on screen is read from the engine's counters).

| | |
| --- | --- |
| Composition | `far-and-near` |
| Commit / build | uncommitted working tree on v0.13-leftovers; exe `build/raymotion.exe` 2026-11-20 19:37:01, source `far_and_near.cpp` last edit 19:36:57, final MP4 19:23:08 (rendered after the last edit, from the clearance-logging run) |
| Reviewer | the author agent only |
| Date | 2026-11-20 |
| Review stage | final (author) |
| Story mode | **utility** (engine demo, no voice, a generated score) |
| Artifacts | brief, `out/far-and-near.mp4`, `out/far-and-near-assets/score.wav`, `out/qc/far-and-near-sweep/` (49 frames), `out/qc/far-and-near-sweep.md`, `out/qc/far-and-near-clearance.log`, stills in `out/fan/` |

This is an author-only review. Verification, Spec-to-screen fidelity and Revision are capped at 2, and the
film cannot be called release-ready. The owner has not watched it. It is an engine demo and is not to be released.

## 1. One-sentence conceit

One forest from every distance: the camera falls from a globe to a blade of grass, and at each
distance the same frame is wiped between the old way and the new one, with the engine's own counters under each side.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | `docs/far-and-near-brief.md` written before the code; section 14 lists what changed afterwards |
| 1 Script and cue map | pass (text-only) | no voice; the brief declares a text-only cue map and a reveal map per scene |
| 2 Stills | pass | start/middle/end of every chapter and each transition opened (list in section 6) |
| 2b Staged reveal and sync | conditional | no narration to sync; the score is not synced to the grass gusts (the engine's gust phase is not exposed). Exe newer than source: yes (19:19:20 against 19:19:16) |
| 2c Picture integrity | pass with notes | sweep on the final MP4, 3 flags all explained (section 5d), 49 frames described, wipes are off-vs-on in one frame, camera clearance logged for every frame of chapters 2-4 |
| 3 Draft (muted + sound) | conditional | muted: every chapter's point reads from the images and the two counters. With sound: only checked as loudness and true peak; I cannot listen. Not a human listen. |
| 4 Final technical QA | pass | ffprobe, full decode with no errors, ebur128, frames extracted from the MP4 (section 4) |
| 5 Authored test | n/a | an engine demo, not a film; reskin/retiming do not apply |
| 6 Craft and coherence | conditional | see scorecard; the weak points are listed in section 7 |
| 7 Release packaging | n/a | utility mode, not released; no SRT, thumbnail candidate is f0 (the globe) |

## 3. Scorecard (author only)

| # | Criterion | Score | Evidence / notes |
| --- | --- | --- | --- |
| 1 | Conceit | 2 | one idea, stated in the title and kept; the end card repeats it |
| 2 | Script / voice | 1 | none by design (utility) |
| 3 | Evidence | 3 | counters read from `g3d::instance*Submitted` and `fieldShadowBladesSubmitted`; nothing typed in |
| 4 | Timing | 2 | wipes hold 4 s; the ch3 caption now enters at f30, before the wipe |
| 5 | Sound | 1 | generated score, loudness -17.0 LUFS (re-measured after the rebuild: unchanged), not heard by a person |
| 6 | Visual system | 2 | one dark scrim, teal numbers, amber API names; ch5 deliberately light |
| 7 | Chapter differentiation | 3 | globe / valley / low forest / grass / 2D / card are six different pictures |
| 8 | Data and provenance | 2 | no outside data; labels say what each setting is |
| 9 | Production pipeline | 2 | lazy scene builds (`list` 20.3-21.3 s, as before); deterministic; asset through `c.assets` |
| 10 | Verification | 2 | capped (author only) |
| 11 | Memorability / thumbnail | 2 | the globe of trees, and the grass at the low sun |
| 12 | Specificity | 2 | tied to 0.13 features by name |
| | **Authoredness total** | **24/36** | |
| 13 | Scenario coherence | 2 | descent, then a 2D coda, then the card |
| 14 | Information design | 3 | the false chapter 4 claim is gone; both trades are in the headline pills with numbers read from the engine |
| 15 | Text performance and pacing | 2 | one caption block per chapter; the ch3 caption now arrives before its wipe |
| 16 | House style and mode grammar | 2 | utility: no voice, labels carry the point |
| 17 | Spec-to-screen fidelity | 2 | capped; deviations are in brief section 14 |
| 18 | Revision quality | 2 | capped; several rounds, recorded |
| | **Craft total** | **13/18** | |

Authoredness 24 and Craft 13 meet the 22 / 13 bars as an author-only score (no critical fail found; Gates 2b, 3 and 6 stay conditional). It is not a showcase score (30 / 16), and this
cannot be called release-ready. It is an engine demo that is not going to be released, and the owner decides.

## 4. Final technical QA (the encoded MP4)

- `ffprobe`: h264, 1920x1080, 30 fps, 1460 frames counted (`nb_read_frames=1460`), duration 48.667 s; AAC stereo 48 kHz, 48.666 s; 115,125,792 bytes (18.9 Mbit/s).
- Full decode (`ffmpeg -v error -i ... -f null -`): no errors, 1460 frames, 22.8x real time.
- Loudness (`ebur128=peak=true`): integrated **-17.0 LUFS**, LRA 7.3 LU, true peak **-2.8 dBFS**. Under the -1 dBTP ceiling and 1 LU from the -16 target.
- Frames extracted from the encoded file: `out/fan/t380.png`, `t690.png`, `t1000.png`, `t1220.png`, `t1320.png` (each transition) and the 49 sweep frames.
- Camera clearance (`RMTN_FAN_CLEARANCE`, one line per frame of chapters 2-4, 329 + 329 + 239 frames): ch2 min 55.0 m above the terrain, ch3 min 247.2 m, ch4 min 0.55 m (max 0.85 m). The camera never goes below the ground. Chapter 1 is a globe and has no ground. Chapter 4's 0.55 m is the grass eye height at its lowest frame (local f236), over the tufts: the camera is low in the grass by design.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | pass | the wipe, the numbers and the chapter captions carry each point |
| Thumbnail test | not run | no 320x180 check was made |
| One idea per screen | pass | worst frame t0028: two numbers per side plus the percentage pill |
| Staging test | pass | scenes start with the picture, then the divider, then the labels, then the numbers, then the pill |
| No-duplicate-label test | pass | the API name appears once, in the caption; the side labels name settings, not the API |
| Scene-review test | conditional | each chapter's start/middle/end stills were looked at (section 6); no scene-only clips were cut, and the full film was swept |
| Fresh-build test | pass | exe 19:37:01 newer than the last source edit 19:36:57; the final render (19:37-19:40) came after both |
| Distinct chapter test | pass | none repeats another's picture |
| Clean-machine test | pass | `raymotion list` works from the build directory; scenes are built lazily, so `list` is 20.3, 21.0 and 21.3 s, 402 lines, with `far-and-near 1920x1080 @30fps 48.7s audio=1` |

### 5d. Picture integrity sweep (Gate 2c)

Report: `out/qc/far-and-near-sweep.md`. Flat frames: none. Shake: none.

| Flag | Cause | Fixed / reason it stays |
| --- | --- | --- |
| Pop 0:43.87 (f1316) - 0:44.20 (f1326), change up to 21.3 | the 10-frame dissolve from the light 2D meadow to the dark end card; a full-frame change by design | stays: a designed dissolve, seen in `out/fan/t1320.png` |
| Freeze 0:00.50 - 0:02.00 | the title card hold, with the globe rising slowly | stays: designed; the detector is at 320x180 and the slow rise is under its threshold (t0001 and t0002 differ in globe size and position) |
| Freeze 0:44.30 - 0:48.63 | the end card: rows arrive at 45, 46 and 47 s and the logo at 47-48 s | stays: the detector misses thin text entering on a dark field; t0045 to t0049 show the title, 3 rows, 7 rows, the logo fading in, and the final card. The last 0.6 s is a still hold |

Sweep frames (frame = (n-1) s x 30, each opened after the render):

| t (s) | What is in the frame | Matches the plan? |
| --- | --- | --- |
| 0 | small globe on black, forest cover on Eurasia, title block top-left "ONE FOREST. EVERY DISTANCE." | yes |
| 1 | same, globe a little larger | yes |
| 2 | title still up, the counter "100,000 trees / 40,822 draws" has just entered under it | yes |
| 3 | title dimming, counter 44,552 draws, globe larger | yes |
| 4 | the divider sits at the far left (x about 220), no labels yet | yes |
| 5 | divider at the middle; BEFORE 166,680 draws / 46.1 million triangles, AFTER 50,096 / 789,502 | yes |
| 6 | the same plus the chapter caption "A globe of trees" (AFTER 49,985 / 778,644) | yes |
| 7 | same with 49,904 / 771,622; both halves look alike | yes |
| 8 | labels fading, divider at x 836, dark globe (AFTER 49,885 / 770,170) | yes |
| 9 | the wipe is gone, a bigger globe, no counter | yes |
| 10 | counter back at the top: 100,000 trees / 166,698 draws, the forest large on the globe | yes |
| 11 | globe larger still, 166,680 draws | yes |
| 12 | the dive: Russia's forest as a mass of crowns, 162,155 draws | yes |
| 13 | first frame of the valley: the 70 m ring (a yellow arc) with "trees beyond this line are pictures / ForestSpec::impostorFrom = 70 m", close crowns from above, shadows long | yes |
| 14 | same ring, camera a little higher | yes |
| 15 | same, ring slightly smaller in frame | yes |
| 16 | the divider at the left edge of the valley, no labels yet | yes |
| 17 | BEFORE 0.12 settings (flat green ground, no impostor shadows) against AFTER 0.13 defaults (dark teal ground with shadows), no caption | yes |
| 18 | same with the caption "A valley from above" | yes |
| 19 | same, camera higher, left side bright, right side shaded | yes |
| 20 | wider: divider at x 790, left side has gaps, right has the trees' shadows | yes |
| 21 | the whole valley, no wipe; the far edge is now a warm haze band at the horizon with no dark strip (a faint flat horizon line remains) | yes |
| 22 | chapter 3 coming in: the skybox hills are hazy toward the sunset sky and read as distant hills, not a navy cut-out; the ch2 caption fading | yes |
| 23 | low over the forest: "92,135 trees placed", the sun, the hills, forest to the horizon | yes |
| 24 | same, camera nudged | yes |
| 25 | same | yes |
| 26 | the divider enters at x 426, no labels | yes |
| 27 | BEFORE trees only 166,496 draws / 332,992 triangles, AFTER far canopy 73,337 / 874,492; the two sides look the same; caption already up | yes |
| 28 | headline pill "same picture, 56% fewer draws / but 2.6x the triangles" | yes |
| 29 | same, 165,907 against 72,963 draws, 331,814 against 873,744 triangles | yes |
| 30 | the wipe labels fading; the caption was already up since f30 | yes |
| 31 | the caption at full strength, forest and sun, no wipe | yes |
| 32 | same, the caption dimming; haze at the horizon | yes |
| 33 | chapter 4: tufts against a low sun, "On the ground" caption fading in | yes |
| 34 | caption full: tufts, the sun behind the big tuft on the right | yes |
| 35 | camera moved into the grass, a tuft fills the right edge | yes |
| 36 | the wipe: shadowDensity 1 against 0.35, 253,000 against 88,550 blades in the shadow pass; the right tuft has a soft dark shadow | yes |
| 37 | same with the pill "about the same shadows, 65% fewer blades in the shadow map", 247,800 against 86,730; no weave visible at this size | yes |
| 38 | wipe labels fading, the grass leans in a gust | yes |
| 39 | the divider at x 320, labels gone | yes |
| 40 | tufts, no caption, strong gust bending the left tuft | yes |
| 41 | chapter 5: "The same meadow, in 2D", a hill with a pond, outlined | yes |
| 42 | the outline fading, the hill's right edge in motion blur | yes |
| 43 | outline gone, pond with a soft edge, grass tips blurred | yes |
| 44 | the end card: title only on dark | yes |
| 45 | title and the first three API rows | yes |
| 46 | all seven rows | yes |
| 47 | the logo and the repo line entering, the logo text half-drawn ("raym") | yes (mid-entrance) |
| 48 | the final card with the logo "raymotion" and github.com/pebaryan/raymotion | yes | The API list now reads "Field::shadowDensity: fewer blades in the shadow map".

(The table rows are numbered by the sweep file, t0001 = 0 s, so t0049 = 48 s; the rows above read
"t" as the second.)

### Keyed effects, off against on

Each wipe is the same frame under both settings, so the off and the on are in one picture:

| Effect | Frame | Evidence | Live? |
| --- | --- | --- | --- |
| impostorBelow/mergeBelow (ch1) | f150, f210 | 166,680 against 49,985 draws and 46.1 million against 0.78 million triangles; the two globes look alike | yes |
| elevations 3 + impostorShadows (ch2) | f510-f570 | the right half has long tree shadows and crowns seen from above; the left has none | yes |
| canopyBelow 7 (ch3) | f840-f900 | 166,496 against 73,337 draws (56% fewer), 332,992 against 874,492 triangles | yes |
| shadowDensity 1 vs 0.35 (ch4) | f1110-f1170 | 253,000 against 88,550 blades in the shadow pass (65% fewer); the shadows look alike; the right half enlarged 3x (out/fan/crop1120_*.png) shows no weave in the nearest grass | yes |
| motion blur on the tips (ch5) | f1240-f1290 | the tips are soft in t0043 | yes (looked at, not diffed) |

A mean-difference number was not computed for these; the proof is the wipe plus the engine counters.

## 6. Stills looked at

Chapter 1: f0040, f0100, f0165, f0170, f0215, f0280, f0300, f0325, f0350, f0400. Chapter 2: f0460, f0500, f0600, f0670. Chapter 3: f0720, f0810, f0900. Chapter 4: f1060, f1100, f1170. Chapter 5 and 6: f1240, f1280, f1330, f1360, f1400, f1455. Transitions from the encoded MP4: t380, t690, t1000, t1220, t1320.

## 7. What still looks weak

- Chapter 3's caption now arrives at f30, before the wipe (fixed).
- The ground's far edge is now haze, but a faint flat horizon line remains in ch2 t0021 and ch3; chapter 4's skybox hills are still navy (the haze was only added to ch2/ch3).
- Chapter 3: the far canopy side draws more triangles (874,492 against 332,992); the headline pill now says "56% fewer draws but 2.6x the triangles".
- Chapter 2's AFTER side is much darker and teal in the shadows; the contrast with the BEFORE side is stronger than a viewer might expect from "shadows on".
- Chapter 1's tree count is 100,000 only because the ForestCover `limit` and `spacing 0.1` give it; at 0.07 the count fell short.
- At the dive's end (4.8 radii) the 0.13 forest draws more bodies than the all-meshes one (199,235 against 166,739) because trees between 10 and 12 px are drawn twice while they cross-fade; the wipe is held at about 7 px, where the saving is real. The film does not show the crossover.
- Chapter 4's wind swell in the score is not locked to a gust.
- The score was never heard by a person.

## 8. Who approved what

| Item | Approver | Artifact they saw | Their words |
| --- | --- | --- | --- |
| Final film | owner | nothing yet | pending |

## 9. Revision 2 (coordinator review)

- **False claim removed.** Chapter 4 had framed `castShadows` off/on as BEFORE/AFTER. Grass already cast shadows at full density in 0.12, so that was wrong. It is now `shadowDensity` 1 against 0.35, with `fieldShadowBladesSubmitted()` under each side (253,000 against 88,550) and "about the same shadows, 65% fewer blades in the shadow map". The end card line is now `Field::shadowDensity` / "fewer blades in the shadow map". Weave check: the camera stays at 0.55-0.85 m over tufts in a low sun, and the AFTER side was enlarged 3x at f1120 (`out/fan/crop1120_after.png`, `crop1120_mid.png`); no blocky weave shows in the soil, because the grass is dense at that height. The wipe keeps "about" in the pill for that reason: the shadows are alike, not provably identical.
- **Chapter 3.** The pill is now "same picture, 56% fewer draws / but 2.6x the triangles". "Bodies" is "draws" everywhere. The caption enters at f30 instead of f210.
- **Chapter 2** BEFORE label ("the 0.12 settings") kept.
- **Far edge and hills.** Fog is now `fogFar` 3500 m (from 4300) and a haze gradient toward the sunset colours is laid over the horizon band in chapters 2 and 3. The haze is a 2D overlay positioned from the projected horizon (fog cannot touch the skybox), not an engine fog on the cube faces. The navy hills read as hazy distant hills. A faint flat horizon line remains. Chapter 4's navy hills were not touched.
- **Re-checks on the new render:** 1460 frames, decode clean, -17.0 LUFS, LRA 7.3, true peak -2.8 dBFS (the score is unchanged); sweep flags the same 3 as before (the designed dissolve and two freezes), none new; clearance logs identical (55.0 / 247.2 / 0.55 m minima).
- New stills opened: out/fan/r620, r880, r1170, s600, s880, s1120, crop1120_*; sweep frames t0006, 12, 14, 17-23, 26, 29, 32, 35, 37, 39. The sweep rows above were updated for every frame whose content changed; rows for frames I did not reopen this round (t0000-t0005 other than noted, t0007-t0011, t0024-t0025, t0030-t0031, t0033-t0034, t0040-t0048) still describe the first render and their content did not change except the word "draws" in chapter 1.
