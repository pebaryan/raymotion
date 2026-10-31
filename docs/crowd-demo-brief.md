# crowd-demo — authoring brief

Status: `internal demo`. The owner asked for "crowd behavior (boids, flocks) for humanoids
and animals", with "module + test demo" as the deliverable. The authoring standard allows a
short brief for a quick internal demo.

| | |
| --- | --- |
| Composition name | `crowd-demo` |
| Story mode | `utility`: a capability demo for `crowd::`, with no claims about the world |
| Owner / agent | pebaryan / Claude |
| Runtime | 23.2 s (696 frames), 1920x1080, 30 fps |
| Voice / audio | no voice; a procedural score with sound effects taken from the bake (section 7) |
| Audience | the owner and people evaluating the engine. Afterwards they can say: "one bake steers a crowd, a herd and a flock with the same rules, and the feet keep up". |
| Source material | none; the scene is staged in code (`examples/showcase/crowd_demo.cpp`) |

## 1. One sentence

> Four bodies, one set of rules: the same steering moves a plaza crowd round a fountain, a
> herd away from a dog, a flock of ducks through a banked turn and a school of fish round a
> pike, and one overlay per beat shows the rule as it acts.

## 3. Chapters (one world, one travelling camera)

| # | Time (s) | Beat | On-screen text | Proof object |
| --- | --- | --- | --- | --- |
| 1 | 0.0–5.6 | the plaza, seen from high three-quarters: 40 people in two counter-streams cross a paper plaza and pass round a fountain | `crowd` + `separation · passing · goals` | one walker's rings, drawn on the ground: its personal space (solid teal, 0.6 m) and its sight (dashed, 3 m), plus a velocity arrow one second long. People part round the fountain and round each other, with nobody sliding sideways. |
| 2 | 5.6–10.6 | the camera travels east to a field: 12 horses stand at pasture; a dog waits, then runs in from the left; the herd bolts together, gallops off and strings out | `herd` + `cohesion · flee` | an amber ring for the dog's reach (the herd's panic radius, 9 m) travels with it; the horses it reaches break into a gallop, and the rest bolt with them |
| 3 | 10.6–14.8 | the camera tilts up from the field and follows 20 ducks circling overhead | `flock` + `alignment · banking into turns` | one duck's bank gauge: a dashed level line, a solid line along its wings, `bank N°`, and a velocity arrow; the whole flock rolls into the turn together |
| 4 | 14.8–19.8 | the camera comes down from the sky over a pond beside the field: 30 fish circle it in a school; a pike lying at the edge darts across, and the school opens round it and closes behind | `school` + `3D steering · split · rejoin` | an amber ring for the pike's reach (the school's panic radius, 1.6 m) at the fishes' depth; the school parts where the ring passes and re-forms. The pike aims at where the school will be: its line comes from a first bake. |
| 5 | 19.8–23.2 | end card over the pond | `ONE BAKE, ANY FRAME.` / `crowd::bake(world)  ·  crowd::cast  ·  crowd::addAll` / the raymotion logo + `github.com/pebaryan/raymotion` | — |

### Reveal map (text-only cue map)

