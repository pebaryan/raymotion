# Six Seats for Sein: narration script

Companion to [a1-sein-six-seats-brief.md](a1-sein-six-seats-brief.md). This is the
spoken and on-screen script for the A1-GRA-01 reel. Six scene WAVs now exist at
`assets/vo/a1-sein/`; the measured total is **38.473 s**. The 45 s timing table below is
the pre-voice target. Re-derive the reveal map and cue map from the WAVs before writing
composition code.

**Composition:** `a1-sein-six-seats` - 1080 x 1920 - 30 fps - 45 s target / **38.473 s
measured** (1,155 frames at 30 fps) - 9:16
**Voice:** generated Qwen3-TTS 1.7B Base xvector-only clone, six scene WAVs; German
language pass for the forms; 38.473 s total, about 103 wpm. Text-only captions are the
fallback.
**Status:** `draft`; VO generated and the built cut is 38.5 s. The timing table below is
the pre-voice target; native language sign-off is still pending.
**Source:** [A1-GRA-01 Personalpronomen + sein](../project/German_CEFR_A1-C2_Reel_Briefs/german_reel_roadmap/A1/grammar/01-personalpronomen-sein.md).

## Target timing (pre-voice)

| # | Time | Frames | Chapter | VO words | Target seconds |
| --- | --- | --- | --- | ---: | ---: |
| 1 | 0:00.0-0:04.8 | 0-144 | Two shapes, one verb | 10 | 4.8 |
| 2 | 0:04.8-0:09.6 | 144-288 | The table has a name | 8 | 4.8 |
| 3 | 0:09.6-0:20.0 | 288-600 | Singular side | 16 | 10.4 |
| 4 | 0:20.0-0:31.2 | 600-936 | Plural side | 20 | 11.2 |
| 5 | 0:31.2-0:42.0 | 936-1,260 | Your turn | 5 | 10.8 |
| 6 | 0:42.0-0:45.0 | 1,260-1,350 | Recap | 7 | 3.0 |

Total target: 66 words in 45 s. The practice scene contains a deliberate 4.0 s arc plus a
1.7 s answer hold, so the words are not spread evenly.

## Measured VO (generated)

| # | File | Start | Duration | Frames |
| --- | --- | ---: | ---: | ---: |
| 1 | `s01.wav` | 0.000 | 5.579 | 0-167 |
| 2 | `s02.wav` | 5.579 | 4.630 | 167-306 |
| 3 | `s03.wav` | 10.209 | 6.907 | 306-513 |
| 4 | `s04.wav` | 17.116 | 9.749 | 513-806 |
| 5 | `s05.wav` | 26.865 | 7.721 | 806-1,038 |
| 6 | `s06.wav` | 34.586 | 3.887 | 1,038-1,154 |
| | **Total** | **0.000** | **38.473** | **1,155** |

Measured narration: 66 words in 38.473 s, about 103 wpm. The files are 48 kHz mono and
are already generated; the target table above is retained for the visual plan and must
not be treated as measured time.

## Chapter 1 - Two shapes, one verb (0:00.0-0:04.8)

**VO**

> Why ich bin, but du bist? Same verb. Six seats.

**On screen**

| In | Out | Caption |
| ---: | ---: | --- |
| 0.0 | 3.1 | Why `ich bin` - but `du bist`? |
| 3.1 | 4.8 | Same verb. Six seats. |

**Visual.** Start on dark table felt with only the kicker and progress line. At 0.34 s,
an `ich` place card lands; at 0.72 s its plate flips to `bin`. At 1.80 s a `du` card
lands; at 2.14 s its plate flips to `bist`. At 2.84 s an amber link draws between the two
plates. At 3.82 s the table edge and six empty place settings enter as one group; the two
hook cards lift away and are gone by 4.4 s. The settings land on "Six" (4.10 s). Do not
show the other forms yet.

**Sync notes.** `ich` 0.62, `bin` 1.00, `du` 2.08, `bist` 2.42, `Same` 3.12, `Six` 4.10.
Every visual starts 0.28 s before its word. The table group is one unit, not six.

**Why these words.** The hook names the contradiction without using the word
"conjugation." "Six seats" gives the viewer the shape of the answer before the table fills.

