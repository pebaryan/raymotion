# far-and-near — authoring brief

> Status: `draft`. Written before any composition code. The film is the engine demo for
> raymotion 0.13 (far trees, far canopy, grass shadows, cube skybox, 2D path meadows). It is
> an engine showcase like `flora` and `maps-2d`, not one of the owner's films.

| | |
| --- | --- |
| Composition | `far-and-near` (`examples/showcase/far_and_near.cpp`, `registerFarAndNear`) |
| Story mode | `utility` |
| Owner / agent | the owner set the conceit; Claude (agent) wrote and built it; owner review pending |
| Status | `draft` |
| Runtime target | 48.7 s (1,460 frames; planned 46 s, see section 14) |
| Platform / aspect | 1920x1080, 30 fps, engine demo |
| Voice | none: a utility clip; the labels are the script |
| Audience | someone who makes films with raymotion and wants to know what 0.13 changed for forests and grass, and which call does it |
| Source material | none outside the engine. The forests and the meadow are generated in code. The numbers on screen are the engine's own counters. |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `utility` |
| Goal | be legible in context: show that each 0.13 feature keeps the same picture while the engine draws less, or draws what 0.12 could not |
| Proof object | per chapter, a before/after wipe on the same frame, or an engine counter read from the scene (`g3d::instanceBodiesSubmitted()`, `instanceTrianglesSubmitted()`, `fieldShadowBladesSubmitted()`, the placed tree count) |
| Required evidence | spec: no number is typed in. Bodies, triangles, tree counts and the distance bands are read from the scene or the spec at draw time |
| Text | short labels, one at a time; the API lines on the end card |
| House grammar | brand/utility, as `flora`, `voxel-splats`: caption card (title, note, call), API end card, logo, repo line |
| Packaging | end card with the API lines, raymotion logo and repo line; no SRT or thumbnail required |

## 1c. Reel / feed cut

n/a: 16:9 only.

## 1. One sentence

> This is a film about **one forest seen from every distance** that makes the viewer
> **see that 0.13 keeps a forest believable and cheap from the globe down to a blade of
> grass** by **descending from space to the ground, and at each distance wiping the same
> frame between the old way and the new, with the engine's own counts on screen.**

## 2. One-line argument

> One forest, five distances, and at each one the new picture matches the old (or shows
> what the old one lacked) for a fraction of the bodies. (The figures are read from the
> engine and replace any example on screen.)

## 3. Chapters

Time is film seconds from the start of each scene (the scenes overlap by a 20-frame
dissolve). The camera direction at the end of one scene is the start of the next, so the
dissolve reads as one descent.

