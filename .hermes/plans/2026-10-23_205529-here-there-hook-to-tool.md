# Here-and-There for X: hook -> tool -> worked result

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Keep the selected everyday hook, then actually introduce Here-and-There as a usable way to inspect a claim, without reverting to a symbol-heavy lecture.

**Architecture:** One continuous fire-and-smoke story. The problem earns attention; two persistent coloured trays become the tool; an animated test supplies the explanation. Name HT after the opening rather than hiding the subject until a separate follow-up. Reuse the same tokens and screen positions throughout.

**Tech stack:** Existing C++ raymotion/raylib and Segoe UI. Native live preview first; no PNG/video export or delivery without a new request.

## Revision authority

This plan supersedes the 30-second standalone concept in `2026-10-23_204612-here-there-x-feed-replan.md`. The user wants that hook as the START of the lesson, followed by scenes introducing HT. In particular, the earlier instruction to keep HT's name off screen is superseded. Avoid unfamiliar technical vocabulary, but introduce the name of the actual tool and explain it immediately.

Target: approximately 88 seconds, adjustable after live viewing. This is not the previous nine-slide lecture with a longer opening. Every section advances one visible action in the same story.

## Scene 1 — The question and the puzzle | 00:00-00:18

Opening copy, exactly:

> If nothing contradicts a claim, should you believe it?

Then show, one statement at a time:

> Our only rule: if there is fire, there is smoke.

> Someone guesses: "There's smoke."

> Nothing breaks the rule. But what backs up the guess?

Visual: a flame-to-smoke rule strip; one amber smoke claim; an empty given-facts area. This is a stipulated toy story. Do not suggest smoke must imply fire, that the event is physically impossible, or that the guess has already been proved false.

End this section with the question unanswered. Do not perform the removal test yet; the next scenes introduce the tool that makes it understandable.

## Scene 2 — Name the tool | 00:18-00:28

Main copy:

> Here-and-There gives us two views of the same claim.

Introduce the name **Here-and-There (HT)** once. The claim card expands into two persistent trays, not a fresh slide full of definitions.

- Amber, left: **THERE — our guess**.
- Turquoise, right: **HERE — what we keep**.

Copy arrives in short sequential beats. No formula, truth table, word "interpretation", or unexplained H/T letters.

Production precision: "what we keep" is a metaphor for the stronger truth component, not an automatic proof engine. The model-selection step comes later. Keep this distinction explicit in the closing bridge and creator notes.

## Scene 3 — Show what the two views mean | 00:28-00:40

Sequential copy:

> There: what we assume is true.

> Here: which of those claims we keep.

> We can keep less. We cannot add something new.

Visual: duplicate the smoke token into the two trays. Briefly show an unrelated fire token trying to enter HERE when it is not in THERE; it cannot enter. Return immediately to smoke. Keep the trays in the same positions for the rest of the lesson.

A short final caption clarifies:

> Two views of one moment. Not now and later.

This conveys the subset restriction without teaching subset notation. It is not a scale of confidence or a probability.

## Scene 4 — Use HT: check the rule on both sides | 00:40-00:56

Prompt:

> The rule has to work on BOTH sides.

Show a short sequence, with short written feedback as well as colour:

1. THERE contains fire without smoke; HERE is empty. THERE's rule check fails. Caption: "Fire needs smoke on this side."
2. Add smoke to THERE. Both rule checks pass. Caption: "Now this side works."
3. Put fire alone in HERE. HERE's check fails. Caption: "Keep fire? Then keep smoke too."
4. Add smoke to HERE. Both checks pass.

Use the same atomic rule throughout this sequence. The viewer watches a token break or repair a condition, not a list of four written set assignments.

Important production note: these are examples of an atomic implication, not a replacement definition for evaluating arbitrary HT formulas by classical evaluation of the smaller set.

## Scene 5 — Return to the original smoke guess | 00:56-01:10

Restore smoke-only in both trays, with no given fire fact. Mark the amber tray as fixed using a pin/lock icon.

Copy:

> Now test our original guess.

> Keep the guess fixed. Try keeping less.

Remove smoke ONLY from the turquoise HERE tray. The amber THERE smoke stays visible.

> Both sides still obey the rule.

> So the smoke was not required.

Tag the original guess **NOT BACKED UP**, not FALSE.

Creator precision: HT supplies the two-view semantics; selecting a total candidate by excluding all proper smaller HERE models is the additional equilibrium criterion. The clip can call this "the next test" rather than requiring the audience to memorise "equilibrium". Do not claim HT alone automatically rejects every unsupported model.

## Scene 6 — A fact makes a difference | 01:10-01:22

Copy:

> Now someone gives us a fact: there IS fire.

Animate a clearly different given-fact card entering the story. Both trays must keep fire; the rule requires smoke on both sides.

Briefly attempt to remove each token from HERE:

- Remove fire: the fact fails.
- Remove smoke: the rule fails.

Return each token to its place immediately. Feedback:

> This time, both claims have to stay.

This gives the viewer a successful explanation as well as a rejected guess. The rule does not change; only the new fire fact is added.

## Scene 7 — Answer the original question | 01:22-01:28

Copy, sequentially:

> Nothing says it's wrong.

> But what says it's right?

Final supporting line:

> HT helps us separate the guess from the test.

No bibliography slide, new formal vocabulary, temporal operators, or request to memorise a definition. Attribute Cabalar in the accompanying post and optional unobtrusive end credit.

## What stays out of this version

- Symbolic definitions, a truth table and the full formal treatment of negation.
- Excluded middle, temporal traces and the mechanics of TEL.
- Claims that this solves hallucinations or establishes physical causal facts.
- A promise that every theory has one uniquely smallest intended answer.

The connection to Cabalar's TEL work is stated in the post: this is the non-temporal foundation, not the full temporal extension. A later lesson can ask "What if the story changes over time?"

## Typography, pacing and visuals

- Portrait 4:5 remains the proposed feed layout, subject to the user's actual placement needs.
- Segoe UI Regular/Bold; large fixed-size text with wrapping, never sentence shrinking.
- Only one statement to read at a time; labels remain stable.
- The frame should retain the rule, the claim tokens and enough context to understand their movement without rereading earlier captions.
- No lecture chapter counters, decorative particles or paragraphs beside a separate subtitle paragraph.
- Build pauses around the token-removal and fact-addition moments; do not give every topic an identical duration by default.
- Check at phone-feed scale. Automated bounds checks do not establish comprehension.
- Muted viewing must work; no added speech/TTS dependency is assumed.

## Animated illustration direction — essential, not optional

The lesson must be understandable through visible actions, with short text naming what just happened. Do not implement the storyboard as caption cards with token lists. This section supplements the scene scripts above and is part of the acceptance criteria.

### Art direction: one little illustrated stage

Use a clean, playful paper-cut/vector world: a small platform, a stylised flame, rounded smoke curls, claim tags, and a rule ribbon. Build all shapes procedurally in raylib. Rounded silhouettes, clear negative space and a restrained palette should make the subject approachable, not like a software dashboard.

The flame and smoke represent logical claims in a toy story. Do not show an apparently observed physical fire or smoke event when the lesson is only assuming a claim. Guesses visibly arrive from a thought bubble or claim card. A given fact arrives with a distinct GIVEN tag. Do not rely on colour alone to distinguish them.

Preserve the same illustrative objects and stage across cuts. Text labels and the rule ribbon remain upright and readable; no spinning words or tiny technical annotations.

### Scene 1 animation: make the opening question concrete

- Start with the selected question, large and immediately visible.
- A thought bubble appears; a smoke puff forms inside it and acquires a GUESS tag. This is a proposed claim, not smoke observed in the environment.
- Reveal the small rule ribbon: flame icon -> smoke icon. A line traces the arrow once to establish its direction.
- The source/given-facts area stays empty. Briefly spotlight it when asking what backs up the guess.
- End by holding the unsupported puff and the empty facts area together. The viewer can see the gap before the tool is named.

### Scene 2 animation: HT transforms the picture

- The single illustrated stage unfolds into two matching versions of itself with a short match-move transition. Do not replace it with a definition slide.
- Attach labels: THERE — our guess, and HERE — what we keep.
- A matching claim token appears on both versions. Keep the correspondence visually clear with one temporary connecting highlight.
- A small shared 'same moment' badge spans the pair. This is a split view, not a time-travel effect.
- Make the expansion itself the reveal of the tool. The letters HT are a small name label, not the scene's main spectacle.

### Scene 3 animation: teach the restriction through an attempted action

- Remove a token from HERE while its counterpart remains in THERE. This movement is allowed.
- Introduce a new fire token only at HERE while it is absent from THERE. It reaches a boundary and gently bounces back, with a short 'Not in our guess' label.
- Return to the original smoke example. Do not leave an unrelated new object cluttering the screen.
- The boundary represents the subset restriction, not a probability threshold or a discovery that fire is false.

### Scene 4 animation: turn rule checking into a visible path

- Each stage has the same compact flame-to-smoke rule ribbon.
- When fire is present, a short highlight travels from the flame towards the smoke position. If smoke is absent, it stops at an outlined empty socket with a written 'Missing smoke' label.
- Add the smoke token; the gap closes and the local check changes to 'Rule obeyed'.
- Repeat the failure/repair on HERE so the viewer sees why checking only THERE was insufficient.
- When fire is absent, do not animate a pulse creating smoke. The rule places no requirement on that stage; use neutral feedback such as 'No fire to check'.
- Use a gentle camera emphasis on the stage being checked, but keep the other stage and its status visible. Never require rereading a paragraph to understand which side failed.

