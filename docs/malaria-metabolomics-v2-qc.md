# malaria-metabolomics-v2 - QC review

| | |
| --- | --- |
| Composition | `malaria-metabolomics-v2` |
| Commit / build | working tree, `raymotion.exe` built 2026-10-27 |
| Reviewer | author-agent self-review; client/human muted + sound watch pending |
| Date | 2026-10-27 |
| Review stage | final preview |
| Story mode | **explainer / promo-grade** |
| Artifacts | `docs/malaria-metabolomics-v2-brief.md`, `docs/malaria-metabolomics-v2-script.md`, `output/malaria-metabolomics-explainer-v2.mp4`, `output/malaria-metabolomics-explainer-v2.srt`, `output/malaria-metabolomics-explainer-v2-thumb.png`, `out/malaria-v2-encoded-contact.png` |

## 1. One-sentence conceit

> A model that scores brilliantly on its practice test can still fail on data it has never
> seen; one serum sample travels through a paper lab and three 3D specimen windows to show
> the gap.

Reviewer restatement after viewing: the 3D chemistry is the specimen layer, not the
argument. The 2D dot plot and LODO lanes still carry the evidentiary claim.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | **pass** | `docs/malaria-metabolomics-v2-brief.md` |
| 1 Script and cue map | **pass** | `docs/malaria-metabolomics-v2-script.md`; 34 sentence cues |
| 2 Stills | **pass** | `out/malaria-v2-check/`; 3D specimen stills + encoded contact |
| 3 Draft (muted + sound) | **conditional** | `out/malaria-metabolomics-v2_draft.mp4`; human watch pending |
| 4 Final technical QA | **pass** | 1920x1080, 5,455 frames, 181.83 s, decode OK, -16.2 LUFS, -3.8 dBTP |
| 5 Authored test | **pass** | Sentence-level Qwen3-TTS, 3D thumbnail, reskin, retiming, clean-machine `list` |
| 6 Craft & coherence | **pass** | Craft 17/18; scenario, proof, text pacing, mode grammar |
| 7 Release packaging | **pass** | title, source/credits, thumbnail, SRT, no promo CTA |

## 3. Scorecard

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 1 | Conceit | **3** | One serum sample, two exam rooms, one gap; the cold open and candidate payoff are the same idea. |
| 2 | Script / voice | **3** | 34 sentence cues, measured, leading-clap trimmed, captions burned. |
| 3 | Evidence | **3** | 3D molecular specimens, matrix, ledger, PCA, Venn, LODO, dot plot, candidate plates, is/is-not. |
| 4 | Timing | **3** | Beat and sentence lengths derive from measured WAVs; picture, SRT and audio share one table. |
| 5 | Sound | **2** | Voice + sparse SFX, measured; no music/ducking by brief. |
| 6 | Visual system | **3** | Paper lab + 3D specimen windows: Gooch shading, ink lines, cream/ink 2D, documented exceptions. |
| 7 | Chapter differentiation | **3** | Thirteen distinct scenes; 3D is reserved for chemistry, 2D for data, geo maps for place. |
| 8 | Data & provenance | **2** | Numbers from the supplied brief; manuscript PDF absent. S07 55/33/20 overrides prose 35/20/13. |
| 9 | Production pipeline | **3** | 34 voice clips, 3D SDF/SMILES assets, SRT, thumbnail, final render, clean-machine `list`. |
| 10 | Verification | **3** | Stills, 3D plates, draft, final ffprobe/decode, encoded frames, loudness/peak. |
| 11 | Memorability / thumbnail | **3** | 3D specimen + 0.99/0.64 + “THE GAP IS THE FINDING”; readable at 320x180. |
| 12 | Specificity | **3** | The specimen windows and the exam gap are inseparable from the study. |

**Authoredness total: 34 / 36 - Improvised.**

### Craft & Coherence

| # | Criterion | Score (0-3) | Evidence / notes |
| --- | --- | --- | --- |
| 13 | Scenario coherence | **3** | 305 samples, 220 shared, LODO, inner vs held-out, 11 candidates all agree. |
| 14 | Information design | **3** | 3D shows chemistry; 2D dot plot/Venn/LODO show the argument; no 3D chart junk. |
| 15 | Text performance & pacing | **3** | Sentence-level voice, Space Grotesk body captions, word-level karaoke; measured holds. |
| 16 | House style & mode grammar | **3** | yapper/FRAME paper grammar with Archivo/Space Grotesk, controlled 3D + geo exceptions; no promo social card. |
| 17 | Spec-to-screen fidelity | **2** | Source PDF absent; S1P 3D is embedded/approximate and labelled illustrative; country maps are illustrative. |
| 18 | Revision quality | **3** | v2 keeps 13 scenes, adds 3D/NPR, geo maps, yapper table/fonts, and fixes the 3D framing and audio mix. |

**Craft total: 17 / 18 - showcase craft.**
**Two-axis verdict: Authoredness 34/36, Craft 17/18.**

## 4. Critical fails

