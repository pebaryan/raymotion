# own-your-compute-v2 — authoring brief

> Status: **voiceless preview built** (94 s, QC 33/36 authoredness, 17/18 craft). Voice, SRT and a dedicated
> thumbnail composition are pending; see `docs/own-compute-v2-qc.md`.
> This is a v2 of `examples/showcase/own_compute.cpp`, rebuilt around a script, a
> measured cost model, and a real offline demo.

| | |
| --- | --- |
| Composition | `own-your-compute-v2` |
| Owner / agent | `<author>` |
| Status | `draft` |
| Runtime target | **94 s** (2,820 frames at 30 fps) |
| Platform / aspect | 1920×1080, 30 fps, 16:9; landscape web / presentation |
| Voice | One cloned-voice file per act, 48 kHz mono; captions + SRT |
| Audience | developers and technical viewers who use hosted models daily and have considered a local machine |
| Source material | `examples/showcase/own_compute.cpp` (v1), `docs/own-compute-v2-script.md` |

## 1. One sentence

> This is a film about **owning the machine that runs your AI** that makes the viewer
> **feel the difference between a rented answer and a machine in the room** by
> **following one $92.97 bill from a cloud aisle to a GPU, a downloaded model, a
> break-even chart, a map, and a house under a dome.**

## 2. One-line argument

> The cloud is not evil; it is someone else's computer. At low use it is cheaper, at high
> use the machine wins, and the real difference is not the bill — it is who holds the box.

## 3. Chapters

Target times are a guide; final scene lengths are derived from the measured voice files.

| # | Time (s) | Chapter | On-screen text (short) | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0:00–0:10 | The bill | “Last month: $92.97” | 0:00–0:02.3 raymotion badge intro; then the fogged server aisle, bill counter to $92.97, token counters to 20.0M in / 5.3M out | The badge and the bill/token counts | Cold pad, meter ticks |
| 2 | 0:10–0:29 | The strings | “I don’t know which building answered.” | Glitch stamps: REGION CHANGED, TERMS UPDATED, SERVICE OUTAGE, YOUR DATA LEAVES THE ROOM; RGB split; shake | The stamps and a region label | Glitch stabs, alarm hits |
| 3 | 0:29–0:36 | The turn | “So I bought a machine.” | Point cloud pours out of the cloud aisle into a chip; the bill glitches away | The chip forming | Riser into impact |
| 4 | 0:36–0:43 | The machine | “300 W. Not free. Not silent. Here.” | Procedural GPU on a desk; fans spin up; DOF, shadows; spec chips read 32 GB VRAM and 300 W | The GPU and the 32 GB / 300 W spec | Fan spin-up, low hum |
| 5 | 0:43–0:51 | The model | “27 GB, downloaded once.” | Download card fills to 27.0 GB; a seven-layer network with a repeated 8-node middle block fires; the wifi icon goes dark | The file size, the repeated-block network, the offline icon | Download ticks, modem click |
| 6 | 0:51–1:08 | The trade | “At 4 h/day, about the same. At 1 h, cloud wins. At 8 h, the machine wins.” | Line chart draws: cloud cost vs local cost; break-even marker at ~3.9 h/day | The chart and the break-even | Cost tick pulse, riser |
| 7 | 1:08–1:22 | The map | “Your prompt leaves the room. This one doesn’t.” | Atlantic map: Vienna → a US cloud region, dotted arc, packets; then the local dome over Vienna | The cross-continent route vs the dome | Distance drone, whoosh |
| 8 | 1:22–1:27 | Offline | “No signal. No per-token meter. Still thinking.” | Terminal answers with the network off; $0.00 per query; cloud bill gone | The offline terminal | Key clicks, room tone |
| 9 | 1:27–1:34 | Home | “The cloud is someone else’s computer. This one is mine.” | Island at dawn; house, server, dome; title, tagline, and a social row (@pebaryan, GitHub/X/YouTube, repo) | The dome, the title, the social card | D-major lift, final bell |

Rules:

- Every chapter has a proof object. The chart and map are not decoration; they carry the
  argument.
- The chart is a **model**, not a measurement. Label the assumptions on screen.
- The map is an **illustrative route**, not a network trace. Label it.
- The film does not claim local is always cheaper or more private in the abstract; it
  says what is true for this scenario and this offline demo.

## 4. Script

Full narration, caption text, cue map and source ledger:
`docs/own-compute-v2-script.md`.

Summary:

- First-person narrator, past tense, one concrete bill and one machine.
- One idea per sentence; no jargon without a plain phrase first.
- The honesty beat (“at one hour the cloud wins”) is mandatory; it is what makes the
  ownership argument credible.
- Numbers carry units: “$92.97”, “25.3M tokens”, “300 W”, “27 GB”, “$89.87/month”.
- The script avoids “cloud bad / local good.” The cloud is fast and cheap at low use; the
  local machine is a different bargain.

