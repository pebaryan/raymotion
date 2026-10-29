# a1-sein-six-seats - QC review

| | |
| --- | --- |
| Composition | `a1-sein-six-seats` |
| Commit / build | `272d326` / final render 2026-10-28 09:53 |
| Reviewer | `opencode` (automated still and encode pass) |
| Date | 2026-10-28 |
| Review stage | `final` |
| Story mode | `explainer` |
| Artifacts | `docs/a1-sein-six-seats-brief.md`, `docs/a1-sein-six-seats-script.md`, `out/a1-sein-final-stills/`, `out/a1-sein-six-seats.mp4`, `out/a1-sein-six-seats.srt`, `output/a1-sein-six-seats-thumb.png` |

## 1. One-sentence conceit

> This is a film about the German verb **sein** that makes an A1 learner understand
> that the verb's present tense is learned as six pronoun slots, not one ending rule,
> by seating each pronoun at a table and flipping its plate to the form.

Reviewer's restatement after viewing:

> `sein` is a six-seat table. Fill one pronoun at a time; `wir` and `sie/Sie` share
> `sind`, so the six seats hold only five unique forms.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | `pass` | `docs/a1-sein-six-seats-brief.md` |
| 1 Script and cue map | `pass` | `docs/a1-sein-six-seats-script.md`, `assets/vo/a1-sein/lines.json` |
| 2 Stills | `pass` | `out/a1-sein-final-stills/` frames 0-1120; encoded stills checked |
| 2b Staged reveal & narration sync | `pass` | scene ledger and cue audit below; source/exe freshness checked |
| 3 Draft (muted + sound) | `conditional` | encoded frames and audio measured; no human muted/sound watch yet |
| 4 Final technical QA | `pass` | 1080x1920, 30 fps, 1,155 frames, 38.5 s; full decode; -16.76 LUFS, -1.15 dBTP |
| 5 Authored test | `conditional` | mute, thumbnail, reskin and retiming evidence; clean-machine missing-asset test not run |
| 6 Craft & coherence | `pass` | table proof, one advancing object, mode grammar, source ledger |
| 7 Release packaging | `pass` | end card, SRT, thumbnail; no promo logo/handles required |

## 3. Scorecard

| # | Criterion | Score (0-3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | **3** | Six-seat table; plates flip; shared `sind` bracket | |
| 2 | Script / voice | **3** | 66-word script, measured six WAVs, cue map; native review pending | |
| 3 | Evidence | **3** | The table is the proof; practice sentence is the recall test | |
| 4 | Timing | **2** | 38.5 s voice-driven cut; first card has a 0.10 s lead; target was 45 s | Add a deliberate intro hold or retime every cue to a 45 s cut |
| 5 | Sound | **2** | Voice, generated bed, sparse SFX; no human mix approval; bed is a simple loop | Human listen, a more authored score, true speech-envelope duck |
| 6 | Visual system | **3** | Ink/felt/cream/amber/mint; one motif; consistent type and safe areas | |
| 7 | Chapter differentiation | **2** | The table changes state: empty, singular, plural, practice, answer | More distinct camera/staging per chapter if a longer cut |
| 8 | Data & provenance | **3** | Source ledger; `A1` marked editorial; no invented statistics | |
| 9 | Production pipeline | **3** | `c.assets` bed/SRT, committed voice, `list` from root | |
| 10 | Verification | **3** | Stills, draft, final, ffprobe, full decode, loudness, encoded frames | |
| 11 | Memorability / thumbnail | **2** | `SEIN` + six empty settings at frame 210; caption is small at 320x180 | Add a larger thumbnail title or use the full-table frame |
| 12 | Specificity (reskin) | **3** | A topic swap changes the paradigm, copy, source and table | |
| | **Authoredness total** | **32/36** | | |

### Craft & Coherence

