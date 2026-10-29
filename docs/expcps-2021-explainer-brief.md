# The Overload Has a Backstory — authoring brief

> Status: roadside transformer and stepped connector revision built and rendered. Final QC is recorded in `docs/expcps-2021-explainer-qc.md`.

| | |
| --- | --- |
| Composition name | `expcps-overload` |
| Story mode | explainer with a research-promo close |
| Owner / agent | Codex |
| Status | roadside utility-pad and connector revision built; picture and technical checks complete |
| Runtime target | 108.5 s final (3,254 frames at 30 fps), timed from measured narration |
| Platform / aspect | 1920 × 1080, 30 fps, web/conference, with a 1080 × 1350 crop considered only after the main cut |
| Voice | English narration, one generated neutral-voice file per chapter; not a clone of the author |
| Audience | energy and knowledge-graph researchers plus technically curious viewers; no prior ExpCPS knowledge assumed |
| Source material | `D:/code/phd/papers/2021-explainable-cpes-knowledge-graph/paper.pdf`, DOI 10.1145/3470481.3472704 |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | explainer |
| Goal | make the paper's simulated overload and explanation mechanism legible |
| Proof objects | typed CityJSON ten-building Flex settlement and transformer gauge, two traced causal routes, topology-to-rule join, an animated `dg::Diagram` backward event walk, an animated prototype result tree |
| Required evidence | section/figure-level ledger below; labels distinguish the simulated scenario, prototype output, and future work |
| Text | one brief headline per shot, phrase-timed captions, SRT; narration supplies connective detail |
| House grammar | physical grid first, evidence trail second, semantic mechanism third; no generic node cloud |
| Packaging | title, paper citation/DOI, source card, thumbnail, SRT, and optional 'Read the paper' closing link |

## 1. One sentence

This is a film about a transformer alarm acquiring a backstory: the viewer follows two trails across a simulated village, then watches a knowledge graph turn system rules and timed events into a candidate explanation.

## 2. One-line argument

ExpCPS can connect a smart-grid alarm to events beyond the power grid, but the 2021 prototype still leaves aggregation and complex causal patterns unresolved.

## 3. Chapters

Final chapter boundaries come from `assets/vo/expcps-overload/timing.json` plus a short end hold. The target ranges below have been replaced with measured frame boundaries.

| # | Target time | Chapter | On-screen text | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0–14.2 s | The alarm | `WHY IS THE TRANSFORMER LOADED?` / `SIMULATED FLEX SCENARIO` | A ten-building CityJSON settlement rises in a fixed viewport: pitched-roof homes, apartment blocks, a solar-roof home, a service building, streets, and a cabinet transformer on its pad. H1/H2 carry the amber price branch and H4 the blue solar branch; load crosses the scenario's 60% recommended-operation threshold. | Typed CityJSON architecture with exactly ten Building objects, tied to the transformer gauge. | Low transformer hum, one restrained warning pulse. |
| 2 | 14.2–28.7 s | Cheap power | `A PRICE SIGNAL CHANGES DEMAND` | Market price falls; an aggregator message reaches two participating houses; their consumption lines rise toward the transformer. | Visible message route plus two household meter traces. | Short data-grid ticks become two load pulses. |
| 3 | 28.7–43.8 s | Dark roof | `RAIN CUTS SOLAR OUTPUT` | Rain covers a solar house; generation drops, an empty battery cannot cover demand, grid draw grows. The two trails now converge at the transformer. | Roof-solar-battery-grid cutaway, not another node diagram. | Rain texture and a descending solar tone; converging bass hit. |
| 4 | 43.8–60.6 s | The stitch | `A RULE BECOMES A LOCAL LINK` | An abstract rule (power can affect load) is placed over the actual network topology; only connected device pairs pass the SPARQL condition and enter the graph. | Two-part join: rule card × topology path → concrete causal link. | Dry locking sounds on accepted pairs; no sound on rejected pairs. |
| 5 | 60.6–76.8 s | Walk backward | `FROM ALARM TO PRIOR EVENTS` | An actual `dg::Diagram` builds from overload backward toward transformer power, house power, and Flex message; a highlight trace follows the candidate path in reverse-time order. | Animated event graph whose visited events stay attached to their source devices and time order. | Receding step pulse, one per visited event. |
| 6 | 76.8–95.4 s | What the prototype showed | `BEYOND THE POWER GRID` then `STILL A PROTOTYPE` | An actual `dg::Diagram` reveals the Figure-9-inspired result tree; `trace` follows the cross-domain branch while the R1/R2/R3 assessment stays adjacent. | Animated result tree with two checks (R1/R2) and an unfilled grouping bracket (R3). | Motif resolves on two notes; last note stays open. |
| 7 | 95.4–108.5 s | Close | `AN ALARM IS THE START OF AN EXPLANATION` | Return to the same CityJSON village. T1 sits on a fenced utility pad south of the sidewalk, outside the carriageway; amber and blue `city::trail` paths turn at the sidewalk and run along it before reaching the pad. Source and DOI appear. | The alarm and its street-routed evidence trails seen together in the same modeled place; source card. | Quiet release and brief outro bell. |