## 5. Sources and claims

Full ledger and arithmetic: `docs/own-compute-v2-script.md` §Sources.

Key claims:

| Claim | Value | Source | On-screen |
| --- | --- | --- | --- |
| Frontier API price | $2/MTok input, $10/MTok output | Anthropic pricing (Sonnet 5.5), accessed 2026-10-26 | no; used in the model |
| Modeled cloud bill | 20.0M in + 5.3M out = $92.97 | arithmetic on the list price | yes |
| US residential electricity | 18.16 ¢/kWh, August 2026 | EIA Electric Power Monthly, Table 5.6.A | chart note |
| Model size | 27 GB (Qwen3.8 27B, Q8-class) | User-specified model; 27 GB is the requested illustrative file size, not a linked public file | yes (“27 GB”) |
| Data-centre regions | 39 regions, 124 AZs | AWS Global Infrastructure | map note |
| Hardware, power, hours | $2,000, 32 GB VRAM, 300 W, 4 h/day, 24-month amortization | **scenario assumptions** | chart note, “illustrative” |
| Map route | Vienna → a US cloud region | **illustrative**; AWS regions exist | map note |

The cloud bill is **modeled**, not a real invoice. The local cost is a **scenario**, not a
quote. The map is **illustrative**. These labels are part of the film, not just the
notes.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | `NIGHT #05060c` ground; `COLD #8fa6ff` cloud/usage; `ALARM #ff4a4a` risk; `TEAL #47dad8` local/ownership; `EMBER #ffb347` home. Colour meaning never swaps. |
| Type | Inter Regular/Bold for narrative; a mono face for terminal and data; title 140–160, captions 56–60, chart labels 24–30, footnotes 18–22. |
| Margins | 96 px safe area; letterbox 44 px top/bottom; captions at y ≈ 900; nothing crosses the caption box. |
| Persistent frame | The **meter**: bill counter → power meter → $0.00/query. It is the through-line; it changes meaning, not position. |
| Motion | 12–16 frame entrances; existing glitch grammar in act 2; chart draw-on; map route draw-on; no idle motion except the meter, LEDs, rain and the terminal cursor. Karaoke: 2.5 words/s in the caption accent, upcoming words dim, no glow or selection box; the text holds after the last word, independent of scene length. |
| Motif | The box: the chip, the GPU, the model file, the machine, the house. The same teal LED appears in all of them. |
| Density | One idea per screen. If the chart needs a paragraph, the chart is wrong. |

The v2 keeps the v1 look — it is a strong system — but adds a **data chart** and a
**map** that use the engine’s newer geo/plot features. The chart and map inherit the
palette; they do not introduce a new one.

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | One file per act, cloned voice (author), 48 kHz mono, trimmed; captions + SRT. Volume 1.0; final mix -16 LUFS ± 1, true peak ≤ -1 dBTP. |
| Score | Extend the v1 score: cold pads under the cloud, glitch stabs on the stamps, riser/impact into the machine, a cost-tick pulse under the chart, a low distance drone under the map, the 120 bpm groove, and the D-major lift at home. |
| Ducking | Bed to 0.55 under every voice span, 10-frame attack/release. |
| SFX | Bill ticks, glitch, fan spin-up, download complete, chart ticks, map whoosh, terminal keys, dome hum, final impact. All on picture cues. |
| Silence | Act 8 is near-silent: room tone, keys, voice. The terminal is the point. |

The current v1 score is the reference for character; the v2 adds cues, not a new genre.
The v1 true peak (-0.4 dBTP) is a known defect; the v2 must fix it.

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback |
| --- | --- | --- | --- |
| PCB texture | generated by `genPcb` | `out/oyc-v2-assets/pcb.png` | regenerate |
| Wood / rack / sky | generated | `out/oyc-v2-assets/` | regenerate |
| Score | generated by `genScore` | `out/oyc-v2-assets/score.wav` | regenerate; fail clearly |
| Voice | committed, one per act | `assets/vo/own-compute-v2/` | registration returns early if missing; `list` still works |
| SRT | generated in `c.assets` | `out/own-compute-v2.srt` | derive from cue map |
| Thumbnail | generated from a still comp or chosen frame | `out/own-compute-v2-thumb.png` | use frame 1:26 |

Clean-machine test: `list` works from any directory; assets generate on first still or
render; missing voice returns early with a clear message; no external images or fonts
beyond the bundled Inter and a mono fallback.

## 9. Cue map

The full phrase-level cue map is in `docs/own-compute-v2-script.md` §Cue map. Target keys:

| Frame (target) | Picture | Sound |
| --- | --- | --- |
| 60 | Bill reaches $92.97 | Meter tick, cold pad |
| 360 | First glitch stamp | Glitch stab |
| 900 | Cloud pours into chip | Riser/impact |
| 1170 | GPU fans spin up | Fan spin-up |
| 1350 | Download reaches 27.0 GB | Download complete |
| 1590 | Chart starts drawing | Cost tick pulse |
| 2040 | Break-even marker | Riser |
| 2100 | Map route draws | Whoosh |
| 2460 | Terminal answers offline | Key clicks |
| 2610 | Title / dome | D-major lift |

