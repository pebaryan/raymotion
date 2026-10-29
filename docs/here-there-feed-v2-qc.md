# here-there-feed-v2 - QC review

| | |
| --- | --- |
| Composition | `here-there-feed-v2` |
| Commit / build | working tree, `raymotion.exe` built 2026-10-26 |
| Reviewer | author-agent self-review; human muted/sound watch pending |
| Date | 2026-10-26 |
| Review stage | final |
| Story mode | **explainer** |
| Artifacts | `docs/here-there-feed-v2-brief.md`, `docs/here-there-feed-v2-script.md`, `out/here-there-feed-v2.mp4`, `out/here-there-feed-v2_draft.mp4`, `out/here-there-feed-v2.srt`, `out/here-there-feed-v2-thumb.png`, `out/ht-feed-v2-check/` |

## 1. One-sentence conceit

> A smoke-only guess passes the rule but is not backed up: keeping the guess fixed while
> removing smoke from HERE shows the gap, and adding the fact fire shows what survives.

Reviewer restatement after viewing: the GUESS becomes THERE, HERE starts empty, both rule
checks pass, the removed-smoke model is rejected, and the given fact makes fire and smoke
both necessary. The paper islands carry the argument; the captions explain the move.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | **pass** | `docs/here-there-feed-v2-brief.md` |
| 1 Script and cue map | **pass** | `docs/here-there-feed-v2-script.md`; 12 cues, word-count timing |
| 2 Stills | **pass** | `out/ht-feed-v2-check/` (chapter stills and encoded contact) |
| 3 Draft (muted + sound) | **conditional** | `out/here-there-feed-v2_draft.mp4` rendered; stills and loudness checked; human watch still required |
| 4 Final technical QA | **pass** | 1080x1350, 1,794 frames, 59.8 s, decode OK, -16.9 LUFS, -2.0 dBTP; SRT and thumbnail present |
| 5 Authored test | **pass** | Mute review, 320x400 thumbnail, reskin, derived retiming, clean-machine `list` |
| 6 Craft & coherence | **pass** | Craft 17/18; exact H/T story, readable paper proof, constant text rate |
| 7 Release packaging | **pass** | source/credits line, thumbnail, SRT, end wordmark; no promo CTA (explainer) |

## 3. Scorecard

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 1 | Conceit | **3** | "A guess is not a proof" is stated in the first caption and proved by the removal. The paper islands, pin and fact badge determine the order. |
| 2 | Script / voice | **3** | Complete text-only script, 12 cue-mapped lines at 2.5 words/s; one idea per line. |
| 3 | Evidence | **3** | Every chapter has a visible proof: GUESS/empty facts, H/T islands, rule ribbons, removed smoke, fact removal checks. Muted, the story reads. |
| 4 | Timing | **3** | Scene and line lengths derive from caption words; `ST`/`DUR` are the only clock; score and SRT read the same table. |
| 5 | Sound | **2** | Generated paper-tick score, checks and stamps; no voice or ducking because this is the declared text-only cut. |
| 6 | Visual system | **3** | Documented paper-cut system: palette meaning (amber THERE, teal HERE, coral unsupported), type, margins, islands, rule ribbons, motion. |
| 7 | Chapter differentiation | **3** | Six distinct beats: guess, unfold, both checks, removal, fact, close; the stage evolves rather than repeating a card. |
| 8 | Data & provenance | **3** | Source ledger and source line; exact two-atom model; fact variant labelled "worked example"; scope limited on screen. |
| 9 | Production pipeline | **2** | `c.assets` generates score and SRT; thumbnail and clean-machine `list` work; voice intentionally absent. |
| 10 | Verification | **3** | Stills, draft, final ffprobe/decode, encoded frames, loudness/peak, SRT and thumbnail checked. |
| 11 | Memorability / thumbnail | **3** | "NOT CONTRADICTED. NOT BACKED UP." with the two paper islands; readable at 320x400. |
| 12 | Specificity | **3** | The H/T islands, removed smoke and given fact are inseparable from HT; a reskin would require new objects. |

**Authoredness total: 34 / 36 - Improvised.**

