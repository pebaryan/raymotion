# here-there-explainer-v2 - script and cue map

Composition: `here-there-explainer-v2`
Story mode: **explainer**
Voice: **none** (text-only cut; captions are the script)
Rate: **2.5 words/s**, one caption line at a time, no per-word glow or box.
Format: 1920x1080, 30 fps. Scene starts come from the TransitionSeries nodes; the values
below are the visible starts after 10-frame crossfades.

## 1. Timing rule

- `line_start_local = round(cumulative_words_before_line / 2.5 * 30)`.
- `scene_duration = round((total_words / 2.5 + tail_seconds) * 30)`.
- `tail_seconds = 0.8` for chapters 1-8; `1.2` for chapter 9 (source card hold).
- `scene_start[i] = scene_start[i-1] + scene_duration[i-1] - 10` (10-frame crossfade).
- The resulting scene starts are `0, 374, 724, 1026, 1436, 1858, 2232, 2582, 2956`;
  the film ends at `3232` frames (107.73 s).
- SRT cues use the same local line starts; the last chapter's final line ends at 3232.
- No second clock: the score, picture, captions and SRT read the same table.

## 2. Caption script and cue map

| Ch | Line | Caption text | Words | Local start (frame) | Absolute start | Absolute end | Visual beat |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | A rule says: if fire, then smoke. | 7 | 0 | 0 | 84 | Rule pill lands; three model cards begin |
| 1 | 2 | Classical logic accepts three models: empty, smoke alone, fire and smoke. | 11 | 84 | 84 | 228 | Three cards; check marks |
| 1 | 3 | All three satisfy the rule. But two invent atoms it never required. | 12 | 228 | 228 | 374 | "Invented" tags on two cards; question |
| 2 | 1 | Here-and-There evaluates one instant at two levels. | 7 | 0 | 374 | 458 | Stacked H/T pair appears |
| 2 | 2 | HERE holds what is established; THERE holds what is assumed. | 10 | 84 | 458 | 578 | HERE/THERE chips; labels |
| 2 | 3 | HERE is a subset of THERE. The reverse need not hold. | 11 | 204 | 578 | 724 | Subset bracket; "one instant, not now/later" note |
| 3 | 1 | For one atom, H subset T leaves three states. | 9 | 0 | 724 | 832 | 2x2 state grid appears |
| 3 | 2 | Absent, assumed only, or established. | 5 | 108 | 832 | 892 | Three allowed cells |
| 3 | 3 | The fourth case, in HERE but not THERE, is impossible. | 10 | 168 | 892 | 1026 | Forbidden cell crossed |
| 4 | 1 | Check fire -> smoke in HERE and in THERE. | 9 | 0 | 1026 | 1134 | Four case cards begin; H/T check labels |
| 4 | 2 | H empty, T fire: HERE passes; THERE fails. | 8 | 108 | 1134 | 1230 | Case 1; fail mark on T |
| 4 | 3 | Add smoke: both pass. Move fire into HERE: HERE fails. | 10 | 204 | 1230 | 1350 | Cases 2 and 3; pass/fail marks |
| 4 | 4 | Add smoke to HERE: both pass. | 6 | 324 | 1350 | 1436 | Case 4; both pass |
| 5 | 1 | Not p means p implies false. | 6 | 0 | 1436 | 1508 | Middle-state pair; p chip |
| 5 | 2 | H empty, T = {p}: p is not true HERE. | 10 | 72 | 1508 | 1628 | p truth row |
| 5 | 3 | p is in THERE, so not p is false HERE too. | 11 | 192 | 1628 | 1760 | not-p truth row |
| 5 | 4 | So p or not p fails HERE. | 7 | 324 | 1760 | 1858 | Excluded-middle card |
| 6 | 1 | Equilibrium starts with a total model (T,T). | 7 | 0 | 1858 | 1942 | Total model pair; lock |
| 6 | 2 | Keep T fixed; shrink HERE. | 5 | 84 | 1942 | 2002 | H shrinks; T lock remains |
| 6 | 3 | Take T = {smoke}. H empty still satisfies the rule. | 10 | 144 | 2002 | 2122 | Smaller-H model; check |
| 6 | 4 | A smaller HERE works, so {smoke} is rejected. | 8 | 264 | 2122 | 2232 | Reject stamp |
| 7 | 1 | The rule alone has one equilibrium model: empty. | 8 | 0 | 2232 | 2328 | Rule-only column; empty stable model |
| 7 | 2 | Add the fact fire. | 4 | 96 | 2328 | 2376 | Fact fire lands |
| 7 | 3 | Remove fire and the fact breaks; remove smoke and the rule breaks. | 12 | 144 | 2376 | 2520 | Two removal tests |
| 7 | 4 | Only {fire, smoke} survives. | 4 | 288 | 2520 | 2582 | Selected stable model |
| 8 | 1 | Time adds a second axis. | 5 | 0 | 2582 | 2642 | Three time columns |
| 8 | 2 | At each instant, HERE is a subset of THERE. | 9 | 60 | 2642 | 2750 | H/T traces |
| 8 | 3 | X means next; G means always. | 6 | 168 | 2750 | 2822 | X/G arrows |
| 8 | 4 | Fire now, always fire -> next smoke, yields smoke next. | 10 | 240 | 2822 | 2956 | Smoke-next cell; trace note |
| 9 | 1 | Two levels of truth, a minimality test, a time axis. | 10 | 0 | 2956 | 3076 | Four-step route |
| 9 | 2 | That is the route from HT to temporal stable models. | 10 | 120 | 3076 | 3232 | Source card + wordmark |

