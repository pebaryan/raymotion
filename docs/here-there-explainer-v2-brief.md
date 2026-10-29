# here-there-explainer-v2 - authoring brief

> Status: **text-only preview built** (107.7 s, Authoredness 34/36, Craft 17/18,
> 2026-10-26). The source ledger, cue map, score, SRT and thumbnail are in place; a human
> muted/sound watch is still required before release. A voice can be attached at the same
> cue map later.

| | |
| --- | --- |
| Composition name | `here-there-explainer-v2` |
| Story mode | **explainer** |
| Owner / agent | author-agent |
| Status | `approved` |
| Runtime target | **~108 s** (3,232 frames at 30 fps; exact from the cue map) |
| Platform / aspect | 1920x1080, 30 fps, presentation / YouTube lecture |
| Voice | **none** - text-only cut; the captions are the script. A voice can be attached at the same cue map later. |
| Audience | developers and technically curious viewers who know true/false and implication, but not Here-and-There or equilibrium logic |
| Source material | Cabalar 2021, sec. 2; Aguado et al. 2013, secs. 2-3; v1 `examples/showcase/ht_explainer.cpp` and `docs/here-there-explainer.md` |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | **explainer** |
| Goal | Make the two-level semantics and the equilibrium selection test legible from the diagrams, without relying on a voice. |
| Proof object | Set diagrams: three classical models, the stacked H/T pair, the three-state 2x2 grid, the four-case implication table, the negation truth values, the smaller-H test, the two stable models, and the temporal trace grid. |
| Required evidence | Source ledger below; complete text-only script and cue map in `docs/here-there-explainer-v2-script.md`; worked examples are exact for the two-atom signature. |
| Text | One caption line at a time, constant **2.5 words/s**, spoken words in ink and upcoming words at 35% ink; no glow, no per-word box. |
| House grammar | Data/lecture explainer: top kicker/progress, chapter title, one diagram, one caption strip, source line. |
| Packaging | Title, credits/source line, 1920x1080 thumbnail, generated SRT. Wordmark optional and small; no promo social card. |

**Rule:** do not apply a promo requirement to this explainer. The proof object is the
set diagram; the brand is only the source and a small end-card wordmark.

## 1. One sentence

> This is a film about **Here-and-There logic** that makes the viewer **see why a rule
> cannot invent facts and how a two-level model selects the stable ones** by **stacking
> HERE and THERE at one instant, then shrinking HERE until equilibrium rejects the
> impostor.**

## 2. One-line argument

> Classical logic lets `fire -> smoke` accept models that invent atoms; HT evaluates one
> instant at two levels, and equilibrium keeps only total models with no smaller HERE, so
> the rule alone yields the empty model, while adding the fact `fire` yields
> `{fire, smoke}`.

## 3. Chapters

Scene starts are after 10-frame crossfades: `0, 374, 724, 1026, 1436, 1858, 2232, 2582,
2956`, ending at `3232`. The final times come from the same word-count cue map used by
the picture and score; this table is a reading aid, not a second clock.

| # | Time (s) | Chapter | On-screen text (short) | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0:00-0:12.5 | A rule should not invent facts | "A rule says: if fire, then smoke." | Rule pill lands; three classical-model cards appear; two get an "invented" tag | Three model cards with membership | Rule click, three ticks, low question |
| 2 | 0:12.5-0:24.1 | One instant, two levels | "HERE holds what is established; THERE what is assumed." | Stacked H/T panels; subset bracket; fire/smoke chips; "one instant, not now/later" note | The H/T pair and H subset T | Two-note interval, subset stroke |
| 3 | 0:24.1-0:34.2 | Three atomic states | "Absent, assumed only, or established." | 2x2 state grid; the H-only cell is crossed out | Exhaustive one-atom state grid | Three ticks, forbidden buzz |
| 4 | 0:34.2-0:47.9 | Both checks | "Check fire -> smoke in HERE and in THERE." | Four case cards; H/T chips; H-check and T-check marks step in | Four-case implication truth table | Click, pass/fail marks |
| 5 | 0:47.9-0:61.9 | Not established is not false | "Not p means p implies false." | The middle-state pair; p and not-p truth rows; excluded-middle card | Truth values for p and not p at (H,T) | Switch clicks, minor chord |
| 6 | 0:61.9-0:74.4 | Equilibrium: no smaller HERE | "Keep T fixed; shrink HERE." | Total model pair; a smaller H is tested; reject stamp | The smaller-H model and the rejection | Lock, shrink sweep, reject hit |
| 7 | 0:74.4-0:86.1 | A fact changes the model | "The rule alone has one equilibrium model: empty." | Rule-only vs rule+fact columns; remove-fire/remove-smoke tests | Two stable models and the removal tests | Fact stamp, two removal clicks, resolve |
| 8 | 0:86.1-0:98.5 | Add time | "At each instant, HERE is a subset of THERE." | Three time columns; H/T traces; X and G arrows; infinite-trace ellipsis | The H/T trace grid | Pulse, three time ticks, lift |
| 9 | 0:98.5-0:107.7 | The route | "Two levels of truth, a minimality test, a time axis." | Four-step route; source card; small raymotion wordmark | The route from HT to temporal stable models | Resolve, logo bell |

