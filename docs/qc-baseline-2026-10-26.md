# QC baseline: four existing examples

Date: 2026-10-26
Rubric: `skills/raymotion/references/quality-control.md` (12 criteria, 0–3, 36 points)
Scope: `ht_explainer`, `asp`, `own_compute`, `halloween` (both cuts)

This is a **calibration pass**, not an award. It tests whether the new scorecard
separates an authored film from a brief-complete demo, and whether it catches the
things the old workflow let through.

**Baseline note:** the detailed sections below score the 12-criterion authoredness axis,
as in the original baseline. A **Craft & Coherence retrospective** was added after the
2026-10-26 v2 feedback; it is scored separately and changes some verdicts. See
`skills/raymotion/references/quality-control.md` for both axes.

## Method

- Read the source, the docs (when present), and the render/asset pipeline.
- Extracted stills at chapter/act midpoints and built contact sheets into `out/qc/`.
- Inspected stills at full size for legibility, safe area, glyphs, and proof objects.
- Measured the encoded files with `ffprobe`, a full decode, and `ebur128`.
- Did **not** audition the mix; sound is assessed from the score code plus measured
  loudness/peak. A human listen is still required for Gate 3.
- Scores are for the current artifacts, not the authors' intentions.

## Artifacts and technical QA

| Work | File | Frames | Duration | LUFS | True peak | Decode |
| --- | --- | --- | --- | --- | --- | --- |
| here-there-explainer | `out/here-there-explainer.mp4` | 3,240 | 108.0 s | -10.8 | -1.4 dBTP | OK |
| asp-feed | `out/asp-feed.mp4` | 880 | 29.33 s | -10.3 (LRA 0.6) | -1.4 dBTP | OK |
| own-your-compute | `out/own-your-compute.mp4` | 1,980 | 66.0 s | -15.8 | **-0.4 dBTP** | OK |
| dukun-vs-demit (2D) | `out/dukun-vs-demit.mp4` | 5,608 | 186.93 s | -18.5 | **-0.7 dBTP** | OK |
| dukun-vs-demit-3d | `out/dukun-vs-demit-3d.mp4` | 3,078 | 102.6 s | -18.2 | -1.6 dBTP | OK |

`build/raymotion_ht_tests.exe` also passed 32 HT semantic checks. The feed suite
(`raymotion_feed_tests.exe`) passed registration, storyboard, layout and real-GL render
checks; those cover the `here-there-feed` companion, not the `ht_explainer` cut.

Frame counts and durations match the code:

- `ht_explainer`: 9 chapters × 360 frames = 3,240 (`ht_explainer.cpp:12`).
- `asp-feed`: 9 scenes with overlapping transitions = 880 frames (`asp.cpp:13-290`).
- `own-your-compute`: `DURATION = 1980` (`own_compute.cpp:29`).
- `dukun-vs-demit`: 12 shots with 22-frame overlaps = 5,608 frames.
- `dukun-vs-demit-3d`: 9 shots with 24-frame overlaps = 3,078 frames.

The bold peaks fail the `≤ -1 dBTP` target in the QC technical checks. The loudness
of `ht_explainer` and `asp-feed` is also hot for web (about -10 LUFS); `asp-feed` has
almost no dynamic range (LRA 0.6).

## Summary

| Work | Score | Band | Gates | Verdict |
| --- | --- | --- | --- | --- |
| here-there-explainer | **27 / 36** | Authored | 0 c, 1 c, 2 pass, 3 fail, 4 pass, 5 fail | Strong authored explainer; not releasable under the new gates |
| asp-feed | **17 / 36** | Brief-complete | 0 fail, 1 fail, 2 not evidenced, 3 fail, 4 fail, 5 fail | Polished social demo; not authored |
| own-your-compute | **29 / 36** | Authored | 0 fail, 1 fail, 2 not evidenced, 3 c, 4 fail (peak), 5 fail | Visually strong feature film; no script/voice/verification |
| dukun-vs-demit (2D) | **24 / 36** | Authored | 0 fail, 1 fail, 2 not evidenced, 3 fail, 4 fail (peak), 5 fail | Ambitious procedural horror; too dark and undocumented |
| dukun-vs-demit-3d | **26 / 36** | Authored | 0 fail, 1 fail, 2 not evidenced, 3 fail, 4 pass (no log), 5 fail | Stronger cut; still no script/voice/verification |

