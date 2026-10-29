# a1-sein-six-seats - authoring brief

> Status: `draft`. This is a filled raymotion brief for the supplied A1-GRA-01
> concept. Six scene WAVs now exist at `assets/vo/a1-sein/`; the measured total is
> **38.473 s**. The built cut is **38.5 s**; the 45 s timing tables below are the
> pre-voice target and remain superseded by the measured cut. Composition:
> `examples/showcase/a1_sein_six_seats.cpp`.

| | |
| --- | --- |
| Composition name | `a1-sein-six-seats` |
| Story mode | `explainer` |
| Owner / agent | `opencode` |
| Status | `draft` |
| Runtime target | `45 s target; 38.473 s measured with the generated voice` |
| Platform / aspect | `1080 x 1920, 30 fps, 9:16, Reels / Shorts / TikTok` |
| Voice | `Qwen3-TTS 1.7B Base xvector-only clone; six scene WAVs in assets/vo/a1-sein/; measured 38.473 s; native-speaker review pending` |
| Audience | `A1 beginners who know greetings and a few subject pronouns; English-literate adult learners; after watching they can produce the six pronoun slots and answer "Wir ___ hier."` |
| Source material | [A1-GRA-01 Personalpronomen + sein](../project/German_CEFR_A1-C2_Reel_Briefs/german_reel_roadmap/A1/grammar/01-personalpronomen-sein.md); supplied CEFR roadmap; standard German present tense |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | `explainer` |
| Goal | `make one grammatical pattern legible through a worked table` |
| Proof object | `six-seat table with one pronoun card and one form plate per seat; the fill-in sentence is the practice proof` |
| Required evidence | `source/claim ledger, script, reveal map, still review, language-editor sign-off` |
| Text | `voice plus captions; two caption lines maximum; about 100 wpm of speech, 88 wpm across the cut; German examples slower than English` |
| House grammar | `worked example and conjugation table, not a data chart or a skit` |
| Packaging | `end card, SRT, thumbnail; no logo or social CTA required for this cut` |

**Rule:** do not add a chart or a narrative subplot to satisfy another mode. The table is
the proof.

## 1. One sentence

> This is a film about the German verb **sein** that makes an A1 learner understand
> **that the verb's present tense is learned as six pronoun slots, not one ending rule**,
> by **seating each pronoun at a table and flipping its plate to the form**.

## 2. One-line argument

> `sein` is irregular, so learn the six pronoun slots and their forms; `wir` and
> `sie/Sie` both take `sind`, so the table has six seats but only five unique forms.

**Learning goal.** After 45 seconds the viewer can say the six pronoun slots, produce
`bin`, `bist`, `ist`, `sind`, `seid` in the table, and answer `Wir ___ hier.` with `sind`.
They should also be able to point out that `sind` appears twice, not invent a seventh form.

## 3. Chapters

| # | Time (s) | Chapter | On-screen text (short) | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `0.0-4.8` | Two shapes, one verb | `Why ich bin — but du bist?` | Two place cards slide into the centre of an overhead table; their plates flip to `bin` and `bist`; an amber link joins the two plates; the hook cards lift away as the table outline and six empty settings enter on "Six seats." | contrast pair: `ich bin` / `du bist`; then six empty seat count | VO; card taps; two plate flips; soft table reveal |
| 2 | `4.8-9.6` | The table has a name | `SEIN / to be` | A `SEIN` plaque lowers into the table head; the six empty settings hold. No forms yet. | six-slot table with a single infinitive plaque | VO; low wooden tap as the plaque lands |
| 3 | `9.6-20.0` | Singular side | `ich bin` / `du bist` / `er/sie/es ist` | The left column fills top to bottom: pronoun card, then form plate, one setting at a time. | the same table, now with three completed settings | VO; three card taps and three plate flips |
| 4 | `20.0-31.2` | Plural side | `wir sind` / `ihr seid` / `sie/Sie sind` | The right column fills top to bottom; after the last setting, a mint thread links the two `sind` plates. | the complete table; the shared `sind` is shown by the link, not another label | VO; three taps/flips; one link tick |
| 5 | `31.2-42.0` | Your turn | `Wir ___ hier.` | The table dims; a sentence strip enters with a blank plate; a four-second progress arc runs; `sind` flips into the blank and the table returns to full brightness. | fill-in sentence with a withheld and then revealed answer | VO; timer ticks; answer stamp |
| 6 | `42.0-45.0` | Recap | `Six seats. One verb.` | The full table is held; the end card enters below it. | complete table as the answer key | VO; final marimba chord; end-card tick |