## 4. Script and cue map

Narration was generated in 21 sentence takes, concatenated into seven chapter WAVs, and measured. Final chapter lengths include 38 frame holds, with a 56 frame closing hold. The script below is the spoken text; the local TTS inserts spelled-out pronunciation for ExpCPS and 2021.

| # | Spoken text | On-screen caption sequence | Target voice time |
| --- | --- | --- | --- |
| 1 | "A transformer alarm tells an operator something is wrong. It does not tell them why. In this simulated settlement, the load passes a threshold set for regular operation." | `An alarm is a symptom.` → `What led to it?` | 12.90 s |
| 2 | "One trail begins outside the grid. An energy-price drop triggers a flexibility message. Two participating homes use more electricity, adding to the transformer load." | `Price drops` → `Message sent` → `Demand rises` | 13.30 s |
| 3 | "Another trail starts on a roof. Rain reduces solar generation. With its battery empty, the house draws from the grid. Both trails meet at the same transformer." | `Rain` → `Solar falls` → `Grid draw rises` | 13.78 s |
| 4 | "ExpCPS joins these kinds of facts in a knowledge graph. Experts provide a general causal rule. A topology query checks which real devices are connected, turning that rule into specific links." | `General rule` → `Actual topology` → `Specific link` | 15.52 s |
| 5 | "Starting from the alarm, the algorithm walks backward through possible causes. It finds earlier events on connected devices and builds an explanation tree. The trail can reach beyond the nearest sensor." | `Start at the alarm` → `Follow earlier events` → `Build a tree` | 14.90 s |
| 6 | "The prototype's tree includes flexibility messages from the data grid and events deeper than the immediate cause. But the paper says related paths still need grouping. This is a demonstrated prototype, not a complete account of every cause." | `Data grid: shown` → `Deeper trace: shown` → `Grouping: still needed` | 17.31 s |
| 7 | "The useful shift is from an isolated warning to a traceable chain of evidence. That is the promise explored in the 2021 ExpCPS paper." | `From warning to traceable evidence` → `Aryan et al. · MSCPES 2021 · DOI` | 11.18 s |

**Cue-map contract:** Each chapter has one voice file. Its actual duration plus a short measured hold determines the chapter duration. Picture and score events attach to the phrases listed in the caption sequence, in that order. The start of the next chapter is the current scene's `node->start + dur`; no parallel hard-coded clock. On-screen captions follow the same phrase cues, and the SRT is generated from them. The terms `simulated`, `prototype`, `potential cause`, and `future work` remain visible when relevant.

## 5. Sources and claims

| Claim / number | Value used | Paper location | On-screen treatment | Scope |
| --- | --- | --- | --- | --- |
| Publication | Aryan et al., MSCPES 2021, DOI 10.1145/3470481.3472704 | title page/citation | closing source card | exact |
| Settlement size | 10 buildings | §2, Figure 1 | `SIMULATED FLEX SCENARIO` and small `10 buildings` map key | scenario, not field deployment |
| Overload threshold | 60% recommended regular-operation load | §2, Figure 8 | gauge label `scenario threshold 60%` | not a universal transformer limit; no invented measured peak |
| Market branch | overproduction elsewhere → price drop → flexibility message → H1/H2 increased consumption | §2, Figure 1, events e1/e3/e4/e5 | staged path, `scenario` label | scenario chain |
| Weather branch | rain → less solar at H4; battery empty → grid draw | §2, Figure 1, events e2/e6 | roof/battery cutaway, `scenario` label | scenario chain |
| Response | transformer load event e7 → charging station reduces charging current e8 | §2, Figure 1 | brief response action, no claim of proven stabilization | scenario action |
| Framework | topology, event, and causality knowledge integrated in a graph | §3.1–3.3, Figures 2–3 | three material layers converge | design/implementation |
| Rule grounding | general property rule plus SPARQL topology condition yields device-specific causal relation | §3.3, Algorithm 1; §4.2, Figures 6–7 | rule × topology proof object | method; does not discover causal rules automatically |
| Explanation algorithm | backward traversal from an event through potential-cause devices and earlier events | §3.4, Algorithm 2 | queue/tree sequence | candidate graph, not causal proof |
| Prototype result | Figure 9 includes FLEX-MESSAGING and deeper-than-adjacent causes (R1/R2); grouping R3 requires post-processing | §4.4, Figure 9 | two checks and one open bracket | reported outcome, not a quantitative benchmark |
| Limits | no causal loops; scaling to real measurements and formal analysis future work | §2 assumptions; §6 | final `prototype` / `future work` card | limitation |

