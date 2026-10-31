# Native C++ runtime helper

The runtime helper compiles one composition translation unit on demand against an
already-built raymotion engine. It is a build-tree workflow for source builds, not an
embedded C++ interpreter or a packaged-app feature.

Build raymotion with its showcase and engine targets, then use a source that defines
`void registerComps(rmtn::Registry&)`:

```sh
cmake -S . -B build -DRAYMOTION_RAYLIB=vendored
cmake --build build --config Release --target raymotion_starter
build/raymotion_starter list --script examples/starter/main.cpp
build/raymotion_starter preview hello --script examples/starter/main.cpp
build/raymotion_starter render promo --script examples/starter/main.cpp
```

The helper supplies the program entry point and links the source against the existing
engine archive. `--cpp-entry path::to::registerMovie` selects a different registry
function. It accepts one `.cpp`, `.cc` or `.cxx` source plus header-only files; a normal
CMake project is required for multiple translation units or external link libraries.
The same source can also be built directly into an ordinary raymotion executable.

Using the helper requires CMake, the matching C++ compiler and the original build-tree
SDK. Generated executables are cached below `build/cpp-cache/<configuration>/` and
rebuilt when the composition, included headers or linked engine changes. Runtime
preview, render, props and still commands use the normal CLI and renderer. `--jobs`
workers run the already-compiled scene without compiling it again.

Packaged Windows installs do not contain the build-tree SDK or C++ toolchain. Running
`--script file.cpp` from a package therefore stops with a clear message that native
C++ compositions are unavailable in packaged builds; build from source to use this
helper. Package README files state this limitation.