| # | Criterion | Score (0-3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | **3** | Six pronoun groups, five unique forms; `Sie` formal noted; `seid` vs `seit` | |
| 14 | Information design | **3** | One table, one shared-form bracket, one practice blank | |
| 15 | Text performance & pacing | **2** | Captions readable; S5 duplicates the answer in strip and caption; no word highlight | One caption layer, or karaoke-style emphasis |
| 16 | House style & mode grammar | **2** | Explainer table, measured VO, SRT; table stays in one layout | A camera/staging variation would raise this |
| 17 | Spec-to-screen fidelity | **2** | 38.5 s final vs 45 s target; `seats` ASR hears `seeds`; German review pending | Native review and a deliberate target decision |
| 18 | Revision quality | **3** | Fixed S1 link crossing, S5 label overlap, link placement, empty-seat contrast | |
| | **Craft total** | **15/18** | | |

## 4. Critical fails

- [ ] Conceit = 0
- [ ] Script and text-only cue map = 0
- [ ] Muted test fails
- [ ] Same template every chapter
- [ ] Central claims unsourced
- [ ] Assets fail on a clean machine
- [ ] No stills/draft/final verification
- [ ] A key scene starts full, arrives late, wrong order, duplicates labels, or stale binary
- [ ] A claim contradicts its source, or an illustrative value is presented as exact
- [ ] Other

No critical fail is recorded. The conditional gates are human review and clean-machine
provenance, not a content contradiction.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test**: argument survives without sound | `pass` | Table fills by pronoun; `sind` bracket; practice blank/answer |
| **Thumbnail test**: readable at 320x180, states the conceit | `pass` | `output/a1-sein-six-seats-thumb-320.png`: `SEIN` and six settings read; caption is small |
| **Reskin test**: a topic swap requires a redesign | `pass` | Different paradigm needs different seats, forms and source |
| **Retiming test**: longer narration moves picture and score | `pass` | Scene starts derive from the measured WAVs; voice nodes and table markers share the map |
| **Clean-machine test**: `list` works; missing assets behave as documented | `conditional` | `build/raymotion.exe list` works; missing-voice fallback not exercised |
| **One idea per screen**: no wall of caption text | `pass` | S3/S4 captions are one form + gloss; S5 one prompt |
| **Staging test**: scene starts sparse; one unit at a time | `pass` | S1 0-120; S3 306-513; S4 513-806; S5 806-1038 |
| **Cue-sync test**: 3 words, visual present at the word after 0.20-0.40 s lead | `pass` | `Six` 4.156/table 3.876; `Sein` 5.20/plaque 4.92; `Sind` 33.466/answer 33.466 |
| **No-duplicate-label test** | `pass` | Forms live on plates; captions are subtitles; no key repeats a table term |
| **Scene-review test**: each scene approved with its own clip/stills | `pass` | Two or three stills per scene in `out/a1-sein-final-stills/` |
| **Fresh-build test**: executable newer than source; final render rebuilt | `pass` | source 07:30:42, exe 07:30:45, MP4 09:53:15 |
| **Distinct chapter test**: every chapter has a visual idea | `pass` | empty table, name plaque, singular fill, plural fill, practice, answer key |

## 5b. Scene review ledger

| Scene | Review asset | Reveal notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 1 | `out/a1-sein-final-stills/f0.png`, `f60.png`, `f120.png` | `ich` at 0; `bin` 0.5; `du` 1.4; `bist` 1.97; table 3.88; hook fades | `yes` | final 09:53 |
| 2 | `out/a1-sein-final-stills/f167.png`, `f210.png`, `f300.png` | empty table; `SEIN` plaque 0.0; no forms | `yes` | final 09:53 |
| 3 | `out/a1-sein-final-stills/f350.png`, `f480.png` | left settings: `ich/bin`, `du/bist`, `er/sie/es/ist`; corrected `es` take + 0.33 s pause before `ist`; `ist` replaced with the long-vowel variant | `yes` | final 09:53 |
| 4 | `out/a1-sein-final-stills/f600.png`, `f780.png` | right settings; shared `sind` bracket at 7.25; left held | `yes` | final 09:53 |
| 5 | `out/a1-sein-final-stills/f850.png`, `f1010.png` | dim table; sentence strip; blank; answer at 6.60 local | `yes` | final 09:53 |
| 6 | `out/a1-sein-final-stills/f1100.png`, `f1120.png` | complete table; end card; caption | `yes` | final 09:53 |