No existing example reaches **30 on the authoredness axis**, and none passes all gates.
That is a useful result: the new gate is stricter than the workflow that produced these
pieces. The Craft & Coherence retrospective below adds the second axis and changes the
release verdict for several of them.

---

## 1. here-there-explainer (`ht_explainer`)

**What it is:** a nine-chapter, 108-second, caption-led lecture on Here-and-There and
equilibrium logic, built directly with raylib drawing calls and the raymotion timeline
(`examples/showcase/ht_explainer.cpp`). Sources, script mapping and verification are in
`docs/here-there-explainer.md`; the logic is checked by `tests/ht_logic_tests.cpp`.

### Scorecard

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 2 | “One instant, two levels of truth” is a specific idea, but the film is a lecture; the conceit is not stated in the first five seconds. The opening is a title and the fire/smoke example. |
| 2 | Script / voice | 2 | Complete text-only chapter script in the docs; no voice. Timing is fixed at 12 s per chapter; there is no phrase-level cue map. |
| 3 | Evidence | 3 | Every chapter visualizes its claim: classical models, H/T worlds, atomic states, both checks, negation, equilibrium, a fact, temporal traces. The verifier tests the two-atom examples. |
| 4 | Timing | 2 | `CHAPTER=12*FPS`, `CHAPTERS=9`; internally consistent, but a fixed clock. The script is fitted to the chapters, not the other way around. |
| 5 | Sound | 2 | A chapter-aligned tonal bed (`c.song`, one chord pair per chapter plus plucks). No voice, ducking, or SFX. Measured -10.8 LUFS, -1.4 dBTP. |
| 6 | Visual system | 2 | Consistent panel grammar, palette and type sizes, but it uses `GetFontDefault()` and direct `Draw*` calls, not `fonts::ui`/`dr::`; the font is low-resolution and the slides are dense. |
| 7 | Chapter differentiation | 2 | Nine distinct diagrams, but the same three-zone slide (header, diagram, callout) every time. The persistent frame is the header/footer rather than a film language. |
| 8 | Data & provenance | 3 | Primary sources, exact chapter mapping, scope caveats (“visual examples are our own”), and a limiting verifier. |
| 9 | Production pipeline | 2 | Pure code, no external assets; `list` works on a clean machine. No thumbnail or SRT; no `c.assets` pipeline. |
| 10 | Verification | 3 | Docs record ffprobe/full decode/encoded-frame checks; tests cover the logic; this review independently confirmed frame count, decode and loudness. |
| 11 | Memorability / thumbnail | 1 | The H/T pair is a reusable motif, but there is no chosen thumbnail and no image that would sell the film at feed size. |
| 12 | Specificity | 3 | The form is inseparable from HT/equilibrium; reskinning would require a new diagram system. |

**Total: 27 / 36 — Authored.**

### Gates

| Gate | Result | Note |
| --- | --- | --- |
| 0 Brief | conditional | `docs/here-there-explainer.md` has sources and a chapter script, but not the new one-sentence conceit, visual-system, asset-plan and thumbnail fields. |
| 1 Script / cue map | conditional | Text-only script exists; timing is a fixed 12 s per chapter, not a cue map. |
| 2 Stills | pass | Nine chapter stills and a contact sheet in `out/here-there-review/`; inspected at 1:1. Transitions are a fade wash in code, not a separate layout. |
| 3 Draft | fail | No draft artifact; the docs record a final verification, not a muted/sound draft pass. |
| 4 Final technical | pass | 3,240 frames, 108 s, decode OK, -1.4 dBTP. Loudness is hot for a bed-only piece but peaks are legal. |
| 5 Authored test | fail | Mute test passes; reskin test passes; clean machine passes; but retiming is not safe and there is no thumbnail. |

**Verdict:** a strong authored explainer, but not releasable under the new gates. To
reach release: add a one-page brief, a phrase cue map or justify the fixed form, a
draft pass, a thumbnail, and use the engine font system (or document the deliberate
default-font choice).

