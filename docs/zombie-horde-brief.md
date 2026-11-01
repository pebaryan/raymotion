# zombie-horde — authoring brief

Status: `draft`. The owner asked for "rendering optimization (instancing), target zombie
horde scene with bats swarm", at "~500 zombies + ~1000 bats", as "a short utility/promo piece
about 20 s". The film is the target and the proof of the instancing work: it must
run at a cost the old per-object path could not.

| | |
| --- | --- |
| Composition name | `zombie-horde` |
| Story mode | `utility`: a capability piece for instanced crowds, with no claims about the world |
| Owner / agent | pebaryan / Claude |
| Status | `draft` |
| Runtime target | 20 s (600 frames) |
| Platform / aspect | 1920x1080, 30 fps, presentation / feed |
| Voice | none: the images and a procedural score carry it; the on-screen text is a text-only cue map (section 4) |
| Audience | the owner and people evaluating the engine. Afterwards they can say: "a horde of 500 and a swarm of 1000 are drawn from a few baked bodies, and each one still walks, flaps and steers on its own." |
| Source material | none; staged in code (`examples/showcase/zombie_horde.cpp`) |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `utility` |
| Goal | be legible in context: show that the crowd is large, individual and cheap |
| Proof object | a counter of bodies in frame (exact, from the bake) and a baked-strip gauge for one highlighted zombie and one bat |
| Required evidence | the profile table in the QC (draw calls and ms a frame, before and after) |
| Text | a kicker, one beat label at a time, a code line on the end card; no sentences |
| House grammar | brand/utility, as `crowd-demo` |
| Packaging | end card with the raymotion logo and the GitHub line; thumbnail frame; no SRT (no speech) |

## 1c. Reel / feed cut

`n/a`: 16:9. A vertical cut is a non-goal.

## 1. One sentence

> This is a film about **a horde of 500 zombies and a swarm of 1000 bats** that makes the
> viewer **see that the crowd is made of a few baked bodies, each played at its own
> moment**, by **a counter that climbs with every body that enters the frame and a strip of
> baked poses with one pointer per highlighted body.**

## 2. One-line argument

> Five hundred zombies are four bodies and one strip of poses, each playing it at its own
> phase.

## 3. Chapters (one street, one travelling camera)

| # | Time (s) | Beat | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0.0–5.0 | dusk, a street at low angle: eight zombies shamble towards the camera, full rig characters with faces, a barricade and a street lamp behind them | `horde` + `8 bodies` | the camera holds low; the eight come on, then the camera cranes up and back and the street behind them is full | the counter: `8` at the first frame, then it climbs as the crane shows the rest, and settles at the number in frame | a low drone; shuffling steps placed from the bake, near ones louder and panned by where the camera sees them |
| 2 | 5.0–11.0 | high three-quarters over the whole street: 500 zombies flow round the barricade and the lamp, some stopping, some lurching, in four bodies | `horde` + `500 · 4 bodies · 1 strip` | the mass flows round the barricade; one zombie is ringed | the strip: eight baked poses of the shamble in a row across the bottom; a pointer rides along it at the ringed zombie's own phase, and the ring follows the zombie | a heartbeat pulse that quickens with the mass's mean speed; groans on the lurchers |
| 3 | 11.0–16.0 | the camera tilts up to a belfry; 1000 bats pour out and wheel over the horde in a swirling mass | `swarm` + `1000 bats` | the swarm streams out of the belfry, spreads, and banks round the tower | a second strip, six baked wingbeats, with a pointer at one ringed bat's phase; the counter now reads `1500 bodies` | a flutter bed that thickens with the number of bats in the air; a stinger as they leave the belfry |
| 4 | 16.0–20.0 | end card over the street | `500 + 1000. ONE STRIP EACH.` / `crowd::bake  ·  crowd::addInstanced` / the raymotion logo + `github.com/pebaryan/raymotion` | the camera settles; the horde walks on under the card | none | the drone resolves; an impact and bells on the title |

