# here-there-feed-v2 - script and cue map

Composition: `here-there-feed-v2`
Story mode: **explainer**
Voice: **none** (text-only cut; captions are the script)
Rate: **2.5 words/s**, one line at a time, spoken words in ink, upcoming at 32%.
Format: 1080x1350, 30 fps. There are no crossfades; chapter and line changes are on the
same word-count cue map.

## 1. Timing rule

- `line_start_local = round(cumulative_words_before_line / 2.5 * 30)`.
- `scene_duration = round((chapter_words / 2.5 + tail_seconds) * 30)`.
- `tail_seconds`: chapter 1 `3.0` (hold the question), chapters 2-5 `0.8`, chapter 6
  `2.0` (source/wordmark hold).
- Starts: `0, 210, 558, 882, 1266, 1614`; end `1794` (59.8 s).
- The score and SRT read the same table; there is no second clock.

## 2. Caption script and cue map

| Ch | Line | Caption text | Words | Local start | Absolute start | Absolute end | Visual beat |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | If nothing contradicts a claim, should you still believe it? | 10 | 0 | 0 | 210 | GUESS bubble; empty facts; smoke appears |
| 2 | 1 | A guess says there is smoke. The given facts are empty. | 11 | 0 | 210 | 342 | Unfold into THERE and HERE |
| 2 | 2 | There is what we assume. Here is what we keep. We can keep less, never add. | 16 | 132 | 342 | 558 | Smoke only in THERE; HERE empty |
| 3 | 1 | The only rule: whenever there is fire, there is smoke. | 10 | 0 | 558 | 678 | Rule ribbons appear |
| 3 | 2 | HERE is empty, so its check passes. THERE has smoke, so its check passes too. | 15 | 120 | 678 | 882 | Both checks land |
| 4 | 1 | A rule that is merely not contradicted is not yet supported. | 11 | 0 | 882 | 1014 | Pin THERE; the test begins |
| 4 | 2 | Keep the guess fixed. Now remove smoke from HERE. | 9 | 132 | 1014 | 1122 | Smoke leaves HERE; rule still passes |
| 4 | 3 | The rule still holds. The smoke was not required here. | 10 | 240 | 1122 | 1266 | "Not backed up" stamp |
| 5 | 1 | Now add a fact: there is fire. | 7 | 0 | 1266 | 1350 | GIVEN: FIRE badge |
| 5 | 2 | Remove fire and the given fact breaks; remove smoke and the rule breaks. | 13 | 84 | 1350 | 1506 | Two removal checks |
| 5 | 3 | Both stay. Now the world is supported. | 7 | 240 | 1506 | 1614 | Supported world |
| 6 | 1 | Nothing says it's wrong. But then what says it's right? | 10 | 0 | 1614 | 1794 | Final question; source/wordmark |

SRT: one cue per line; each cue ends at the next line start (or 1794 for the last).

## 3. Chapter map

| Ch | Title | Starts | Ends | Proof object | Score |
| --- | --- | --- | --- | --- | --- |
| 1 | The question | 0 | 210 | GUESS bubble with smoke; empty facts | Paper tick, smoke puff |
| 2 | Two views | 210 | 558 | H/T paper islands; smoke only in THERE | Unfold whoosh, two chimes |
| 3 | Both sides | 558 | 882 | Fire-to-smoke ribbon and two checks | Two check bells |
| 4 | Not backed up | 882 | 1266 | Pin, smoke leaves HERE, rule still passes | Pin, slide, low stamp |
| 5 | The fact | 1266 | 1614 | GIVEN: FIRE; two removal tests; both stay | Fact stamp, two checks |
| 6 | Close | 1614 | 1794 | Final question; source/wordmark | Resolve, logo bell |

## 4. Source-to-chapter mapping

| Chapter | What it claims | Source |
| --- | --- | --- |
| 1 | A guess can be smoke-only; no given facts | Cabalar 2021, sec. 2 opening (our teaching framing) |
| 2 | There is assumed; Here is kept; a view can keep less, not add | HT pair with H subset T; Cabalar 2021, pp. 4-6 |
| 3 | `fire -> smoke` is checked at both sides; H empty/T smoke passes both | Implication clause; exact for H={}, T={smoke} |
| 4 | A smaller HERE still passes, so the guess is not backed up | Equilibrium Definition 1; no smaller-H model |
| 5 | Fact fire makes fire and smoke both necessary; remove either breaks | Definition 1 + our worked fact extension |
| 6 | "Not contradicted" is not the same as "supported" | The film's one-line argument; no new formal claim |

## 5. Scope language

- The fact world is **our worked example**, labelled on screen.
- The feed does not cover negation, temporal operators or full formal definitions.
- It does not claim that smoke is physically impossible, or that a guess is a proof.
- It says "not backed up" for the specific total model `{smoke}` under the no-smaller-H
  criterion; it does not claim every smoke-only theory is unsupported.
- "HERE" is the stronger/established component and "THERE" the assumed component at one
  instant; not now/later.

## 6. Visual and audio notes

- Two islands: THERE amber (left), HERE teal (right). H subset T is the labelled relation.
- Tokens: flame and smoke are cut-paper objects; the ribbon is the rule.
- The pin and the removed-smoke tray show that THERE is fixed while HERE shrinks.
- The final supported world keeps both tokens; remove-fire and remove-smoke checks show
  why.
- Captions are top, one line at a time; no box; the islands start at y=531.
- Score: sparse paper ticks, a two-note interval, a smoke whoosh, check bells, a low
  stamp for "not backed up", a fact stamp, and a resolve. Target -16 LUFS, -1.5 dBTP.

## 7. Still and final checks

- Still frames: 60, 150, 400, 620, 1040, 1125, 1320, 1580, 1700, 1785.
- Extract five encoded frames from the final MP4, including the removal and supported
  world.
- Record ffprobe, full decode, integrated loudness and true peak in the QC review.