---

## 2. asp-feed (`asp`)

**What it is:** a 29.33-second vertical social explainer about answer set programming,
in nine 108-frame scenes with a small illustration per scene (`examples/showcase/asp.cpp`).
No docs, no source ledger, no voice, no SRT.

### Scorecard

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 2 | “Describe what a solution looks like, not the steps” is a clear hook and is visible early, but the film is a generic card explainer; the ending is a slogan. |
| 2 | Script / voice | 2 | The captions have a voice and a point of view, but there is no script document, no voice, and no cue map. |
| 3 | Evidence | 2 | Each concept gets a small illustration: step tower, HOW/WHAT, fact chips, rule arrow, world cards, choice, constraint cross. The cards illustrate but do not prove; the captions carry the argument. |
| 4 | Timing | 1 | Every scene is 108 frames; the 29.33 s cut is a fixed clock. No content-driven timing. |
| 5 | Sound | 1 | A single Am–F–C–G arpeggio loop (`asp.cpp:18-30`); no SFX, no ducking, no voice. Measured -10.3 LUFS, LRA 0.6: very compressed and static. |
| 6 | Visual system | 2 | Consistent palette, type and chip/card motifs; the same caption-top/art-middle layout. At 1080×1920 the illustration occupies a small part of the frame. |
| 7 | Chapter differentiation | 2 | Nine distinct simple illustrations, but several are the same card row with different labels; the outro is a title card. |
| 8 | Data & provenance | 1 | No sources, no data, no scope note. It is conceptual, so the cost is lower, but there is no ledger or “illustrative” labelling. |
| 9 | Production pipeline | 1 | No assets or `c.assets` pipeline; the score is an in-engine `c.song`; no voice, thumbnail, or SRT. It runs on a clean machine because it has nothing to load. |
| 10 | Verification | 1 | No stills, draft, or final QA record. This review extracted stills and measured the file. |
| 11 | Memorability / thumbnail | 1 | Clean cards, but no chosen thumbnail and no image that states the ASP idea at feed size. |
| 12 | Specificity | 1 | The same nine-card structure could sell any “describe the goal” concept by changing labels. |

**Total: 17 / 36 — Brief-complete.**

### Gates

Gate 0 fail (no brief), Gate 1 fail (no script/cue map), Gate 2 not evidenced (no author
stills; this review extracted them), Gate 3 fail (no draft), Gate 4 fail (no author QA
log; this review found peak -1.4 dBTP but a hot, flat mix), Gate 5 fail (no thumbnail,
fixed clock, reskin test fails).

**Verdict:** a clean social demo, not an authored film. To move up, it needs a written
script with a real example (actual solver output or a sourced ASP example), a content-
driven scene length, a composed score with SFX, a source/scope note, a draft review, and
a thumbnail. The visual vocabulary is already good enough to build on.

---

## 3. own-your-compute (`own_compute`)

**What it is:** a 66-second, seven-act story film about running AI on your own machine,
with generated 3D, glitch, particles, a procedural GPU, a neural net, a terminal, and a
dawn island (`examples/showcase/own_compute.cpp`). Every asset and the score are generated
by code. No docs, no voice, no source ledger.

