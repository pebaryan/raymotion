# asp-feed-v2 — script, cue map and sources

Companion to `docs/asp-feed-v2-brief.md`.
Composition: `asp-feed-v2` · 1080×1920 · 30 fps · target 45 s · explainer · text-only first cut.

Status: **text-only preview built** (45 s, Authoredness 33/36, Craft 17/18).
No voice files yet; the captions are the script. A voice can be attached later at the
same cue map.

## Voice and caption rules

- First person omitted; direct address, short sentences.
- One idea per sentence. No “grounding”, “stable model” or “Herbrand universe” on screen.
- Define nothing that the map does not show. “Answer set” is shown, then named.
- Numbers: “two colors”, “four regions”, “two answer sets”, “one constraint”. Say “two”
  only for this instance.
- Constant **2.5 words/s**; spoken words use the caption accent, upcoming words use
  `withAlpha(INK, 0.35)`; no glow or selection box; the line holds after the last word.
- The film is text-only first; the SRT carries the same cues.

## Narration and captions

Visible scene starts after 10-frame transitions: 0, 130, 260, 425, 540, 690, 870, 1065,
1180; end 1,350.

| # | Chapter | Voice / caption | Target |
| --- | --- | --- | --- |
| 1 | The old way | “You know how coding means writing every step?” | 0:00–0:04.3 |
| 2 | The shift | “Answer set programming: describe the solution, not the steps.” | 0:04.3–0:08.7 |
| 3 | The problem | “Color four regions with two colors. Neighbours must differ.” | 0:08.7–0:14.2 |
| 4 | Facts | “The regions and borders are facts.” | 0:14.2–0:18.0 |
| 5 | Rules | “Every region gets one color. Neighbours cannot match.” | 0:18.0–0:23.0 |
| 6 | Answer sets | “The solver returns every valid coloring. There are two.” | 0:23.0–0:29.0 |
| 7 | Constraint | “Add one constraint: A is not red. One world disappears.” | 0:29.0–0:35.5 |
| 8 | Why care | “Schedules, plans, puzzles: you describe, it solves.” | 0:35.5–0:39.3 |
| 9 | Outro | “Describe the goal. Skip the steps.” | 0:39.3–0:45.0 |

Total 74 words; at 2.5 words/s the text takes ~30 s and the holds fill the 45 s. The build
uses the scene starts and the constant rate; do not stretch the highlight to fill a scene.

## The exact map instance

```
regions: a (top-left), b (top-right), c (bottom-left), d (bottom-right)
borders: a-b, a-c, b-d, c-d
colors:  red, blue
```

The graph is a four-cycle, so the only proper 2-colorings are:

| # | A | B | C | D |
| --- | --- | --- | --- | --- |
| 1 | red | blue | blue | red |
| 2 | blue | red | red | blue |

Constraint: `A is not red`. Answer set 1 is removed; answer set 2 survives:

```
{A=blue, B=red, C=red, D=blue}
```

The film shows these as two mini-maps; it does not claim “two” is a general count.

## Cue map (targets)

Picture and score read the same table. Final frames come from the scene starts.

| Chapter | Phrase | Target | Picture | Sound |
| --- | --- | --- | --- | --- |
| 1 | “every step” | 0:03 | Step cards stack | Tick per card |
| 2 | “describe the solution” | 0:06 | Step stack collapses; map appears | Riser/impact |
| 3 | “four regions” | 0:09 | Four tiles; borders draw | Map ticks |
| 3 | “two colors” | 0:11 | Red/blue chips | Chip pops |
| 4 | “facts” | 0:15 | Region + border chips snap in | Chip pops |
| 5 | “one color” | 0:19 | Rule card; one region gets a color | Rule stamp |
| 5 | “cannot match” | 0:21 | Adjacent pair highlighted | Check tick |
| 6 | “every valid coloring” | 0:25 | First answer-set mini-map | Solution chime |
| 6 | “two” | 0:27 | Second answer-set mini-map | Solution chime |
| 7 | “A is not red” | 0:31 | Constraint card; red-A map crossed | Cross hit |
| 7 | “disappears” | 0:33 | Red-A map leaves; blue-A map remains | Remaining chime |
| 8 | “schedules” | 0:36 | Three use cards | Pulse |
| 9 | “Skip the steps” | 0:42 | End card | D-major lift |

## Sources and assumptions

| Claim / number | Value | Source | On-screen | Exact or illustrative |
| --- | --- | --- | --- | --- |
| ASP / clingo | language and solver | Potassco, clingo documentation: <https://potassco.org/clingo/> | source line | Reference |
| Four-region / four-border map | 2×2 instance | Ours; the exact instance is shown on screen | yes | Exact |
| Base answer sets | 2 | Enumerated all 16 two-color assignments; exactly 2 satisfy the four borders | yes (“two”) | Exact for this instance |
| Constraint answer set | 1 | Remove `A=red`: only `{A=blue,B=red,C=red,D=blue}` remains | yes | Exact for this instance |
| “Every valid coloring” | every valid coloring of this map | Enumeration (below) | yes | Exact for this instance |

The source line is conceptual; the film does not claim a live solver run. The map-coloring
instance is our own teaching example, not a benchmark.

### Enumeration check

There are 2^4 = 16 assignments of two colors to four regions. Check the four borders:

```
a-b, a-c, b-d, c-d must differ.
```

Only:

```
{A=red,  B=blue, C=blue, D=red}
{A=blue, B=red,  C=red,  D=blue}
```

survive. After `A != red`, only the second remains. A tiny test in the v2 code (or a
comment in the script) should assert this before render.

## Optional clingo sketch (not displayed as code)

```
region(a;b;c;d).
border(a,b). border(a,c). border(b,d). border(c,d).
color(red;blue).

{ assign(R,C) : color(C) } = 1 :- region(R).
:- assign(R1,C), assign(R2,C), border(R1,R2).
% constraint:
:- assign(a,red).
#show assign/2.
```

The film shows only plain-language rule cards, not this syntax, to keep the feed
non-specialist. The sketch is the reproducibility note.

## Audio plan

- Generated with `procgen::Score` into `out/asp-v2-assets/score.wav`.
- Cold pad under the step stack; ticks for cards/borders; a two-note chime for the two
  answer sets; a cross hit; a D-major lift at the end.
- No voice in this cut; leave headroom for one.
- Target **-16 LUFS ± 1**, true peak **≤ -1 dBTP**; soft-limit if needed.

## SRT and thumbnail

- SRT: one cue per chapter with the caption text; write `out/asp-feed-v2.srt` in
  `c.assets`.
- Thumbnail: the two answer-set mini-maps at ~0:27 or the surviving map at ~0:33; choose
  the frame that says “two valid solutions, one survives” at 320×180.

## Open questions before code

1. Is the 2×2 / two-color map the right level, or should it be a 3-color map with more
   answer sets? (Two colors keeps the proof exact and legible at feed size.)
2. Text-only first cut, or add a voiceover now? (Recommend text-only; voice can follow the
   same cue map.)
3. Is 45 s the right runtime for a feed explainer?
4. The end card uses a 2× raymotion wordmark at 3/4 height; the source line stays at the
   bottom. The wordmark is large enough to read but still secondary to the close.
