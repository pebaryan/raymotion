# Quality control: from brief-complete to improvised

This is the review framework for raymotion work. It exists to answer one question:

> Is this an **authored film**, or a **brief-complete demo**?

It is not a style score for effort. It is a set of gates, a rubric, and a remediation
loop. The same framework is used before code, at stills, at draft, and at the final
encoded file. It ends with a score, a ship/revise/rewrite decision, and evidence.

Use `../templates/QC-REVIEW.md` for the written review.

---

## 1. The two failure modes

### Brief-complete (the half-assed mode)

The work answers the prompt but not the subject.

- organized by required beats or engine features, not by an argument;
- one template repeated, or one abstract diagram asked to carry everything;
- meaning lives in captions; the images are decoration or category icons;
- a fixed clock that the script is pasted into;
- a generic bed or loop instead of a score;
- claims and numbers asserted, not visualized or sourced;
- assets assumed to exist on the author's machine;
- no stills, no draft review, no encoded-frame or loudness check;
- no thumbnail candidate and no memorable image.

It usually renders. That is the problem: "it renders" is treated as done.

### Authored / improvised (the mode this framework drives toward)

The work is a specific performance of an idea.

- one conceit that can be stated without naming the topic;
- a written script with a point of view, or a deliberately text-only script with a cue
  map;
- every chapter has a proof object: a chart, map, artifact, mechanism, count, or
  physical metaphor;
- timing derived from the narration/audio, or from one cue table, not duplicated
  constants;
- a composed soundtrack, or a justified silence, with cues on the picture;
- a documented visual system and a recurring motif;
- data in arrays, sources in a ledger, illustrative values labelled;
- assets generated or committed with a fallback; `list` works on a clean machine;
- stills, draft, final render, encoded-frame check, loudness/peak check, and a
  determinism check;
- a chosen thumbnail and at least one image that survives muting.

It is "improvised" in the jazz sense: a specific performance built on a motif, not a
template with the names changed. It is not an absence of planning; the benchmark films
in this repository are the most planned work in it. The improvisation is in the creative
choices, not in skipping the brief.

**The deepest test:** if you can reskin the piece to another topic by editing text and
pictures, it is brief-complete. If the form is inseparable from the subject, it is
authored.

---

## 1b. Declare the story mode

Before scoring, choose one mode. The rubric is the same; the proof objects, pacing and
house grammar are not.

| Mode | Goal | Proof object | Required evidence | Text | House grammar | Packaging |
| --- | --- | --- | --- | --- | --- | --- |
| **Explainer** | make a concept or data legible | chart, map, diagram, worked example | source ledger | captions/VO at one reading rate | data/lecture grammar | title/sources/credits, thumbnail, SRT; logo optional |
| **Narrative** | make a story land | shot, beat, continuity, performance | script/shot list/storyboard; sources for factual or cultural claims | dialogue, subtitles, title cards | genre grammar | title/credits, thumbnail, SRT; logo optional |
| **Promo** | make an idea/product desirable and credible | demo, spec, cost, benefit, CTA | claim ledger; brand kit | tagline/VO/CTA | brand grammar | logo/social/CTA, thumbnail, SRT |
| **Utility** | be legible in context | type, layout, integration | spec | hierarchy | brand/utility grammar | per spec |

Rules:

- Do not fail a narrative for lacking a chart, a source ledger for fiction, or a social
  card. Judge its staging, continuity and genre grammar instead.
- Do not fail an explainer for lacking a dramatic arc. Judge its diagram, source and
  reading rate.
- Do not fail a promo for lacking a chart when its proof is a demo, spec, price or CTA.
- Criteria 2 (Script/voice), 3 (Evidence) and 8 (Data & provenance) are read through
  this table: a narrative's proof is the shot/beat; a promo's is the demo/spec/claim; an
  explainer's is the chart/map/source.
- The mode is part of the brief. If the film changes mode, update the brief and re-score.

---

## 2. Gates: stop or continue

Each gate is pass / conditional / fail. A fail stops the work. Record the evidence in
`QC-REVIEW.md`.

### Gate 0 — Brief (before code)

