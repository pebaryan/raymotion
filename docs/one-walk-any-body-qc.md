# one-walk-any-body — QC review

| | |
| --- | --- |
| Composition | `one-walk-any-body` (+ `one-walk-any-body-thumb`) |
| Commit / build | uncommitted working tree on `52a4d20`; `build/raymotion.exe` 12:55:27 (2026-10-30) |
| Reviewer | Claude (author self-review); owner review pending |
| Date | 2026-10-30 |
| Review stage | final (author); owner draft viewing outstanding |
| Story mode | `promo` |
| Artifacts | `docs/one-walk-any-body-brief.md`, `examples/showcase/one_walk.cpp`, `out/one-walk-any-body.mp4`, `out/one-walk-any-body.srt`, `out/one-walk-any-body-thumb.png`, `out/one-walk-assets/score.wav`, stills in `tmp/ow/` |

## 1. One-sentence conceit

> This is a film about **configurable characters** that makes the viewer **see that
> motion and body are separate** by **letting one walk cycle run, unbroken, while every
> part of the body is swapped out from under it**.

Reviewer's restatement after viewing the stills and encoded frames:

> A bare skeleton walks; parts pop onto it and swap mid-stride without the walk ever
> breaking. Then the same rig shows rigid vs soft limbs, an SVG face that talks, and the
> same character drawn flat and in depth.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief written before `one_walk.cpp`; conceit, proof object per chapter, cue map, claim ledger, visual system, audio plan, stills list, thumbnail |
| 1 Script and cue map | pass | text-only, declared in the brief header; caption table §4 and reveal map; captions live in one `CAPTIONS` table that drives screen and SRT |
| 2 Stills | pass | every chapter at start/middle/end plus every transition (`tmp/ow/s*.png`, `tmp/ow/f*.png`); the issues found are listed in §10 and fixed |
| 2b Staged reveal & narration sync | pass | ledger §5b; each scene starts sparse (skeleton only; empty tray; figures enter one at a time; strip after 0.9 s; 2D before 3D; line-up before title); exe newer than every source |
| 3 Draft (muted + sound) | **conditional** | the author can't watch in real time: muted review was done from encoded frames and the sound from the cue code and loudness. **The owner must watch once muted and once with sound** |
| 4 Final technical QA | pass | §7: ffprobe, full decode, -16.2 LUFS / -1.3 dBTP, frames from the MP4, deterministic stills and score |
| 5 Authored test | pass | §5 |
| 6 Craft & coherence | pass | craft 15/18 (§3) |
| 7 Release packaging | pass | logo prefab, repo URL as CTA, API line, SRT, thumbnail comp |

## 3. Scorecard

| # | Criterion | Score (0–3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | 3 | opens on a bodiless walk (first frame); the order adds body, then material, face, renderer; ends on four bodies doing one clip | |
| 2 | Script / voice | 2 | deliberately text-only, one idea per line, cue-mapped; plain declarative lines with little voice | a sharper line for 2b/5 ("Swap any part, mid-stride." is functional, not memorable) |
| 3 | Evidence | 3 | proof objects: joint overlay + parts tray; softness dial; SVG mouth-shape strip; yaw dial across 2D/3D; line-up. Muted viewer can follow; the lip sync is labelled "approx." | |
| 4 | Timing | 2 | `DUR`/`XF` give scene starts, checked against `node->start`; `SWAPS`, `FACE_KEYS`, `SAY_*`, `yawAt`, `softnessAt` drive picture, tray and score from one table. No narration to derive from | n/a without voice; acceptable for a text-only promo |
| 5 | Sound | 2 | procedural score: pad, footfall ticks at the walk cadence, a rising pluck per swap, riser + bell on the softness sweep, blips taken from the actual mouth-shape changes, bells per expression, end impact. Measured loudness. No VO, so no ducking | a motif that recurs per chapter instead of a pad |
| 6 | Visual system | 3 | documented colour meaning (teal = motion only; coral = part that just changed; amber = material), type scale, margins, chrome; the stills hold to it | |
| 7 | Chapter differentiation | 3 | walker + tray; three-figure line; head close-up + strip; split 2D/3D; end line-up; persistent chrome | |
| 8 | Data & provenance | 3 | promo claims are properties of the code, each shown happening (brief claim ledger → API); no numbers; approximation labelled | |
| 9 | Production pipeline | 3 | `c.assets` writes score and SRT; deleting `out/one-walk-assets` regenerates a byte-identical `score.wav`; SVGs committed with documented fallbacks (`simpleFace`, basic outfit); `list` works from `tmp/` | |
| 10 | Verification | 2 | stills per scene, encoded-frame check, ffprobe, decode, loudness/peak, still determinism (md5), still vs encoded PSNR ≈ 40 dB | owner real-time viewing (Gate 3) |
| 11 | Memorability / thumbnail | 3 | thumbnail comp checked at 320×180 (title + four bodies readable); iconic frames: the walking skeleton, the mouth-shape strip, the line-up | |
| 12 | Specificity (reskin) | 3 | the subject is the engine drawing itself; swapping topics means rebuilding every proof | |
| | **Authoredness total** | **32/36** | | |

