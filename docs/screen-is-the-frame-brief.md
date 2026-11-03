# screen-is-the-frame — authoring brief

> Status: `draft`, self-reviewed by the agent against `authoring-standard.md` §10; the
> owner asked for the film straight after the engine work ("do the film then back to the
> engine"), so owner review happens at the stills. The film is the demo for `g3d::Surface`,
> `g3d::fillCamera`, `g3d::fillCtx` and `g3d::mix`. No external asset: everything is built
> in code.

| | |
| --- | --- |
| Composition name | `screen-is-the-frame` |
| Story mode | `utility` (a feature demo, as `voxel-splats` and `zombie-horde`) |
| Owner / agent | pebaryan / Claude |
| Status | `draft` |
| Runtime target | 22 s (660 frames) |
| Platform / aspect | 1920x1080, 30 fps, presentation / feature demo |
| Voice | none: a utility clip; the labels and the live readout are the script |
| Audience | someone who makes films with raymotion and has only put 2D *over* 3D so far. After it they can say: "I can draw an app once and it can be an object in the scene, the whole frame, or a stack of layers, and the change between them doesn't show." |
| Source material | none; the app is fictional and drawn in code. Engine facts come from `tests/scene3d_tests.cpp` (`surfaces()`) |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `utility` |
| Goal | be legible in context: show what a Surface is and the three places one drawing can live |
| Proof object | the app's own live screen, plus a readout of the surface's texture density read from `ui->resolution` every frame (not typed in), and a `3D`/`2D` tag that flips while the picture does not change |
| Required evidence | spec: the numbers are read from the engine; the "same pixels" claim is the engine test (mean difference below 1/255) |
| Text | one kicker per chapter, one at a time; the readout; layer labels that are themselves surfaces; API lines on the end card |
| House grammar | brand/utility: kicker, live readout, API end card, logo, repo line |
| Packaging | end card with the API lines, raymotion logo and repo line; thumbnail candidate; no SRT (no voice) |

## 1c. Reel / feed cut

n/a: 16:9 only. (A portrait phone in a landscape frame is the point: the break-out in
chapter 3 uses the width.)

## 1. One sentence

> This is a film about **one phone screen** that makes the viewer **see that an app drawn
> once in 2D can be an object on a desk, the whole frame, and a stack of layers** by
> **flying into the screen until it is the frame, letting the app break out of the phone,
> and then pulling it apart into its layers.**

## 2. One-line argument

> The screen is the frame: the same drawing is sharp on a desk, takes over the picture
> with no visible cut, and comes apart into layers you can walk round.

## 3. Chapters

| # | Time (s) | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0.0-5.0 | the object | kicker `A 2D app, on a desk` | a phone lies on a desk, slightly tilted, in a slow arc; its app is live: a bus-arrival screen whose countdown ticks and whose bus dot moves along the route | the live app on the phone, plus the readout `texture 1.00×` (texture px per design px), read each frame | soft pad; a tick per countdown second |
| 2 | 5.0-10.0 | the push | kicker `Fly in` | the camera eases square to the screen and in until the screen fills the frame top to bottom; the readout climbs as it goes; at 10.0 the `3D` tag flips to `2D` and the picture does not move | the readout climbing (1.00× → 2.00×; `sharpness` 1.5) and the tag flipping over an unchanged picture | a rising whoosh; a soft click exactly at the flip |
| 3 | 10.0-14.5 | the break-out | kicker `Now it is the frame` | drawn in 2D now: the map card widens out of the phone to the full 16:9 frame, the bus reaches the stop (`Now`), and the card folds back into the phone | the map card at full width: impossible for a texture on the phone | a swell as the card opens; a chime when the bus arrives; the swell reverses as it folds |
| 4 | 14.5-19.5 | the layers | kicker `One drawing, in layers` | the tag flips back to `3D`; the camera pulls back and swings to a three-quarter view as the screen separates into four layers (backdrop, map, card, text) that stand apart; a label beside each layer, added one at a time, is hidden whenever a layer passes in front of it | the exploded stack; a label disappearing behind the layer in front | a pop per layer; a low pulse under the orbit |
| 5 | 19.5-22.0 | end card | `THE SCREEN IS THE FRAME` / `g3d::Surface · g3d::fillCamera · g3d::fillCtx` / logo + repo | the layers close back onto the phone under the card | none | pad resolves; an impact and bells on the title |

One idea per chapter: 1 it is an object, 2 it can become the frame without a seam, 3 once
it is the frame it is free of the phone, 4 it is made of layers that live in 3D. Each
chapter has its own picture: an arc round an object, a straight push, a flat 2D animation,
an orbit round a stack.

### 3b. Reveal map (text-only cue map)

| Scene | Unit | Anchor | Visual start | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | phone + live app | frame 0 (already in motion) | 0.0 | 0.0 | — |
| 1 | kicker `A 2D app, on a desk` | after the first countdown tick | 0.5 | 0.8 | 3.6 |
| 1 | readout `texture N×` | after the kicker has been read | 2.2 | 2.5 | until 14.3 |
| 2 | kicker `Fly in` | the push starts | 5.0 | 5.3 | 3.5 |
| 2 | `3D` → `2D` tag flip | the camera arrives (10.0) | 9.8 | 10.0 | 1.5 |
| 3 | kicker `Now it is the frame` | the card starts to widen | 10.2 | 10.5 | 3.0 |
| 3 | bus `Now` + chime | the dot reaches the stop | 12.3 | 12.6 | 1.4 |
| 4 | `2D` → `3D` tag | the cut back (14.5) | 14.3 | 14.5 | 1.0 |
| 4 | kicker `One drawing, in layers` | the stack starts to open | 14.8 | 15.1 | 3.5 |
| 4 | labels backdrop / map / card / text | one per layer, 0.5 s apart | 15.6 | 15.9 … 17.4 | until 19.4 |
| 5 | title, then API line, then logo + repo | card | 19.6 | 19.9 | to end |

One entrance at a time; no scene starts at final density (chapter 1's frame 0 shows only
the phone and its live screen); text leads its sound cue by 0.2-0.3 s.

## 4. Script

Voice: none. A utility clip whose kickers and readout are the script:

1. `A 2D app, on a desk`
2. `Fly in`
3. `Now it is the frame`
4. `One drawing, in layers`
5. `THE SCREEN IS THE FRAME` / `g3d::Surface · g3d::fillCamera · g3d::fillCtx`

Cue map: see section 9; every picture and sound cue reads the same constants in the
source (`PUSH_AT`, `ARRIVE`, `BREAK_OUT`, `BUS_NOW`, `FOLD`, `BACK_3D`, `OPEN`, `CARD_AT`).

## 5. Sources and claims

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| texture density | read from `ui->resolution` each frame | the engine at render time | readout | exact (what the renderer used) |
| the cut does not show | mean difference < 1/255 between the 3D fill and the direct 2D draw | `tests/scene3d_tests.cpp`, `surfaces()` | no (the picture is the evidence) | exact, for the test scene |
| phone size | 71.5 x 154.7 mm screen area, 390 x 844 design px | a typical 6.1-inch phone; the app is fictional | no | illustrative |
| the app, its stops and its times | `Halte`, stops named after flowers (`Melati` …), a 13 s countdown | fictional | small `fictional app` tag in chapter 1 | illustrative |

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | INK `#14202b` = the film's own text and HUD; BONE `#f1ede2` = kickers; TEAL `#2fc4b2` = the engine (readout, API, labels); AMBER `#f0a43c` = the bus (in the app only); the app's own palette is white cards on a pale map, its own blue `#2b6cf6`. Never teal inside the app. |
| Type | kickers 44 px UI bold, tracked 2; readout 26 px code font; layer labels 22 px code font on an INK chip; end title 84 px bold; API line 28 px code |
| CJK | n/a |
| Margins / safe area | 96 px left/right, 80 px top/bottom |
| Caption box | kicker at (96, 120), one line; readout bottom-left (96, H-80) |
| HUD / persistent frame | the readout and the `3D`/`2D` tag, bottom-left, chapters 1-4 |
| Motion grammar | entrances 9-12 frames ease-out; one camera move per chapter (arc, push, none, orbit); no idle motion in the HUD |
| Motif | the phone screen's rounded rectangle (the app's frame), the shape the card breaks out of and folds back into |
| Charts / maps | the app's map: a stylised route line with five stops; no real geography claimed |
| Text performance | no karaoke; one kicker at a time |
| Brand kit | raymotion logo prefab + repo line on the end card, as `voxel-splats` |

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| sharp at any distance | a readout | number = texture px per design px | the number climbs as the camera pushes in |
| no visible cut | a tag over an unchanged picture | `3D` = rendered on the phone; `2D` = drawn into the frame | the tag flips; the picture stays still |
| free of the phone | the map card | the phone's screen rectangle vs the 16:9 frame | the card widens past the phone's edges, then folds back |
| made of layers | an exploded stack | one layer per part of the drawing | the layers separate in depth; labels hide behind layers in front |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | procedural (`procgen::Score`), as the other demo films: a soft pad (chapter 1), a riser into the push, a brighter pad for the break-out, a low pulse under the orbit, a resolve on the card |
| Ducking | n/a (no voice) |
| SFX | tick per countdown second in chapters 1-2 (quiet); click at `ARRIVE`; swell at `BREAK_OUT`; chime at `BUS_NOW`; reverse swell at `FOLD`; click at `BACK_3D`; four pops at the labels; impact + bells at `CARD_AT` |
| Silence | none |
| Loudness target | about -18 LUFS integrated, peak ≤ -1 dBFS (as the other demo films) |

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| screen | 0.0715 x 0.1547 m, 390 x 844 px | same aspect (0.462) so fillCtx and fillCamera agree | illustrative |
| fill | the screen's height fills 1080 px | 1080 / 844 = 1.28 design → frame; × sharpness 1.5 = 1.92 → quarter-octave step 2.00× | computed |
| countdown | 0:13 → `Now` at `BUS_NOW` | the app's clock is the surface's clock | fictional |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | generated | `out/screen-is-the-frame-assets/score.wav` via `c.assets` | regenerated on render |
| Everything else | code | the example | n/a |

