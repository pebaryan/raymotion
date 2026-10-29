# Worked examples: what good looks like, and why

`authoring-standard.md` says what a film must have. This file shows the decisions that
made real films good, so you can copy the **reasoning**, not the layout. Read the
example closest to your job before you write the brief.

Each case has the same parts: the conceit, the shape, the decisions that carried it, and
what nearly failed. The decisions are the point. Two films can share a palette and a
caption style and still be one good film and one template.

| Example | Format | Mode | Read it for |
| --- | --- | --- | --- |
| 1. The Token Tax | 16:9, 5:15, voiced | explainer | turning a research paper into one physical metaphor; an honesty chapter; a score that follows the argument |
| 2. The Token Tax, vertical | 9:16, 60 s | explainer reel | cutting a long film down to one claim and rebuilding it for a tall frame |
| 3. Time Travel: Austria | 16:9, ~140 s, voiced | narrative explainer | a conceit that orders history; one artifact per chapter; timing from the voice |
| 4. Six seats, one verb | 9:16, 38.5 s, voiced | explainer reel | one object that changes state for the whole reel |
| 5. ASP feed, v1 to v2 | 9:16, 45 s, text-only | explainer reel | how a template becomes a film: replace cards with a proof |
| 6. Here and There feed | 4:5, 60 s, text-only | explainer reel | proving a logic claim by removing something |
| 7. Cautionary | — | — | what "polished but not authored" looks like |

---

## 1. The Token Tax (RotorSubword explainer)

An external project built with raymotion; its source is not in this repository, so the
decisions are recorded here in full.

**Conceit.** "The tokenizer is a price list. It was written by popularity, and
popularity has a passport." A model reads text as pieces (tokens); the same sentence in
Japanese costs far more pieces than in English; a research tokenizer (RotorSubword)
changes the rule that builds the price list and makes it more even, at a cost.

**Audience.** People who use chatbots and have never heard the word "tokenizer". This
choice drove everything: no jargon before the thing it names is on screen.

**Shape: the order is the argument.**

| # | Beat | Job | Proof object |
| --- | --- | --- | --- |
| 1 | The token tax | a number the viewer can feel before any jargon | three towers of token segments: English 4, Chinese 21, Japanese 38 and off the top |
| 2 | What the model sees | define token, cost, context | a sentence cut into chips on a cutting table; chips flip to IDs |
| 3 | The popularity contest | the cause: tokenizers glue the most popular pairs | a merge press: pairs go in, one chip comes out; a counter spins to 50,000 |
| 4 | More pieces, same rule | kill the obvious objection ("just use a bigger vocabulary") | a skyline of seven production tokenizers, still uneven |
| 5 | A different referee | the idea, only now that the problem is felt | a dial that scores a join; the equation typeset from TeX |
| 6 | The loop | how the method is actually trained | a four-station ring with a carrier chip |
| 7 | What changed | the payoff | eight rows; a wipe falls and each bar retracts to its new length |
| 8 | A fairer ruler | audit our own headline: the metric was biased too | a caliper; the ruler cracks; a characters-per-token scale replaces it |
| 9 | The fine print | the cost: compression, tiny data, no downstream test | a balance scale; `PROTOTYPE` and `NO DOWNSTREAM TEST` stamps |
| 10 | Who pays | the memorable close | the towers from beat 1, rebuilt on the fair scale |

Why this order: problem before mechanism, mechanism before method, and the measurement
twist (beat 8) **after** the result, so it corrects the result instead of pre-empting
it. The last beat returns to the first image, so the film closes a loop.

**Decisions that carried it.**

- **One motif.** Everything countable is the same token chip: rounded card, glyph, ID
  underneath. "One piece" versus "a shower of pieces" reads without a legend.
- **Colour has one meaning each, forever.** Turquoise = the fairer alternative. Amber =
  the cost / the bias. Red at most three times, only for "shredded". Colours never swap
  roles.
- **Motion grammar written down.** Entrances 12-16 frames ease-out; numbers count up over
  20 frames; bars grow over 24. No idle motion except a 2 px progress line. Only fade and
  slide between chapters. No zoom for its own sake.
- **Density rule.** "If a still needs a caption to explain the chart, the chart is
  wrong."
- **Equations are typeset, never typed.** `math::Formula::tex`, written on with
  `math::write`.
