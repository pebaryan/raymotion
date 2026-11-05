# scripts-09 — authoring brief

> Status: `in production`. Revised after the first build: the terminal runs `still` (the
> command that was measured), every chip shows that save's own measured time, and the
> timings below are the ones in the code.

| | |
| --- | --- |
| Composition name | `scripts-09` (thumbnail: `scripts-09-thumb`) |
| Story mode | `promo` |
| Owner / agent | owner: pebaryan; author: Claude (agent) |
| Status | `in production` |
| Runtime target | 60 s (1800 frames) |
| Platform / aspect | 1920x1080, 30 fps; YouTube, the v0.9 release page, social links |
| Voice | none: the owner chose a text-only cut. The code written on screen carries the argument; one caption line names each beat. |
| Audience | developers and motion designers who know raymotion or saw the v0.8 promo; some write C++, many do not. Afterwards they should be able to say: "with 0.9 I can download the ZIP and make video, 3D included, by editing a text file, with no compiler." |
| Source material | the v0.9.0/0.9.1 releases; `docs/scripting.md`; `examples/scripts/*.as`; the measurements in §5 |

## 1b. Mode contract

| | |
| --- | --- |
| Mode | promo |
| Goal | make scripting desirable: the loop from idea to frame is now a text edit |
| Proof object | a live demo: real `.as` source on screen and its real rendered output beside it |
| Required evidence | claim ledger (§5): every number measured or read from the release |
| Text | one caption line per beat (≤ 8 words), plus the code itself |
| House grammar | raymotion brand: dark paper, Inter, JetBrains Mono, the raymotion logo |
| Packaging | thumbnail, SRT of the caption lines, logo end card with version, command and URL |

## 1c. Reel / feed cut

n/a: landscape cut only.

## 1. One sentence

> This is a film about **raymotion 0.9's scripting** that makes the viewer **feel that a
> video is now a text file you edit and re-run** by **writing one script on screen and
> showing the frame it really renders about half a second after every save**.

## 2. One-line argument

> With 0.9, anyone with the release ZIP can make video, 3D and physics included, from a
> text file: no compiler, about half a second from save to frame.

## 3. Chapters

The persistent frame is the **editor, preview and terminal**: `film.as` on the left, the
frame it renders top right, the command and its output below that. Each chapter changes
what that frame does, so it is a film and not one layout with new text.

| # | Time (s) | Chapter | On-screen caption | Visual action | Proof object | Sound cue |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0:00–0:08 | A text file | `A video is a text file now.` | the editor holds only the four `configure` lines (dimmed: written once, never edited); the cursor types `draw` with a moving "hello". Save: the terminal runs `raymotion_script still film --at 60 --script film.as`, prints `wrote out/film_still_0060.png`, the preview shows that frame, an amber chip reads `save → frame 0.5 s` | the edit and its frame, with the measured time; the frame appears that long after the save | key clicks; a two-note save blip; a high blip when the frame lands |
| 2 | 0:08–0:17 | No compiler | `Unzip. Write. Render. No compiler.` | full-screen slide: the release ZIP's real rows (`raymotion_script.exe 8.3 MB`, `raymotion.exe 11.9 MB`, `scripts/hello.as 2.2 KB`, `assets/ 275 files`, the text files). Beside it "To make a video you need": a C++ compiler, CMake, a build of raymotion, a text editor, ffmpeg (for MP4 files). The first three are struck through in turn; the text editor and ffmpeg get teal ticks (the ZIP README lists ffmpeg on PATH for MP4 output) | the real file list of the v0.9.1 ZIP; the struck-through toolchain | whoosh in; a pop per strike; chime on the tick |
| 3 | 0:17–0:29 | A body | `Characters, lights, cameras: from the script.` | back to the editor; the hello lines are selected and erased; a scene setup is pasted (world, camera, light, ground, render); two lines are typed: a `body` with clip `"run"`, `addCharacter`. Save: the character mid-stride. One word changes, `run` → `cheer`; save; both arms go up | the character answering a one-word edit | clicks; save blips; frame blips |
| 4 | 0:29–0:40 | Physics | `Same frame, every run.` | the character lines are erased; a cloth is pasted; five wind lines are typed. Save: the cloth in the wind. `strength(3)` → `strength(12)`; save; it whips. The preview splits into `run 1` and `run 2` of the same frame; a readout: `pixels that differ: 0 of 921,600` | the cloth answering the edit; a measured pixel diff of two runs | clicks; blips; chime on the readout |
| 5 | 0:40–0:50 | Guard rails | `No globals. No runaway loops.` | `int frames = 0;` is typed above `draw`; save; the terminal prints the real compile error (`film.as:5:1: Global variables have been disabled by the application`); the preview dims: "no new frame". The line is erased. `while (true) {}` is typed into `draw`; save; the film runs it and prints the real abort (`script execution aborted (statement budget exceeded)`). The line is erased; save; the cloth returns. Footnote: `Scripts are trusted code: run only scripts you would run as a program.` | the two real error messages, caught from the engine while the film renders | a thud per error; the bed thins |
| 6 | 0:50–1:00 | Close | `raymotion 0.9.1` | the end card slides up: the raymotion logo; `raymotion 0.9.1`; `bodies, physics and scripts`; the command `raymotion_script preview hello-script --script scripts/hello.as` typed in a box; `github.com/pebaryan/raymotion/releases`; hold | the command the viewer runs next | whoosh; a soft boom on the logo; the score resolves on D |

