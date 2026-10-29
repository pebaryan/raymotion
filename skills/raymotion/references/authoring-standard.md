# Authoring standard: author the film, not the demo

**Status: mandatory.** Read this before writing a raymotion composition. Fill
`../templates/BRIEF.md` before writing code. A raymotion piece is not finished when it
renders; it is finished when a muted viewer can follow the argument, every claim has a
proof object or a source, and the sound and timing belong to the subject.

This standard scales. A 10-second lower third may use a one-paragraph brief, but it
still needs a conceit, a reason for each visual choice, and a still review. Do not
scale the principles down to nothing.

---

## 0. The one-sentence test

Before anything else, write one sentence:

> This is a film about **\_\_\_\_** that makes the viewer **feel / understand \_\_\_\_**
> by **\_\_\_\_**.

It must be specific enough that it could not describe three other videos. "A history of
Austria" is a topic. "A time machine that sets the dial back from 2026 to 3300 BC, with
one real artifact per stop" is a conceit. "The tokenizer is a price list written by
popularity, so the same sentence costs more in Japanese" is a conceit.

If you cannot write the sentence, stop. You are not ready to write code.

---

## 1. Brief before code

Copy `../templates/BRIEF.md` to `docs/<slug>-brief.md` and fill every section:

- audience, runtime, platform, aspect ratio, voice (or a declared text-only cut);
- one-sentence conceit and one-line argument;
- chapter table: time, chapter, on-screen text, visual action, **proof object**, sound
  cue;
- script, caption text, and cue map;
- source ledger for every number and historical claim;
- visual system: palette meaning, type scale, margins, motion grammar, recurring motif;
- audio plan: voice, score, ducking, SFX, loudness target;
- asset plan: generated vs committed, fallback if missing;
- still frames to review;
- acceptance checks and known risks.

Do not write composition code until the brief is reviewed. The brief is a contract.
When the film changes, update the brief in the same change, not a week later.

---

## 2. Script first, then timing

The script is the spine. Write it before the storyboard is locked.

- One idea per sentence. Twenty words is a good ceiling.
- Plain phrase first, jargon second. Define each term once.
- Numbers always carry their unit: "tokens per word", "characters per token", "at the
  same vocabulary size". Never a bare number.
- Round for speech, keep exact values for labels. Say "about sixty", label `60.08`.
- Do not claim more than the source does. Mark illustrative values on screen, not only
  in a private note.
- Banned words unless quoted from a source: "solves", "eliminates", "fair AI",
  "breakthrough", "revolutionary". Prefer the paper's own hedge: prototype, preliminary,
  small-scale.

If there is a voice:

- Generate one file per chapter, then read its real length and derive the scene length
  from it. Do not paste finished audio into a clock that was decided first.
- Keep a single cue map from phrase to frame. Every visual and music cue reads the same
  map.

If there is no voice, declare that in the brief and write the cue map anyway. Captions
are a script, not a subtitle afterthought. A silent piece is legitimate only when the
images and typography carry the argument on their own.

Reference pattern: `Vo` with `seconds`, `frames()` and `cue(phrase)`, `Chapter` built from
those lengths, and a `Plan` of cuts/speech/cues in
`examples/showcase/austria_time_travel.cpp`. Reference anti-pattern: a fixed clock with
narration pasted at hand-picked frames.

---

## 3. Evidence rule

Every chapter needs a **proof object**: the thing the viewer can see that makes the
claim true. Not a label. Not a caption.

| Claim type | Proof object |
| --- | --- |
| A quantity | chart, bar, tower, map, counter, count-up with a unit |
| A process | mechanism, press, conveyor, loop, state change |
| A place | map, globe, terrain, projected outline, route |
| A person or event | artifact, document, quote, date page, count of victims |
| A concept | one physical metaphor that can be drawn and reused |
| A comparison | two things on one scale, with the same unit |

If a beat has no proof object, cut it or merge it into a beat that does. A decorative
vignette next to a paragraph is not a scene; it is a slide.

Sourcing rule: every number or historical claim has one of these:

1. a source in the brief's ledger, or
2. an explicit on-screen "illustrative" label.

Do not silently round, invent precision, or imply causality the source does not claim.

---

## 4. Timing rule

Keep one source of truth for time.