The SRT has one cue per line; the end of each cue is the start of the next line (or the
end of the film for the last line). There is no separate subtitle track.

## 3. Chapter map

| Ch | Title | Starts | Ends | Proof object | Score |
| --- | --- | --- | --- | --- | --- |
| 1 | A rule should not invent facts | 0 | 374 | Three classical-model cards | Rule click, three ticks, low question |
| 2 | One instant, two levels | 374 | 724 | H/T pair, H subset T | Two-note interval, subset stroke |
| 3 | Three atomic states | 724 | 1026 | Exhaustive 2x2 state grid | Three ticks, forbidden buzz |
| 4 | Both checks | 1026 | 1436 | Four-case implication table | Clicks, pass/fail marks |
| 5 | Not established is not false | 1436 | 1858 | p/not-p truth rows | Switch clicks, minor chord |
| 6 | Equilibrium: no smaller HERE | 1858 | 2232 | Smaller-H test and rejection | Lock, shrink, reject |
| 7 | A fact changes the model | 2232 | 2582 | Rule-only vs +fact stable models | Facts stamp, removal clicks, resolve |
| 8 | Add time | 2582 | 2956 | H/T trace grid | Pulse, time ticks, lift |
| 9 | The route | 2956 | 3232 | Four-step route + sources | Resolve, logo bell |

## 4. Source-to-chapter mapping

| Chapter | What it claims | Source |
| --- | --- | --- |
| 1 | `fire -> smoke` has classical models `{}`, `{smoke}`, `{fire,smoke}`; the rule alone should not introduce atoms | Cabalar 2021, sec. 2 opening |
| 2 | HT is a pair `(H,T)` with `H subset T`; H is established, T is assumed; these are truth components at one instant, not now/later | Cabalar 2021, pp. 4-6; Aguado et al. 2013, sec. 2 Def. 1 |
| 3 | One atom has three allowed states; the H-only state is forbidden | Consequence of `H subset T`; exhaustive one-atom enumeration |
| 4 | Implication is checked at `(H,T)` and `(T,T)`; the four cases are exact | Implication clause in Cabalar 2021 / Aguado et al. 2013 |
| 5 | `not p` holds iff p is absent from T; with H={} and T={p}, neither p nor not p is true Here; excluded middle can fail | Cabalar 2021, p. 5; Aguado et al. 2013, sec. 2.1 |
| 6 | Equilibrium is a total model with no smaller H that is still a model; T is fixed | Cabalar 2021, Def. 1; Aguado et al. 2013, Defs. 9-10 |
| 7 | Rule only has stable model `{}`; adding fact `fire` yields `{fire, smoke}` | Definition 1 applied to the rule; the fact variant is our worked extension (labelled "worked example") |
| 8 | Temporal HT uses H/T traces; X = next, G = always; the smaller-H test ranges over whole traces | Aguado et al. 2013, secs. 2-3 |
| 9 | Recap: HT semantics -> equilibrium -> temporal stable models | Both sources; no new claim |

## 5. Scope language

- The film says "worked example" on the chapter 7 fact variant. It is our extension, not
  a claim that Cabalar states that exact example.
- The chapter 8 diagram is **schematic**: it shows three positions of an infinite trace
  and an ellipsis. It does not claim a finite horizon.
- The film does not claim that HT "solves" or "eliminates" anything; it explains a
  semantics. Avoid the words "solves", "eliminates", "breakthrough", "fair AI".
- "Three classical models" and "three states" are exact for the shown two-atom signature
  and one atom; they are not general counts.

## 6. Visual and audio notes

- H/T motif: HERE is cyan and always above THERE (gold); a bracket or subset label sits
  between them. The same pair is reused in chapters 2, 4, 5, 6, 7 and 8.
- Pass marks use green + a check; fail marks use coral + an X; never color alone.
- Caption strip: one line at a time at y=826, max width 1580; current words in ink,
  upcoming at 35% ink; no glow/box.
- The score is generated in `c.assets`; it follows the chapter starts and line cues. The
  bed is quiet; the marks are the foreground.
- The end card has a small `raymotion-logo` (size 120) at bottom right and the two
  author-hosted source URLs. The source line stays at the bottom.

## 7. Still and final checks

- Still frames: 80, 190, 430, 920, 1410, 1800, 2160, 2500, 2920, 3230.
- Transition middles: 379, 729, 1031, 1441, 1863, 2237, 2587, 2961.
- Encoded final frame: one chapter start/middle/end plus the end card, extracted from the
  MP4, not only pre-encode PNGs.
- Record ffprobe, full decode, integrated loudness and true peak in the QC review.