Rules: one idea per chapter; every proof object is a set diagram or a truth table, not a
caption; the stacked H/T pair is the recurring motif.

## 4. Script

Full caption script, line timing, frame cue map and source-to-chapter mapping:
`docs/here-there-explainer-v2-script.md`.

- Voice: **none**. The text-only cut is deliberate: the viewer needs to read the set
  membership and the checks at their own pace; a voice would compete with the table.
- Captions are the script. One line at a time, **2.5 words/s**, with the current line's
  words revealed in ink and the rest at 35% ink. No karaoke glow or selection box.
- The first line is the hook and must survive muting: "A rule says: if fire, then smoke."
- Every chapter holds its last line for at least 0.4 s before the crossfade.
- `Cue map`: line start = scene start + cumulative words / 2.5 s; see the script doc for
  every frame.

## 5. Sources and claims

| Claim / number | Value used | Source (URL, file, table) | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| `fire -> smoke` has classical models `{}`, `{smoke}`, `{fire,smoke}` | 3 models | Cabalar 2021, sec. 2 opening; signature `{fire,smoke}` | model cards | exact for this signature |
| HT interpretation is `(H,T)` with `H subset T` | 2 components | Cabalar 2021, pp. 4-6; Aguado et al. 2013, sec. 2 Def. 1 | H/T panels | exact |
| One atom has three allowed states | 3 states | Consequence of `H subset T`; exhaustive for one atom | 2x2 grid | exact |
| Implication is checked at `(H,T)` and `(T,T)` | 2 checks | Implication clause in Cabalar 2021 and Aguado et al. 2013 | four-case table | exact for the shown examples |
| `not p` holds iff `p` is absent from `T`; excluded middle can fail | truth values | Cabalar 2021, p. 5; Aguado et al. 2013, sec. 2.1 | negation panel | exact |
| Equilibrium: total `(T,T)` with no smaller `H` still a model | definition | Cabalar 2021, Def. 1; Aguado et al. 2013, Defs. 9-10 | smaller-H test | exact |
| Rule-only stable model | `{}` | Cabalar 2021, Definition 1, applied to the rule | stable-model card | exact |
| Rule + fact stable model | `{fire, smoke}` | Our worked extension of Cabalar's example; flagged "worked example" | stable-model card | illustrative / our extension |
| Temporal H/T traces; `X` next, `G` always; smaller-H test over traces | schematic | Aguado et al. 2013, secs. 2-3 | trace grid | schematic; three-position prefix of an infinite trace |

Source links:

1. Pedro Cabalar (2021), *Temporal ASP: from logical foundations to practical use with
   telingo*, sec. 2, pp. 2-6: <https://www.dc.fi.udc.es/~cabalar/RW2021.pdf>
2. Felicidad Aguado, Pedro Cabalar, Martin Dieguez, Gilberto Perez and Concepcion Vidal
   (2013), *Temporal Equilibrium Logic: a survey*, secs. 2-3:
   <https://www.dc.fi.udc.es/~cabalar/tel-dp60.pdf>

