# The Overload Has a Backstory — QC review

| | |
| --- | --- |
| Composition | `expcps-overload` |
| Build | `build/raymotion.exe`, roadside-transformer and stepped-connector revision, 2026-10-27 |
| Reviewer | Codex |
| Review stage | final technical and visual review; auditory sign-off pending |
| Story mode | explainer |
| Artifacts | `docs/expcps-2021-explainer-brief.md`; `assets/data/expcps/flex.city.json`; `out/expcps-overload-final.mp4`; `out/expcps-overload.srt`; `out/expcps-overload-thumbnail.png`; encoded contact sheet `out/expcps-v5-qc-sheet.png` |

## 1. One-sentence conceit

A transformer alarm acquires a backstory as two trails cross a simulated village and a knowledge graph turns rules and prior events into a candidate explanation. The rendered scenes show this progression.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief written and user approved concept before `expcps_overload.cpp` |
| 1 Script and cue map | pass | 21 sentence takes, measured in `assets/vo/expcps-overload/timing.json`; same cues drive picture, score and SRT |
| 2 Stills | pass | 40 encoded frames: starts/middles/ends, seven fade midpoints, stepped diagram stages and pavement-route stages inspected; full-resolution views confirm T1 lies on a fenced pad south of the sidewalk and three trails turn into it |
| 3 Draft muted, then sound | conditional | inspected the 40-frame encoded progression muted; audio track and cue map are unchanged from the prior ASR-checked master; direct auditory playback is unavailable to this reviewer |
| 4 Final technical QA | pass | revised MP4: ffprobe 1920×1080 / 30 fps / 3,254 frames / 108.5 s; full decode succeeds; -16.3 LUFS and -1.3 dBTP; encoded contact sheet extracted |
| 5 Authored test | pass | two pavement-routed physical trails lead to one roadside transformer; rule/topology join, animated reverse traversal and paper-result tree follow; ending retains unresolved R3 |
| 6 Craft and coherence | pass | CityJSON has exactly ten Building objects; T1 cabinet and fenced pad are outside the carriageway. Final `city::trail` ribbons turn at the sidewalk; `dg::Diagram` edges use fixed right-angle waypoints. Invented geography and types are labeled illustrative; scenario and prototype remain separated |
| 7 Release packaging | pass | paper title/DOI in video, full-VO SRT with 21 cues, thumbnail checked at 320×180 |

The conditional Gate 3 means this is a review master awaiting a human sound audition, not a fully certified release under the repository standard.

## 3. Scorecard

| # | Criterion | Score | Evidence / remaining issue |
| --- | --- | --- | --- |
| 1 | Conceit | 3 | alarm to two trails to evidence tree |
| 2 | Script / voice | 3 | source-grounded seven-chapter narration, measured per sentence |
| 3 | Evidence | 3 | distinct proof object per chapter and Figure 9 outcome |
| 4 | Timing | 3 | scene lengths derived from measured WAV, cue timestamps shared |
| 5 | Sound | 2 | authored score and intelligible mixed narration by ASR; perceptual audition pending |
| 6 | Visual system | 3 | typed CityJSON village anchors opening and closing; pavement routes and stepped diagram edges carry the same semantic branch palette |
| 7 | Chapter differentiation | 3 | typed village, message path, roof cutaway, join, animated reverse trace, animated result tree |
| 8 | Data and provenance | 3 | paper section/figure ledger, scenario labels, no invented measurements |
| 9 | Production pipeline | 3 | score/SRT through `c.assets`, list from build directory, deterministic stills |
| 10 | Verification | 2 | visual and technical complete; auditory sign-off pending |
| 11 | Memorability / thumbnail | 2 | two projected trails converge on T1 in the CityJSON settlement; thumbnail legible at 320×180 |
| 12 | Specificity | 3 | Flex events, rule/topology grounding, Figure 9 and R3 caveat |
| | **Authoredness total** | **33/36** | exceeds public-cut score target, Gate 3 remains conditional |

