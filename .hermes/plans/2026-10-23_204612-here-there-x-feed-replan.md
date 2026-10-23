# Here-and-There for the X feed: editorial and production plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Make one short, visually understandable introduction that earns interest in Here-and-There, instead of trying to teach the entire formalism in a feed video.

**Architecture:** Use one persistent fire/smoke diagram and one fixed candidate throughout a single micro-story. Animate the reasoning action rather than laying out lecture slides. Keep the existing long explainers as reference versions; implement a separate social-feed composition only after approval.

**Tech stack:** Existing C++ raymotion, raylib, installed Segoe UI fonts; ffmpeg only if an export is later requested. First review is a live native preview, with no PNG/video exports.

---

## 1. Diagnosis: what needs to change

The previous redesign changed the font and vocabulary but retained the lecture structure:

- Nine equally weighted chapters require sustained attention before the payoff.
- The viewer has to read panels and explanatory captions at the same time.
- The audience learns a vocabulary before seeing why they should care.
- Most of the explanation lives in text, not in a visible event.
- The horizontal 720p layout makes the lesson small when viewed inside a phone feed.
- Replacing symbols with long sentences has not reduced the amount to understand.

The remedy is NOT another font change, more transitions, or a faster version of the same nine scenes. It is less scope, a stronger opening, and a single visible test.

## 2. Recommended concept

**Selected hook (preserve verbatim):** If nothing contradicts a claim, should you believe it?

**Working title:** What makes a claim worth believing?

**One takeaway:** Nothing saying a claim is wrong is not the same as having a reason to believe it.

**Viewer promise:** Learn one simple question to ask before accepting a claim: what actually backs it up?

**Language lock:** No complicated terminology in screen copy or optional spoken script. Do not say model, interpretation, equilibrium, minimisation, entailment, H/T, or temporal operators. Keep even the name Here-and-There in the accompanying source note, not the closing lesson. Technical language below is for production accuracy only.

**Duration target:** Approximately 30 seconds. This is an editorial choice to test, not a claim about an optimal X algorithmic length.

**Format:** A 1080 x 1350 portrait composition (4:5) for this first feed-oriented version. Preview it scaled down as well as at native size. This is a design choice, not a claim that X requires this aspect ratio. Confirm current upload constraints only when export/upload is actually requested.

**Tone:** Curious, concrete, slightly playful; not an academic lecture, not sensationalist claims about AI.

**Sound:** The video must make sense muted. First preview needs no voice or soundtrack. Optional short narration can be considered later, but no TTS dependency or narration asset should be added without agreement.

## 3. What this first clip deliberately does NOT teach

Remove from the first clip:

- Truth tables and three truth states.
- The general implication clause at both HT worlds.
- Negation and excluded middle.
- Quantification over all smaller H interpretations.
- Temporal operators, infinite traces, and global temporal minimisation.
- A multi-paper bibliography squeezed onto a closing slide.

This is a motivation for HT/equilibrium, not a complete explanation of either. State that scope in the accompanying text. Do not label it a complete TEL tutorial.

## 4. Revised storyboard: the viewer's question comes first

The viewer sees only the short screen copy below. Production notes and mathematical guardrails are not additional subtitles.

### Beat 1 — A question worth asking

**Time:** 00:00-00:05.

**Screen copy, verbatim:** “If nothing contradicts a claim, should you believe it?”

**Visual:** A single floating claim card. A clear opening question, with no branding intro, academic title, formula, or competing caption. The hook can use three large lines if that is more readable than squeezing it into two.

**Purpose:** Connect to the viewer's everyday decision to believe a claim before introducing the teaching example.

### Beat 2 — A tiny story

**Time:** 00:05-00:09.

**Screen copy:** “One rule: if there is fire, there is smoke.”

**Visual:** A flame, a one-way arrow and a smoke puff form a persistent strip. Label this as our little story, not a universal claim about the physical world. The arrow must not imply that smoke proves fire.