### Craft & Coherence (second axis)

| # | Criterion | Score (0-3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | 3 | the tray reads its names from `rig::slots` of the active look; the dial values are the same functions that drive softness and yaw; the mouth chips light from `FaceTrack::at` | |
| 14 | Information design | 2 | proof hierarchy clear; dials legible; in the mouth strip the "F" label touches its large shape | shrink the shape fit or move the letter below the card |
| 15 | Text performance & pacing | 2 | one line at a time, no box or glow, ~2.5 words/s; "Hi! I'm Pip." holds 5.6 s past the speech (brief-specified) | end 4b at ~5.6 s and let the expression label carry the rest |
| 16 | House style & mode grammar | 3 | raymotion brand: paper/ink/teal, kicker + progress chrome, logo prefab, repo URL CTA | |
| 17 | Spec-to-screen fidelity | 3 | the brief was updated wherever review changed the film (caption windows, dial position, CTA); stills match the reveal map | |
| 18 | Revision quality | 2 | every self-review issue landed and was re-checked on stills (§10); no owner feedback round yet | |
| | **Craft total** | **15/18** | | |

## 4. Critical fails

- [ ] Conceit = 0
- [ ] Script and text-only cue map = 0
- [ ] Muted test fails (Evidence ≤ 1)
- [ ] Same template every chapter (Differentiation = 0)
- [ ] Central claims unsourced (Provenance = 0)
- [ ] Assets fail on a clean machine (Pipeline = 0)
- [ ] No stills/draft/final verification (Verification = 0)
- [ ] A key scene starts at final density, arrives after its anchor, wrong order, duplicate labels, or stale binary
- [ ] A claim contradicts its source, or an illustrative value is presented as exact
- [ ] Other: none

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test** | pass | the film is designed muted; the tray, dials, chip strip and split screen carry every claim |
| **Thumbnail test** | pass | `out/one-walk-any-body-thumb.png` at 320×180: "ONE RIG. / ANY BODY." and four bodies readable |
| **Reskin test** | pass | nothing survives a topic swap except the chrome |
| **Retiming test** | pass | changing a `SWAPS[i].at` moves the wear() time, the tray flip and the pluck together; `DUR` changes move scene starts, captions (scene-local) and the score |
| **Clean-machine test** | pass | `../build/raymotion.exe list` from `tmp/` lists both comps; deleted assets regenerate identically |
| **One idea per screen** | pass | worst frame f300: walker, tray and one caption |
| **Staging test** | pass | §5b |
| **Cue-sync test** | pass | §5c (text anchors) |
| **No-duplicate-label test** | pass | the tray rows say HEAD/ARMS…, the cards say the part keys; dials have value + one name; no term repeats in a figure and its caption |
| **Scene-review test** | pass | §5b, one strip of stills per scene |
| **Fresh-build test** | pass | exe 10:20:48 > newest source 10:20:44 (`one_walk.cpp`); MP4 10:21:14 |
| **Distinct chapter test** | pass | none |

## 5b. Scene review ledger

