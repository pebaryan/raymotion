# Programming language badge sheet — QC review

| | |
| --- | --- |
| Composition | `programming-language-badge-sheet` |
| Story mode | utility |
| Build | local Release `build/raymotion.exe`, 2026-10-28 |
| Reviewer | Codex |
| Artifacts | `docs/programming-language-badge-sheet-brief.md`, `out/programming-language-badge-sheet.png` |
| Decision | ship |

## Conceit and gates

A labelled grid compares the actual registered language badge prefabs at equal size. Gate 0 passed: the brief and text-only cue map preceded composition code. Gate 1 passed: the one-frame hierarchy is complete. Gate 2 passed: the full PNG was visually inspected; all 18 marks, names, and IDs are legible, with clear gutters and no clipping. Gate 3 is not applicable to a still. Gate 4 passed: the PNG is 2400 × 2200 and the external-current-directory repeat has the same SHA-256. Gate 5 passed: the cells show actual prefab instances. Gate 6 passed: contrast and spacing are consistent; JavaScript and C use dark marks on light backgrounds. Gate 7 passed: the requested utility is packaged as a standalone sheet.

## Scores

Authoredness: 30/36 under the static utility interpretation. Craft: 16/18. No critical fail. No audio, voice, subtitle, MP4 decode, loudness, or true-peak gate applies to this one-frame PNG.

## Technical evidence

| Check | Result |
| --- | --- |
| `raymotion_icon_tests.exe` | passed; 18 language paths, registrations, and rendered badges checked |
| Still image | RGBA PNG, 2400 × 2200, 377907 bytes |
| Visual review | all 18 marks and labels readable; no card or glyph clipping |
| Repeat render | identical SHA-256 from `D:\code` current directory |
| `list` from `D:\code` | composition listed |
| Source provenance | Simple Icons 16.36.0, C# from 12.4.0, Java from Devicon; documented in brief and badge notes |

The catalogue is a one-frame utility image. Its thumbnail candidate is the full sheet. The monochrome badge system simplifies multi-colour source marks; the source ledger states that treatment explicitly.