| # | Scene length | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 11 s | Globe | `ONE FOREST. EVERY DISTANCE.` kicker, then `100,000 trees · N drawn`; side labels on the wipe | the camera falls from space to 1.2 radii over a boreal forest on a globe, tilting from straight down to 55 degrees | a **wipe** against `impostorBelow = 0, mergeBelow = 0` (every tree a mesh), with both body and triangle counts, and the counter | a low pad opens; a soft pulse as the wipe lands |
| 2 | 11 s | Valley | `meshes · reduced · pictures` bands; side labels `0.12` / `0.13` | the camera, 55 degrees down, flies over a valley forest and tilts up to the horizon; the sky is a cube-face skybox | a **wipe** of `elevations 1, impostorShadows off` against the 0.13 defaults: crowns seen from above and the far trees' shadows; rings on the ground at `reducedFrom` and `impostorFrom` | pad moves up a third; the sky opens as the camera tilts up |
| 3 | 11 s | Low over the forest | `trees only` / `far canopy`; `same picture, X% fewer bodies` | a low camera (about 3 degrees from the forest's middle), the sun low; the camera backs off and rises a little | a **wipe** of trees only against `canopyBelow = 7`, nearly invisible on purpose, with both body counts and the percentage | the pad thins to a single line; a chime on the percentage |
| 4 | 8 s | Ground | `shadows off` / `shadows on`; `shadow blades N` | the camera glides at 0.7 m through a meadow with tall tufts in gusts under a low sun, into it | a **wipe** of `shadowDensity` 1 against 0.35: the tufts' shadows sway with the blades; the shadow-blade count | wind swells; a gust whoosh timed to a gust front |
| 5 | 4 s | 2D coda | `the same meadow, in 2D` | a curved hill with a pond cut out (even-odd path), gusts, 8-sample motion blur on the tips | the hill's own path as the meadow's shape: blades stay inside the curve and out of the pond; tips blur, roots do not | light plucks on the wind |
| 6 | 5 s | End card | `ONE FOREST. EVERY DISTANCE.` and the API lines | dark card; the lines come in one at a time; logo and repo line | none (packaging) | pad resolves on G; bells on the title |

One idea per chapter: 1 the globe stays cheap, 2 the crowns and shadows from above, 3 the
distance is canopy, 4 the grass shades itself, 5 the same grass in 2D. The proof object
column is never "caption".

### 3b. Reveal map (text-only cue map)

Times are seconds from each scene's own start. Lead = how far the visual starts before the
beat it belongs to; every unit holds at least 0.8 s before the next unrelated entrance.

| Scene | Unit | Anchor (beat) | Visual start | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | kicker `ONE FOREST. EVERY DISTANCE.` | scene start / pad opens | 0.2 | 0.6 | 2.2 |
| 1 | counter `100,000 trees · N drawn` | the globe is large in frame | 2.4 | 2.8 | 2.0 |
| 1 | wipe: the before picture comes in from the left | the counter has held | 4.6 | 5.4 | 3.2 hold at the middle |
| 1 | side labels, with each side's bodies and triangles | the divider rests | 5.2 | 5.6 | 2.8 |
| 1 | caption card with the call | the divider rests | 6.2 | 6.6 | 2.6 |
| 2 | band rings and labels (`meshes`, `reduced`, `pictures`) | camera begins to fly | 0.6 | 1.0 | 2.6 |
| 2 | wipe in | the rings have held | 4.0 | 4.8 | 3.4 |
| 2 | side labels `0.12` / `0.13` | the divider rests | 4.6 | 5.0 | 3.0 |
| 2 | caption card | the divider rests | 5.6 | 6.0 | 2.6 |
| 2 | camera tilts up: the sky shows | after the wipe retreats | 8.2 | 9.8 | to the end |
| 3 | counter: trees placed, bodies drawn | scene start | 0.4 | 0.8 | 2.0 |
| 3 | wipe in (near invisible) | the counter has held | 3.4 | 4.2 | 3.6 |
| 3 | side labels with bodies | the divider rests | 4.0 | 4.4 | 3.2 |
| 3 | `same picture, X% fewer bodies` | the divider rests | 5.4 | 5.8 | 2.4 |
| 3 | caption card | after the percentage | 7.0 | 7.4 | 2.4 |
| 4 | caption card | scene start | 0.4 | 0.8 | 2.0 |
| 4 | wipe in: shadows off on the left | the card has held | 2.6 | 3.4 | 2.6 |
| 4 | side labels, shadow blade count | the divider rests | 3.2 | 3.6 | 2.4 |
| 5 | title `the same meadow, in 2D` and call | scene start | 0.2 | 0.5 | 2.4 |
| 6 | title | card start | 0.2 | 0.6 | to the end |
| 6 | API lines, one at a time | after the title | 1.0 | 1.3 + 0.18 each | to the end |
| 6 | logo, repo line | after the last API line | 3.0 | 3.4 | to the end |

One entrance at a time; no scene is at full density at its first frame.

## 4. Script

Voice: none. A utility clip whose labels are its script.

| # | Spoken text | Caption text | Voice seconds | Notes |
| --- | --- | --- | --- | --- |
| 1 | none | `ONE FOREST. EVERY DISTANCE.` / `100,000 trees · N drawn` / `A globe of trees` / `trees under impostorBelow px become pictures, the farthest are merged` / `ForestCover { impostorBelow, mergeBelow }` | n/a | N is read from the engine |
| 2 | none | `A valley from above` / `crowns seen from above, and far trees that cast shadows` / `TreeBuild::elevations · ForestSpec::impostorShadows · Scene::environmentFaces` | n/a | band radii from the spec |
| 3 | none | `Low over the forest` / `where trees are a few pixels tall, one patch a cell` / `ForestSpec::canopyBelow` | n/a | percentage from the two counts |
| 4 | none | `On the ground` / `tufts in the wind, and the shadows move with them` / `Field::shadowDensity` | n/a | |
| 5 | none | `The same meadow, in 2D` / `a curved hill with a pond; the blur is on the tips` / `flora2d::meadow(path)` | n/a | |
| 6 | none | `ONE FOREST. EVERY DISTANCE.` + seven API lines | n/a | |

> `Cue map` (scene starts are the engine's `ts.add` positions; offsets are frames from the
> scene start at 30 fps): scene 1: kicker f6, counter f72, wipe f138-f162, labels f156,
> card f186. Scene 2: rings f18, wipe f120-f144, labels f138, card f168, sky f246-f294.
> Scene 3: counter f12, wipe f102-f126, labels f120, percentage f162, card f210. Scene 4:
> card f12, wipe f78-f102, labels f96. Scene 5: title f6. Scene 6: title f6, API lines f30
> on, logo f90.

## 5. Sources and claims

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| trees on the globe | read from the placed forest (limit 100,000) | `geo3d::forest` placements | counter | exact (the count of placed trees); where they stand is **illustrative**: a Poisson scatter over Natural Earth's country shapes, not a census |
| bodies and triangles drawn | read each frame | `g3d::instanceBodiesSubmitted()`, `instanceTrianglesSubmitted()` | side labels | exact, for this frame and this window size |
| shadow blades | read each frame | `g3d::fieldShadowBladesSubmitted()` | side labels | exact |
| trees in the valley forest | read from the placements | `flora::scatter` | counter | exact; the forest is **illustrative** |
| distance bands | `reducedFrom`, `impostorFrom` read from the spec | `flora::ForestSpec` | ring labels | exact |
| "same picture, X% fewer bodies" | `1 - canopyBodies / treeBodies`, both read | the two scenes' counters | label | exact |
| "0.12" settings | `elevations 1`, `impostorShadows false` | the 0.12 defaults (docs/flora.md; release notes) | side label | exact for those two settings; other 0.13 changes (leaf mipmaps) stay on both sides, and the label says "the 0.12 settings" |

No historical or statistical claim. The sun and sky are art direction.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | cool dark UI ink (`#f2f4ff`, `#b4bfcc`) on a dark scrim, as `flora`; amber (`#ffd27a`) = the call (API text) and the divider; teal (`#3ad6c5`) = a number read from the engine; warm white = the "before" label dimmed, "after" label bright |
| Type | `fonts::ui`: caption title 60, note 28, call 26 (as `flora`); counters 56 for the number, 24 for its unit; end card 96 |
| CJK | n/a |
| Margins / safe area | 64 px; caption card at the lower left, counters at the top, labels top centre on the divider |
| Caption box | the `flora` card: three lines over a soft scrim |
| HUD / persistent frame | one counter row at the top; the wipe divider as a thin amber line with a small handle |
| Motion grammar | 0.4-0.6 s ease-out entrances; the camera moves all film (a descent); the divider eases in/out; no idle motion on text |
| Motif | the divider (a wipe on the same frame) and the descent itself |
| Charts / maps | none; the counters and the wipes are the charts |
| Text performance | short labels; one entrance at a time |
| Brand kit | raymotion logo prefab and repo line on the end card |

**Density rule:** one idea per screen: during a wipe, only the two side labels, the divider
and the caption card.

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| a far tree can be a picture | a wipe: left = every tree a mesh, right = the default | the same frame, the same camera; the divider reveals one or the other | the divider comes in from the left, rests at the middle, goes back; both counts shown under their side |
| distance chooses the level | rings on the ground | ring 1 = `reducedFrom`, ring 2 = `impostorFrom`, from the camera's ground point | the rings draw in once, then stay |
| crowns from above, shadows from far trees | a wipe: left = `elevations 1`, `impostorShadows` off; right = defaults | same camera over the same forest | divider as above |
| a canopy replaces trees where they are tiny | a wipe, nearly invisible | left = trees only, right = canopy; the body counts differ, the picture does not | divider, then the percentage drawn from the two counts |
| grass shades itself | a wipe: left = no grass shadows | tufts' shadows run on the grass between them and sway with the blades | divider; the shadow-blade count |
| a path can be a meadow's shape | the hill's path outline drawn once over the blades | blades root inside the curve, pond bare | outline draws in at the start, then fades |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | generated by `procgen::Score` into `out/far-and-near-assets/score.wav`: a slow pad over G (G, Em, C, D) that thins as the camera comes down; wind noise that swells with the gusts and is loudest in chapter 4; light plucks in chapter 5; bells on the end title |
| Ducking | none: no voice |
| SFX | a soft pulse where each wipe lands, a chime on the percentage, a gust whoosh on the first gust front in chapter 4, bells on the title |
| Silence | the pad drops to wind alone for the ground chapter's first second |
| Loudness target | about -16 LUFS integrated, true peak under -1 dBTP |

The score reads the same scene-start table as the picture, so retiming a chapter moves both.

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Globe forest | up to 100,000 trees over Natural Earth boreal countries, height 0.025 radii | counter 1, wipe 1 | `ForestCover::limit`; illustrative |
| Valley forest | 2.4 km x 3.6 km, one scatter (spacing 7 m, clumped), 4 species | chapters 2 and 3: the same placements | `flora::scatter`; illustrative |
| Camera in chapters 2-3 | 45 m up in the valley, 3 degrees from the forest's middle in chapter 3 | far canopy only below `canopyMaxElevation` (20 degrees) | engine limit, documented |
| Sun | 6-8 degrees above the horizon, from the skybox's sun | long shadows in chapters 2-4 | the skybox's own sun |
| Meadow | 60 m x 60 m, tufts 1.4 m tall, density 400/m2 | chapter 4 | illustrative |
| Time of day | dusk-gold throughout chapters 2-4 | one sky | the six cube faces in `assets/textures/skybox-example/` |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | `c.assets`, from the code | `out/far-and-near-assets/score.wav` | regenerated on demand |
| Skybox faces | committed | `assets/textures/skybox-example/{px,nx,py,ny,pz,nz}.png` | the scene throws a named error; `list` never loads them |
| Forests, grass, globe map | built in code, lazily, at the first draw | n/a | n/a |
| Logo | prefab `raymotion-logo` | engine | n/a |

**Clean-machine test:** with `out/` absent the film regenerates its score;
`raymotion list` works from any directory, and the forests are not built by `list`
(measured: list time before and after is recorded in the QC).

## 9. Cue map

| Scene-local frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 1: 0 | descent begins | pad opens | node start |
| 1: 138 | before picture wipes in | soft pulse | the wipe table |
| 2: 18 | band rings | pad up | node start |
| 2: 120 | wipe | soft pulse | the wipe table |
| 3: 102 | wipe | soft pulse | the wipe table |
| 3: 162 | percentage | chime | the wipe table |
| 4: 0 | meadow | wind alone, then swells | node start |
| 4: 78 | wipe | soft pulse | the wipe table |
| 5: 0 | 2D hill | plucks | node start |
| 6: 6 | end title | bells | node start |

The scene starts and the wipe timings live in one table; the picture and the score both
read it.

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| scene-1 start, middle, end, wipe middle | globe | the globe reads as a globe, trees are visible as a green cover at the end; the wipe divider is sharp; both counts show |
| scene-2 start, wipe middle, end | valley | crowns from above, shadows, rings and labels not overlapping the divider's labels; the sky visible at the end |
| scene-3 start, wipe middle, end | low | the two sides look alike; the counts differ; the percentage reads |
| scene-4 start, wipe middle, end | ground | tufts and long shadows; shadows missing on the left of the divider |
| scene-5 start, middle, end | 2D | the hill, the pond bare; tips blurred |
| scene-6 middle, end | end card | title, API lines, logo, repo line inside the safe area |
| each transition middle | all | one descent, not a cut between two unrelated pictures |

Thumbnail candidate: the chapter-2 wipe middle (crowns and shadows, `0.12` against `0.13`,
the divider across a golden valley) or chapter 1's globe wipe; chosen after the stills.

### 10b. Scene review ledger

| Scene | Review asset | Reveal / sync notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 1 | stills | | pending | |
| 2 | stills | | pending | |
| 3 | stills | | pending | |
| 4 | stills | | pending | |
| 5 | stills | | pending | |
| 6 | stills | | pending | |

## 11. Acceptance checks

- [ ] Muted, the argument is legible: one descent, a wipe per distance, the counts.
- [ ] Each chapter's proof object is visible at its stated frame.
- [ ] Every scene starts sparse and reveals one unit at a time with a lead and a hold.
- [ ] Every scene approved individually.
- [ ] The executable is newer than the last source edit; the final render was rebuilt
      after the last approved change.
- [ ] Every number on screen is read from the engine.
- [ ] Labels inside the safe area; the side labels never over the caption card.
- [ ] Score and picture share the cue map.
- [ ] `list` works from another directory, with `out/` absent, and its time is unchanged.
- [ ] The camera clearance is logged for every frame.
- [ ] Duration, frame count, loudness and peak recorded.
- [ ] Packaging placed (end card, logo, repo line).
- [ ] Authoredness >= 22 and Craft >= 13 (author-only review: Verification, Spec-to-screen
      and Revision at most 2).

## 12. Risks / non-goals

- **Risks:** the canopy needs a very large forest and a wide lens to hand over (trees under
  7 px); the "same picture" wipe in chapter 3 must be honest, not tuned until it passes;
  four forests in memory; first-frame build time of 100,000 and 90,000 trees; the 0.12
  side is the 0.12 settings, not the 0.12 binary.
- **Non-goals:** the weather and water of 0.14; speaking characters; the maps-2d
  great-circle flourish (left out to keep the 2D coda short).
- **Out of scope:** a high camera over the canopy (documented limit), grass
  `shadowDensity` below 1 near the camera (documented limit).

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

Recorded after the build; each one is also in the QC.

- **Length.** Scene 1 grew from 330 to 390 frames and the film from 1,400 to 1,460 frames
  (48.7 s). Reason: trees are only a few pixels tall (where the new levels pay) from about 6.6
  radii out, so the wipe sits at 6.8 radii and the close-in dive got its own 4 s at the end.
  The scene starts are 0 / 370 / 680 / 990 / 1,210 / 1,310 frames; the code asserts that they
  match the series.
- **Chapter 1 wipe.** Reveal map times moved: wipe in at f126, rest f150, out f252; the
  kicker holds to f100; the counter (now at y 214, under the kicker) enters at f60. The tree
  count (100,000) is read from the placed forest. Seen at 4.8 radii (the dive) the 0.13 forest
  draws *more* bodies than all-meshes (199,235 against 166,739), because trees between 10 and
  12 px are drawn twice during their cross-fade; the wipe is held where the saving is real
  (about 7 px: 50,433 against 166,739 bodies, 0.83 against 46.0 million triangles).
- **Chapter 2.** The wipe's side labels show no counts (the bodies and triangles are alike
  to within 40%, and neither is the proof: the picture and the shadows are). The camera is 55
  m over the trees, not 40, `impostorFrom` is set to 70 m (the default is 80) so the ring
  falls mid-frame, the sun is 16 degrees up (not 7: at 7 the long shadows stairstepped), and
  the shadow extent is 150 m. The 0.12 side is the two 0.12 settings, not the 0.12 binary;
  the label says so.
- **Chapter 3.** The forest is 3 km x 3.6 km (92,135 trees). The camera is 295 m up at 10
  degrees from the forest's middle (not "3 degrees"): at 3 degrees the far forest is a strip
  a dozen pixels high. The lens is 75 degrees. The wipe reports bodies (56% fewer) and
  triangles (2.6 times as many: a patch is a dense height field), and the label says both.
- **Chapters 2 and 3 share one camera path** (`valleyPose`), so the dissolve is a continuation
  of the same crane move up from the forest.
- **Chapter 4 (revised after review).** The first build framed `castShadows` off against on as
  before and after; that was false (grass cast shadows in 0.12, and castShadows defaults to true).
  It is now `shadowDensity` 1 against 0.35 with `fieldShadowBladesSubmitted()` counts. Chapter 3
  now says "56% fewer draws, 2.6x the triangles" in the headline, its caption enters before the
  wipe, and ch2/ch3 get a horizon haze. The wind swell
  in the score is not synced to a gust front (the engine's gust phase is not exposed).
- **Chapter 5.** The great-circle route flourish was left out.