## Chapter 2 - The table has a name (0:04.8-0:09.6)

**VO**

> Sein is irregular. Learn it seat by seat.

**On screen**

| In | Out | Caption |
| ---: | ---: | --- |
| 4.8 | 6.2 | `Sein` is irregular. |
| 6.2 | 7.8 | Learn it seat by seat. |

**Visual.** The six empty settings hold. At 4.92 s a cream `SEIN` plaque with the small
gloss `to be` lowers into the table head and settles at 5.20 s. No pronoun cards and no
form plates are present. The plaque is the only change.

**Sync notes.** `Sein` 5.20, `irregular` 5.70, `Learn` 6.40. Lead the plaque by 0.28 s;
hold the empty table through the rest of the line.

**Why these words.** "Irregular" is the honest reason the endings are not predictable.
"Seat by seat" tells the viewer how to watch the next two scenes.

## Chapter 3 - Singular side (0:09.6-0:20.0)

**VO**

> Ich bin: I am. Du bist: you are. Er, sie, es ist: he, she, it is.

**On screen**

| In | Out | Caption |
| ---: | ---: | --- |
| 9.9 | 12.1 | `ich bin` = I am |
| 12.1 | 14.2 | `du bist` = you are |
| 14.2 | 20.0 | `er/sie/es ist` = he, she, it is |

**Visual.** Fill the left column, top to bottom. At 9.62 s the `ich` card lands and at
10.18 s its plate flips to `bin`. At 11.52 s the `du` card lands and at 12.08 s its plate
flips to `bist`. At 13.72 s the three-name card `er / sie / es` lands and at 14.72 s its
plate flips to `ist`. The English gloss stays in the caption, not inside the table.

**Sync notes.** `ich` 9.90, `bin` 10.18, `du` 11.80, `bist` 12.08, `Er` 14.00, `ist`
14.72. Each card leads its form by one place-setting unit. Hold `ist` until the plural
scene begins.

**Why these words.** The short English gloss after each colon gives an A1 viewer a
meaning anchor without turning the table into a bilingual dictionary.

## Chapter 4 - Plural side (0:20.0-0:31.2)

**VO**

> Wir sind: we are. Ihr seid: you all are. Sie sind: they are, or you are, formal.
> Sind appears twice.

**On screen**

| In | Out | Caption |
| ---: | ---: | --- |
| 20.4 | 22.3 | `wir sind` = we are |
| 22.3 | 24.4 | `ihr seid` = you all are |
| 24.4 | 27.6 | `sie / Sie sind` = they / you (formal) |
| 27.6 | 31.2 | `Sind` appears twice. |

**Visual.** Fill the right column. At 20.12 s the `wir` card lands; at 20.70 s its plate
flips to `sind`. At 22.22 s the `ihr` card lands; at 22.80 s its plate flips to `seid`.
At 24.62 s the `sie / Sie` card lands with a small `formal` tag under `Sie`; at 25.20 s
its plate flips to `sind`. At 27.52 s a mint thread draws between the two `sind` plates
and settles on "Sind" (27.80 s). The thread is the shared-form proof; do not add a
`same form` label.

**Sync notes.** `Wir` 20.40, `sind` 20.70, `Ihr` 22.50, `seid` 22.80, `Sie` 24.90,
`sind` 25.20, `Sind` 27.80. The `formal` tag is part of the card, not a separate unit.

**Why these words.** "You all" distinguishes `ihr` from singular `du`. "Formal" is the
one register note an A1 viewer needs for `Sie`. "Sind appears twice" names the table's
one repeated form without implying a second conjugation rule.

## Chapter 5 - Your turn (0:31.2-0:42.0)

**VO**

> Your turn. Wir ___ hier. [4.0 s practice arc] Sind.

**On screen**

| In | Out | Caption |
| ---: | ---: | --- |
| 31.5 | 38.9 | Your turn: `Wir ___ hier.` |
| 39.9 | 42.0 | `sind` |

**Visual.** The table dims to about 35 percent. At 31.22 s a `Your turn` label enters; at
32.12 s a cream sentence strip lands with `Wir ___ hier.` and a raised blank plate. At
34.20 s a four-second mint arc begins around the blank. It completes at 38.20 s. The table
and strip hold empty until 39.62 s, when the blank plate flips to `sind` and settles at
39.90 s; the table returns to full brightness. No multiple-choice options appear.