Rules: one idea per chapter. The table is the same object, but its state changes from
empty to singular to plural to practice to answer key; it is not a new card template in
each scene.

### 3b. Reveal map (one row per visual unit)

These are the pre-voice target times at 30 fps. The measured segment map is in
`out/a1-sein-vo-timings.json`; do not use this table as measured time.

| Scene | Unit | Spoken anchor | Visual start | Lands at word | Lead | Hold |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `ich` card | "ich" | 0.34 | 0.62 | 0.28 | 0.10 |
| 1 | `bin` plate | "bin" | 0.72 | 1.00 | 0.28 | 0.80 |
| 1 | `du` card | "du" | 1.80 | 2.08 | 0.28 | 0.10 |
| 1 | `bist` plate | "bist" | 2.14 | 2.42 | 0.28 | 0.40 |
| 1 | amber link | "Same" | 2.84 | 3.12 | 0.28 | 0.70 |
| 1 | table + six empty settings | "Six" | 3.82 | 4.10 | 0.28 | 0.80 |
| 2 | `SEIN` plaque | "Sein" | 4.92 | 5.20 | 0.28 | 1.40 |
| 3 | `ich` card / `bin` plate | "ich" / "bin" | 9.62 | 10.18 | 0.28 | 1.30 |
| 3 | `du` card / `bist` plate | "du" / "bist" | 11.52 | 12.08 | 0.28 | 1.60 |
| 3 | `er/sie/es` card / `ist` plate | "Er" / "ist" | 13.72 | 14.72 | 0.28 | 4.60 |
| 4 | `wir` card / `sind` plate | "Wir" / "sind" | 20.12 | 20.70 | 0.28 | 1.50 |
| 4 | `ihr` card / `seid` plate | "Ihr" / "seid" | 22.22 | 22.80 | 0.28 | 1.80 |
| 4 | `sie/Sie` card / `sind` plate | "Sie" / "sind" | 24.62 | 25.20 | 0.28 | 2.30 |
| 4 | mint shared-form link | "Sind" | 27.52 | 27.80 | 0.28 | 3.40 |
| 5 | `Your turn` label | "Your" | 31.22 | 31.50 | 0.28 | 0.60 |
| 5 | `Wir ___ hier.` strip | "Wir" | 32.12 | 32.40 | 0.28 | 4.80 |
| 5 | answer plate `sind` | "Sind" | 39.62 | 39.90 | 0.28 | 2.10 |
| 6 | end card | "Six" | 42.02 | 42.30 | 0.28 | 2.70 |

Notes:

- The pronoun card and its plate are one **place-setting unit**. The card begins the
  unit; the plate flips on the form word. Do not animate a separate unrelated object
  between them.
- The six empty settings enter as one group; they are a count, not six separate reveals.
- The mint link is a consequence of the two `sind` plates, so it arrives after them.
- Hold is the minimum before the next unit; a scene may hold longer. The answer plate
  arrives after the blank has been visible for the full arc.

## 4. Script

| # | Spoken text | Caption text | Voice seconds | Notes |
| --- | --- | --- | --- | --- |
| 1 | Why ich bin, but du bist? Same verb. Six seats. | Why `ich bin` — but `du bist`? Same verb. Six seats. | `4.4 est.` | Raise the contrast on `bin` / `bist`; slow `Six seats.` |
| 2 | Sein is irregular. Learn it seat by seat. | `Sein` is irregular. Learn it seat by seat. | `4.2 est.` | `Sein` rhymes with "mine"; do not say "seen". |
| 3 | Ich bin: I am. Du bist: you are. Er, sie, es ist: he, she, it is. | `ich bin` = I am · `du bist` = you are · `er/sie/es ist` = he, she, it is | `9.4 est.` | Pause after each form; keep the English gloss light. |
| 4 | Wir sind: we are. Ihr seid: you all are. Sie sind: they are, or you are, formal. Sind appears twice. | `wir sind` = we are · `ihr seid` = you all are · `sie/Sie sind` = they / you (formal) · `Sind` appears twice. | `10.2 est.` | Distinguish `seid` (are) from `seit` (since); hold `Sie` for formal. |
| 5 | Your turn. Wir ___ hier. [4.0 s practice arc] Sind. | Your turn: `Wir ___ hier.` / `sind` | `8.8 est.` | Do not fill the blank while asking; answer only after the arc completes. |
| 6 | Six seats. One verb. Learn the table. | Six seats. One verb. Learn the table. | `2.7 est.` | Land the final line on the complete table. |

