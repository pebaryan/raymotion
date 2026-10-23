#!/bin/sh
# Build MSVC-style import libraries (with __imp_ aliases) from system DLLs.
# Needed because this machine's slim VS install ships no Windows-SDK / UCRT import .libs.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MSVC_DIR="${MSVC_DIR:-C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207}"
BIN="$MSVC_DIR/bin/Hostx64/x64"
OUT="$ROOT/third_party/msvc-sdk-libs"
cd "$ROOT" || exit 1
mkdir -p "$OUT" tools/tmp_defs
cd tools/tmp_defs || exit 1
rm -f "$OUT"/*.lib "$OUT"/*.exp
for e in kernel32 user32 gdi32 shell32 ole32 oleaut32 comdlg32 advapi32 winspool-winspool.drv ucrt-ucrtbase ucrtd-ucrtbased; do
  n="${e%%-*}"; base="${e#*-}"; [ "$base" = "$e" ] && base="$n"
  case "$base" in *.*) DLL="C:/Windows/System32/$base" ;; *) DLL="C:/Windows/System32/$base.dll" ;; esac
  unix="$(printf '%s' "$DLL" | sed 's|^C:/|/c/|')"
  ls "$unix" >/dev/null 2>&1 || ls "$(printf '%s' "$unix" | tr 'A-Z' 'a-z')" >/dev/null 2>&1 || { echo "no dll for $n ($DLL)"; continue; }
  mod="$(basename "$DLL" | tr 'a-z' 'A-Z')"
  printf 'LIBRARY %s\nEXPORTS\n' "$mod" > head.txt
  "$BIN/dumpbin.exe" -EXPORTS "$DLL" > "$n.exports.txt" 2>/dev/null
  # import libs: ONLY qualified alias form works here:  __imp_A = MODULE.B   (plain rows break LNK1127)
  awk -v mod="$(basename "$DLL" | tr 'a-z' 'A-Z')" '{hit=0; for(i=1;i<=NF;i++) if($i~/^[0-9a-fA-F]{8}$/) hit=1; if(!hit) next;
       if(NF>=3 && $(NF-1)=="="){ a=$(NF-2); t=$NF; sub(/[@][0-9]+$/,"",t);
          if(a ~ /^[A-Za-z_?]/ && !(a in seen)){seen[a]=1; print "  __imp_" a " = " mod "." t} next }
       tok=$NF; sub(/[@][0-9]+$/,"",tok); sub(/=$/,"",tok)
       if(tok ~ /^[A-Za-z_?][A-Za-z0-9_:@?.-]*$/ && !(tok in seen)){seen[tok]=1; print "  __imp_" tok " = " mod "." tok}}' \
      "$n.exports.txt" >> "$n.def.tmp"
  cat head.txt "$n.def.tmp" > "$n.def"
  cnt=$(grep -c '^  __imp_' "$n.def")
  if [ "$cnt" -gt 0 ]; then
    "$BIN/lib.exe" /nologo "/def:$n.def" "/out:$OUT/$n.lib" /machine:X64 >/dev/null 2>&1 \
      && echo "built $n.lib ($cnt exports)" || echo "FAILED $n"
  else
    echo "skip $n (no exports parsed)"
  fi
done
printf 'LIBRARY UUID.DLL\nEXPORTS\n' > uuid.def   # placeholder: no uuid dll on modern Windows
"$BIN/lib.exe" /nologo "/def:uuid.def" "/out:$OUT/uuid.lib" /machine:X64 >/dev/null 2>&1 && echo "built uuid.lib (empty placeholder)"
