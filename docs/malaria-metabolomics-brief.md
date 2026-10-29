# malaria-metabolomics - authoring brief

> Status: **voice preview built** (2026-10-27). The supplied project brief is the source;
> the manuscript PDF and `references/FRAME.md` are not in this workspace. The voice is a
> Qwen3-TTS clone of the user (34 sentence files, leading clap only), mixed with sparse SFX and **no
> music**. Inter/JetBrains Mono are the documented FRAME fallback. Manuscript verification
> and final voice approval are still required.

| | |
| --- | --- |
| Composition | `malaria-metabolomics` |
| Story mode | **explainer** |
| Owner / agent | author-agent |
| Status | `voice preview` |
| Runtime | **191.6 s** (5,749 frames at 30 fps; exact from the 34 measured sentence WAVs) |
| Platform / aspect | 1920x1080, 30 fps, presentation |
| Voice | **Qwen3-TTS cloned voice** (34 sentence clips; **leading clap only**, no end clap). Final approval pending. |
| Audience | internal research team and life-science stakeholders with little ML background |
| Source material | `project/001-malaria-metabolomics-explainer.md`; manuscript reference `resources/manuscript_v1-2026-09-23.pdf` (not in workspace) |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | **explainer** |
| Goal | Explain what the study did, how it protected against fooling itself, what it found, and what it does **not** claim. |
| Proof object | Stat grid, metabolite tiles, overfit matrix, dataset ledger, PCA islands, Venn, LODO lanes, dot plot, 220 -> 11 funnel, demo UI, is/is-not ledger, closing plate. |
| Required evidence | Source ledger below; 13-scene caption script; numbers sheet from the supplied brief. |
| Text | VO lines with an SRT sidecar; burnt captions are optional; the on-screen labels carry the terms. |
| House grammar | FRAME: cream ground, flat ink borders, one hard shadow, mono labels, four accents. |
| Packaging | Source/credits line, thumbnail, SRT; no promo CTA. |

**Missing-source note:** the manuscript PDF is absent. All numbers come from the supplied
brief's numbers sheet; this animatic is not independently verified against the PDF and is
for internal review only.

## 1. One sentence

> This is a film about **cross-dataset malaria metabolomics** that makes the viewer
> **understand why a brilliant practice-test score can collapse on unseen data** by
> **following one honest Leave-One-Dataset-Out test from 220 metabolites to 11 candidates.**

## 2. One-line argument

> The study's contribution is the honest cross-dataset test: tuning scores up to 0.99 AUC,
> but held-out AUC lands at 0.50-0.70, and the 11 surviving metabolites are candidates for
> expert review, not a diagnostic.

## 3. Chapters

The 13 scene lengths are set from 34 separately generated sentence WAVs, a 0.37 s lead-in,
a 0.27 s sentence gap and a short hold. Scene windows: 7.80, 15.93, 13.63, 13.40, 16.27,
17.13, 11.90, 14.20, 25.03, 19.67, 9.53, 14.20, 12.93 s = 191.63 s.

| # | Time (s) | Chapter | On-screen text (short) | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- |
| 1 | 0:00-0:07.8 | Cover | DOES IT TRAVEL? | Title and molecule field | Paper tick, low thud |
| 2 | 0:07.8-0:23.7 | Stakes | 282M cases / 610K deaths / US$5.4B gap | Three stat cells | Three stamp thuds |
| 3 | 0:23.7-0:37.4 | Metabolomics | METABOLOMICS | Serum drop + molecule tiles | Soft pops, whoosh |
| 4 | 0:37.4-0:50.8 | The trap | MEMORISING ISN'T LEARNING. | 220-column matrix + answer-key stamp | Ticking build, wrong thunk |
| 5 | 0:50.8-1:07.0 | The data | 305 samples, 220 shared | Two-dataset ledger | Two slide clicks |
| 6 | 1:07.0-1:24.2 | Cleaning | LOG / PQN / KNN / Z-SCORE | Step cards, histogram, PCA islands | Four ticks, merge whoosh |
| 7 | 1:24.2-1:36.1 | Narrowing | BOTH METHODS MUST PICK | Venn 55 / 20 / 33 | Pops, lock click |
| 8 | 1:36.1-1:50.3 | Honest test | TRAIN ON ONE. TEST ON THE OTHER. | Two lanes + padlock | Padlock, swap whoosh |
| 9 | 1:50.3-2:15.3 | Result | PRACTICE vs REAL EXAM | Dot plot: 0.93-0.99 vs 0.50-0.70 | Rising tone, falling ticks |
| 10 | 2:15.3-2:35.0 | Candidates | 220 -> 11 | Funnel + three candidates + 8 tiles | Squeeze, three ticks |
| 11 | 2:35.0-2:44.5 | Demo | 11 VALUES IN -> MODEL -> WHY | UI mock-up | Two UI ticks |
| 12 | 2:44.5-2:58.7 | Is / is not | WHAT IT IS / WHAT IT IS NOT | Two-column ledger | Checks and soft crosses |
| 13 | 2:58.7-3:11.6 | Next + close | MODEST RESULT. HONEST METHOD. | Step cards + green closing plate | Stamps, resolve |

