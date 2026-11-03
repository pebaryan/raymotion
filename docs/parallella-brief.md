# parallella — authoring brief (one film, two chapters)

Status: `draft` (author-approved for build; owner review pending).

The owner asked for one film instead of two parts: "can't it be made as 1 video?". This film merges
`parallella-2012` (part 1, `docs/parallella-2012-brief.md`) and `parallella-sixty` (part 2,
`docs/parallella-sixty-brief.md`). Those briefs keep the full source ledger, metaphor maps and visual
system. Both still apply here; this file records only what the merge changed.

| | |
| --- | --- |
| Composition | `parallella` (`examples/showcase/parallella.cpp`) |
| Story mode | explainer |
| Runtime | 84.5 s (the voice: 71 s of takes). Under the 90 s vertical-feed ceiling; over the 30-60 s reel default |
| Aspect | 1080x1920, 30 fps |
| Voice | the owner's Qwen3-TTS clone; 11 lines in `assets/vo/parallella/` (8 reused from the parts, 3 new) |

## 1. One sentence

> A $99 board from 2012 runs a real language model, exactly. Its famous "60 tokens a second" was
> true only for one matrix pass: the model runs at 15.8, and at 40 when batched.

## 2. Structure

| # | Chapter | Scene | Voice line | From |
| --- | --- | --- | --- | --- |
| 1 | 01 · DOES IT RUN? | hook: the board's sentence typing over the 3D board | A ninety-nine dollar board from twenty twelve wrote this. | part 1 |
| 2 | | the machine: dolly to the chip, 16 cores rise | It's a Parallella. One chip with sixteen cores, ... | part 1 |
| 3 | | the problem: 60.8 MB vs 512 KB to scale; streamed | A small story model has fifteen million weights ... | part 1 |
| 4 | | the shrink: weights snap to -1/0/+1; 3.8 MB | So the model was trained with three values per weight ... | part 1 |
| 5 | | the proof: board vs reference, 24 of 24 identical | Every multiply runs on that chip. Checked word for word ... | part 1 |
| 6 | 02 · HOW FAST? | bridge: `IT RUNS.` -> `HOW FAST?`; the ruler; the dashed 60 rises on "sixty" | **new:** So it runs. How fast? On paper, sixty tokens a second. | new |
| 7 | | ARM bar to 13.8 | **new:** The board's own ARM processor, for comparison, manages fourteen. | new (replaces part 2's setup, which re-introduced the board) |
| 8 | | measured 49.4; 1 KB chunks; 57-59; `PROJECTION HELD` | Measured on the chip: forty-nine ... | part 2 |
| 9 | | `ONE MATRIX PASS`; drop to 15.8, labelled "the whole model: the one that wrote the sentence" | **new:** But that was one matrix pass, not the model. The whole model runs at just under sixteen. | shortened: the token-exact proof is chapter 1's, so part 2's typed strip is cut |
| 10 | | 8 stories -> 40.2; two boards -> 80.2; each story 5 tok/s | Batching shares the weights ... | part 2 |
| 11 | | close: `SIXTY WAS TRUE. FOR A MATRIX PASS. MEASURE WHAT YOU SHIP.`; source; sinks | Sixty was true, for a matrix pass. Measure the thing you ship. | part 2 |

What was cut from the parts: part 1's close (`IT RUNS. HOW FAST? PART 2.`), which becomes the bridge's
first headline; part 2's hook and its board card; part 2's typed token strip.

## 3. Reveal map (new scenes only; scenes 1-5 and 8, 10, 11 keep the parts' maps with line indices +5)

| Scene | Unit | Anchor (line, word) |
| --- | --- | --- |
| 6 | `IT RUNS.` | "So" (5,0), out at "How" |
| 6 | `HOW FAST?` | "How" (5,3) |
| 6 | the dashed line rises 0 -> 60 | from "On" (5,5) to "sixty" (5,7) +0.2 |
| 6 | `60 TOK/S ON PAPER.`, `PROJECTED 60` | "sixty" (5,7), "tokens" (5,8) |
| 7 | ARM bar rises, 13.8 | from "ARM" (6,3) to "fourteen" (6,8) |
| 9 | `ONE MATRIX PASS` | "matrix" (8,4) |
| 9 | drop to 15.8 | from "The" (8,9) to "sixteen" (8,16) |
| 9 | "the whole model ..." label | "model" (8,11) |

## 4. Visual and audio continuity

- The HUD carries the chapter: `01 · DOES IT RUN?` in teal, then `02 · HOW FAST?` in amber. The progress
  line changes colour with it. The page counter runs `nn / 11`.
- One score: a D-minor pulse, opening to major for the proof (scene 5) and the batch (scene 10),
  resolving on the close.
- SFX: both parts' cues at their new line indices.
- No loop: the close sinks to the empty ruler, and frame 0 is the 3D board. A feed loop jumps there.

## 5. Sources

Unchanged from the parts' §5. The new lines add no claim: "fourteen" is the ARM's 13.84 (S §2, §10), and
"just under sixteen" is the decoder's 15.80 (S §5). "The one that wrote the sentence" is true: the 15.80 tok/s
decoder is the one whose 24 tokens chapter 1 shows (S §5, V).

## 6. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Merge into one film | owner | 2026-11-02 | "can't it be made as 1 video?" |
| Brief | author | 2026-11-02 | written after the merge was built (see QC Gate 0) |
| Owner review | the user | — | pending |
