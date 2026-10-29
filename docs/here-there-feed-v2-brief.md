# here-there-feed-v2 - authoring brief

> Status: **text-only preview built** (59.8 s, Authoredness 34/36, Craft 17/18,
> 2026-10-26). The paper-cut H/T islands and the smoke-only guess are the proof; a human
> muted/sound watch is still required before release. A voice can be attached at the
> same cue map.

| | |
| --- | --- |
| Composition name | `here-there-feed-v2` |
| Story mode | **explainer** |
| Owner / agent | author-agent |
| Status | `approved` |
| Runtime target | **59.8 s** (1,794 frames at 30 fps; exact from the cue map) |
| Platform / aspect | 1080x1350, 30 fps, 4:5 social feed |
| Voice | **none** - text-only cut; captions are the script. A voice can be attached at the same cue map later. |
| Audience | social viewers who know true/false and "if ..., then ..." but not HT or equilibrium |
| Source material | Cabalar 2021, sec. 2; Aguado et al. 2013, secs. 2-3; v1 `docs/ht-feed-preview.md` and `examples/showcase/ht_feed.cpp` |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | **explainer** |
| Goal | Make one idea land in under a minute: a rule can be satisfied by a guess that is not supported; HT's two views and a given fact decide what survives. |
| Proof object | The paper-cut H/T islands, atom tokens, rule ribbon, the removed-smoke tray, and the given-fact badge with removal checks. |
| Required evidence | Source ledger below; 6-chapter text-only script and cue map in `docs/here-there-feed-v2-script.md`; exact two-atom examples. |
| Text | One line at a time, constant **2.5 words/s**, spoken words in ink, upcoming at 32%; no glow or caption box. |
| House grammar | Feed explainer: paper-cut stage, two islands, rule ribbon, one question per beat, small source line. |
| Packaging | Title/credits/source line, 1080x1350 thumbnail, generated SRT, small end wordmark. No promo CTA. |

## 1. One sentence

> This is a film about **Here-and-There logic** that makes the viewer **feel the gap
> between "not contradicted" and "supported"** by **keeping a smoke-only guess fixed
> while removing smoke from HERE, then adding the fact fire and watching what survives.**

## 2. One-line argument

> A smoke-only guess passes `fire -> smoke` on both sides, but a smaller HERE still
> passes, so it is not supported; adding the fact `fire` makes both `fire` and `smoke`
> necessary, and that world survives.

## 3. Chapters

Scene starts are cumulative from the word-count cue map: `0, 210, 558, 882, 1266, 1614`,
ending at `1794` (59.8 s). There are no crossfades; the paper stage changes at those
frames, and the caption line changes on its own word-count boundary.

| # | Time (s) | Chapter | On-screen text (short) | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0:00-0:07.0 | The question | "If nothing contradicts a claim, should you believe it?" | A GUESS bubble holds a smoke puff; the given-facts tray is empty | The guess and the empty facts | Paper tick, smoke puff |
| 2 | 0:07.0-0:18.6 | Two views | "There is what we assume. Here is what we keep." | The stage unfolds into THERE (amber) and HERE (teal); smoke is only in THERE | The H/T islands and the H subset T relation | Unfold whoosh, two chimes |
| 3 | 0:18.6-0:29.4 | Both sides | "HERE is empty, so it passes. THERE has smoke, so it passes too." | The fire-to-smoke ribbon appears on both islands; checks land | The two rule checks | Two checks |
| 4 | 0:29.4-0:42.2 | Not backed up | "Keep the guess fixed. Remove smoke from HERE." | THERE is pinned; smoke leaves HERE; the rule still passes; a stamp lands | The removed-smoke model and the rejection | Pin, slide, low stamp |
| 5 | 0:42.2-0:53.8 | The fact | "Now add a fact: there is fire." | GIVEN: FIRE lands; fire and smoke appear; remove-fire/remove-smoke checks | The supported world and the two removal tests | Fact stamp, two checks |
| 6 | 0:53.8-0:59.8 | Close | "Nothing says it's wrong. But what says it's right?" | The supported stage holds; source line and small wordmark | The final question | Resolve, logo bell |

