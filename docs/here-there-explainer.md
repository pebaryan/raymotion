# Here-and-There: a caption-led raymotion explainer

Output: `out/here-there-explainer.mp4`
Composition: `here-there-explainer`
Format: 1280x720, 30 fps, 108 seconds. English on-screen explanation with a code-generated tonal soundtrack; no spoken narration. All visuals are drawn by C++/raylib. No external images or fonts.

## Primary sources

1. Pedro Cabalar (2021), *Temporal ASP: from logical foundations to practical use with telingo*, Reasoning Web summer-school lecture notes. Section 2, especially printed pages 2-6: the fire-to-smoke motivation; HT interpretations; satisfaction, negation, persistence; and Definition 1 (equilibrium models).
   Author-hosted PDF: https://www.dc.fi.udc.es/~cabalar/RW2021.pdf

2. Felicidad Aguado, Pedro Cabalar, Martin Dieguez, Gilberto Perez and Concepcion Vidal (2013), *Temporal Equilibrium Logic: a survey*, Journal of Applied Non-Classical Logics 23(1-2), 2-24. Section 2, Definition 1: temporal HT; section 2.1: persistence, negation and excluded middle; section 3, Definitions 9-10: temporal equilibrium and temporal stable models.
   Author-hosted manuscript: https://www.dc.fi.udc.es/~cabalar/tel-dp60.pdf

Author's publication listing: https://www.dc.fi.udc.es/~cabalar/

These were retrieved and read for this video, not inferred from search snippets. The explanation paraphrases the sources; diagrams and the worked teaching sequence are original. The video is not authored or endorsed by Cabalar. HT and equilibrium logic are not attributed as Cabalar's inventions: the sources credit Heyting's HT and David Pearce's equilibrium logic.

## Chapter script / source mapping

00:00-00:12 — Why ordinary implication is not enough.
Over the signature {fire,smoke}, fire -> smoke has the classical models {}, {smoke} and {fire,smoke}. A rule-based reading should not arbitrarily introduce atoms. Based on Cabalar 2021 section 2's opening example.

00:12-00:24 — One instant; two levels of truth.
An HT interpretation is (H,T), with H a subset of T. Atoms in H count as established, atoms in T as assumed true. These are truth components at one instant, not now/future. H is not an automatic proof procedure. Based on Cabalar 2021 pp. 4-6, including his warning about interpreting the tag 'proved' before minimisation.

00:24-00:36 — Three possible atomic states.
p can occur in neither set, only T, or both. The H-only case is forbidden. This is not a probability scale or a claim of inconsistency.

00:36-00:48 — Both implication checks.
Four 3-second examples for fire -> smoke:
  H={}, T={fire}: H's local condition passes, T's fails.
  H={}, T={fire,smoke}: both pass.
  H={fire}, T={fire,smoke}: H fails, T passes.
  H={fire,smoke}, T={fire,smoke}: both pass.
For arbitrary formulas, the checks mean satisfaction in (H,T) and in (T,T), not simply classical evaluation of the smaller set. The video says this explicitly. Based on the implication clause in both sources.

00:48-01:00 — Negation and excluded middle.
not p abbreviates p -> false. It holds iff p is absent from T. With H={}, T={p}, neither p nor not p holds Here; p OR not p is therefore not HT-valid. This is a counterexample to validity, not a statement that excluded middle is always false. Based on Cabalar 2021 p. 5 and survey section 2.1.

01:00-01:12 — Equilibrium's removal test.
Start with a total model (T,T), keep T fixed, and test proper subsets H. For T={smoke}, the smaller H={} still satisfies fire -> smoke; hence the total candidate is rejected. The definition is a universal no-smaller-model condition, not merely deletion of one atom at a time. Based on Cabalar 2021 Definition 1.

01:12-01:24 — Add a fact.
The rule alone has only the empty stable model. Adding fact fire yields the unique stable model {fire,smoke}: removing fire violates the fact, removing smoke violates the implication. The fact-plus-rule variant is our small worked extension of Cabalar's example.

01:24-01:36 — Temporal extension.
Separate the two axes: time runs horizontally, and H_i is a subset of T_i vertically at each time. X means next and G means always. The displayed example is fire AND G(fire -> X smoke), with fire at time 0, smoke at time 1, and empty states thereafter. The diagram shows only the first three positions of an infinite trace (ellipsis), not a finite-trace horizon. In this example the empty continuation contains neither atom. TEL's smaller-H test ranges over entire traces with T fixed. Based on survey sections 2-3.

01:36-01:48 — Recap and author-hosted references.
HT supplies the model semantics, equilibrium supplies model selection, and temporal operators extend the setting to traces.

## Reproduce and verify

    cmake --build build
    ctest --test-dir build --output-on-failure
    build/raymotion.exe render here-there-explainer

`src/rmtn/here_there.hpp` is a deliberately limited executable verifier of these two-atom worked examples, not a general HT/TEL or ASP solver. `tests/ht_logic_tests.cpp` exhausts all valid two-atom HT interpretations for implication/negation and all four candidate T sets for the two displayed theories.

Full-video verification used ffprobe's frame count and a full ffmpeg decode. Representative frames were extracted from the encoded MP4, not just pre-encode PNGs, into `out/here-there-review/`.