## 4. Script

Full 13-block caption script and cue map: `docs/malaria-metabolomics-script.md`.
Each sentence is a separate Qwen3-TTS take with a leading clap marker; the clap is trimmed, and the SRT carries the 34 sentence cues.

- 2.5 words/s; mono caption strip at the bottom; 2-3 wrapped lines.
- The 13 blocks are complete; the visuals carry the proof while the captions carry the
  argument.
- No "validated", "diagnostic", "detects malaria", "AI diagnoses malaria" outside the
  explicit "not a diagnostic" statements.

## 5. Sources and claims

All numbers are copied from the supplied brief's numbers sheet. The manuscript PDF is not
in this workspace, so this animatic cannot be a verified release.

| Claim / number | Value | Source | On-screen? |
| --- | --- | --- | --- |
| Cases / deaths, 2024 | 282M / 610K | supplied brief, numbers sheet (manuscript §1) | yes |
| Funding gap | US$3.9B funded vs US$9.3B target = US$5.4B, whole response | supplied brief | yes |
| pfhrp2 deletions | 46 endemic countries | supplied brief | small note |
| Datasets | ST001516 199 obs / ST001517 106 obs | supplied brief, Table 1 | yes |
| Pooled | 305 observations, 220 shared, ~8% missing | supplied brief | yes |
| Batch gap | PC1 19.33 -> 0.04 after ComBat, diagnostic only | supplied brief | yes |
| LASSO/RF overlap | 55 / 20 / 33 (illustrative, ST001516 only) | supplied brief numbers sheet; use this over the scene's 35/20/13 | yes, illustrative |
| LODO fold sets | 20 and 34 | supplied brief §2.4 | small note |
| Final consensus | 11 metabolites | supplied brief, Tables 4-5 | yes |
| Held-out AUC | 0.499-0.699; LR 0.646/0.638; XGB 0.569/0.647; RF 0.499/0.633 | supplied brief, Table 6 | dot plot |
| Means | LR 0.642, XGB 0.608, RF 0.566 (descriptive, 2 folds) | supplied brief | dot labels |
| Tuned outer AUC | XGB 0.587/0.699 (0.643); RF 0.574/0.644 (0.609) | supplied brief | dot plot |
| Inner-CV / gap | 0.931-0.996 / 0.293-0.357 | supplied brief | dot plot |
| Top coefficients | Phe +1.214; S1P -1.121; retinol -0.944 | supplied brief, Table 5 | candidate cells |

**Conflict resolved:** the S07 prose says 35 / 20 / 13, while the numbers sheet says
55 LASSO / 33 RF / 20 shared. The numbers sheet is the source of truth; the Venn is
labelled `ILLUSTRATIVE: ONE TRAINING DATASET` and the final pooled set is 11.