Rules from the template: one idea per chapter (beat 1 the scale, beat 2 the construction,
beat 3 the second creature); the proof object column never reads "caption".

### 3b. Reveal map (text-only cue map)

| Scene | Unit | Anchor | Visual start | Lands at | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | kicker `HORDE` + `baked once, sampled per frame` | first frame | 0.3 s | 0.6 s | to 19.5 s |
| 1 | label `horde` and the counter at `8` | the eight are walking in | 0.6 s | 0.9 s | to 4.8 s |
| 1 | counter climbs | the crane starts to show the street behind | 2.6 s | 3.0 s | to 4.8 s |
| 2 | label `500 · 4 bodies · 1 strip` | the high shot has settled | 5.2 s | 5.5 s | to 10.8 s |
| 2 | the strip | after the label has been up | 6.1 s | 6.4 s | to 10.8 s |
| 2 | ring and pointer on one zombie | after the strip is in | 7.2 s | 7.5 s | to 10.8 s |
| 3 | label `swarm · 1000 bats` | the tilt passes the roofline | 11.4 s | 11.7 s | to 15.8 s |
| 3 | bat strip, ring and pointer | the bats are above the horde | 12.6 s | 12.9 s | to 15.8 s |
| 3 | counter to `1500` | the swarm is fully out of the belfry | 13.4 s | 13.8 s | to 15.8 s |
| 4 | title, then code line, then logo, then the GitHub line | the horde walks on | 16.3 s, 16.9 s, 17.3 s, 17.7 s | | to the end |

One label at a time, each fading over 8 frames. A beat's overlay arrives at least 0.6 s
after its label. No beat opens at final density: at `t = 0` the still shows only the
street, the kicker and `8`.

## 3c. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| a mass is made of few bodies | a film strip of baked poses | one frame of the strip = one pose of the shamble; the pointer = this zombie's moment in the cycle | the pointer slides along the strip as the ringed zombie steps and wraps at the end; the ring keeps it tied to its zombie |
| the swarm is one wing-cycle | a second strip of six wingbeat poses | one frame = one pose of the flap | the pointer cycles faster than the zombie's, at the bat's flap rate |
| how many bodies are in view | a counter | the number of agents inside the camera's view (the same sphere test the renderer culls with) | counts up as the camera shows more of the street; a bat or zombie leaving the frame counts it down |

No term is both in a diagram and in its key: `bodies` is only on the counter, `strip` only
in the beat's label.

## 4. Script

`Voice: none`, because the beats are carried by the picture and the score and the text is
labels, not sentences. The cue map is section 3b plus this table, for score and picture.

| Cue | Source of truth |
| --- | --- |
| steps: one tick each 0.75 m a zombie covers | the bake (`Crowd::travelled`) |
| groans: when a zombie first passes the lurch speed | the bake |
| bat wing flutter: one soft pulse per flap per near bat | the bake and the flap rate |
| the belfry stinger | the first frame at which a bat is outside the belfry's door |
| beat starts: 5.0, 11.0, 16.0 s | one table of beat starts in code, read by picture and score |

## 5. Sources and claims

