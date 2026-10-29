# Agent instructions for this repository

## Before writing a raymotion composition

This is mandatory, not advisory:

1. Read `skills/raymotion/references/authoring-standard.md` and
   `skills/raymotion/references/worked-examples.md` (the decisions behind good films).
   For a vertical or feed cut (9:16, 4:5, under ~90 s), also read
   `skills/raymotion/references/reels.md` and use its defaults.
2. Copy `skills/raymotion/templates/BRIEF.md` to `docs/<slug>-brief.md` and fill it.
3. Do not write composition code until the brief has:
   - a one-sentence conceit (not just a topic);
   - a **proof object** for every chapter (chart, map, artifact, mechanism, count, or
     physical metaphor);
   - a script, or a declared text-only cue map;
   - a source ledger, or on-screen "illustrative" labels;
   - a visual system, audio plan, asset plan, still-frame list and thumbnail candidate;
   - a **reveal map** for every scene: word/onset anchor, 0.20-0.40 s visual lead, hold,
     and one entrance at a time;
   - a **metaphor map** for every numeric or relational claim (mapping + animated
     change), with no duplicated diagram/key labels.

## After the build

Run `skills/raymotion/references/quality-control.md` and fill
`skills/raymotion/templates/QC-REVIEW.md`. A release needs:

- each scene reviewed one at a time from a scene-only clip or 2-3 stills, with a written
  sync note and approval; no contact sheet or full-film pass as a substitute;
- stills reviewed at every chapter start/middle/end and in transitions;
- a draft watched muted, then with sound;
- the executable confirmed newer than the last source edit, and the final render rebuilt
  after the last approved change;
- a final render checked with `ffprobe`, a full decode, loudness/true peak, and frames
  extracted from the encoded MP4;
- Declare the story mode (explainer / narrative / promo / utility).
- Authoredness ≥ 22 **and** Craft ≥ 13 with no critical fail and Gates 0–7 passed.
  A public or showcase cut should reach Authoredness ≥ 30 and Craft ≥ 16. The craft
  axis covers scenario coherence, information design, text pacing, mode grammar,
  spec-to-screen fidelity and revision quality; Gate 7 covers mode packaging.

"It renders" is not done. A polished template is still a template.

## Repository rules that still apply

- Frames are pure functions of `t`; never accumulate state between frames.
- `up` assigns absolute values from `t`; never `+=`.
- GL calls (fonts, textures, models) only inside drawing callbacks.
- Lambdas capture by value or `shared_ptr`, never references to locals.
- Generated files go through `c.assets`; `list` must work from any directory.
- Use deterministic randomness (`rnd01`, `rrange`), never `rand()`.
- Every historical or statistical claim needs a source or an explicit illustrative
  label.

## Reference implementations

`skills/raymotion/references/worked-examples.md` explains each of these, plus The Token
Tax (an external RotorSubword explainer and its 60 s vertical cut, recorded there in full).

- Authored: `examples/showcase/austria_time_travel.cpp` +
  `docs/austria-time-travel-script.md`.
- Authored reels: `examples/showcase/a1_sein_six_seats.cpp` (one object, many states),
  `examples/showcase/asp_v2.cpp` (a template rewritten into a proof),
  `examples/showcase/ht_feed_v2.cpp` (proof by removal), each with brief and QC in `docs/`.
- Cautionary: `examples/kgnesy/main.cpp` (abstract diagram, claims in captions, no
  voice/docs/verification).
- Cautionary: `examples/showcase/austria_journey.cpp` (one station template repeated,
  feature-coverage doc instead of a script).
