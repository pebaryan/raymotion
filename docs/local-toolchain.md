# Building on the original development machine

A standard toolchain (Visual Studio, GCC, Clang) needs none of this; see the README.
These notes cover the machine raymotion started on: a stripped Visual Studio 2022 install
with no Windows SDK import libraries and no `rc`/`mt` tools.

```sh
sh tools/configure.sh          # cmake + ninja -> build/raymotion.exe
```

`tools/configure.sh` pins the MSVC compiler, llvm `mt` and the `rc` shim. Set `MSVC_DIR`
(and `LLVM_MT`) to point at another toolset. CMake loads `cmake/LocalMsvcToolchain.cmake`
automatically when `third_party/msvc-sdk-libs/kernel32.lib` exists.

## Why the extra vendoring under third_party/

This install keeps its C runtime headers and import libraries inside **Windows Kits**
(`C:\Program Files (x86)\Windows Kits\10\...`) rather than merged into the VC-tools
trees, and those directory names contain spaces which poison raw `link.exe` command
lines. So the build uses space-free mirrors:

| mirror | source | purpose |
|---|---|---|
| `third_party/msvc-sdk-libs` | Windows Kits `Lib\10.0.22621.0\um\x64` real import libs (kernel32, user32, gdi32, winmm, opengl32, glu32, ...) | system DLL imports |
| `third_party/msvc-crt` | Kits `Lib\...\ucrt\x64` + VS `lib\x64` selected archives | UCRT import archive (`ucrt.lib`) |
| `third_party/vs-libs` | VS `VC/Tools/MSVC/<v>/lib/x64` (all *.lib) | DLL-runtime glue (`msvcrt`, `vcruntime`, `msvcprt`, `oldnames`, delayimp) |
| `third_party/c-headers` | hand-written | only used if the Kits include dir is absent |

The three Microsoft mirrors are machine-local and not redistributable; `.gitignore`
keeps them out of version control. The Kits `ucrt`, `um` and `shared` include dirs are
added with `/I` (the last two are needed when raylib builds from source).

Import-lib gotchas learned here (kept in `tools/mkimports.py` for regeneration from raw
DLL exports): bare-name def rows parse fine but mixing alias+plain forms in one archive
makes `LNK1127: library is corrupt`; hand-rolled aliases silently mismatch when the
real Windows-Kits archives exist — prefer copying the real `.LIB` files (size checks
caught a bogus hand-built `winmm.lib` that broke raylib's `timeBeginPeriod`).

Everything links one CRT model, the DLL runtime (`/MD`: msvcrt + vcruntime + ucrt +
msvcprt), because the prebuilt `raylib.lib` is `/MD`. Don't add `libcmt`/`libvcruntime`
(static) or `ucrtd` (debug) to the link line.

`tools/bin/rc.exe` is a compiled shim (source `tools/shim_rc.c`): `link.exe` spawns
`rc.exe` for its manifest step and llvm-rc can't digest the emitted source, so the shim
writes an empty COFF object (`/MANIFEST:NO` is passed too). `tools/bin/mt.exe` is LLVM's
`llvm-mt` (Apache-2.0 with LLVM exception).
