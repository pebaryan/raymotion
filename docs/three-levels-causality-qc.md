# Three Levels of Causality — QC review

| | |
| --- | --- |
| Composition | `three-levels-causality` |
| Build | local Release build, 2026-10-27 |
| Reviewer | Codex self-review; second reviewer pending |
| Stage / mode | encoded review cut / explainer |
| Artifacts | `docs/three-levels-causality-brief.md`, `out/three-levels-causality.mp4`, `out/three-levels-causality.srt`, `out/three-levels-causality-thumbnail.png`, `out/three-levels-draft-contact.png` |

## 1. Conceit

One causal thread changes form three times, then a knowledge graph stitches its rule, connected devices and recorded event into a traceable explanation.

After frame-sequence review: the film shows a property relation, a prefab-backed device topology, a timed event trail, then the rule triples, SPARQL topology condition, and named observation/event triples.

## 2. Gates

| Gate | Result | Evidence / remaining work |
| --- | --- | --- |
| 0 Brief | pass (self-reviewed) | Brief includes mode, conceit, proof objects, text-only cue map, sources, visual/audio/asset plan, still list and thumbnail. Second reviewer pending. |
| 1 Script / cue map | pass | `Plan` controls chapter starts, captions, SRT and score cue times; 56 s asserted in code. |
| 2 Stills | pass (self-reviewed) | Opened full-resolution frames 1250/1450 and contact sheets; reviewed frames across chapters and transition samples. The 15 s near-black contact sample is the planned transition fade. Second reviewer pending. |
| 3 Draft, muted + sound | conditional | Inspected all 28 two-second draft samples visually, plus key full-resolution stills. The Windows Computer Use native pipe was unavailable after retry/reset, so a continuous muted and with-sound player watch could not be completed. No listening verdict is claimed. |
| 4 Final technical QA | pass | ffprobe 1920×1080, 30/1 fps, 56.000 s, 1,680 frames; full ffmpeg decode no errors; −19.0 LUFS, −1.5 dBFS true peak; encoded frame 1450 visually checked and compared to still (mean absolute pixel error 1.168). |
| 5 Authored test | conditional | Visual sequence and 320×180 thumbnail communicate the three levels and KG; `list` works from `D:\code`. Continuous mute watch and second reviewer pending. |
| 6 Craft / coherence | conditional | KG IDs/predicates and SPARQL variables spot-checked against 2021 Figures 5–8; 60% scenario threshold labelled; score/audio not perceptually reviewed. Second reviewer pending. |
| 7 Packaging | pass for review cut | Title/source/credits, thumbnail and SRT present. No public release claim while Gates 3, 5, 6 remain conditional. |

## 3. Scores (provisional self-review)

| # | Authoredness criterion | Score | Evidence / gap |
| --- | --- | ---: | --- |
| 1 | Conceit | 3 | Three levels joined by a KG is visible in opening/order/ending. |
| 2 | Script / voice | 3 | Deliberate text-only cue map, short lines. |
| 3 | Evidence | 3 | Each chapter has a diagram, model topology, event mechanism or RDF graph. |
| 4 | Timing | 2 | Shared `Plan` for main cues; a few diagram trace offsets remain local constants. |
| 5 | Sound | 2 | Cue-timed original score and measured encode; perceptual pass pending. |
| 6 | Visual system | 3 | Documented palette, scale, margins, motif, motion; opened stills. |
| 7 | Chapter differentiation | 2 | PBR topology differs; property/event/KG all use graph grammar, with different structures. |
| 8 | Data / provenance | 3 | Figures/algorithms ledger; no invented peak or timestamp. |
| 9 | Pipeline | 2 | Score/SRT through `c.assets`; required committed prefabs; clear missing-asset error. |
| 10 | Verification | 2 | Full technical QA, stills and draft frame sequence; continuous player watch missing. |
| 11 | Thumbnail | 2 | 320×180 closing card readable as a concept; small detail text is not. |
| 12 | Specificity | 3 | RDF predicate graph and smart-grid topology need redesign for another subject. |
| | **Authoredness** | **30/36** | Provisional; no public release without remaining gate work. |

| # | Craft criterion | Score | Evidence / gap |
| --- | --- | ---: | --- |
| 13 | Scenario coherence | 3 | Event order and 60% scenario threshold agree with paper. |
| 14 | Information design | 3 | Named KG nodes and predicate arrows legible at 1:1. |
| 15 | Text pacing | 2 | Caption holds checked by frame sequence; continuous playback pending. |
| 16 | Explainer grammar | 3 | Worked property/device/event examples and source card. |
| 17 | Spec-to-screen | 3 | Figure 5–8 KG identifiers and relations inspected against paper. |
| 18 | Revision quality | 2 | KG extension, closing alpha and audio gain corrected; second review pending. |
| | **Craft** | **16/18** | Provisional. |

No critical fail is known from the inspected artifacts. A perceptual watch could reveal one, so the score does not authorize public release.

## 4. Authored and packaging tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | conditional | 28 sampled draft frames tell rule → devices → events → RDF graph; continuous watch pending. |
| Thumbnail | pass | Frame 1640 from encoded MP4, inspected at 320×180. |
| Reskin | pass | Named RDF predicates, device topology and event observation chain are subject-specific. |
| Retiming | conditional | Chapter starts, SRT and score derive from `Plan`; some local animation offsets would need adjustment if chapter cues change. |
| Clean-machine list | pass | `D:\code\raymotion\build\raymotion.exe list` run from `D:\code`, reports 56 s. |
| One idea / screen | pass | KG rule graph and instance graph are shown sequentially, each with one caption. |
| Distinct chapters | pass | The PBR topology and KG predicate diagrams have different proof objects. |

Packaging: title/source/credits in film; SRT and full-resolution/320px thumbnail files generated. Human or agent second review and continuous playback are outstanding for release.

## 5. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920×1080 | 1920×1080 | pass |
| Frame rate | 30 fps | 30/1 | pass |
| Frames / duration | 1,680 / 56 s | 1,680 / 56.000 s | pass |
| Full decode | no errors | ffmpeg exit 0, no error text | pass |
| Integrated loudness | about −19 LUFS | −19.0 LUFS | pass |
| True peak | ≤ −1 dBFS | −1.5 dBFS | pass |
| Encoded frame | legible, matches still | frame 1450 opened; pixel MAE 1.168 vs still | pass |
| SRT | present | 1,513 bytes | pass |
| Thumbnail | present | encoded frame 1640, 1920×1080 and 320×180 | pass |
| Continuous muted/sound watch | required | unavailable: native pipe failure | pending |

## 6. Decision

**Review cut, conditional QC.** The two weakest areas are (1) sound verification: listen through the full draft with sound and mark cue/transition issues, and (2) independent craft review: inspect the brief, full-resolution stills and MP4 before any public/showcase designation. Keep the conceit, prefab topology and KG predicate graphs.