- Scene starts come from `node->start`, not a second recomputation.
- Voice lengths come from the actual WAVs.
- Picture cues and score cues come from the same phrase/cue table.
- If a number must be hardcoded, derive it from the script and assert the sum.
- Scene lengths plus transition overlaps must equal `c.duration`; check it.

Good: `ch.start = node->start;` and `Chapter::cue(line, phrase)` in the Austria time-travel
film. Bad: scene starts hardcoded in one array, voice offsets hardcoded in another, and
the score ducking hardcoded in a third.

If retiming a chapter would require editing three unrelated arrays, the timing design is
wrong.

---

## 4b. Staging and narration sync

A scene is a **timed hierarchy**, not a finished layout with an entrance animation. The
viewer must receive one unit at a time, in the order the argument needs it.

### The reveal map

For every scene, write a reveal map before animating:

| Unit | Spoken anchor | Visual start | Lands at | Hold |
| --- | --- | --- | --- | --- |
| headline | first phrase | -0.28 s | word onset | 1.5 s |
| proof object | "282 million" | -0.28 s | "two" | 1.0 s |
| annotation | "funding" | -0.28 s | "funding" | 1.0 s |
| source | after the claim | -0.20 s | last word | end |

Rules:

- **Never start at final density.** A still at `t = 0` should show only the first unit;
  the last unit may not appear until the narration earns it.
- **Sync to words, not line starts.** Measure the actual word onsets (forced alignment
  or ASR word timings). A line-level cue is not enough.
- **Lead by 0.20-0.40 s.** The visual begins slightly before the spoken word so the
  viewer anticipates it, then resolves on the word. Do not lead by more than half a
  second; the picture must not tell the story before the voice.
- **Hold after each arrival.** A new unit may not start until the previous one has been
  readable for roughly 0.8-1.5 s. The final unit holds until the line ends.
- **One entrance at a time.** Do not animate two unrelated units in the same interval.
  A group (a Venn set, a histogram, a specimen) may enter as one unit.
- **Semantic order beats convenience.** If the narration says amino acids, vitamins,
  lipids, the specimens appear in that order. If it says "both", the intersection is the
  thing that appears, not the whole diagram.
- **No duplicate labels.** Put a term in the diagram or in its key, not both. A label
  that is already on the right panel does not belong inside the Venn circle.
- **Animate the relation, not the decoration.** The motion must explain the claim:
  columns fill then rows add; folds duplicate rightward; a histogram copy moves and
  morphs; an interval rectangle covers the stated range; a lock arrives after the box.

### Scene review loop

- Review **one scene per pass**: a scene-only clip or 2-3 stills, then stop for notes.
- Do not use a full contact sheet or a finished full-film pass as a substitute for scene
  approval.
- Record the approved scene, the notes and the re-render in the QC scene ledger.
- Rebuild after every source edit. Check that the executable is newer than the source;
  a stale binary can render the previous scene even when the source is correct.
- When a scene changes after approval, re-open it and re-run its clip before moving on.

---

## 5. Sound rule

The default is a real soundtrack: voice, score, ducking, SFX, and a verified mix.

- A single pad, loop, or generic bed is not a score. It is a placeholder.
- Place SFX on picture beats: impact, stamp, press, whoosh, key click, door, explosion.
- Duck the music under speech with an explicit `volumeAt` or score duck array.
- Leave headroom for the encoder. Check integrated loudness and true peak.
- Captions/SRT are part of the audio deliverable, not an export afterthought.
- Silence is allowed only when the brief says why the silence is doing work.

Reference: the cue-driven score, custom strings/flute/wind and speech-span ducking in
`examples/showcase/austria_time_travel.cpp`.

---

## 6. Visual system

Document the system in the brief before drawing:

- **Palette with meaning.** One colour per idea, not one colour per mood. State what
  each accent means and where it is allowed.
- **Type.** Typeface families, sizes for kicker/headline/body/caption/footnote, line
  height, tracking, and the CJK fallback if the film uses CJK.
- **Layout.** Margins, safe area, caption box, HUD position, chart area. Nothing may
  collide with the caption box.
- **Motion grammar.** Entrance duration, easing, transition types, idle motion policy.
  "No idle motion except the progress line" is a valid rule.
