# <slug> — QC review

> Copy to `out/<slug>-qc.md` or `docs/<slug>-qc.md` and fill it in. A score without
> evidence is not a review. The reviewer must have opened the stills and watched the
> draft, not just read the code. Statistics and captions find problems; they do not
> approve frames. If a later pass finds a defect an earlier gate missed, reopen that gate
> here and strike its scores.

| | |
| --- | --- |
| Composition | `<slug>` |
| Commit / build | `<hash or build id>` |
| Reviewer | `<name>` |
| Date | `<date>` |
| Review stage | `brief / stills / draft / final / re-review` |
| Story mode | `explainer / narrative / promo / utility` |
| Artifacts | `<brief, script, sources, stills, draft, final, SRT, thumbnail>` |

## 1. One-sentence conceit

> `<paste the conceit from the brief, or write "MISSING">`

Reviewer's restatement after viewing:

> `<one sentence. If it differs from the brief, the film or the brief is wrong.>`

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | `pass / conditional / fail` | `<link>` |
| 1 Script and cue map | `pass / conditional / fail` | `<link>` |
| 2 Stills | `pass / conditional / fail` | `<frames, contact sheet>` |
| 2b Staged reveal & narration sync | `pass / conditional / fail` | `<scene ledger, sync audit, stale-binary check>` |
| 2c Picture integrity | `pass / conditional / fail` | `<qc_sweep report on the final MP4, §5d sweep table, effect on/off diffs, camera clearance log>` |
| 3 Draft (muted + sound) | `pass / conditional / fail` | `<timecoded notes>` |
| 4 Final technical QA | `pass / conditional / fail` | `<ffprobe, decode, loudness, encoded frames>` |
| 5 Authored test | `pass / conditional / fail` | `<mute, thumbnail, reskin, retiming, clean machine>` |
| 6 Craft & coherence | `pass / conditional / fail` | `<scenario coherence, information design, text pacing, mode grammar, spec-to-screen, revision quality>` |
| 7 Release packaging | `pass / conditional / fail` | `<title/credits, thumbnail, SRT; logo/social/CTA for a promo>` |

A fail at any gate stops the release. Record the exact failure.

## 3. Scorecard

0 = absent, 1 = generic, 2 = present, 3 = authored. Anchors are in
`skills/raymotion/references/quality-control.md`.

| # | Criterion | Score (0–3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | | | |
| 2 | Script / voice | | | |
| 3 | Evidence | | | |
| 4 | Timing | | | |
| 5 | Sound | | | |
| 6 | Visual system | | | |
| 7 | Chapter differentiation | | | |
| 8 | Data & provenance | | | |
| 9 | Production pipeline | | | |
| 10 | Verification | | | |
| 11 | Memorability / thumbnail | | | |
| 12 | Specificity (reskin) | | | |
| | **Authoredness total** | **/36** | | |

### Craft & Coherence (second axis)

Score these 0-3 as well. Anchors are in `skills/raymotion/references/quality-control.md`.

| # | Criterion | Score (0-3) | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | | | |
| 14 | Information design | | | |
| 15 | Text performance & pacing | | | |
| 16 | House style & mode grammar | | | |
| 17 | Spec-to-screen fidelity | | | |
| 18 | Revision quality | | | |
| | **Craft total** | **/18** | | |

## 4. Critical fails

Check every one. Any checked box caps the verdict at Brief-complete (21).

- [ ] Conceit = 0
- [ ] Script and text-only cue map = 0
- [ ] Muted test fails (Evidence ≤ 1)
- [ ] Same template every chapter (Differentiation = 0)
- [ ] Central claims unsourced (Provenance = 0)
- [ ] Assets fail on a clean machine (Pipeline = 0)
- [ ] No stills/draft/final verification (Verification = 0)
- [ ] A key scene starts at final density, arrives after its spoken anchor, uses the
      wrong semantic order, duplicates diagram/key labels, or came from a stale binary
- [ ] A claim contradicts its source, or an illustrative value is presented as exact
- [ ] Other: `<describe>`

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| **Mute test**: argument survives without sound | `pass / fail` | `<what the images say>` |
| **Thumbnail test**: readable at 320×180, states the conceit | `pass / fail` | `<frame number>` |
| **Reskin test**: a topic swap requires a redesign | `pass / fail` | `<what would survive>` |
| **Retiming test**: longer narration moves picture and score | `pass / fail` | `<which chapter was tested>` |
| **Clean-machine test**: `list` works; missing assets behave as documented | `pass / fail` | `<command and output>` |
| **One idea per screen**: no wall of caption text | `pass / fail` | `<worst frame>` |
| **Staging test**: scene starts sparse; one unit at a time | `pass / fail` | `<scene + frames>` |
| **Cue-sync test**: 3 words, visual present at the word after 0.20-0.40 s lead | `pass / fail` | `<cue table rows>` |
| **No-duplicate-label test**: no term in both diagram and key | `pass / fail` | `<term + locations>` |
| **Scene-review test**: each scene approved with its own clip/stills | `pass / fail` | `<scene ledger>` |
| **Fresh-build test**: executable newer than source; final render rebuilt | `pass / fail` | `<mtimes or hash>` |
| **Distinct chapter test**: every chapter has a visual idea | `pass / fail` | `<counterexample or none>` |