## 4. Script

Full caption script, cue map and source mapping: `docs/here-there-feed-v2-script.md`.

- Voice: **none**; the feed is read. The captions are the script.
- Constant **2.5 words/s**; spoken words in ink, upcoming at 32%; one line at a time.
- The first line is the hook and must survive muting.
- The stage has one idea per chapter; the islands and rule ribbon are the proof.
- The fact variant is labelled **worked example**; the two-atom scope is on screen.

## 5. Sources and claims

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| `fire -> smoke` is the only rule | one rule | Cabalar 2021, sec. 2 opening | ribbon | exact |
| A smoke-only guess passes on both sides | H={}, T={smoke} | Implication clause; `ht::theory(0,SMOKE)` | island checks | exact for the shown example |
| A smaller HERE still passes | H={}, T={smoke} | Equilibrium Definition 1; `ht::theory(0,SMOKE)` | removed-smoke panel | exact |
| Adding fact `fire` requires both atoms | H=T={fire,smoke} | Definition 1 applied to the rule + fact | supported world | our worked extension |
| The guess is not backed up | not equilibrium | Equilibrium's no-smaller-H condition | "not backed up" stamp | exact for this example |

Source links:

1. Pedro Cabalar (2021), *Temporal ASP: from logical foundations to practical use with
   telingo*, sec. 2: <https://www.dc.fi.udc.es/~cabalar/RW2021.pdf>
2. Aguado, Cabalar, Dieguez, Perez & Vidal (2013), *Temporal Equilibrium Logic: a survey*,
   secs. 2-3: <https://www.dc.fi.udc.es/~cabalar/tel-dp60.pdf>

The diagrams and the fact variant are our own. HT/equilibrium are not attributed as
Cabalar's inventions. The feed does not cover negation, temporal operators or the full
formal definitions; see the longer `here-there-explainer-v2` for those.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | Paper `#F9F0DA`; ink `#273640`; amber `#E19B39` = THERE / guess; teal `#239994` = HERE / kept; coral `#D24C3A` = unsupported / fail; muted `#636C6C`. Never use color alone: labels and check marks carry the state. |
| Type | Inter via `fonts::ui(true/false)`. Caption 58/66; labels 40/44; small labels 30/34; source 22. |
| CJK | n/a; English-only. |
| Margins / safe area | 48 px side margins; caption 64-285; islands 531-1165; footer 1268-1310. |
| Caption box | One line at a time, max width 952, top at 64; 1-3 wrapped lines; no box behind the glyphs. |
| HUD / persistent frame | Small `01 / 06` at top right; thin progress line at 40; source line at 1290. |
| Motion grammar | Paper unfolds in 0.8 s; tokens slide in 0.65 s; checks pop in 0.25 s; no idle motion beyond the smoke curl and progress line. |
| Motif | The two paper islands and their rule ribbons; one guess, two views, one removal test. |
| Charts / maps | No chart; the proof is a two-island set diagram with membership tokens and a rule ribbon. |
| Text performance | 2.5 words/s; current words in ink, upcoming at 32%; no glow/box; the line holds. |
| Brand kit | Small `raymotion-logo` on the closing beat, bottom right; source line; no social row (explainer). |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | None; declared text-only. |
| Score | Generated `procgen::Score`: paper ticks, two-note H/T interval, smoke whoosh, check bells, a low stamp for "not backed up", a fact stamp and a resolve. |
| Ducking | n/a (no voice); keep the marks forward. |
| SFX | Guess puff, unfold, two checks, pin, smoke slide, reject stamp, fact stamp, removal checks, resolve/logo. |
| Silence | None declared; the score is sparse and papery. |
| Loudness target | -16 LUFS integrated, true peak <= -1.5 dBTP. |

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Signature | `{fire, smoke}` | all tokens and checks | Cabalar example |
| Guess | H={}, T={smoke} | smoke only in THERE | exact |
| Rule | `fire -> smoke` | both island ribbons | exact |
| Removed-smoke model | H={}, T={smoke} | rule still passes | exact |
| Fact world | H=T={fire,smoke} | fact + rule | our worked example |
| Fact removal | fire out -> fact breaks | supported world | exact |
| Rule removal | smoke out -> rule breaks | supported world | exact |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | generated in `c.assets` | `out/ht-feed-v2-assets/score.wav` | regenerate from the cue map |
| Voice | n/a | - | - |
| SRT | generated in `c.assets` | `out/here-there-feed-v2.srt` | derive from the caption table |
| Thumbnail | generated still from `here-there-feed-v2-thumb` | `out/here-there-feed-v2-thumb.png` | render a still at frame 30 |
| Fonts | bundled Inter | `fonts::ui` | fail clearly if missing |