| Scene | Review asset (clip / stills) | Reveal notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 01 One walk | `tmp/ow/sc1.png` (f10, f100, f143) | skeleton walks from frame 0; kicker 0.27 s; phase ring 1.73 s; caption alone | yes | — |
| 02 Any body | `tmp/ow/sc2.png` (f160, f205, f232), `f300.png` | tray first (empty), body pops at 0.6 s with coral cards; arms 1.5; head+hair 2.4; swaps continue mid-stride; x-ray dims to 50 % once a body exists | revise → yes | brick head now wears `hair.block` (the round hair clipped it); floor fogs into the paper |
| 03 Rigid or soft | `tmp/ow/sc3.png` (f420, f470, f574), `f640.png` | figures enter 0.3/1.2/2.1 s; labels follow; dial at 3.0 s; sweep 3.6–7.0 | revise → yes | dial moved off the middle head to top right |
| 04 A face from vectors | `tmp/ow/sc4.png` (f700, f780, f906) | close-up; strip at 0.9 s; chip lights with the mouth during 3.2–5.1; expression label at 5.6 s | revise → yes | Pip face box lowered so the brows clear the fringe |
| 05 Flat or deep | `tmp/ow/sc5.png` (f945, f1115, f1150), `fix.png` | 2D first, 3D at 0.9 s; dial at 2.0 s; turn 2.3–5.0; stripes land on both at 5.8 | revise → yes | engine fix (2D fills lost when a 3D scene renders after them); 2D outfit bands; floor removed so both halves share the paper |
| 06 Hands on things | `tmp/ow/sc6-acting.png` (f1230, f1290, f1440, f1520), `tmp/ch6/f1365.png` | office set and typist from the first frame; headline 0.27 s; rings + palm dots 1.0 s, the dots sit in the rings and bob per keystroke; colleague with phone 2.4 s, label coral then ink; truck 5.0–6.2 s; placard 6.1, shield 7.0, skater 7.9 (rolls in), scooter 8.8, one at a time, each label coral on arrival | revise ×6 → yes | the PC tower hid the keyboard and the typist was in profile: the office now turns 210° so the tower is on the far side and the typist faces the camera three-quarters; the camera came down to 1.95 m; the keyboard label moved off the arms to the left with a leader line; the phone colleague was cropped or over the headline in four placements, then placed from the camera's projection at the right edge; the placard overlapped the headline: the street camera's target rose and the line-up moved right, clear of the colleague; the skater starts further right so its label clears the shield's. Owner feedback: the office heads were too small. The chapter's adults keep adult limbs (so the furniture fits) but take a 0.37 m head, near toon size; both cameras pulled back to make room, and the colleague now leaves at 5.0 s with their label, so the street opens on the four entrances only |
| 07 End card | `tmp/ow/sc6.png` (f1190, f1230), `s1310.png`, `tmp/ch6/f1600.png`, `f1638.png` | line-up idles then cheers (staggered); title 0.7 s; API line 1.6 s; logo + repo URL | revise → yes | full-frame render (no viewport edge); logo inked for paper; CTA added |
| transitions | `tmp/ow/mp4_a.png`, `mp4_b.png` (f129, f420, f665, f940, f1185), `tmp/ow/f1185.png`, `f1535.png` | 10-frame dissolves; no caption overlap after the caption windows were tightened; 1535 dissolves the street into the line-up, labels fading with it | revise → yes | caption windows end before each crossfade |

## 5c. Cue-sync audit

Text-only: the anchor is the caption onset.

| Scene | Anchor (caption phrase) | Onset (s, local) | Visual start (s) | Lead | Lands at word? |
| --- | --- | --- | --- | --- | --- |
| 01 | "Just joints and timing." (≈1.9 s into the line) | 2.0 | phase ring 1.73 | 0.27 | pass |
| 02 | "Put parts on it." | 0.27 | tray 0.0, body 0.6 | n/a (on the word) | pass |
| 03 | "three materials" | 3.3 | softness dial 3.0 | 0.30 | pass |
| 04 | "Hi! I'm Pip." | 3.2 | mouth moves 3.2; chips light per shape | 0 (the mouth is the speaker) | pass |
| 05 | "drawn flat or in depth" | 2.27 | yaw dial 2.0 | 0.27 | pass |
| 06a | "find the keys" (≈1.4 s at 2.5 words/s) | 1.4 | rings + palm dots 1.0 | 0.40 | pass |
| 06b | "Phones, placards…" (caption onset) | 5.4 | truck 5.0 | 0.40 | pass; the four figures then enter in the order the line names them |

## 6. Packaging (Gate 7)

- [x] title / credits: chapter kickers, end-card title
- [x] thumbnail candidate: `one-walk-any-body-thumb` → `out/one-walk-any-body-thumb.png` (tagline adds "props"; the line-up moved right so the tagline clears it; checked at 320×180)
- [x] SRT: `out/one-walk-any-body.srt` (10 cues, same table as the screen)
- [x] logo / wordmark and CTA: `raymotion-logo` prefab + `github.com/pebaryan/raymotion`
- [x] repo link (no social handles in the house kit for docs films)
- [x] clean-machine `list` and asset behaviour

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920×1080 | 1920×1080 | pass |
| Frame rate | 30 | 30/1 | pass |
| Frame count | 1665 | 1665 | pass |
| Duration | 55.50 s | 55.499 s | pass |
| Full decode | no errors | `ffmpeg -v error -f null` clean | pass |
| Integrated loudness | -16 LUFS ±1 | -16.2 LUFS (LRA 5.2); the first pass with chapter 6 measured -17.2, so the score's saturation drive went 2.0 → 2.4 and its peak target 0.70 → 0.68 | pass |
| True peak | ≤ -1 dBTP | -1.3 dBTP | pass |
| Voice sync at chapter starts | n/a (no voice) | — | n/a |
| Encoded-frame check | frames match stills | 12 frames from the MP4 at starts/mids/ends/transitions; chapter 6 and the end card: exact frames 1230/1290/1440/1520/1535/1600–1664 (`tmp/ow/mp4_c.png`, `tmp/ch6/enc/`) | pass |
| Still vs render determinism | identical | two stills of f1060 md5-identical; still vs encoded PSNR 40.4 / 40.6 dB (f300 / f1060), 39.4 dB (f1520, after the head change); `score.wav` regenerated md5-identical (7928eb8b…) | pass |
| Source vs executable freshness | exe newer | exe 12:55:27, newest source 12:55:23 | pass |
| Final render after last edit | yes | MP4 12:56:21, thumbnail 12:55:46 | pass |
| Reviewed scene present in final | yes | encoded frames match the approved stills | pass |
| SRT present | yes | yes | pass |
| Thumbnail present | yes | yes | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | 2 Script / voice | 2 | captions are correct but flat | rewrite 2b and 5 with a point of view | owner | — |
| Authoredness | 10 Verification | 2 | no real-time viewing yet | owner watches muted, then with sound | owner | — |
| Craft | 15 Text pacing | 2 | 4b holds long after the speech | end 4b at 5.6 s | author | — |
| Craft | 18 Revision quality | 2 | self-review only | one owner feedback round | owner | — |