### Scorecard

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | “Own your compute”: the cloud bill and the dark server aisle become a local machine, a downloaded model, an offline terminal, and a home under a privacy dome. The arc is visible from the first shot to the title. |
| 2 | Script / voice | 2 | Sparse, well-written captions (`own_compute.cpp:1016-1026`) but no script document, no voice, no cue map. |
| 3 | Evidence | 3 | Each act demonstrates its claim with a mechanism: the meter climbs, the stamps hit, the cloud pours into a chip, the fans spin, the terminal streams an answer with the network off, the dome closes. A muted viewer can follow the arc. |
| 4 | Timing | 2 | Act boundaries `A1..A7` (`own_compute.cpp:34`) are a single source of truth, and the score lands on those seconds; but the form is a fixed clock with no narration to derive it from. |
| 5 | Sound | 3 | A composed score with act cues: cold pads and ticks under the cloud, glitch stabs on the stamps, a riser, an impact and a 120 bpm groove on Dm–Bb–F–C, resolving to D major under the title (`own_compute.cpp:257-310`). No voice or ducking, but none is needed for this cut. Measured -15.8 LUFS. |
| 6 | Visual system | 2 | A coherent palette, letterbox, title treatment, Inter/mono type and recurring motifs (bill, LED, teal, dome), but the system is documented only in code comments/README, not as a design brief. |
| 7 | Chapter differentiation | 3 | Seven distinct acts with different media (3D aisle, 2D glitch, particle morph, 3D GPU, neural net, terminal, 3D island). No filler. |
| 8 | Data & provenance | 2 | No factual claims or numbers, so no ledger is needed; the “every mesh, texture and note generated from code” claim is true. No scope note. |
| 9 | Production pipeline | 2 | `c.assets` generates PCB, wood, rack and score; clean-machine `list` works. No voice/SRT/thumbnail, and `assetsReady` is a global-state hack. |
| 10 | Verification | 2 | A draft (`out/own-your-compute_draft.mp4`) and a final exist; no still/draft/QA log. This review found a true peak of -0.4 dBTP, above the -1 target. |
| 11 | Memorability / thumbnail | 2 | Strong images (server aisle, GPU, island/dome) but no chosen thumbnail. |
| 12 | Specificity | 3 | The acts are specific to local-AI compute; a topic swap would require new art, not a text edit. |

**Total: 29 / 36 — Authored (just under Improvised).**

### Gates

Gate 0 fail (no brief), Gate 1 fail (no script/voice/cue map), Gate 2 not evidenced (no
author stills; this review extracted them), Gate 3 conditional (a draft exists, no
review record), Gate 4 **fail** (true peak -0.4 dBTP; no loudness log), Gate 5 fail
(mute passes, reskin mostly passes, clean machine passes, but no thumbnail and the fixed
acts cannot be retimed from a script).

**Verdict:** visually the strongest of the four, and close to the Improvised band. It is
still a **feature film without a script**: the visuals and score are authored, but the
words are sparse captions and there is no source/scope apparatus. To reach 30+: add a
short written script (even a text-only one) with a cue map, a one-page visual brief, a
proper QA log (and pull the peak down below -1 dBTP), a draft-watch note, and a
thumbnail. The engine is doing the heavy lifting already; the missing piece is the
authoring layer.

---

## 4. halloween (`dukun-vs-demit` and `dukun-vs-demit-3d`)

**What they are:** a procedural Indonesian folk-horror featurette in two cuts: a 2D cut
(12 shots, 186.93 s) and a 3D adaptation (9 shots, 102.6 s). Both are generated entirely
from code, with a procedural score and generated rain/wind/screech WAVs. No docs, no
voice, no script, no source ledger.

### 4a. dukun-vs-demit (2D)

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | A specific folk-horror duel: a dukun against a demit in a haunted kampung, with a ritual, a possession, a banishment and a final eye. |
| 2 | Script / voice | 2 | The Indonesian captions tell the story and have a voice, but there is no script document, no voice, no cue map. |
| 3 | Evidence | 2 | The images carry the story (grave, rise, title, village, ritual, duel, vortex, dawn, twist), but many frames are nearly black; the captions do a lot of narrative work. |
| 4 | Timing | 2 | Fixed shot lengths and caption windows; `sceneStart` is recorded before `ts.add` (`main.cpp:444-448`), so audio cues are one transition late; there is no shared cue map. |
| 5 | Sound | 2 | A composed score with motifs, thunder, heartbeat, chant and clash, plus generated rain/wind/screech. No voice/ducking. Measured -18.5 LUFS, -0.7 dBTP. |
| 6 | Visual system | 2 | Consistent palette, letterbox, grain, rain and red moon; the 2D figures are blobs and the contrast is very low. |
| 7 | Chapter differentiation | 2 | Twelve shots, but many are the same night sky/forest/moon; the ritual, vortex and dawn are distinct. |
| 8 | Data & provenance | 1 | Fiction; no sources, no cultural notes, no scope note. |
| 9 | Production pipeline | 2 | Generated audio and `c.assets`; clean-machine list works. No thumbnail/SRT/voice. |
| 10 | Verification | 1 | No docs/stills/draft/log; this review extracted stills and measured the file. |
| 11 | Memorability / thumbnail | 2 | The title card and red moon are strong; no chosen thumbnail. |
| 12 | Specificity | 3 | The Indonesian folk-horror frame, language and characters are specific; a topic swap would require a new film. |

