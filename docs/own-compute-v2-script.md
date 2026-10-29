# own-your-compute-v2 — script, cue map and sources

Companion to `docs/own-compute-v2-brief.md`.
Composition: `own-your-compute-v2` · 1920×1080 · 30 fps · target 94 s · voice + score.

Status: **voiceless preview built** (94 s, QC 33/36 authoredness, 17/18 craft). The voice files do not exist yet;
this cut uses the target act bounds and the captions as the script. The final scene
lengths will be derived from the measured WAVs; the times below are targets.

## Voice and caption rules

- First person, past tense, one concrete bill and one machine.
- One idea per sentence. Say the number, then the unit.
- Plain phrase first; no “inference,” “on-prem,” or “TCO” on screen.
- The honesty beat is mandatory: at low use the cloud is cheaper.
- No “cloud bad / local good.” The claim is ownership, not moral superiority.
- Spoken forms: “ninety-two dollars and ninety-seven cents”, “twenty million tokens in,
  five million out”, “twenty-seven-gigabyte”, “three hundred watts”,
  “kilowatt-hours”, “break-even”.
- Captions are shorter than the voice, but never contradict it.
- Karaoke: a constant **2.5 words/s**, independent of the scene budget. Spoken words use
  the caption accent; upcoming words use `withAlpha(INK, 0.35)`. No per-word glow or
  selection box. The line holds after the last word. When the voice is attached, drive
  the spoken count from word timings (or `Vo::cue`).

## Narration and captions

| # | Act | Voice (spoken) | Caption (on screen) | Target |
| --- | --- | --- | --- | --- |
| 1 | The bill | “Last month I spent ninety-two dollars and ninety-seven cents asking a model questions. Twenty million tokens in. Five million out.” | “Last month: $92.97. 20M in. 5.3M out.” | 0:00–0:10 |
| 2 | The strings | “I do not know which building answered. It is a data centre in a region I chose from a menu, under terms I did not write. It can change those terms, lose a region, or go down at two in the morning.” | “I don’t know which building answered.” | 0:10–0:29 |
| 3 | The turn | “So I bought a machine. A graphics card. A small case. A fan that never stops.” | “So I bought a machine.” | 0:29–0:36 |
| 4 | The machine | “It draws about three hundred watts. It is not free. It is not silent. But it is here.” | “300 W. Not free. Not silent. Here.” | 0:36–0:43 |
| 5 | The model | “It downloaded a twenty-seven-gigabyte model once. Now it answers with the network unplugged.” | “27 GB, downloaded once.” | 0:43–0:51 |
| 6 | The trade | “But here is the honest part. At four hours a day, it costs about the same. At one, the cloud wins. At eight, the machine wins. The break-even is about four hours.” | “At 4 h/day, about the same. At 1 h, cloud wins. At 8 h, the machine wins.” | 0:51–1:08 |
| 7 | The map | “The cloud is someone else’s computer. Your prompt leaves your room, crosses a network, and lands in a region you do not control. This one stays under the dome.” | “Your prompt leaves the room. This one doesn’t.” | 1:08–1:22 |
| 8 | Offline | “No signal. No per-token meter. Still thinking.” | “No signal. No per-token meter. Still thinking.” | 1:22–1:27 |
| 9 | Home | “The cloud is someone else’s computer. This one is mine.” | “The cloud is someone else’s computer. This one is mine.” | 1:27–1:34 |

The film opens with a 2.3 s raymotion badge intro (no narration); the first line starts
at 0:02.3. The end card adds the latest promo's social row (`github`, `x`, `youtube`,
`@pebaryan`) and the repo, with no narration.

Total spoken ≈ 188 words. At ~130 wpm plus pauses, ≈ 94 s. The build measures the WAVs and
sets each chapter length from them; do not force the text into the target times.

## Cue map (targets)

Picture and score read the same table. The final code uses `Vo::cue(phrase)` (character
position in the spoken text) and `node->start`, not the absolute frames below.

