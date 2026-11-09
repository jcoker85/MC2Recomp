#!/usr/bin/env bash
# Builds MC2Recomp from your own Midnight Club II (USA) disc image, in Git Bash on Windows.
#
#   bash tools/build.sh "/c/Games/Midnight Club II (USA).iso"
#
# 1. fetches ps2-native-recomp (the submodule, v0.0.4) and applies this project's changes to it (patches/);
# 2. sets up the toolkit's build tools (its scripts/setup.sh);
# 3. translates the game's code from your disc into C++ and builds the game program (the toolkit's scripts/port.sh), with the
#    switchable widescreen / 60 fps patches (mc2-options/toggles.txt) and this project's HUD and pedestrian patches;
# 4. builds the launcher, MC2Recomp.exe, in this folder, with its icon (tools/make_icon.ps1 downloads it from SteamGridDB).
# Everything taken from the disc stays in ps2-native-recomp/games and ps2-native-recomp/generated, which git ignores.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
iso=${1:-}
if [ -z "$iso" ] || [ ! -f "$iso" ]; then
  echo "usage: bash tools/build.sh <your Midnight Club II (USA) disc image, an .iso>" >&2
  exit 2
fi
iso=$(cd "$(dirname "$iso")" && pwd)/$(basename "$iso")

cd "$here"
git submodule update --init ps2-native-recomp
cd ps2-native-recomp
patch=../patches/ps2-native-recomp.patch
if git apply --check "$patch" 2> /dev/null; then
  git apply "$patch"
  echo "applied $patch"
elif git apply --reverse --check "$patch" 2> /dev/null; then
  echo "$patch is already applied"
else
  echo "ps2-native-recomp has changes that are not $patch: set it back (git -C ps2-native-recomp checkout .) and run again" >&2
  exit 1
fi

bash scripts/setup.sh --native-only
bash scripts/port.sh mc2 "$iso" --native-only \
  --toggles ../mc2-options/toggles.txt \
  --pnach ../mc2-options/hud43.pnach --pnach-section "HUD 4:3" \
  --pnach ../mc2-options/peds60.pnach --pnach-section "Peds 60 FPS"

cd "$here"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/make_icon.ps1
cmd //c "hub\\build.cmd"
echo
echo "Done: run MC2Recomp.exe, press Play and choose your disc image."