**Total: 24 / 36 — Authored.**

### 4b. dukun-vs-demit-3d

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | Same specific folk-horror duel, tighter and more cinematic. |
| 2 | Script / voice | 2 | Act cards and captions tell the story; no script/voice/cue map. |
| 3 | Evidence | 2 | The 3D images are more legible (graveyard, rise, title, village, ritual, duel, vortex, dawn), but the captions still carry the plot. |
| 4 | Timing | 2 | Fixed shot lengths; `sceneStart` is again one transition early (`main3d.cpp:492-498`); no cue map. |
| 5 | Sound | 2 | Composed score and generated ambience; no voice/ducking. Measured -18.2 LUFS, -1.6 dBTP. |
| 6 | Visual system | 3 | Cinematic serif titles, act cards, letterbox, a consistent night palette and a 3D world; one of the strongest visual systems of the four. |
| 7 | Chapter differentiation | 3 | Nine distinct shots, each with its own set and action; no filler. |
| 8 | Data & provenance | 1 | Fiction; no sources or scope note. |
| 9 | Production pipeline | 2 | Generated audio in `c.assets`; clean-machine list works; no thumbnail/SRT/voice. |
| 10 | Verification | 1 | No docs/stills/draft/log; this review extracted stills and measured the file. |
| 11 | Memorability / thumbnail | 2 | Strong title and ritual images; no chosen thumbnail. |
| 12 | Specificity | 3 | Specific to the story and the Indonesian setting. |

**Total: 26 / 36 — Authored.**

### Gates (both cuts)

- Gate 0 fail: no brief, script, sources or scope note.
- Gate 1 fail: no script/cue map; fixed shot lengths.
- Gate 2 not evidenced: no author stills; this review extracted them.
- Gate 3 fail: no draft-watch record.
- Gate 4: **2D fails** (true peak -0.7 dBTP); 3D passes the peak (-1.6) but has no
  loudness log.
- Gate 5 fail: mute test is marginal (the 2D cut is very dark), thumbnail is absent,
  retiming is not safe, clean machine passes, distinct-chapter test passes for 3D but is
  weaker for 2D.

**Verdict:** an ambitious, authored genre piece. The 3D cut is the better film; the 2D
cut is too dark and too long for its story. To reach release: lift the 2D exposure and
contrast, fix the `sceneStart`/audio offset, write a shot list with a cue map, add a
source/scope note for the folklore, pull the 2D peak below -1 dBTP, add a draft-watch
record and a thumbnail, and decide which cut is the release.

---

## Craft & Coherence retrospective (2026-10-26)

This axis was added after the v2 feedback. The old 36-point score did not move because
it is saturated by the authoredness criteria; these six criteria score the qualities the
feedback actually exposed.

| Work | Mode | 13 Scenario | 14 Info design | 15 Text pacing | 16 Mode grammar | 17 Spec-to-screen | 18 Revision | Craft /18 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| here-there-explainer | explainer | 3 | 3 | 2 | 2 | 3 | 0 | **13** |
| asp-feed | explainer | 2 | 2 | 1 | 2 | 2 | 0 | **9** |
| own-your-compute (v1) | promo | 2 | 2 | 1 | 2 | 2 | 0 | **9** |
| dukun-vs-demit (2D) | narrative | 1 | 2 | 2 | 2 | 1 | 0 | **8** |
| dukun-vs-demit-3d | narrative | 1 | 3 | 2 | 3 | 1 | 0 | **10** |
| own-your-compute-v2 (reference) | promo | 3 | 3 | 3 | 3 | 2 | 3 | **17** |
| asp-feed-v2 (reference) | explainer | 3 | 2 | 3 | 3 | 3 | 3 | **17** |