**Purpose:** Give the viewer exactly one rule to remember.

### Beat 3 — Someone makes a guess

**Time:** 00:09-00:14.

**Screen copy, sequentially:**

- “Someone guesses: 'There's smoke.'”
- “But nobody said that happened.”

**Visual:** A smoke claim appears under a small amber “THE GUESS” label. The given-facts tray is empty. Do not put a FALSE stamp on the guess: it does not contradict the rule.

**Purpose:** Make the missing support visible, without claiming the event is impossible.

### Beat 4 — Try leaving it out

**Time:** 00:14-00:21.

**Screen copy, sequentially:**

- “Try leaving smoke out.”
- “The rule still works.”

**Visual:** Freeze the original amber guess. In a turquoise “THE TEST” area, let a matching smoke token disappear. The original guess and rule stay visible. No two paragraphs, truth table, or list of new terms.

**Purpose:** Let the viewer see that the rule does not force the smoke claim.

**Production accuracy:** The original guess is held fixed. The removal is only from the tested stronger-truth component. This preserves the source example rather than silently changing the definition of the test.

### Beat 5 — Answer the puzzle without declaring the claim false

**Time:** 00:21-00:25.

**Screen copy:** “So far, it's still just a guess.”

**Visual:** Put a “NOT BACKED UP” tag on the original claim, not a FALSE tag. Hold the composition still so the distinction registers.

**Purpose:** Distinguish lack of support from proof that something is false. Do not imply that all everyday beliefs require certainty or that no evidence can ever arrive later.

### Beat 6 — A reusable question

**Time:** 00:25-00:30.

**Screen copy, sequentially:**

- “Nothing says it's wrong.”
- “But what says it's right?”

**Visual:** The unsupported claim becomes a simple question card. No new topic, technical name, lecture recap or bibliography. Full attribution and the link to Cabalar belong in the accompanying post.

**Purpose:** Answer the opening question with a practical distinction, not a slogan claiming we have proved the guess false. Leave the viewer with a question they can use.

## 5. Visual and typography rules

- Use one focal action at a time. No simultaneous panel reading and subtitle reading.
- Maintain one persistent rule strip and consistent claim tokens throughout.
- Preserve amber for the fixed guess and turquoise for the kept component.
- Use procedural flame/smoke icons, not stock media. The installed fonts remain the agreed exception to code-generated visual assets.
- Use Segoe UI Bold for the main copy and Regular only where needed.
- Starting typography targets at 1080-wide portrait output: 72-88px hook, 56-64px main statement, approximately 40px minimum for essential token labels. Tune by viewing at phone-feed scale, not by accepting numbers blindly.
- Fit the prose to the design; do not shrink a sentence to fit. Rewrite or split it.
- Essential copy should normally fit within two short lines, with no paragraph blocks. The selected opening question may use three large lines; never shrink it to enforce a line count.
- Reserve breathing room around all edges and keep the important action out of the footer. Do not assume a particular X UI overlay layout without checking the actual playback context.
- No scrolling source text, default pixel font, grain over glyphs, bloom over captions, or decorative particles.
- Use direct motion with a causal purpose: appearance, pinning, removal, verification and rejection.

## 6. Mathematical boundaries

- This is an illustrative formal theory over fire and smoke. It is not a real-world causal diagnosis.
- The original non-temporal rule remains the same throughout this first clip.
- Under ordinary classical semantics, smoke-only is permitted. The first clip must not say that the smoke guess contradicts the rule.
- The equilibrium test rejects that candidate because a proper smaller kept component still satisfies the theory while the assumed component remains fixed.
- Do not imply that HT truth alone automatically proves which claims are justified. The meaningful selection comes from equilibrium.
- Do not imply that this intuition completely defines HT negation, implication, or TEL.

## 7. Proposed accompanying post

