# asp-feed-v2 — authoring brief

> Status: **text-only preview built** (45 s, Authoredness 33/36, Craft 17/18).
> Voice is optional and can be attached at the same cue map; see `docs/asp-feed-v2-qc.md`.

| | |
| --- | --- |
| Composition | `asp-feed-v2` |
| Story mode | **explainer** |
| Owner / agent | `<author>` |
| Status | `draft` |
| Runtime target | **45 s** (1,350 frames at 30 fps) |
| Platform / aspect | 1080×1920, 30 fps, 9:16 social feed |
| Voice | Text-only first cut; captions are the script; a voice can be attached later at the same cue map |
| Audience | developers and technical viewers who use chatbots but have never written a solver |
| Source material | `examples/showcase/asp.cpp` (v1), `docs/asp-feed-v2-script.md` |

## 1. One sentence

> This is a film about **answer set programming** that makes the viewer **see a solver
> return every valid solution, then lose one when a constraint is added**, by **coloring a
> four-region map with two colors and showing both answer sets side by side.**

## 2. One-line argument

> ASP flips the job: you describe what a valid solution looks like — facts, rules,
> constraints — and the solver returns every answer set; add a constraint and the invalid
> worlds disappear.

## 3. Chapters

Target times are after 10-frame transitions; final scene lengths are `140, 140, 175, 125,
160, 190, 205, 125, 170` frames with 8 × 10-frame overlaps, so the visible starts are
0, 130, 260, 425, 540, 690, 870, 1065, 1180 and the film ends at 1,350.

| # | Time (s) | Chapter | On-screen text (short) | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0:00–0:04.3 | The old way | “You know how coding means writing every step?” | Four step cards stack; a goal card sits apart | The step stack | Cold pad, tick per step |
| 2 | 0:04.3–0:08.7 | The shift | “Answer set programming: describe the solution, not the steps.” | The step stack collapses; a goal card becomes a map | The map appearing | Riser, soft impact |
| 3 | 0:08.7–0:14.2 | The problem | “Color four regions with two colors. Neighbours must differ.” | A 2×2 map with four regions; borders draw; two color chips | The map and the color chips | Map draw ticks |
| 4 | 0:14.2–0:18.0 | Facts | “The regions and borders are facts.” | Region chips + border lines snap in | The fact chips | Chip pops |
| 5 | 0:18.0–0:23.0 | Rules | “Every region gets one color. Neighbours cannot match.” | A rule card with two simple clauses; the map highlights one pair | The rule card and a checked pair | Rule stamp |
| 6 | 0:23.0–0:29.0 | Answer sets | “The solver returns every valid coloring. There are two.” | Two mini-maps flip in, each a valid checkerboard | The two answer-set maps | Two solution chimes |
| 7 | 0:29.0–0:35.5 | Constraint | “Add one constraint: A is not red. One world disappears.” | A constraint card lands; the red-A map is crossed out and leaves | The surviving map | Cross hit, remaining chime |
| 8 | 0:35.5–0:39.3 | Why care | “Schedules, plans, puzzles: you describe, it solves.” | Three use cards; the same map motif repeats | The use cards | Tick, soft pulse |
| 9 | 0:39.3–0:45.0 | Outro | “Describe the goal. Skip the steps.” | Title card; source line; raymotion wordmark | The end card | D-major lift, final bell |

## 4. Script

Full narration, captions, cue map, answer sets and source ledger:
`docs/asp-feed-v2-script.md`.

Rules:

- First line is the hook; it must survive muting.
- One idea per sentence; no “grounding”, “stable model” or “Herbrand universe” on screen.
- The two answer sets are exact, not illustrative. State “two” only because the map has
  exactly two valid colorings.
- The constraint is `A is not red`; the exact remaining answer set is
  `{A=blue, B=red, C=red, D=blue}`.
- The film is a text-only explainer first; the captions are the script.
- Constant reading rate **2.5 words/s**; spoken words in the accent, upcoming dim; no
  glow or selection box; the line holds after the last word.

## 5. Sources and claims

Full ledger in `docs/asp-feed-v2-script.md` §Sources.

| Claim | Value | Source | On-screen |
| --- | --- | --- | --- |
| ASP and clingo | language / solver | Potassco, clingo documentation: <https://potassco.org/clingo/> | source line |
| The map instance | 4 regions, 4 borders, 2 colors | Our own 2×2 instance, verified by enumeration | yes |
| Base answer sets | 2 exact colorings | Enumerated: `{A=R,B=B,C=B,D=R}`, `{A=B,B=R,C=R,D=B}` | yes |
| Constraint answer set | 1 exact coloring | Remove the `A=R` set: `{A=B,B=R,C=R,D=B}` | yes |
| “Every valid coloring” | exact for this instance | Enumeration in the script/appendix | yes |