| Act | Phrase | Act-local target | Picture | Sound |
| --- | --- | --- | --- | --- |
| 1 | “ninety-two dollars” | 0:02 | Bill reaches $92.97 | Meter tick |
| 1 | “twenty million tokens in” | 0:04 | Input counter 20.0M | Tick |
| 1 | “five million out” | 0:06 | Output counter 5.3M | Tick |
| 2 | “which building answered” | 0:02 | First glitch stamp | Glitch stab |
| 2 | “data centre” | 0:05 | Region label | Low boom |
| 2 | “terms I did not write” | 0:09 | TERMS UPDATED stamp | Glitch |
| 2 | “two in the morning” | 0:14 | SERVICE OUTAGE stamp | Alarm |
| 3 | “bought a machine” | 0:01 | Particle cloud starts pouring | Riser |
| 3 | “graphics card” | 0:04 | Chip outline forms | Impact |
| 4 | “three hundred watts” | 0:02 | Spec chips read 32 GB and 300 W | Fan spin-up |
| 4 | “not free” | 0:05 | Cost label appears | Low hum |
| 4 | “here” | 0:09 | GPU settles | Fan hum |
| 5 | “twenty-seven-gigabyte” | 0:02 | Download card reaches 27.0 GB | Download ticks |
| 5 | “network unplugged” | 0:07 | Wi-Fi icon goes dark | Modem click |
| 6 | “four hours a day” | 0:02 | Chart draws to 4 h marker | Cost tick |
| 6 | “one hour a day” | 0:08 | 1 h marker | Tick |
| 6 | “eight” | 0:13 | 8 h marker | Tick |
| 6 | “break-even” | 0:15 | Break-even label | Riser |
| 7 | “someone else’s computer” | 0:01 | Globe/region | Distance drone |
| 7 | “leaves your room” | 0:04 | Home pin, route starts | Whoosh |
| 7 | “region you do not control” | 0:08 | Route lands at region | Whoosh |
| 7 | “under the dome” | 0:12 | Local dome closes | Soft impact |
| 8 | “No signal” | 0:01 | Wi-Fi off icon | Key click |
| 8 | “No per-token meter” | 0:02 | $0.00 per query | Key click |
| 8 | “Still thinking” | 0:04 | Answer streams | Key clicks |
| 9 | “someone else’s computer” | 0:01 | Title starts | D-major lift |
| 9 | “mine” | 0:05 | Dome/title | Final bell |

## Cost model

This is the arithmetic behind the chart. It is a **model**, not a real invoice.

### Cloud bill

- 20.0M input tokens × $2 / MTok = **$40.00**
- 5.297M output tokens × $10 / MTok = **$52.97**
- **Total: $92.97** (the film shows “20M in / 5.3M out” and “$92.97”)
- Source: Anthropic pricing, Sonnet 5.5 list prices, accessed 2026-10-26.
- The bill is modeled on list prices; it is not a real invoice.

### Local machine

- Hardware: **$2,000** one-time, amortized over **24 months** = **$83.33 / month**
  (scenario assumption; not a quote).
- Power: **300 W** system draw under load (scenario assumption; a 32 GB GPU class,
  user-set spec, not a specific product).
- Electricity: **18.16 ¢/kWh** (EIA US residential average, August 2026, used as a
  scenario proxy; the home is Vienna, so this is not an Austrian tariff).
- At **h** hours/day, 30 days: energy = 0.3 kW × h × 30 = **9h kWh**;
  cost = **$1.6344 h**.
- Monthly local cost = **$83.33 + $1.6344 h**.
- At 4 h/day: $83.33 + $6.54 = **$89.87 / month**.
- At 1 h/day: **$84.96**; at 8 h/day: **$96.41**.

### Cloud line on the chart

The cloud bill is per token; to compare at the same workload, the chart scales the
modeled bill linearly with hours of equivalent use:

- Cloud monthly cost = **$23.2425 h** (because $92.97 / 4 h/day baseline).
- At 1 h/day: $23.24; at 2 h: $46.49; at 4 h: $92.97; at 6 h: $139.46; at 8 h: $185.94.

### Break-even

- $23.2425 h = $83.33 + $1.6344 h
- $21.6081 h = $83.33
- **h ≈ 3.86 h/day**

The chart shows the crossing at ~3.9 h/day. The voice says “about four hours.” The film
must display “modeled; assumptions in notes” and “hardware/power/hours illustrative.”

### Chart data (for the plot)

| Hours/day | Cloud ($/month) | Local ($/month) |
| --- | --- | --- |
| 0 | 0.00 | 83.33 |
| 1 | 23.24 | 84.96 |
| 2 | 46.49 | 86.60 |
| 4 | 92.97 | 89.87 |
| 6 | 139.46 | 93.14 |
| 8 | 185.94 | 96.41 |

Plot the **1–8 h/day** range; 0 is in the table for the arithmetic only, not as a
recommended operating point.

## Sources and assumptions

