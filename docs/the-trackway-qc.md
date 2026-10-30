# the-trackway — QC review

| | |
| --- | --- |
| Composition | `the-trackway` (+ `the-trackway-thumb`) |
| Commit / build | on top of `8b88334` (uncommitted: `examples/showcase/trackway.cpp`); `build/raymotion.exe` 15:17:39, source 15:17:35, final render 15:19:11 |
| Reviewer | Claude (agent); owner sign-off pending |
| Date | 2026-10-30 |
| Review stage | final |
| Story mode | promo (for `rig::` animals, gaits and mounts; the claims about real animals are sourced) |
| Artifacts | `docs/the-trackway-brief.md`, `out/the-trackway.mp4`, `out/the-trackway.srt`, `out/the-trackway-thumb.png`, `out/the-trackway-stills/f*.png` (from the encoded MP4), `out/the-trackway-assets/score.wav` |

## 1. One-sentence conceit

> This is a film about **how bodies move** that makes the viewer **see a gait as a pattern
> the feet write on the ground** by **letting every animal cross one strip of sand and leave
> its own trail — until the trails stop, because a body took to the air, the water, the
> bars, or another body's back**.

Reviewer's restatement after viewing:

> One strip of sand records every body that crosses it: hooves in changing orders, a hen's
> staircase head, a snake's single groove, a T. rex print holding a hen's, and trails that
> end where a body flies, swims, swings or climbs onto another.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | brief written and approved ("go") before `trackway.cpp`; conceit, proof object per chapter, caption script, claim ledger, visual system, audio plan, reveal map, stills list, thumbnail |
| 1 Script and cue map | pass | text-only, declared in the brief header; caption table §4 drives screen and SRT; reveal map §9 retimed after the first render (§14) |
| 2 Stills | pass | every chapter at start/middle/end and the transitions, first from `still`, then from the encoded MP4 (§10 frames, `out/the-trackway-stills/`); the issues found are in §10 and fixed |
| 2b Staged reveal & narration sync | pass | ledger §5b; each scene starts on empty sand or a single animal, and overlays arrive one at a time; cue audit §5c; exe newer than source, final render newer than exe |
| 3 Draft (muted + sound) | **conditional** | the agent can't watch in real time or hear: the muted review is from encoded frames, the sound from the cue code and loudness. **The owner must watch once muted and once with sound** |
| 4 Final technical QA | pass | §7: ffprobe, full decode, -16.0 LUFS / -1.7 dBTP, frames from the MP4, deterministic stills |
| 5 Authored test | pass | §5 |
| 6 Craft & coherence | pass | craft 16/18 (§3) |
| 7 Release packaging | pass | end card with title, API line, logo prefab and repo URL (CTA); SRT; thumbnail comp |

## 3. Scorecard

| # | Criterion | Score (0–3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | 3 | one strip of sand is the whole film; each chapter is a different hand writing on it | — |
| 2 | Script / voice | 2 | ten speakable caption lines, one per beat; no voice | voiced cut (Qwen clone) if the owner wants one |
| 3 | Evidence | 3 | prints, footfall chart and thuds all come from one contact list detected from the solved feet; the chart's gap is a measured airborne span, not drawn by hand | — |
| 4 | Timing | 2 | reveals lead their caption phrases by 0.27–0.33 s; but the gallop gap is only ~6 frames, and transitions are fast trucks over empty sand | longer gap pose in `clips::gait`'s gallop; a held beat at each transition |
| 5 | Sound | 2 | score generated from the contacts (thuds per hoof, silence in the gap, hiss, splash, creaks, plucks, end bell); not heard by the reviewer | owner listen (Gate 3) |
| 6 | Visual system | 3 | warm paper, pale sand, pressed prints in darker sand, teal for data, coral for the trail's last print; one camera grammar (follow + truck) | — |
| 7 | Chapter differentiation | 3 | chart (2), head traces (3), nested prints and push-in (4), groove (5), three endings (6), counters (7), map (8) | — |
| 8 | Data & provenance | 3 | Muybridge 1878, Hildebrand 1965, Necker 2007, Gray 1946 in the brief §5; the T. rex ratio is labelled "sizes illustrative" on screen | — |
| 9 | Production pipeline | 3 | score in `c.assets`, regenerated when missing; `list` from another directory works; SRT written by the composition | — |
| 10 | Verification | 2 | every scene reviewed from stills and encoded frames; determinism checked; no real-time watch | owner watch (Gate 3) |
| 11 | Memorability / thumbnail | 3 | the hen's print inside a T. rex's middle toe; reads at 320×180 | — |
| 12 | Specificity (reskin) | 3 | the film is made of contacts; swapping the topic leaves only the paper and type | — |
| | **Authoredness total** | **32/36** | | |

