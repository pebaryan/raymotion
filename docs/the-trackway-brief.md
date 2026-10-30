# the-trackway — authoring brief

Status: `approved` (owner: "go", text-only). Revised during the build: see §14.

| | |
| --- | --- |
| Composition name | `the-trackway` (+ `the-trackway-thumb`) |
| Story mode | `promo` (for the rig's animals and mounts; its few claims about real animals are sourced) |
| Owner / agent | pebaryan / Claude |
| Status | `approved`, in build |
| Runtime target | ~70 s (2100 frames; final length from the chapter table) |
| Platform / aspect | 1920x1080, 30 fps; README / docs / social feed |
| Voice | none by default: like `one-walk-any-body` it autoplays muted in docs and feeds, and the prints and footfall chart carry the argument. A voiced cut (Qwen clone, `tools/vo_batch.py`) is an option for the owner to choose; the script below is written to be speakable |
| Audience | people who make motion graphics in code; afterwards they can say "every animal's feet stay put where they land, each body plan has its own gait, and one character can carry another" |
| Source material | the `rig::` animals and mounts (`include/rmtn/rig.hpp`, `src/engine/rig_{creature,ape,bird,chain}.cpp`); the sources in §5 |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | promo |
| Goal | make one idea desirable: animals whose feet really meet the ground, on any body plan, and characters that ride each other |
| Proof object | the trackway itself: every print is placed where a foot of the solved pose touched down, and stays there; a footfall chart drawn from the same contacts; head traces; trails that end |
| Required evidence | claim ledger: the rig claims are properties of the code shown happening; the animal claims are sourced (§5) |
| Text | one headline per chapter, one caption line at ~2.5 words/s; the end card carries the tagline, the API names and the URL |
| House grammar | raymotion brand: warm paper ground and ink type, teal accent, logo prefab on the end card |
| Packaging | thumbnail composition, SRT, end card with logo, API line and repo URL (CTA) |

## 1c. Reel / feed cut

`n/a`: 16:9 docs film. A vertical cut would keep chapters 1, 2 and 6 (the horse's gap and the rider).

## 1. One sentence

> This is a film about **how bodies move** that makes the viewer **see a gait as a
> pattern the feet write on the ground** by **letting every animal cross one strip of sand
> and leave its own trail — until the trails stop, because a body took to the air, the
> water, the bars, or another body's back**.

## 2. One-line argument

> The ground remembers every step: each body plan signs it differently, and when one body
> carries another, two lives leave one trail.

## 3. Chapters

The whole film is one trackway: a strip of pale sand that runs left to right; the camera
trucks along it. The prints never move once pressed.

| # | Time (s) | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0:00–0:06 | The ground remembers | `THE TRACKWAY` / "The ground remembers every step." | empty sand; prints press in one at a time in a walking rhythm, by feet we cannot see yet; then the horse fades in over the prints, its hooves landing exactly in them | prints pressing in time, then the hooves in them | a soft thud per print; a low pad enters |
| 2 | 0:06–0:18 | Four feet, four beats | `FOUR FEET` / "Walk, trot, gallop: the same four feet, new orders." then "At the gallop, for a moment, nothing touches." | the horse walks, trots, gallops; under the strip a footfall chart (four lanes: LH, LF, RF, RH) draws a bar while each hoof is down, from the same contacts that press the prints; at the gallop the chart opens a gap where no lane has a bar; a small Muybridge card (1878) pins to the gap | the footfall chart and its gap | thuds follow the hooves: 4 even beats, 2 paired, then a roll of 4 and a silence in the gap |
| 3 | 0:18–0:27 | Two feet | `TWO FEET` / "A hen holds her head still, then catches up." then "A duck doesn't bob. It waddles." | a hen crosses: three-toed prints in one line; a teal dot trace of her head draws above her: a staircase (hold, thrust); a duck follows: webbed prints toed in, wider apart; its head trace is a smooth line while the body rolls | two head traces on one scale: steps vs a ramp | light ticks for the hen's steps, a click per thrust; a softer pair for the duck |
| 4 | 0:27–0:33 | Same print, any size | `ANY SIZE` / "The same three toes, nine times the length." (sizes illustrative) | a T. rex walks into frame and leaves a print; once its foot lifts, a hen's print (ink outline, true size) appears inside the middle toe; the camera pushes in on the pair, then cranes back to the whole animal standing | nested prints: the hen's print inside the T. rex's middle toe | one deep impact; sub drop; the roar is a filtered noise swell |
| 5 | 0:33–0:40 | No feet | `NO FEET` / "A snake leaves one groove. Every part of it follows." | a snake slides in; its trail is a single continuous S groove drawn from where its head has been; its whole body lies in the groove as it moves | the groove and the body lying in it | a dry hiss under the slide; no thuds |
| 6 | 0:40–0:52 | Where trails end | `WHERE TRAILS END` / "Some trails stop: in the air, in the water, on the bars." | three endings in one truck: the hen runs and flaps off — her prints end mid-stride and the camera stays on them as she flies out of frame; a turtle walks to the water's edge — its prints stop at the line and it paddles out past a fish; a chimp knuckle-walks (knuckle prints between footprints) to a row of bars, swings up and brachiates away — the trail ends under the first bar | the last print of each trail, each at its own edge | wingbeats; a splash and water bed; a bar creak per swing |
| 7 | 0:52–1:04 | Two bodies, one trail | `ONE TRAIL` / "Two bodies. One trail." then "A rider, a passenger, a pilot." | a person's footprints walk up to the horse and stop; the rider mounts; only hoof prints continue; a chimp's knuckle prints end at a walking person's heels, the chimp now on their back; a pilot's prints end at a titan's foot, the titan lifts the pilot on its palm and walks on, the giant prints continuing; a counter in the corner: `BODIES 2 · TRAILS 1` for each pairing | each trail that ends at another body, and the counter | a rising pluck per mount; the titan's footfalls are the biggest thuds in the film |
| 8 | 1:04–1:10 | End card | `EVERY BODY SIGNS THE GROUND.` / `rig::animal · clips::gait · Character::mount` | the camera cranes up until the whole trackway reads as a map of every trail; title, API line, logo, URL | the map of trails | impact; bell arpeggio; resolve |

### 3b. Reveal map

Text-only: the anchor is the caption phrase onset (captions run at ~2.5 words/s from each
scene's frame 8). Each visual leads its phrase by 0.27 s (8 frames).

| Scene | Unit | Anchor | Visual start (local s) | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | empty sand + headline | first frame | 0.00 | 0.40 | to end |
| 1 | prints pressing (unseen walker) | "remembers" | 0.80 | each footfall | to end |
| 1 | horse fades in over its prints | "every step" | 2.70 | 3.40 | to end |
| 2 | footfall chart frame (empty lanes) | "Walk" | 0.00 | 0.30 | to end |
| 2 | walk: bars + prints | "Walk" | 0.27 | footfalls | 2.5 |
| 2 | trot | "trot" | 2.90 | footfalls | 2.5 |
| 2 | gallop | "gallop" | 5.50 | footfalls | 2.0 |
| 2 | the gap highlighted | caption 2b onset (7.90) | 7.60 | 7.93 | to end |
| 2 | Muybridge 1878 card | after the gap | 8.60 | 9.00 | to end |
| 3 | hen + prints | first frame | 0.00 | 0.30 | to end |
| 3 | hen head trace (staircase) | "holds her head still" | 1.00 | 1.30 | to end |
| 3 | duck + prints | "A duck" | 4.70 | 5.00 | to end |
| 3 | duck head trace (smooth) | "doesn't bob" (~5.5) | 5.20 | draws for 4.2 s | to end |
| 4 | impact + T. rex print | "The same three toes" | 0.00 | 0.30 | 1.2 |
| 4 | hen print nested inside | "nine times" (~1.87) | 1.55 | 1.88 | 1.9 |
| 4 | crane up to the head | after the print | 3.00 | 5.50 | — |
| 5 | snake enters | first frame | 0.00 | 0.30 | to end |
| 5 | groove drawn from the head's path | "one groove" | 1.20 | follows the head | to end |
| 5 | body-in-groove highlight | "Every part of it follows" (~2.27) | 2.00 | 2.47 | to end |
| 6 | hen takes off; last prints | "up" | 1.20 | 2.20 | 1.5 |
| 6 | turtle at the edge; paddles | "down" | 3.80 | 5.00 | 2.0 |
| 6 | chimp to the bars; swings | "on the bars" | 7.00 | 8.50 | to end |
| 7 | rider's prints, mount | "Two bodies" | 0.27 | 2.40 | 1.5 |
| 7 | counter `BODIES 2 · TRAILS 1` | "One trail" | 1.50 | 1.80 | per pairing |
| 7 | chimp onto a back | "a passenger" | 5.00 | 6.20 | 1.5 |
| 7 | pilot onto a titan's palm; titan walks | "a pilot" | 7.80 | 9.50 | to end |
| 8 | crane up to the map | first frame | 0.00 | 3.00 | to end |
| 8 | title | — | 1.80 | 2.10 | to end |
| 8 | API line + logo + URL | — | 3.00 | 3.30 | to end |

## 4. Script

Voice: none by default (see header). The captions are the complete script; each line is
written to be spoken if the owner picks the voiced cut.

| # | Caption text | Words | Window (local s) |
| --- | --- | --- | --- |
| 1 | The ground remembers every step. | 5 | 0.27–5.6 |
| 2a | Walk, trot, gallop: the same four feet, new orders. | 9 | 0.27–7.6 |
| 2b | At the gallop, for a moment, nothing touches. | 8 | 7.9–11.6 |
| 3a | A hen holds her head still, then catches up. | 9 | 0.27–4.5 |
| 3b | A duck doesn't bob. It waddles. | 6 | 4.7–8.6 |
| 4 | The same three toes, nine times the length. *(sizes illustrative)* | 8 | 0.27–5.6 |
| 5 | A snake leaves one groove. Every part of it follows. | 10 | 0.27–6.6 |
| 6 | Some trails stop: in the air, in the water, on the bars. | 11 | 0.27–11.6 |
| 7a | Two bodies. One trail. | 4 | 0.27–4.8 |
| 7b | A rider, a passenger, a pilot. | 6 | 5.0–11.6 |
| 8 | Every body signs the ground. | 5 | title card |

`Cue map`: see §9. Every window closes (with its 8-frame fade) before its scene's
10-frame crossfade, so two captions never share the screen.

## 5. Sources and claims

| Claim | Value used | Source | On screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| a galloping horse has a moment with no hoof on the ground, legs gathered under the body | the gap in the chart; the card "Muybridge, 1878" | Eadweard Muybridge, *The Horse in Motion* (Sallie Gardner at a Gallop), Palo Alto, June 1878; prints at the Library of Congress (public domain) | card | exact (the fact); the gap's length in the rig is illustrative |
| walk and trot footfall orders (walk: four evenly spaced beats, lateral sequence; trot: diagonal pairs) | chart lanes | M. Hildebrand, "Symmetrical gaits of horses", *Science* 150(3697):701–708, 1965, doi:10.1126/science.150.3697.701 | chart | exact order; durations illustrative |
| a walking hen's head holds still in space, then thrusts forward (one bob per step) | the staircase head trace | R. Necker, "Head-bobbing of walking birds", *J Comp Physiol A* 193:1177–1183, 2007 | trace | exact (the pattern); amplitude illustrative |
| web-footed waterfowl do not head-bob | the duck's smooth trace | Necker 2007 (same review) | trace | exact (the pattern) |
| in lateral undulation every point of a snake follows nearly the same path | the body lying in its groove | J. Gray, "The mechanism of locomotion in snakes", *J Exp Biol* 23:101–120, 1946; restated by Jayne (1986) and later JEB papers | groove | exact (the pattern) |
| T. rex tracks are three-toed like a bird's; "nine times the length" | nested prints | illustrative scale: the rig's toe lengths (hen 0.06 m, T. rex 0.55 m, a 1:9 length ratio) — labelled **sizes illustrative** on screen | label | illustrative |
| rig claims (feet stay where they land; one trail for a mounted rider; grips) | prints, chart, counters | properties of the code: `clips::gait` plants feet while down; `Character::mount` moves the rider's root; prints are placed from the solved pose at touchdown | — | exact (code) |

Wording guards: say "for a moment, nothing touches", not a duration; give the T. rex ratio as a
length ratio of the rig's toes, labelled illustrative; the 1878 card says "Muybridge, 1878", not a day (sources differ:
15 or 19 June).

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | paper `#f3efe6` UI ground; sand `#e6dcc6` (lit through a `#d6d0c6` material so it sits below the paper) with prints in a darker sand `#b59d72` (pressed) — prints are the only marks on the sand; ink `#1f1d2b` text; teal `#139e98` = the trace of motion (head traces, the groove highlight, the footfall bars) and nothing else; coral `#e8553f` = where a trail ends (the last print of a trail, the gap in the chart); dim `#8a8597` = labels |
| Type | UI font (bold) headline 64 px, kicker 22 px tracked 4; caption 36 px regular; chart labels 22 px; end card API line in the code font 30 px |
| CJK | n/a |
| Margins / safe area | 96 px sides, 72 px top; caption baseline at y 1010 |
| Caption box | none: one ink line centred at y 1010, max one line |
| HUD / persistent frame | kicker `THE TRACKWAY` top-left with a chapter count `01 / 08` and a 2 px progress line; the sand strip itself, always in frame |
| Motion grammar | one continuous camera truck along the strip (left to right) for chapters 1–7, eased at chapter boundaries; one crane in chapter 4 and one in chapter 8; prints press in over 4 frames (sink + darken) and then never change; entrances 10–14 frames ease-out; fades only between chapters where the truck cannot carry it; no idle motion except the animals' own clips |
| Motif | the print: every chapter is told in prints, and the coral last print marks every ending |
| Charts / maps | one footfall chart (chapter 2): four lanes, time running right in step with the truck, bars in teal while a hoof is down, the gap column outlined in coral |
| Text performance | one caption line at a time, ~2.5 words/s, no karaoke box or glow |
| Brand kit | `raymotion-logo` prefab on the end card, URL `github.com/pebaryan/raymotion` beside it (CTA) |

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| feet stay where they land | prints | one print = one touchdown of one foot, at the foot's solved position | a print presses in at touchdown and never moves; the foot sits in it while down |
| a gait is an order of footfalls | footfall chart | lane = one foot; bar = that foot down; x = time | bars draw as the feet land; the pattern changes at each gait change |
| the gallop's airborne moment | gap in the chart | a column with no bar in any lane | the column outlines in coral; the card pins to it |
| head-bob vs no bob | head trace | dot = the head's position each frame, on the sand's scale | hen: a staircase (flat holds, steep thrusts); duck: a smooth ramp |
| same shape, any size | nested prints | the hen print inside the T. rex middle toe | the small print presses into the big one after it lands |
| lateral undulation | groove | groove = the path of the head; the body drawn over it | the groove extends from the head; the body never leaves it |
| leaving the ground | the last print | coral = the final print of a trail | the trail stops; the body continues off the sand (air, water, bars) |
| one body carries another | trails that merge | two trails in, one trail out; counter `BODIES 2 · TRAILS 1` | the rider's/passenger's/pilot's prints end; the carrier's continue |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none (the voiced cut is an owner option) |
| Score | `procgen::Score` in `c.assets`: a sparse pad under the open sand; footfall thuds generated from the same contact list that presses the prints (pitch and weight by animal: hooves wood-like, hen ticks, the T. rex a sub impact); a silence in the gallop's gap; a hiss under the snake; wingbeats, a splash and a water bed, bar creaks; a rising pluck per mount; impact and bell arpeggio on the end card |
| Ducking | n/a (no voice) |
| SFX | from the contact list and the cue map (§9), all synthesised |
| Silence | the gallop's airborne gap is silent by construction (no contact, no thud): the sound proves the claim |
| Loudness target | -16 LUFS integrated, true peak <= -1 dBTP |

## 7b. Scenario coherence

| Quantity | Value | Depends on | Source |
| --- | --- | --- | --- |
| contacts (prints, chart bars, thuds) | one list per animal, from sampling the solved pose: a contact starts when a foot is within a few millimetres (scaled per animal) of its planted height (the 15th percentile of its height: a gait blend can dip lower) and nearly still | prints, chart, score | code (one function) |
| each animal's ground speed | `gaitSpeed(build, gait)` | node position, truck speed | code |
| gait changes (horse) | walk → trot at local 2.9 s, trot → gallop at 5.5 s, blended over 0.4 s | chart, prints, thuds | one `HORSE_GAITS` table |
| T. rex vs hen toe | 0.55 m vs 0.06 m | nested print label | `BipedSpec` values |
| endings | hen takeoff 1.2 s, turtle at the edge 3.8 s, chimp at the bars 7.0 s (chapter 6 local) | last prints, SFX | one `ENDINGS` table |
| mounts | rider 2.0 s, chimp 5.8 s, pilot 8.6 s (chapter 7 local) | trails ending, counters, plucks | one `MOUNTS` table, used by `Character::mount` |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | generated in `c.assets` | `out/the-trackway-assets/score.wav` | regenerated on render; the film builds silent until assets run |
| SRT | generated in `c.assets` | `out/the-trackway.srt` | from the caption table |
| Muybridge card | drawn (a line-drawn horse silhouette from the rig at the gap frame, captioned "Muybridge, 1878"), not the photograph | code | n/a |
| Characters | procedural (`rig::animal`, `Look::toon`, `Look::robot`) | code | n/a |

**Clean-machine test:** no committed assets are needed; `list` works from any directory.

## 9. Cue map

Filled when the chapter lengths are locked (frames = chapter start from `node->start` +
the local times in §3b and §7b). Every thud reads the contact list; every caption reads
the script table; every ending and mount reads its table.

## 10. Still frames to review

| Frame (approx.) | Chapter | What must be true |
| --- | --- | --- |
| 40 | 1 | prints pressing on empty sand, no animal yet; headline in |
| 150 | 1 | horse in, every hoof that is down sitting in a print |
| 280 | 2 | walk bars in the chart, four lanes readable |
| 470 | 2 | the gap column outlined; the card pinned; no hoof touches the sand |
| 620 | 3 | the hen's staircase trace above her prints |
| 760 | 3 | the duck's smooth trace beside the hen's, on one scale |
| 890 | 4 | the hen print inside the T. rex middle toe; "illustrative" label legible |
| 1100 | 5 | the snake's body inside its groove |
| 1280 | 6 | the hen's last prints coral; she is airborne |
| 1430 | 6 | the turtle's last print at the water's edge; it paddles |
| 1640 | 7 | the rider mounted; person prints end coral at the horse; counter in |
| 1880 | 7 | the pilot on the titan's palm; titan prints continue |
| 2060 | 8 | the map of trails, title, API line, logo |

Plus one still from the middle of every transition and encoded frames from the final MP4.

## 10b. Scene review ledger

Kept in `docs/the-trackway-qc.md`.

## 11. Acceptance checks

- [ ] Muted, a viewer can say what each trail shows without reading the captions.
- [ ] Every print is under the foot that made it at touchdown, and no print moves.
- [ ] The chart's gap and the silence fall on the same frames as the airborne horse.
- [ ] Every scene starts sparse and reveals one unit at a time.
- [ ] The thuds, chart bars and prints come from one contact list.
- [ ] Sources in §5 appear as cards or labels where the claim is made; the T. rex scale is labelled illustrative.
- [ ] `list` works from another directory.
- [ ] Duration, loudness and true peak recorded; thumbnail readable at 320x180.
- [ ] Authoredness ≥ 30 and Craft ≥ 16 (showcase).

## 12. Risks / non-goals

- **Risks:** many characters in one scene (render time; draw only the active chapter's
  cast); prints as many small meshes (batch per animal); the T. rex and titan scale against
  a hen-sized camera (chapter 4's crane and chapter 7's framing must be designed, not
  zoomed); the mount hop is a blend, not a jump arc (keep it brief and partly hidden by
  the carrier).
- **Non-goals:** biomechanical accuracy beyond the sourced patterns; real trackway
  photographs; a flying chapter of its own (flight appears as a trail ending).
- **Out of scope:** 2D puppets (this film is 3D; the 2D renderer is shown in the
  previous promo).

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | owner | | pending |
| Script / cue map | owner | | pending |
| Stills | agent | | |
| Staged reveal / sync | agent | | |
| Draft | owner | | |
| Final QC | owner | | |

## 14. Revisions during the build

- Chapter 4's ratio is stated as length ("nine times the length"), the rig's 0.06 m and 0.55 m
  toes, and no longer as area; the "sixty times larger" line is gone.
- Chapter 6's caption names the three media ("in the air, in the water, on the bars") instead
  of "up, down, into the trees": the chimp swings along bars, not trees.
- The hen of chapter 6 runs (the trot/run gait) before take-off; the camera holds on her last
  prints instead of following her up.
- The duck walks half a metre behind the hen so both stay readable; chapter 3's head chart
  plots each bird's head position over its own first 4.2 s on one scale.
- The titan is 2.6x life size so its lowered palm is within the pilot's reach; its prints are
  block-shaped, like its feet.
- Text-only, as proposed: the owner said "go" with no voice requested.
- Cue-sync pass after the first full render: the gap outline, duck trace, nested hen print,
  groove highlight and coral last prints were moved to lead their phrase (or their label) by
  0.25-0.33 s; the reveal map above has the new times.