Revision = 0 for the original examples because they have no feedback cycle; that is
_unearned_, not a defect. The v2 earns 3 by incorporating the review. The v2 is included
as a reference, not as part of the original baseline.

Why the old examples score as they do, **in their own mode**:

- **ht_explainer (13, explainer):** strong H/T diagrams, a tested model and a consistent
  lecture grammar. It loses points on static slide text and the absence of a voice/reading
  rate, but it passes the craft gate. Its blockers are the draft/retiming, thumbnail and packaging gates.
- **asp (9, explainer):** coherent concept and a consistent social-card grammar, but the
  proof is thin (no chart/map/worked example), the scenes are fixed and the text is static.
- **own_compute v1 (9, promo):** a 4.7 GB model on a 24 GB / 500 W machine, a bill/GPU
  demo but no cost chart/map, sparse captions and a made-with credit. The promo grammar is
  coherent; the proof and pacing are not.
- **Halloween 2D (8, narrative):** the genre grammar and staging are there, but the
  `sceneStart`/audio offset is a continuity bug, the frames are dark, the subtitles are
  static and there is no shot list/scope note.
- **Halloween 3D (10, narrative):** the strongest genre grammar (act cards, serif titles,
  3D staging), but the same timing bug and no shot list/scope note.

### How much the score changed

The authoredness score is unchanged; the craft axis adds the new points. The combined
`/54` is a quick read of both axes, not the official release rule.

| Work | Mode | Authoredness /36 | Craft /18 | Combined /54 | Added by craft | Old verdict | New two-axis verdict |
| --- | --- | --- | --- | --- | --- | --- | --- |
| here-there-explainer | explainer | 27 | 13 | 40 | +13 | Authored | **Revise** - craft passes; draft/retiming, thumbnail and packaging gates fail |
| asp-feed | explainer | 17 | 9 | 26 | +9 | Brief-complete | **Brief-complete** |
| own-your-compute (v1) | promo | 29 | 9 | 38 | +9 | Authored | **Revise** - Craft 9, gate 13 |
| dukun-vs-demit (2D) | narrative | 24 | 8 | 32 | +8 | Authored | **Revise** - Craft 8, gate 13 |
| dukun-vs-demit-3d | narrative | 26 | 10 | 36 | +10 | Authored | **Revise** - Craft 10, gate 13 |
| own-your-compute-v2 (reference) | promo | 33 | 17 | 50 | +17 | - | **Preview approved; release blocked on voice/SRT/thumbnail/human watch** |
| asp-feed-v2 (reference) | explainer | 33 | 17 | 50 | +17 | Brief-complete v1 | **Preview approved; release blocked on human watch/voice** |
| here-there-explainer-v2 (reference) | explainer | 34 | 17 | 51 | +17 | Authored v1 | **Improvised; preview approved; release blocked on human watch/voice** |
| here-there-feed-v2 (reference) | explainer | 34 | 17 | 51 | +17 | 88 s preview-only v1 | **Improvised; preview approved; release blocked on human watch/voice** |

**The meaningful change is the verdict, not the old number.** `ht_explainer` now passes
the craft gate (13/18) because it is judged as a lecture, not a promo; it still needs its
draft, thumbnail, SRT and packaging before release. `own_compute` v1 and both Halloween
cuts remain craft-blocked, but for mode-relevant reasons: promo proof/pacing for the
former, continuity/staging/shot-list for the latter. `asp` v1 remains brief-complete;
the v2 cuts are the only ones clearing the showcase craft gate (17/18), and their
release still waits on the human watch; asp and the feed v2 also wait on an optional voice.

The mode-awareness matters: a horror film is not failed for lacking a chart, and a logic
explainer is not failed for lacking a theatrical arc. The old 36-point scores are unchanged;
the craft axis adds the mode-specific points.

## What this test revealed about the rubric

1. **The score works as a separator.** The one piece with a real source/script/verification
   apparatus (`ht_explainer`) sits at 27; the clean but generic social demo (`asp`) sits
   at 17; the visually strong but scriptless feature film (`own_compute`) sits at 29.
   That ordering matches the qualitative read.