- The one-sentence conceit exists and is subject-specific.
- The story mode is declared and the mode contract is filled.
- Audience, runtime, platform, voice/text-only decision are written down.
- Every chapter has the mode's proof object.
- The mode's required evidence exists or is planned: source ledger (explainer),
  script/shot list (narrative), claim ledger/brand kit (promo), spec (utility).
- The visual system and timing source exist (cue map, scene plan or shot list).
- The asset plan and clean-machine behaviour are described.
- The packaging plan is mode-appropriate.

**Fail:** start coding anyway; "we'll figure out the story while building."

### Gate 1 — Script and cue map (before score/voice work)

- The text for the mode is complete: script/VO (explainer/promo), screenplay/dialogue/
  title cards or shot list (narrative), spec/hierarchy (utility).
- Terms are defined once; numbers carry units where the mode has them; banned words are
  absent from explanatory/promo copy.
- Voice lengths, a text-only cue map or a shot list exist; picture and score cues read the
  same table.
- Scene lengths plus transition overlaps equal `c.duration`.

**Fail:** a fixed timeline with narration pasted into it; cue times copied into three
unrelated arrays.

### Gate 2 — Stills (after the first build)

- At least one still per chapter start, middle, and end.
- One still in the middle of every transition.
- Text inside the safe area; no overlap with HUD, captions, charts or letterbox.
- Fonts are real (no `?` for CJK); sizes are legible at 1:1 and at feed size.
- Data labels match the arrays and sources; units are present.
- The proof object is visible at the frame the script says it should be.

**Fail:** "I'll check it in motion"; no images; a contact sheet with no verdicts.

### Gate 2b - Staged reveal and narration sync (before the draft)

This gate exists because a scene can be legible and still fail as a reveal. Check one
scene at a time against its measured word onsets.

- The scene starts sparse: at `t = 0` only the first unit is present; the final density
  is earned later.
- Each unit has a spoken anchor (word or phrase onset, not only the caption line start).
- Each visual begins **0.20-0.40 s before** its anchor and resolves on it.
- The previous unit holds for at least ~0.8 s before the next unrelated entrance.
- The semantic order matches the sentence: if it says amino / vitamins / lipids, the
  plates appear in that order; if it says "both", the intersection appears, not the
  whole diagram.
- Each visual metaphor maps the claim exactly (left set / right set / intersection;
  columns then rows; before copy / move / morph; interval band; lock after its box).
- No term is repeated in both a diagram and its key.
- One entrance at a time; no competing motion intervals.
- A scene-only clip or 2-3 stills were reviewed and approved; no full-film or contact
  sheet was used as a substitute for scene approval.
- The executable is newer than the last source edit; the final render was rebuilt after
  the last approved change.

**Fail:** a full layout at `t = 0`, visuals after the words, wrong semantic order,
duplicate labels, a stale binary, or a full-film pass offered in place of scene review.

### Gate 3 — Draft (motion and sound)

Watch the draft twice:

1. **Muted.** Can you follow the argument from the images alone? Where does it become a
   slideshow? Where does a caption rescue a bad image?
2. **With sound.** Do cues land on the picture? Is the bed ducked? Is any voice clipped
   or late? Does the music fight the narration?

Check transitions, pacing, reading pauses, and the ending. Log timecodes for every
problem.

**Fail:** only watching the draft with sound; accepting "it looks fine"; no timecoded
notes.

### Gate 4 — Final technical QA

- `ffprobe` confirms the expected width, height, frame rate, duration and frame count.
- A full decode completes without errors.
- Integrated loudness and true peak are measured and recorded.
- Extract frames from the **encoded MP4**, not only the pre-encode PNGs; check banding,
  compression, and text legibility.
- A still at frame N matches frame N of the render (determinism).
- Audio is in sync at chapter starts and at the final line.
- The executable is newer than the last source edit, and the final render came from that
  executable. Record both mtimes or a build hash.
- The reviewed scene edit is present in the final file; `out/` is not an older render.
- The SRT and thumbnail exist for a publishable cut.

Suggested commands:

```sh
ffprobe -v error -select_streams v:0 -count_frames \
  -show_entries stream=width,height,r_frame_rate,nb_read_frames,duration \
  -of default=nw=1 out/<slug>.mp4
ffmpeg -v error -i out/<slug>.mp4 -f null -
ffmpeg -hide_banner -i out/<slug>.mp4 -af ebur128=peak=true -f null -
ffmpeg -ss <seconds> -i out/<slug>.mp4 -frames:v 1 out/qc/encoded-<frame>.png
```