### 3b. Reveal map

Text-only: the anchors are moments in the cue map (§9), not spoken words. Code types at
26–38 characters a second (a fast typist); a unit lands, then holds before the next one
starts. One entrance at a time. The "visual lead" of a voiced film becomes the save: the
frame always appears after its save, by the measured time.

| Scene | Unit | Anchor | Visual start | Lands | Hold |
| --- | --- | --- | --- | --- | --- |
| 1 | caption | frame 0 | 0.3 | 0.65 | to 7.6 |
| 1 | `draw` (4 lines) | typing | 0.8 | 3.5 | 0.8 |
| 1 | terminal command | save | 4.3 | 4.55 | to chapter end |
| 1 | preview "hello" | save + 0.53 | 4.83 | 4.83 | to chapter end |
| 1 | chip `0.5 s` | the frame | 4.83 | 5.03 | 2.2 |
| 2 | ZIP header | after the slide | 8.3 | 8.7 | to end |
| 2 | ZIP rows, one at a time | 0.32 s apart | 8.9 | 10.5 | 0.7 |
| 2 | "you need" list | after the rows | 11.2 | 12.0 | 0.4 |
| 2 | strikes 1, 2, 3 | 0.8 s apart | 12.4 | 14.25 | 0.55 each |
| 2 | ticks on "a text editor", "ffmpeg" | after the last strike | 14.6, 14.8 | 15.1 | 1.8 |
| 2 | caption | after the tick | 14.9 | 15.25 | to 16.6 |
| 3 | erase, paste | the cursor | 17.9 | 18.6 | 0.8 |
| 3 | `body` + `addCharacter` | typing | 19.4 | 21.8 | 0.8 |
| 3 | run frame + chip | save 22.6 + 0.58 | 23.18 | 23.4 | 1.0 |
| 3 | `run` → `cheer` | the cursor | 24.1 | 24.96 | 0.25 |
| 3 | cheer frame + chip | save 25.2 + 0.57 | 25.77 | 25.95 | to 28.6 |
| 3 | caption | after the cheer | 26.4 | 26.75 | to 28.6 |
| 4 | erase, paste the cloth | the cursor | 29.4 | 30.1 | 0.6 |
| 4 | wind lines | typing | 30.7 | 34.0 | 0.6 |
| 4 | cloth frame + chip | save 34.6 + 0.56 | 35.16 | 35.3 | 0.3 |
| 4 | `3` → `12` | the cursor | 35.4 | 36.1 | 0.5 |
| 4 | whip frame + chip | save 36.6 + 0.55 | 37.15 | 37.3 | 0.7 |
| 4 | split run 1 / run 2 | after the whip | 38.0 | 38.3 | 0.3 |
| 4 | diff readout | after the split | 38.6 | 38.9 | to 39.9 |
| 4 | caption | with the readout | 38.6 | 38.95 | to 39.7 |
| 5 | global line | typing | 40.5 | 41.1 | 0.3 |
| 5 | compile error | save 41.4 + 0.02 | 41.42 | 41.42 | to chapter end |
| 5 | loop line | typing | 43.7 | 44.35 | 0.25 |
| 5 | abort | save 44.6 + 0.72 | 45.32 | 45.32 | to chapter end |
| 5 | caption | after the abort | 45.8 | 46.15 | to 49.5 |
| 5 | trusted-code footnote | after the caption | 46.7 | 47.1 | to 49.5 |
| 5 | the cloth again | save 46.9 + 0.56 | 47.46 | 47.65 | to 49.6 |
| 6 | logo | after the slide | 50.5 | 51.4 | to end |
| 6 | version, subtitle | after the logo | 52.0 | 52.8 | to end |
| 6 | command | after the version | 53.2 | 54.6 | to end |
| 6 | URL | after the command | 55.4 | 55.8 | to end |