**Clean-machine test:** delete the assets folder; `list` still works (no assets made at
registration), and `render` regenerates the score.

## 9. Cue map

| Time (s) | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 0.0 | phone in an arc, app live | pad; ticks | `0` |
| 5.0 | push starts | riser | `PUSH_AT` |
| 10.0 | camera arrives; `3D`→`2D`; switch to direct 2D | click | `ARRIVE` |
| 10.3 | map card starts to widen | swell | `BREAK_OUT` |
| 12.6 | bus at the stop: `Now` | chime | `BUS_NOW` |
| 13.4 | card folds back | reverse swell | `FOLD` |
| 14.5 | back to 3D; pull out begins | click | `BACK_3D` |
| 15.0 | layers start to open | low pulse | `OPEN` |
| 15.9 … 17.4 | labels, 0.5 s apart | pops | `LABELS_AT`, `LABEL_DT` |
| 17.2 | drift right: front layers slide over the rear labels | — | `DRIFT` |
| 19.5 | end card | impact, bells | `CARD_AT` |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 0 | 1 | only the phone and its live app; no text yet |
| 90 | 1 | kicker readable; app text legible on the phone; `fictional app` tag |
| 225 | 2 | mid-push: app sharp, readout higher than at 90 |
| 299 / 300 | 2/3 | the frames either side of the cut look the same; tag flips |
| 360 | 3 | the card spans the frame; the phone bezel is covered by it |
| 380 | 3 | `Now` |
| 434 / 435 | 3/4 | the cut back: same picture, tag flips |
| 540 | 4 | four layers apart, all four labels in depth order |
| 575 | 4 | the backdrop label's leader runs under the map layer, its dot hidden |
| 640 | 5 | end card complete; logo; repo line |