The film must not imply that “two” is a general answer-set count; it is the count for this
map. The source line is conceptual, not a claim that the solver was run live.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | `NIGHT #0c1220`; `INK #f2f4ff`; `TEAL #47dad8` (solution/answer set); `GOLD #ffc365` (facts/regions); `RED #ff6b5a` (constraint/cross only). |
| Type | Inter Regular/Bold; kicker 30, caption 54–62, answer-set labels 30, labels 22; code card mono 28. |
| Layout | 64 px margins; kicker top; progress hairline; caption block bottom (y ≈ 1450–1700); map/answer cards centre. |
| Persistent frame | One **region tile** motif: the same rounded tile becomes a fact chip, a map region and a mini-map cell. |
| Motion | 12–16 frame entrances; map borders draw on; answer-set cards flip; constraint stamp and cross; no idle motion except the progress line. |
| Density | One idea per screen; the map must read at feed size; no caption crowds the map. |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | Text-only first cut; captions + SRT. Voice can be attached later at the cue map. |
| Score | Generated with `procgen::Score`: cold pad, ticks for facts, a two-note solution chime, a cross hit, a D-major end lift. |
| Ducking | No voice in this cut; leave headroom for a later voice. |
| SFX | Chip pop, border draw tick, solution chime, constraint cross, end bell. |
| Loudness | **-16 LUFS ± 1**, true peak **≤ -1 dBTP**. |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback |
| --- | --- | --- | --- |
| Score | generated by `genScore` | `out/asp-v2-assets/score.wav` | regenerate |
| SRT | generated in `c.assets` | `out/asp-feed-v2.srt` | derive from the cue table |
| Thumbnail | chosen still / thumb comp | `out/asp-feed-v2-thumb.png` | use the answer-set frame |
| Everything visual | code | — | — |

Clean-machine: `list` works from any directory; score and SRT generate on first still or
render; no external images or fonts beyond the bundled Inter and mono fallback.

## 9. Cue map

Full phrase map in `docs/asp-feed-v2-script.md`. Target keys:

| Frame (target) | Picture | Sound |
| --- | --- | --- |
| 60 | Step cards stack | Tick per card |
| 200 | Step stack collapses, map appears | Riser/impact |
| 330 | Map borders draw; color chips | Map ticks |
| 480 | Facts snap in | Chip pops |
| 620 | Rule card lands; pair checks | Rule stamp |
| 760 | First answer-set map | Solution chime |
| 830 | Second answer-set map | Solution chime |
| 960 | Constraint card lands | Cross hit |
| 1000 | Red-A map crossed out | Remaining chime |
| 1270 | End card | D-major lift |

## 10. Still frames to review

| Frame | What must be true |
| --- | --- |
| 60 | Hook legible; step stack readable |
| 200 | The map replaces the stack without a white-out clip |
| 330 | Four regions, four borders, two color chips readable |
| 480 | Fact chips and borders legible |
| 620 | Rule card legible; pair highlight clear |
| 760/830 | Both answer-set mini-maps readable and different |
| 960 | Constraint card and cross legible |
| 1000 | Exactly one answer set remains |
| 1270 | End card legible; source line present |

Plus one still in every transition, and encoded frames from the final MP4.

## 11. Acceptance checks

- [ ] The map has exactly two valid colorings; the constraint leaves exactly one.
- [ ] The film says “two” only for this instance.
- [ ] Muted, the proof reads: step stack → map → facts → rules → two answer sets → one.
- [ ] Constant 2.5 words/s; captions use the mode/house style; no glow/box.
- [ ] The source line and SRT exist.
- [ ] The score is -16 LUFS ± 1, true peak ≤ -1 dBTP.
- [ ] `list` works from a different directory; assets generate.
- [ ] Authoredness ≥ 22 and Craft ≥ 13; target Authoredness ≥ 27, Craft ≥ 15.
- [ ] Stills, draft, final ffprobe/decode, encoded frames and determinism checked.

## 12. Risks / non-goals

**Risks**

- The map-coloring instance is small; do not imply ASP is only coloring.
- The source line must not imply a live solver run; the colorings are enumerated.
- A text-only cut may need a voice later; do not stretch the captions to fill the scenes.

**Non-goals**

- A full clingo tutorial, syntax reference or performance comparison.
- A claim that ASP is better than imperative programming.
- A generic “AI” story; this is specifically about answer set programming.

## 13. Implementation notes (for the build)

- New file `examples/showcase/asp_v2.cpp`; composition `asp-feed-v2`; add to `scenes.cpp`
  and the `raymotion` target. Keep v1 untouched.
- 1080×1920, 30 fps; `TransitionSeries` fades; scene lengths above; `c.duration = ts.end()`.
- Captions: constant 2.5 words/s; captions-example color style; no glow/box.
- Map: custom `dr::` tiles/rounded cards; two answer-set mini-maps; constraint cross.
- Score: `procgen::Score` in `c.assets`; SRT from the cue table; thumbnail still.
- Packaging (explainer): title/source/credits, thumbnail, SRT; a 2× raymotion wordmark
  at 3/4 height, clear of the main content and the footer
  is optional.
- QC: fill `skills/raymotion/templates/QC-REVIEW.md` with `Story mode: explainer`.

## 14. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | `<name>` | `<date>` | `revise / approved` |
| Script / cue map | | | |
| Stills | | | |
| Draft | | | |
| Final QC | | | |
