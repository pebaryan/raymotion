# malaria-metabolomics - QC review

| | |
| --- | --- |
| Composition | `malaria-metabolomics` |
| Commit / build | working tree, `raymotion.exe` built 2026-10-27 |
| Reviewer | author-agent self-review; client/human muted + sound watch pending |
| Date | 2026-10-27 |
| Review stage | final preview |
| Story mode | **explainer** |
| Artifacts | `docs/malaria-metabolomics-brief.md`, `docs/malaria-metabolomics-script.md`, `out/malaria-metabolomics.mp4`, `out/malaria-metabolomics_draft.mp4`, `out/malaria-metabolomics.srt`, `out/malaria-metabolomics-thumb.png`, `out/malaria-check/` |

## 1. One-sentence conceit

> A model that scores brilliantly on its practice test can still fail on data it has never
> seen; this study tests that honestly and ends with 11 candidate metabolites, not a
> diagnostic.

Reviewer restatement after viewing: the practice-vs-real-exam gap is the finding; the
LODO lane, removed/held-out dot plot, and is/is-not ledger carry the argument. The 220 ->
11 candidate list is explicitly a review list.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | **pass** | `docs/malaria-metabolomics-brief.md`; adapted from the supplied project brief |
| 1 Script and cue map | **pass** | `docs/malaria-metabolomics-script.md`; 34 sentence cues from measured WAVs |
| 2 Stills | **pass** | `out/malaria-check/` chapter stills + encoded contact |
| 3 Draft (muted + sound) | **conditional** | `out/malaria-metabolomics_draft.mp4` rendered; stills and loudness checked; human watch pending |
| 4 Final technical QA | **pass** | 1920x1080, 5,749 frames, 191.63 s, decode OK, -16.2 LUFS, -3.9 dBTP |
| 5 Authored test | **pass** | Qwen3-TTS sentence-level voice (leading clap only), 320x180 thumbnail, reskin, VO-derived retiming, clean-machine `list` |
| 6 Craft & coherence | **pass** | Craft 17/18; scenario, information design, pacing, mode grammar |
| 7 Release packaging | **pass** | title, source/credits, thumbnail, SRT, no promo CTA |

## 3. Scorecard

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 1 | Conceit | **3** | The gap between practice and real exam is visible in the first minute and determines the order, hero scene and close. |
| 2 | Script / voice | **3** | 34 sentence cues VO lines; the supplied script is spoken and burned as captions. |
| 3 | Evidence | **3** | Every scene has a proof object: stat grid, overfit matrix, dataset ledger, PCA islands, Venn, LODO lanes, dot plot, 220 -> 11 funnel, demo UI, is/is-not ledger. |
| 4 | Timing | **3** | Scene lengths derive from measured VO WAVs + holds; picture, SRT and audio use one cue table. |
| 5 | Sound | **2** | Qwen3-TTS clone + sparse SFX, measured; no music/ducking by brief, so the full composed-score anchor is not met. |
| 6 | Visual system | **3** | Documented FRAME system: cream, 6px ink, one shadow, green/pink/yellow/orange semantics, mono labels, Inter fallback. |
| 7 | Chapter differentiation | **3** | 13 distinct scenes, from the molecule field and overfit matrix to the dot plot, funnel, UI, ledger and green plate. |
| 8 | Data & provenance | **2** | Source ledger from the supplied brief; the manuscript PDF is absent, so numbers are not independently verified. S07's prose 35/20/13 is superseded by the numbers sheet 55/20/33. |
| 9 | Production pipeline | **3** | Voice, SRT, thumbnail and final render; clean-machine `list` works; no missing assets. |
| 10 | Verification | **3** | Stills, draft, final ffprobe/decode, encoded frames, loudness/peak, SRT and thumbnail. |
| 11 | Memorability / thumbnail | **3** | "DOES IT TRAVEL?" with practice/real dots; readable at 320x180. |
| 12 | Specificity | **3** | The argument and visuals are inseparable from cross-dataset malaria metabolomics. |

**Authoredness total: 34 / 36 - Improvised.**

