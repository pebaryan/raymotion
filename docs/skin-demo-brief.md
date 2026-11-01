# skin-demo — authoring brief

Status: `internal demo` (owner asked for "a video using the rigged glb"; short brief as
the authoring standard allows for a quick internal demo).

| | |
| --- | --- |
| Composition name | `skin-demo` |
| Story mode | `utility` (a capability demo for `rig::loadSkin`; no claims about the world) |
| Owner / agent | pebaryan / Claude |
| Runtime | 13 s (390 frames), 1920x1080, 30 fps |
| Voice / audio | none: a silent technical clip |
| Audience | the owner and people evaluating the rig: afterwards they can say "a baked mesh plays the rig's own clips, its joints inside the limbs" |
| Source material | a body baked by `tools/skin_bake.py` (passed with `--props '{"glb": "..."}'`) |

## 1. One sentence

> One mesh, any clip: a body baked onto the rig walks, runs, waves and sits on the same
> clips the primitive characters use, and its skeleton shows through to prove it.

## 3. Chapters (one continuous shot)

| # | Time (s) | Beat | On-screen text | Proof object |
| --- | --- | --- | --- | --- |
| 1 | 0.0–3.8 | walks in along a pale floor | `walk` + `rig::clips::walk()` | the stride; from 1.6 to 3.4 s the body turns translucent and its 16 joints (Root, on the floor, left out) show as dots and bones inside the limbs (label "the rig's joints, inside the mesh") |
| 2 | 3.8–6.6 | breaks into a run, then slows | `run` | the run's longer stride and lean |
| 3 | 6.6–9.4 | stops, turns to the camera, waves | `wave` + `rig::clips::wave(), elbow held` | shoulder and elbow bending without collapse (dual-quaternion skinning) |
| 4 | 9.4–11.3 | sits back onto the block behind it | `sit` + `rig::clips::seated(build, seat)` | hips and knees at 90 degrees, feet on the floor, seat on the block |
| 5 | 11.3–13.0 | end card over the seated body | `ONE MESH, ANY CLIP.` / `tools/skin_bake.py · rig::loadSkin · rig::add(scene, ch, skin)` | — |

### Reveal map

| Unit | Anchor | Starts | Hold |
| --- | --- | --- | --- |
| kicker + clip label `walk` | first frame of the walk | 0.3 s | to 3.6 s |
| x-ray (body to 35 %, joint dots, label) | mid-walk | 1.6 s | to 3.4 s |
| clip label `run` | 0.3 s before the run blend ends | 3.9 s | to 6.4 s |
| clip label `wave` | the arm starts to rise | 7.3 s | to 9.3 s |
| clip label `sit` | the hips are halfway down | 9.9 s | to 11.2 s |
| end card | the seat has settled | 11.3 s | to the end |

One label at a time; each fades over 8 frames.

## 5. Sources and claims

No claims about the world. The capability shown is a property of the code (`rig::loadSkin`,
`rig::skinMatrices`, the clips). The mesh is a generated body (Hunyuan3D output decimated
by the owner's pipeline), shown untextured in the bake's grey.

## 6. Visual system

Warm paper ground `#f3efe6`, ink `#1f1d2b` type, teal `#16a39a` for the joint dots and the
API line; Inter (the engine's UI font). One camera grammar: a 3/4 follow that settles
frontal for the wave and sit.

## 8. Asset plan

The GLB and its `.rig.json` are not committed (generated, owner's mesh). With no `glb`
prop the composition renders a card that says how to bake one; `list` works anywhere.

## 10. Still frames to review

30 (walk), 75 (x-ray), 150 (run), 240 (wave), 315 (sit landing), 375 (end card).
Thumbnail candidate: 75 (the joints inside the translucent body).

## 11. Acceptance checks

- Feet do not visibly slide during the walk and run; the seated feet rest on the floor.
- The joint dots sit inside the limbs.
- No collapsed shoulders or elbows in the wave; no web between the legs.
- The seat meets the block (no float, no sink beyond a few mm).

## Revisions after the still review

- The wave: `clips::wave` straightens the elbow on each beat, and at that phase the raised
  straight arm read as a salute. The demo holds the elbow bent (85 ± 16 degrees) so the forearm
  stands up and the hand waves; the label says `elbow held`.
- The x-ray label said 17 joints; 16 are drawn (Root is on the floor). Now unnumbered.
- The body is drawn double-sided: the bake splits the legs at the crotch, and at the run
  the far inner thigh opened onto the floor. It now shows the inside of the cut (still visible).