**Fail:** no frame count, no full decode, no loudness measurement, no encoded-frame
check.

### Gate 5 — Authored test (the "make it improvised" gate)

- **Mute test:** the argument survives without sound.
- **Thumbnail test:** one frame is readable at 320×180 and states the conceit.
- **Reskin test:** changing the topic would require a redesign, not a find-and-replace.
- **Retiming test:** lengthen one chapter's narration; the picture and score move with
  it because they share the cue map.
- **Clean-machine test:** `list` works from another directory; missing assets fail with
  a clear, documented behaviour.
- **One-idea-per-screen test:** no chapter needs a wall of caption text to explain its
  image.
- **Staging test:** each scene begins sparse and reveals one unit at a time; no scene
  reaches final density at `t = 0`.
- **Cue-sync test:** sample three semantic words; the matching visual is present at the
  word, having started 0.20-0.40 s earlier. No visual arrives after its word.
- **No-duplicate-label test:** no term appears in both a diagram and its key.
- **Scene-review test:** every scene has a scene-only clip or stills with an explicit
  approval note; a full contact sheet was not used as a substitute.
- **Fresh-build test:** the executable is newer than the source and the final render was
  rebuilt after the last approved change.
- **Distinct-chapter test:** every chapter has a visual idea; a persistent HUD/motif is
  what unifies them.

**Fail:** any of the above. A piece that fails Gate 5 is not "improvised" even if it is
polished and long.

### Gate 6 — Craft & coherence

- **Scenario coherence:** model, VRAM, power, cost, usage and names are consistent; the
  chart is recomputed when a spec changes.
- **Information design:** the right chart/map at the right scale; labels, units and source
  notes are legible; no collisions.
- **Staging & sync:** every unit has a spoken anchor, a 0.20-0.40 s lead and a hold; the
  semantic order matches the sentence; no duplicate diagram/key labels; the metaphor's
  animation shows the relation, not decoration.
- **Text performance & pacing:** declared constant reading rate; spoken words light up in
  the house caption style; no glow/box gimmick; the line holds after the last word.
- **House style & mode grammar:** the film uses the grammar of its declared mode
  (explainer/data, narrative/genre, promo/brand, utility). A logo/social card is a
  packaging item, not a universal craft requirement.
- **Spec-to-screen fidelity:** the artifact matches the brief/claims at the detail level
  (e.g. "4-8-8-8-8-8-3", not a different network; Qwen3.8 27B, not a different model).
- **Revision quality:** every reviewed change landed with its consequences updated, and
  nothing regressed.

**Fail:** any craft criterion = 0, or Craft < 13. A showcase cut needs Craft ≥ 16.

### Gate 7 - Release packaging (mode-dependent)

- **Explainer:** title/source/credits, thumbnail, SRT; logo optional.
- **Narrative:** title/credits, thumbnail, SRT; logo optional.
- **Promo:** logo/wordmark, social/CTA, thumbnail, SRT.
- **Utility:** per its spec.

**Fail:** a required artifact for the declared mode is missing. Packaging is not a story
score; it is the last gate before release.

---

## 3. The rubric

Score each criterion 0–3. Total is 36. Use the anchors; do not award 3 for effort.