| Claim / number | Value | Source | On-screen | Exact or illustrative |
| --- | --- | --- | --- | --- |
| Frontier API price | $2/MTok input, $10/MTok output | Anthropic pricing, Sonnet 5.5: <https://www.anthropic.com/pricing> (accessed 2026-10-26) | No; used in the model | Exact list price at access |
| Modeled cloud bill | 20.0M in + 5.297M out = $92.97 | Arithmetic on the list price above | Yes (“$92.97”) | **Modeled, not a real invoice** |
| US residential electricity | 18.16 ¢/kWh, August 2026 | EIA Electric Power Monthly, Table 5.6.A: <https://www.eia.gov/electricity/monthly/epm_table_grapher.php?t=epmt_5_6_a> | Chart note | Exact published average; used as a scenario proxy (home is Austria, not a US tariff) |
| Model size | 27 GB (Qwen3.8 27B, Q8-class) | User-specified model; 27 GB is the requested illustrative file size, not a linked public file | Yes (“27 GB”) | **Illustrative size** |
| GPU class / power | 32 GB VRAM, 300 W under load | Scenario hardware spec, user-set; not a specific product | “32 GB” / “300 W” | **Illustrative scenario** |
| Hardware price | $2,000 one-time | Scenario assumption | Chart note | **Illustrative** |
| Usage | 4 h/day baseline | Scenario assumption | Chart note | **Illustrative** |
| Amortization | 24 months | Scenario assumption | Chart note | **Illustrative** |
| Data-centre regions | 39 regions, 124 AZs | AWS Global Infrastructure: <https://aws.amazon.com/about-aws/global-infrastructure/regions_az/> | Map note | Exact at access |
| Map route | Vienna → a US cloud region | Illustrative cross-continent route; AWS regions exist | “illustrative route” | **Illustrative** |
| Service risks | terms/region/outage | Illustrative risks; provider terms vary | Stamps | **Illustrative** |
| Offline privacy | the prompt stays on the machine | Demonstrated in the offline act | Yes | True for the demo |

**Do not remove these labels.** The film’s credibility depends on admitting that the
bill is modeled, the hardware/power/hours are assumptions, and the map is illustrative.

## Audio plan

### Voice

- 9 files, `assets/vo/own-compute-v2/s01-bill.wav` … `s09-home.wav`.
- 48 kHz mono, trimmed, normalized; volume 1.0; captions + SRT.
- The build measures each WAV and sets the chapter length from it.
- If the files are missing, registration returns early; `list` still works.

### Score

Extend the v1 score (same character, new cues):

- cold pads + meter ticks under the bill;
- glitch stabs on the stamps;
- riser into the chip, impact on the turn;
- fan spin-up + low hum under the machine;
- download ticks and a modem click;
- cost-tick pulse and a small riser under the chart;
- distance drone and a whoosh under the map;
- key clicks and room tone under the terminal;
- the 120 bpm groove into the D-major lift at home.

Target: **-16 LUFS integrated, true peak ≤ -1 dBTP**. The v1 peak (-0.4 dBTP) is a known
defect; fix it with gain staging/limiting, not by lowering the whole film into mud.

### SFX

Bill ticks, glitch, fan, download complete, chart ticks, map whoosh, terminal keys, dome
hum, final impact. Place on the cue map; no random SFX.

### Ducking

Bed to 0.55 under each voice span, 10-frame attack/release. Leave the terminal act almost
dry: room tone + keys + voice.

## Verification plan

1. **Stills:** the ten frames in the brief, plus one still in the middle of every
   transition. Check text, chart units, map labels, source labels, no overlap.
2. **Draft muted:** does the bill → risk → machine → model → trade → map → offline → home
   story read without sound? Does the chart still make sense?
3. **Draft with sound:** voice sync, ducking, SFX cues, no clipped words, no music fight.
4. **Final:** `ffprobe` frame count/duration; full decode; `ebur128` loudness/peak;
   encoded-frame extraction; still N == render N; SRT and thumbnail.
5. **Clean machine:** rename `assets/vo/own-compute-v2/` and confirm `list` still works
   and `render` fails with a clear message.
6. **QC:** fill `skills/raymotion/templates/QC-REVIEW.md`; target **≥ 30/36** with no
   critical fail and Gates 0–5 passed.

## Open questions before code

1. **Voice:** author voice clone or TTS? If clone, which reference and tool? (The repo
   uses `tools/vo_batch.py` for the author’s Qwen3-TTS clone.)
2. **Bill:** is $92.97 a real bill, or a modeled scenario? If real, replace the model and
   update the ledger; if modeled, keep the label.
3. **Hardware:** do we want a real machine spec (e.g., a specific GPU) and a real price,
   or keep the $2,000 / 32 GB / 300 W scenario labels?
4. **Map:** the route is Vienna → a US cloud region, illustrative. Is that the right
   cross-continent pairing, or should the US region be named?
5. **Thumbnail:** island/dome at 1:26, or the bill at 0:02? Pick one before the v2 build.
6. **Runtime:** 94 s is the target; if the measured voice runs long, cut words rather than
   speed the read.