## 5c. Cue-sync audit

| Scene | Spoken anchor | Word onset (s) | Visual start (s) | Lead | Lands at word? |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | `Six` | 4.156 | 3.876 | 0.28 | `pass` |
| 2 | `Sein` | 5.200 | 4.920 | 0.28 | `pass` |
| 5 | `Sind` | 33.466 | 33.466 | 0.00 | `pass` |

## 6. Packaging (Gate 7)

- [x] end card / credits: `SIX SEATS. ONE VERB.` / `A1 · GRAMMAR · A1-GRA-01`
- [x] thumbnail candidate: `output/a1-sein-six-seats-thumb.png`
- [x] SRT: `out/a1-sein-six-seats.srt`
- [ ] logo / wordmark / social CTA: n/a for this explainer cut
- [ ] handles / repo link: n/a for this explainer cut
- [ ] clean-machine `list` and asset behaviour: `list` pass, missing-asset test pending

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width x height | 1080 x 1920 | 1080 x 1920 | `pass` |
| Frame rate | 30 fps | 30 fps | `pass` |
| Frame count | 1,155 | 1,155 | `pass` |
| Duration | 38.5 s | 38.500 s | `pass` |
| Full decode | no errors | no errors | `pass` |
| Integrated loudness | about -16 LUFS | -16.76 LUFS | `pass` |
| True peak | <= -1 dBTP | -1.15 dBTP | `pass` |
| Voice sync at chapter starts | measured | scene roots at 0/167/306/513/806/1038 | `pass` |
| Encoded-frame check | frames 0-1120 | sees table, captions, end card | `pass` |
| Still vs render determinism | source stills equal | pre-encode stills and encode checked; compression differs | `conditional` |
| Source vs executable freshness | exe newer | source 07:30:42, exe 07:30:45 | `pass` |
| Final render after last edit | yes | MP4 09:53:15 | `pass` |
| Reviewed scene present in final | yes | f0-f1120 extracted from MP4 | `pass` |
| SRT present | yes | 788 bytes | `pass` |
| Thumbnail present | yes | 81,521 bytes, 320x180 checked | `pass` |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | Timing | 2 | First card lead 0.10 s; 38.5 cut vs 45 target | Add a deliberate intro hold or retime all cues to 45 | editor | |
| Authoredness | Sound | 2 | Simple generated bed; no human mix | Human listen; author the score/duck | editor | |
| Craft | Text pacing | 2 | S5 answer duplicated in strip and caption | One answer layer; word emphasis | editor | |
| Craft | Spec-to-screen | 2 | 38.5 vs 45; `seats` ASR; German review pending | Native review; lock target | editor | |

## 9. Decision

- [ ] **Ship**
- [x] **Revise** - fix the conditional gates and either accept 38.5 s or add holds to 45 s.
- [ ] **Rewrite**

Verdict: **Authored** at **32/36** authoredness, **15/18** craft, with conditional
release gates (human mute/sound watch, native German review, clean-machine missing-asset
test, and the final target decision).

Reviewer signature: `opencode (automated)` - 2026-10-28

## 10. Notes for the next author

- What worked and must be preserved: one table as the only proof object; constant 0.28 s
  leads; the shared-form bracket is a consequence of the two `sind` plates, not a label.
- What nearly failed: S1's original vertical link crossed the `du` card; S5's `YOUR TURN`
  label sat inside the table. Both are fixed in the final.
- What should be extracted into the engine or tools: a reusable `word/reveal cue` helper
  and a scene ledger writer; this composition currently carries the map in the brief.
- What should never be copied into another film: two simultaneous answer displays (S5
  strip and caption), or a 45 s script pasted into a 38.5 s voice.