No claims about the world. The counts on screen are the scene's own: 8 hero zombies, 500
instanced ones, 1000 bats, and the body counter, which is computed per frame from the bake
and the camera. Nothing is labelled illustrative because nothing is a statistic.

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| zombies | 8 + 500 | the scene's data (`HEROES`, `HORDE`) | label, counter | exact |
| bats | 1000 | the scene's data (`BATS`) | label, counter | exact |
| bodies in frame | per frame | the bake and the camera, the renderer's sphere test | counter | exact |
| bodies | 4 | the scene's data (`VARIANTS`) | label | exact |
| baked poses | the strip's frame count | the bake in `build` | the strip | exact |

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | dusk: sky `#2b2640` fading to `#5b3d5a` at the horizon; ground `#34303a`; bone `#e9e4d4` = text and the counter; sick green `#8fa77a` = anything about a body (the zombies' skin, the strip, the zombie ring); amber `#e6a23c` = light only (the lamp, windows, the belfry's glow); bats `#17141f`. Amber is never used for text. |
| Type | Inter, as `crowd-demo`: 26 px kicker, 96 px beat label, 30 px for the code line, 64 px for the counter |
| CJK | n/a |
| Margins / safe area | 72 px |
| Caption box | n/a (no captions) |
| HUD / persistent frame | the kicker and the counter, the same place across beats (top left, then top right for the counter) |
| Motion grammar | one continuous camera path, eased; labels and overlays fade over 8 frames; no idle motion except the horde itself and the lamp's slight flicker |
| Motif | the strip: it is the proof in both beat 2 and beat 3 and returns, unlabelled, on the end card |
| Charts / maps | none |
| Text performance | labels, not sentences: at most five words, one at a time |
| Brand kit | the `raymotion-logo` prefab on the end card, the GitHub line, as `crowd-demo` |

**Density rule:** one idea per screen. The mass of bodies is the image, so the overlays are
small and wait their turn.

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | `procgen::Score`, generated into `c.assets`: a low drone in D minor under a slow pulse, thinning in beat 2, a held choir pad in beat 3, resolving to D major on the card |
| Ducking | none (no voice); SFX sit against the bed through the bake-driven gain |
| SFX | footsteps (one tick each 0.75 m); lurch groans (noise through vowel formants, pitched by body size); wing flutter (band-passed noise pulses at the flap rate); the belfry stinger (a reversed swell into a hit); an impact and bells on the title |
| Silence | a half-second hole just before the bats leave the belfry, so the stinger lands |
| Loudness target | about -16 LUFS integrated, true peak at or below -1 dBTP, measured on the encoded MP4 |

`ensureAssets` caches `score.wav`: delete `out/zombie-horde-assets/score.wav` after any
world or sound change, as with `crowd-demo`.

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| zombies instanced | 500 | the street area: about 0.2 per m² at the densest, so a 70 m by 40 m street | the scene's data |
| bats | 1000 | the belfry's door and the sky area over the street | the scene's data |
| body counter | per frame | the culling test the renderer uses | the bake |
| strip frames | 8 shown, more baked | the shamble cycle length and the pose count in the baked texture | the bake |
| runtime | 20 s | beat starts 0, 5, 11, 16 s; the sum of the beats | the beat table |

A change to the count of zombies recomputes the street's size, the counter's ceiling and
the label.

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | generated by `c.assets` | `out/zombie-horde-assets/score.wav` | synthesised at build; `list` works without it |
| Bodies and strips | generated in `build` from rig primitives and procedural meshes | in memory | none needed |
| Fonts | system and bundled, loaded in drawing callbacks | | the engine fallback |
| Logo | the `raymotion-logo` prefab | | none needed |

**Clean-machine test:** with `out/zombie-horde-assets` removed, `raymotion list` still
prints the film, and a render regenerates the score.

## 9. Cue map

| Frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 0 | the eight come on at low angle, counter `8` | the drone | the beat table |
| 78 | the crane starts; the counter climbs | the pulse begins | the beat table |
| 150 | the high shot; beat 2 | footsteps thicken | the beat table |
| 225 | ring and pointer on one zombie | a groan from it | the bake |
| 330 | beat 3 starts; the tilt | the silence, then the stinger as the first bat leaves | the bake |
| 390 | bat strip and pointer | the flutter bed | the bake |
| 480 | beat 4: the card | the chord resolves; impact and bells | the beat table |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 0 | 1 | only the street, the kicker and `8`; the eight on the road; nothing else |
| 45 | 1 | the heroes' faces and gait are readable; no foot slides |
| 90 | 1 | the crane shows the horde behind; the counter is mid-climb |
| 135 | 1→2 | mid-transition: nothing pops, the horde is in frame |
| 180 | 2 | the strip is in; the ring sits on its zombie, the pointer on a pose |
| 240 | 2 | the mass flows round the barricade; no body inside it |
| 300 | 2 | the ring and pointer still agree after the zombie's stride wrapped |
| 345 | 2→3 | the tilt passes the roofline; bats are leaving the belfry |
| 390 | 3 | the swarm is spread over the horde; the bat strip is readable; counter `1500` |
| 450 | 3 | the swarm banked round the tower |
| 495 | 4 | the card, the logo and the GitHub line, with the horde walking behind |
| 590 | 4 | the final hold |