Total spoken words: 66. Target speech rate: about 100 wpm of speech, 88 wpm across the
cut including the practice arc. The exact WAV lengths are the timing source; these
estimates are not.

## 5. Sources and claims

| Claim / number | Value used | Source (URL, file, table) | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| Six pronoun slots and their present-tense forms | `ich bin; du bist; er/sie/es ist; wir sind; ihr seid; sie/Sie sind` | [A1-GRA-01 brief](../project/German_CEFR_A1-C2_Reel_Briefs/german_reel_roadmap/A1/grammar/01-personalpronomen-sein.md) | table; end card | `exact` for the standard paradigm; language-editor sign-off pending |
| `wir` and `sie/Sie` share `sind` | `sind` appears on two plates | same brief; standard German present tense | mint link; caption | `exact` |
| Practice sentence and answer | `Wir ___ hier.` -> `sind` | same brief | scene 5 | `exact` |
| `A1` level tag | `A1` | supplied roadmap README and folder tag | kicker; end card | `editorial`; this is not an official CEFR exam spec |
| English glosses | `I am; you are; he, she, it is; we are; you all are; they / you (formal)` | editorial translation for teaching | captions | `illustrative/editorial`; native-speaker review pending |
| "Six seats, five unique forms" | six slots; `bin`, `bist`, `ist`, `sind`, `seid` | same brief | not printed as a statistic; visible on the table | `exact` |

No statistical, historical, or scientific claims are made. The supplied roadmap is a
curriculum source, not a grammar authority. Before publication, a German teacher or
native editor must confirm every form, gloss, and the `sie/Sie` register note.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | `#101A17` ink/background; `#1C4036` table felt; `#F3EBDD` cards; `#FFB45C` `SEIN` plaque and attention; `#7FE0C3` form plates/answer; `#8FA6A0` muted text; `#FFFFFF` active text. No red/green correctness coding. |
| Type | Archivo Black 104-124 px for hook/headline; Space Grotesk 72-96 px for pronouns and forms; Space Grotesk 44-52 px for captions; JetBrains Mono 28-32 px for kicker, tags, source note. |
| CJK | n/a - Latin and German only. Check `ä ö ü ß` coverage; no German example uses them in this cut. |
| Layout | Overhead table; two columns of three settings; no camera move. |
| Margins / safe area | 96 px side margins; 180 px top; 300 px bottom. Table safe box `x=96, y=300, w=888, h=1000`. |
| Caption box | `x=96, y=1430, w=888, h=230`; two lines maximum; 16 px bottom inset. |
| HUD / persistent frame | Top-left kicker `A1 · GRAMMAR`; a 1 px progress hairline at the top; no other persistent chrome. |
| Motion grammar | 10-frame cubic ease-out for cards; 8-frame plate flip with a small shadow; 12-frame link draw; no idle motion except the practice arc and progress line. |
| Motif | Place card plus plate. One `SEIN` plaque. The table is the single repeated object. |
| Charts / maps | n/a - a conjugation table, not a chart. No axes or invented units. |
| Text performance | 100 wpm of speech (88 across the cut); two caption lines; German examples slightly slower; no per-word glow or box. Active contrast is bold or mint, never both. |
| Brand kit | No raymotion logo. End card `A1 · Grammar · A1-GRA-01`; optional series handle only if the series requires it. |

**Density rule:** a still at any moment shows only the units already spoken. The table
starts empty; at no point is the full table visible before the plural scene earns it.

**Label ownership:** pronouns live on cards; forms live on plates; `SEIN` / `to be` lives
on the head plaque; `formal` lives as a small tag on the `Sie` card. Do not repeat those
terms in a separate key. Captions repeat spoken language as subtitles, not as a second
diagram.

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| `sein` has six pronoun slots | six-seat table | one place setting per pronoun set; left column singular, right column plural; head plaque = infinitive | empty settings enter as a group; then one setting fills at a time |
| `ich bin`, `du bist`, `er/sie/es ist` | left-column plates | card = pronoun, plate = form | card lands; plate flips on the form word |
| `wir sind`, `ihr seid`, `sie/Sie sind` | right-column plates | card = pronoun, plate = form | card lands; plate flips on the form word |
| `wir` and `sie/Sie` share `sind` | two mint plates | two seats, same form | after both plates exist, a mint thread connects them |
| `sie` plural vs `Sie` formal | split name card | `sie / Sie`; small `formal` tag | the tag is part of the card; no separate entrance |
| practice answer | blank plate | `Wir ___ hier.`; blank = withheld form | blank holds through the arc, then flips to `sind` |