### Craft & Coherence (second axis)

| # | Criterion | Score (0-3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | 2 | one continuous world, prints never move; but the pilot tilts while lifted, and the titan is scaled 2.6x so its palm is in reach | fix the palm grip orientation in `Character::mount` |
| 14 | Information design | 3 | chart lanes labelled once (LH/LF/RF/RH), gait names on the chart's time axis, one label per print, a single counter for chapter 7 | — |
| 15 | Text performance & pacing | 2 | one headline and one caption per beat at ~2.5 words/s; chapter 6's 11-word caption must cover three events | split chapter 6 into three short lines timed to each ending |
| 16 | House style & mode grammar | 3 | raymotion paper/ink/teal, kicker and chapter counter, logo prefab, API line and URL on the end card | — |
| 17 | Spec-to-screen fidelity | 3 | every brief chapter is on screen; departures (T. rex ratio, chapter 6 wording, hen run, titan scale) recorded in brief §14 | — |
| 18 | Revision quality | 3 | first-render issues (single detected hoof, overexposed sand, aliasing ripples, hidden duck, camera chasing the hen into the sky, rider standing on the horse, a 1-frame false gap, -20.8 LUFS) each fixed at the cause | — |
| | **Craft total** | **16/18** | | |

## 4. Critical fails

- [ ] Conceit = 0
- [ ] Script and text-only cue map = 0
- [ ] Muted test fails (Evidence ≤ 1)
- [ ] Same template every chapter (Differentiation = 0)
- [ ] Central claims unsourced (Provenance = 0)
- [ ] Assets fail on a clean machine (Pipeline = 0)
- [ ] No stills/draft/final verification (Verification = 0)
- [ ] A key scene starts at final density, arrives after its anchor, uses the wrong semantic
      order, duplicates diagram/key labels, or came from a stale binary
- [ ] A claim contradicts its source, or an illustrative value is presented as exact
- [ ] Other: none

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test** | pass | the film is designed muted: prints, chart gap, head traces, groove and counters carry every claim (f280–f1880) |
| **Thumbnail test** | pass | `out/the-trackway-thumb.png`: chapter 4's push-in at `henGhost.t + 1.0`, title and labels legible at 320×180 |
| **Reskin test** | pass | a new topic would need new contacts, charts and endings; only the palette and type survive |
| **Retiming test** | pass | chapter starts come from `DUR`; key timings (gait changes, takeoff, mounts) are offsets from chapter starts; overlays and thuds follow the detected contacts, so changing a chapter's length moves picture and score together (by construction; the gap-outline and Muybridge-card retimes moved both) |
| **Clean-machine test** | pass | `raymotion list` run from another directory lists `the-trackway` and `the-trackway-thumb`; missing `score.wav` is regenerated on render |
| **One idea per screen** | pass | worst frame f1280 (chapter 6 caption plus the "trail ends: air" label): two short lines |
| **Staging test** | pass | ch1 prints on empty sand before the horse; ch2 chart before the gap, gap before the card; ch3 hen trace before the duck's; ch4 T. rex print before the hen's; ch7 counter after each mount |
| **Cue-sync test** | pass | §5c |
| **No-duplicate-label test** | pass | lane names only on the chart; gait names only on its axis; animal names only as print labels in ch4 and the ch7 counter heading |
| **Scene-review test** | pass | §5b |
| **Fresh-build test** | pass | source 15:17:35 < exe 15:17:39 < final render 15:19:11 |
| **Distinct chapter test** | pass | none: each chapter's proof object differs (§3 row 7) |

## 5b. Scene review ledger

| Scene | Review asset (clip / stills) | Reveal notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 01 ground remembers | stills at 0, 40, 90, 150, 178 | prints press one at a time on empty sand, then the ghost horse fades in on them | yes | f40, f150 |
| 02 four feet | stills at 180–540; chart frames at the walk/trot/gallop changes and the gap | chart draws from the contacts; the gap outline leads caption 2b by 0.30 s; Muybridge card 1.0 s later. First pass found only one hoof and a 1-frame false gap: fixed | yes (after revision) | f280, f400 |
| 03 two feet | stills at 540–810 | hen staircase trace first, duck follows 0.5 m behind with a smooth trace on the same scale. First pass: duck hidden behind the hen; fixed | yes (after revision) | f620, f760 |
| 04 any size | stills through the push-in | T. rex print first; hen print appears after the foot lifts; push-in, then crane back. First pass: focus print under the standing foot; fixed | yes (after revision) | f880 |
| 05 no feet | stills at 990–1170 | groove draws behind the head; the "one groove" label leads its phrase by 0.27 s | yes | f1100 |
| 06 where trails end | stills at each ending | hen runs and flies; the camera holds on the last prints (coral, then label); turtle at the waterline; chimp under the first bar. First pass: camera chased the hen upward; fixed | yes (after revision) | f1280, f1400 |
| 07 one trail | stills at each mount | rider, passenger, pilot each step off their own trail; counter after each mount. First pass: rider stood on the horse before the mount blend; fixed. Known: the pilot tilts while lifted | yes, with the known issue | f1640, f1880 |
| 08 end card | stills at 1890–2099 | crane up to the map of trails, then title, API line, logo, URL | yes | f2060 |

