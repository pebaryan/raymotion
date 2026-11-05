# scripts-09 — QC review

| | |
| --- | --- |
| Composition | `scripts-09` (and `scripts-09-thumb`) |
| Commit / build | uncommitted on `main` @ `9af71b2`; `build/raymotion.exe` 2026-11-05 09:00:55 |
| Reviewer | Claude (author-only review) |
| Date | 2026-11-05 |
| Review stage | final (author); owner watch pending |
| Story mode | promo |
| Artifacts | `docs/scripts-09-brief.md`, `out/scripts-09.mp4`, `out/scripts-09.srt`, `out/scripts-09-thumb_still_0000.png`, `out/qc/scripts-09-sweep/`, `out/qc/scripts-09-sweep.md`, `out/scripts-09-assets/save-0..7/film.as` |

## 1. One-sentence conceit

> This is a film about raymotion 0.9's scripting that makes the viewer feel that a video
> is now a text file you edit and re-run, by writing one script on screen and showing the
> frame it really renders about half a second after every save.

Reviewer's restatement after viewing:

> One `film.as` grows from "hello" to a cheering character and a flag in the wind. Every
> save shows the real frame about 0.5 s later. Two runs give the same pixels, and two broken
> scripts fail with the engine's own messages.

## 2. Gates

| Gate | Result | Evidence |
| --- | --- | --- |
| 0 Brief before code | pass | `docs/scripts-09-brief.md` written and filled before `scripts_09.cpp` |
| 1 Script and cue map | pass | text-only cue map, brief §4; captions in `CAPTIONS[]` match it |
| 2 Stills | pass | brief still list; sweep frames at every chapter start, middle and end (table below) |
| 2b Staged reveal & sync | pass (author) | one entrance at a time: line, save, chip, frame; saves never land inside an edit (asserted in `writeScripts`, 0.2 s margin) |
| 2c Picture integrity | pass (author) | `out/qc/scripts-09-sweep.md` on the final MP4; 13 flags, each explained below; 60 frames described |
| 3 Draft (muted + sound) | conditional | author: muted read via frames; sound via loudness and the cue list. **Owner has not watched it yet.** |
| 4 Final technical QA | pass | §7 |
| 5 Authored test | pass (author) | §5 |
| 6 Craft & coherence | conditional | author scores capped; §3 |
| 7 Release packaging | pass | logo, version, run command, release URL, SRT, thumbnail comp |

## 3. Scorecard

| # | Criterion | Score | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 1 | Conceit | 3 | the whole film is one file and its real frames | — |
| 2 | Script / voice | 2 | text-only by owner choice; 5 short captions plus one footnote | a voice cut would carry more |
| 3 | Evidence | 3 | every preview is the frame the engine rendered from the on-screen text; the errors are caught live from the engine; ZIP sizes are measured | — |
| 4 | Timing | 2 | latency chips are medians of 7 runs (0.53–0.58 s); runs drift about 10% between sessions, so the chips are rounded to 0.1 s | — |
| 5 | Sound | 2 | synth UI clicks on every key, paste, erase, save and frame; D–Bm–G–A pad; -16.0 LUFS | a less generic score |
| 6 | Visual system | 2 | editor/preview/terminal; one teal accent, coral for errors, amber for chips | the end card is a little top-heavy |
| 7 | Chapter differentiation | 3 | hello → ZIP inventory → character → cloth + determinism split → errors → end card | — |
| 8 | Data & provenance | 3 | brief §5 ledger: ZIP contents and sizes, README ffmpeg line, measured latencies, 0-pixel diff | — |
| 9 | Production pipeline | 3 | assets generated through `c.assets`; `list` works from another directory (after the fix in §5); the scripts are written to `out/scripts-09-assets/` and can be re-run | — |
| 10 | Verification | 2 (cap) | author-only | owner watch |
| 11 | Memorability / thumbnail | 2 | cheering character next to its own code; at 320×180 the caption and figure read, the code does not | — |
| 12 | Specificity (reskin) | 3 | about this engine's API and errors; another topic would need a new film | — |
| | **Authoredness total** | **30/36** | | |