## 4. Script

`Voice: none`: the owner chose a text-only cut. The caption lines are the whole spoken
layer; the code on screen is the demo. One caption line at a time.

| # | Caption text | Reading time at 2.5 words/s | On screen |
| --- | --- | --- | --- |
| 1 | A video is a text file now. | 2.8 s | 0.3–7.6 |
| 2 | Unzip. Write. Render. No compiler. | 2.0 s | 14.9–16.6 |
| 3 | Characters, lights, cameras: from the script. | 2.4 s | 26.4–28.6 |
| 4 | Same frame, every run. | 1.6 s | 38.6–39.7 (with the readout) |
| 5 | No globals. No runaway loops. | 2.0 s | 45.8–49.5 |
| 5b | Scripts are trusted code: run only scripts you would run as a program. | footnote | 46.7–49.5 |
| 6 | raymotion 0.9.1 | — | 52.0–end |

> `Cue map`: see §9. The editor's text, every save, the terminal and the UI sounds read one
> list of operations and one list of saves in the code; nothing is timed twice.

**Same string.** The file's text at any moment is a pure function of time (typing, paste
and erase operations on the opening lines). At each save that text is written to
`out/scripts-09-assets/save-N/film.as`, loaded with `script::registerFile` and grafted into
the preview pane, frozen at frame 60, as `raymotion_script still film --at 60` renders it.
The code on screen and the code that renders are the same string. The compile error is
caught from `registerFile` while building; the loop's abort is caught inside the frame.

## 5. Sources and claims

