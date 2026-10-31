# Runtime compositions

## Scope

The `.as` backend is for quick 2D motion graphics: titles, lower thirds, simple cards
and compact overlays authored without a native compiler. It is not a replacement for
the full C++ authoring API. Use a normal CMake project and the `raymotion::engine`
library for 3D, audio, media timelines, complex layers, assets or compositions that
need native engine features. The native C++ compile-on-demand helper is maintained
separately from this embedded scripting feature.

## Embedded scripts

Build the host once, then edit and run scripts without rebuilding raymotion.
`examples/scripts/hello.as` is a complete example (two overlapping scenes, props, easing
and seeded randomness); release ZIPs include it as `scripts/hello.as`:

```sh
build/raymotion_script preview hello-script --script examples/scripts/hello.as
```

For your own script:

```sh
cmake -S . -B build -DRAYMOTION_BUILD_EXAMPLES=OFF
cmake --build build --config Release --target raymotion_script
build/raymotion_script list --script path/to/scene.as
build/raymotion_script preview my-scene --script path/to/scene.as
build/raymotion_script still my-scene --script path/to/scene.as --at 30
build/raymotion_script render my-scene --script path/to/scene.as --jobs 2
```

On multi-configuration Windows builds use `build/Release/raymotion_script.exe`.
The showcase and other applications using `runCli` accept the same `--script` option.
Repeat the option to load multiple files; composition names must be unique.
Relative paths resolve from the current working directory, including worker processes.
Each invocation reads the current source. Restart preview after an edit; live reload
inside an open window is not implemented.

The first backend uses [AngelScript 2.38.0](https://www.angelcode.com/angelscript/),
an embedded, statically typed language with C++-like syntax. Scripts compile to VM
bytecode in memory; no external compiler, linker or DLL is needed at runtime.
This is a supported C++-like subset API, not an interpreter for arbitrary existing
C++ files. Native compositions keep the complete C++ API.

## Script contract

Each `.as` file defines `void configure(Composition &inout c)` and either
`void draw(int frame)` or named scene functions registered by
`scene(start, duration, "functionName")` in `configure`.

`Composition` exposes `string name`, `int w`, `h`, `fps`, `duration`, `supersample`,
`statementBudget`, and `uint background` (RGB hex, e.g. `0x101820`). Set a name and
positive duration.
Names allow letters, digits, hyphens and underscores. Supersample is 1, 2 or 4.
Scenes must lie within the composition duration. Overlaps draw in registration
order. A scene receives its **local frame**: frame 0 at its start. If no scenes are
registered, `draw` spans the whole composition.

`configure` is CPU only, runs during registration, and runs again before use with
input props. It must produce stable names. Drawing from `configure` raises a script
exception. `list` needs no graphics context and creates no assets or output files.

Scripts support typed locals, expressions, conditionals, loops, functions, strings,
enums, and AngelScript's local objects. There are no C++ includes, macros, STL
templates, raw pointers, or automatic access to native engine headers. Global
variables (including global constants) are disabled so frames cannot accumulate
script state. Use local constants, helper functions or enums instead.

## Initial bindings

All coordinates and lengths are in composition pixels; colors are `uint` RGB hex.
Drawing respects the native node context. Optional alpha defaults to 1.

```cpp
draw::rect(x, y, w, h, color, alpha = 1);
draw::roundRect(x, y, w, h, radius, color, alpha = 1);
draw::circle(x, y, radius, color, alpha = 1);
draw::line(x1, y1, x2, y2, width, color, alpha = 1);
draw::gradient(x, y, w, h, topLeft, topRight, bottomRight, bottomLeft, alpha = 1);
draw::fillPoly(array<float> points, color, alpha = 1); // alternating x,y values
draw::image(file, x, y, w, h, alpha = 1);             // uses media::resolve search paths
draw::push(x = 0, y = 0, rotation = 0, scale = 1, alpha = 1);
draw::pop();                                           // must match a push
draw::text(text, x, y, size, color, alpha = 1, anchorX = 0, anchorY = 0,
           face = FONT_UI);
```

`draw::push` nests translation, rotation, uniform scale and opacity for following
calls; `draw::pop` restores the previous transform. Polygons accept 3–1024 vertices.
Images load through the engine's image resolver on first draw. Font choices are
`draw::FONT_UI`, `draw::FONT_UI_BOLD`, `draw::FONT_MONO` and
`draw::FONT_MONO_BOLD`; fonts load inside the drawing callback. Anchors range from
0 (left/top) to 1 (right/bottom).

`examples/scripts/lower_third.as` is a complete composition using transforms, a four
corner gradient, an SVG image, a filled polygon, and UI/monospace font choices.
Its source art is `assets/images/script-lower-third.svg`.

Props are accessible in configure and drawing:

```cpp
props::number("width", 1280);  // double
props::text("title", "Hello"); // string
props::flag("visible", true);  // bool
```

Pass them with `--props '{"title":"New title"}'` or `--props @props.json`.
Missing keys use the supplied fallback; wrong types produce a diagnostic.
Script defaults currently live in these fallback arguments; `props` prints `null`.

Math includes the standard AngelScript math addon (`sin`, `cos`, `sqrt`, `pow`,
etc.), plus `clamp01(t)`, `ease(kind, t)`, `rnd01(seed, index)` and
`rrange(seed, index, low, high)`. Easing constants match native `E_LINEAR` through
`E_BOUNCE`. Use seeded randomness and absolute expressions of the frame.

The bindings cover simple 2D drawing and scene timing. Native 3D, media timelines,
layers, audio, asset-generation hooks and the wider native object API are not bound.
Existing render options such as supersampling, PNG frames, alpha, formats and worker
jobs still operate through the normal renderer.

Source errors report the filename, line and column. Runtime exceptions report the
function and line. Each callback defaults to a one-million statement-callback budget.
Set `c.statementBudget` to a value from 1 through 100,000,000 for heavier drawing
loops. Runaway callbacks abort with a diagnostic. This is a responsiveness guard,
not a security sandbox or a memory quota. Programs are used on the renderer thread,
not concurrently.

## Additional languages

`include/rmtn/script.hpp` is independent of AngelScript. A backend implements:

- `Program::configure(props) -> Description`: metadata and named scenes, without GL.
- `Program::draw(function, ctx, localFrame, props, statementBudget)`: drawing inside
  the native renderer with per-composition immutable props and a bounded callback budget.
- A loader returning `shared_ptr<Program>` for a source path.

Register it once before `runCli` with
`script::registerBackend(".lua", loader)` (or `.py`, etc.). The registry handles
validation, props rebuilds, timeline nodes and callback ownership. The same
`--script` CLI works for every registered extension, including worker processes
when the executable registers that backend at startup. Backends should enforce
deterministic frames and translate interpreter failures into C++ exceptions.
VM state must remain alive until all native callbacks have been destroyed.

Lua, Python and ActionScript backends are extension points, not shipped runtimes.
Each will need its own language bindings and dependency. In particular, ActionScript
would need an appropriate runtime; it is a different language from AngelScript.

`RAYMOTION_SCRIPTING=OFF` omits the vendored AngelScript backend while retaining the
backend interface, host and CLI. AngelScript source is vendored under
`third_party/angelscript`, so configuring scripting does not fetch a new dependency
from the network. raylib can still use its normal vendored, system or populated
FetchContent mirror.
