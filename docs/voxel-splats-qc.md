# voxel-splats — QC review

| | |
| --- | --- |
| Composition | `voxel-splats` (`examples/showcase/voxel_splats.cpp`) |
| Commit / build | uncommitted, on 0a78048 |
| Reviewer | Claude (agent); owner review pending |
| Date | 2026-10-31 |
| Review stage | internal demo (first cut) |
| Story mode | `utility` |
| Artifacts | brief `docs/voxel-splats-brief.md`; stills `tmp/vs/`; encoded frames `tmp/vs/mp4/`; score `out/voxel-splats-assets/score.wav`; island `out/voxel-splats-assets/island.vox`; final `out/voxel-splats.mp4` |

## 1. One-sentence conceit

> One island is built twice, as a voxel model and as a cloud of Gaussian splats made from
> it, and a front hands it from one to the other.

Reviewer's restatement after viewing: the island rises out of the sea with a glowing edge
and a voxel counter; its top faces are outlined as merged quads with the quad/face counts;
a front turns it into splats with a splat counter and one splat enlarged to its sigma
rings; a second front brings the voxels back as far as the middle, with both counts live;
the end card gives the two API lines. Matches.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief first; section 14 records the changes |
| 1 Script and cue map | pass | text-only cue map = the brief's reveal map |
| 2 Stills | pass | stills at chapter starts, middles, ends, plus frames from the MP4 |
| 2b Staged reveal | pass | one label at a time; overlays follow their label |
| 3 Draft (muted + sound) | conditional | the agent cannot watch or listen; sound checked by measurement only. Owner: watch once muted, once with sound |
| 4 Final technical QA | pass | section 7 |
| 5 Authored test | pass (utility) | proof objects: the build rising, the quad outlines and counts, the enlarged splat, the live split |
| 6 Craft & coherence | pass | module tests cover meshing, `.vox`, splat projection, sort, clip |
| 7 Release packaging | pass (utility) | end card with API lines, logo and repo line |

## 3. Scorecard

| # | Criterion | Score | Notes |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | one object, two representations, one hand-off |
| 2 | Script / voice | 2 | text-only cue map |
| 3 | Evidence | 3 | counts are read from the build; the Gaussian is drawn from its own formula |
| 4 | Timing | 2 | beats 5 / 7 / 5 / 3 s; owner to confirm in motion |
| 5 | Sound | 2 | procedural, ticks placed from the rise; measured, not heard |
| 6 | Visual system | 2 | one palette, teal for voxel marks, amber for splat marks |
| 7 | Chapter differentiation | 3 | rise, outlines, dissolve, enlarged splat, split |
| 8 | Data & provenance | 3 | no outside claims; every number is the scene's own |
| 9 | Production pipeline | 3 | bake in `build`; score and `.vox` via `c.assets`; `list` works from any directory |
| 10 | Verification | 2 | stills, encoded frames, loudness, tests; not watched or heard |
| 11 | Memorability | 2 | the amber front turning blocks into soft splats mid-island |
| 12 | Specificity (reskin) | 2 | island, palette and counters are data |
| | **Authoredness total** | **29/36** | |

| # | Criterion | Score | Notes |
| --- | --- | --- | --- |
| 13 | Scenario coherence | 2 | one island in one sea, orbiting camera |
| 14 | Information design | 2 | one label and one overlay at a time |
| 15 | Text pacing | 2 | labels hold 4–7 s |
| 16 | Mode grammar | 3 | utility: counters, API end card, logo and repo line |
| 17 | Spec-to-screen | 2 | brief section 14 records every change |
| 18 | Revision quality | 2 | fixes: opaque overlays (alpha units), sky banding, soft splats, sigma glyphs |
| | **Craft total** | **13/18** | |

## 4. Critical fails

None. Known issues:

- **Not watched or heard.**
- **Splats read soft next to voxels.** That is the representation (Gaussians, 0.3 px
  low-pass), not a focus setting; splats are not lit or shadowed (the colours are baked
  by hand in the film).
- **Score cache.** `ensureAssets` keeps an existing `score.wav`; delete
  `out/voxel-splats-assets/score.wav` after any change to the island or the sound.
- **The enlarged panel overlaps its sigma labels** slightly at 2 sigma.

## 5b. Scene review ledger

| Beat | Frames | Sync note | Approved |
| --- | --- | --- | --- |
| rise + voxels | 20, 45, 90 | the edge climbs with the counter; label at 0.5 s | yes |
| quads | 125, 150 | outlines draw in after the label; counts lead the sweep | yes |
| dissolve | 170, 180, 195 | front crosses the island; voxels ahead, splats behind | yes |
| splats | 240, 330 | counter, then the enlarged splat with a leader to its place | yes |
| split + end | 400, 460, 540, 585 | both counters live; the card appears over the half-and-half island | yes |

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Video | 1920×1080, 30 fps | h264 1920×1080, 30/1 | pass |
| Frame count / duration | 600 / 20.0 s | 600 / 20.000 s (video and audio) | pass |
| Full decode | no errors | `ffmpeg -v error -f null`: no output | pass |
| Loudness | about -16 LUFS | -16.0 LUFS, LRA 3.5 LU (on the MP4) | pass |
| Peak | no clipping | true peak -4.7 dBFS | pass |
| Encoded-frame check | matches stills | frames at 1, 3, 7, 11, 14, 18 s from the MP4 viewed (7 and 14 in detail) | pass |
| Source vs executable | exe newer | built after the last source edit; render after the build | pass |
| `list` from another directory | works | run from `/tmp`, `voxel-splats` listed | pass |
| Render cost | recorded | 600 frames in 8.7 s | recorded |
| Module tests | pass | `ctest -R "scene3d\|voxel\|splat\|crowd\|rig"`: 6/6 | pass |

**Profile.** 1M splats measured at 25.9 ms a frame at 240×160 including sort and readback
(engine test); the film's cloud is tens of thousands of splats (the on-screen counter) and the whole film renders at about 14.5 ms a frame including encode.

**Not yet done.** A clean-machine build with `tools/configure.sh`; the owner's watch.

## 9. Decision

- [x] **Ship as an internal demo:** Authoredness 29 ≥ 22, Craft 13 ≥ 13, Gate 3
  conditional on the owner's watch.
- [ ] Not a showcase cut until watched and heard, and Authoredness ≥ 30, Craft ≥ 16.
