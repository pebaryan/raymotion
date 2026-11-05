#!/usr/bin/env bash
# Packages the showcase binary for a Linux or macOS release: raymotion and raymotion_script
# with the assets they load from <exe dir>/assets, the third-party notices and a short readme,
# as a .tar.gz (a tarball keeps the executable bit; a zip made here would not).
#   tools/package-unix.sh build v0.10.0 linux-x64 dist
#   tools/package-unix.sh build v0.10.0 macos-arm64 dist
set -euo pipefail

build=${1:?usage: package-unix.sh <build dir> <version> <platform> [out dir]}
version=${2:?missing version}
platform=${3:?missing platform (linux-x64 or macos-arm64)}
out=${4:-dist}
root=$(cd "$(dirname "$0")/.." && pwd)
name="raymotion-$version-$platform"
stage="$out/$name"

[ -x "$build/raymotion" ] || { echo "missing $build/raymotion" >&2; exit 1; }

rm -rf "$stage"
mkdir -p "$stage/licenses"
cp "$build/raymotion" "$stage/"
if [ -x "$build/raymotion_script" ]; then
  cp "$build/raymotion_script" "$stage/"
  mkdir -p "$stage/scripts"
  cp "$root/examples/scripts/hello.as" "$stage/scripts/"
fi
cp -R "$root/assets" "$stage/"
cp "$root/LICENSE" "$stage/LICENSE.txt"
cp "$root/NOTICE" "$stage/NOTICE.txt"
cp "$root/third_party/raylib/LICENSE" "$stage/licenses/raylib-zlib.txt"
cp "$root/third_party/angelscript/LICENSE" "$stage/licenses/angelscript-zlib.txt"
cp "$root/assets/fonts/Inter-OFL.txt" "$stage/licenses/Inter-OFL.txt"
cp "$root/assets/fonts/JetBrainsMono-OFL.txt" "$stage/licenses/JetBrainsMono-OFL.txt"
cp "$root/assets/fonts/LatinModernMath-GUST-FONT-LICENSE.txt" "$stage/licenses/LatinModernMath-GUST-FONT-LICENSE.txt"

case "$platform" in
  linux-*)
    title="Linux x64"
    requirements="  - ffmpeg on PATH, for MP4 output (apt install ffmpeg, dnf install ffmpeg, ...)
  - an OpenGL 3.3 driver and an X11 or XWayland session; on a headless machine run
    under a virtual display: xvfb-run -a -s \"-screen 0 1920x1080x24\" ./raymotion ...
  - glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 and later)"
    ;;
  macos-*)
    title="macOS, Apple Silicon"
    requirements="  - macOS 11 or newer on an Apple Silicon Mac (no Intel build)
  - ffmpeg on PATH, for MP4 output (brew install ffmpeg)
  - the binaries are not signed by a developer ID: after unpacking, clear the download
    quarantine once with  xattr -dr com.apple.quarantine $name"
    ;;
  *) echo "unknown platform $platform" >&2; exit 1 ;;
esac

cat > "$stage/README.txt" <<EOF
raymotion $version ($title)

Motion graphics written in C++. This is the showcase build: every composition from the
repository's examples, ready to preview and render.

Requirements
$requirements

Try it
  ./raymotion list                            list the compositions
  ./raymotion preview raymotion-promo         play one in a window
  ./raymotion render raymotion-promo          render it to out/raymotion-promo.mp4
  ./raymotion                                 with no arguments, every command and option
  ./raymotion_script preview hello-script --script scripts/hello.as
                                              a runtime composition: edit it, run it again

Native C++ compositions passed with --script need the original source build and its
build-tree SDK. This packaged install does not include that SDK and will report that
the native C++ runtime is unavailable.

Keep the assets folder next to raymotion.
Source, docs and the intro video: https://github.com/pebaryan/raymotion
Licensed under the Apache License 2.0 (LICENSE.txt); third-party notices in NOTICE.txt and licenses/.
EOF

tar -C "$out" -czf "$out/$name.tar.gz" "$name"
echo "packaged $out/$name.tar.gz"
