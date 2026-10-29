# here-there-explainer-v2 - QC review

| | |
| --- | --- |
| Composition | `here-there-explainer-v2` |
| Commit / build | working tree, `raymotion.exe` built 2026-10-26 |
| Reviewer | author-agent self-review; human muted/sound watch pending |
| Date | 2026-10-26 |
| Review stage | final |
| Story mode | **explainer** |
| Artifacts | `docs/here-there-explainer-v2-brief.md`, `docs/here-there-explainer-v2-script.md`, `out/here-there-explainer-v2.mp4`, `out/here-there-explainer-v2_draft.mp4`, `out/here-there-explainer-v2.srt`, `out/here-there-explainer-v2-thumb.png`, `out/ht-v2-check/` |

## 1. One-sentence conceit

> A rule cannot invent facts: stacking HERE and THERE at one instant shows what is
> established versus assumed, and shrinking HERE until equilibrium rejects the impostor
> shows which total model survives.

Reviewer restatement after viewing: the opening `fire -> smoke` models motivate the
two-level pair; the 2x2 grid and four-case table explain the checks; the smaller-H test
and the fact change select the stable models. The argument is carried by the set
diagrams, not by a voice.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | **pass** | `docs/here-there-explainer-v2-brief.md` |
| 1 Script and cue map | **pass** | `docs/here-there-explainer-v2-script.md`; 31 cues, word-count timing |
| 2 Stills | **pass** | `out/ht-v2-check/` (chapter stills, transition reviews, encoded contact) |
| 3 Draft (muted + sound) | **conditional** | `out/here-there-explainer-v2_draft.mp4` rendered; stills and loudness checked; human watch still required |
| 4 Final technical QA | **pass** | 1920x1080, 3,232 frames, 107.73 s, decode OK, -16.1 LUFS, -2.0 dBTP; SRT and thumbnail present |
| 5 Authored test | **pass** | Mute review, thumbnail at 320x180, reskin, content-derived retiming, clean-machine `list` |
| 6 Craft & coherence | **pass** | Craft 17/18; scenario, information design, text pacing, lecture grammar and spec-to-screen pass |
| 7 Release packaging | **pass** | title, source line, credits, thumbnail, SRT, end-card wordmark; no promo CTA (explainer) |

## 3. Scorecard

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 1 | Conceit | **3** | "A rule should not invent facts" is stated and shown in the first 5 s; the H/T pair and smaller-H test determine the opening, order and ending. |
| 2 | Script / voice | **3** | Complete text-only script with 31 cue-mapped lines, one idea per line; the captions are the declared script. |
| 3 | Evidence | **3** | Every chapter has a set diagram: three classical models, H/T pair, 2x2 state grid, four-case table, p/not-p rows, smaller-H test, two stable models, trace grid. Muted, the argument reads. |
| 4 | Timing | **3** | Scene lengths derive from caption word count at 2.5 words/s; `node->start` is the source of truth; picture, score and SRT share the cue map. |
| 5 | Sound | **2** | Generated pad score with chapter marks and SFX, measured; no voice or ducking because this is the declared text-only cut. |
| 6 | Visual system | **3** | Documented 1920x1080 system: Inter, palette meaning (cyan H, gold T, green pass, coral fail), 96 px margins, caption strip, H/T motif, motion grammar. |
| 7 | Chapter differentiation | **3** | Nine distinct diagrams; no repeated split-card template; the H/T pair is the unifier, not a filler layout. |
| 8 | Data & provenance | **3** | Source ledger and on-screen source line; exact two-atom examples; fact variant labelled "worked example"; temporal diagram labelled "schematic". |
| 9 | Production pipeline | **2** | `c.assets` generates the score and SRT; thumbnail and clean-machine `list` work; voice is intentionally absent, so this is not the full voice pipeline. |
| 10 | Verification | **3** | Stills, transition reviews, draft, final ffprobe/decode, encoded frames, loudness/peak, SRT and thumbnail checked. |
| 11 | Memorability / thumbnail | **3** | The H/T pair is a clear motif; dedicated 1920x1080 thumbnail states "A RULE SHOULD NOT INVENT FACTS" and reads at 320x180. |
| 12 | Specificity | **3** | The diagrams and worked examples are inseparable from HT/equilibrium; a reskin would require a new model system. |

**Authoredness total: 34 / 36 - Improvised.**

### Craft & Coherence (second axis)

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 13 | Scenario coherence | **3** | Signature, models, three states, four checks, two stable models and trace values agree; changing the rule would require a data change. |
| 14 | Information design | **2** | The right proof objects at the right scale; the four-case table and 2x2 grid are dense, and the ch1/ch5 cards have unused space. Labels still read at 1:1. |
| 15 | Text performance & pacing | **3** | Constant 2.5 words/s; spoken words in ink, upcoming at 32%; one line at a time; no glow/box; line holds. |
| 16 | House style & mode grammar | **3** | Sustained lecture grammar: kicker/progress, chapter title, one diagram, caption strip, source line; no promo social card forced in. |
| 17 | Spec-to-screen fidelity | **3** | Briefed examples, exact answer-set checks and cue frames match the rendered film; the fact variant and temporal prefix carry their labels. |
| 18 | Revision quality | **3** | v2 replaces the dense default-font slides with engine typography, a cue map, a score, thumbnail/SRT and a clean transition; review fixes landed. |

