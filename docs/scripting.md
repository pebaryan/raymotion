# Runtime compositions

## Native C++: the same authoring code

Use `.cpp`, `.cc` or `.cxx` when source compatibility is the priority. The runtime
command compiles only your composition translation unit and a small CLI entry point,
links them against the **already-built engine**, and runs the result. It does not
rebuild the engine or showcase application. The compiled runner is cached; subsequent
invocations reuse it until the source, an included header, or a linked library changes.

The existing starter works **without changing any source code**:

```sh
cmake --build build --config Release --target raymotion_script
build/raymotion_script list --script examples/starter/main.cpp
build/raymotion_script preview hello --script examples/starter/main.cpp
build/raymotion_script render promo --script examples/starter/main.cpp --props @props.json
```

This uses full C++17 and the full native engine API: `Comp`, `Registry`, `Ctx`,
`NodeProps`, lambda captures, STL, tracks, springs, layers, transitions, 3D, media,
audio and asset hooks. There is no second drawing API to learn. The same source can
still be compiled directly into your normal C++ executable.

By default the file must define `void registerComps(rmtn::Registry&)` (it may be
`static`). For another name, supply `--cpp-entry myNamespace::registerMovie`.
The runner supplies `main`; an existing `main` in the source is renamed and ignored.
Keep composition initialization in the registration function and hooks, rather than
depending on custom logic in your original `main`.

The native path requires CMake, the original matching C++ toolchain, source headers
and build-tree SDK. It is a **compile-on-demand workflow**, not an embedded C++
interpreter. Compiler diagnostics are reported when compilation fails. Build the
engine once before using it; the runner imports its archives rather than rebuilding
them. Generated cache files live under `build/cpp-cache/<configuration>/`.

The current native command accepts one source translation unit, plus any header-only
helpers it includes. Additional separately compiled sources and third-party link
dependencies belong in a normal CMake project. It runs the compositions registered
by that file, rather than merging in the host's compiled-in showcase registry. You
can still pass additional `.as` files to the generated runner.

Close preview before recompiling the same source, then rerun the command to pick up
edits. Concurrent builds of the same cached runner are serialized; parallel frame
workers use the compiled runner directly. Native C++ is available even with
`RAYMOTION_SCRIPTING=OFF`; this switch controls only the optional AngelScript backend.
Release ZIPs containing only executables/assets do not contain the development SDK.

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
and `uint background` (RGB hex, e.g. `0x101820`). Set a name and positive duration.
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
draw::text(text, x, y, size, color, alpha = 1, anchorX = 0, anchorY = 0);
```

Text uses the engine's UI font, loaded only during drawing. Anchors range from
0 (left/top) to 1 (right/bottom).

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

The initial bindings cover 2D drawing and scene timing. Native 3D, media, layers,
assets, audio and the wider native object API are not yet bound. Existing render
options such as supersampling, PNG frames, alpha, formats and worker jobs still
operate through the normal renderer.

Source errors report the filename, line and column. Runtime exceptions report the
function and line. Each callback has a one-million statement-callback budget;
infinite loops abort with an error. This is a responsiveness guard, not a security
sandbox or a memory quota. Programs are used on the renderer thread, not concurrently.

## Additional languages

`include/rmtn/script.hpp` is independent of AngelScript. A backend implements:

- `Program::configure(props) -> Description`: metadata and named scenes, without GL.
- `Program::draw(function, ctx, localFrame)`: drawing inside the native renderer.
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

`RAYMOTION_SCRIPTING=OFF` omits the AngelScript dependency and its download while
retaining the backend interface, host and CLI. The build fetches a checksum-pinned
SDK once; offline builds can set `FETCHCONTENT_SOURCE_DIR_ANGELSCRIPT` to an
extracted SDK directory containing `angelscript/` and `add_on/`.