These are targets; the implementation derives final frames from `node->start`, the
measured voice lengths, and one cue table.

## 10. Still frames to review

| Frame | What must be true |
| --- | --- |
| 30 | raymotion badge intro read at feed size |
| 60 | Bill legible; token counts readable; no caption overlap |
| 360 | Glitch stamps legible; “illustrative risks” label visible |
| 900 | Chip and particle morph read; no white-out clipping |
| 1170 | GPU reads; 32 GB / 300 W spec readable; DOF not hiding the card |
| 1350 | 27.0 GB download card legible; seven-layer network visible; wifi-off icon present |
| 1700 | Chart axes, units, cloud/local labels legible |
| 2040 | Break-even marker and numbers legible; “modeled” note visible |
| 2200 | Map route legible; “illustrative route” note visible |
| 2500 | Terminal text readable; network-off icon and $0.00 clear |
| 2700 | Title and dome read at 1:1 and at 320×180 |
| 2740 | Social row and @pebaryan / repo legible |

Plus one still from the middle of every transition, and encoded frames from the final MP4.

## 11. Acceptance checks

- [ ] The raymotion badge intro opens the film; the social card closes it.
- [ ] Muted, the argument is legible: bill → risk → machine → model → trade → map → offline → home.
- [ ] The chart arithmetic matches the source ledger; the break-even is correct.
- [ ] The cloud bill is labelled “modeled,” not “a real invoice.”
- [ ] The hardware/power/hours are labelled “illustrative assumptions.”
- [ ] The map is labelled “illustrative route.”
- [ ] The voice and score share the cue map; scene lengths are derived from the voice.
- [ ] Captions are inside the safe area; no chart/map/caption overlap.
- [ ] `list` works from another directory; missing voice fails clearly.
- [ ] Final: 2,820 frames ± 2, 94 s ± 2, -16 LUFS ± 1, true peak ≤ -1 dBTP.
- [ ] Full decode OK; encoded frames checked; still N == render N.
- [ ] SRT and thumbnail exist.
- [ ] Authoredness ≥ 30 and Craft ≥ 16 (Improvised) with no critical fail.

## 12. Risks / non-goals

**Risks**

- The cost model is a scenario. A real local machine may be slower, hotter, or more
  expensive; the film must not imply a universal break-even.
- A 300 W system draw, 32 GB VRAM and $2,000 hardware price are assumptions; label them.
- The map route is illustrative; do not imply a specific provider’s network.
- The local model’s quality is not benchmarked against the cloud; the film is about
  ownership, not “local is better.”
- The voice clone may mispronounce “Anthropic,” “Qwen 3.8,” or “kWh”; spell the spoken forms
  in the script.

**Non-goals**

- A benchmark of model quality, latency, or energy/water footprint.
- A buying guide or a provider comparison.
- A claim that the cloud is unsafe. The claim is ownership and control.

## 13. Implementation notes (for the build)

- New file `examples/showcase/own_compute_v2.cpp`; register `own-your-compute-v2` in
  `examples/showcase/scenes.cpp` and the `raymotion` target. Keep v1 untouched.
- `Comp` 1920×1080, 30 fps; the voiceless preview uses contiguous act nodes and the
  inherited `fadeIn`/cut grammar. The acts sum to 2,820 frames.
- Intro/outro: use the `raymotion-logo` prefab (badge, stacked) from `social-icons` at the start, and the `social-row` from `raymotion-promo` (`github`, `x`, `youtube`, `@pebaryan`) at the end.
- Voice: when the nine WAVs exist, reuse the `Vo` / `Chapter` / `cue(phrase)` pattern
  from `examples/showcase/austria_time_travel.cpp`; read the WAV seconds, derive the
  scene lengths, and fall back to text length only when a file is absent. The preview
  uses the target act bounds.
- Chart: data arrays next to the drawing code (`plot::` or `dr::` axes); plot the 1–8 h/day
  range; keep the “modeled / illustrative” note on screen.
- Map: the `geo` flat-map projection, Vienna → a US cloud region, a dotted
  cross-continent route, then the local dome over Vienna; label it illustrative.
- Score: extend the v1 `genScore`; the preview generates the score in `c.assets` and
  targets -16 LUFS / ≤ -1 dBTP. SRT and a dedicated thumbnail composition are pending.
- QC: fill `skills/raymotion/templates/QC-REVIEW.md`; target ≥ 30/36.

## 14. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | `<name>` | `<date>` | `revise / approved` |
| Script / cue map | | | |
| Stills | | | |
| Draft | | | |
| Final QC | | | |