None, with the release caveat: manuscript unverified, voice pending final approval.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | **pass (agent)** | Burned captions + 3D specimens; human watch pending |
| Thumbnail test | **pass** | `output/malaria-metabolomics-explainer-v2-thumb.png`; 3D molecule + scores |
| Reskin test | **pass** | 3D chemistry/lab and the LODO/dot-plot proof are specific |
| Retiming test | **pass** | 34 measured sentence WAVs drive scene/beat lengths |
| Clean-machine test | **pass** | `list` from `C:\Users\aryan\AppData\Local\Temp\opencode` |
| One idea per screen | **pass** | Thirteen scenes; 3D molecule plates and geo insets are one idea each |
| Distinct chapter test | **pass** | Cover, stakes, 3D specimens, trap, data, cleaning, narrowing, LODO, result, candidates, demo, limits, close |

## 6. Packaging (Gate 7)

- [x] title / source / credits
- [x] 3D thumbnail (`out/malaria-metabolomics-v2-thumb.png`)
- [x] SRT (`out/malaria-metabolomics-v2.srt`)
- [x] source line / closing credits / repo link
- [x] clean-machine `list`
- [ ] final release: manuscript verification + voice approval

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width x height | 1920x1080 | 1920x1080 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frame count | 5,455 | 5,455 | pass |
| Duration | 181.83 s | 181.83 s | pass |
| Full decode | no errors | OK | pass |
| Integrated loudness | -16 LUFS ± 1 | -16.2 LUFS | pass |
| True peak | <= -1.5 dBTP | -3.8 dBTP | pass |
| Voice sync | sentence starts | SRT and audio share measured starts | pass |
| 3D/NPR check | Gooch + ink, no glow | 3D specimen stills inspected | pass |
| Encoded-frame check | no banding/text loss | 39 encoded frames inspected, including maps | pass |
| Still vs render determinism | same frame | two stills at 800 byte-identical | pass |
| SRT | present | 34 cues | pass |
| Thumbnail | present | 1920x1080 | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | Sound | 2 | No music/ducking; voice + SFX only | Keep brief’s no-music choice; final voice + light mix | author | - |
| Authoredness | Data & provenance | 2 | Manuscript PDF absent; S07 conflict documented | Verify against the PDF and publish the source ledger | author | - |
| Craft | Spec-to-screen fidelity | 2 | Source PDF absent; S1P 3D and maps illustrative | Verify structures/numbers; keep illustrative labels | author | - |
| Craft | *(next lowest, 3)* | 3 | No craft blocker | - | - | - |

## 9. Decision

- [ ] **Ship** - blocked by manuscript verification and human watch.
- [x] **Preview approved for client review.**
- [ ] **Revise** - no blocking craft fix; verification and voice approval remain.
- [ ] **Rewrite** - not needed.

Verdict: **Explainer/promo-grade. Authoredness 34/36 (Improvised), Craft 17/18
(Showcase craft); 13-scene 3D/NPR + geography preview; release blocked on manuscript verification and final
voice approval.**

## 10. What changed from v1

- 13 scenes **kept and sharpened**; no longer compressed to 9 beats.
- 2D-only -> **3D specimen windows** on cover, metabolomics and candidates.
- No geography -> **Burkina Faso map** in scene 5 and **Indonesia map** in scene 12, using `geo::`.
- Long VO takes -> **34 sentence-level takes**, leading clap only, no end clap.
- 2D thumbnail -> **3D specimen + 0.99/0.64 gap** thumbnail.
- Inter/mono -> **Archivo Black display + Space Grotesk body**; black yapper table and pink callout.
- v1 remains at `malaria-metabolomics`; v2 is the stronger client preview.

## 11. Review notes

- **Preserved:** the dot plot, LODO lanes, PCA islands, guardrails and the green close.
- **Nearly failed:** S1P started off-frame because its 2D SDF was not centred; it is now
  embedded and box-centred. The cover’s 2D molecule field showed above the 3D plate and
  was removed. The thumbnail’s specimen label collided with the score line and was moved.
  The stereo mix was +3 dB over mono and now uses 0.75 clip gain.
- **3D source:** phenylalanine CID 6140 and retinol CID 445354 are PubChem 3D SDFs; S1P
  CID 5283560 is a 2D SDF/SMILES embedded for preview. The 3D is illustrative, not
  evidence of binding or diagnosis.
- **Do not:** add 3D PCA/Venn/dot plots; the specimen exception only works because the
  data stays 2D.
- **Last-scene review:** the three “next” cards now reveal one by one; “MODEST RESULT.” and
  “HONEST METHOD.” are 136 px and reveal sequentially. The is/is-not rows arrive as
  pairs at 0.15 / 1.55 / 2.95 / 4.35 s inside a 16-sample, 0.5-shutter motion-blur layer,
  so each pair has read time before the next. The three next cards arrive at 0.15 / 1.60 /
  3.05 s with their own motion blur before the green plate. “PROOF OF CONCEPT” is an outlined approval stamp that slams from 2.4x
  to 1x in six frames, with a 14 px decaying shake, two shock rings, a two-frame
  cream/yellow split, and impact + stamp SFX. It slightly overlaps “HONEST METHOD.” The
  stamp move borrows the own_compute Act 2 slam.

- **Geography pass:** scene 5 now carries an illustrative orthographic Africa map with Burkina Faso in pink; scene 12 carries an Equal Earth Indonesia silhouette labelled `INDONESIA / NO DATA HERE`. The maps answer the brief's two locations without turning them into data overlays.
- **Text emphasis pass:** `pfhrp2` is set in pink bold sans with a pink underline against the mono note; candidate `UP` / `DOWN` are 28 px colored arrows, not muted labels; all 34 sentence captions now use word-level karaoke highlighting timed by the measured voice duration.
