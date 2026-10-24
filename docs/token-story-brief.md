# token-story — storyboard and accuracy notes

**Composition:** `token-story` — 1920x1080 @30fps, ~40 s, procedural art + synthesised score.
**Audience:** general feed viewer who uses chatbots but has never seen how they read text.
**Learning goal:** after watching, the viewer can explain in their own words that a model
turns text into numbered pieces and repeatedly predicts the most likely next piece.

**Hook (opening question):** "Any chatbot finishes this sentence - how does it know?"
with the running example `The cat sat on the mat.` shown as a chat line.

**One continuous example:** the same sentence is chopped, numbered, predicted, completed,
extended, and re-predicted throughout. Objects persist across scenes (same chips, same
machine panel, same candidate rows in scenes 4/7).

## Scenes (8 + outro)

| # | Caption on screen | Visual action |
|---|---|---|
| 1 | Any chatbot finishes this sentence - how does it know? | Chat bubble types "The cat sat on the", gold gap + blinking cursor, "?" pops over the gap. |
| 2 | First it chops the sentence into pieces - tokens. | Cut lines drop between words; words separate into chips; dots mark spaces glued inside pieces. |
| 3 | Each piece flips into a number from its vocabulary. | Chips flip (squash-out/spring-in) from word to id: 464, 5882, 6377, 592, 262. |
| 4 | Then it answers one question: what usually comes next? | Numbered chips stream into the machine panel; candidate chips mat/bed/rug grow odds bars; tag marks the current most likely one. |
| 5 | It appends the winner - and asks the same question again. | Winning "mat" chip flies along an arc into the gap; period pops; "+1 piece" badge. |
| 6 | Piece by piece, that becomes a whole paragraph. | Loop arrow draws from the finished sentence back into the machine; more token chips pop onto the chain ("It slept for hours"). |
| 7 | Swap one piece, and every guess changes. | cat chip slides out, dog chip pops in with a flash ring; the odds bars morph live - bed overtakes mat; verdict tag moves rows. |
| 8 | No reading, no thinking - just very good guesses about what comes next. | Mosaic of mini prediction panels fades in behind a card restating the pipeline: sentence -> pieces -> numbers -> what comes next; credits. |
| — | outro | "raymotion" wordmark, tagline. |

Transitions: slide(16) / fade(12-18), overlapping between scenes.

## Precision caveats (creator-only, not on screen)

- Token ids (464/5882/6377/592/262, dog 1294) are illustrative placeholders in the style of
  real subword vocabularies; the on-screen footnote states "ids illustrative".
- Odds bars (58/21/13 %, then 22/49/9 %) are made-up demonstrations of a distribution over
  candidates; labelled "odds illustrative". Sums stay below 100 % because unseen candidates
  exist ("one pass = one prediction").
- Tokens ≠ words: leading-space markers (dots) show that a token may carry a glued space.
- Scene 6 footnote mentions the model stops when the likely piece signals the end.
- "No reading, no thinking" describes the autoregressive next-piece mechanism at intro
  level; it is a framing of the actual computation (sequence -> next-token distribution),
  not a claim about model capability or understanding.

## Acceptance checks

- Every scene's copy fits one wrapped caption block; nothing overlaps the kicker/caption area.
- Chip, machine-panel, and candidate-row positions persist across scenes 4 and 7 so the
  odds change is legible as the *same* list rebalancing.
- Frame determinism: all randomness is frame-indexed hashing, no accumulated state.