No numerical performance gain, accuracy measure, real-world deployment, automatic root-cause certainty, or resolved R3 will be claimed. Illustrative traces and scenery will carry an `ILLUSTRATIVE SCHEMATIC` label.

The CityJSON file `assets/data/expcps/flex.city.json` is an authored, illustrative spatial model of the ten-building Flex scenario in §2/Figure 1. Its coordinates, footprints, heights, building-type mix, roof forms, windows, solar panels, trees, streets, and parcel layout are invented for the film; it is not BIFROST source data or a reconstruction of real geography. Only the ten-building count and H1/H2/H4 roles come from the paper. The viewport carries `ILLUSTRATIVE CITYJSON MODEL` on screen. The transformer is `CityFurniture` on a fenced roadside utility pad, excluded from the ten-building count. The final path ribbons use `city::trail` along the south sidewalk and a short perpendicular pad spur. Diagram edges use fixed orthogonal waypoints, not the straight auto-route.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | red = transformer alarm; amber = market/message branch; blue = weather/solar branch; teal = graph link accepted by rule/topology; gray = unconfirmed or future work |
| Type | UI bold ~80 px headline, ~48 px captions, ~30 px labels, ~25 px source/footer |
| CJK | n/a; English/Latin characters only |
| Margins / safe area | 110 px horizontal, 80 px vertical; 100 px lower caption clearance |
| Caption box | bottom band y≈850–990, max two lines; no diagram labels behind it |
| HUD / persistent frame | small `SIMULATED FLEX SCENARIO` label until result chapter; chapter title and source footnote where needed |
| Motion grammar | physical flows follow paved routes with right-angle turns into the utility pad; explanation trace reverses direction through fixed stepped diagram edges. 10–16 frame entrances; 8–12 frame holds after each causal step; no idle floating nodes |
| Motif | two colored trails converging at one red transformer, then unwinding backward as an evidence path |
| Charts / maps | an angled, typed CityJSON settlement view; a threshold gauge; and `dg::Diagram` event/path and result-tree animations. The settlement and traces are explicitly illustrative; no fabricated time series or y-axis values |
| Text performance | ~135–145 spoken words/minute; captions are phrase summaries, not karaoke boxes |
| Brand kit | paper title/DOI and subtle raymotion mark only on close; no unrelated social row |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | neutral English narration, generated as one WAV per chapter; measure each file before fixing scene lengths. Do not clone the author's voice without a separate choice. |
| Score | original restrained pulse motif: two branch pitches converge at the transformer, then reverse during the explanation walk; generated deterministically in `c.assets` |
| Ducking | score ducks under each measured voice span via `volumeAt`, with short attack/release |
| SFX | alarm at chapter 1; message ticks in 2; rain/solar fall in 3; pair-locks in 4; traversal ticks in 5; incomplete cadence in 6; release bell in 7 |
| Silence | short pause before the caveat, to separate demonstrated result from future work |
| Loudness target | approximately -16 LUFS integrated, true peak ≤ -1 dBTP, verified on final MP4 |

## 7b. Scenario coherence

| Quantity / condition | Value | Depends on | Source or assumption |
| --- | --- | --- | --- |
| Settlement | 10 buildings | village map | §2, scenario |
| Regular-operation threshold | 60% load | transformer gauge | §2 and Figure 8, scenario threshold |
| Simulated paths | price-message-demand; rain-solar-battery-grid | event arrows | §2 and Figure 1 |
| Prototype output | FLEX-MESSAGING visible; grouping still needed | result chapter | §4.4 and Figure 9 |
| Peak load, kW, event timestamps | omitted | no numerical chart | paper does not report a coherent quantified trace for this film |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Flex settlement | authored CityJSON v2.0 with ten typed Building objects (homes, apartments, service building), a transformer CityFurniture cabinet on a fenced pad outside the road, pitched and flat roofs, windows, H4 solar panels, lane/footpaths, ground and trees; loaded by `city::load`, rendered via `city::add`/`g3d::draw` | `assets/data/expcps/flex.city.json` | fail clearly if missing; opening/close depend on it |
| Animated method diagrams | `dg::Graph`/`dg::Diagram` nodes and fixed right-angle `Edge.points` authored from the paper's event chain and Figure 9 structure; `build` and `trace` are timed to chapter cue map | composition code | fail build if API changes; no static-diagram fallback |
| Transformer gauge, roof, battery, graph | procedural drawings | composition code | always available |
| Voice | generated, committed per chapter after script lock | `assets/vo/expcps-overload/` | clear build error for missing WAV; `list` still works |
| Score/SFX | deterministic generated WAV in `c.assets` | generated assets directory | draft can be muted; final requires audio |
| Captions/SRT | generated from measured phrase cue map | `out/expcps-overload.srt` | fail release if absent |
| Paper figures | redraw ideas rather than reproduce PDF images | paper cited in brief/end card | source ledger remains available |
| Fonts | bundled raymotion UI fonts | engine | engine fallback |

