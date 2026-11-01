# voxel-splats — authoring brief

> Status: `draft`. Written before any composition code; the film is the demo for the new
> `voxel::` and `splat::` modules. No external asset: everything is built in code.

| | |
| --- | --- |
| Composition name | `voxel-splats` |
| Story mode | `utility` |
| Owner / agent | Claude (agent); owner review pending |
| Status | `draft` |
| Runtime target | 20 s |
| Platform / aspect | 1920x1080, 30 fps, presentation / feature demo |
| Voice | none: a utility clip; the labels are the script |
| Audience | someone who makes films with raymotion; after it they can say what a voxel model and a splat cloud are here and which call makes each |
| Source material | none; the island is built in code (`voxel::Grid`) |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `utility` |
| Goal | be legible in context: show what the two modules do and how they are called |
| Proof object | counters that are the scene's own numbers: voxels, quads after greedy meshing, splats; one splat enlarged to show the Gaussian |
| Required evidence | spec: the numbers shown are read from the grid, the mesh and the cloud, not typed in |
| Text | short labels, one at a time, then the API lines on the end card |
| House grammar | brand/utility, as `crowd-demo` and `zombie-horde`: kicker, label, API end card, logo, repo line |
| Packaging | end card with the API lines, raymotion logo and repo line; no SRT or thumbnail required |

## 1c. Reel / feed cut

n/a: 16:9 only.

## 1. One sentence

> This is a film about **one island made twice** that makes the viewer **see that a voxel
> model and a splat cloud are two ways to hold the same shape** by **dissolving the
> greedy-meshed voxel island into a cloud of Gaussians that is built from it, with the
> counts of each on screen.**

## 2. One-line argument

> One island, two representations: 6,000 voxels become 1,100 quads, then 6,000 soft
> Gaussians. (The figures are read from the build and replace these on screen.)

## 3. Chapters

| # | Time (s) | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0.0-6.0 | voxels | `voxel::Grid` + `N voxels` then `M quads` | a slow orbit round a lit, shadowed voxel island in water; the greedy mesh's quads are outlined for a moment | the counter: voxels, then the (much smaller) quad count after meshing | low pad; a tick per block as the island builds |
| 2 | 6.0-12.0 | splats | `splat::Cloud` + `N splats` | the camera pushes in; the island is now a cloud of soft Gaussians; one is enlarged beside the island | the enlarged splat, drawn as its 2D ellipse with its sigma rings, next to the cloud | a soft airy bed; a chime on the enlarged splat |
| 3 | 12.0-17.0 | one into the other | `same shape, two ways` | a dissolve sweeps across: voxel faces disperse into splats and the splats gather into the voxels again; the camera orbits so the splats' colour shifts with view | a split frame count: voxel quads falling, splats rising, always the same shape | a riser into the sweep, a flutter as the dissolve crosses |
| 4 | 17.0-20.0 | end card | `ONE ISLAND. TWO WAYS.` / `voxel::mesh  ·  splat::fromVoxels` / logo + `github.com/pebaryan/raymotion` | the camera settles over the island; both forms side by side under the card | none | pad resolves; an impact and bells on the title |

One idea per chapter: 1 the voxel form, 2 the splat form, 3 they are the same shape. The
proof object column is never "caption".

### 3b. Reveal map (text-only cue map)

| Scene | Unit | Anchor | Visual start | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | kicker `voxel::Grid` | start | 0.3 | 0.6 | 1.5 |
| 1 | voxel counter | the island finishes building | 1.6 | 2.0 | 1.6 |
| 1 | quad outline + quad count | greedy meshing | 3.6 | 4.0 | 1.8 |
| 2 | kicker `splat::Cloud` | the push-in | 6.2 | 6.5 | 1.5 |
| 2 | splat counter | the cloud forms | 7.6 | 8.0 | 1.5 |
| 2 | enlarged splat + rings | the camera settles | 9.0 | 9.4 | 2.4 |
| 3 | label `same shape, two ways` | the dissolve starts | 12.2 | 12.5 | 2.0 |
| 3 | split counts | mid-dissolve | 13.6 | 14.0 | 2.5 |
| 4 | title, API lines, logo, repo | card | 17.2 | 17.5 | 2.5 |

One entrance at a time; every unit leads its sound by 0.2-0.4 s; no scene is at full
density at its first frame.

## 4. Script

Voice: none. A utility clip whose labels are its script.

| # | Spoken text | Caption text | Voice seconds | Notes |
| --- | --- | --- | --- | --- |
| 1 | none | `voxel::Grid` / `N voxels` / `M quads` | n/a | counts read from the build |
| 2 | none | `splat::Cloud` / `N splats` | n/a | |
| 3 | none | `same shape, two ways` | n/a | |
| 4 | none | `ONE ISLAND. TWO WAYS.` / `voxel::mesh  ·  splat::fromVoxels` | n/a | |

> `Cue map`: kicker 1 -> f9; voxel counter -> f48; quad outline -> f108; kicker 2 -> f186;
> splat counter -> f228; enlarged splat -> f270; dissolve -> f366; title -> f525.

## 5. Sources and claims

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| voxel count | read from the grid | `voxel::Grid` | counter | exact |
| quad count | read from the mesh | `voxel::mesh` | counter | exact |
| splat count | read from the cloud | `splat::Cloud` | counter | exact |
| Gaussian falloff `exp(-d^2/2)` | formula | 3D Gaussian Splatting (Kerbl et al., 2023) | the sigma rings | exact |