### Craft & Coherence (second axis)

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 13 | Scenario coherence | **3** | 305 samples/220 shared; LODO; inner vs held-out; 11 candidates; all numbers agree. |
| 14 | Information design | **3** | Right proof objects at the right scale; the dot plot and Venn carry their arguments; labels legible. |
| 15 | Text performance & pacing | **3** | Voice drives timing; mono captions at 23px; no per-word glow/box; lines hold. |
| 16 | House style & mode grammar | **3** | FRAME-style paper cards, flat fills, one shadow, mono chrome, four accents; Inter is the documented Archivo fallback. |
| 17 | Spec-to-screen fidelity | **2** | The supplied brief/numbers are on screen; the manuscript PDF is not available, the FRAME fonts are fallback, and S07 required a numbers-sheet override. |
| 18 | Revision quality | **3** | Still review fixes landed: dual-mono loudness, S04 stamp overlap, S06 cloud overflow, S09 label collision, S10 direction arrows. |

**Craft total: 17 / 18 - showcase craft.**
**Two-axis verdict: Authoredness 34/36, Craft 17/18.**

## 4. Critical fails

None, with a release caveat: the manuscript PDF is absent, so the numbers are a supplied
brief preview, not a verified final.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test**: argument survives without sound | **pass (agent)** | Burned captions and proof objects; human watch pending |
| **Thumbnail test**: readable at 320x180 | **pass** | `out/malaria-check/thumb-320.png` |
| **Reskin test**: topic swap requires a redesign | **pass** | The LODO/dot plot/funnel proof is specific |
| **Retiming test**: longer VO moves picture and score | **pass** | Scene lengths and SRT derive from the measured WAVs |
| **Clean-machine test**: `list` works | **pass** | `list` from `C:\Users\aryan\AppData\Local\Temp\opencode` |
| **One idea per screen** | **pass** | 13 scenes, one claim each; S04 is the densest but still one trap |
| **Distinct chapter test** | **pass** | No repeated template; the green plate is the only green ground |

## 6. Packaging (Gate 7)

- [x] title / source / credits
- [x] thumbnail (`out/malaria-metabolomics-thumb.png`)
- [x] SRT (`out/malaria-metabolomics.srt`)
- [x] source line on every cream frame
- [x] closing credits, company and repo link
- [x] clean-machine `list`
- [ ] final client release: manuscript PDF verification + voice approval pending

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width x height | 1920x1080 | 1920x1080 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frame count | 5,749 | 5,749 | pass |
| Duration | 191.63 s | 191.63 s | pass |
| Full decode | no errors | OK | pass |
| Integrated loudness | -16 LUFS ± 1 | -16.2 LUFS | pass |
| True peak | <= -1.5 dBTP | -3.9 dBTP | pass |
| Voice sync at chapter starts | within 1 frame | SRT and audio use measured starts | pass |
| Encoded-frame check | no banding/text loss | six encoded frames inspected | pass |
| Still vs render determinism | same frame | two stills at 4000 byte-identical | pass |
| SRT | present | 34 cues | pass |
| Thumbnail | present | 1920x1080 | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | Sound | 2 | No music/ducking by brief; voice + SFX only | Keep the no-music choice; add a final voice take and light SFX mix | author | - |
| Authoredness | Data & provenance | 2 | Manuscript PDF absent; S07 conflict documented | Verify every number against the PDF; update the ledger and S07 | author | - |
| Craft | Spec-to-screen fidelity | 2 | Fonts are fallback; source PDF absent | Load Archivo Black / Space Grotesk and verify the manuscript | author | - |
| Craft | *(next lowest, 3)* | 3 | No craft blocker | - | - | - |

## 9. Decision

- [ ] **Ship** - blocked by the missing manuscript verification and human watch.
- [x] **Preview approved for client review.**
- [ ] **Revise** - no blocking craft fix; verification and voice approval remain.
- [ ] **Rewrite** - not needed.

Verdict: **Explainer mode. Authoredness 34/36 (Improvised), Craft 17/18 (Showcase
craft); client preview; release blocked on manuscript verification and final voice
approval.**

## 10. Notes for the next author

- **Preserved:** the FRAME palette, the 220-column matrix, the PCA islands, the LODO
  lanes, the practice-vs-real dot plot and the 220 -> 11 funnel.
- **Nearly failed:** the stereo mix was +3 dB over the mono VO (fixed with a 0.75 clip
  gain); the S04 stamp overlapped the bottom note; S06 clouds crossed the panel edge;
  S09's averages label collided with the axis; S10 used right arrows instead of up/down.
  All were fixed before the final render.
- **Voice:** Qwen3-TTS clone. Each line was generated with `Clap.` at both ends and
  trimmed between the ASR clap boundaries, then normalized to 48 kHz with ffmpeg. This
  avoids chasing leaked prompt context.
- **Next:** verify the manuscript PDF, replace the clone preview with the approved final
  voice, and get a human watch before calling it a client release.
