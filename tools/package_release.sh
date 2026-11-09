#!/usr/bin/env bash
# Stage a ready-to-run release from a finished build (tools/build.sh) and zip it.
#
# The result is what a player downloads: the launcher, the game program and the files it reads beside itself. It holds no
# disc images, saves, settings or controls from this machine, cover art, sources or generated C++. The player runs the
# launcher, which downloads the cover art, and the first Play asks for their own disc image.
#
# Usage (from the repository root, after tools/build.sh):
#   bash tools/package_release.sh [--version 0.1]
# Output: dist/MC2Recomp-<version>/ and the same name + .zip
#
# The layout keeps the launcher's paths (hub/hub.c): games.txt points at <toolkit>/build/native-mc2/ps2run.exe, the
# toolkit folder holds games/mc2/ (patches.txt; the memory card and keys.ini are made there on first use), and
# mc2-options/ beside the toolkit folder switches on the widescreen / 60 FPS / HUD options.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
VERSION=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) VERSION="${2:?}"; shift 2 ;;
    *) echo "error: unknown argument $1" >&2; exit 1 ;;
  esac
done
[[ -n "$VERSION" ]] || VERSION="$(git describe --tags --always 2>/dev/null || echo dev)"
VERSION="${VERSION#v}"
NAME="MC2Recomp-$VERSION"
STAGE="dist/$NAME"
TK=ps2-native-recomp
BUILD="$TK/build/native-mc2"
GAME="$TK/games/mc2"

for need in MC2Recomp.exe "$BUILD/ps2run.exe" "$BUILD/SDL2.dll" "$BUILD/dxcompiler.dll" "$BUILD/dxil.dll" \
            "$GAME/patches.txt" mc2-options/menus43.txt; do
  [[ -f "$need" ]] || { echo "error: $need is missing; run tools/build.sh first" >&2; exit 1; }
done

rm -rf "$STAGE" "dist/$NAME.zip"
mkdir -p "$STAGE/game/build/native-mc2" "$STAGE/game/games/mc2" "$STAGE/mc2-options" "$STAGE/licenses"
cp MC2Recomp.exe "$STAGE/"
# games.txt with the game program at game/build/native-mc2 (the toolkit folder is "game" in the release)
sed 's#|ps2-native-recomp/build/native-mc2/ps2run.exe|#|game/build/native-mc2/ps2run.exe|#' hub/games.txt > "$STAGE/games.txt"
grep -q '|game/build/native-mc2/ps2run.exe|' "$STAGE/games.txt" || { echo "error: games.txt was not rewritten" >&2; exit 1; }
cp "$BUILD/ps2run.exe" "$BUILD/SDL2.dll" "$BUILD/dxcompiler.dll" "$BUILD/dxil.dll" "$STAGE/game/build/native-mc2/"
cp "$GAME/patches.txt" "$STAGE/game/games/mc2/"
cp mc2-options/menus43.txt "$STAGE/mc2-options/"

cp LICENSE "$STAGE/licenses/MC2Recomp-LICENSE.txt"
cp "$TK/LICENSE" "$STAGE/licenses/ps2-native-recomp-LICENSE.txt"
for f in "$TK"/third_party/dxc/LICEN*; do cp "$f" "$STAGE/licenses/dxc-$(basename "$f")"; done
cat > "$STAGE/licenses/OTHER-NOTICES.txt" <<'EOF'
SDL2 (SDL2.dll, https://libsdl.org): zlib license, Copyright (C) 1997-2024 Sam Lantinga.
wgpu-native (built into ps2run.exe, https://github.com/gfx-rs/wgpu-native): MIT or Apache-2.0, Copyright the wgpu developers.
DirectX Shader Compiler (dxcompiler.dll, dxil.dll, https://github.com/microsoft/DirectXShaderCompiler): see the dxc-*
  files in this folder.
AMD FidelityFX Super Resolution 1.0: MIT, Copyright (c) 2021 Advanced Micro Devices, Inc. (the FSR picture mode is
  written after AMD's published shaders).
Widescreen 16:9 (Arapapa) and 60 FPS (PeterDelta) come from the PCSX2 community patches,
  https://github.com/PCSX2/pcsx2_patches.
EOF

cat > "$STAGE/START_HERE.txt" <<EOF
MC2Recomp $VERSION

Midnight Club II (PS2, 2003) as a native Windows program. Unofficial fan project; not affiliated with or endorsed by
Rockstar Games, Take-Two Interactive or Sony Interactive Entertainment.

You need your own disc image of Midnight Club II (USA, SLUS-20209) as an .iso. Nothing from the game's disc is included
here. A .chd can be turned into an .iso with MAME's chdman:
  chdman extractdvd -i "Midnight Club II.chd" -o "Midnight Club II.iso"

Needs 64-bit Windows 10 or 11, a CPU with AVX2, and a graphics card with Direct3D 12.

1. Run MC2Recomp.exe. The cover art downloads in the background the first time.
2. Press Play. The first time, it asks for your disc image; the launcher remembers it
   (Settings > Disc Image changes it).
3. Settings has two tabs: Graphics (widescreen, 60 FPS, resolution, FSR and more) and Controls. Every option starts
   off, so a fresh install plays the game as it is on a PS2.

Saves and controls are kept in game\\games\\mc2. The game program contains a machine-translated build of the game's own
code. Licenses are in the licenses folder.
EOF

# Nothing personal or taken from the disc may ride along.
bad="$(find "$STAGE" -type f \( -iname '*.iso' -o -iname '*.chd' -o -iname '*.cue' -o -iname '*.bin' -o -iname '*.elf' \
      -o -iname 'SLUS_*' -o -iname '*.ps2' -o -iname 'keys.ini' -o -iname 'settings.ini' -o -iname 'boot.txt' \
      -o -iname '*.png' -o -iname '*.cpp' -o -iname '*.a' -o -iname '*.state' \) 2>/dev/null || true)"
[[ -z "$bad" ]] || { echo "error: the package would include files it must not:" >&2; echo "$bad" >&2; exit 1; }

PY="$(command -v python3 || command -v python)"
"$PY" - "$STAGE" "dist/$NAME.zip" <<'PY'
import os, sys, zipfile
stage, out = sys.argv[1], sys.argv[2]
base = os.path.dirname(stage)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for d, _, files in os.walk(stage):
        for f in sorted(files):
            p = os.path.join(d, f)
            z.write(p, os.path.relpath(p, base))
print(f"{out}: {os.path.getsize(out) / 1048576:.1f} MB")
PY
