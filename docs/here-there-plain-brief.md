# Video / live-preview brief: Here-and-There without the math barrier

## Purpose and audience

Explain the intuition behind Pedro Cabalar's introduction to Here-and-There (HT) and equilibrium logic to a viewer who dislikes mathematical notation. Start with one familiar story, reuse its vocabulary, and introduce technical names only after the mechanism is understandable.

The question running through the piece is: **Are we keeping a claim because the rules require it, or did we simply put it into our guess?**

## Current delivery

- Composition: `here-there-plain` in `src/ht_plain.cpp`.
- Live display: `build/raymotion.exe preview here-there-plain`.
- Canvas: 1280 x 720; frame rate: 30 fps; sequence: 108 seconds, looping in preview.
- Current version is silent and caption-led. There is no spoken narration.
- Preview draws to GPU render textures and the window only. It does not export PNGs, audio files or video.
- The earlier `here-there-explainer.mp4` is a separate, unchanged artifact; it does not contain this redesign.

## Typography and visual direction

- Body font: Segoe UI Regular, from the installed Windows font.
- Headings: Segoe UI Bold.
- Rasterize both fonts at 64 pixels and use bilinear texture filtering.
- Main headings: 44 pixels. Core explanations/captions: 32 pixels. Supporting card text: 30 pixels. Labels: 27-30 pixels. Branding/source footers are smaller and are not necessary for following the lesson.
- Fixed-size word wrapping; never shrink a sentence just to make it fit.
- Dark blue background, near-white main text, clear contrast, generous space between blocks.
- Amber consistently means **our guess**; turquoise consistently means **what we keep**.
- No grain, bloom, moving starfields or decorative effects across the text.
- A brief heading entrance and growing divider introduce each scene. A small progress line gives continuity. The main diagrams change when the explanation changes, rather than moving constantly.

## Vocabulary and mathematical guardrails

- **Our guess** is the author's assumed-true component, T.
- **What we keep** is the stronger truth component, H, tested within that guess.
- The kept list cannot contain a claim outside the guess.
- These are two truth components at the **same moment**. They are not present and future.
- Calling H 'what we keep' is an explanatory metaphor, not an automatic proof algorithm. The closing scene says so explicitly.
- Rule satisfaction and equilibrium selection are separate steps. Passing both rule checks does not yet prove that a candidate is stable.
- A candidate is accepted only when no proper smaller kept list satisfies the theory with the original guess fixed. This is not an arbitrary preference for fewer facts across different guesses.
- Temporal minimisation compares whole traces: the kept trace can be smaller at one or more instants; it need not be smaller at every instant.

## Scene-by-scene plan

### 1. A rule is not a reason to invent a fire

**Time:** 00:00-00:12.

**Learning goal:** Understand why merely obeying an ordinary conditional is not enough for the intended rule-based reading.

**On-screen setup:** “Our only rule: if there is fire, there must be smoke.”

**Visual:** Three large cards across the screen:

1. Neither — “No fire, so the rule is not broken.”
2. Smoke only — “The rule does not forbid smoke on its own.”
3. Fire and smoke — “Fire has the smoke it needs.”

**Bottom explanation:** “Ordinary logic allows all three. But our rule alone gives us no reason to add fire or smoke.”

**Direction:** Start with the everyday sentence, not an implication symbol. The three cards are possible sets of true claims over the two named atoms, not three physical experiments.

### 2. Separate a guess from what we keep

**Time:** 00:12-00:24.

**Learning goal:** Introduce the two-component interpretation without opening with H and T.

**Visual:** Two fixed-position panels:

- Left, amber: OUR GUESS. Fire and Smoke are active.
- Right, turquoise: WHAT WE KEEP. Both claim tokens are inactive; the list is empty.

**Supporting lines:** “Assume these for the moment.” / “Try keeping fewer claims.”

**Bottom explanation:** “The kept list can only contain claims from the guess. These are two views of ONE moment, not now and later.”

**Direction:** Keep the labels in the same positions for subsequent scenes so the viewer does not have to relearn the diagram.

### 3. There is a middle ground

**Time:** 00:24-00:36.

**Learning goal:** Understand the three statuses of one claim without a truth table.

**Visual:** Three cards:

- Not in the guess — “We treat the claim as false in this scenario.”
- Only a guess — “It is assumed, but not on the kept list.”
- Also established — “The claim is on both lists.”

**Bottom explanation:** “These are three logical statuses, not percentages. A claim cannot be kept if it was never in our guess.”

**Guardrail:** Do not describe the middle state as a probability, contradiction or a physical observation waiting to happen.

### 4. Check the rule on BOTH lists

**Time:** 00:36-00:48.

**Learning goal:** Show why the rule has to survive both components.

**Visual:** Reuse the two panels and the Fire/Smoke tokens. Change state every four seconds:

| Time | Our guess | What we keep | Feedback |
|---|---|---|---|
| 00:36-00:40 | Fire | Nothing | The guess breaks the rule: fire lacks smoke. |
| 00:40-00:44 | Fire and Smoke | Nothing | Both lists obey the rule. This is not yet a stable explanation. |
| 00:44-00:48 | Fire and Smoke | Fire | The kept list now breaks the rule: it keeps fire without smoke. |