Clean-machine test: `list` works from any directory; score/SRT regenerate with an empty
asset directory; no external images or Windows-only fonts.

## 9. Cue map

Full line-level map: `docs/here-there-feed-v2-script.md`.

| Frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 0 | GUESS bubble; empty facts | Paper tick | ch1 line |
| 75 | Smoke puff appears | Smoke puff | ch1 midpoint |
| 210 | Islands unfold | Unfold whoosh | ch2 line 1 |
| 342 | "keep less" note | Two-note interval | ch2 line 2 |
| 558 | Rule ribbons appear | Two chimes | ch3 line 1 |
| 678 | Both checks | Two checks | ch3 line 2 |
| 882 | Pin THERE | Pin | ch4 line 1 |
| 1014 | Smoke leaves HERE | Slide | ch4 line 2 |
| 1122 | "Not backed up" stamp | Low stamp | ch4 line 3 |
| 1266 | GIVEN: FIRE | Fact stamp | ch5 line 1 |
| 1350 | Removal checks | Two checks | ch5 line 2 |
| 1506 | Both stay | Resolve | ch5 line 3 |
| 1614 | Close | Resolve | ch6 line |
| 1740 | Source + wordmark | Logo bell | end |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 60 | 1 | Guess bubble, smoke puff, empty facts; caption safe |
| 150 | 1 | Hook readable; smoke visible; no clipping |
| 400 | 2 | THERE/HERE islands, smoke only in THERE; caption safe |
| 620 | 3 | Rule ribbon and both checks; no token overlap |
| 1040 | 4 | Pin + smoke leaving HERE; rule still passes |
| 1125 | 4 | "Not backed up" stamp; no overlap |
| 1320 | 5 | GIVEN: FIRE; supported world; fact label |
| 1580 | 5 | Both removal checks; both stay |
| 1700 | 6 | Final question; source/wordmark |
| 1785 | 6 | End frame; no clipping |

Plus transition/fade states and five encoded frames from the final MP4.

## 11. Acceptance checks

- [ ] Muted, the guess/kept/removed/supported story is legible.
- [ ] Every chapter has its proof object visible at the stated frame.
- [ ] Every claim matches the source ledger; the fact variant is labelled.
- [ ] Captions stay above the islands and inside the safe area.
- [ ] Scene lengths and score cues derive from the caption word count.
- [ ] `list` works from another directory; score/SRT regenerate.
- [ ] Duration, frame count, decode, loudness and true peak recorded.
- [ ] Caption rate is constant 2.5 words/s.
- [ ] Thumbnail is readable at 320x400 and states the conceit.
- [ ] Authoredness >= 22 and Craft >= 13; showcase >= 30 and >= 16.

## 12. Risks / non-goals

- **Risks:** the 4:5 feed has less room for the two islands; the removal animation must
  not look like a semantic state change in THERE; the smoke-only guess can be mistaken
  for a proof of smoke.
- **Non-goals:** not a proof of physical smoke, not a full HT/TEL tutorial, not a
  temporal cut. The feed omits negation and the 2x2 state table.
- **Out of scope:** voice, temporal operators, historical attributions.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | author-agent self-review | 2026-10-26 | approved |
| Script / cue map | author-agent self-review | 2026-10-26 | approved |
| Stills | pending | - | - |
| Draft | pending | - | - |
| Final QC | pending | - | - |
