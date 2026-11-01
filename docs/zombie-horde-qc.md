# zombie-horde — QC review

| | |
| --- | --- |
| Composition | `zombie-horde` (`examples/showcase/zombie_horde.cpp`) |
| Commit / build | uncommitted, on 83c5283; `build/raymotion.exe` 10:57:02 |
| Reviewer | Claude (agent); owner review pending |
| Date | 2026-10-31 |
| Review stage | internal demo (first cut) |
| Story mode | `utility` |
| Artifacts | brief `docs/zombie-horde-brief.md`; stills `out/zombie-horde-stills/`; encoded frames `tmp/zh/n*.png` and `tmp/zh/m*.png`; score `out/zombie-horde-assets/score.wav`; final `out/zombie-horde.mp4` |

## 1. One-sentence conceit

> Five hundred zombies are four bodies and one strip of poses, each playing it at its own
> phase; a thousand bats are one body and six wingbeats.

Reviewer's restatement after viewing: eight full characters come up a dusk street, the
camera cranes up and the street is full; a counter climbs with the bodies in frame; a
strip of baked poses with a pointer at the ringed zombie's phase shows how; the bats leave
the belfry and the counter reads 1500; the end card gives the API. Matches.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief written first; section 14 records the changes made during the build |
| 1 Script and cue map | pass | text-only cue map = the brief's reveal map |
| 2 Stills | pass | stills at chapter starts, middles and ends, and frames pulled from the MP4 (section 5b) |
| 2b Staged reveal | pass | one label at a time, each overlay after its label; the bats are hidden until the belfry glow |
| 3 Draft (muted + sound) | conditional | the agent cannot watch or listen. Sound was checked by measurement only. The owner needs to watch it once muted and once with sound |
| 4 Final technical QA | pass | section 7 |
| 5 Authored test | pass (utility) | each chapter has a proof object: the counter, the pose strip with a pointer, the wingbeat strip |
| 6 Craft & coherence | pass | tests cover the bake, the cycles and the instancing path |
| 7 Release packaging | pass (utility) | end card with the API line, the logo and the GitHub line; no SRT or thumbnail required for utility |

## 3. Scorecard

| # | Criterion | Score | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | 2 | one idea: the crowd is a few baked bodies | — (utility) |
| 2 | Script / voice | 2 | text-only cue map; short labels | — |
| 3 | Evidence | 3 | the counter, and strips whose pointers follow the numbers the renderer plays | |
| 4 | Timing | 2 | beats 5 / 6 / 5 / 4 s; the bats leave at 12.2 s, 1.2 s after their label | owner watch |
| 5 | Sound | 2 | procedural, placed from the bake; measured, not heard | owner listen |
| 6 | Visual system | 2 | dusk street, amber windows and lamp; teal overlays | |
| 7 | Chapter differentiation | 3 | low street view, high crane, tilt to the belfry, card | |
| 8 | Data & provenance | 2 | no outside claims; the numbers are the scene's own | |
| 9 | Production pipeline | 3 | bake in `build`; score from `c.assets`; `list` works from any directory | |
| 10 | Verification | 2 | stills, encoded frames, loudness, tests; not watched or heard in motion | owner watch |
| 11 | Memorability | 2 | the swarm leaving the belfry over the horde (f366) | |
| 12 | Specificity (reskin) | 2 | zombie look, clip and belfry are data | |
| | **Authoredness total** | **27/36** | | |

| # | Criterion | Score | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | 2 | the horde walks round the barricade cars towards the camera; bats come from the tower | |
| 14 | Information design | 2 | one label and one overlay at a time | |
| 15 | Text pacing | 2 | labels hold 4–6 s | |
| 16 | Mode grammar | 3 | utility: counter, strips, API end card, logo and repo line | |
| 17 | Spec-to-screen | 2 | brief section 14 records every change | |
| 18 | Revision quality | 2 | fixes during the build (fog hiding the horde, early counter, jammed barricade, scrim corners); no owner round yet | |
| | **Craft total** | **13/18** | | |

## 4. Critical fails

None. Known issues:

- **Not watched or heard.** The draft was judged from stills, frames pulled from the MP4,
  and measurement.
- **Baked look.** The mass has flat colours; outfit and face textures show only on the
  eight hero rigs.
- **Wide barricade.** At the end card the horde walks through the cars' gap.
- **Score cache.** `ensureAssets` keeps an existing `score.wav`; delete
  `out/zombie-horde-assets/score.wav` after any change to the world or the sound.

## 5b. Scene review ledger

| Beat | Frames | Sync note | Approved |
| --- | --- | --- | --- |
| street | 0, 45, 100, 150 | the eight arrive first; counter starts at 8 and climbs as the crane shows the rest | yes |
| horde | 150, 210, 270, 330 | ring on one zombie and the pointer on the strip move together; the mass parts round the cars | yes |
| swarm | 366, 380, 400, 440 | glow and riser lead the release at 12.2 s; counter 1500 | yes |
| end card | 495, 500, 590 | title, code line, logo, repo line; the horde walks on beneath | yes |

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Video | 1920×1080, 30 fps | h264 1920×1080, 30/1 | pass |
| Audio | stereo, 48 kHz | aac 48000 Hz, 2 ch | pass |
| Frame count | 600 | 600 | pass |
| Duration | 20.0 s | 20.000 s (video and audio) | pass |
| Full decode | no errors | `ffmpeg -v error -f null`: no output | pass |
| Loudness | about -16 LUFS integrated | -15.9 LUFS, LRA 3.6 LU (on the MP4) | pass |
| Peak | no clipping | sample peak -5.2 dBFS (`ebur128` peak mode, on the MP4) | pass |
| Encoded-frame check | matches stills | frames 0, 45, 100, 150, 270, 330, 366, 400, 495, 590 from the MP4 viewed | pass |
| Source vs executable | exe newer | source 10:56:59, exe 10:57:02 | pass |
| Final render after last build | yes | MP4 10:57:31 | pass |
| `list` from another directory | works | `zombie-horde 1920x1080 @30fps 20.0s audio=1` (run from `C:\Windows`) | pass |
| Render cost | recorded | 600 frames in 12.9 s | recorded |
| Module tests | pass | `ctest -R "crowd\|rig\|scene3d"`: 4/4 | pass |

**Profile (instancing).**

- Same film with the horde as 500 rig characters: 188.1 s for 600 frames (about 313 ms a
  frame). Instanced: 14.4 s (about 24 ms including encode). About 13 times faster.
- Bakes: zombie 2,385 vertices × 40 frames, bat 149 vertices × 12 frames.
- Each group is one draw call per pass after the per-instance cull.
- Pixel parity: instanced boxes match ordinary objects, with shadows (`scene3d` test).

**Not yet done.** A clean-machine build with `tools/configure.sh` from a fresh checkout.

## 9. Decision

- [x] **Ship as an internal demo:** Authoredness 27 ≥ 22, Craft 13 ≥ 13, Gate 3
  conditional on the owner's watch.
- [ ] Not a showcase cut until it has been watched and heard in motion and reaches
  Authoredness 30 and Craft 16.