**Sync notes.** `Your` 31.50, `Wir` 32.40, `hier` 33.02, `Sind` 39.90. The arc is a
countdown, not decoration; it must not reveal the answer. Leave the blank visible for the
full arc.

**Why these words.** The challenge reuses the exact sentence from the supplied brief:
`Wir ___ hier.` The answer is fixed, so the edit must supply it. The four-second arc plus the answer hold is the practice time.

## Chapter 6 - Recap (0:42.0-0:45.0)

**VO**

> Six seats. One verb. Learn the table.

**On screen**

| In | Out | Caption |
| ---: | ---: | --- |
| 42.0 | 45.0 | Six seats. One verb. Learn the table. |

**Visual.** Hold the complete table. At 42.02 s the end card `A1 · Grammar · A1-GRA-01`
enters below it and settles at 42.30 s. The two `sind` plates remain linked. No new forms,
no logo, no social row.

**Sync notes.** `Six` 42.30, `One` 43.20, `Learn` 43.90. Let the last word land on the
end card and hold to 45.0.

**Why these words.** "Six seats" is the visual; "one verb" is the category; "learn the
table" is the action. The line avoids claiming that the learner has mastered all of
`sein`.

## Cue map

Frames at 30 fps; this is the pre-voice target cue map, not the measured 38.473 s map.

| Frame | Picture event | Sound event | Source of truth |
| ---: | --- | --- | --- |
| 0 | kicker and progress only | score bed starts | scene 1 |
| 10 | `ich` card | card tap | "ich" |
| 22 | `bin` plate | plate flip | "bin" |
| 54 | `du` card | card tap | "du" |
| 64 | `bist` plate | plate flip | "bist" |
| 85 | amber link | link tick | "Same" |
| 115 | six empty settings | low table reveal | "Six" |
| 148 | `SEIN` plaque | wooden tap | "Sein" |
| 289 | `ich/bin` setting | tap + flip | "ich" / "bin" |
| 346 | `du/bist` setting | tap + flip | "du" / "bist" |
| 412 | `er/sie/es/ist` setting | tap + flip | "Er" / "ist" |
| 604 | `wir/sind` setting | tap + flip | "Wir" / "sind" |
| 667 | `ihr/seid` setting | tap + flip | "Ihr" / "seid" |
| 739 | `sie/Sie/sind` setting | tap + flip | "Sie" / "sind" |
| 826 | mint link | link tick | "Sind" |
| 937 | `Your turn` label | no SFX | "Your" |
| 964 | sentence strip | card tap | "Wir" |
| 1026 | practice arc | timer tick | "Wir ___ hier." |
| 1189 | answer `sind` | answer stamp | "Sind" |
| 1261 | end card | final chord / tick | "Six" |
| 1350 | out | tail | scene 6 |

## Language notes

- `sein` present tense: `ich bin`, `du bist`, `er/sie/es ist`, `wir sind`, `ihr seid`,
  `sie/Sie sind`.
- Six pronoun slots; five unique forms: `bin`, `bist`, `ist`, `sind`, `seid`. `sind`
  appears twice because `wir` and `sie/Sie` share it.
- `ihr seid` means "you all are." Do not confuse it with `ihr seit`, "since you."
- `Sie` with a capital S is formal "you"; `sie` is "they." The caption makes the
  distinction; the voice alone cannot.
- `er/sie/es` occupies one seat; `sie/Sie` occupies one seat. The cards show the groups.
- The `A1` tag comes from the supplied roadmap. It is an editorial level tag, not an
  official CEFR or Goethe/ÖSD exam specification.
- The English glosses are teaching aids, not a claim that German and English map
  one-to-one.

## Production gate

1. Choose a German speaker or German TTS; generate `01.wav` ... `06.wav`.
2. Measure word onsets; update the brief's reveal map and the cue map. If the measured
   runtime exceeds 45 s, cut the English gloss in scenes 3-4 or extend to 50 s. Do not
   rush the German examples or remove the practice arc.
3. Have a German teacher/native editor check the forms, glosses, and the `Sie` note.
4. Only then write composition code and review one scene at a time.

No composition code has been written for this brief.