| # | Criterion | 0 | 1 | 2 | 3 |
| --- | --- | --- | --- | --- | --- |
| 1 | **Conceit** | Topic only | Generic metaphor applied uniformly | Specific metaphor shapes several chapters | One-sentence conceit determines the opening, order and ending; visible in the first 5 s |
| 2 | **Script / voice** | None; captions pasted on | Generic captions or TTS lines; no point of view | A real script, but timing/voice not integrated | Written with a point of view, spoken or deliberately text-only, one idea per line, cue-mapped |
| 3 | **Evidence** | Decorative images; claims in text | Some data visuals; key claims asserted | Most chapters visualize their claim; a few labels | Every chapter has a proof object; a muted viewer can reconstruct the argument; illustrative values labelled |
| 4 | **Timing** | Fixed clock; copy/voice pasted in | Fixed but internally consistent; scene starts hardcoded | Scene starts from `node->start`; voice at scene starts; a cue table exists | Scene lengths and cues derived from narration/audio; one source of truth; retiming is safe |
| 5 | **Sound** | None, or one generic bed | A loop/bed, maybe SFX; no ducking | Score + voice + duck + SFX; some cues approximate | Composed motifs, voice, phrase cues, ducking, SFX on picture beats, measured loudness/peaks |
| 6 | **Visual system** | Defaults; one font/colour | Consistent colour, no documented system | Palette/type/margins/motion defined; some meaning | Documented system: colour meaning, type scale, layout, motion grammar, motif; stills prove it |
| 7 | **Chapter differentiation** | Same template every chapter | Only colour/text changes | Several custom scenes; some filler | Every chapter has a distinct visual idea; persistent frame unifies; no filler |
| 8 | **Data & provenance** | Numbers unsourced | Some sources, inconsistent | Source ledger for most claims; exact vs illustrative | Complete ledger, code/data linkage, caveats, no false precision |
| 9 | **Production pipeline** | Manual assets; fails on a clean machine | Committed assets; no generation/fallback | `c.assets`/lazy/fallback for some; `list` on a clean machine works | Script→voice→SRT/score→assets→thumbnail pipeline; clean-machine render works or fails clearly |
| 10 | **Verification** | "It renders" | A few stills; no draft/final checks | Stills + draft + final render; no encoded-frame/audio checks | Stills, draft, final ffprobe/decode, encoded-frame check, loudness/peaks, sync/transcript, determinism |
| 11 | **Memorability / thumbnail** | No image worth a thumbnail | One decent frame | A clear motif and a chosen thumbnail | Multiple iconic images; thumbnail chosen and tested at feed size; conceit visible in it |
| 12 | **Specificity (reskin test)** | Could be reskinned by editing text | A few subject-specific assets, mostly template | Form partly shaped by subject; some chapters swappable | Form inseparable from the subject; reskinning requires a redesign |

### Score bands

| Total | Verdict | Action |
| --- | --- | --- |
| 0–12 | **Half-assed** | Do not ship. Rewrite from the brief. |
| 13–21 | **Brief-complete** | Internal demo only, labelled as such. Apply the remediation passes. |
| 22–29 | **Authored** | Ship after fixing the two lowest criteria. |
| 30–36 | **Improvised** | Showcase-grade. Use as a reference implementation. |

This table is the **authoredness** axis. Craft is scored separately below; a piece can be
authored and still fail the craft/coherence gate.

**Improvised** requires all of: Authoredness ≥ 30, Craft ≥ 16, no critical fail, and
Gates 0–7 passed.

### Craft & Coherence (second axis)

The 12 criteria above answer "is this authored?" They saturate at 3: once a piece has a
conceit, a script, evidence, a system and a clean render, further work on coherence,
legibility and house style does not move the 36-point score. Score these six 0–3 as well,
and report the craft score alongside the authoredness score.

| # | Criterion | 0 | 1 | 2 | 3 |
| --- | --- | --- | --- | --- | --- |
| 13 | **Scenario coherence** | Specs/claims contradict each other or are physically impossible | One key relation is stale or unexplained | Model, VRAM, power, cost, usage and names mostly fit; one minor gap | Model size fits VRAM; power feeds the cost; the chart is recomputed; names match the source ledger |
| 14 | **Information design** | No chart/map/proof object, or labels unreadable | A proof object exists but the wrong type/scale, or labels collide | Right chart/map, units and scale; minor annotation issues | Right encoding at the right scale; labels, units and source notes are legible at 1:1 and feed size; the chart/map carries the argument |
| 15 | **Text performance & pacing** | Unreadable or distracting text; no consistency | Readable but stretched to the scene budget or over-animated | Constant reading rate; legible; one distracting effect | Declared constant rate; house caption style; no glow/box gimmick; line breaks and emphasis serve reading; holds after the last word; voice-ready |
| 16 | **House style & mode grammar** | Ignores its declared mode's grammar | Generic or borrowed grammar | Consistent mode grammar; one weak element | Mode-specific grammar is sustained and the film feels like itself; the brand kit is a packaging item, not a universal craft requirement |
| 17 | **Spec-to-screen fidelity** | Artifact contradicts the brief/claims | Several mismatches | Mostly matches; one detail slipped | Every stated detail is visible and matches the brief/source ledger; a design-QA pass verifies the specifics, not just the render |
| 18 | **Revision quality** | Feedback ignored or regressions | One requested change landed, others missed | Most changes landed with a small regression | Every requested change landed, its consequences were updated, nothing regressed, and the change is visible in the artifact |