**Direction:** Use explicit written pass/fail feedback as well as colour. The story, not symbolic manipulation, carries the explanation.

**Guardrail:** These list checks illustrate this particular atomic rule. Do not present classical evaluation of the smaller list as the semantics for arbitrary nested HT formulas.

### 5. Not established does not mean false

**Time:** 00:48-01:00.

**Learning goal:** Distinguish lack of stronger truth from negation.

**Visual:** The guess contains Fire; the kept list is empty. Two supporting statements read “Fire is still in the guess” and “Fire is not established here.”

**Bottom explanation:** “We cannot call fire false just because it is not established. To say 'not fire', it must be absent from the guess too.”

**Guardrail:** This is an illustration of an atom's truth status, not a claim that this pair satisfies the earlier fire-to-smoke rule. It explains the meaning of negation without requiring the audience to learn the term 'excluded middle'.

### 6. Try taking an unsupported claim away

**Time:** 01:00-01:12.

**Learning goal:** Make equilibrium's smaller-H test concrete.

**Initial visual:** Both lists contain Smoke only. The rule is still the original same-moment rule: fire requires smoke.

**Animation at 01:04:** Remove Smoke from WHAT WE KEEP, leaving OUR GUESS unchanged. The rule still passes because the kept list contains no fire requiring smoke.

**Before removal caption:** “Keep the guess fixed. Can we keep fewer claims while still obeying every rule?”

**After removal caption:** “The rule still works without keeping smoke. Reject the padded guess: smoke was not required.”

**Direction:** The unchanged amber panel is important. We are changing the kept component inside one fixed candidate, not comparing unrelated guesses.

### 7. Now give the story a real starting fact

**Time:** 01:12-01:24.

**Learning goal:** Show how evidence changes what must remain.

**Visual:** A side-by-side comparison:

- RULE ALONE — “Nothing needs to happen.” / “No fire. No smoke.”
- ADD A FACT: THERE IS FIRE — “Now smoke is required.” / “Fire and smoke both stay.”

**Bottom explanation:** “Removing fire would break the fact. Removing smoke would break the rule. Now both claims have to stay.”

**Guardrail:** This is a formal toy theory. The rule is stipulated, not a scientific account of combustion. Over this two-atom example, the rule alone has the empty stable model; adding the fire fact gives the stable model containing both atoms.

### 8. The same idea can follow a story over time

**Time:** 01:24-01:36.

**Learning goal:** Introduce TEL only after the non-temporal intuition is established.

**New rule, stated explicitly:** “Fire must be followed by smoke at the next moment.”

**Visual:** Three time cards connected left-to-right:

- START — Fire.
- NEXT MOMENT — Smoke.
- AFTER THAT — Nothing.

Below the cards: “The story continues ...”

**Bottom explanation:** “Start with a fire. Smoke must follow. Keep the whole guessed story fixed, and test whether any claims can be dropped.”

**Guardrails:** This is a new next-moment rule, not a change in the meaning of the earlier rule. The three cards show a prefix of an infinite trace, with neither atom in the continuation of this example. They are not the two HT components. The precise example is an initial fire fact together with an always-next-smoke rule.

### 9. That is the idea behind equilibrium

**Time:** 01:36-01:48.

**Learning goal:** Attach the technical names to the intuition already shown.

**Visual:** Two large named sections:

- HERE-AND-THERE — “The kept list and the guess are two levels of truth, not a proof algorithm.”
- EQUILIBRIUM — “Accept a complete guess only if no smaller kept list still obeys the theory.”

**Bottom explanation:** “For temporal equilibrium, compare whole stories. A smaller kept story differs at one or more moments, not necessarily all.”

**Direction:** 'Complete' here means total: the initial kept list equals the guess. It does not mean an arbitrary theory has a unique intended model. Keep the final conclusion on screen long enough to read before looping.

## Sources and attribution

- Pedro Cabalar (2021), *Temporal ASP: from logical foundations to practical use with telingo*, section 2: https://www.dc.fi.udc.es/~cabalar/RW2021.pdf
- Aguado, Cabalar, Dieguez, Perez and Vidal (2013), *Temporal Equilibrium Logic: a survey*, sections 2-3: https://www.dc.fi.udc.es/~cabalar/tel-dp60.pdf
- Detailed formal source mapping and the first version's script: `docs/here-there-explainer.md`.

This is an independent educational paraphrase of Cabalar's presentation, not his own production or endorsement. The simplification preserves the underlying definitions; it does not replace them with a different logic.

## Preview and acceptance checks

- Open a native raylib window; do not export PNG/video during review.
- Segoe UI must actually load; do not silently fall back to the old pixel font.
- Keep body font sizes fixed and wrap text. Check every frame for measured width/vertical overflow.
- Preserve the same list positions and colours throughout.
- Readability must be judged by the viewer in the live window; automated bounds tests do not prove that the explanation feels clear.
- Existing controls: Space pauses/resumes; hold Left for slow playback; hold Right for faster playback; Escape closes. The sequence loops.