- **Script rules.** One idea per sentence, 20 words maximum. Plain phrase first, term
  second. Numbers always carry their unit ("tokens per word"). Round for speech ("about
  sixty"), exact on the label (`60.08`). Banned: "solves", "eliminates", "fair AI",
  "cheaper", "breakthrough". The paper says prototype, so the film says prototype.
- **An accuracy ledger with a guard per claim.** Each on-screen number has its table in
  the paper and a wording constraint ("at a matched ~50k vocabulary"; "per word, as
  tokenizers count it"). It also lists what is deliberately **not** said.
- **The honesty chapter is not optional.** Beat 9 is what makes beat 7 credible.
- **Sound follows the argument.** A synthesised score, minor and sparse for the
  problem, opening up at "what changed", thinning for the fine print, resolving on the
  end card. SFX only on picture beats: press for the merge stomps, stamp for the stamps,
  pop for the swap, riser into the results, impact on the end card. The bed ducks about
  6 dB under the voice. Measured: -17.3 LUFS, -1.07 dBTP.
- **Voice attached at each scene's local frame 0,** so retiming a chapter moves its
  voice with it. Speech text spells out what TTS would guess wrong ("B P E", "X L M R");
  captions keep the real spelling.
- **Flat background.** A gradient banded badly in H.264's dark range; the fix was found
  by checking frames extracted from the encoded MP4, not the PNG stills.

**What nearly failed.** CJK glyphs rendered as `?` because the font atlas only held
Latin and `.ttc` collections did not load. Both were fixed in the engine
(`fonts::loadFor`, collection support), with a fallback if no CJK face is installed.
A TTS take clipped a leading short word; the ASR gate caught it.

---

## 2. The Token Tax, 60 s vertical

**The decision that matters: this is not a crop.** The landscape film argues with a
skyline: columns side by side, a camera that pulls back, a chart that sweeps left to
right. None of that survives 9:16. The vertical cut keeps the objects and changes the
axis.

| # | Time | Beat | On-screen line | What moves |
| --- | --- | --- | --- | --- |
| 1 | 0:00-0:09 | One word, two prices | ONE WORD. 60x THE PRICE. | two towers rise from the bottom edge; the camera cranes up the taller one until it leaves frame; `60x` snaps on |
| 2 | 0:09-0:21 | How text is chopped | The model doesn't read letters. It reads pieces. | a sentence strip slides **down** through a blade; pieces drop into a vertical tray and flip to IDs |
| 3 | 0:21-0:34 | Why the bias exists | A popularity contest, not a fairness rule. | pairs drop into a press; a counter spins; a `BPE` stamp lands |
| 4 | 0:34-0:47 | What changes | 2.7x more even. Same vocabulary. | six rows; a wipe falls; each amber bar retracts to its turquoise length |
| 5 | 0:47-1:00 | The catch and the close | Fairness isn't free. The rule is a choice. | a vertical balance tips; then the end card |

**What was cut.** Five of ten chapters, including the method (beat 5-6) and the metric
audit (beat 8). The reel keeps problem, cause, result and cost. The honest limit
survives even at 60 s; the mechanism does not.

**Reel decisions.** First frame already mid-motion. About 110 words in 60 s (~45 s of
speech) so the cuts breathe; a new script, not the landscape voice trimmed. Type 1.3x the
landscape size (captions 46, labels 30, headline 96-120). Safe margins 72 px; top 180 px
and bottom 300 px clear. Art zone y 260-1360, caption zone y 1420-1700. One camera move
per beat; **hard cuts on the beat**, a fade only into the final card. Sources in the post
description, not on screen. Towers 220 px wide with a segment every 18 px, so "a stack of
pieces" reads at arm's length.

---

## 3. Time Travel: Austria

In this repository: `examples/showcase/austria_time_travel.cpp`,
`docs/austria-time-travel-script.md`.

**Conceit.** A time machine sets the dial back from 2026 to 3300 BC and returns, with one
real artifact per stop. History is a topic; the dial is a conceit, because it decides the
order, the transitions and the closing line ("Five thousand years, and we are home").

**Decisions that carried it.**

- **One artifact per chapter,** never a paragraph: Ötzi's copper axe on glacier ice in
  depth of field, Hallstatt as a tilt-shift miniature, the Danube frontier drawn on a map
  where Vindobona morphs into Vienna, the deed of 996 typed onto parchment, the Habsburg
  lands extruded on a tabletop map and sinking under rain in 1918, calendar pages for the
  ten occupied years, 26 October ringing on a month card.
- **The year dial is the persistent frame.** It unifies twelve chapters that each have
  their own visual idea.
- **Timing comes from the voice.** Each line is a measured WAV; chapter lengths derive
  from it; picture and score cues read the same phrase map (`Vo`, `cue(phrase)`). No
  hand-picked frames.
- **A source for every number,** with the exact census figures and their caveats (the
  1910 map extrudes only today's states that lay wholly inside the monarchy).
- **Restraint where it matters.** 1938-1945 draws 66,000 points of light inside Austria's
  outline, one per murdered Austrian Jew, and the score has **no melody** there.
  Restraint is a taste decision; write it into the brief.
- **Music from the subject.** Public-domain Strauss and Mozart fragments: the Danube
  waltz on a music box, rewound, in the minor as the empire falls, in full at the end.
- **Speech text versus caption text.** The voice says "Eeps", the caption shows "Ybbs".

**Its known weaknesses** (the reason it scores 30-34, not 36): one monolithic file and a
few duplicated timing constants.

---

## 4. Six seats, one verb (German A1 reel)

In this repository: `examples/showcase/a1_sein_six_seats.cpp`,
`docs/a1-sein-six-seats-{brief,script,qc}.md`. Scored 32/36 authoredness, 15/18 craft.

**Conceit.** The present tense of *sein* is learned as six pronoun seats at one table,
not as one ending rule. Each pronoun takes a seat and its plate flips to the form.

**Why it works: one object, many states.** The table is the only proof object. Empty,
named (`SEIN` plaque), singular side filled, plural side filled, dimmed for practice,
complete as the answer key. Six beats, one object; nothing is a slide.

| # | Beat | State of the table |
| --- | --- | --- |
| 1 | `Why ich bin — but du bist?` | two place cards, plates flip to `bin` / `bist`, then six empty settings |
| 2 | the name | `SEIN` plaque lowers; still no forms |
| 3 | singular | left column fills top to bottom, one setting at a time |
| 4 | plural | right column fills; a thread links the two `sind` plates |
| 5 | your turn | table dims; `Wir ___ hier.`; a progress arc; `sind` flips into the blank |
| 6 | recap | full table held; end card |

**Taste details.** The hook is a contradiction (`ich bin` but `du bist`), not a topic.
The shared form is shown by a link between the two `sind` plates, **not** by another
label. Every visual leads its spoken word by 0.28 s. The practice beat withholds the
answer until the arc ends.

**What nearly failed** (and is now in the reel failure gallery): a link line crossed the
`du` card; `YOUR TURN` sat inside the table; the answer appeared in the strip and the
caption at once; a 45 s script met a 38.5 s voice.

---

## 5. ASP feed, v1 to v2 (the template-to-film rewrite)

In this repository: `examples/showcase/asp_v2.cpp`, `docs/asp-feed-v2-{brief,script,qc}.md`.

v1 scored **17/36**: a generic card per beat (a farmer, a cat), no proof, no source, no
script. v2 scored **33/36** with the same topic and the same length. What changed is the
lesson:

| v1 | v2 |
| --- | --- |
| a card illustrating each sentence | one 2x2 map that is the problem, the facts and the answer |
| "ASP finds solutions" in a caption | two answer-set mini-maps appear: the solver's actual output |
| no turn | a constraint ("A is not red") crosses out one answer set; one world disappears |
| generic bed | ticks per step, two solution chimes, a cross hit, a resolve on the end card |
| no source | a source line (ASP/clingo, Potassco); "two" is labelled as true for this instance |

**The rewrite recipe.** Find the one object the claim is about; make every beat a state
of that object; make the turn something you can see removed, added or crossed out.

---

## 6. Here and There feed (proof by removal)

In this repository: `examples/showcase/ht_feed_v2.cpp`,
`docs/here-there-feed-v2-{brief,script,qc}.md`. 34/36, 17/18.

**Conceit.** A guess that nothing contradicts is not yet backed up. Two paper islands,
THERE (what we assume, amber) and HERE (what we keep, teal), show it: keep the guess
fixed, remove smoke from HERE, and the rule still passes, so the guess was never
supported. Add the fact "fire" and both become necessary.

**Taste details.** The proof is an **action**: removal. The hook is a question ("If
nothing contradicts a claim, should you believe it?"), and the close asks it back
("Nothing says it's wrong. But what says it's right?"). Text is one karaoke line at a
constant 2.5 words/s, spoken words in ink, upcoming at 32 %, no box, no glow. The fact
variant is labelled "worked example" on screen.

**Fixes worth copying.** Removed items must land in a visible tray, not drift into the
gutter. Meta-objects (rule icons) need a label or a different visual register so they do
not read as part of the world.

---

## 7. Cautionary examples

- `examples/kgnesy/main.cpp`: strong 3D craft and a clear colour system, but one abstract
  diagram, claims in captions, no voice, no script, no sources. Polish without an
  argument. Scores 12-16.
- `examples/showcase/austria_journey.cpp`: a good camera path and real data, but eleven
  stations in the same card template on one path, and a feature-coverage document
  instead of a script. A tour, not a film. Scores 14-18.

---

## The pattern across the good examples

1. **A conceit that decides structure.** The dial orders Austria; the price list orders
   the Token Tax; the table orders *sein*. If your conceit does not decide the order of
   the beats, it is a topic.
2. **One object, many states.** The best reels have one proof object whose state change
   is the argument. Long films have one motif that every chapter reuses.
3. **The turn is visible.** Something is removed, crossed out, retracted, flipped or
   cracked, at the moment the claim is made.
4. **Honesty is a beat.** The cost, the limit or the caveat gets its own screen time.
5. **Colour and motion are rules, written down.** One meaning per colour; a fixed
   entrance grammar; no idle motion.
6. **Time comes from content.** Measure the voice, then set the beats; one cue map for
   picture, captions and score.
7. **Restraint is a decision.** Silence under the hardest chapter, a single fade, no zoom
   for its own sake, no "made with raymotion" chapter.
8. **Checked as the viewer sees it.** Encoded frames, phone-size stills, muted playback,
   measured loudness.