**Clean-machine test:** run `list` from another directory without generated assets; registration must still report composition metadata. Missing voice must be caught at draft/render with a clear path and not silently dropped in the final cut.

## 9. Cue map

Final numeric frames will be computed after voice generation. The shot order and cue ownership are fixed here.

| Chapter phrase | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| `transformer alarm` | threshold gauge enters red | warning pulse | chapter 1 voice cue |
| `price drop` / `message` / `two homes` | price ticker / data-grid arrow / two meters | three data ticks | chapter 2 voice cues |
| `rain` / `battery empty` / `grid` | roof cutaway / battery empties / grid cable lights | rain / descending tone / convergence | chapter 3 voice cues |
| `general rule` / `topology query` / `specific links` | rule card / topology overlay / accepted connections | three locks | chapter 4 voice cues |
| `starting from the alarm` / `earlier events` / `tree` | reverse walk / temporal filter / tree growth | traversal pulses | chapter 5 voice cues |
| `includes flexibility messages` / `still need grouping` | Figure-9-style result / open bracket | partial cadence / pause | chapter 6 voice cues |
| `traceable chain` / `2021 paper` | full trail / DOI source card | release bell | chapter 7 voice cues |

## 10. Still frames to review

At 30 fps, reviewed encoded chapter start/middle/end frames: 0/212/409; 425/643/846; 862/1088/1298; 1314/1566/1802; 1818/2061/2288; 2304/2583/2846; 2862/3058/3238. Reviewed fade midpoints: 418, 855, 1307, 1811, 2297, 2855, 3247. Diagram build/trace progression was inspected at 1840/1900/1980/2061/2180 and 2350/2400/2480/2583. Roadside pad and `city::trail` turns were inspected at 2880/2930/2960/3058/3150 and in full-resolution stills. See `out/expcps-v5-qc-sheet.png`.

**Chosen thumbnail:** encoded frame at 104 s, with the CityJSON settlement, the two colored evidence branches, and the headline `AN ALARM HAS A BACKSTORY`. Exported to `out/expcps-overload-thumbnail.png`; checked at 320 × 180.

## 11. Acceptance checks

- [ ] Muted, a viewer can reconstruct both scenario trails and distinguish them from the prototype result.
- [ ] Each chapter's proof object is visible at its middle and before transition; CityJSON forms read as distinct house, apartment, service and transformer types at 1:1 and 320×180.
- [ ] The backward event walk and result tree visibly animate through `dg::Diagram` actions rather than hand-drawn static connectors.
- [ ] T1/pad does not occupy the carriageway; all final evidence ribbons visibly turn at the sidewalk and terminate at the cabinet; diagram branches use right-angle waypoints.
- [ ] The 60% label says `scenario threshold`; no invented load curve or measured peak appears.
- [ ] The result says R1/R2 demonstrated and R3 still needing post-processing.
- [ ] Captions clear the diagrams and read at the planned rate; SRT matches the measured cue map.
- [ ] Score and picture use the same phrase cue map, and voice length sets scene duration.
- [ ] `list` works from a different directory without generated assets.
- [ ] Still review, muted and sound draft review, ffprobe, full decode, loudness/true peak, and encoded-frame checks are recorded.
- [ ] Final cut meets Authoredness ≥ 30 and Craft ≥ 16, no critical fail, Gates 0–7.
- [ ] The chosen thumbnail reads at 320 × 180.

## 12. Risks / non-goals

- **Risks:** the paper's conceptual scenario includes a rain path, but its displayed prototype result emphasizes FLEX-MESSAGING; the film must not show the full scenario as a measured result. The 60% threshold belongs to this simulation. The explanation tree indicates potential causes, not established causal certainty. Voice pronunciation of `ExpCPS`, `BIFROST`, and `SPARQL` needs review.
- **Non-goals:** a general tutorial on RDF/SPARQL, a quantified grid-performance claim, a live demonstration of a deployed utility system.
- **Out of scope:** causal loops, cascading blackouts, and completed user-specific explanation summarization, which the paper identifies as future needs.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief/source audit | Codex | 2026-10-27 | complete; concept presented for user review |
| Script / cue map | Codex | 2026-10-27 | provisional wording and phrase order complete; timing awaits voice |
| Concept direction | User | 2026-10-27 | approved |
| Stills | Codex | 2026-10-27 | reviewed |
| Draft | Codex | 2026-10-27 | visual sequence and mixed-audio analysis complete |
| Final QC | Codex | 2026-10-27 | recorded separately |