- **The motif.** One repeated visual object that makes the film recognisable: a token
  chip, a dial, a card, a tower, a pin. Reuse it; do not invent a new visual language
  per chapter.
- **One idea per screen.** If a still needs a caption to explain the chart, the chart is
  wrong. Fix the chart before adding words.

Each chapter must have its own visual idea. A persistent frame (HUD, kicker, palette,
progress line, year dial) unifies them. If every chapter uses the same split card with a
new illustration in the same box, you have a deck, not a film.

### 6b. Visual metaphor contract

For each abstract or numeric claim, name the metaphor and the exact mapping before
drawing it:

| Claim | Metaphor | Mapping | Animation |
| --- | --- | --- | --- |
| "both methods must pick" | two-set Venn | left = linear/LASSO, right = tree/RF, overlap = both | left set + its key; right set + its key; overlap + stamp |
| "5-fold cross-validation" | five boxes | one box per fold | duplicate the rightmost box and shift right until five |
| "220 molecules x 300 samples" | matrix | columns = molecules, rows = samples | fill the top row, then add rows downward |
| "ComBat corrects the picture" | histogram pair | left = before, right = after | duplicate, recolor, move right, morph |
| "0.50-0.70 interval" | range band | x-range = held-out interval | transparent rectangle fades over the stated range |
| "test set is locked" | padlock | body = test set, shackle = locked | lock arrives after the test box |

Rules:

- The metaphor must be **readable without its caption**. If the viewer needs the
  caption to decode the diagram, the diagram is wrong.
- One unit enters at a time; the **change** is the animation. Do not animate a static
  metric for decoration.
- Do not repeat a term in the diagram and its key. Choose one home for each word.
- Reuse a proven prefab or drawing helper; do not redraw a lock, icon or geometry three
  times. A prefab must be geometrically correct at its intended size (e.g. a padlock
  shackle is a half-circle, not a full ring).
- Place a dependent object after its parent: the lock after the test box, the value
  after the box, the interval after the dots it explains.

---

## 7. Data, assets and the clean machine

- Put data in arrays next to the drawing code, with a source comment. Do not scatter
  hand-typed values through draw calls.
- Label illustrative data on screen. Check that charts and labels cannot drift apart.
- Generate files in `c.assets` when the film can generate them. Key generated filenames
  by duration or content when the output depends on timing.
- Build GL resources lazily: `txt::later`, `lazyMap`, `g3d::draw` scenes, fonts inside
  drawing callbacks. Do not load fonts or textures at registration.
- Provide a fallback for missing assets: return early from registration when the script
  is absent, synthesize a duration from text when audio is absent, or fail with a clear
  message. `list` must work from any directory.
- Use deterministic randomness only: `rnd01(seed, index)`, `rrange(seed, index, a, b)`,
  or a frame-indexed hash.
- Run the clean-machine test: rename the generated/committed assets and confirm the
  failure mode is the one the brief promises.

---

## 8. Code rules

Follow the rules in `SKILL.md` without exception:

- frames are pure functions of `t`; no accumulated state between frames;
- `up` assigns absolute values, never `+=`;
- GL calls only inside drawing callbacks;
- lambdas capture by value or `shared_ptr`;
- generated files go through `c.assets`;
- deterministic randomness only.

In addition:

- Do not copy-paste a chapter ten times with new strings. Factor the repeated grammar
  and keep the content in data.
- Do not duplicate timing constants between picture and score.
- Do not leave dead helper functions, unused font loaders, or placeholder beats.
- Keep the film-specific content in the example; extract genuinely reusable helpers
  only after they are proven.

---

## 9. Required artifacts

Before calling a piece done, these must exist:

- a declared **story mode** (explainer / narrative / promo / utility) and the mode
  contract from the brief;

- `docs/<slug>-brief.md` (from `../templates/BRIEF.md`);
- `docs/<slug>-script.md` or a script section in the brief;
- `docs/<slug>-sources.md` or a source section in the brief;
- a still review (frame numbers + what you saw);
- a draft render;
- a final render plus `ffprobe`, full decode, and loudness log;
- an SRT and a chosen thumbnail for any publishable cut;
- a filled `../templates/QC-REVIEW.md`, including the declared story mode and the
  mode-specific packaging checklist.