> If nothing contradicts a claim, should you believe it?
>
> Not being ruled out is different from being backed up. This little example follows Pedro Cabalar's introduction to Here-and-There: a starting intuition, not the full definition.
>
> Source: https://www.dc.fi.udc.es/~cabalar/RW2021.pdf (section 2)

For a thread/reply, add the survey:
https://www.dc.fi.udc.es/~cabalar/tel-dp60.pdf (sections 2-3).

Do not frame the clip as “the solution to AI hallucinations” or imply the author endorsed this production. A fictional reasoning assistant can be a visual character, but it must not become an unsupported claim about deployed AI systems.

## 8. Follow-up series, not additional scenes in this clip

If the first concept works, separate subsequent lessons:

1. **Why 'not proved' does not mean 'false'.** Introduce the middle HT truth state with one claim.
2. **Why a rule has to work on both lists.** One counterexample, then one correction.
3. **What changes when time enters the story.** Only then add a next-moment rule and the separate temporal axis.

Each follow-up should stand alone. Do not append these to the first clip and recreate the original lecture.

## 9. Implementation tasks after approval

No implementation is authorised by this planning turn. Preserve the live preview and previous videos as they are.

### Task 1 — Lock the micro-story

- Review this storyboard for conceptual accuracy and read it aloud at a natural pace.
- Confirm that the hook, the fixed guess, and the removal action make sense without an explanation outside the clip.
- Tighten wording before drawing more visuals. Reject additions that teach another formal concept.

### Task 2 — Add a separate composition

Likely files:

- Create `src/ht_feed.cpp` with `register_ht_feed(...)` and composition name `here-there-feed`.
- Modify `src/scenes.cpp` to register it.
- Modify `CMakeLists.txt` to compile it.
- Preserve `src/ht_explainer.cpp` and `src/ht_plain.cpp` for comparison.
- Reuse `src/rmtn/here_there.hpp` for the smoke-only witness; do not add a general theorem prover.

### Task 3 — Test semantics and portrait layout before polishing

- Add `tests/ht_feed_tests.cpp` with failing checks first, then implement enough to pass.
- Verify the fixed smoke guess and its smaller empty kept component both satisfy the atomic rule.
- Verify the smoke-only total candidate is not equilibrium and the empty candidate is equilibrium.
- Check text bounds for every beat and for transition frames.
- Render to a GPU texture in memory for tests; do not export screenshots or video.
- Verify the required font was loaded rather than falling back to the pixel font.

### Task 4 — Build and review live

- Run `cmake --build build`.
- Run the renderer, HT-semantics, and new feed-layout tests only. Do not run the existing CLI export tests while the no-PNG/video restriction remains in force.
- Launch `build/raymotion.exe preview here-there-feed` in the native window.
- Check a phone-scale view, roughly 360 pixels wide, using an in-memory scaled render/window path if the existing preview cannot resize. Do not judge only at full desktop size.
- The current preview window is not a browser; no web stack is required.

### Task 5 — User review gate

Ask the viewer to watch muted and answer:

1. What was the rule?
2. What happened when smoke was removed from the kept list?
3. Why was the original guess rejected?

Success means the viewer can describe that little event in their own words. Pixel bounds and the creator's confidence do not substitute for this check.

Only after that review should an MP4 export or optional narration be considered. Do not upload, schedule delivery, or send anything to Telegram/X without a new request.

## 10. Tradeoffs and open decisions

- Shorter scope gives up a complete formal explanation in exchange for an understandable first idea. This is intentional.
- The original fire/smoke metaphor remains to stay close to the author's motivation, but must be presented as a stipulated rule rather than scientific causation.
- A 4:5 version is the proposed starting layout; change it if the user's actual X placement needs another format.
- Keeping both GUESS and KEEP visible costs some screen space, but avoids teaching the wrong minimality operation. If this still overwhelms the viewer, simplify the first clip further and label it only as motivation for rule-based reasoning, not a demonstration of the HT test.
- Voiceover may help a later version, but it does not justify restoring dense screen copy. Muted comprehension remains required.
