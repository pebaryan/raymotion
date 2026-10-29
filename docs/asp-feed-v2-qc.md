# asp-feed-v2 — QC review

| | |
| --- | --- |
| Composition | `asp-feed-v2` |
| Story mode | **explainer** |
| Build | `out/asp-feed-v2.mp4` |
| Reviewer | author-agent self-review; human watch pending |
| Date | 2026-10-26 |
| Stage | text-only preview |
| Artifacts | `docs/asp-feed-v2-brief.md`, `docs/asp-feed-v2-script.md`, `out/asp-feed-v2.mp4`, `out/asp-feed-v2_draft.mp4`, `out/asp-feed-v2-thumb.png`, `out/asp-feed-v2.srt`, `out/asp-v2-check/` (incl. `encoded-end.png`) |

## 1. One-sentence conceit

> ASP flips the job: describe what a valid solution looks like, and the solver returns
> every answer set; coloring a four-region map proves it — two colorings, then one
> constraint leaves one.

Reviewer restatement after viewing: the step stack becomes a map, the map yields two
exact answer sets, and the constraint removes one. The argument is carried by the map,
not by the captions.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | `docs/asp-feed-v2-brief.md` |
| 1 Script and cue map | pass | `docs/asp-feed-v2-script.md`; text-only cut, cue-mapped |
| 2 Stills | pass | 10 stills + final contact sheet in `out/asp-v2-check/` |
| 3 Draft (muted + sound) | conditional | `out/asp-feed-v2_draft.mp4` rendered; stills and loudness checked; human watch still required |
| 4 Final technical QA | pass | 1080×1920, 1,350 frames, 45.0 s, decode OK, -16.9 LUFS, -1.2 dBTP; SRT and thumbnail present |
| 5 Authored test | conditional | Mute, reskin, clean machine, thumbnail and distinct-chapter pass; voice/retiming test pending (text-only first cut) |
| 6 Craft & coherence | pass | Craft 17/18; scenario, information design, text pacing, explainer grammar, spec-to-screen and revision pass |
| 7 Release packaging | pass | explainer title/source/credits, thumbnail, SRT; 2x wordmark at 3/4 height |

## 3. Authoredness scorecard

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | One map, two colors, two answer sets, one constraint. The hook and the proof are the same idea. |
| 2 | Script / voice | 3 | Full text-only script with a cue map; one idea per line; no voice yet by design. |
| 3 | Evidence | 3 | The map and answer-set mini-maps are the proof; the constraint removes the red-A map. Muted, the argument reads. |
| 4 | Timing | 2 | Scene starts are exact; captions use one constant rate and the cue map. No voice-derived timing yet. |
| 5 | Sound | 2 | Generated score with section cues and SFX (ticks, two solution chimes, cross, end lift); no voice/duck in this cut. |
| 6 | Visual system | 3 | Documented vertical system: kicker/progress, region tile, answer-set maps, red/blue semantics, source line. |
| 7 | Chapter differentiation | 3 | Nine distinct beats: step stack, shift, map, facts, rules, two answer sets, constraint, uses, end card. No filler. |
| 8 | Data & provenance | 3 | Source line (ASP/clingo, Potassco); exact 2×2 enumeration; “two” is true for this instance only. |
| 9 | Production pipeline | 2 | Score, SRT and thumbnail generated/committed; voice is the only missing piece. |
| 10 | Verification | 3 | Stills, draft, final ffprobe/decode, loudness/peak, encoded frames and determinism checked. |
| 11 | Memorability / thumbnail | 3 | The two answer maps are a clear motif; the dedicated thumbnail is readable at feed size. |
| 12 | Specificity | 3 | The map, answer sets and constraint are inseparable from the ASP story. |

**Authoredness total: 33 / 36 — Improvised.**

## 4. Craft & Coherence

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 13 | Scenario coherence | 3 | Four regions, four borders, two colors; exactly two valid colorings; the constraint leaves exactly one. |
| 14 | Information design | 2 | The map and answer maps are clear; the first scene is sparse and the fact chips are small at feed size. |
| 15 | Text performance & pacing | 3 | Constant 2.5 words/s; spoken words in the accent, upcoming dim; no glow/box; the line holds. |
| 16 | House style & mode grammar | 3 | Explainer grammar: top kicker/progress, region-tile motif, source line, prominent 3/4-height wordmark. No promo social card forced in. |
| 17 | Spec-to-screen fidelity | 3 | The colorings and constraint match the enumerated answer sets; the source line matches the ledger. |
| 18 | Revision quality | 3 | The v2 replaces the generic cards with a real proof; pacing, score, packaging and thumbnail were updated together. |

