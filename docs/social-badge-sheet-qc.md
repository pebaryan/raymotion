# Social badge sheet — QC review

| | |
| --- | --- |
| Composition | `social-badge-sheet` |
| Build | local `build/raymotion.exe`, 2026-10-28 |
| Reviewer | Codex |
| Review stage | final PNG |
| Story mode | utility |
| Artifacts | `docs/social-badge-sheet-brief.md`, `out/social-badge-sheet.png` |

## 1. One-sentence conceit

This is a sheet of raymotion's social badges that lets a user compare every registered brand at a glance by placing the actual prefabs on one consistent grid. Viewed result matches the brief.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief completed before the one-frame composition |
| 1 Script and cue map | pass | text-only frame 0 cue; no voice or animation |
| 2 Stills | pass | opened final PNG; 19 marks, including six marketplace marks, labels, and margins visible |
| 3 Draft | n/a | still image, no timeline or soundtrack to watch |
| 4 Final technical QA | pass | PNG opens at 2400 × 2200; fresh repeat and external-CWD renders have identical SHA-256 |
| 5 Authored test | pass for utility | grid makes actual prefabs directly comparable |
| 6 Craft & coherence | pass | four aligned rows, consistent badges and type hierarchy, no clipping; willhaben uses official icon artwork |
| 7 Release packaging | pass | requested standalone PNG delivered |

## 3. Scorecard

For this one-frame utility, video-specific criteria are read through the static-image mode contract.

| # | Criterion | Score | Evidence |
| --- | --- | --- | --- |
| 1 | Conceit | 2 | direct comparison sheet |
| 2 | Script / voice | 2 | complete title and label cue, voice n/a |
| 3 | Evidence | 3 | 19 actual registered prefab instances |
| 4 | Timing | 3 | one frame, no timing ambiguity |
| 5 | Sound | 2 | silence appropriate to a PNG |
| 6 | Visual system | 3 | consistent cells, palette, hierarchy |
| 7 | Chapter differentiation | 2 | one chapter, 19 visually distinct brands |
| 8 | Data & provenance | 3 | registry drives count, names, and cells |
| 9 | Production pipeline | 3 | reproducible CLI still command |
| 10 | Verification | 3 | visual inspection and pixel-identical rerender |
| 11 | Memorability / thumbnail | 2 | full sheet useful as a reference, thumbnail n/a |
| 12 | Specificity | 2 | actual raymotion social prefab identifiers |
| | **Authoredness total** | **30/36** | static utility interpretation |

| # | Craft criterion | Score | Evidence |
| --- | --- | --- | --- |
| 13 | Scenario coherence | 3 | title count matches cells |
| 14 | Information design | 3 | equal scale, labeled comparison |
| 15 | Text performance & pacing | 3 | simultaneous, readable labels |
| 16 | House style & mode grammar | 3 | raymotion dark UI, utility grammar |
| 17 | Spec-to-screen fidelity | 3 | all six requested marketplaces and earlier badges present |
| 18 | Revision quality | 3 | resized grid for 19; fixed generated initializer and verified mark rendering |
| | **Craft total** | **18/18** | |

## 4. Critical fails

None observed. There are no statistical or historical claims. The user requested a static image, so MP4, audio, and SRT checks do not apply.

## 5. Authored tests

- Mute: n/a; the PNG is silent and complete.
- Thumbnail: n/a; full-resolution reference sheet is the deliverable.
- Reskin: each cell invokes a named raymotion prefab rather than generic artwork.
- Retiming: n/a; one frame.
- Clean machine: `D:\code\raymotion\build\raymotion.exe list` from `D:\code` lists `social-badge-sheet`.
- One idea per screen: pass; every cell presents the same comparison unit.
- Distinct chapter: n/a; one static sheet.

## 6. Packaging

Standalone PNG is present. No title sequence, credits, subtitles, CTA, or audio are required for this utility image.

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 2400 × 2200 | 2400 × 2200 | pass |
| File | PNG | RGBA PNG, 467387 bytes | pass |
| Still determinism | identical | fresh repeat and external-CWD renders have the same SHA-256 as final PNG | pass |
| Full decode | image opens | opened and inspected | pass |
| MP4 frame count, loudness, peak, SRT | n/a | n/a | n/a |

## 8. Two lowest criteria

Authoredness: Conceit and sound (2 each) because this is a deliberately silent reference sheet. Craft criteria all score 3 after the grid and path-initializer revisions. The incorrect willhaben magnifier was replaced with its official current app icon and checked in the final sheet.

## 9. Decision

**Ship** the one-frame utility image. Authoredness 30/36, Craft 18/18, no critical fail under the static-image mode contract.

Reviewer: Codex — 2026-10-28.

## 10. Notes for the next author

Keep the cell list derived from `icons::brands()` so future prefabs appear automatically. The current four-row sheet accommodates 19; revise its height when the brand count grows beyond 20.