Plus one still mid-way through each transition and one frame extracted from the encoded
MP4. Scene approval uses a scene-only clip or 2-3 stills, one scene at a time.

Thumbnail candidate: 240 (the mass flowing round the barricade under the ring), tested at
320x180.

### 10b. Scene review ledger

Kept in `docs/zombie-horde-qc.md`.

## 11. Acceptance checks

- [ ] Muted, the argument is legible: a big mass, a few ringed individuals, a strip.
- [ ] Every chapter shows its proof object at the stated frame.
- [ ] Every scene starts sparse and reveals one unit at a time with a hold.
- [ ] No body walks through the barricade, the lamp or another body; no foot visibly slides.
- [ ] The ring and pointer agree: the pointer's pose is the pose the ringed body shows.
- [ ] The bats stay in a swarm round the tower and do not pass through the roofline.
- [ ] The counter agrees with the culling test (a spot check at three frames).
- [ ] Frames are bit-identical with culling on and off, and instanced versus per-object
      for the same bodies at rest, as the test says.
- [ ] Sounds land on their events: steps with strides, the stinger as the first bat leaves.
- [ ] Executable newer than the last source edit; the final render rebuilt after approval.
- [ ] `list` works from another directory and with assets absent.
- [ ] Duration, frame count, loudness and true peak recorded.
- [ ] The profile is recorded in the QC: ms a frame and draw calls a pass, per beat, with
      individual objects and with instancing.
- [ ] Authoredness ≥ 22 and Craft ≥ 13; a thumbnail is chosen and readable at 320x180.

## 12. Risks / non-goals

- **Risks:**
  - The instanced bodies lose faces and outfit textures: only the eight heroes keep them.
    The brief puts the heroes in the first beat for that reason.
  - Texture size and bake time for four bodies by several poses.
  - A ring that follows an instanced body needs its transform from the bake, not the scene
    graph.
  - GPU time is visible only through a readback sync; the profile uses one.
- **Non-goals:**
  - No claim about real crowds or real bats.
  - No gore: the zombies are stylised, flat-coloured, with torn clothes.
  - No vertical cut.
- **Out of scope:** per-instance outfits, transparent instances, and dynamic lights moving
  with the bodies.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | owner | | pending |
| Script / cue map | owner | | pending |
| Stills | | | |
| Staged reveal / sync | | | |
| Draft | | | |
| Final QC | | | |

## 14. Revisions made during the build

1. Beat starts are 5, 11 and 16 s; the release of the bats (`BAT_GO`) is at 12.2 s, with the
   bats hidden in the belfry until 11.9 s.
2. The barricade is three cars, not a wall, so the horde parts round it; the windows of the
   buildings are instanced too (a one-frame baked quad, unlit amber).
3. The horde is not on screen at the start: the camera's far plane and the fog are driven
   by time, so the horde pops in unseen and the counter climbs honestly (it uses the
   renderer's own `visibleSphere`).
4. The strips are 2D puppets (`rig::draw`) and a hand-drawn bat schematic; the pointers
   follow `crowd::walkCycle` and `crowd::flapCycle`, the numbers the renderer plays.
5. The score is procedural, from the bake: pads, heartbeat, footfalls, groans, a flutter bed
   that follows the bats in frame, a riser and impact at `BAT_GO`, and the end card.