Mode anchors for the craft axis:

- **Explainer:** 13 data/model/hardware; 14 chart/map/worked example; 15 captions/VO at
  one rate; 16 data/lecture grammar; 17 brief/data.
- **Narrative:** 13 world/continuity; 14 staging/shot grammar/continuity; 15
  dialogue/subtitles/title cards; 16 genre grammar; 17 shot list/storyboard.
- **Promo:** 13 product claims/price; 14 benefit/proof hierarchy; 15 tagline/VO/CTA; 16
  brand grammar; 17 claim/proof/CTA.
- **Utility:** 13 spec/layout; 14 hierarchy; 15 legibility; 16 brand/utility grammar; 17
  spec.

**Craft bands:** 0–6 surface polish; 7–12 competent but generic; 13–15 crafted;
16–18 showcase craft.

**Two-axis release rule:** a release needs **Authoredness ≥ 22 and Craft ≥ 13**; a
showcase or public cut needs **Authoredness ≥ 30 and Craft ≥ 16**. The 36-point score is
the authoredness axis; report it alongside the craft axis, not instead of it.



### Critical fails

Any one of these caps the verdict at **Brief-complete (21)** regardless of the total:

- criterion 1 = 0 (no conceit);
- criterion 2 = 0 (no script, no text-only cue map, and no declared text-only
  rationale);
- criterion 3 ≤ 1 (muted test fails);
- criterion 7 = 0 (one template repeated);
- criterion 8 = 0 (unsourced central claims);
- criterion 9 = 0 (assets fail on a clean machine for a committed composition);
- criterion 10 = 0 (no stills/draft/final verification);
- a key scene fails Gate 2b: full layout at `t = 0`, a visual after its spoken word,
  wrong semantic order, duplicate diagram/key labels, or a stale binary render;
- a central claim contradicts its source, or an illustrative value is presented as
  exact.

A sparse or absent score is not automatically critical if the brief declares silence and
the visuals carry the argument. But a generic bed with no cues or design still caps
criterion 5 at 1.

---

## 4. Review protocol

### Who reviews

- **Author self-review** at every gate.
- **Second reviewer** (human or another agent) for Gate 0 (brief), Gate 2 (stills),
  Gate 2b (staged reveal and sync), Gate 5 (authored test) and Gate 6 (craft &
  coherence). Gate 7 packaging can be self-checked against the mode checklist. The
  reviewer looks at artifacts, not at the author's description of the artifacts.

### What the reviewer receives

- the filled brief;
- the script, reveal map and source ledger;
- the scene review ledger: scene, clip/still path, reveal notes, approval, re-render;
- a contact sheet for coverage, not as the only still evidence;
- the draft render;
- the final render, ffprobe/decode/loudness log, encoded stills;
- the SRT and thumbnail candidate;
- the filled `QC-REVIEW.md`.

### How to review

1. Read the one-sentence conceit. Watch the piece muted. Can you state the argument?
2. Watch with sound. Mark every cue that lands late, every duck that is missing, every
   line that is clipped.
3. Open the scene review ledger. For each scene, watch its clip or 2-3 stills and check
   the reveal map: sparse start, one unit at a time, word-anchored entrances with lead,
   semantic order, no duplicate labels. Approve or send notes one scene at a time.
4. Look at the stills at feed size and at 1:1. Look for overlap, orphaned words, fake
   glyphs, illegible labels, and charts without units.
5. Read the script and the source ledger. Spot-check three numbers against the sources.
6. Run the reskin test, the retiming test and the clean-machine test.
7. Confirm the declared story mode. Score Craft & Coherence against that mode: scenario
   coherence, the right proof object (chart/map for an explainer; staging/shot grammar for
   a narrative; demo/benefit for a promo), text pacing, mode grammar, spec-to-screen and
   revision.
8. Check Gate 7 packaging for the mode (title/credits, thumbnail, SRT; logo/social/CTA
   for a promo).
