#!/bin/sh
# Configure + build on this machine (slim VS install): tell CMake where the synthesized
# SDK import libs live and substitute llvm's rc/mt for the missing MSVC tools.
# Override MSVC_DIR / LLVM_MT for another Visual Studio version or install location.
ROOT="$(cd "$(dirname "$0")/.." && (pwd -W 2>/dev/null || pwd))"   # Windows-style path for cl/link
MSVC_DIR="${MSVC_DIR:-C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207}"
LLVM_MT="${LLVM_MT:-C:/Program Files/LLVM/bin/llvm-mt.exe}"
CL="$MSVC_DIR/bin/Hostx64/x64/cl.exe"
SDKLIBS="kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib comdlg32.lib advapi32.lib"
LIBP="$ROOT/third_party/msvc-sdk-libs"
CRTLIB="$MSVC_DIR/lib/x64"
# No LIB env hack here: MSYS path-conversion mangles ; separators into bogus filenames.
# All search paths arrive via explicit /LIBPATH flags on link instead.
export PATH="$ROOT/tools/bin:$PATH"
cd "$ROOT" || exit 1
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CL" -DCMAKE_CXX_COMPILER="$CL" \
  -DCMAKE_RC_COMPILER="$ROOT/tools/bin/rc.exe" \
  -DCMAKE_MT="$LLVM_MT" \
  -DCMAKE_C_STANDARD_LIBRARIES="$SDKLIBS" \
  -DCMAKE_CXX_STANDARD_LIBRARIES="$SDKLIBS" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_CXX_LINK_INITIAL_FLAGS="/MANIFEST:NO" \
  -DCMAKE_EXE_LINKER_INITIAL_FLAGS="/LIBPATH:$LIBP /LIBPATH:$CRTLIB /MANIFEST:NO" \
  || exit 1
cmake --build build
