# one-walk-any-body — authoring brief

Status: `draft` (agent-reviewed; awaiting the owner's approval).

| | |
| --- | --- |
| Composition name | `one-walk-any-body` (+ `one-walk-any-body-thumb`) |
| Story mode | `promo` |
| Owner / agent | pebaryan / Claude |
| Status | `draft` |
| Runtime target | 55.5 s (1665 frames) |
| Platform / aspect | 1920x1080, 30 fps; README / docs / social feed |
| Voice | none: the film autoplays muted in docs and feeds; the captions are the script, and the face chapter "speaks" with mouth shapes |
| Audience | people who make motion graphics in code and have used a character rig at most once; afterwards they can say "motion lives in the skeleton, the body is clothing, the face is an SVG, and props are parts the hands find" |
| Source material | the `rig::` module (`include/rmtn/rig.hpp`), `assets/characters/*.svg` |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | promo |
| Goal | make one idea desirable: a character whose body, material and face are data, on one unchanging motion |
| Proof object | the demo itself: a joint overlay that stays on the body while every part swaps; a softness dial; a mouth-shape strip; one character drawn twice; palm dots landing in target rings set by the furniture |
| Required evidence | claim ledger (every claim is a property of the code, shown happening) |
| Text | one headline per chapter, one caption line at ~2.5 words/s, a tagline and the API names on the end card |
| House grammar | raymotion brand: warm paper ground, ink type, teal accent, logo prefab on the end card |
| Packaging | thumbnail composition, SRT, end card with logo, API line and repo URL (CTA) |

## 1c. Reel / feed cut

`n/a`: 16:9 docs film. A vertical cut would keep chapters 1-2 only.

## 1. One sentence

> This is a film about **configurable characters** that makes the viewer **see that
> motion and body are separate** by **letting one walk cycle run, unbroken, while every
> part of the body is swapped out from under it**.

## 2. One-line argument

> The walk lives in the skeleton; heads, limbs, materials, faces, props and even
> 2D-or-3D are clothes it wears.

## 3. Chapters

| # | Frames (s) | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0-135 (0.0-4.5) | One walk | `ONE WALK` / "This walk has no body yet. Just joints and timing." | a bare skeleton (teal bones, joint dots) walks on a scrolling floor; a phase ring beside it ticks round once per stride | the skeleton + phase ring: motion with no body | soft pad; a tick on every footfall |
| 2 | 125-425 (4.2-14.2) | Any body | `ANY BODY` / "Put parts on it. The walk never changes." then "Swap any part, mid-stride." | parts pop onto the walking skeleton one slot at a time; then head, arms, legs, hat, feet swap; the joint overlay stays on the body; the phase ring never stutters | the joint overlay + a parts tray whose cards flip as each slot changes | a pluck per part (rising), footfall ticks continue |
| 3 | 415-670 (13.8-22.3) | Rigid or soft | `RIGID OR SOFT` / "Same elbow, three materials." | three figures enter one at a time, each waving the same wave: rigid segments, soft, rubber hose; a dial at top right (clear of the three heads) sweeps the middle figure's softness 0 to 1 and its arm melts from angular to hose | the softness dial driving the middle arm | riser under the dial sweep; a bell when it lands |
| 4 | 660-945 (22.0-31.5) | A face from vectors | `A FACE FROM VECTORS` / "The face is an SVG of named shapes." then "Hi! I'm Pip." | close on the head; a strip of the SVG's mouth shapes A-H; as Pip "says" the line the active chip lights; then happy, surprised, wink | the mouth-shape strip lighting in sync with the mouth | tiny blips on mouth changes; a bell on the wink |
| 5 | 935-1190 (31.2-39.7) | Flat or deep | `FLAT OR DEEP` / "One character, drawn flat or in depth." | split frame: the same Character as a 2D cut-out and as 3D meshes; one yaw dial turns both; the outfit swaps to stripes on both at once | one yaw dial driving two renderings; one outfit change landing on both | whoosh on the turn; pluck on the outfit swap |
| 6 | 1180-1540 (39.3-51.3) | Hands on things | `HANDS ON THINGS` / "The hands find the keys by themselves." then "Phones, placards, shields and boards are parts too." | an office corner built from the `interior3d` desk, workstation and chair: a typist types, two teal target rings at the keyboard with the teal palm dots inside them; a colleague with a phone enters (and leaves as the camera moves); one camera truck to a street where a placard, a riot shield, a skateboard (rolling in) and a scooter enter one at a time, each labelled with the part names it wears | target rings at the keyboard (from the furniture's geometry) holding the palm dots (from the pose); the part labels read from each figure's `Look` | soft key clicks from the typing clip's own keystroke table; a riser on the truck; a rising pluck per entrance |
| 7 | 1530-1665 (51.0-55.5) | End card | `ONE RIG. ANY BODY.` / `rig::Character · rig::Look · rig::parts` | the four bodies from chapter 2 stand in a row and cheer; title, API line, logo | the line-up: four bodies, one clip | impact, bell arpeggio, resolve |

### 3b. Reveal map

Text-only: the anchor is the caption phrase onset (captions run at ~2.5 words/s from the
scene's frame 8). Each visual leads its phrase by 0.27 s (8 frames).

| Scene | Unit | Anchor | Visual start (local s) | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | skeleton walking | first frame (mid-stride) | 0.00 | 0.00 | to end |
| 1 | headline `ONE WALK` | — | 0.27 | 0.60 | to end |
| 1 | phase ring | "timing" | 1.73 | 2.00 | to end |
| 2 | parts tray (empty slots) | "Put parts" | 0.00 | 0.30 | to end |
| 2 | body (torso, hips, legs, feet) | "parts" | 0.60 | 0.95 | 0.9 |
| 2 | arms + hands | — | 1.50 | 1.85 | 0.9 |
| 2 | head + face + hair | — | 2.40 | 2.75 | 1.2 |
| 2 | head swap (brick head) | "Swap any part" | 3.60 | 3.95 | 1.2 |
| 2 | arm swap (brick arms, elbows lock) | — | 4.80 | 5.15 | 1.2 |
| 2 | leg swap (robot legs) | — | 6.00 | 6.35 | 1.2 |
| 2 | hat on | — | 7.20 | 7.55 | 1.2 |
| 2 | feet swap (flippers) | — | 8.40 | 8.75 | to end |
| 3 | rigid figure + label | "Same elbow" | 0.30 | 0.60 | 0.9 |
| 3 | soft figure + label | — | 1.20 | 1.50 | 0.9 |
| 3 | hose figure + label | — | 2.10 | 2.40 | 0.9 |
| 3 | softness dial | "three materials" | 3.00 | 3.30 | sweep 3.6-7.0 |
| 4 | head close-up | first frame | 0.00 | 0.30 | to end |
| 4 | mouth-shape strip | "named shapes" | 0.90 | 1.20 | to end |
| 4 | speech (chips light) | "Hi! I'm Pip." | 3.20 | 3.47 | 1.9 |
| 4 | expressions: happy / surprised / wink | — | 5.60 / 6.80 / 8.00 | +0.2 | 1.2 each |
| 5 | 2D puppet + label | first frame | 0.00 | 0.30 | to end |
| 5 | 3D figure + label | — | 0.90 | 1.20 | to end |
| 5 | yaw dial; both turn | "drawn flat or in depth" | 2.00 | 2.27 | turn 2.3-5.0 |
| 5 | outfit swap on both | — | 5.80 | 6.15 | to end |
| 6 | office set + typist typing | first frame | 0.00 | 0.30 | to end of beat A |
| 6 | headline `HANDS ON THINGS` | — | 0.27 | 0.60 | to end |
| 6 | target rings + palm dots | "find the keys" | 1.00 | 1.30 | 3.6 |
| 6 | colleague with phone + label | — | 2.40 | 2.75 | 2.3 |
| 6 | camera truck to the street | "Phones, placards" | 5.00 | 6.20 | — |
| 6 | placard bearer + label | "placards" | 6.10 | 6.45 | 0.9 |
| 6 | riot officer + label | "shields" | 7.00 | 7.35 | 0.9 |
| 6 | skater rolls in + label | "boards" | 7.90 | 8.25 | 0.9 |
| 6 | scooter rider + label | "parts too" | 8.80 | 9.15 | to end |
| 7 | line-up | first frame | 0.00 | 0.30 | to end |
| 7 | title | — | 0.70 | 1.00 | to end |
| 7 | API line + logo | — | 1.60 | 1.90 | to end |

## 4. Script

Voice: none (see header). The captions are the complete script.

| # | Caption text | Words | Window (local s) |
| --- | --- | --- | --- |
| 1 | This walk has no body yet. Just joints and timing. | 10 | 0.27-3.85 |
| 2a | Put parts on it. The walk never changes. | 8 | 0.27-3.4 |
| 2b | Swap any part, mid-stride. | 4 | 3.6-9.35 |
| 3 | Same elbow, three materials. | 4 | 0.27-7.85 |
| 4a | The face is an SVG of named shapes. | 8 | 0.27-3.0 |
| 4b | Hi! I'm Pip. | 3 | 3.2-8.85 (spoken by the mouth 3.2-5.1) |
| 5 | One character, drawn flat or in depth. | 7 | 0.27-7.85 |
| 6a | The hands find the keys by themselves. | 7 | 0.27-4.6 |
| 6b | Phones, placards, shields and boards are parts too. | 8 | 5.4-11.35 |
| 7 | One rig. Any body. | 4 | title card |

Every window closes (with its 8-frame fade) before its scene's 10-frame crossfade begins, so
two captions never share the screen.

`Cue map`: see §9.

## 5. Sources and claims

Every claim is a property of the code, shown on screen as it happens; there are no
numbers about the world.

| Claim | Shown by | Code |
| --- | --- | --- |
| the walk does not change when parts change | the joint overlay and phase ring stay continuous across every swap | `Character::poseAt` depends on motion only (and part locks) |
| brick arms lock the elbow | the arm stops bending at the moment of the swap | `PartDef::locks` on `arm.block` |
| softness is a dial | the middle arm changes as the dial turns | `Character::softness`, `tubeCurve` |
| the face is an SVG of named shapes | the strip draws the actual `mouth-A..H` shapes from `assets/characters/face-pip.svg` | `svgFace`, `FaceTrack::say` |
| one character renders flat or in 3D | both halves read the same `Character` object | `rig::draw`, `rig::add` |
| the mouth shapes for "Hi! I'm Pip." | the letter-to-shape guess, labelled on screen as "letters to shapes (approx.)" | `rig::visemes` |
| the hands find the keys by themselves | the rings are drawn at the keyboard's position taken from the workstation prefab's geometry; the palm dots are projected from the solved pose; they coincide while the typist types | `clips::typing` (two-bone IK via `rig::hand`), `rig::palmAt` |
| props and rides are parts | each label is read from the figure's `Look` slots (`holdL`, `holdR`, `ride`) at render time, not typed | `rig::slots`, `Slot::Hold`, `Slot::Ride` |

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | paper `#f3efe6` ground; ink `#1f1d2b` text; teal `#139e98` = motion (skeleton, phase ring, yaw dial) and nothing else; coral `#e8553f` = the part that just changed; amber `#f0a830` = material (softness dial); dim `#8a8597` = labels |
| Type | UI font (bold) headline 64 px, kicker 22 px tracked 4; caption 36 px regular; labels 24-26 px; API line in the code font 30 px |
| CJK | n/a |
| Margins / safe area | 96 px sides, 72 px top; caption baseline at y 1010 |
| Caption box | none: one ink line centred at y 1010, max one line |
| HUD / persistent frame | kicker `CHARACTERS` top-left with a chapter count `01 / 07` and a 2 px progress line; the teal phase ring wherever the walk runs |
| Motion grammar | entrances 10-14 frames, spring or ease-out cubic; parts pop with a back-out over 0.35 s; fades between chapters (10 frames); no camera move except chapter 4's slow push-in and chapter 6's one truck from the office to the street; no idle motion except the characters' own clips |
| Motif | teal joint dots: the skeleton is visible whenever a claim depends on it |
| Charts / maps | dials only: a ring with a teal/amber arc and a value label |
| Text performance | one caption line at a time, ~2.5 words/s, no karaoke box or glow |
| Brand kit | `raymotion-logo` prefab on the end card, with the repo URL `github.com/pebaryan/raymotion` beside it as the call to action |

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| motion is separate from body | x-ray joint dots over the body | dot = joint position from the pose | parts change under the dots; the dots keep their path |
| the walk never changes | phase ring | one turn = one stride; tick = footfall | keeps turning at the same rate through every swap |
| rigid vs soft | dial | 0 = sharp elbow, 1 = rubber hose | the needle and the middle arm move together |
| words to mouth shapes | chip strip of mouth shapes | chip = one SVG shape, lit = in use | chips light in the order the mouth uses them |
| 2D and 3D are one character | one yaw dial between two halves | dial angle = both characters' facing | both halves turn with the needle |
| hands find their targets | teal target ring + teal palm dot | ring = a point taken from the furniture; dot = the palm from the pose | the dots bob with each keystroke and never leave the rings |
| props are parts | the chapter-2 card grammar, under each figure | label = the part names in its hold/ride slots; coral = just arrived | each label turns coral as its figure pops in, then ink |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | `procgen::Score` in `c.assets`: a light pad (C-G-Am-F) under chapters 1-2, footfall ticks on every step (from the same walk cadence the picture uses), a rising pluck per part in chapter 2, a riser under the dial sweep and a bell on its landing, short blips on mouth changes, a whoosh on the yaw turn, impact and bell arpeggio on the end card |
| Ducking | n/a (no voice) |
| SFX | from the cue map (§9), all synthesised |
| Silence | a beat of near-silence (pad only) at the start of chapter 4 so the mouth blips read |
| Loudness target | -16 LUFS integrated, true peak <= -1 dBTP |

## 7b. Scenario coherence

| Quantity | Value | Depends on | Source |
| --- | --- | --- | --- |
| walk cadence | 1 stride/s per foot (2 footfalls/s) | footfall ticks, phase ring, floor scroll speed | `clips::walk(1)`, `walkSpeed(build)` |
| floor scroll | `walkSpeed(build)` m/s | cadence and build | code |
| swap times | chapter 2 local 0.6 + 0.9/1.2 s steps | parts tray flips, plucks | one `SWAPS` table |
| softness sweep | 0 at local 3.6 s, 1 at 7.0 s | dial needle, middle arm, riser | one function |
| speech | "Hi! I'm Pip." over local 3.2-5.1 s | mouth chips, blips, caption | `FaceTrack::say` cues |
| keystrokes | 7 slots/s per hand, a slot struck when its seeded draw > 0.3 | key clicks, the hands' dips | `clips::typing` seeds 71/73, mirrored in `genScore` |
| entrances | chapter 6 local 2.4, 6.1, 7.0, 7.9, 8.8 s | pops, labels, plucks | one `ACTS` table |
| skater's roll | 0.9 m, easing out, from local 7.9 s | the board's position | one function `rollAt()` |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | generated in `c.assets` | `out/one-walk-assets/score.wav` | regenerated on render; the film builds without audio until assets run |
| SRT | generated in `c.assets` | `out/one-walk-any-body.srt` | from the caption table |
| Face SVG | committed | `assets/characters/face-pip.svg` | the film falls back to `simpleFace()` |
| Outfit SVG | committed | `assets/characters/outfit-sailor.svg` | falls back to `Outfit::basic` stripes-free colours |

**Clean-machine test:** with `assets/characters` renamed, `list` still works and the film
renders with the procedural face and plain outfit.

## 9. Cue map

| Frame (global) | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| every 15 from 0 to 425 | footfall | tick | `clips::walk(1)` cadence |
| 125 + 18 | body pops on | pluck C5 | `SWAPS[0]` |
| 125 + 45 | arms + hands | pluck D5 | `SWAPS[1]` |
| 125 + 72 | head, face, hair | pluck E5 | `SWAPS[2]` |
| 125 + 108 / 144 / 180 / 216 / 252 | head / arms / legs / hat / feet swap | plucks G5 A5 C6 D6 E6 | `SWAPS[3..7]` |
| 415 + 108 → 415 + 210 | softness dial sweep | riser | `softnessAt()` |
| 415 + 210 | dial lands at 1 | bell | `softnessAt()` |
| 660 + 96 → 660 + 153 | speech | blip per mouth change | `FaceTrack` cues |
| 660 + 168 / 204 / 240 | happy / surprised / wink | bells | `FACE_KEYS` |
| 935 + 69 → 935 + 150 | yaw turn | riser/whoosh | `yawAt()` |
| 935 + 174 | outfit swap on both | pluck | `OUTFIT_AT` |
| 1180 + 0 → 1180 + 150 | typing | soft clicks on struck slots | `clips::typing` keystroke table |
| 1180 + 150 → 1180 + 186 | camera truck | riser | `truckAt()` |
| 1180 + 72 / 183 / 210 / 237 / 264 | phone / placard / shield / skateboard / scooter enter | plucks G4 C5 E5 G5 C6 | `ACTS` |
| 1530 + 21 | title lands | impact + bells | end card |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 20 | 1 | skeleton mid-stride, headline in, no ring yet |
| 110 | 1 | phase ring present; caption fully readable |
| 200 | 2 | body + arms on; tray shows three filled slots; dots on joints |
| 300 | 2 | brick head + brick arms; elbows straightened at the lock angle |
| 400 | 2 | robot legs, hat, flippers; tray all coral-flipped once |
| 470 | 3 | rigid and soft figures, labels under each |
| 590 | 3 | dial mid-sweep; middle arm visibly rounder than left |
| 780 | 4 | head close; strip lit on a mouth shape that matches the face |
| 900 | 4 | wink |
| 1010 | 5 | both halves turned to the same angle |
| 1150 | 5 | stripes on both |
| 1230 | 6 | typist typing; both palm dots inside their rings; headline in |
| 1290 | 6 | colleague with phone in; label coral then ink |
| 1440 | 6 | street: placard and shield in; labels readable; no figure overlaps another |
| 1520 | 6 | all four street figures; skater on its board, mid-roll |
| 1640 | 7 | line-up, title, API line, logo |

Plus one still from the middle of every transition (frames 130, 420, 665, 940, 1185, 1535) and
encoded frames from the final MP4.

## 10b. Scene review ledger

Kept in `docs/one-walk-any-body-qc.md`.

## 11. Acceptance checks

- [ ] Muted, the separation of motion and body is legible from the images alone.
- [ ] The phase ring and joint dots are continuous across all chapter 2 swaps.
- [ ] Every scene starts sparse and reveals one unit at a time.
- [ ] No term repeats in a figure label and its caption.
- [ ] The score's ticks/plucks share the picture's tables.
- [ ] `list` works from another directory and with `assets/characters` absent.
- [ ] Duration, loudness and true peak recorded.
- [ ] Thumbnail readable at 320x180.

## 12. Risks / non-goals

- **Risks:** a 2D/3D mismatch in chapter 5 (the 2D puppet uses projected primitives, not
  the meshes); the letter-to-shape lip sync is approximate (labelled).
- **Risks (chapter 6):** furniture is real size, so the chapter's adults use adult limbs
  with a toon-sized head (0.37 m), to match the rest of the cast;
  the 3D furniture has no 2D drawing, so this chapter is 3D only.
- **Non-goals:** Lottie faces (phase two of the module), cloth simulation, full-body IK
  (two-bone limbs only).
- **Out of scope:** a voice; it would compete with the mouth-shape demonstration.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | owner | | pending |
| Script / cue map | owner | | pending |
| Stills | agent | 2026-10-30 | see QC |
| Staged reveal / sync | agent | 2026-10-30 | see QC |
| Draft | owner | | pending |
| Final QC | owner | | pending |