No duplicated diagram/key labels. The two `sind` plates are intentional because two
seats share the form, not a repeated key.

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | One bilingual German-English voice; six WAVs in `assets/vo/a1-sein/`; English explanation, German examples. Generated with Qwen3-TTS 1.7B Base xvector-only clone; German forms use the German language pass; a native-speaker review is still required. |
| Score | Warm marimba, soft plucked bass, brushed shaker; 92 BPM; six-note motif, one note per seat; no melody under speech; final chord at 42.0 s. |
| Ducking | Score duck `-9 dB` under voice, 8-frame attack, 12-frame release. |
| SFX | Card tap on each card; ceramic plate flip on each form; low tap for `SEIN`; timer ticks at 1 Hz during the practice arc; soft stamp on `sind`; end-card tick. |
| Silence | Measured: no voice from 29.0 to 33.466 s; `Sind` at 33.466 s. Target: a 4.0 s practice arc with low bed. A 0.3 s gap before `Sind`. |
| Loudness target | `-16 LUFS` integrated, true peak `<= -1 dBTP`; stereo. |

## 7b. Scenario coherence

This is a language explainer, not a hardware or cost scenario. The dependent quantities
are the visible count and the repeated form.

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Pronoun slots | 6 | six place settings; two columns of three | supplied brief |
| Unique form plates | 5 | `bin`, `bist`, `ist`, `sind`, `seid`; `sind` appears twice | standard German paradigm |
| Shared-form seats | 2 (`wir`, `sie/Sie`) | mint link between the two `sind` plates | supplied brief |
| Practice answer | `sind` | `Wir ___ hier.` | supplied brief |
| Runtime | 45 s / 1,350 frames | six chapters; one 4.0 s practice arc plus a 1.7 s answer hold | editorial target |

**Rule:** if the voice runs long, cut English glosses or extend to 50 s; do not speed up
the German examples or remove the practice arc. If the table gains a seventh seat,
the count, copy, and thumbnail change together.

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| Fonts | committed | `assets/fonts/ArchivoBlack-Regular.ttf`, `assets/fonts/SpaceGrotesk-Variable.ttf` | fall back to `fonts::ui()` with a clear visual check |
| Voice | generated per scene | `assets/vo/a1-sein/s01.wav` ... `s06.wav` | text-only captions; no fake German audio |
| Score / SFX | generated | `c.assets` | fail clearly if the score cannot be generated; do not ship a silent placeholder |
| SRT | generated from cue map | `c.assets` | derive from captions; `list` still works |
| Table / cards | procedural drawing | composition source | no external texture; blank-safe |
| Thumbnail | committed frame | `output/a1-sein-six-seats-thumb.png` | choose frame 210 and re-check at 320x180 |

**Clean-machine test:** `list` works from any directory; missing voice leaves a legible
text-only cut; missing fonts fall back; missing score fails with a clear message rather
than silently rendering.

## 9. Cue map

Frames at 30 fps; this is the pre-voice target map. The measured 38.473 s map is in `out/a1-sein-vo-timings.json`.