| Unit | Anchor | Starts | Hold |
| --- | --- | --- | --- |
| kicker `CROWDS` + `boids, baked once, sampled per frame` | first frame | 0.3 s | to 14.6 s |
| label `crowd` | the streams are moving | 0.6 s | to 5.4 s |
| walker rings + arrow | the highlighted walker nears the fountain | 1.2 s | to 5.4 s |
| label `herd` | the field is coming into frame | 6.0 s | to 10.4 s |
| dog ring | the dog is running, before the herd bolts | 6.6 s | to 9.8 s |
| label `flock` | the tilt passes the horizon | 11.2 s | to 14.6 s |
| duck bank gauge + arrow | the flock turns away from the camera (from below, a roll can't be seen) | 13.0 s | to 14.6 s |
| label `school` | the pond is in view (the camera's aim leads its move down) | 15.8 s | to 19.4 s |
| pike ring | just before the pike strikes (16.5 s) | 16.4 s | to 18.8 s |
| end card | the school has closed again | 19.8 s | to the end |
| logo, then the GitHub line | the card's code line is in | 20.7 s, 21.0 s | to the end |

One label at a time, each fading over 8 frames. A beat's overlay arrives at least 0.6 s
after its label.

## 5. Sources and claims

No claims about the world. The behaviour shown is a property of the code: `crowd::bake`, the
steering weights in `crowd::Steer`, and gait matching in `crowd::locomote`. The radii on
screen are the values the scene uses.

## 6. Visual system

- Ground: warm paper `#f3efe6`, with the plaza a shade darker and a pale line grid every
  2 m, so a sliding foot would show against it.
- Type: ink `#1f1d2b`. Labels use Inter, as in skin-demo: the 26 px kicker, a 96 px beat
  name, the 30 px code font for the rule line.
- Teal `#16a39a` means "an agent's own rules": its rings and its arrow.
- Amber `#d98b2b` means "a threat", and appears only as the dog's ring and the pike's ring.
- The pond: a stone torus for a rim, a blue-grey bed, a faint translucent surface; orange fish
  read against the bed, the pike is dark olive and three times their length.
- Motion grammar: one continuous camera move, eased. Labels and overlays fade over
  8 frames. No other idle motion.

## 7. Audio plan

All procedural (`procgen::Score`), generated by `c.assets` into
`out/crowd-demo-assets/score.wav` from the same bake the picture samples. Every effect is
placed from an agent's state, so what you hear is what the beat shows:

| Beat | Music | Effects (from the bake) |
| --- | --- | --- |
| crowd | a light pad, D major, and bass | a footstep tick each 0.75 m any person walks, louder and panned by where the camera sees them |
| herd | the pad darkens (B minor); a riser into the bolt | the dog's patter; a low hoof hit each 1.6 m a horse covers, so the gallop is a rumble that starts at the bolt; an impact at the bolt (the first moment the herd's mean speed passes 2.5 m/s) |
| flock | the pad opens (G major) | a soft wingbeat per flap at the ducks' `rig::flapRate`, near ducks louder |
| school | a filtered water bed under an Em pad | a swoosh for the pike's dart; high plucks as the school splits |
| end card | the D chord resolves | an impact and four bells on the title |

Mix: `tanh` soft-clip, then a gain to land near -16 LUFS integrated with true peak at or
below -1 dBTP, measured on the encoded MP4.

## 8. Asset plan

Nothing external. The characters are the rig's primitives, the bake runs in `build`, and
the score is generated by `c.assets` into `out/crowd-demo-assets/`. `list` works anywhere.
The logo is the `raymotion-logo` prefab.

## 10. Still frames to review

| Frame | Beat | What must be true |
| --- | --- | --- |
| 30 | 1 | the streams are moving; the label is in; no rings yet |
| 90 | 1 | the rings sit under the highlighted walker; people part round the fountain |
| 150 | 1 | the streams pass each other without overlap |
| 185 | 1→2 | mid-transition: nothing pops |
| 215 | 2 | the herd stands; the dog's ring reaches it |
| 255 | 2 | the herd is fleeing, bunched and galloping |
| 300 | 2 | the herd strung out on the far side, the dog peeling off |
| 335 | 2→3 | the tilt passes the horizon with the ducks in frame |
| 380 | 3 | the flock overhead, filling the frame |
| 420 | 3 | the flock is banked into its turn; the gauge is readable |
| 465 | 3→4 | the camera comes down on the pond; nothing pops |
| 495 | 4 | the school circling, label in, no ring yet |
| 520 | 4 | the pike in its ring, about to reach the school |
| 535 | 4 | the school split round the pike |
| 575 | 4 | the school closed again behind it |
| 690 | 5 | the end card with the logo settled and the GitHub line |

Thumbnail candidate: 255 (the herd galloping inside the amber ring).

## 11. Acceptance checks

- Nobody walks through the fountain or through another person.
- Feet do not visibly slide on the grid, including at gait changes.
- The herd's flight reads without the label: away from the dog, then a settle.
- The ducks bank into the turn (inner wing down), all together.
- The school opens round the pike and closes again; no fish leaves the pond.
- Each sound lands on its event: footsteps under walking people, the rumble from the bolt,
  wingbeats with the flaps, the swoosh with the dart.
- The render time per frame with about 73 characters is recorded in the QC.

## 12. Risks / non-goals

- **Risks:**
  - The render cost of 73 characters.
  - Faces only on the first 12 agents of each group; the far ones read as faceless at
    this distance.
- **Non-goals:**
  - No claim about real crowd behaviour. The weights are tuned to read well, not
    calibrated to measurements.

## Revisions after the owner's first watch

- **"No SFX/BGM or logo."** A score and effects were added (section 7), and the logo and
  GitHub line on the end card.
- **"No school of fishes?"** The module's Water medium was untested on screen. A fourth beat
  was added: a school in a pond, split by a pike. The runtime grows from 16.5 s to 23.2 s.

### Beat 4 and sound, after the still review

- **Fish left the pond.** The pike struck straight out from the middle, and the school fled
  through the rim to 4.8 m from the middle of a 4 m pond. The world's bounds are one box
  for every group, so the engine gained a per-group pen (`Group::penAt`, `penRadius`). A
  soft wall turns a group back even mid-flight, and the edge is hard. A test was added
  (`pen()` in `tests/crowd_tests.cpp`): the school's farthest point is 3.77 m with the pen
  and 10.9 m without it.
- **The pike was slow.** It reached 1.5 m/s and arrived 1.2 s after the school had passed,
  because arrival braking slows any agent near its goal and its goal weight was 1. It now
  charges a goal 12 m out for 1 s with goal weight 4, turns at 360°/s, and reaches 2.7 m/s.
- **The pike's aim.** At first it waited at the rim, where the school might pass within its
  reach on an earlier lap; that would make the two bakes differ. It now waits in the middle,
  with the school circling just beyond its reach, so the first bake's school path is the
  second's up to the strike. Aimed at the school's place at 17.45 s, when the pike actually
  arrives, it passes about 0.4 m from the school's centre.
- **Fish drawn on the rim.** From 56° the far rim's inner face showed, and fish against it
  looked as if they lay on the stone. Now the rim is higher (top 0.4 m), the fish swim
  lower (0.12–0.28 m), the pen sits a body length in, and the camera looks down at about
  66°.
- **Blocky fish shadows.** The shadow map still covered the field's 18 m. At the pond it
  covers 7 m.
- **Timing.**
  - At 15.4 s the pond was not yet in view, so the `school` label moved to 15.8 s and the
    camera's aim now leads its move down.
  - The logo was still animating on the last frames, so the end card grew to 3.4 s.
- **The bolt's hit.** At first it was cued on the herd's mean speed, which fired at 8.7 s,
  0.7 s after the first horse breaks. It is now cued on the first horse to pass 2.5 m/s
  (8.0 s).

## Revisions after the still review

- **Beat 1, timing.** The streams met the fountain at 4 s and were still mid-pass when the
  beat ended. They now start 1.5 m closer, with a 2.5 s pre-roll, so they pass and clear
  inside the beat. A counterflow test was added
  (`tests/crowd_tests.cpp`: 40 of 40 past, mean 1.43 m/s).
- **Beat 2, the herd didn't flee.** Four engine faults stood in the way:
  - The dog overshot its waiting spot and turned round to face away. A walker just past
    its goal now stops instead of turning back.
  - A horse's goal held it at its spot against the flee. A panicking agent now lets go of
    its goal.
  - Alarm rose with distance, so most of the herd trotted. Alarm is now full at half the
    panic radius, and it spreads to flockmates.
  - Standing agents turned slowly. Startled agents now wheel round at full speed.

  The panic radius is now 9 m.
- **Beat 3.**
  - The flock was 12–14 m away and too small to show a bank. The camera now stands under
    the circle and follows the flock's centre (computed from the bake).
  - The 8 m sight ring read as noise. It is replaced by a bank gauge, which shows the
    beat's actual claim.
  - Seen from below, a roll is invisible, so the gauge waits until the flock turns away.
- **Render bug.** The camera hook assigned a whole `g3d::Camera`, which erased its own `up`
  hook after the first frame. Stills were right; the rendered MP4 kept the plaza camera.
  It now assigns fields only. Caught by checking frames extracted from the encoded MP4.