9. Fill both scorecards. Name the two lowest criteria and the fix for each.
10. Decide: **ship**, **revise**, or **rewrite**. Never "looks fine".

### Reviewer rules

- Do not accept "it renders" as evidence.
- Do not accept a plan document as evidence that the film does what the plan says.
- Do not accept a still if you have not opened it.
- Do not accept a claim because it is in the voiceover.
- Do not accept a score because the music is pleasant.
- If the reviewer cannot state the conceit after one viewing, the film fails Gate 5.
- Do not accept a full-film pass or contact sheet as approval for an unreviewed scene.
- Do not accept line-start sync when the semantic word lands later; Gate 2b is a fail.
- Do not accept duplicated diagram/key labels or an all-at-once scene as "polish".
- Do not accept a still/render that came from a binary older than the source.
- Do not accept a spec mismatch (network counts, model name, GPU, map route, a number)
  as "close enough"; that is a Gate 6 failure.

---

## 5. Remediation: make it improvised

Do not polish a brief-complete piece. Rewrite it in three passes. Each pass ends with a
re-score.

### Pass A — Conceit

1. Write the one-sentence conceit. If it names only the topic, it is not a conceit.
2. Choose one **proof object per chapter** from the evidence table in the authoring
   standard.
3. Choose the opening image and the thumbnail. If they are the same image, good.
4. Cut every chapter that exists only to demo a feature. If the feature matters, find
   the artifact that needs it.
5. Replace the repeated template with a distinct visual idea per chapter; keep one
   persistent frame (HUD, kicker, palette, progress line) to unify.

Result: criteria 1, 7, 11 and 12 move.

### Pass B — Evidence

1. Move every number out of the copy and into the mode's proof object: a chart, map,
   geometry, counter or artifact for an explainer; a staged beat or shot for a narrative;
   a demo, spec, price or CTA for a promo.
2. Build data arrays with source comments; export a source ledger; label illustrative
   values on screen.
3. Delete decorative vignettes that do not prove a claim.
4. Check the muted test after each chapter. If a chapter fails, redesign the image
   before adding words.
5. Add exact-vs-illustrative labels and caveats.

Result: criteria 3, 6 and 8 move.

### Pass C — Performance

1. Write the script as spoken lines. Read it aloud. Cut anything you would not say.
2. Record/generate one voice file per chapter; measure the WAV lengths.
3. Build the cue map from the script; derive scene lengths from the voice.
4. Compose or arrange the score; place SFX on picture beats; duck under speech.
5. Point picture, score and captions at the same cue table. Remove duplicated timing
   constants.
6. Run Gates 2–5 and record the evidence.

Result: criteria 2, 4, 5, 9 and 10 move.

### Per-criterion fixes

| If the score is low on… | Do this |
| --- | --- |
| 1 Conceit | Rewrite the one-sentence; choose one metaphor and delete the rest; design the first 5 s and the thumbnail first |
| 2 Script | Write 8–12 spoken lines with a point of view; ban the jargon until it is defined; record and measure |
| 3 Evidence | Replace each caption claim with a proof object; if it cannot be drawn, cut the beat |
| 4 Timing | Replace fixed offsets with `node->start`, WAV lengths and a single cue map; assert the duration sum |
| 5 Sound | Replace the bed with a motif score; add ducking and SFX; measure loudness and true peak |
| 6 Visual system | Write the palette/type/margins/motion/motif table; apply it; check 12 stills |
| 7 Differentiation | Give each chapter a new visual idea; keep the HUD/palette as the unifying frame |
| 8 Provenance | Build the claim ledger; spot-check three numbers; label illustrative values |
| 9 Pipeline | Move generation to `c.assets`; lazy-load GL resources; add a fallback; run the clean-machine test |
| 10 Verification | Stills, draft, final ffprobe/decode, encoded frames, loudness, sync, determinism |
| 11 Memorability | Choose one iconic frame; test it at 320×180; make it the thumbnail |
| 12 Specificity | Ask what would survive a topic swap; redesign what survives; keep what is inseparable |
| 13 Scenario coherence | Reconcile the mode's coherence: model/VRAM/power/cost for explainer/promo; world/continuity for narrative; spec/layout for utility |
| 14 Information design | Choose the mode's proof object: chart/map for explainer; staging/shot grammar for narrative; benefit/proof for promo; hierarchy for utility |
| 15 Text performance | Set one reading rate; use the house caption style; remove glow/boxes; hold after the last word |
| 16 House style | Use the declared mode's grammar: explainer data/lecture, narrative genre, promo brand kit, utility brand/utility |
| 17 Spec-to-screen | Audit the mode's spec line by line (data/brief, shot list, claim/CTA, layout) and fix every mismatch |
| 18 Revision | Checklist every review item; update its consequences; re-render and re-check |
| Gate 2b Staging/sync | Build the reveal map from word onsets; stage one unit at a time; lead 0.20-0.40 s; remove duplicate labels; re-render the approved scene clip |