### Scene 5 animation: make the fixed guess unmistakable

- Restore smoke-only on both stages. A pin visibly fixes the THERE claim in place.
- The matching HERE smoke token becomes the object of the test. It slides off its claim position into a temporary removal area; it does not disappear from both sides.
- Recheck both rule ribbons. Neither breaks. Let the viewer see this result before displaying 'So the smoke was not required'.
- Tag the candidate 'NOT BACKED UP'. Do not stamp it FALSE or put a red cross over physical smoke.
- This is the extra selection test after HT rule checking, not an implication that HT satisfaction itself already rejects the candidate.

### Scene 6 animation: show the contrast, not another explanation block

- A new GIVEN: FIRE card lands in the input area, visually distinct from the earlier thought-bubble guess.
- Update the two illustrated stages to contain fire and smoke, with the rule checks visibly satisfied.
- Attempt to remove fire from HERE: the given-fact check breaks. Return fire to its position.
- Attempt to remove smoke instead: the rule path breaks. Return smoke; the path reconnects.
- End with the two claims retained. The viewer has watched why each one must stay rather than merely reading the assertion.
- Use these returns as constraint feedback in the toy reasoning illustration, not a suggestion that HT is a mechanical proof algorithm.

### Scene 7 animation: pay off the original image

- Briefly recall the initial unsupported smoke guess, now bearing its 'not backed up' tag.
- Contrast it with the completed fact-plus-rule illustration, where both required claims stay.
- Settle into a quiet final composition for the closing question. Stop nonessential motion while the audience reads it.
- No new notation, spinning logo, or dense credits take over the ending.

### Motion principles

- Every major movement should reveal a relation: introducing a claim, matching the two views, attempting a forbidden addition, checking a rule, holding a guess fixed, or testing a removal.
- Continuous decorative motion is not the goal. Brief idle holds let each consequence register.
- Use anticipation -> action -> visible consequence -> pause. Do not move a token and change its explanation in unrelated places at the same instant.
- Easing can be soft and friendly for entrances; logical failures should stop clearly, not wobble so much that they seem to pass.
- Fire may have restrained shape motion and smoke may curl gently, but these must not distract from the current claim/test distinction.
- Keep one main focal action at a time. Motion is not a substitute for readable pacing.
- Animate from the current frame and deterministic easing functions. No accumulated physics state, random outcome, image assets, browser stack, or new rendering dependency is needed.

### Animation review gate

In addition to the script checks, review the live preview with the long explanatory copy hidden, leaving the short rule and object labels visible. The viewer should still be able to follow:

1. Which smoke is a guess rather than a given fact.
2. That HERE and THERE are paired views of one moment.
3. Which side breaks the rule and what repairs it.
4. That only HERE changes during the removal test.
5. Why adding a fire fact changes the result.

If those events cannot be followed, improve the staging and movement instead of adding another paragraph. The user should judge engagement at phone-feed scale; no claim of better retention is established merely by adding animation.

## Sources and accuracy checks

Primary references already researched:

- Pedro Cabalar (2021), *Temporal ASP: from logical foundations to practical use with telingo*, section 2: https://www.dc.fi.udc.es/~cabalar/RW2021.pdf
- Aguado et al. (2013), *Temporal Equilibrium Logic: a survey*, sections 2-3: https://www.dc.fi.udc.es/~cabalar/tel-dp60.pdf

The smoke-only candidate satisfies the classical rule. With THERE fixed to smoke, empty HERE still satisfies it. The empty candidate is the stable result for the rule alone. Adding the fire fact makes fire-and-smoke the unique stable result for this two-atom theory. Existing `src/rmtn/here_there.hpp` and `tests/ht_logic_tests.cpp` check these small examples.

## Implementation after storyboard approval

1. Lock the sequential captions and check the spoken reading pace without adding narration assets.
2. Create `src/ht_feed.cpp` as a separate `here-there-feed` composition; register it in `src/scenes.cpp` and add it to `CMakeLists.txt`. Preserve the earlier variants for comparison.
3. Add `tests/ht_feed_tests.cpp` before implementation: semantic state transitions, fixed THERE during removal, allowed tokens, loaded fonts, and text bounds throughout the portrait timeline.
4. Implement the minimum live composition needed to pass the checks. Test with a real GPU render target in memory, not image/video exports.
5. Build with `cmake --build build`; run renderer/HT/feed layout tests only while export remains prohibited. Do not run the CLI export suite.
6. Open `build/raymotion.exe preview here-there-feed` and verify the actual responsive native window.
7. Ask the viewer to explain what THERE and HERE do, why the first smoke guess fails the extra test, and why adding fire changes the result. Simplify based on that response.

No code changes, media export, upload or delivery are part of this planning revision.