**Guardrails:** held-out is the headline; inner-CV is always labelled optimistic; the
cohorts are West African children with repeat visits; the fact example is not a biomarker
panel or diagnostic; ComBat was diagnostic only; the 20:1 is feature reduction, not a
20-fold performance gain; the top-5 SHAP lists differed between folds.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | Cream `#F2E9D6`; ink `#151515`; green `#3FAE6A` = training/learned/what it is; pink `#EF5B7B` = held-out/real exam/what it is not; yellow `#F3C64E` = annotation/warning; orange `#E76F51` = hard shadow/big claim; muted `#7A7468`. |
| Type | Inter Bold uppercase (FRAME's Archivo Black is not in `assets/fonts`; documented fallback) for headlines; JetBrains Mono for labels, tables, captions, source. |
| Layout | 1920x1080; 64px margins; topbar 42; content 120-880; caption 900-1000; footer 1030. |
| Borders | 6px ink; zero radius except one pill; one hard-offset shadow per frame (12px orange). |
| Motion | 12-18 frame slides/stamps; hard cuts at scene boundaries; no idle motion beyond one schematic bar/cloud. |
| Motif | Flat paper cards, mono labels, one accent per idea; green/pink contrast for practice vs real exam. |
| Captions | 26px JetBrains Mono, 2-3 lines, bottom; 2.5 words/s word reveal; no box/glow. |
| Green ground | Closing plate only. |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | Qwen3-TTS clone, 34 sentence files, -16 LUFS normalized; leading clap only. |
| Music | **None** (per brief). |
| SFX | Shared paper ticks, soft pops, stamp thuds, padlock click, whooshes at the cue map; no bed. |
| Loudness | -16 LUFS integrated / <= -1.5 dBTP. |
| Silence | Sparse by design; voice + SFX only, no music. |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback |
| --- | --- | --- | --- |
| SFX | shared committed clips | `assets/audio/austria/` | skip SFX; voice remains |
| Voice | committed Qwen3-TTS WAVs | `assets/vo/malaria-sentences-norm/` | silence; SRT still works |
| SRT | generated in `c.assets` | `out/malaria-metabolomics.srt` | derive from the caption table |
| Thumbnail | generated still | `out/malaria-metabolomics-thumb.png` | still at frame 30 |
| Fonts | Inter + JetBrains Mono | `assets/fonts` | documented fallback for Archivo/Space Grotesk |

Clean-machine: `list` works anywhere; SFX/SRT regenerate; no external images.

## 9. Cue map

| Frame | Picture | Sound |
| --- | --- | --- |
| 0 | Cover | Paper tick |
| 234 | Stat grid | Three thuds |
| 712 | Molecule tiles | Pops/whoosh |
| 1121 | Overfit matrix | Ticking build/wrong thunk |
| 1523 | Dataset ledger | Two slide clicks |
| 2011 | Cleaning/PCA | Four ticks/merge whoosh |
| 2525 | Venn | Pops/lock |
| 2882 | LODO lanes | Padlock/swap |
| 3308 | Dot plot | Rise/fall ticks |
| 4059 | Candidates | Squeeze/three ticks |
| 4649 | Demo UI | Two UI ticks |
| 4935 | Is/is not | Checks/crosses |
| 5361 | Next/closing | Stamps/resolve |

## 10. Still frames

90, 450, 900, 1250, 1700, 2250, 2750, 3200, 3800, 4600, 5000, 5250, 5680, 5750.
Plus one frame from each scene's middle and encoded frames from the final.

## 11. Acceptance

- [ ] Muted, the 13 scenes read as a story.
- [ ] Every scene has its proof object visible at the stated frame.
- [ ] Numbers match the supplied numbers sheet; conflicts documented.
- [ ] No forbidden claim; "not a diagnostic"/"not new" appear.
- [ ] Captions inside safe area; one rate.
- [ ] `list` works; committed voice/SFX resolve; SRT regenerates.
- [ ] Duration, frame count, decode, loudness/peak recorded.
- [ ] Thumbnail readable.
- [ ] Voice is the approved final take; manuscript numbers are verified.

## 12. Risks / non-goals

- **Risks:** missing manuscript PDF; missing FRAME.md/fonts; text-only cut may be harder to
  follow; S07 numbers conflict; no approved voice.
- **Non-goals:** not a final diagnostic claim, not a verified release, not a temporal or
  full methods talk.
- **Out of scope:** music, manuscript re-verification, exact FRAME font matching.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | author-agent self-review | 2026-10-26 | approved as animatic |
| Script / cue map | author-agent self-review | 2026-10-26 | approved as animatic |
| Stills | pending | - | - |
| Draft | pending | - | - |
| Final QC | pending | - | - |