| Claim / number | Value used | Source | On-screen? | Exact or illustrative? |
| --- | --- | --- | --- | --- |
| Save to frame, per save | medians 0.53, 0.58, 0.57, 0.56, 0.55, 0.56 s (all runs 0.52–0.63 s) | measured 2026-11-05 on the author's PC (Windows 11, MSVC Release build of `raymotion_script` 0.9.1): `raymotion_script still film --at 60 --script save-N/film.as` on the film's own written files, one warm-up then 7 runs each. An earlier session measured 0.49–0.53 s for the same kind of files: runs drift ~10% between sessions | chips, rounded to 0.1 s (`0.5 s`, `0.6 s`); the film's gap uses the median | exact to 0.1 s (one machine) |
| Compile error time | 0.02 s | same session, save 5 | no (the error simply appears) | exact |
| Loop abort time | 0.72 s | same session, save 6 | no | exact |
| Same loop for `hello.as` | 0.49–0.52 s | same session, earlier: the shipped `hello.as`, `still --at 30` | no (supports §2's "about half a second") | exact |
| Native `.cpp` runner loop | 2.92–2.99 s (3.88 s cold) | same session, `raymotion_starter still --script starter.cpp` | no | — |
| Rebuild + still | 3.59–5.99 s | same session, edit `lowerthird.cpp`, incremental build + still | no (the film argues "no compiler", not "faster than a rebuild") | — |
| ZIP | 124,972,325 B; 286 entries | `raymotion-v0.9.1-windows-x64.zip`, listed 2026-11-05 | `125 MB · 286 files` | exact |
| `raymotion_script.exe` | 8,313,856 B | same listing | `8.3 MB` | exact |
| `raymotion.exe` | 11,883,520 B | same listing | `11.9 MB` | exact |
| `scripts/hello.as` | 2,211 B | same listing | `2.2 KB` | exact |
| `assets/` | 275 files, 152,197,493 B | same listing | `275 files` | exact |
| No compiler for `.as` | — | `docs/scripting.md`: AngelScript compiles in memory; the ZIP has no toolchain | caption, strikes | exact |
| ffmpeg for MP4 | — | the ZIP's `README.txt`: "ffmpeg on PATH, for MP4 output" (also an OpenGL 3.3 driver and the VC++ runtime, not shown) | the fifth "you need" row, ticked | exact |
| Global variable error | `film.as:5:1: Global variables have been disabled by the application` | caught from the compiler while the film builds; the CLI prints the same line | terminal, path shortened to `film.as` | exact |
| Loop abort | `film.as: script execution aborted (statement budget exceeded)` | caught from the run while the film renders; the CLI prints the same | terminal | exact |
| Same frame every run | 0 of 921,600 pixels differ | two `still --at 60` runs of save 4, compared pixel by pixel (PIL), 2026-11-05 | readout | exact |
| Scripts are trusted | — | `docs/scripting.md` | footnote | exact |

Not claimed: speed versus other tools, "instant", "live reload" (0.9 has none: a save is
followed by a run), timings on other machines, any feature the shipped `.as` bindings lack.

## 6. Visual system

| Element | Decision |
| --- | --- |
| Palette | paper `#0d1017`; panels `#151a24`; ink `#e9edf5`; **teal `#47dad8` = your edit** (cursor, typed/pasted text highlight, selection, the unsaved dot, the editor tick); **amber `#f0ad52` = time** (the save → frame chip, the diff readout, the progress line); **coral `#ff6b5a` = an error**, only in chapter 5's terminal; dim `#7c8597` for outputs, strikes, footnotes. Syntax colours are muted (lavender keywords, soft green strings, sand numbers), never teal, amber or coral. |
| Type | captions Inter Bold 46; labels Inter 24–38; code JetBrains Mono 25 (line height 1.26); terminal JetBrains Mono 20; footnote Inter 22 |
| CJK | n/a |
| Margins / safe area | 72 px sides, 88 px top; nothing but the caption and footnote below y 912 |
| Caption box | one line centred at y 996; the footnote at y 1046 |
| HUD / persistent frame | editor x 72–1060, y 88–912 (tab `film.as`); preview x 1108–1848, y 88–504 (16:9, the script's 1280x720 at 0.578); terminal x 1108–1848, y 532–912; a 3 px amber progress line at the top |
| Motion grammar | entrances 0.3–0.4 s ease-out cubic; typing 26–38 chars/s; full-screen chapters slide in and out over 0.4 s; the preview cuts to each new frame; no idle motion except the caret blink |
| Motif | the amber `save → frame` chip after every save that renders |
| Charts / maps | the diff readout only |
| Text performance | captions fade in whole, one at a time; code types character by character, a new line opening first |
| Brand kit | `icons::logo` on the end card; version, release name, command, URL |

### 6b. Metaphor map

| Claim | Visual metaphor | Mapping | Animated change |
| --- | --- | --- | --- |
| "save to frame in about 0.5 s" | the chip, and the gap itself | the frame appears in the film exactly the measured time after the save | the chip lands with the frame, never before |
| "no compiler" | a needs list | rows = what a C++ project needs | three rows struck through one at a time; `a text editor` and `ffmpeg` ticked |
| "the script drives the engine" | the preview pane | left = the source, right = its output | one word (`run` → `cheer`) changes; the output changes with it |
| "frames are pure" | two runs side by side | left = run 1, right = run 2 of frame 60 | the pane splits; the readout lands on 0 |
| "guard rails" | the terminal | one coral line per error, with file:line:col | the bad line is typed, saved, rejected, erased |

## 7. Audio plan

| Element | Decision |
| --- | --- |
| Voice | none |
| Score | `renderSong`: D–Bm–G–A pad, two seconds a chord, with a plucked root pulse; louder in chapter 2, pad only (quieter) in chapter 5, a held D chord from the logo |
| Ducking | n/a (no voice) |
| UI sounds | synthesised from the same operations and saves into `ui.wav`: a click per typed character (seeded), a noise swish per paste, a low blip per erase, two notes per save, a high blip when a frame lands |
| SFX | `whoosh.wav` on the three full-screen slides; `pop.wav` per strike; `chime.wav` on the tick and the diff readout; `thud.wav` per error; `boom.wav` on the logo |
| Silence | none intended; the bed is lowest under chapter 5 |
| Loudness target | -16 LUFS integrated, true peak ≤ -1 dBTP |

## 7b. Scenario coherence

| Quantity | Value | Fits / depends on | Source or assumption |
| --- | --- | --- | --- |
| Save-to-frame times | 0.53–0.58 s (medians) | the very files the film shows, `still --at 60` | measured (§5) |
| ZIP sizes | 8.3 MB, 11.9 MB, 2.2 KB, 275 files | v0.9.1 ZIP | listed (§5) |
| Version on the end card | 0.9.1 | the release the command and ZIP come from | GitHub release |
| Preview content | the real output of the on-screen script, frame 60 | the same string is shown and rendered | by construction |
| Terminal output file | `out/film_still_0060.png` | `c.name = "film"`, `--at 60` | the CLI's naming |

## 8. Asset plan

| Asset | Generated / committed | Where | Fallback if missing |
| --- | --- | --- | --- |
| The saved scripts | generated from the operation list | `out/scripts-09-assets/save-N/film.as` (through `c.assets`) | rebuilt every render; no committed copy to drift |
| Score, UI sounds | generated | `out/scripts-09-assets/score.wav`, `ui.wav` | the film renders silent |
| SFX | committed | `assets/sfx/*.wav` | silent hits |
| SRT | generated from the captions | `out/scripts-09.srt` | — |
| Fonts | committed | `assets/fonts` | bundled |

**Clean-machine test:** `list` reads only names and lengths, so it works from any
directory. Rendering needs the AngelScript backend: with `RAYMOTION_SCRIPTING=OFF` the build
throws a clear error naming the missing backend (the film has nothing to show without it).

## 9. Cue map

All cues live in two lists in the code (operations, saves); this table is their plan
(seconds; frames at 30 fps).

| Time | Picture event | Sound event | Source of truth |
| --- | --- | --- | --- |
| 0.0 | editor with the four configure lines, cursor blinking | bed starts | `START` |
| 0.8–3.5 | `draw` types | clicks | op 1 |
| 4.3 / 4.83 | save; hello frame + chip | save blip; frame blip | save 0 |
| 7.6–8.0 | ZIP slide covers the editor | whoosh | `cover()` |
| 12.4, 13.2, 14.0 | strikes | pop × 3 | `drawZip` |
| 14.6 | tick | chime | `drawZip` |
| 16.6–17.0 | slide out to the editor | whoosh | `cover()` |
| 17.9 / 18.6 / 19.4 | erase; paste; type | blip; swish; clicks | ops 2–4 |
| 22.6 / 23.18 | save; run frame | blips | save 1 |
| 24.1–24.96 | `run` → `cheer` | blip; clicks | ops 5–6 |
| 25.2 / 25.77 | save; cheer frame | blips | save 2 |
| 29.4 / 30.1 / 30.7 | erase; paste the cloth; type the wind | blip; swish; clicks | ops 7–9 |
| 34.6 / 35.16 | save; cloth frame | blips | save 3 |
| 35.4–36.1 | `3` → `12` | blip; clicks | ops 10–11 |
| 36.6 / 37.15 | save; whip frame | blips | save 4 |
| 38.0 / 38.6 | split; readout | chime | `SPLIT` |
| 40.5 / 41.4 / 41.42 | global typed; save; error | clicks; blip; thud | op 12, save 5 |
| 42.9 | erased | blip | op 13 |
| 43.7 / 44.6 / 45.32 | loop typed; save; abort | clicks; blip; thud | op 14, save 6 |
| 46.1 / 46.9 / 47.46 | erased; save; the cloth again | blips | op 15, save 7 |
| 49.6–50.0 | end card slides up | whoosh | `cover()` |
| 50.5 | logo | boom | `drawEnd` |
| 60.0 | end | bed fades over the last 1.5 s | `c.duration` |

## 10. Still frames to review

| Frame | Chapter | What must be true |
| --- | --- | --- |
| 0 | 1 | the editor with four dim lines, cursor, caption; preview black; terminal empty |
| 150 | 1 | "hello" in the preview; code readable; chip in |
| 300 | 2 | ZIP rows; sizes match §5 |
| 430 | 2 | three strikes, `a text editor` and `ffmpeg` ticked, caption |
| 700 | 3 | the character mid-stride, whole body in the pane, code legible |
| 800 | 3 | the character cheering; `cheer` in the code |
| 1000 | 4 | the wind lines typing; the old frame still in the preview |
| 1060 | 4 | the cloth frame with its chip |
| 1160 | 4 | run 1 = run 2; the readout |
| 1260 | 5 | the compile error in coral with `film.as:5:1`; "no new frame" |
| 1400 | 5 | the abort line; caption and footnote |
| 1650 | 6 | logo, version, command, URL, all in the safe area |

Plus one still mid-transition at 234, 504 and 1494, and frames from the encoded MP4.

## 10b. Scene review ledger

| Scene | Review asset | Reveal / sync notes | Approved? | Re-render |
| --- | --- | --- | --- | --- |
| 1–6 | see `docs/scripts-09-qc.md` | | | |

## 11. Acceptance checks

- [ ] Muted (the film is text-only), the argument reads from the images: edit, frame, no compiler.
- [ ] The preview shows the real output of the code on screen (same string written and loaded).
- [ ] Every number matches §5.
- [ ] Every scene starts sparse; one entrance at a time; holds as in §3b.
- [ ] The code is legible at 1080p and at 1280x720.
- [ ] Nothing below y 912 but the caption and footnote.
- [ ] No save lands inside an edit (asserted in the code).
- [ ] `list` works from another directory.
- [ ] -16 LUFS ± 1, true peak ≤ -1 dBTP; ffprobe, full decode, encoded frames.
- [ ] Gate 2c sweep: every flag fixed or explained; every sweep frame described.
- [ ] Thumbnail readable at 320x180; SRT written.
- [ ] Authoredness ≥ 30 and Craft ≥ 16 for a public cut, with the owner's review.

## 12. Risks / non-goals

- **Risks:** the frame times are one machine's; the chips say what was measured, and the
  QC records the machine. The 3D preview renders in every frame of chapters 3–5 (and the
  loop runs to its budget in every frame after its save), so the render is slower than a
  2D film. Code at 25 px must survive H.264 (check encoded frames).
- **Non-goals:** the other 0.9 features (crowds, water, voxels, splats, 2D-in-3D) get no
  chapter: this film is about scripting. The native `.cpp` runner is not shown (it needs a
  source build, which contradicts "no compiler" for this audience).
- **Out of scope:** live reload (0.9 has none; the film shows save, then run).

## 13. Approval

| Gate | Reviewer | Date | Result |
| --- | --- | --- | --- |
| Brief | pebaryan | | waiting |
| Script / cue map | pebaryan | | |
| Stills | | | |
| Staged reveal / sync | | | |
| Draft | | | |
| Final QC | | | |