For a quick internal demo, the brief may be shorter, but never skip the still review,
the authoredness and craft scores, or the "illustrative" labels.

---

## 10. Pre-flight checklist

All answers must be **yes** before code:

- [ ] I can state the conceit in one sentence without naming the topic.
- [ ] I know the audience, runtime, platform and whether there is a voice.
- [ ] Every chapter in the table has a proof object.
- [ ] Every scene has a reveal map: word/onset anchor, 0.20-0.40 s lead, hold, and one
      entrance at a time.
- [ ] Every numeric or relational claim has a metaphor contract (mapping + state change),
      and no label is duplicated between the diagram and its key.
- [ ] I have a script, or a declared text-only cue map.
- [ ] Every number/claim has a source or an on-screen "illustrative" label.
- [ ] There is one source of truth for scene starts, voice lengths and score cues.
- [ ] The palette, type scale, margins, motion grammar and motif are written down.
- [ ] The asset plan says what is generated, what is committed, and what happens if it
      is missing.
- [ ] I have chosen the still frames and the thumbnail candidate.
- [ ] The acceptance checks are concrete and observable.
- [ ] A reviewer is named for the brief and the final QC.

If any answer is no, do not write composition code. Fix the brief first.

---

## 11. Anti-patterns: the half-assed tells

Reject a publishable draft that has any of these. For a pure utility overlay (lower
third, title card), apply the ones that make sense and say which ones are `n/a`.

- the same layout repeated for every chapter, with only the text changed;
- a scene that shows its full layout at `t = 0` and calls the entrance animation a
  reveal;
- visuals that arrive after the narration names them, or in an order that contradicts
  the sentence;
- syncing to line starts while the semantic word lands seconds later;
- a diagram and its key repeating the same term (e.g. LINEAR inside the Venn and again
  in the right panel);
- an animated metric that does not map to a claim (motion as decoration);
- captions doing the explaining while the image is decorative;
- numbers living only in prose, never in a chart, map, counter or geometry;
- a fixed clock with narration or copy pasted into it;
- one generic music bed with no cues and no ducking;
- a feature checklist ("show 3D here, show a chart here") instead of a story;
- a "MADE WITH RAYMOTION" chapter that exists only to demo the engine;
- no source ledger, or sources that stop at "the paper";
- no stills, no draft, no encoded-frame check;
- a full-film pass or contact sheet offered in place of scene-by-scene approval;
- rendering from a stale binary after the source changed;
- assets assumed to exist on the author's machine;
- no thumbnail candidate;
- "it renders" treated as acceptance.

These are not style preferences. Each one is a failure to make the subject visible.

---

## 12. Benchmarks

Read the good references before starting:

- `examples/showcase/austria_time_travel.cpp` + `docs/austria-time-travel-script.md` —
  a time-machine conceit, one artifact per chapter, timing derived from the voice, and a
  source ledger.

Read the cautionary references to see the difference:

- `examples/kgnesy/main.cpp` — a single abstract diagram, claims in captions, no voice,
  no public docs, no verification.
- `examples/showcase/austria_journey.cpp` — 11 station cards on one camera path, with a
  feature-coverage document that says the film is not the complete showcase.

The point is not length. The length follows from the argument. The point is that the
images, timing, sound and sources are specific to the subject.

---

## 13. Definition of done

A piece is done when all of the following are true:

1. The one-sentence conceit is visible in the first five seconds.
2. A muted viewer can reconstruct the argument from the images.
3. Every chapter has a proof object and a distinct visual idea.
4. The narration and score share a cue map; retiming one chapter is safe.
5. Every scene has a reveal map, starts sparse, and reveals one unit per spoken anchor
   with a 0.20-0.40 s lead and a readable hold. No scene reaches final density at `t = 0`.
6. Every number is sourced or labelled illustrative.
7. Every scene was reviewed and approved individually; the final render was rebuilt after
   the last source edit.
8. A reviewer has looked at stills, a muted draft, a sound draft, and encoded frames.
9. The story mode is declared. Authoredness is **22+** and Craft is **13+** with no
   critical fail; a showcase or public cut reaches Authoredness **30+** and Craft **16+**,
   with the mode's packaging complete.
10. The brief, script, sources, SRT and thumbnail are in the repository.

If any of these is false, the piece is a draft, not a release.
