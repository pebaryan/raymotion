# Programming language badge sheet — authoring brief

| | |
| --- | --- |
| Composition | `programming-language-badge-sheet` |
| Story mode | utility |
| Owner | Codex |
| Status | approved |
| Runtime | one still frame |
| Platform | 2400 × 2200 PNG |
| Voice | none; static reference sheet |
| Audience | raymotion authors choosing language badge prefabs |
| Source material | Simple Icons 16.36.0, archived C# icon, Devicon Java icon |

## Mode contract and conceit

This is a visual catalogue of programming languages that makes each reusable badge easy to find by placing the actual registered prefabs on one consistent grid. The proof object is the rendered prefab in every labelled cell. The hierarchy is title, badges, language names, and prefab IDs; packaging is a standalone PNG.

## Chapters and cue map

| Chapter | Frame | Text | Visual action | Proof object | Sound |
| --- | --- | --- | --- | --- | --- |
| Catalogue | 0 | `PROGRAMMING LANGUAGES`, count, language names, prefab IDs | Show every registered language badge once in an aligned grid | Actual prefab instances, counted by registry data | silence |

Voice: none. Text-only cue map: frame 0 shows the complete labelled catalogue. No animation or audio is needed for this utility image.

## Source ledger

| Claim or asset | Source | Status |
| --- | --- | --- |
| 18 language badge paths and colours | [Simple Icons 16.36.0](https://github.com/simple-icons/simple-icons/tree/16.36.0), except below | exact monochrome path data and catalog colours |
| C# path | [Simple Icons 12.4.0](https://github.com/simple-icons/simple-icons/tree/12.4.0/icons) | archived monochrome path |
| Java path | [Devicon Java plain](https://github.com/devicons/devicon/blob/master/icons/java/java-plain.svg) | monochrome adaptation of Devicon vector |
| Badge count and IDs | `icons::languages()` and prefab registry | exact |

Brand colours are used as badge backgrounds and marks are rendered white, except JavaScript, whose black mark preserves contrast on yellow. This is a coherent monochrome badge treatment, not a claim that each official logo is single-colour.

## Visual system

Dark ink background and darker cell panels match the social badge sheet. A 6 × 3 grid gives each 190-pixel badge one equal cell. Headline, title, and ID use existing UI fonts; large margins and consistent gutters make the sheet scannable. The recurring motif is the equal badge cell. There is no motion, chart, map, caption box, HUD, or metaphor beyond direct comparison. Thumbnail candidate is the full sheet.

## Audio and assets

Silent PNG. SVG paths are embedded in the engine; no runtime network asset or generated file is required. Source version and exceptions are recorded above. `list` and rendering must work from another current directory.

## Stills and acceptance

Review the only frame at full size and inspect all cells, especially thin/small marks and JavaScript contrast. Confirm 18 registrations, nonempty glyph bounds, pixel colours, a repeat render, and an external-current-directory render. Render output: `out/programming-language-badge-sheet.png`.

Known risk: monochrome paths simplify multi-colour official identities. Preserve source paths and document the treatment.