### Pass D - Craft & coherence

1. Audit the numbers: model, VRAM, power, cost, usage and names must agree; recompute the
   chart when a spec changes.
2. Choose the right chart/map at the right scale; fix labels, units and source notes;
   check them at 1:1 and feed size.
3. Set one reading rate for captions; use the house caption style; remove per-word glow,
   selection boxes and other attention magnets; hold after the last word.
4. Use the declared mode's grammar. A promo uses the brand kit and social card; an
   explainer uses its title/source grammar; a narrative uses its title/credits.
5. Compare the artifact to the brief line by line: network counts, model name, GPU, map,
   numbers, labels.
6. Respond to every review item and update its consequences (cost chart, timing, labels,
   copy).

Result: criteria 13-18 move.

### Stop rules

- If the piece still fails a critical fail after Pass C, rewrite from the brief.
- Do not trade a critical fail for a higher total. A polished template is still a
  template.
- Do not add a voiceover to rescue a film whose images say nothing. Fix the images
  first; the voice is not a caption track.
- A high authoredness score cannot compensate for a low craft score. The two axes
  gate the release together.
- If a scene is approved only from a full-film or contact-sheet pass, it is not
  approved. Re-open the scene and approve it on its own.
- If the binary is older than the source, re-build before making any judgment about the
  frame. A stale render is not evidence.

---

## 6. Quick scans

### Half-assed scan (any three = rewrite)

- The same card/layout appears in every chapter.
- A scene shows its full layout at `t = 0`; the "animation" just fades the finished board.
- A visual arrives after the narration names it, or the reveal order contradicts the
  sentence.
- The captions explain the chart.
- The numbers exist only in prose.
- The scene lengths are fixed and the voice was pasted in.
- The score is one loop with no cues.
- The feature list is the story.
- There is a "made with the engine" chapter but no subject-specific artifact.
- No source ledger.
- No stills or no encoded-frame check.
- The assets only exist on the author's machine.
- There is no thumbnail candidate.
- "It renders" is the acceptance criterion.

### Improvised scan (all should be true)

- The conceit is stated in one sentence and is visible in the first 5 s.
- Every chapter has a proof object.
- A muted viewer can reconstruct the argument.
- The script has a point of view; the voice is measured; the cue map is shared.
- Every scene has a word-anchored reveal map with a 0.20-0.40 s lead and a hold.
- The score has motifs and cues; the bed ducks.
- The visual system is documented and applied.
- Each chapter has a distinct image; the HUD/palette unifies.
- Every scene was approved individually; no diagram duplicates its key labels.
- Every number is sourced or labelled illustrative.
- The pipeline runs on a clean machine or fails clearly.
- Stills, draft, final decode, loudness, encoded frames and determinism were checked.
- The thumbnail is chosen and readable at feed size.
- A topic swap would require a redesign.

### Craft scan (any two = revise)

- The model, VRAM, power, cost and usage figures do not agree.
- A chart/map is the wrong type or scale, or its labels collide.
- A reveal is line-start synced, in the wrong semantic order, or all-at-once.
- A diagram repeats a term already in its key.
- Text is stretched to the scene budget or over-animated; a glow or box follows every word.
- The film ignores the house/mode grammar (a promo without the brand kit, a narrative
  without a genre grammar).
- The artifact does not match the stated network, spec or number (a design-QA miss).
- Feedback landed in one place but its consequences (cost chart, timing, labels) did not.
- A promo/intro/outro has no brand kit, social card or handle where the mode requires it.
- The revision is invisible: the same score with no craft delta.