| # | Criterion | Score | Evidence / notes | Fix if < 3 |
| --- | --- | --- | --- | --- |
| 13 | Scenario coherence | 3 | each edit follows from the last; the broken scripts are fixed by undoing the edit | — |
| 14 | Information design | 3 | one change per beat; struck vs ticked needs list | — |
| 15 | Text performance & pacing | 2 | typing at 14–38 cps; holds of 2–7 s are long for a promo (the freeze flags) | trim 1 s from the ZIP and end holds |
| 16 | House style & mode grammar | 3 | promo: version, command, URL, logo, CTA | — |
| 17 | Spec-to-screen fidelity | 2 (cap) | matches the brief, with two recorded changes: "run" became "cheer", and the ffmpeg row was added | owner check |
| 18 | Revision quality | 2 (cap) | 3 drafts plus final; every fix re-swept | owner check |
| | **Craft total** | **15/18** | | |

## 4. Critical fails

None checked. Every on-screen number has a source (§5 of the brief). No claim is illustrative.

## 5. Authored tests

| Test | Result | Evidence |
| --- | --- | --- |
| Mute test | pass | there is no voice; the code, the frames and the captions carry it |
| Thumbnail test | pass | `out/qc/thumb-320.png`: the caption "Characters, lights, cameras: from the script." and the figure read; the code is texture |
| Reskin test | pass | nothing survives a topic swap except the three-pane layout |
| Retiming test | pass | save times come from the op list; chips and frames follow `SAVES`; the assert guards edits |
| Clean-machine test | pass (after a fix) | `raymotion list` from `D:\` failed (`create_directories: Access is denied ... out/scripts-09-assets/save-0`): the build wrote the saves itself, and `list` runs build without `c.assets`. Fixed: only `c.assets` writes them, and build skips the script panes when they are not there yet. `list` from `D:\` and from a scratch directory exits 0 and writes no `out/`; a still at frame 1100 after deleting the saves rewrote all 8 and matches the encoded MP4 frame (SSIM 0.990, encode loss) |
| One idea per screen | pass | worst frame is 16 s (ZIP list plus needs list plus caption) |
| Staging test | pass | each chapter starts with the editor in its previous state; one line at a time |
| Cue-sync test | n/a | no voice; captions arrive 0.3–0.8 s after the visual they name |
| No-duplicate-label test | pass | the split labels are "run 1"/"run 2"; the readout is not repeated in the caption |
| Scene-review test | pass (author) | ledger 5b |
| Fresh-build test | pass | §7 |
| Distinct chapter test | pass | §3 #7 |

## 5b. Scene review ledger

| Scene | Review asset | Reveal notes | Approved? |
| --- | --- | --- | --- |
| 1 hello (0–8 s) | sweep t0001–t0008 | `draw` typed, the command typed, then the "hello" frame and a 0.5 s chip. Sync: the frame appears after the chip, at save + latency | author yes; owner pending |
| 2 ZIP (8–17 s) | t0008–t0017 | rows enter one at a time; the three strikes come before the two ticks; caption at 14.9 | author yes |
| 3 character (17–29 s) | t0017–t0029 | erase, paste the stage, type the body; the frame at 22.6+0.58; "run" selected, "cheer" typed, new frame | author yes |
| 4 cloth + determinism (29–40 s) | t0029–t0040 | cloth pasted, wind typed, frame; the "3" becomes "12", frame; split at 38 s, readout, caption | author yes |
| 5 errors (40–50 s) | t0041–t0050 | global typed → compile error 0.02 s and "no new frame"; erased; loop → abort 0.72 s; erased → the cloth frame returns | author yes |
| 6 end (50–60 s) | t0051–t0060 | logo, version, subtitle, the command typed, URL | author yes |

## 5d. Picture integrity sweep (Gate 2c)

`python tools/qc_sweep.py out/scripts-09.mp4 --out out/qc/scripts-09-sweep --report out/qc/scripts-09-sweep.md`

| Flag | Cause | Reason it stays |
| --- | --- | --- |
| flat 7.87–8.00 s | the ZIP slide covers the editor: a sparse header on dark paper for 4 frames | designed; content arrives with the slide 0.13 s later |
| freeze 4.87–7.37 | hello frame and chip hold under the opening caption | designed hold to read the caption |
| freeze 8.03–14.90 | ZIP rows and needs list; each entrance is small, under the detector threshold | rows enter at 8.4–13.5 (checked frames t0009–t0014) |
| freeze 19.43–20.97 | body typing; one character per frame is below the threshold | typing visible frame by frame (t0020, t0021) |
| freeze 21.03–23.13 | typing, then the save and the frame at 23.2 | as above |
| freeze 23.20–25.73 | "run" selection and "cheer" typing | small edits |
| freeze 26.53–28.97 | cheer frame under the caption | designed hold |
| freeze 33.33–35.13 | wind typing, then the save | small edits |
| freeze 35.20–37.13 | the "12" edit and the save | small edits |
| freeze 41.47–43.33 | compile error and "no new frame" | designed hold to read the error |
| freeze 46.93–49.47 | caption and footnote | designed hold to read the footnote |
| freeze 50.03–52.00 | logo settling, then the version line enters | small entrance |
| freeze 53.27–59.97 | the command typing, then the URL and the hold | end card; the longest hold, see §8 |

One line per sweep frame (tNNNN = second NNNN-1), written after opening each frame.
Frames 1, 5, 8, 13–17, 20, 39, 40 were reopened on the final render. They were the frames
that changed against draft 3. The other frames differ from draft 3 by H.264 noise only (≤ 16 grey levels).

| t (s) | What is in the frame | Plan? |
| --- | --- | --- |
| 0 | empty editor: lines 1–4 dimmed, line numbers 5–6, caret; caption "A video is a text file now." | yes |
| 1 | `void draw(int t) {` typing begins on line 6 | yes |
| 2 | `draw` body: the text line typing | yes |
| 3 | `draw` closes; caret at the end | yes |
| 4 | the command typing in the terminal | yes |
| 5 | "hello" in the preview; chip "save → frame 0.5 s" | yes |
| 6 | same, chip holding; caption | yes |
| 7 | the ZIP slide begins from the right | yes |
| 8 | ZIP header "raymotion-v0.9.1-windows-x64.zip", "125 MB · 286 files" | yes |
| 9 | `raymotion_script.exe 8.3 MB` row highlighted | yes |
| 10 | `raymotion.exe`, `scripts/hello.as` rows | yes |
| 11 | `assets/ 275 files`; needs list title | yes |
| 12 | README/LICENSE/NOTICE row; needs items listed | yes |
| 13 | "a C++ compiler" struck | yes |
| 14 | "CMake" struck | yes |
| 15 | "a build of raymotion" struck; "a text editor" ticked | yes |
| 16 | "ffmpeg, for MP4 files" ticked; caption "Unzip. Write. Render. No compiler." | yes |
| 17 | the ZIP slides out left; the editor and preview come in from the right; the caption fades | yes |
| 18 | `hello` body erased; the stage pasted (lines 7–12) | yes |
| 19 | caret on line 13, ready for the body; preview still "hello" (stale by design) | yes |
| 20 | `string body = """{"clips":...` typing; preview "hello" | yes |
| 21 | `addCharacter(body)` typing | yes |
| 22 | the character running in the preview; chip 0.6 s | yes |
| 23 | "run" selected in the JSON | yes |
| 24 | "cheer" typing | yes |
| 25 | the character cheering, arms up; chip 0.6 s | yes |
| 26 | same; caption "Characters, lights, cameras: from the script." | yes |
| 27 | same hold (the thumbnail frame is 26.6) | yes |
| 28 | the body lines selected for erase | yes |
| 29 | the body erased; preview still shows the cheer frame | yes |
| 30 | `PhysicsCloth@ flag = physics::cloth(18, 12, 0.1f,` pasted | yes |
| 31 | `PhysicsWind@ wind` typing | yes |
| 32 | `wind.gust(0.6f)` typing | yes |
| 33 | `addCloth(flag)`; the save | yes |
| 34 | the flag hanging in a light wind; chip 0.6 s | yes |
| 35 | the "3" selected in `strength(3)` | yes |
| 36 | "12" typed | yes |
| 37 | the flag whipped out by the stronger wind; chip | yes |
| 38 | the preview split: "run 1"/"run 2", two identical flags; two commands in the terminal | yes |
| 39 | readout "pixels that differ: 0 of 921,600"; caption "Same frame, every run." | yes |
| 40 | back to one pane; the flag | yes |
| 41 | `int frames = 0;` typed above `draw` | yes |
| 42 | coral `film.as:5:1: Global variables have been disabled by the application`; the preview dims, "no new frame" | yes |
| 43 | the global line erased | yes |
| 44 | `while (true) {}` typed on line 7 | yes |
| 45 | coral `script execution aborted (statement budget exceeded)`; "no new frame" | yes |
| 46 | the loop line selected; caption "No globals. No runaway loops." | yes |
| 47 | the loop erased; the footnote "Scripts are trusted code…" | yes |
| 48 | the flag returns with a 0.6 s chip; the footnote | yes |
| 49 | the end-card slide begins | yes |
| 50 | the logo entering | yes |
| 51 | the logo settled; "raymotion 0.9.1" | yes |
| 52 | "bodies, physics and scripts" | yes |
| 53 | the command begins typing | yes |
| 54 | `raymotion_script preview hello-script` typing | yes |
| 55 | `--script scripts/hello.as` complete | yes |
| 56 | the URL `github.com/pebaryan/raymotion/releases` | yes |
| 57 | hold | yes |
| 58 | hold | yes |
| 59 | hold, final frame | yes |

Keyed effects: none (no fog, DOF or post effect is keyed).

- [x] Camera clearance (3D): the script camera is static at (2.2, 1.3, 2.2), looking at (0, 1.1, 0), above the ground plane at y 0 in every frame
- [x] Closest approach: the character stands on the plane; the flag is pinned along its top row and does not intersect the ground
- [x] Payoff frame: 39 s, two identical flag frames and a 0-pixel diff
- [x] Ramps: the slides are a 0.7 s smoothstep; no pops flagged

## 5e. Who approved what

| Item | Approver | Artifact they saw | Their words |
| --- | --- | --- | --- |
| Format | pebaryan | question in chat | "Landscape ~60 s (Recommended)", "Text only" |
| Final film | pebaryan | — | **not yet watched** |

## 6. Packaging (Gate 7)

- [x] title / credits: the end card
- [x] thumbnail candidate: `scripts-09-thumb` → `out/scripts-09-thumb_still_0000.png`
- [x] SRT: `out/scripts-09.srt`, 7 cues, no overlaps (the footnote joins its caption as a second line)
- [x] logo and CTA: the logo, the run command, the release URL
- [x] repo link
- [x] clean-machine `list`

## 7. Technical evidence

| Check | Expected | Actual | Result |
| --- | --- | --- | --- |
| Width × height | 1920×1080 | 1920×1080 h264 | pass |
| Frame rate | 30 | 30/1 | pass |
| Frame count | 1800 | 1800 | pass |
| Duration | 60 s | 60.000 s; aac 48 kHz stereo | pass |
| Full decode | no errors | 0 errors | pass |
| Integrated loudness | -16 LUFS | -16.0 LUFS, LRA 9.5 LU | pass |
| True peak | ≤ -1 dBTP | -4.5 dBTP | pass |
| Voice sync | n/a | text only | — |
| Encoded-frame check | frames from the MP4 | the 60 sweep frames came from the encoded file | pass |
| Determinism | identical | `still --at 60` twice on save-4: 0 of 921,600 pixels differ | pass |
| Latency (on screen) | measured | medians of 7 runs: 0.53, 0.58, 0.57, 0.56, 0.55, 0.56 s; compile error 0.02 s; abort 0.72 s. Runs drift about 10% between sessions | pass |
| Source vs executable | exe newer | source 09:00:39, exe 09:00:55 | pass |
| Final render after last edit | yes | MP4 09:02:23 | pass |
| SRT present | yes | yes | pass |
| Thumbnail present | yes | yes | pass |

## 8. Two lowest criteria

| Axis | Criterion | Score | Why | Fix |
| --- | --- | --- | --- | --- |
| Authoredness | Sound | 2 | a generic synth pad | a composed cue or a licensed track |
| Authoredness | Timing | 2 | the 7 s end hold and the 7 s ZIP hold are long for a promo | trim each by about 1–2 s for a 55 s cut |
| Craft | Text pacing | 2 | as above | as above |
| Craft | Spec-to-screen / Revision | 2 (cap) | author-only | owner watch |

## 9. Decision

- [ ] Ship: needs the owner to watch `out/scripts-09.mp4` muted, then with sound (§5e)
- [x] **Revise only if the owner asks**; otherwise ship after the watch. Keep: the one-file
      conceit, the real frames and errors, and the determinism split

Verdict: Authored (pending owner) at **30/36** authoredness, **15/18** craft. The scores
are author-only, so the film is not release-ready until the owner watches it.

Reviewer signature: Claude — 2026-11-05

## 10. Notes for the next author

- Preserve: the preview pane is the real script output, grafted from `script::registerFile`; errors are the engine's own messages, caught at render time.
- What nearly failed: walk vs wave looked the same at frame 60, so the film uses run and cheer; save times landing inside an edit (now asserted); the missing ffmpeg row.
- What to extract: the editor op model (`docAt(t)` from Type/Paste/Erase ops) would suit a `code::` helper.
- Never copy: hard-coded latency chips without re-measuring on the shipping build.