## 5c. Cue-sync audit

Text-only: the anchor is the caption phrase's onset at ~2.5 words/s from the caption's start.

| Scene | Anchor (caption phrase) | Onset (local s) | Visual start (local s) | Lead | Lands at word? |
| --- | --- | --- | --- | --- | --- |
| 02 | "At the gallop, … nothing touches" (caption 2b) | 7.90 | gap outline 7.60 | 0.30 | pass |
| 03 | "doesn't bob" | ~5.5 | duck trace 5.20 | 0.30 | pass |
| 04 | "nine times" | ~1.87 | hen print 1.55 | 0.32 | pass |
| 05 | "Every part of it follows" | ~2.27 | groove highlight 2.00 | 0.27 | pass |
| 06 | each trail end's label | endsAt + 0.35 | coral last prints endsAt + 0.05 | 0.30 | pass |

## 6. Packaging (Gate 7)

- [x] title / credits: end card "EVERY BODY SIGNS THE GROUND."
- [x] thumbnail candidate: `out/the-trackway-thumb.png`
- [x] SRT: `out/the-trackway.srt`
- [x] logo / wordmark and CTA: logo prefab and `github.com/pebaryan/raymotion`
- [x] API line: `rig::animal · clips::gait · Character::mount`
- [x] clean-machine `list` and asset behaviour

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920×1080 | 1920×1080 h264 | pass |
| Frame rate | 30 | 30 | pass |
| Frame count | 2100 | 2100 | pass |
| Duration | 70 s | 69.999 s; AAC 48 kHz stereo | pass |
| Full decode | no errors | no errors | pass |
| Integrated loudness | -16 LUFS | -16.0 LUFS, LRA 5.4 | pass |
| True peak | ≤ -1 dBTP | -1.7 dBTP | pass |
| Voice sync at chapter starts | n/a | text-only | n/a |
| Encoded-frame check | frames from the MP4 | 13 frames (§10 list) in `out/the-trackway-stills/` | pass |
| Still vs render determinism | identical | repeated stills byte-identical; still vs encoded 44.6 dB PSNR | pass |
| Source vs executable freshness | exe newer | 15:17:35 < 15:17:39 | pass |
| Final render after last edit | yes | 15:19:11 | pass |
| Reviewed scene present in final | yes | encoded stills match the reviewed stills | pass |
| SRT present | yes | yes | pass |
| Thumbnail present | yes | yes | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | Timing | 2 | the gallop gap is ~6 frames; transitions are quick trucks | longer suspension in the gallop clip; a held beat per transition | agent | after the fix |
| Authoredness | Verification | 2 | no real-time or audio review | owner watches muted, then with sound | owner | after Gate 3 |
| Craft | Scenario coherence | 2 | the pilot tilts while lifted; titan scaled for reach | palm-grip orientation in `Character::mount` | agent | after the fix |
| Craft | Text pacing | 2 | chapter 6 carries three events in one caption | three short captions | agent | after the fix |

## 9. Decision

- [ ] **Ship** — Authoredness ≥ 22 and Craft ≥ 13, no critical fail, Gates 0–7 passed.
- [x] **Revise** — only Gate 3: the owner's muted and sound watch. Keep everything else.
- [ ] **Rewrite**

Verdict: **Authored** at **32/36 authoredness**, **16/18 craft** (meets the showcase bar),
pending Gate 3.

Reviewer signature: Claude (agent) — 2026-10-30

## 10. Notes for the next author

- What worked and must be preserved: one contact list drives prints, chart and sound, so
  every claim is a measurement; the world is continuous and the prints are pure functions
  of `t` (each tile redraws the contacts it holds up to `t`).
- What nearly failed: contact detection (a global minimum height is skewed by gait blends:
  use a low percentile); gap detection (require a minimum duration); `Mount.from` (the blend
  eases in before `from`, so add the blend).
- What should be extracted into the engine or tools: contact detection from a solved rig
  (`rig::contacts(track, foot, eps, vmax)`) and the print-tile renderer.
- What should never be copied into another film: the hard-coded titan scale for reach;
  use a grip that adapts to the carrier's size.