**Craft total: 17 / 18 - showcase craft.**
**Two-axis verdict: Authoredness 34/36, Craft 17/18.**

## 4. Critical fails

None.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test**: argument survives without sound | **pass (agent)** | The sets and check marks carry the argument; human watch pending |
| **Thumbnail test**: readable at 320x180, states the conceit | **pass** | `out/ht-v2-check/thumb-320.png`; headline and H/T pair read |
| **Reskin test**: a topic swap requires a redesign | **pass** | The proof is the two-atom H/T/equilibrium example, not a card template |
| **Retiming test**: longer narration moves picture and score | **pass** | Scene lengths derive from `words/2.5`; no voice, but the cue map is one source of truth |
| **Clean-machine test**: `list` works; missing assets behave | **pass** | `list` from `C:\Users\aryan\AppData\Local\Temp\opencode`; score/SRT regenerate in `c.assets` |
| **One idea per screen** | **pass** | Four-case table is the densest; it still has one claim and clear labels |
| **Distinct chapter test** | **pass** | Nine different diagrams; no repeated template |

## 6. Packaging (Gate 7)

- [x] title / credits
- [x] thumbnail (`out/here-there-explainer-v2-thumb.png`)
- [x] SRT (`out/here-there-explainer-v2.srt`)
- [x] wordmark on the end card (secondary)
- [ ] voice (declared text-only cut; optional at the same cue map)
- [x] clean-machine `list` and asset behaviour

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width x height | 1920x1080 | 1920x1080 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frame count | 3,232 | 3,232 | pass |
| Duration | 108 s ± 2 | 107.73 s | pass |
| Full decode | no errors | OK | pass |
| Integrated loudness | -16 LUFS ± 1 | -16.1 LUFS | pass |
| True peak | <= -1 dBTP | -2.0 dBTP | pass |
| Voice sync at chapter starts | n/a (text-only) | cue map only | n/a |
| Encoded-frame check | no banding/text loss | 12 encoded frames + transition midpoint inspected | pass |
| Still vs render determinism | same frame | two stills at 920 byte-identical; key encoded beats visually identical | pass |
| SRT | present | `out/here-there-explainer-v2.srt` | pass |
| Thumbnail | present | `out/here-there-explainer-v2-thumb.png` | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | Sound | 2 | No voice or ducking; the score is a bed with SFX | Add a voice at the cue map and duck the bed; keep text-only as a fallback | author | - |
| Authoredness | Production pipeline | 2 | No voice stage in the pipeline | Include voice files in `c.assets` and the SRT/cue map | author | - |
| Craft | Information design | 2 | Dense four-case table and empty space in ch1/ch5 | Tighten spacing; fill the ch5 left panel with a small H/T diagram | author | - |
| Craft | *(next lowest, 3)* | 3 | No craft blocker; remaining polish is optional | - | - | - |

## 9. Decision

- [ ] **Ship** - release waits on the human muted/sound watch (Gate 3).
- [x] **Preview approved for human review.**
- [ ] **Revise** - no blocking craft/evidence fixes.
- [ ] **Rewrite** - not needed.

Verdict: **Explainer mode. Authoredness 34/36 (Improvised), Craft 17/18 (Showcase
craft); text-only preview; release blocked on the human watch and optional voice.**

## 10. Comparison with v1

| Criterion | v1 | v2 | Change |
| --- | --- | --- | --- |
| Authoredness | 27 | 34 | +7 |
| Craft | 13 | 17 | +4 |
| Combined /54 | 40 | 51 | +11 |

The v2 is not a polish pass: it moves from 720p direct drawing and a fixed 12 s clock to
1920x1080 engine typography, a hook, content-derived cue timing, a generated score,
SRT, a dedicated thumbnail and a clean transition. The H/T diagrams remain the proof.

## 11. Notes for the next author

- **Preserved:** the H/T pair, the 2x2 state grid, the four-case table, the smaller-H
  rejection, and the fact change are the spine; keep them.
- **Nearly failed:** ch3 had a static note under the caption; the ch9 logo appeared before
  the source card; the first score was -19.7 LUFS; the crossfade showed both diagrams.
  All were fixed before the final render.
- **Extract:** the transition-to-background dip and the "scene start + word count" cue
  map are reusable for other text-only explainers.
- **Do not copy:** dense four-case cards as a default; keep the proof type per chapter and
  leave more breathing room around the diagrams.
