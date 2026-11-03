# screen-is-the-frame — QC review

| | |
| --- | --- |
| Composition | `screen-is-the-frame` (`examples/showcase/screen_is_the_frame.cpp`) |
| Commit / build | uncommitted, on d672a5f |
| Reviewer | Claude (agent); owner review pending |
| Date | 2026-11-01 |
| Review stage | internal demo (first cut) |
| Story mode | `utility` |
| Artifacts | brief `docs/screen-is-the-frame-brief.md`; stills `tmp/sf/`; encoded frames `tmp/sf/mp4/`; score `out/screen-is-the-frame-assets/score.wav`; final `out/screen-is-the-frame.mp4` |

## 1. One-sentence conceit

> One phone screen, drawn once in 2D, is an object on a desk, then the whole frame, then a
> stack of layers.

Reviewer's restatement after viewing: a phone on a stand shows a live bus app, its
countdown ticking and its bus moving on the map, with the texture density read out
beneath. The camera flies square into the screen and the readout climbs. At the cut the tag
flips to `2D` and nothing else changes. The map card then opens past the phone to the
whole frame and shows the full route. The bus arrives (`Now`), the card folds back, and the
film cuts back to 3D. The screen comes apart into four layers. Each layer gets a label,
which is itself a surface, and as the camera drifts the map layer slides over the backdrop
label's leader. The layers close under the end card. Matches the brief.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief written first; section 14 records the changes |
| 1 Script and cue map | pass | text-only cue map = the brief's reveal map; every cue reads a named constant |
| 2 Stills | pass | frames 0, 90, 225, 299/300, 360, 380, 434/435, 449/450, 520, 540, 575, 580, 640, 659, plus frames from the MP4 |
| 2b Staged reveal | pass | one kicker at a time; labels 0.5 s apart; the tag flip leads the cut sound by 0 s (the cut itself) |
| 3 Draft (muted + sound) | conditional | the agent cannot watch or listen; sound checked by measurement only. Owner: watch once muted, once with sound |
| 4 Final technical QA | pass | section 7 |
| 5 Authored test | pass (utility) | proof objects: the live readout from `ui->resolution`, the unchanged picture across the tag flip, the card leaving the phone, the label hidden by a layer |
| 6 Craft & coherence | pass | the engine claim ("the cut does not show") is the `surfaces()` test and is measured again on the film's own frames (section 7) |
| 7 Release packaging | pass (utility) | end card with API lines, logo and repo line; thumbnail frame 540 |

## 3. Scorecard

| # | Criterion | Score | Notes |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | one drawing, three places, no seam between them |
| 2 | Script / voice | 2 | text-only cue map |
| 3 | Evidence | 3 | readout is the renderer's own number; the cut is measured on the film's frames |
| 4 | Timing | 2 | beats 5 / 5 / 4.5 / 5 / 2.5 s; owner to confirm in motion |
| 5 | Sound | 2 | procedural; ticks from the countdown, chime from `BUS_NOW`; measured, not heard |
| 6 | Visual system | 2 | dark desk, the app's own palette inside, teal only for the engine |
| 7 | Chapter differentiation | 3 | arc, straight push, flat 2D, orbit round a stack |
| 8 | Data & provenance | 3 | app and stops invented and tagged `FICTIONAL APP`; engine numbers read live |
| 9 | Production pipeline | 3 | score via `c.assets`; `list` works from any directory (after the Bundaran fix below) |
| 10 | Verification | 2 | stills, cut diffs, encoded frames, loudness, tests; not watched or heard |
| 11 | Memorability | 3 | the map tearing out of the phone to full frame |
| 12 | Specificity (reskin) | 2 | the app is data-light code; the camera beats are constants |
| | **Authoredness total** | **30/36** | |

