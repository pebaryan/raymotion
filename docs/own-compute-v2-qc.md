# own-your-compute-v2 — QC review (voiceless preview)

| | |
| --- | --- |
| Composition | `own-your-compute-v2` |
| Build | `out/own-your-compute-v2.mp4` |
| Reviewer | author-agent self-review; human watch pending |
| Date | 2026-10-26 |
| Stage | voiceless preview |
| Story mode | promo |
| Artifacts | `docs/own-compute-v2-brief.md`, `docs/own-compute-v2-script.md`, `out/own-your-compute-v2.mp4`, `out/own-your-compute-v2_draft.mp4`, `out/oyc-v2-check/`, `out/own-your-compute-v2-thumb.png` |

## 1. One-sentence conceit

> A $92.97 cloud bill becomes a machine on a desk, an honest break-even chart, a map
> where the prompt leaves the room, and a house under a dome: the real difference is
> who holds the box.

Reviewer restatement after viewing: the film reads as a first-person ownership essay
with two proof beats (the chart and the map), not a feature tour.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | `docs/own-compute-v2-brief.md` |
| 1 Script and cue map | pass | `docs/own-compute-v2-script.md`; text-only cut uses the same cue map |
| 2 Stills | pass | 10 stills + contact sheet in `out/oyc-v2-check/`; chart, map, terminal and title inspected at 1:1 |
| 3 Draft (muted + sound) | conditional | `out/own-your-compute-v2_draft.mp4` rendered; stills and loudness checked; human muted/sound watch still required |
| 4 Final technical QA | conditional | 1920×1080, 2,820 frames, 94.0 s, decode OK, -16.3 LUFS, -1.5 dBTP; SRT and a dedicated thumbnail comp are not generated yet |
| 5 Authored test | conditional | Mute test, reskin, clean machine and distinct-chapter pass; retiming test and thumbnail test are pending (voice not attached) |
| 6 Craft & coherence | pass | Craft 17/18; scenario, information design, text pacing, promo grammar, spec-to-screen and revision quality all pass |
| 7 Release packaging | conditional | logo/social/CTA present; SRT and a dedicated thumbnail comp are missing |

## 3. Scorecard

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | The bill → machine → chart → map → dome arc is specific and visible from the first shot. |
| 2 | Script / voice | 3 | Full first-person script, one idea per line, text-only preview, cue-mapped; karaoke words light up at a constant 2.5 words/s in the caption accent, upcoming words dim, no glow or box. Voice files are the next step. |
| 3 | Evidence | 3 | The chart proves the break-even, the map proves the route vs the dome, the terminal proves the offline answer, and the GPU/model are physical proof objects. Muted, the argument still reads. |
| 4 | Timing | 2 | Fixed target act boundaries (`A1..A9`) are internally consistent and equal 2,820 frames; the scene lengths are not yet derived from measured voice. One source of truth; captions and score use the same acts. |
| 5 | Sound | 3 | Composed score with act cues: cold pads, glitch stabs, riser/impact, groove, cost ticks, distance drone, near-silence, D-major lift. Measured -16.3 LUFS, -1.5 dBTP. No voice/duck in this preview; the score is a deliberate text-only bed. |
| 6 | Visual system | 3 | Documented palette, type, letterbox, motif (the meter) and motion grammar; the raymotion badge intro, Atlantic map, karaoke captions and social card inherit the same system. |
| 7 | Chapter differentiation | 3 | Nine distinct acts: badge intro, aisle, glitch, chip, GPU, seven-layer repeated-block network, chart, Atlantic map, terminal, island/social card. No filler. |
| 8 | Data & provenance | 3 | Source ledger in the script; Qwen3.8 27B (27 GB), 32 GB / 300 W scenario, modeled bill, 300 W chart arithmetic, Vienna → US route labelled illustrative. |
| 9 | Production pipeline | 2 | `c.assets` generates PCB, wood, rack and score; `list` works from any directory. Voice, SRT and a dedicated thumbnail comp are missing. |
| 10 | Verification | 3 | Ten stills + contact sheet, draft render, final ffprobe, full decode, loudness/true peak, encoded-frame contact sheet; still/encoded frames at 2 s match. |
| 11 | Memorability / thumbnail | 2 | Strong chart/map/island images; a thumbnail candidate exists at 1:30, but no dedicated thumbnail composition. |
| 12 | Specificity | 3 | The chart, map, terminal and island are specific to the local-AI ownership story; a topic swap would require a redesign. |

**Authoredness total: 33 / 36 — Improvised (preview).**