The film paraphrases the sources; all diagrams and the worked sequence are our own. It is
not authored or endorsed by the authors. HT/equilibrium are not attributed as Cabalar's
inventions; the v1 source note remains the authority.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | `BG #0B1421`; `PANEL #152536`; `INK #EAF0F6`; `DIM #9AAEC2`; **cyan #47DAD8 = HERE / established**; **gold #FFC365 = THERE / assumed**; **green #73E2A7 = pass / selected**; **coral #FF788B = fail / rejected**. Never use green for "true" without a label, or cyan/gold for mood only. |
| Type | Inter via `fonts::ui(false/true)`. Kicker 22; progress 22; headline 54; subhead 26; diagram label 22-28; set text 32; caption 30; footnote/source 19. |
| CJK | n/a; English-only. |
| Margins / safe area | 96 px side margins; top title 100; diagram 250-760; caption 810-890; source 1000; no content outside 96-1824. |
| Caption box | One line at a time, max width 1580, centered at x=960, y=826; 2-line wrap if needed; no box behind the glyphs. |
| HUD / persistent frame | Kicker top-left, progress top-right, thin progress line at y=92; source line at y=1000; chapter title at 100. |
| Motion grammar | 12-frame out-cubic reveals; 10-frame fade transitions; SFX/pass marks pop in 6-8 frames; no idle motion except the progress line. |
| Motif | The stacked **HERE over THERE** pair with a subset bracket; each chapter puts a different object into the same pair. |
| Charts / maps | Set diagrams and truth tables, not axes. Units are sets/atoms; labels use `H = { ... }`, `T = { ... }`; no chart grid. |
| Text performance | 2.5 words/s; current words in `INK`, upcoming at 35% `INK`; one line at a time; no per-word glow/box; line holds until the next line starts. |
| Brand kit | Small `raymotion-logo` (size 120, horizontal, tagline "video as code") on the end card at bottom right; source URLs on the end card; no social row (explainer packaging). |

Density rule: one idea per chapter. The 2x2 state grid and the four-case table are the
densest screens; they are allowed because each cell is a set membership/check, not prose.

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | None. The text-only cut is deliberate; captions carry the script. |
| Score | Generated `procgen::Score`: low pads per chapter, a two-note interval for the H/T pair, ticks for reveals, bells for passes, a low hit for a failed check, a resolve for the end card. Not a generic loop; it follows the cue map. |
| Ducking | n/a (no voice); keep the bed below the SFX. |
| SFX | Rule click at ch1/0; three model ticks at ch1 lines; subset stroke at ch2; forbidden buzz at ch3; pass/fail marks at ch4; switch clicks at ch5; lock/reject at ch6; fact stamp/removal clicks at ch7; time ticks at ch8; resolve/logo bell at ch9. |
| Silence | None declared; the bed is quiet and continuous. |
| Loudness target | -16 LUFS integrated, true peak <= -1.5 dBTP; verify after render. |

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Signature | `{fire, smoke}` | All model cards and chips | Cabalar example; exact |
| Classical models of `fire -> smoke` | 3 | three model cards | exact for this signature |
| Atomic states | 3 allowed + 1 forbidden | 2x2 grid | `H subset T`; exact |
| Implication cases | 4 | four-case table | two H values x two T checks; exact |
| Rule-only stable model | `{}` | rule-only column | equilibrium applied to the rule |
| Rule + fact stable model | `{fire, smoke}` | rule+fact column | our worked extension; labelled |
| Temporal prefix | 3 instants + ellipsis | three time columns | schematic of an infinite trace |

One change recomputes its consequences: changing the signature would require redoing the
model cards, the chips and the four-case table; changing the rule would invalidate the
stable-model examples. The code keeps these values in one data table.

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | generated in `c.assets` | `out/ht-v2-assets/score.wav` | regenerate from the cue map; the film is silent only if generation fails |
| Voice | n/a | - | - |
| SRT | generated in `c.assets` | `out/here-there-explainer-v2.srt` | derive from the caption table |
| Fonts | engine bundled Inter | `fonts::ui` | fail clearly if the engine font cannot load |
| Thumbnail | generated still from `here-there-explainer-v2-thumb` | `out/here-there-explainer-v2-thumb.png` | render a still at frame 30 |

Clean-machine test: `list` must work from any directory; the score and SRT regenerate
when `out/ht-v2-assets/` is empty; no external images or fonts are required.

## 9. Cue map

The complete line-level map is in `docs/here-there-explainer-v2-script.md`. Scene starts
and the score use the same table:

| Frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 0 | Rule pill + three model cards | Rule click | ch1 line 1 |
| 84 | Check marks on three models | Three ticks | ch1 line 2 |
| 228 | "Invented" tags on two models | Low question | ch1 line 3 |
| 374 | H/T stack + subset bracket | Two-note interval | ch2 line 1 |
| 458 | HERE/THERE chips | Subset stroke | ch2 line 2 |
| 578 | "one instant, not now/later" note | Soft tick | ch2 line 3 |
| 724 | 2x2 state grid | Three ticks | ch3 line 1 |
| 832 | Three allowed states | State ticks | ch3 line 2 |
| 892 | Forbidden cell crossed | Forbidden buzz | ch3 line 3 |
| 1026 | Four-case grid starts | Click | ch4 line 1 |
| 1134 | Case 1/2 checks | Pass/fail marks | ch4 line 2 |
| 1230 | Case 3 check | Fail mark | ch4 line 3 |
| 1350 | Case 4 check | Pass mark | ch4 line 4 |
| 1436 | Middle-state pair | Switch clicks | ch5 line 1 |
| 1508 | p truth row | Low click | ch5 line 2 |
| 1628 | not-p truth row | Minor chord | ch5 line 3 |
| 1760 | Excluded-middle card | Resolve-to-fail | ch5 line 4 |
| 1858 | Total model + lock | Lock | ch6 line 1 |
| 1942 | T fixed; H shrinks | Shrink sweep | ch6 line 2 |
| 2002 | Smaller-H model | Check | ch6 line 3 |
| 2122 | Reject stamp | Reject hit | ch6 line 4 |
| 2232 | Rule-only column | Fact stamp | ch7 line 1 |
| 2328 | Fact fire lands | Fact hit | ch7 line 2 |
| 2376 | Remove-fire/remove-smoke tests | Two clicks | ch7 line 3 |
| 2520 | Only `{fire, smoke}` survives | Resolve | ch7 line 4 |
| 2582 | Three time columns | Pulse | ch8 line 1 |
| 2642 | H/T traces | Three ticks | ch8 line 2 |
| 2750 | X/G arrows | Arrow tick | ch8 line 3 |
| 2822 | Smoke-next cell | Lift | ch8 line 4 |
| 2956 | Four-step route | Resolve | ch9 line 1 |
| 3076 | Source card + wordmark | Logo bell | ch9 line 2 |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 80 | 1 | Rule pill, three model cards, no overlap; captions inside 810-890 |
| 190 | 1 | Check marks; "invented" tags clear; text not clipped |
| 430 | 2 | H/T stack, subset bracket, chips; H subset T readable |
| 920 | 3 | 2x2 grid; forbidden cell has a cross and label; no caption collision |
| 1410 | 4 | Four cases readable; H/T checks aligned; case 3 highlighted |
| 1800 | 5 | p/not-p rows; excluded-middle card; no overlap |
| 2160 | 6 | Total model, lock, smaller H; reject stamp not covering text |
| 2500 | 7 | Rule-only vs +fact columns; removal tests readable |
| 2920 | 8 | Three time columns; H/T traces; X/G arrows |
| 3230 | 9 | Route and source card; wordmark at bottom right; source legible |

Plus: one still from the middle of every transition (e.g. 379, 729, 1031, 1441, 1863,
2237, 2587, 2961) and one encoded frame from the final MP4.

## 11. Acceptance checks

- [ ] Muted, the argument is legible from the set diagrams and captions.
- [ ] Every chapter's proof object is visible at the stated frame.
- [ ] Every number/claim matches the source ledger; the fact example is labelled as a
      worked extension and the temporal diagram as schematic.
- [ ] Captions stay inside 96-1824 and do not overlap the diagram or source line.
- [ ] The score and picture share the cue map; scene starts come from `node->start`.
- [ ] `list` works from another directory; score/SRT regenerate from an empty asset dir.
- [ ] Duration, frame count, decode, loudness and true peak are recorded.
- [ ] Captions use one constant 2.5 words/s rate and the house text style.
- [ ] Thumbnail + SRT + source/credits exist; wordmark is present but secondary.
- [ ] Authoredness >= 22 and Craft >= 13; showcase target >= 30 and >= 16.
- [ ] Thumbnail is readable at 320x180.

## 12. Risks / non-goals

- **Risks:** the four-case grid and the 2x2 state grid are dense; the `subset`/arrow
  glyphs may not be in the font; the runtime is around 108 s; a text-only lecture can
  feel static if the diagrams do not animate enough.
- **Non-goals:** this is not a full HT/TEL proof, not a general solver, and not a
  demonstration that physical smoke is impossible. The worked models are exact only for
  the shown signature; the temporal diagram is a three-position prefix of an infinite
  trace.
- **Out of scope:** voice, 3D, historical claims about the inventors, and any claim that
  HT is a proof procedure.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | author-agent self-review | 2026-10-26 | approved |
| Script / cue map | author-agent self-review | 2026-10-26 | approved |
| Stills | pending | - | - |
| Draft | pending | - | - |
| Final QC | pending | - | - |