## 9. Decision

- [ ] **Ship**: Authoredness ≥ 22 and Craft ≥ 13, no critical fail, Gates 0–7 passed.
- [x] **Revise**: Gate 3 needs the owner's muted and sound viewing; then fix the lowest
  criteria if wanted. Keep: the conceit, the walker/tray chapter, the mouth strip, the split
  2D/3D yaw dial.
- [ ] **Rewrite**

Verdict: **Authored** at **32/36 authoredness**, **15/18 craft**. That is past the
Improvised band on authoredness, but Improvised also needs craft ≥ 16. Release-eligible
once the owner signs off Gate 3.

Reviewer signature: Claude (author) — 2026-10-30

## 10. Notes for the next author

- What worked and must be preserved: one walk on its own root spanning chapters 1–2, so
  the cut never interrupts it; the tray reads from the character's own looks, so it can't
  disagree with the picture; dial values come from the same function as the motion.
- What nearly failed:
  - 2D vector fills drawn before a `g3d::render` in the same frame were silently lost.
    `animate()` re-uploads mesh buffers mid-batch. Fixed in the engine:
    `g3d::render` now flushes the batch first.
  - A flat 2D fill of a striped outfit sampled one point (it hit the star). It now draws
    each texture row's dominant colour as bands (`detail::outfitRows`).
  - Caption windows ran into the next scene's crossfade.
  - Owner feedback: in 2D a turning head shrank its face, because the face fit a box
    narrowed by the turn, uniformly. It is now drawn at full size and foreshortened across
    only. The same review found that creating a render texture mid-frame sends pending 2D
    vertices to the wrong framebuffer. The face canvas flushes first, as `g3d::render`
    does.
  - The brick head clipped round hair, and the fringe hid Pip's brows.
  - Owner feedback, second round, in the 2D cut-out:
    - The hair was missing. Caps of hair sorted behind the head, so only a rim showed.
      They now sort just over the skull, with a 2D hairline (`Prim::from2d`) that rises
      to the brow as the head faces the viewer. The face is its own item above the cap.
      Third round: that hairline read as a hard straight edge, so it now arches. It is
      highest over the brow and falls to the cap's 3D edge at the temples. The arch is
      spliced into the cap's convex outline, which on its own can't follow the inward curve.
    - The chest drew a skin band in front of the neck. It came from the torso texture's
      skin "collar dip" (in `Outfit::basic` and `outfit-sailor.svg`), which the 2D row
      bands picked up. It is now a collar band in the garment's own colour.
    - Re-checked on stills f760/f960/f1050/f1115 and on frames from the re-rendered MP4.
      45/45 tests pass; the film is still -16.6 LUFS / -1.5 dBTP, 1315 frames, and
      decodes clean.
  - Chapter 6, "Hands on things" (owner request: put the acting feature in the film).
    Brief updated first (chapter row, reveal map, claims, metaphor map, cue map, stills).
    The film is now 55.5 s, seven chapters. The rings are drawn from the workstation
    prefab's keyboard geometry and the dots from `rig::palmAt`, so the proof is not
    staged. Labels are read from each figure's `Look` slots. Six framing rounds on
    stills (see §5b), then a seventh after owner feedback (bigger heads). Lesson: place figures by projecting through the scene camera, not
    by eye; a 34° lens from 2 m up makes standing figures read high.
    45/45 tests pass (run ctest with both `TEMP` and `TMP` pointed at a writable folder,
    or the media test aborts).
- What should be extracted into the engine or tools: a caption table → screen + SRT
  helper (every film re-implements it); the dial widget; a helper that solves the
  ground position for a point on screen.
- What should never be copied into another film: the hard-coded `PIP_BOX`, which is
  specific to this face SVG and this hair.
