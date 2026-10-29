# Three Levels of Causality — authoring brief

> Status: self-reviewed production brief, 2026-10-27. Story mode: explainer.

| | |
| --- | --- |
| Composition | `three-levels-causality` |
| Owner / reviewer | Codex; final editorial review by user |
| Runtime / platform | 56 s, 1,680 frames at 30 fps, 1920 × 1080, web/presentation |
| Voice / audience | text-only cut; smart-grid and knowledge-graph researchers and new ExpCPS viewers |
| Sources | `D:/code/phd/papers/2020-simulation-support-explainable-cpes/paper.pdf`; `D:/code/phd/papers/2021-explainable-cpes-knowledge-graph/paper.pdf` |

## Mode contract, conceit, argument

**Explainer contract:** a concrete proof object per chapter, section-level source ledger, cue-paced labels, distinct visual grammar, title/source card, SRT and thumbnail.

**Conceit:** One causal thread changes form three times, then a knowledge graph stitches its rule, connected devices and recorded event into a traceable explanation.

**Argument:** A property rule, a grounded device relation and an event trail describe different levels of the same smart-grid situation; KG predicates connect the three levels without making a fourth causal level.

## Chapters and proof objects

| # | Time | Chapter / on-screen text | Visual action and proof object | Sound |
| --- | --- | --- | --- | --- |
| 1 | 0–4 s | `ONE SYSTEM / THREE CAUSAL LEVELS` | Three nested lens frames form around one thread; this physical metaphor names the levels. | Three pitched clicks. |
| 2 | 4–15 s | `01 / PROPERTY RULES` | Animated `dg::Diagram`: weather → solar intensity → generated power; consumed power → transformer loading. Arrows connect properties with no device names. | High plucks follow blue edges; low pulse at loading. |
| 3 | 15–28 s | `02 / CONNECTED DEVICES` | PBR CityJSON solar carport, distribution substation, EV charger and e-car plus native EV-driver figure. A wide grid view and a charger/car close-up share the screen; several property chips dock to each device and orthogonal routes show topology. | Mechanical latches as links ground. |
| 4 | 28–38 s | `03 / EVENTS IN TIME` | Device-anchored event ladder: active charge session stays distinct from rain → reduced PV output → transformer load event → charging-current reduction. Gauge marks the scenario's 60% threshold and an exceedance indicator without inventing a peak value. | Rain texture, one alarm beat, falling charge tone. |
| 5 | 38–52 s | `04 / THE KNOWLEDGE GRAPH` | Three sequential native `dg::Diagram` views: the named rule `Causality_2` with `source`, `target`, `context`; Figure 7's SPARQL feature variables with `hasConnection` and type constraints; and Figure 5/8's named transformer, loading sensor, observation and overload event with `hasReference`. Predicate labels and identifiers are the proof objects. | Distinct short tones for rule, topology and recorded evidence. |
| 6 | 52–56 s | `THREE LEVELS / ONE KNOWLEDGE GRAPH` | Three representations align within one KG frame; source and DOIs enter. | Three-note resolution. |

## Text-only script and cue map

**Voice: none.** The labels are the complete script. The score carries material and temporal distinctions. Cue seconds are relative to chapter starts; one code table drives pictures, sound and SRT.

| Chapter | Cue seconds | Caption / visual cue |
| --- | --- | --- |
| 1 | 0, 1.2, 2.4 | `ONE SYSTEM` / `THREE CAUSAL LEVELS` / `PROPERTY • DEVICE • EVENT`; lenses enter in that order |
| 2 | 0, 2.2, 5.2, 8.0 | `A rule links properties` / `Weather changes solar intensity` / `Solar intensity affects generated power` / `Consumed power affects transformer loading`; diagram builds |
| 3 | 0, 3.0, 6.2, 9.2 | `The rule meets a real topology` / `One device carries several properties` / `Connected devices make the link local` / `Power reaches the charger and car`; city, chips, routes reveal |
| 4 | 0, 2.1, 5.0, 7.8 | `Now the system records events` / `Rain lowers solar output` / `Transformer load exceeds the scenario threshold` / `Charging current is reduced`; event ladder builds |
| 5 | 0, 3.0, 6.0, 9.0, 12.0 | `The graph keeps the three levels together` / `A rule names source, target and context` / `Topology grounds it in connected devices` / `An observation belongs to a sensor and device` / `The overload event references that observation`; KG lanes build and trace |
| 6 | 0, 1.8 | `Three levels. One knowledge graph.` / `Aryan et al. • MSCPES 2020 / 2021`; alignment and source card |