### Craft & Coherence (second axis)

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 13 | Scenario coherence | 3 | Qwen3.8 27B sits in 27 GB; 32 GB VRAM / 300 W; the chart is recomputed at 300 W; names and labels match the ledger. |
| 14 | Information design | 3 | Chart at the right scale with units and break-even; Atlantic map with separated labels, route, dome and source notes. |
| 15 | Text performance & pacing | 3 | Constant 2.5 words/s, independent of scene budgets; captions-example colour light-up; no glow/box; line holds after the last word. |
| 16 | House style & mode grammar | 3 | The promo grammar is sustained: raymotion badge intro, latest promo social row/handles, and captions-example karaoke. The logo/social kit is also checked in Gate 7. |
| 17 | Spec-to-screen fidelity | 2 | Current artifact matches the stated Qwen model, 32 GB/300 W, 3.9 h break-even and 4-8-8-8-8-8-3 network; the earlier network mismatch was caught only after review. |
| 18 | Revision quality | 3 | All six feedback items landed, the cost chart and caption pacing consequences were updated, and the final render was re-checked. |

**Craft total: 17 / 18 — showcase craft.**
**Two-axis verdict: Authoredness 33/36, Craft 17/18.**

The 36-point score stays 33 because the old rubric was saturated; the 18-point craft axis
moved from 7/18 (v1) to 17/18 (v2). That is the improvement the old score could not see.

## 4. Critical fails

None. The only central numbers are sourced or labelled; the chart arithmetic was checked.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | pass (agent review) | Captions + chart/map carry the argument; encoded contact sheet reviewed; human watch pending |
| Thumbnail test | conditional | 1:30 island/dome candidate is readable at feed size; no dedicated thumb comp yet |
| Reskin test | pass | The chart/map/terminal form is inseparable from the local-AI story |
| Retiming test | pending | Voice not attached; current act lengths are target times |
| Clean-machine test | pass | `list` works; assets generate into `out/oyc-v2-assets/` |
| One idea per screen | pass | Chart = break-even; map = route vs dome; terminal = offline answer |
| Distinct chapter test | pass | Nine acts, no repeated template |

## 6. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920×1080 | 1920×1080 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frame count | 2,820 | 2,820 | pass |
| Duration | 94.0 s ± 2 | 94.0 s | pass |
| Full decode | no errors | OK | pass |
| Integrated loudness | -16 LUFS ± 1 | -16.3 LUFS | pass |
| True peak | ≤ -1 dBTP | -1.5 dBTP | pass |
| Encoded-frame check | no banding/text loss | contact sheet inspected | pass |
| Still vs render | same frame | 2 s still and encoded frame match | pass |
| SRT | present for release | not yet | pending |
| Thumbnail comp | present for release | candidate frame only | pending |

## 7. Comparison with v1

| Criterion | v1 | v2 | Change |
| --- | --- | --- | --- |
| 1 Conceit | 3 | 3 | — |
| 2 Script / voice | 2 | 3 | +1 (full text-only script + cue map) |
| 3 Evidence | 3 | 3 | — |
| 4 Timing | 2 | 2 | — (voice derivation pending) |
| 5 Sound | 3 | 3 | — (new cues, fixed peak) |
| 6 Visual system | 2 | 3 | +1 (documented + chart/map) |
| 7 Chapter differentiation | 3 | 3 | — |
| 8 Data & provenance | 2 | 3 | +1 (source ledger + labels) |
| 9 Production pipeline | 2 | 2 | — (voice/SRT/thumb pending) |
| 10 Verification | 2 | 3 | +1 (final QA + encoded frames) |
| 11 Memorability / thumbnail | 2 | 2 | — (candidate only) |
| 12 Specificity | 3 | 3 | — |
| **Authoredness total** | **29** | **33** | **+4** |
| **Craft total** | **7** | **17** | **+10** |

The improvement is not a longer film. It is the two proof beats (chart and Atlantic map),
the captions-example karaoke (constant 2.5 words/s, no glow), the raymotion badge intro,
the social card, the source ledger, the documented visual system, the repeated-block
network, and the final QA that the v1 lacked. The requested content refinements are in:
27 GB Qwen3.8 27B, 32 GB VRAM / 300 W, break-even ~3.9 h/day, and Vienna → a US cloud
region. The remaining gap is the voice and the timing it will drive.

## 8. Decision

- [x] **Preview approved for human review.**
- [ ] **Ship** — Authoredness 33 and Craft 17 clear the craft gate, but Gate 7 packaging is conditional (SRT/thumbnail) and release is blocked on voice, retiming and a human muted/sound watch.
- [ ] **Rewrite** — not needed.

Verdict: **Promo mode. Authoredness 33/36 (Improvised), Craft 17/18 (Showcase craft), voiceless preview; Gate 7 packaging conditional (SRT/thumbnail); release blocked on voice and the human watch.**

## 9. Next steps

1. Generate the nine voice files; measure them; derive the act lengths and re-time the
   cue map and the karaoke word highlight. Re-run the retiming test.
2. Duck the score under the voice; re-measure loudness/peak.
3. Generate the SRT and a dedicated thumbnail composition.
4. Human watch: muted, then with sound; mark any cue that lands late.
5. Re-run this QC; Authoredness stays 33 for this preview, while the Craft score (17/18) is what moved on the feedback. The release still waits on voice, SRT, thumbnail and the human watch.
