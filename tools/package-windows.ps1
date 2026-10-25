# Packages the showcase binary for a release: raymotion.exe with the assets it loads from
# <exe dir>/assets, the third-party notices, and a short readme.
#   ./tools/package-windows.ps1 -Build build/Release -Version v0.1.0 -Out dist
param(
  [Parameter(Mandatory)] [string] $Build,
  [Parameter(Mandatory)] [string] $Version,
  [string] $Out = "dist"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$name = "raymotion-$Version-windows-x64"
$stage = Join-Path $Out $name

$exe = Join-Path $Build "raymotion.exe"
if (-not (Test-Path $exe)) { throw "missing $exe" }

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage "licenses") | Out-Null
Copy-Item $exe $stage
Copy-Item -Recurse (Join-Path $root "assets") $stage
Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")
Copy-Item (Join-Path $root "NOTICE") (Join-Path $stage "NOTICE.txt")
Copy-Item (Join-Path $root "third_party/raylib/LICENSE") (Join-Path $stage "licenses/raylib-zlib.txt")
Copy-Item (Join-Path $root "assets/fonts/Inter-OFL.txt") (Join-Path $stage "licenses/Inter-OFL.txt")
Copy-Item (Join-Path $root "assets/fonts/JetBrainsMono-OFL.txt") (Join-Path $stage "licenses/JetBrainsMono-OFL.txt")
Copy-Item (Join-Path $root "assets/fonts/LatinModernMath-GUST-FONT-LICENSE.txt") (Join-Path $stage "licenses/LatinModernMath-GUST-FONT-LICENSE.txt")

@"
raymotion $Version (Windows x64)

Motion graphics written in C++. This is the showcase build: every composition from the
repository's examples, ready to preview and render.

Requirements
  - ffmpeg on PATH, for MP4 output (https://ffmpeg.org/download.html)
  - an OpenGL 3.3 GPU driver
  - the Microsoft Visual C++ 2015-2022 x64 runtime (installed on most systems;
    https://aka.ms/vs/17/release/vc_redist.x64.exe)

Try it
  raymotion list                              list the compositions
  raymotion preview raymotion-promo           play one in a window
  raymotion render raymotion-promo            render it to out/raymotion-promo.mp4
  raymotion                                   with no arguments, every command and option

Keep the assets folder next to raymotion.exe.
Source, docs and the intro video: https://github.com/pebaryan/raymotion
Licensed under the Apache License 2.0 (LICENSE.txt); third-party notices in NOTICE.txt and licenses/.
"@ | Set-Content -Encoding utf8 (Join-Path $stage "README.txt")

$zip = Join-Path $Out "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "packaged $zip"