**Craft total: 17 / 18 — showcase craft.**
**Two-axis verdict: Authoredness 33/36, Craft 17/18.**

## 5. Critical fails

None. The map instance is exact and the “two” claim is limited to the instance.

## 6. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | pass (agent review) | The map → two answer sets → one constraint reads without sound; human watch pending |
| Thumbnail test | pass | `out/asp-feed-v2-thumb.png`; two answer maps + title at feed size |
| Reskin test | pass | A different topic would require a new proof, not a text swap |
| Retiming test | pending | Text-only first cut; voice can be attached at the cue map |
| Clean-machine test | pass | `list` works from another directory; score/SRT generate |
| One idea per screen | pass | Map = problem; two maps = answer sets; cross = constraint |
| Distinct chapter test | pass | Nine distinct beats |

## 7. Packaging (Gate 7)

- [x] explainer title, source line and credits
- [x] thumbnail (`out/asp-feed-v2-thumb.png`)
- [x] SRT (`out/asp-feed-v2.srt`)
- [x] wordmark (2× size, 3/4 height; present)
- [ ] voice (text-only first cut; pending)

## 8. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1080×1920 | 1080×1920 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frame count | 1,350 | 1,350 | pass |
| Duration | 45.0 s ± 2 | 45.0 s | pass |
| Full decode | no errors | OK | pass |
| Integrated loudness | -16 LUFS ± 1 | -16.9 LUFS | pass |
| True peak | ≤ -1 dBTP | -1.2 dBTP | pass |
| Encoded-frame check | no banding/text loss | encoded end frame at 44.0 s inspected after resize | pass |
| Still vs render | same frame | checked at the key beats | pass |
| SRT | present | `out/asp-feed-v2.srt` | pass |
| Thumbnail | present | `out/asp-feed-v2-thumb.png` | pass |

## 9. Comparison with v1

| Criterion | v1 | v2 | Change |
| --- | --- | --- | --- |
| Conceit | 2 | 3 | +1 |
| Script / voice | 2 | 3 | +1 |
| Evidence | 2 | 3 | +1 |
| Timing | 1 | 2 | +1 |
| Sound | 1 | 2 | +1 |
| Visual system | 2 | 3 | +1 |
| Chapter differentiation | 2 | 3 | +1 |
| Data & provenance | 1 | 3 | +2 |
| Production pipeline | 1 | 2 | +1 |
| Verification | 1 | 3 | +2 |
| Memorability / thumbnail | 1 | 3 | +2 |
| Specificity | 1 | 3 | +2 |
| **Authoredness** | **17** | **33** | **+16** |
| **Craft** | **9** | **17** | **+8** |
| **Combined /54** | **26** | **50** | **+24** |

The v2 is a different film, not a polish pass: a real proof, a source line, a script/cue
map, a composed score, a dedicated thumbnail and an explainer end card replace the generic
farmer/cat cards.

## 10. Decision

- [x] **Preview approved for human review.**
- [ ] **Ship** — text-only cut is complete; release still waits on a human muted/sound
  watch and, if wanted, a voice at the same cue map.
- [ ] **Rewrite** — not needed.

Verdict: **Explainer mode. Authoredness 33/36 (Improvised), Craft 17/18 (Showcase craft);
text-only preview; release blocked on human watch and optional voice.**

## 11. Next steps

1. Watch muted, then with sound; mark any caption that highlights too fast or too slow.
2. If a voice is wanted, generate nine files and drive the karaoke from word timings at
   the same cue map; then re-run the retiming test.
3. Re-check the final from encoded frames after any voice/score change.

## 10. Post-QC change

After the first final render, the end-card wordmark was doubled from size 52 to 104 and
moved from y=1130 to y=1440 (3/4 of the 1920 px height). The final MP4 was re-rendered
and the encoded end frame was re-checked; the wordmark now sits clear of the main
content and the footer. Scores are unchanged.
