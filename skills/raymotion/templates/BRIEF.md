# <slug> — authoring brief

> Copy this file to `docs/<slug>-brief.md` and fill it in **before** writing composition
> code. Replace every `<...>`. If a section does not apply, write `n/a` and why.
> Status: `draft` / `approved` / `superseded`.

| | |
| --- | --- |
| Composition name | `<slug>` |
| Story mode | `explainer / narrative / promo / utility` |
| Owner / agent | `<name>` |
| Status | `draft` |
| Runtime target | `<seconds>` |
| Platform / aspect | `<1920x1080, 30 fps, feed / presentation / overlay>` |
| Voice | `<cloned voice / TTS / none, and why>` |
| Audience | `<who, what they already know, what they should be able to say after>` |
| Source material | `<paper / archive / dataset / client brief, with paths>` |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `<explainer / narrative / promo / utility>` |
| Goal | `<make a concept legible / make a story land / make an idea desirable / be legible in context>` |
| Proof object | `<chart/map/worked example; shot/beat/continuity; demo/spec/CTA; layout>` |
| Required evidence | `<source ledger; script/shot list; claim ledger; spec>` |
| Text | `<captions/voice rate; dialogue/subtitles; tagline/VO/CTA; hierarchy>` |
| House grammar | `<data/lecture; genre; brand; brand/utility>` |
| Packaging | `<title/credits/thumbnail/SRT; logo/social/CTA for a promo>` |

**Rule:** do not apply another mode's proof requirement. A narrative is not failed for
lacking a chart; an explainer is not failed for lacking a dramatic arc.

## 1. One sentence

> This is a film about **<subject>** that makes the viewer **<feel / understand>**
> **<specific idea>** by **<specific device>**.

## 2. One-line argument

> <The single claim the film makes. One sentence. No hedging.>

## 3. Chapters

| # | Time (s) | Chapter | On-screen text (short) | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `0:00–0:22` | `<name>` | `<headline>` | `<what happens, frame by frame>` | `<chart / map / artifact / mechanism / count>` | `<music / VO / SFX>` |
| 2 | | | | | | |
| 3 | | | | | | |

Rules:

- One idea per chapter. If two ideas share a row, split it.
- The proof object column may not be "caption" or "text".
- Every chapter has a distinct visual idea; the motif/HUD is what unifies them.

### 3b. Reveal map (one row per visual unit)

| Scene | Unit | Spoken anchor | Visual start | Lands at word | Hold |
| --- | --- | --- | --- | --- | --- |
| `<n>` | `headline / proof object / label / source` | `<word or phrase>` | `<s>` | `<s>` | `<s>` |
| | | | | | |

Rules: scene starts sparse; one entrance at a time; lead the spoken word by
**0.20-0.40 s**; hold before the next unit; no scene reaches final density at `t = 0`.
Review one scene per pass before moving to the next.

## 4. Script

| # | Spoken text | Caption text | Voice seconds | Notes |
| --- | --- | --- | --- | --- |
| 1 | `<what the voice says>` | `<what the caption shows; may match>` | `<measured>` | `<pronunciation, emphasis>` |
| 2 | | | | |

If there is no voice, write `Voice: none`, give the reason, and make the caption text a
complete script. Add:

> `Cue map`: `<phrase or fraction> -> <frame>` for every picture and score cue.

Rules:

- One idea per sentence; aim for ≤ 20 words.
- Plain phrase first, term second. Define a term once.
- Numbers with units. Round for speech, exact for labels.
- Mark illustrative values in the caption or on screen.
- Banned unless quoted and sourced: "solves", "eliminates", "fair AI", "breakthrough".

## 5. Sources and claims

| Claim / number | Value used | Source (URL, file, table) | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| `<e.g. Japanese tokens per word>` | `<56.16x>` | `<paper table 3>` | `<label>` | `exact` |
| | | | | |

Rules:

- Every claim is either sourced or labelled `illustrative`.
- State caveats and sample sizes. Do not imply precision or causality the source does
  not support.
- Keep the source ledger next to the data arrays in code when possible.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | `<colour> = <meaning>; <colour> = <meaning>; never use <colour> for <x>` |
| Type | `<face and sizes for kicker / headline / body / caption / footnote>` |
| CJK | `<font candidates, sample string, loadFor plan>` |
| Margins / safe area | `<px>` |
| Caption box | `<x, y, w, h; max lines>` |
| HUD / persistent frame | `<what stays across chapters>` |
| Motion grammar | `<entrance frames, easings, transitions, idle-motion policy>` |
| Motif | `<the single repeated object>` |
| Charts / maps | `<chart types, units, axis policy, label sizes>` |
| Text performance | `<words per second; karaoke/word highlight style; house caption style; no per-word glow/box>` |
| Brand kit | `<logo prefab and placement; social row/handles; intro/outro>` |

