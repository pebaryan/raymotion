#!/bin/sh
# Build MSVC-style import libraries from system DLLs (this machine's VS install lacks them).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MSVC_DIR="${MSVC_DIR:-C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207}"
BIN="$MSVC_DIR/bin/Hostx64/x64"
OUT="$ROOT/third_party/msvc-sdk-libs"
cd "$ROOT" || exit 1
mkdir -p "$OUT" tools/tmp_defs
cd tools/tmp_defs || exit 1
for n in kernel32 user32 gdi32 shell32 ole32 oleaut32 comdlg32 advapi32; do
  DLL="C:/Windows/System32/$n.dll"
  "$BIN/dumpbin.exe" -EXPORTS "$DLL" > "$n.exports.txt" 2>/dev/null || { echo "no dll for $n"; continue; }
  # def file: LIBRARY header + EXPORTS with plain (undecorated) names, skip ordinals
  awk -v n="$n" 'BEGIN{print "LIBRARY " toupper(n ".DLL"); print "EXPORTS"}
    { # process any line that carries an 8-hex RVA token (export-table rows)
      hit=0; for(i=1;i<=NF;i++) if($i ~ /^[0-9a-fA-F]{8}$/) hit=1
      if(!hit) next
      tok=$NF; sub(/[@][0-9]+$/,"",tok); sub(/=$/,"",tok)
      if(tok ~ /^[A-Za-z_?][A-Za-z0-9_:@?.-]*$/ && !(tok in seen)){ seen[tok]=1; print "  " tok }
    }' "$n.exports.txt" > "$n.def"
  cnt=$(grep -c '^  ' "$n.def")
  if [ "$cnt" -gt 1 ]; then
    "$BIN/lib.exe" /nologo "/def:$n.def" "/out:$OUT/$n.lib" /machine:X64 >/dev/null 2>&1 \
      && echo "built $n.lib ($cnt symbols)" || echo "FAILED $n"
  else
    echo "skip $n (no exports parsed)"
  fi
done
rm -f ../..never-mind 2>/dev/null; true
