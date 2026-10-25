---
name: raymotion
description: Make videos with raymotion, a C++17 motion-graphics engine on raylib and ffmpeg (Remotion-style compositions as code). Use when asked to create, edit, preview or render a video, animation, explainer, title sequence or social clip with raymotion, or to animate diagrams (Mermaid, Graphviz DOT, BPMN, knowledge graphs, trees), molecules (SMILES, SDF) or proteins (PDB), particle effects (fireworks, confetti, rain, snow, fire), or 3D scenes in code.
---

# raymotion

A raymotion video is a C++ function of the frame number. A project registers
compositions (`Comp`) and hands them to the CLI, which previews them, writes single
frames as PNG, or renders an MP4 through ffmpeg. Frames carry no state, so frame N
always looks the same, whether rendered alone or as part of the whole video.

Repository: https://github.com/pebaryan/raymotion (API docs in its README.md and in the
comments of `include/rmtn/*.hpp`).

## Workflow

1. **Set up a project** (once). Copy `templates/CMakeLists.txt` and `templates/main.cpp`
   from this skill into an empty folder, then build:
   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
   It fetches raymotion (and raylib) with CMake's FetchContent. Needs CMake 3.21+, a
   C++17 compiler, and ffmpeg on `PATH` for MP4 output. On Linux install the X11/GL
   development packages raylib needs. The executable may land in `build/` or
   `build/Release/` depending on the generator.
   Inside a raymotion checkout, add compositions to `examples/showcase/` instead (one
   `.cpp` with a `registerX(Registry&)` function, declared and called in `scenes.cpp`,
   and listed in the `raymotion` target in `CMakeLists.txt`).

2. **Write the composition** in `main.cpp` (see "Writing compositions" below).

3. **Look before rendering.** Build, then write stills at the frames that matter: the
   start and middle of each scene, and the frames in the middle of transitions.
   ```sh
   build/myvideo list
   build/myvideo still intro --at 45 --png out/check-045.png
   ```
   Open each PNG and check it: text inside the frame and not overlapping, nothing
   cut off, elements where the script says they should be at that moment. Fix and
   repeat. Stills are cheap (well under a second each); full renders are not.

4. **Render.** `build/myvideo render intro --draft` (half size, fast) to check motion and
   timing, then `build/myvideo render intro` for the final `out/intro.mp4`. Use
   `--png-frames` to get a PNG sequence when ffmpeg is unavailable.

Other commands: `preview <name>` plays in a live window with sound (needs a desktop),
`props <name>` prints a composition's default props, `--props '{"title": "Hi"}'` or
`--props @file.json` overrides them, `--ss 1|2|4` sets supersampling, `--mute` drops audio.

## Writing compositions

```cpp
#include "rmtn/raymotion.hpp"
using namespace rmtn;

static void registerComps(Registry& out) {
    Comp c;
    c.name = "intro"; c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 150;
    c.background = mkColor(0x101420);
    TNode* title = addRoot(c.roots, 0, 150);         // starts at frame 0, lasts 150 frames
    title->props.pos = { 960, 540 };
    title->up = [](int t, NodeProps& p) {            // animate transform from the local frame
        p.opacity = (float)interpolate(t, { 0, 20 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        p.scale = (float)spring(t, { 30.0 });
    };
    title->fn = [](const Ctx& cx, int t) {           // draw at the local frame
        dr::text(fonts::ui(true), cx, "Hello", { 0, 0 }, 120, 0, WHITE, 0.5, 0.5);
    };
    out.push_back(std::move(c));
}
int main(int argc, const char** argv) { return runCli(argc, argv, registerComps); }
```

- **The tree:** each `TNode` has `start` and `dur` in frames relative to its parent,
  `props` (pos, scale, rot in degrees, opacity), `up` to animate them and `fn` to draw.
  Children (`node->add(offset, duration)`) inherit the transform. `t` is always the
  node's local frame, starting at 0.
- **Drawing:** use `dr::` (rect, roundRect, circle, ring, arc, sector, star, blob,
  fillPoly, strokeSeg, polyStroke, text) with the `Ctx` you are given. Coordinates are
  relative to the node's origin, in composition pixels; the ctx applies position,
  scale, rotation and opacity for you. `dr::textBox` wraps text into a rectangle.
- **Scenes:** `TransitionSeries ts(c.roots, W, H); ts.add(90); ts.transition(Transition::fade(20)); ts.add(120);`
  then `c.duration = ts.end();`. `Series` places sequences back to back without transitions.
- **Timing in seconds:** multiply by `c.fps`. Leave each scene time to be read: about
  1 s per short line of text, plus the time its entrance animation takes.

See the references for everything else:

- `references/authoring.md`: animation (interpolate, springs, easings, keyframe
  tracks), text and layout, layers (opacity, blend modes, masks, filters), transitions,
  particles (bursts, streams, weather, fire, confetti, custom looks), images, video,
  audio and the procedural synth, props, prefabs, generated assets, deterministic randomness.
- `references/3d.md`: GPU 3D scenes (geometry, models, materials, lights, shadows,
  environments, cameras, depth of field) and illustration styles (toon, Gooch, hatch,
  ink lines, watercolour and halftone filters).
- `references/diagrams.md`: diagrams from Mermaid, DOT, BPMN, Turtle/JSON-LD, JSON and
  Newick, with build, trace, flow, focus, dim and morph animations; molecules
  from SMILES/SDF and proteins from PDB, in 2D and 3D.

## Rules

- **Pure frames.** Compute everything in `up` and `fn` from `t` (and captured
  constants). Never keep state that changes between calls (counters, accumulated
  positions, `static` variables updated per frame): frames are drawn out of order and
  more than once. For randomness use `rnd01(seed, index)` / `rrange(seed, index, a, b)`,
  never `rand()`.
- **`up` assigns, never accumulates.** `up` writes into the node's own props, which keep
  their values between frames. Set each property it touches from `t` every time
  (`p.pos.x = 100 + 5 * t`), never `p.pos.x += 5`.
- **GL only inside drawing.** Fonts (`fonts::ui`, `fonts::load`) and textures need the
  window, which exists only while frames are drawn. Call them inside `fn`, not while
  registering. Building scenes, diagrams and molecules at registration is fine; they
  load lazily.
- **Captures by value.** Lambdas outlive the registration function: capture values or
  `shared_ptr`s, never references to locals.
- **Files.** Paths are resolved from the working directory, then registered search
  paths and the `assets/` folders. Files the composition generates (textures, music)
  are written in `c.assets`, which runs only before frames are drawn.
- **Check what you made.** Always look at stills before claiming a video is done, and
  report what you checked. Render the full video only once the stills look right.