| Frame | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| `0` | kicker and progress line only | score bed starts | scene 1 |
| `10` | `ich` card | card tap | "ich" |
| `22` | `bin` plate | plate flip | "bin" |
| `54` | `du` card | card tap | "du" |
| `64` | `bist` plate | plate flip | "bist" |
| `85` | amber link | link tick | "Same" |
| `115` | table + six empty settings | low table reveal | "Six" |
| `148` | `SEIN` plaque | wooden tap | "Sein" |
| `289` | `ich/bin` setting | tap + flip | "ich" / "bin" |
| `346` | `du/bist` setting | tap + flip | "du" / "bist" |
| `412` | `er/sie/es/ist` setting | tap + flip | "Er" / "ist" |
| `604` | `wir/sind` setting | tap + flip | "Wir" / "sind" |
| `667` | `ihr/seid` setting | tap + flip | "Ihr" / "seid" |
| `739` | `sie/Sie/sind` setting | tap + flip | "Sie" / "sind" |
| `826` | mint shared-form link | link tick | "Sind" |
| `937` | `Your turn` label | no SFX | "Your" |
| `964` | sentence strip | soft card tap | "Wir" |
| `1026` | practice arc starts | timer tick | "Wir ___ hier." |
| `1189` | answer `sind` | answer stamp | "Sind" |
| `1261` | end card | final chord / tick | "Six" |
| `1350` | out | tail | scene 6 |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| `120` | 1 | two cards readable; `bin`/`bist` contrast; table begins without crowding the hook |
| `210` | 2 | `SEIN` plaque and six empty settings read at feed size; no forms visible yet |
| `480` | 3 | left column complete; cards and plates align; no duplicate gloss inside the table |
| `780` | 4 | right column complete; two `sind` plates visibly linked; `formal` tag legible |
| `1010` | 5 | blank is unmistakable; answer not visible before the arc ends; sentence inside safe area |
| `1120` | 6 | full table and end card; no overlap; `A1` tag readable |

Plus: transition stills at frames 167/168, 306/307, 513/514, 806/807 and 1,038, and one
encoded frame from the final MP4. Scene approval uses a scene-only clip or 2-3 stills,
one scene at a time; a contact sheet is coverage, not approval.

**Thumbnail candidate:** frame `210` (7.0 s): `SEIN` plaque, six empty settings, and the
caption `Learn it seat by seat.` Read at 320x180; if the caption is too small, crop to the
plaque and settings rather than adding a new title.

## 10b. Scene review ledger

| Scene | Review asset | Reveal / sync notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 1 | `<pending clip>` | `<pending>` | `<pending>` | `<pending>` |
| 2 | `<pending clip>` | `<pending>` | `<pending>` | `<pending>` |
| 3 | `<pending clip>` | `<pending>` | `<pending>` | `<pending>` |
| 4 | `<pending clip>` | `<pending>` | `<pending>` | `<pending>` |
| 5 | `<pending clip>` | `<pending>` | `<pending>` | `<pending>` |
| 6 | `<pending clip>` | `<pending>` | `<pending>` | `<pending>` |

## 11. Acceptance checks

- [ ] Muted, the six-seat table and the `sind` duplicate are legible without the captions.
- [ ] Every chapter has its proof object visible at the stated frame.
- [ ] Every scene starts sparse and reveals one unit per spoken anchor, with a
      0.20-0.40 s lead and a hold.
- [ ] Every scene is approved individually; no scene reaches final density at `t = 0`.
- [ ] No term is duplicated in the table and a separate key; the executable is newer
      than the last source edit and the final render is rebuilt after approval.
- [ ] The German forms and glosses have a native-speaker/language-editor sign-off.
- [ ] Every caption is inside the safe area and does not overlap the table.
- [ ] The score and picture share the cue map; retiming a chapter is safe.
- [ ] `list` works from a different directory and with voice absent.
- [ ] The final duration, frame count, loudness and true peak are recorded.
- [ ] `A1` is labelled as an editorial level tag, not an official exam claim.
- [ ] Captions use one reading rate and the house text style.
- [ ] The end card, SRT and thumbnail are present.
- [ ] Authoredness is at least 22 and Craft at least 13; showcase targets are 30/16.
- [ ] The thumbnail is readable at 320x180.

## 12. Risks / non-goals

- **Risks:** German accuracy and CEFR fit; `seid` vs `seit`; formal `Sie` vs plural `sie`;
  `er/sie/es` and `sie/Sie` grouping could confuse a beginner; 45 s may be too tight if
  the voice is slow; six settings must stay readable at 320x180.
- **Non-goals:** the full pronoun system, regular verb endings, cases, tenses other than
  present, dialect, or a Goethe/ÖSD exam specification.
- **Out of scope:** 3D assets, a dialogue skit, quiz options, social CTA, and a raymotion
  logo. The table and the practice sentence carry the film.
- **Release condition:** no publication until a German teacher/native editor signs off on
  every form, gloss, and register note.

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | `<pending>` | `<date>` | `<pending>` |
| Script / cue map | `<pending>` | `<date>` | `<pending>` |
| Stills | `<pending>` | `<date>` | `<pending>` |
| Staged reveal / sync | `<pending>` | `<date>` | `<pending>` |
| Draft | `<pending>` | `<date>` | `<pending>` |
| Final QC | `<pending>` | `<date>` | `<pending>` |
