Plus encoded frames from the final MP4 at 3, 10, 12, 17 and 21 s.

## 10b. Scene review ledger

Filled in `docs/screen-is-the-frame-qc.md`.

## 11. Acceptance checks

- [ ] Muted, the four ideas read from the pictures and the kickers.
- [ ] The readout is the engine's number; it climbs during the push.
- [ ] The frames either side of each cut match (checked by a still pair and a pixel
      difference).
- [ ] The card visibly leaves the phone's outline in chapter 3.
- [ ] A label is visibly hidden by a layer in front of it in chapter 4.
- [ ] The app's text is legible on the phone in chapter 1 (no smear, no shimmer).
- [ ] `list` works from another directory; assets regenerate.
- [ ] Duration 660 frames, ffprobe, full decode, loudness recorded.
- [ ] Authoredness ≥ 22 and Craft ≥ 13 (utility).
- [ ] Thumbnail: frame 540 (the open stack with its four labels), readable at 320x180.

## 12. Risks / non-goals

- **Risks:** a portrait screen in a landscape frame leaves the sides of the frame to the 3D
  scene at the cut: they must stay identical, so the 3D scene keeps rendering under the 2D
  layer after the cut. Text shimmer on the phone at a grazing angle (sharpness 1.5).
  Canvas cost: four 390x844 layers at up to 4x density.
- **Non-goals:** no real app, brand or transit data; no device model detail beyond a body,
  bezel and screen.
- **Out of scope:** 3D lines and diagrams (the next engine step); device prefabs.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | agent self-review against §10 | 2026-11-01 | approved for code |
| Stills | owner | | |
| Final QC | owner | | |

## 14. Changes during the build

- Stop names are invented (flower names) rather than real Jakarta stops, so the fictional
  app makes no transit claim; the countdown is 13 s so it reaches `Now` inside the film.
- Readout values are the engine's: 1.00x on the desk, 2.00x at the fill, because the
  screen asks for `sharpness` 1.5 (small text at a slant).
- Chapter 4: the labels were on the layers' left edges and the camera swung only left,
  so nothing hid them. Now the camera goes out to the left (where the stack reads as depth)
  and from `DRIFT` drifts right, sliding the map layer over the backdrop label's leader.
  The labels come 0.5 s apart from 15.9 s so all four are read before the drift.
- No glass box under the screen: it sat in the screen's plane and z-fought with it.
