# skin-demo — QC review

| | |
| --- | --- |
| Composition | `skin-demo` (`examples/showcase/skin_demo.cpp`) |
| Commit / build | uncommitted, on 5ee483a; `build/raymotion.exe` 19:57:38 |
| Reviewer | Claude (agent); owner review pending |
| Date | 2026-10-30 |
| Review stage | final (internal demo) |
| Story mode | `utility` |
| Artifacts | brief `docs/skin-demo-brief.md`; stills `out/skin-demo-stills/`; encoded frames `out/skin-demo-stills/mp4/`; final `out/skin-demo.mp4` |

Rendered with `raymotion render skin-demo --props '{"glb": "tmp/skin/bake/ibu.glb"}'` (the
T-pose test body baked by `tools/skin_bake.py`; not committed).

## 1. One-sentence conceit

> One mesh, any clip: a body baked onto the rig walks, runs, waves and sits on the same
> clips the primitive characters use, and its skeleton shows through to prove it.

Reviewer's restatement after viewing: a generated mesh, bound by the bake, plays four stock
clips in one shot, and for two seconds its joints are drawn inside it. Matches.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief written before `skin_demo.cpp` |
| 1 Script and cue map | pass | text-only cue map = the brief's reveal map |
| 2 Stills | pass | frames 30, 75, 120, 150, 205, 240, 255, 300, 315, 375 |
| 2b Staged reveal | pass | one label at a time; each enters after the motion it names starts (see 5b) |
| 3 Draft (muted + sound) | conditional | silent clip; reviewed as stills and encoded frames, not watched in motion: foot slide and fade timing still need the owner's eye |
| 4 Final technical QA | pass | section 7 |
| 5 Authored test | pass (utility) | reskin: any baked GLB through `--props`; no-props card renders (`out/skin-demo-stills/noprops.png`) |
| 6 Craft & coherence | conditional | the crotch tear at the run (section 4) |
| 7 Release packaging | pass (utility) | end card with the API line; no SRT/thumbnail required for utility |

## 3. Scorecard

| # | Criterion | Score | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | 2 | clear claim, a capability rather than a story | — (utility) |
| 2 | Script / voice | 2 | clip names + the call that plays them | — |
| 3 | Evidence | 3 | joints and bones drawn through the translucent body mid-stride (f75) | |
| 4 | Timing | 2 | beats 3.8 / 2.8 / 2.8 / 1.9 s; sit label is short | lengthen the sit hold |
| 5 | Sound | 1 | silent by design | footfall ticks from contacts |
| 6 | Visual system | 2 | house paper/ink/teal, floor lines for slide; untextured grey body | bake with `--color` |
| 7 | Chapter differentiation | 2 | camera settles frontal for wave/sit; walk/run same framing | |
| 8 | Data & provenance | 2 | no claims; mesh provenance in the brief | |
| 9 | Production pipeline | 3 | bake → sidecar → `--props`; works with no mesh | |
| 10 | Verification | 2 | stills + encoded frames; not watched in motion | owner watch |
| 11 | Memorability | 2 | f75 x-ray frame | |
| 12 | Specificity (reskin) | 3 | any baked body drops in | |
| | **Authoredness total** | **26/36** | | |

| # | Criterion | Score | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | 2 | sits on the block it stopped beside; inner-thigh tear at the run | fix the bake's crotch weights |
| 14 | Information design | 2 | one label at a time; x-ray label beside the head | |
| 15 | Text pacing | 2 | labels hold 1.3–3.3 s | |
| 16 | Mode grammar | 2 | utility: kicker, labels, API end card | |
| 17 | Spec-to-screen | 2 | wave changed to an elbow-held variant and labelled so; brief updated | |
| 18 | Revision quality | 3 | three still-review fixes made and re-verified (below) | |
| | **Craft total** | **13/18** | | |

## 4. Critical fails

None blocking an internal demo. Known defect: during the run (about 3.9–5.9 s, worst at the
walk→run blend, f120 and f150) the far leg's inner thigh near the crotch opens into a flap.
It is in the bake (`separate_legs` and the crotch weights in `tools/skin_bake.py`), not the
composition; double-sided drawing hides the floor showing through, but not the flap. A
smaller seam shows at the crotch when seated (f315) and along the far armpit in the wave.

Re-bake (owner notes: torso side collapsing with the arms down; thigh flattened near the
crotch), final rendered from `tmp/skin/bake2/ibu.glb`:

- Fixed: the ribs under the T-pose arms carried arm weight (mean 0.13) and caved in when
  the arms came down; now 0.01. The wave's far armpit edge is clean.
- Fixed: each leg held 5–27 % of the other side's hip and buttock, and both legs held the
  crotch midline (pulled two ways in a stride); the chest held the crotch. The crotch is
  now found from the mesh (0.906 m, was a fixed 0.938 m). The seated crotch is cleaner.
- Not fixed: the scan's inner thighs are flat where they touched, and the flat face shows
  between the legs in the walk and run (f120). That is geometry, not weights; a first
  rounding pass was too noisy and was taken out.

## 5b. Scene review ledger

| Beat | Frames | Sync note | Approved |
| --- | --- | --- | --- |
| walk + x-ray | 30, 75 | label after the first step; dots ride the joints, all inside the limbs | yes |
| run | 120, 150 | label fades in as the run blend ends; tear noted | yes, with defect |
| turn + wave | 205, 240, 255 | turn lands before the arm rises; hand waves with the elbow bent | yes (after fix) |
| sit | 300, 315, 330 | hips settle on the block top, feet planted ahead | yes |
| end card | 375, 380 | card over the seated body; API line follows | yes |

Revisions: (1) `clips::wave` straightened the elbow at f240 and the raised straight arm read as
a salute: elbow now held bent; (2) "17 joints" corrected (16 drawn); (3) body double-sided.

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920×1080 | 1920×1080 h264 yuv420p | pass |
| Frame rate | 30 | 30/1 | pass |
| Frame count | 390 | 390 | pass |
| Duration | 13.0 s | 13.000 s | pass |
| Full decode | no errors | `ffmpeg -f null` clean | pass |
| Loudness / true peak | n/a | no audio stream (silent utility clip) | n/a |
| Encoded-frame check | matches stills | f75, f120, f255, f330, f380 from the MP4 | pass |
| Source vs executable | exe newer | source 19:57:35, exe 19:57:38 | pass |
| Final render after last edit | yes | MP4 19:57:46 | pass |
| `list` / no mesh | works | no-props card | pass |

## 9. Decision

- [x] **Ship as an internal demo**: Authoredness 26 ≥ 22, Craft 13 ≥ 13, Gates 3 and 6 conditional.
- [ ] Not a showcase cut until the crotch tear is fixed in the bake, the clip has been
  watched in motion, and it has footfall sound.