| # | Criterion | Score | Notes |
| --- | --- | --- | --- |
| 13 | Scenario coherence | 2 | one desk, one phone, one app clock that drives the countdown, the bus and the ticks |
| 14 | Information design | 2 | one kicker, one readout; labels one at a time |
| 15 | Text pacing | 2 | kickers hold 3.3–3.9 s |
| 16 | Mode grammar | 3 | utility: live readout, API end card, logo, repo line |
| 17 | Spec-to-screen | 2 | brief section 14 records every change |
| 18 | Revision quality | 2 | fixes: z-fighting glass, labels never hidden, HUD over the app, bus off the card at frame 0 |
| | **Craft total** | **13/18** | |

## 4. Critical fails

None. Known issues:

- **Not watched or heard.**
- **Props are primitives** (a cylinder mug, box notebook, cylinder pen). They read as a
  desk but are plain; device prefabs and simple props are the next engine step.
- **The text layer floats furthest out**, so at the end of the drift its words sit off
  the card they belong to (frame 575). That is what layers in depth do; it reads as busy
  for a moment.
- **Score cache.** `ensureAssets` keeps an existing `score.wav`; delete it after changing
  any timing constant.

## 5b. Scene review ledger

| Beat | Frames | Sync note | Approved |
| --- | --- | --- | --- |
| the object | 0, 90 | bus on the card from frame 0; kicker at 0.5 s; readout from 2.2 s (1.00×) | yes |
| the push | 225, 299 | readout 1.41× → 2.00×; the push eases to rest at 10.0 s | yes |
| the cut | 299 / 300 | same picture, tag flips; click on frame 300 | yes |
| the break-out | 360, 380 | card spans the frame, the route and river appear; `Now` at 12.6 s with the chime | yes |
| the cut back | 434 / 435 | same picture, tag flips back | yes |
| the swap to layers | 449 / 450 | change no larger than the camera motion between neighbours | yes |
| the layers | 520, 540, 575 | labels in depth order; at 575 the backdrop label's dot is under the map layer | yes |
| end | 640, 659 | card over the closed stack; logo complete by 659 | yes |

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Video | 1920×1080, 30 fps | h264 1920×1080, 30/1 | pass |
| Frame count / duration | 660 / 22.0 s | 660 / 22.000 s (video and audio) | pass |
| Full decode | no errors | `ffmpeg -v error -f null`: no output | pass |
| Loudness | about -18 LUFS | -17.1 LUFS (on the MP4) | pass |
| Peak | ≤ -1 dBFS | true peak -5.0 dBFS | pass |
| Cut 3D → 2D (299/300) | invisible | mean abs difference 0.10 / 255 outside the HUD | pass |
| Cut 2D → 3D (434/435) | invisible | 0.09 / 255 | pass |
| One surface → four layers (449/450) | invisible | 2.71 / 255, against 2.91 and 3.20 for the neighbouring frame pairs (camera motion) | pass |
| Encoded-frame check | matches stills | frames at 3, 10, 12, 17, 21 s from the MP4 viewed | pass |
| Source vs executable | exe newer | exe built after the last source edit; final render after the build | pass |
| `list` from another directory | works | run from the temp dir: completes, film listed, no assets made | pass |
| Render cost | recorded | 660 frames in 8.1 s (four 390×844 layer canvases redrawn each frame in chapter 4) | recorded |
| Module tests | pass | `ctest -R scene3d`: passed | pass |

**Found on the way:** `list` from another directory aborted at `bundaran-hi-banner`,
whose `build` loaded `assets/data/bundaran-hi/...` relative to the working directory,
which hid every film registered after it. Fixed by giving the path relative to the
assets root (`data/bundaran-hi/...`), which `media::resolve` finds from anywhere.

**Not yet done.** A clean-machine build with `tools/configure.sh`; the owner's watch.

## 9. Decision

- [x] **Ship as an internal demo:** Authoredness 30 ≥ 22, Craft 13 ≥ 13, Gate 3
  conditional on the owner's watch.
- [ ] Not a showcase cut until watched and heard and Craft ≥ 16 (props, the text layer's
  drift).