## 5b. Scene review ledger

| Scene | Review asset (clip / stills) | Reveal notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| `<01>` | `<path>` | `<what arrived, order, lead>` | `yes / revise` | `<new path>` |
| `<02>` | | | | |

## 5c. Cue-sync audit

| Scene | Spoken anchor (word/phrase) | Word onset (s) | Visual start (s) | Lead | Lands at word? |
| --- | --- | --- | --- | --- | --- |
| `<scene>` | `<word>` | `<s>` | `<s>` | `<0.20-0.40>` | `pass / fail` |
| | | | | | |

## 5d. Picture integrity sweep (Gate 2c)

Run `python tools/qc_sweep.py out/<slug>.mp4 --out out/qc/<slug>-sweep --report out/qc/<slug>-sweep.md`
on the **final** encoded file. Paste the flags, then explain or fix each one.

| Flag (type, timecode) | Cause | Fixed / reason it stays |
| --- | --- | --- |
| `<flat 0:45-0:48>` | `<camera inside dust layer>` | `<thinned; re-swept: gone>` |

One line per sweep frame, written after opening the frame. "Looks fine" is not a line.

| t (s) | What is in the frame | Matches the plan? |
| --- | --- | --- |
| 0 | `<street, title rising, sun glare upper left>` | `yes / no: ...` |

Keyed effects, rendered off vs on (a zero diff is a dead effect):

| Effect | Frame | Mean diff off→on | Live? |
| --- | --- | --- | --- |
| `<fog>` | `<f0300>` | `<12.4>` | `yes / DEAD` |

- [ ] Camera clearance checked every frame (3D): `<min height / log path>`
- [ ] Closest-approach stills of built objects: nothing floats or intersects; primitives keep their proportions
- [ ] Payoff frame described by eye: `<frame, what is seen>`
- [ ] Gradual ramps sampled every 0.5 s: no stalls, no pops

## 5e. Who approved what

Quote the owner's words; an agent's probe of a draft is not a watch.

| Item | Approver | Artifact they saw | Their words |
| --- | --- | --- | --- |
| Final film | `<owner>` | `<out/slug.mp4, full size, real time>` | `"<quote>"` |

## 6. Packaging (Gate 7)

Mode-appropriate checklist:

- [ ] title / credits (all modes except utility, per spec)
- [ ] thumbnail candidate (explainer, narrative, promo)
- [ ] SRT (explainer, narrative, promo)
- [ ] logo / wordmark and social/CTA (promo; optional for explainer/narrative)
- [ ] handles and repo link (promo)
- [ ] clean-machine `list` and asset behaviour

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | | | |
| Frame rate | | | |
| Frame count | | | |
| Duration | | | |
| Full decode | no errors | | |
| Integrated loudness | `<target>` | | |
| True peak | `≤ -1 dBTP` | | |
| Voice sync at chapter starts | | | |
| Encoded-frame check | | | |
| Still vs render determinism | identical | | |
| Source vs executable freshness | exe newer | | |
| Final render after last edit | yes | | |
| Reviewed scene present in final | yes | | |
| SRT present | | | |
| Thumbnail present | | | |

## 8. Two lowest criteria

Name the two lowest criteria on each axis.

| Axis | Criterion | Score | Why it is low | Fix | Owner | Re-score |
| --- | --- | --- | --- | --- | --- | --- |
| Authoredness | | | | | | |
| Authoredness | | | | | | |
| Craft | | | | | | |
| Craft | | | | | | |

## 9. Decision

- [ ] **Ship** — Authoredness ≥ 22 and Craft ≥ 13, no critical fail, Gates 0–7 (with 2c)
      passed, and the owner has watched the final file (§5e). An author-only review scores
      criteria 10, 17 and 18 at most 2.
- [ ] **Revise** — apply the fixes above and re-review. Keep: `<conceit/script/scenes>`.
- [ ] **Rewrite** — no conceit/script, or the form is a template. Start from the brief.

Verdict: `<Half-assed / Brief-complete / Authored / Improvised>` at **`<authoredness>/36` authoredness**, **`<craft>/18` craft**.

Reviewer signature: `<name>` — `<date>`

## 10. Notes for the next author

- What worked and must be preserved: `<...>`
- What nearly failed: `<...>`
- What should be extracted into the engine or tools: `<...>`
- What should never be copied into another film: `<...>`