### Craft & Coherence

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 13 | Scenario coherence | 3 | separates conceptual weather route from displayed FLEX-MESSAGING result |
| 14 | Information design | 3 | transformer lies on a distinct roadside pad; pavement routes show right-angle turns; graph join and animated tree prove the method |
| 15 | Text performance & pacing | 2 | short captions, ~108 s; human pacing audition pending |
| 16 | House style & mode grammar | 3 | research explainer with simulation label and source footer |
| 17 | Spec-to-screen fidelity | 3 | ten typed CityJSON buildings, H4 panels, roadside T1, pavement-aligned ribbons and fixed stair edges; R3 left open |
| 18 | Revision quality | 3 | T1 moved off road and paths/diagram edges rerouted after user review; final stills and encoded frames checked |
| | **Craft total** | **17/18** | exceeds public-cut score target |

## 4. Critical fails

None observed. The remaining condition is a perceptual sound review, recorded under Gate 3 rather than hidden.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | pass for sampled visual review | 40 encoded samples show causal order, stair diagrams, and the paved route into T1 |
| Thumbnail test | pass | encoded frame at 104 s retains title, CityJSON village and two trails at 320×180 |
| Reskin test | pass | Flex market, H4 roof/battery, BIFROST-style topology and R1/R2/R3 outcome are specific to this paper |
| Retiming test | pass by construction | measured chapter WAVs determine chapter lengths; score/SRT read same cues |
| Clean-machine list | pass | `build/raymotion.exe list` from `D:\code` reports composition without score generation |
| One idea per screen | pass | each chapter centers one proof object |
| Distinct chapter | pass | physical, digital, semantic and result grammars differ |

## 6. Packaging

- [x] Title, citation/DOI and source footer.
- [x] Thumbnail candidate and 320×180 check.
- [x] 21-cue SRT with full spoken text.
- [x] No promo social row; mode is explainer.
- [x] Clean-machine `list` checked from another working directory.

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920 × 1080 | 1920 × 1080 | pass |
| Frame rate | 30 fps | 30 fps | pass |
| Video frames | 3,254 | 3,254 | pass |
| Duration | 108.47 s picture | 108.50 s MP4 including AAC tail | pass |
| Full decode | no errors | `ffmpeg -v error -i final.mp4 -f null NUL` exits 0 | pass |
| Integrated loudness | about -16 LUFS | -16.3 LUFS | pass |
| True peak | ≤ -1 dBTP | -1.3 dBTP | pass |
| Voice wording | script preserved | ASR 1.000 for five chapter WAVs; 0.923/0.909 for ExpCPS pronunciation; mixed-master match 0.976 | pass for content |
| Encoded-frame check | roadside T1 and stepped routes legible | 40 selected frames extracted from final MP4 into `out/expcps-v5-qc-sheet.png` and viewed | pass |
| Still determinism | same cue/frame | same frame model; independently extracted encoded frames agree visually | pass |
| SRT present | full text | 21 cues, final end 106.6 s | pass |
| Thumbnail present | readable at 320×180 | `out/expcps-overload-thumbnail.png`, checked | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why | Next step |
| --- | --- | --- | --- | --- |
| Authoredness | Sound | 2 | objective checks cannot judge voice timbre or score balance by ear | human audition |
| Authoredness | Verification | 2 | Gate 3 lacks direct auditory playback | human audition |
| Craft | Text performance & pacing | 2 | caption rhythm visually sampled; listener pacing not judged by ear | human audition |
| Craft | Information design | 3 | lowest remaining tier is 3 | no blocking fix |

## 9. Decision

**Revised review master delivered.** Authoredness 33/36, Craft 17/18, no observed critical fail, Gates 0–2 and 4–7 passed. Gate 3 remains conditional until a listener watches the draft or final cut with sound. This is not called a fully certified release.

Reviewer: Codex — 2026-10-27.

## 10. Notes for the next author

Preserve the source distinction: the rain branch is part of the conceptual Flex scenario, while Figure 9 demonstrates FLEX-MESSAGING and a deeper trace; R3 grouping remains future work. Preserve the measured cue source and the `scenario threshold 60%` label. A listener should specifically check the pronunciation of `ExpCPS`, `SPARQL`, and the naturalness of the generated narrator before publication.