---

## 7. Reference benchmarks

Use these when calibrating scores:

- **Authored:** The Token Tax, an external RotorSubword explainer recorded in
  `worked-examples.md` §1-2; it documents a visual system, a narration script, a score,
  an accuracy ledger and a verification log, and has a vertical cut rebuilt for 9:16.
- **Authored:** `examples/showcase/austria_time_travel.cpp` with
  `docs/austria-time-travel-script.md`; it derives timing from the voice, gives each
  chapter an artifact, and keeps a source ledger.
- **Authored reels:** `examples/showcase/a1_sein_six_seats.cpp`, `asp_v2.cpp` and
  `ht_feed_v2.cpp`, each with a filled QC review in `docs/`; judge reels against
  `reels.md` as well.
- **Brief-complete:** `examples/kgnesy/main.cpp` — one abstract diagram, claims in
  captions, no voice, no public docs, no verification.
- **Brief-complete:** `examples/showcase/austria_journey.cpp` — a station template
  repeated on a camera path, with a feature-coverage document instead of a script.
- **Narrative reference:** `examples/halloween/main3d.cpp` — a genre cut with a consistent
  title/act grammar; its craft gaps are continuity/timing, shot-list fidelity and
  packaging, not "no chart".
- **Promo reference:** `examples/showcase/own_compute_v2.cpp` + `docs/own-compute-v2-qc.md`
  — the mode-aware craft and packaging example.

The benchmark is not "make it 1,900 lines." It is "make the form specific to the
subject." A 20-second title can be authored; a 5-minute explainer can be half-assed.

### Calibration examples (approximate, for training the reviewer)

These are rough bands for the 12-criterion authoredness axis, not verdicts. The Craft &
Coherence axis is scored separately and was introduced after the 2026-10-26 v2 feedback,
so these older examples are not re-scored here.

| Work | Estimated band | Why |
| --- | --- | --- |
| `kgnesy` | 12–16 / Brief-complete | Strong 3D craft and a clear colour system, but one abstract diagram, no voice, claims in captions, no public script/sources/verification. |
| `austria-journey` | 14–18 / Brief-complete | Good camera path and real map/chart data, but 11 repeated station cards, feature-coverage framing, fixed narration offsets, no encoded-frame/loudness log. |
| `rotor-subword` (The Token Tax, external) | 29–33 / Authored–Improvised | Ten distinct scenes, data as geometry, typeset equations, VO + score + duck + SFX, accuracy ledger, verification log. Deductions for hardcoded VO ducking and assets not generated in `c.assets`. |
| `asp-feed` v1 → v2 | 17 → 33 | Same topic and length. v1: a generic card per beat, no proof or source. v2: one map that is the problem and the answer, two answer sets, a constraint that removes one, a source line, a composed score. |
| `austria-time-travel` | 30–34 / Improvised | One artifact per chapter, timing from measured voice, phrase cues, custom score and ducking, source ledger, thumbnail. Deductions for the monolithic file and duplicated timing constants. |

A reviewer who scores a 510-line station template above a 1,940-line artifact-driven film
is not measuring authoredness; they are measuring length or polish. Use the anchors, not
the line count.

---

## 8. Final decision

The review ends with one of three decisions:

- **Ship:** Authoredness ≥ 22, Craft ≥ 13, no critical fail, all gates 0–7 passed.
  Record both scores and the evidence. A public or showcase cut needs Authoredness ≥ 30
  and Craft ≥ 16 (Improvised); 22–29 authoredness is an internal cut, not a showcase
  piece.
- **Revise:** Authoredness < 22, Craft < 13, or a gate failed, but the conceit and
  script are worth keeping. Apply Pass A/B/C/D to the two lowest criteria, then re-review.
- **Rewrite:** no conceit, no script, or the form is a template. Start from the brief.

A released piece must have:

1. a filled brief with a declared story mode;
2. the mode's required evidence (script/sources for an explainer, shot list for a narrative, claim ledger for a promo);
3. a filled QC review with authoredness and craft scores and evidence;
4. a final render checked from encoded frames;
5. the mode's packaging (title/credits, thumbnail, SRT; logo/social/CTA for a promo).

Anything less is a draft.