**Density rule:** one idea per screen. If a still needs a caption to explain the chart,
the chart is wrong.

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| `<claim>` | `<venn / folds / matrix / histogram / range / lock>` | `<left/right/overlap; rows/columns; before/after>` | `<duplicate / move / morph / fill / arrive after parent>` |
| | | | |

Rules: the metaphor must be readable without its caption; one term has one home (no
duplicate diagram/key labels); a dependent object arrives after its parent.

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | `<files, chapter mapping, volume>` |
| Score | `<instruments / motifs / references; generated by what>` |
| Ducking | `<volumeAt or score duck; attack/release frames>` |
| SFX | `<event -> frame -> file -> volume>` |
| Silence | `<where and why>` |
| Loudness target | `<e.g. -16 LUFS integrated, true peak ≤ -1 dBTP>` |

## 7b. Scenario coherence

If the film makes a technical or numeric claim, the numbers must fit together. Fill this
and recompute every dependent value when one changes.

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Model / subject size | `<27 GB>` | `<VRAM / budget>` | `<source or illustrative>` |
| Hardware / capacity | `<32 GB VRAM, 300 W>` | `<model size / power>` | `<source or illustrative>` |
| Usage | `<4 h/day>` | `<cost chart>` | `<source or illustrative>` |
| Cost / price | `<...>` | `<usage / chart>` | `<source or illustrative>` |
| Names / labels | `<model, hardware, route>` | `<source ledger>` | `<exact or illustrative>` |

**Rule:** one change recomputes its consequences. A model-size change updates the download
label, the VRAM fit, the cost chart and the copy; a route change updates the map and its
labels. Do not leave a stale number anywhere.

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Score | `<c.assets / committed>` | `<path>` | `<synthesize / fail clearly>` |
| Voice | `<committed>` | `<path>` | `<return empty, list still works>` |
| SRT | `<generated in c.assets>` | `<path>` | `<derive from cue map>` |
| Maps / textures | `<lazy>` | `<code>` | `<blank / procedural>` |

**Clean-machine test:** `<what happens when assets are absent; what must still work>`

## 9. Cue map

| Frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| `<frame>` | `<event>` | `<event>` | `<script phrase / node->start / measured WAV>` |
| | | | |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| `<n>` | `<chapter>` | `<text in safe area, no overlap, glyphs real, data correct>` |
| | | |

Plus: one still from the middle of every transition, and one encoded frame extracted from
the final MP4. Scene approval uses a scene-only clip or 2-3 stills, one scene at a time;
a contact sheet is coverage, not approval.

## 10b. Scene review ledger

| Scene | Review asset | Reveal / sync notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| `<n>` | `<clip or stills>` | `<order, lead, hold, duplicate labels>` | `yes / revise` | `<path>` |
| | | | | |

## 11. Acceptance checks

- [ ] Muted, the argument is still legible from the images.
- [ ] Every chapter has its proof object visible at the stated frame.
- [ ] Every scene starts sparse and reveals one unit per spoken anchor, with a
      0.20-0.40 s lead and a hold.
- [ ] Every scene was approved individually; no scene reaches final density at `t = 0`.
- [ ] No term is duplicated in a diagram and its key; the executable is newer than the
      last source edit and the final render was rebuilt after approval.
- [ ] Every number matches the source ledger; illustrative values are labelled.
- [ ] Every caption is inside the safe area and does not overlap the HUD/chart.
- [ ] The score and picture share the cue map; retiming a chapter is safe.
- [ ] `list` works from a different directory and with assets absent.
- [ ] The rendered duration, frame count, loudness and true peak are recorded.
- [ ] Scenario numbers agree; changing one recomputes its consequences.
- [ ] Captions use one constant reading rate and the house text style.
- [ ] The mode-appropriate packaging is placed (title/credits, thumbnail, SRT; logo/social/CTA for a promo).
- [ ] Authoredness ≥ 22 and Craft ≥ 13; showcase ≥ 30 and ≥ 16.
- [ ] A thumbnail frame is chosen and readable at 320×180.

## 12. Risks / non-goals

- **Risks:** `<asset fragility, font coverage, historical ambiguity, runtime>`
- **Non-goals:** `<what this film does not try to prove or show>`
- **Out of scope:** `<features intentionally not used, and why>`

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | `<name>` | `<date>` | `approved / revise` |
| Script / cue map | | | |
| Stills | | | |
| Staged reveal / sync | | | |
| Draft | | | |
| Final QC | | | |