No historical or statistical claim. The island is not a real place.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | dusk-to-day sky; voxel palette of sand, grass, rock, wood; teal = a counter or overlay on the scene; amber = the thing being explained (the enlarged splat, the dissolve front) |
| Type | as `zombie-horde`: kicker 34, label 44, counters 96, end card 88 |
| CJK | n/a |
| Margins / safe area | 96 px |
| Caption box | one line, lower left, over a soft scrim |
| HUD / persistent frame | the counter pair in the top left, one counter at a time |
| Motion grammar | 0.4 s ease-out entrances; the camera orbits all film; no idle motion on text |
| Motif | the island |
| Charts / maps | none; the counters are the charts |
| Text performance | captions are short labels, not sentences; one at a time |
| Brand kit | raymotion logo prefab and repo line on the end card |

**Density rule:** one idea per screen.

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| greedy meshing merges faces | outlined quads over the voxel faces | each outline is one quad covering several voxel faces | the outlines draw in after the voxel counter, the quad count follows |
| a splat is a soft ellipse | one splat enlarged beside the cloud | its rings are 1, 2, 3 sigma | the rings expand out from the centre |
| the two forms are one shape | a dissolve front sweeping across the island | left of the front splats, right of it voxels, then reverses | the front moves; counts trade places |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | generated by `procgen::Score` (pads, a pulse, a riser at the dissolve, bells on the title) |
| Ducking | none: no voice |
| SFX | tick per block as the island builds (placed from the build order), chime on the enlarged splat, flutter across the dissolve front, impact and bells on the title |
| Silence | a beat of near silence before the dissolve |
| Loudness target | -16 LUFS integrated, sample peak <= -1 dBFS |

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Island size | about 64 x 24 x 64 voxels | voxel count, quad count | built in code |
| Voxel count | read from the grid | counter 1, splat count | exact |
| Quad count | read from `voxel::mesh` | counter 1, split counts | exact |
| Splat count | read from the cloud (one per surface voxel, with a few jittered copies) | counter 2, split counts | exact |
| Dissolve | a plane sweeping along +x from 12.5 to 15.5 s | split counts | the same sweep drives both forms |

A change to the island recomputes every count, because they are read, not typed.

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | `c.assets`, from the build | `out/voxel-splats-assets/score.wav` | synthesised on demand |
| Island `.vox` | `c.assets`, written then loaded back | `out/voxel-splats-assets/island.vox` | rewritten on demand |
| Splat cloud | built in code, no file | n/a | n/a |

**Clean-machine test:** with `out/` absent the film regenerates the score and the `.vox`;
`list` works from any directory and never needs them.

## 9. Cue map

| Frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 9 | kicker 1 | pad enters | node start |
| 48 | voxel counter | last block tick | the build order |
| 108 | quad outlines | soft pulse | node start |
| 186 | kicker 2, push-in | pad opens | node start |
| 270 | enlarged splat | chime | node start |
| 366 | dissolve starts | riser, flutter | the sweep's position |
| 525 | title | impact, bells | node start |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 15 | voxels | island half built, kicker only, shadows on the island |
| 90 | voxels | counter and island, water and caustics round it |
| 130 | voxels | quad outlines readable, quad count shown |
| 200 | splats | the cloud reads as the island; soft, no holes |
| 285 | splats | the enlarged splat and its rings clear of the cloud |
| 380 | one into the other | the dissolve front sharp, voxels on one side, splats on the other |
| 440 | one into the other | view-dependent colour visible between two orbit angles |
| 560 | end card | title, API lines, logo, repo line in the safe area |

Plus a transition still, and an encoded frame from the final MP4.

### 10b. Scene review ledger

| Scene | Review asset | Reveal / sync notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 1 | stills | | pending | |
| 2 | stills | | pending | |
| 3 | stills | | pending | |
| 4 | stills | | pending | |

## 11. Acceptance checks

- [ ] Muted, the argument is legible: one island, two forms, same shape.
- [ ] Each chapter's proof object is visible at its stated frame.
- [ ] Every scene starts sparse and reveals one unit at a time with a lead and a hold.
- [ ] Every scene approved individually.
- [ ] The executable is newer than the last source edit; the final render was rebuilt
      after the last approved change.
- [ ] Every number on screen is read from the build.
- [ ] Captions inside the safe area, not over the counters.
- [ ] Score and picture share the cue map.
- [ ] `list` works from another directory and with `out/` absent.
- [ ] Duration, frame count, loudness and peak recorded.
- [ ] Packaging placed (end card, logo, repo line).
- [ ] Authoredness >= 22 and Craft >= 13.

## 12. Risks / non-goals

- **Risks:** the splat cloud must read as the island without a captured scene's detail;
  1M-splat sort time; the dissolve must look like a hand-off, not two overlaid scenes.
- **Non-goals:** loading captured scenes in the film (the loader is tested, not shown);
  lighting or shadows on splats; training or editing splats.
- **Out of scope:** SH above degree 1, GPU sorting, animated splats (only the dissolve).

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | owner | | pending |
| Script / cue map | | | |
| Stills | | | |
| Staged reveal / sync | | | |
| Draft | | | |
| Final QC | | | |

## 14. Changes made during the build

- The final chapter is a second sweep that stops at the middle, so the island ends half
  voxel and half splat (the end card sits over it), not a full reverse sweep.
- The island rises out of the sea through a clip plane with a glowing edge (the build
  itself is the first proof) instead of appearing whole.
- The merged top faces are drawn as quad outlines over the island; the counters (voxels
  per layer, quads, faces, splats, and the live split during the second sweep) are read
  from the grid, the mesh and the cloud, not typed in.
- The enlarged splat shows its 1, 2 and 3 sigma rings, stacked from `exp(-d^2/2)`.
- `fromVoxels` uses 9 splats a face (a coarser setting looked out of focus at 1080p).
- Sky clouds are off: at some camera angles they banded into chevrons.
- `withAlpha` takes a multiplier, not 0-255; the film uses a local `ca(colour, 0..255)`.