Each caption ends at the next cue or chapter boundary. `EV charging active` is illustrative context, with no arrow into the overload.

## Source and claim ledger

| Claim / value | Source | Screen treatment and scope |
| --- | --- | --- |
| Abstract property relation versus concrete relation between actual devices | 2021 §3.3, Algorithm 1 | distinct property and device chapters; framework definition |
| Solar intensity → generated power; generated/consumed power → loading level | 2020 §III–IV, Figure 4; 2021 §3.3 | directed property edges; general rules with conditions abstracted |
| Topology grounds a rule by filtering property-bearing device pairs | 2021 §3.3, Algorithm 1 | chip-to-device docking and physical links; no claim of automatic rule discovery |
| Rain lowers solar generation and raises house grid draw | 2021 §2, Figure 1, e2/e6 | blue simulated-scenario path |
| Transformer load event followed by charging-current reduction | 2021 §2, Figure 1, e7/e8 | red alarm then amber action; no claim of final stabilization |
| 60% recommended-operation threshold | 2021 §2 and Figure 8 | `SCENARIO THRESHOLD 60%`; not a universal rating or invented peak |
| Explanation traces prior events on connected devices | 2020 §III.C–IV; 2021 §3.4, Algorithm 2 | event ladder is a candidate path, not proof of a unique cause |
| Abstract rule `Causality_2` has `source ACTIVE_POWER_3P`, `target LOADING`, `context TrafoGridCondition` | 2021 §3.3, Figure 6 | exact graph identifiers and predicates in KG rule lane |
| Device topology condition joins `?src_feat` typed `POWERGRID_CONNECTOR` to `?tgt_feat` typed `LV_TRANSFORMER` by `hasConnection` | 2021 §3.3, Figure 7 | separate SPARQL-variable view; rule grounding is conditional, not a fabricated named-instance triple |
| `LOADING_1` observes `LOADING` and is placed at `LV_TRANSFORMER_1` | 2021 Figure 5 | observation lane; semantic links, no fabricated measurement value |
| `OverloadEvent_1` refers to `LOADING_1_obs_1` through `hasReference` | 2021 Figure 8 | event evidence lane; no invented event timestamp or peak |
| Shapes, positions, event clock, active EV session | authored illustrative choices | persistent `ILLUSTRATIVE SCENARIO` label; no field measurements |
| Publication | 2020 DOI 10.1109/MSCPES49613.2020.9133700; 2021 DOI 10.1145/3470481.3472704 | closing source card; exact |

The film follows one weather branch; the 2021 paper also has a market/message branch. It uses no invented voltage relationship or numerical load peak.

## Visual system

| Element | Decision |
| --- | --- |
| Palette | blue `#6BBDE8` weather/solar; amber `#EFC16C` demand/charging; red `#F27575` alarm; teal `#66D7C0` grounded relation; gray inactive context |
| Type | bundled UI font: kicker 24, headline 64, cue 36, property 26, source 20 px at 1920 × 1080; English, CJK n/a |
| Layout | 100 px side margins, 70 px top, 65 px bottom; one-line caption box at x110/y888/w1700/h120 |
| Persistent frame | thin progress rail, level number, `ILLUSTRATIVE SCENARIO` footer |
| Motion | 12–20 frame entrances; diagram edges draw along directed/orthogonal paths; device links use elbows; events persist after onset; no idle node drift |
| Motif | the same luminous thread becomes rule edge, physical route and event line |
| Diagrams | native animated `dg::Diagram` with fixed positions for property, event and RDF predicate graphs; KG lanes carry identifiers rather than a generic node cloud |
| Reading rate | short phrase holds at least 2 s; no per-word boxes or glow |
| Brand | n/a; academic explainer uses source card, no logo or CTA |