2. **“Authored” is not the same as “releasable.”** `own_compute` (29) and the Halloween
   cuts (24/26) are authored by score, but they fail Gates 0, 1, 3, 4, 5, 6 or 7. The gates are
   doing their job: a strong render is not a release.

3. **The text-only path needs its own wording.** `ht_explainer` is deliberately text-only,
   and that is legitimate, but criterion 2 still needs a written script or cue map, not
   just captions pasted on screen. The rubric should say “script or text-only cue map”
   explicitly in the anchor, not only in the surrounding prose. (It mostly does; make it
   impossible to miss.)

4. **Narrative evidence is under-specified.** The evidence table is built for claims and
   data. For a horror film, the “proof object” is the image that carries the beat, and the
   mute test is “can you follow the story without the captions?” Add a narrative row to
   the evidence table, and make the mute test distinguish “no sound” from “no captions.”

5. **The true-peak gate catches real problems.** `own_compute` (-0.4 dBTP) and the 2D
   Halloween cut (-0.7 dBTP) fail the -1 dBTP target even though they look and sound
   finished. This is exactly the kind of thing a scorecard should catch. Keep the gate
   hard.

6. **The retiming test is strict for fixed-form pieces.** A lecture with nine 12-second
   chapters and a 7-act visual poem are not built to retime. That is a legitimate form,
   but the rubric should let it pass with an explicit “fixed form” justification and a
   cue map that explains the lengths; otherwise every fixed-form piece fails Gate 5.

7. **Thumbnail is the most common missing artifact.** None of the four has a chosen
   thumbnail. The rubric already penalizes it; the gate should make it a release blocker
   for any public cut.

8. **No docs is not just a documentation gap.** `asp`, `own_compute` and Halloween have no
   brief, source ledger or QA artifact, and that shows up in criteria 8–10 and in Gates
   0, 1, 3 and 4. The docs are part of the work, not an afterthought.

9. **The craft axis was the missing piece.** The 36-point authoredness score saturated;
   the six craft criteria move the own_compute craft score from 7 (v1) to 17 (v2) and change `own_compute` and both
   Halloween cuts from "Authored" on the old score to "Revise" under the Craft ≥ 13 gate.
   `ht_explainer` now passes craft (13/18) when judged as a lecture; its remaining blockers
   are packaging and the draft/stills gates.

10. **One rubric, four modes.** Halloween is a narrative, asp/ht are explainers, and
   own_compute is a promo. The old craft anchors implicitly demanded a chart/source/brand
   card from every mode. The mode contract fixes that: narrative staging/continuity,
   explainer charts/sources, promo demo/benefit/CTA, utility layout. Without a declared
   mode, the score is not comparable across examples.

## Recommended calibration adjustments

- In criterion 2, make the anchor explicit: **“script or text-only cue map”**; a caption
  track without a written script is a 1, not a 2.
- Add a **narrative evidence** row to the evidence table, and define the mute test as
  “without sound and without captions” for the image-only pass.
- In Gate 5, allow a **fixed-form exception**: pass the retiming test if the brief
  declares a fixed form and the cue map justifies each length; otherwise fail.
- Treat **true peak > -1 dBTP** as a Gate 4 fail, not just a criterion-10 deduction.
- Make **thumbnail candidate** a Gate 0 field and a Gate 5 release blocker for public
  cuts.
- Add a **“no-assets” note** to criterion 9: a pure-code piece can pass with a clean
  machine even if it has no `c.assets` pipeline; do not penalize it for the absence of
  assets it does not need.

## Suggested next actions

1. Keep the two-axis scorecard; do not merge authoredness and craft into one number.
2. Fix the two objective technical fails first: `own_compute` true peak and 2D Halloween
   true peak.
3. `here-there-explainer-v2` is now the explainer reference (34/17): keep the
   v1/v2 pair as the before/after for engine typography, a cue map, a score and packaging.
4. `own_compute` v1 needs the chart/map, karaoke/house captions, brand kit and a script;
   the v2 is the reference. Halloween needs the timing fix, exposure, docs and brand;
   `asp` and `ht_explainer` now have v2 references.
5. Finish the v2 release gaps (human watch; optional voice), then re-run both scorecards.
