# Raymotion Studio

An editor for raymotion compositions with an agent in it, built on
[CUI](https://github.com/yappologistic/CUI) (MIT, C99, immediate mode). It is a separate
executable, `raymotion_studio`, and is off by default because configuring it fetches CUI.

```sh
cmake -S . -B build -DRAYMOTION_STUDIO=ON
cmake --build build --target raymotion_studio
build/raymotion_studio --workspace .
build/raymotion_studio examples/scripts/hello.as     # open a script straight away
```

On the machine-local MSVC setup, `opengl32`, `imm32`, `dwmapi`, `shcore` and `winhttp` import libs must be in
`third_party/msvc-sdk-libs` (copy them from the Windows Kit, as for the other libs).

## The loop

1. The Code panel opens on the **example gallery**: every `examples/scripts/*.as`, with a
   picture, grouped by category and searchable. Click one, or **New script** (Ctrl N) for a
   blank composition in `studio-projects/`.
2. Edit. **Ctrl S** saves, compiles and refreshes the Preview. Nothing else needs pressing.
3. Compile and runtime errors land in **Problems** under the editor, and the lines are tinted.
   Double-click a problem to go to its line.
4. In the Preview, drag the scrubber or step with Left / Right (Shift: one second). **Space**
   plays the film in the panel.

The Studio reopens the script, brief, frame and panel layout you had last time.

## Panels

| Panel | What it does |
| --- | --- |
| Code | The gallery, then the editor. Below it: Problems and the host's raw Output. The grid button (Ctrl E) goes back to the gallery. A shipped example shows a "Save a copy" banner, so the repository's copy stays as it is. |
| Preview | The frame under the scrubber, rendered at full size whenever the script is saved or the frame changes (switch **Auto** off to render only on request, Ctrl R). Zoom with the wheel; double-click fits. Play uses a silent half-size draft of the whole film, rendered once per save; while it renders, playback and scrubbing use the frames that exist. The arrow button opens the normal raymotion window, with sound (F5). |
| Brief | `docs/*-brief.md`: read or edit. New copies `skills/raymotion/templates/BRIEF.md`. |
| Assets | A filterable list of `assets/`, image previews, copy the path as scripts write it, "Mention in chat". |
| Chat | The agent. Tool calls show as rows you can open. Its suggestions follow what is open (for example "Fix the problems" when the script does not compile). |

The status bar shows the workspace, the open file and caret, draft and video progress, the
check result (click it to see the problems) and the agent's model.

## Menus and shortcuts

- **File**: New script (Ctrl N), New brief, Browse examples (Ctrl E), Save (Ctrl S), Save a copy,
  Open workspace, Rescan files, Settings (Ctrl ,), Quit.
- **Run**: Check script, Render this frame (Ctrl R), Play draft (Space), Live window (F5),
  Render video to `out/`.
- **Agent**: Stop, New conversation, Load models.
- **View**: Command palette (Ctrl K: every command, script and brief), panels, Reset layout, theme.

Settings is a dialog: base URL, API key, model with **Load models**, workspace, renderer
path, theme. It says whether the renderer was found.

## Example headers

The gallery reads the first lines of each example, and `tests/test_script.py` checks that every
file in `examples/scripts/` has them and compiles:

```
// Title -- One sentence about what it shows.
// Category: Charts
// Still: 45          (optional: the frame for the picture; default is 60% in)
```

Pictures are rendered in the background on first use and cached in `out/studio/thumbs/`.

## How it fits together

- The studio does not link the renderer. Checks, stills, draft frames, the video and the live
  window are `raymotion_script` child processes, so a crashing script cannot take the editor
  down. The host is found next to the studio, then in `build/`, or set it in Settings.
- Stills go to `out/studio/frames/` and are dropped when the script changes. The draft is the
  host's own `render --draft --png-frames`, so it lands in `out/<name>_draft_frames/` and
  replaces what was there.
- The agent speaks OpenAI-style `/chat/completions` with tools, through WinHTTP on Windows (system proxy and
  certificate store, no child process, nothing written to disk, so the key stays in memory) and `curl` elsewhere.
  Any compatible server works, including local ones. Settings has **Load models**, which calls
  `GET {base URL}/models` and fills a searchable picker; typing a name still works.
- Agent tools: `read_file`, `write_file`, `list_files`, `check_script`, `render_still`. Paths are
  confined to the workspace; writes cannot touch `.git`, `third_party` or `build*`.
- The system prompt includes `AGENTS.md`, so the agent is told to write the brief first and to verify.
  That is guidance to the model, not enforcement: the QC steps in `AGENTS.md` still need a person.
- Files the agent writes reload into the editors unless you have unsaved edits. A still it
  renders moves the scrubber to that frame.

Headless: `raymotion_studio --workspace . --agent "prompt"` runs one agent turn and prints events
(`--url`, `--model`, `--key` override the settings). `--list-models` prints the server's model ids.

For screenshots and UI checks: `CUI_SCREENSHOT=shot.png CUI_SCREENSHOT_FRAMES=300` saves the
window after that many frames and exits; `--hidden` keeps the window off screen, `--fresh`
ignores and does not write the session, and `--click FRAME:X:Y` (repeatable) clicks a point.

## Known limits

- Closing the window with the title bar's x does not ask about unsaved edits. File > Quit and
  switching scripts do.
- Draft playback has no sound and is half size; the live window has both, and it reloads
  on save like the Preview panel (the host watches the script; see `docs/scripting.md`).
- A long film's draft takes about as long to render as the film runs, and its frames stay in `out/`.
- The API key is plain text in the settings file. Prefer `OPENAI_API_KEY`.
- Stop takes effect between requests and tool calls; an in-flight HTTP call finishes first.
- Replies are not streamed; the typing indicator shows until the full reply arrives.
- Windows only so far: the POSIX branches (process code, `curl` fallback) are unbuilt. The HTTPS path is untested against a real server (only http://127.0.0.1 mocks).
- `CUI_RENDERER=software` forces CUI's CPU renderer if OpenGL misbehaves.