## Audio plan

| Element | Decision |
| --- | --- |
| Voice / ducking | none; text-only cut, no voice ducking |
| Score | generated three-register motif via `procgen::Score`: high property tones, mid device latches, low event pulse, then KG tones connecting those registers; all cue-timed |
| SFX | glass ticks on edges, latches on links, rain-like texture before load, restrained alarm and descending charging tone |
| Silence | 0.2 s gap before overload separates observation from response |
| Target | about −19 LUFS integrated for the instrumental text-only cut; true peak ≤ −1 dBTP, verified after encode |

## Scenario coherence and assets

| Item | Decision / dependency |
| --- | --- |
| Threshold | 60% is scenario-specific; only the threshold mark is exact |
| Route | solar home → transformer → charger → vehicle is illustrative placement, showing topology |
| Event order | rain/lower PV output precedes load event, which precedes current reduction (2021 §2) |
| Charging session | shown active but not as the demonstrated overload cause |
| City models | existing `assets/data/city/smartgrid/` CityJSON with `smartgrid3d::cityLook()`; explicit render error if required file absent |
| Human | existing native `human3d::Role::EvDriver`; final requires it |
| Diagram | composition-local graph code; no external files |
| KG records | Figure 5–8 identifiers and relation names are from the 2021 paper; the topology-query variables are distinct from named instance triples; layout is illustrative, and no observation result/time is fabricated |
| Score/SRT | generated inside `c.assets` in `out/three-levels-causality-assets/` and `out/three-levels-causality.srt` |
| Fonts | loaded inside draw callbacks; UI fallback |

**Clean-machine check:** `list` works from a different current directory even if render assets are absent. Rendering a missing required CityJSON file fails clearly. A single chapter/cue data table controls scene starts, picture, score and SRT.

## Still list and acceptance

Review frames 0/60/112 (opening); 130/220/322/440 (property); 451/570/690/825 (city and links); 841/925/1030/1128 (events); 1142/1250/1360/1460/1550 (KG); 1570/1620/1670 (synthesis), plus chapter-transition midpoints and encoded samples. **Thumbnail candidate:** frame 1360, KG rule/topology/evidence lanes; also compare frame 770 with all device prefabs at 320 × 180.

- [ ] Muted, the three levels remain distinct and the same thread is traceable.
- [ ] Arrows have directions; device paths use elbows and land on meaningful objects.
- [ ] Prefabs are recognizable and sensibly scaled; transformer is on its pad.
- [ ] EV charging is not depicted as the demonstrated overload cause.
- [ ] The 60% mark says `SCENARIO THRESHOLD`; no invented peak or clock value.
- [ ] KG predicate arrows, identifiers and graph lanes are legible; the observation/event links match Figures 5–8.
- [ ] Labels do not overlap or leave the safe area; cues permit reading.
- [ ] `list` works elsewhere; score and SRT come from `c.assets`.
- [ ] Draft watched muted and with sound; final MP4 passes ffprobe, full decode, loudness/peak and encoded-frame checks.
- [ ] Authoredness ≥ 22, Craft ≥ 13, Gates 0–7, no critical fail; thumbnail readable at 320 × 180.

## Risks / non-goals / review

The weather branch is one of two contributions in the paper, so the film says it is one illustrative path. City models are authored illustrations, not BIFROST exports. This is not a numerical simulation, proof of a unique root cause, voltage model or full reproduction of Figure 9.

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief and source ledger | Codex | 2026-10-27 | approved for implementation; all required sections and proof objects present |
| Text script / cue map | Codex | 2026-10-27 | approved; one cue table contract |
| Stills, draft, final QC | pending | | pending |