### Craft & Coherence (second axis)

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 13 | Scenario coherence | **3** | Guess H={}, T={smoke}; removed smoke; fact fire; all checks agree with `here_there.hpp`. |
| 14 | Information design | **2** | The proof is legible at feed size; the rule-bar icons sit close to the world tokens and the removed smoke has no tray, so the first read needs the captions. |
| 15 | Text performance & pacing | **3** | Constant 2.5 words/s; current words ink, upcoming 32%; no glow/box; line holds. |
| 16 | House style & mode grammar | **3** | Paper-cut feed explainer: one question/beat, two islands, rule ribbon, source line; no promo CTA. |
| 17 | Spec-to-screen fidelity | **3** | Caption frames, checks and removal states match the brief and source ledger. |
| 18 | Revision quality | **3** | v2 replaces the 88 s preview-only cut with a 59.8 s cue-mapped film, score, SRT, thumbnail and a clean stage; review fixes landed. |

**Craft total: 17 / 18 - showcase craft.**
**Two-axis verdict: Authoredness 34/36, Craft 17/18.**

## 4. Critical fails

None.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test**: argument survives without sound | **pass (agent)** | The empty HERE, removed smoke and fact badge carry the turn; human watch pending |
| **Thumbnail test**: readable at 320x400 | **pass** | `out/ht-feed-v2-check/thumb-320.png`; title and islands read |
| **Reskin test**: topic swap requires a redesign | **pass** | The paper islands and fact/removal proof are specific |
| **Retiming test**: longer narration moves picture and score | **pass** | Durations derive from word counts; one cue table |
| **Clean-machine test**: `list` works; missing assets behave | **pass** | `list` from `C:\Users\aryan\AppData\Local\Temp\opencode`; score/SRT regenerate |
| **One idea per screen** | **pass** | Each beat has one question; the islands are the proof |
| **Distinct chapter test** | **pass** | Six different states, not a repeated card |

## 6. Packaging (Gate 7)

- [x] source / credits line
- [x] thumbnail (`out/here-there-feed-v2-thumb.png`)
- [x] SRT (`out/here-there-feed-v2.srt`)
- [x] wordmark on the closing beat
- [ ] voice (declared text-only cut; optional at the same cue map)
- [x] clean-machine `list` and asset behaviour

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width x height | 1080x1350 | 1080x1350 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frame count | 1,794 | 1,794 | pass |
| Duration | 59.8 s | 59.8 s | pass |
| Full decode | no errors | OK | pass |
| Integrated loudness | -16 LUFS ± 1 | -16.9 LUFS | pass |
| True peak | <= -1.5 dBTP | -2.0 dBTP | pass |
| Voice sync | n/a (text-only) | cue map only | n/a |
| Encoded-frame check | no banding/text loss | five encoded frames inspected | pass |
| Still vs render determinism | same frame | two stills at 620 byte-identical | pass |
| SRT | present | `out/here-there-feed-v2.srt` | pass |
| Thumbnail | present | `out/here-there-feed-v2-thumb.png` | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | Sound | 2 | No voice or ducking; the score is a paper-tick bed | Add a voice at the cue map and duck; keep text-only as fallback | author | - |
| Authoredness | Production pipeline | 2 | No voice stage | Include voice files in `c.assets` and the SRT | author | - |
| Craft | Information design | 2 | Rule-bar icons can read as world tokens; removed smoke has no tray | Add a small RULE label (done), a tray for the removed smoke, and more contrast for HERE-empty | author | - |
| Craft | *(next lowest, 3)* | 3 | No craft blocker | - | - | - |

## 9. Decision

- [ ] **Ship** - release waits on the human muted/sound watch (Gate 3).
- [x] **Preview approved for human review.**
- [ ] **Revise** - no blocking craft/evidence fixes.
- [ ] **Rewrite** - not needed.

Verdict: **Explainer mode. Authoredness 34/36 (Improvised), Craft 17/18 (Showcase
craft); text-only preview; release blocked on the human watch and optional voice.**

## 10. Comparison with the v1 feed

The v1 feed was an 88 s, 1080x1350 preview-only cut with no audio, SRT, thumbnail or QC.
The v2 is 59.8 s, cue-mapped at 2.5 words/s, with a generated score, SRT, thumbnail and a
cleaner proof. It keeps the paper-cut islands and the smoke-only guess, but makes the
removal and the given fact the spine.

## 11. Notes for the next author

- **Preserved:** the paper-cut islands, cut-paper flame/smoke, the smoke-only guess and the
  given-fact badge. The H/T pair is the visual language.
- **Nearly failed:** the thumbnail initially put the smoke in HERE (swapped masks); the
  first score peaked at -1.1 dBTP; the rule bar icons were ambiguous. All were fixed
  before the final render.
- **Extract:** the "guess -> two views -> remove -> fact" beat table is reusable for other
  logic feed explainers.
- **Do not copy:** dense rule bars with unlabeled icons; a removed token should land in a
  visible tray, not just drift into the gutter.
