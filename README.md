# MC2Recomp

Midnight Club II (PS2, 2003) as a native Windows program, built with
[ps2-native-recomp](https://github.com/exoteric33/ps2-native-recomp) from your own disc, with a launcher and a set of
optional improvements: widescreen, 60 FPS, higher resolutions, upscaling, and more.

| Game | Disc | Serial |
|---|---|---|
| Midnight Club II | USA | SLUS-20209 (program CRC E5F2DF38) |

This is an unofficial fan project. It is not affiliated with or endorsed by Rockstar Games, Take-Two Interactive or Sony
Interactive Entertainment.

## Play

You need your own disc image of Midnight Club II (USA) as an `.iso`. A `.chd` can be turned into one with MAME's chdman:
`chdman extractdvd -i "Midnight Club II.chd" -o "Midnight Club II.iso"`.

Windows 10 or 11 (64-bit), a CPU with AVX2 and a Direct3D 12 graphics card.

1. Download the latest release zip from [Releases](https://github.com/jcoker85/MC2Recomp/releases) and unzip it anywhere
   (or build it yourself, below).
2. Run `MC2Recomp.exe`. The cover art downloads in the background the first time.
3. Press **Play**. The first time, it asks for your disc image; the launcher remembers it
   (**Settings > Disc Image** changes it).
4. **Settings** has two tabs: **Graphics**, with the options below, and **Controls**. Every option starts off, so a fresh
   install plays the game as it is on a PS2.

### Options

- **Fullscreen**, **Resolution** (1x to 4x the PS2's), **Vsync**, **Frame Pacing Fix** (frames on an even 59.94 Hz clock)
- **Picture Mode**: how the picture is enlarged to the window: Crisp, Soft or Sharp, each with optional edge smoothing,
  or **FSR** (AMD FidelityFX Super Resolution 1)
- **Texture Filtering**: the game's own (Bilinear PS2), Nearest, or Bilinear (Forced)
- **60 FPS** (the PCSX2 community patch by PeterDelta), with fixes for what it left at the wrong speed: the game speed after a
  crash and respawn, and the pedestrians' walk animation
- **Widescreen** 16:9 (the PCSX2 community patch by Arapapa), **Fixed Widescreen HUD** (the race HUD keeps its 4:3 shapes at
  the screen's edges), **Menu/Movie Aspect Ratio** (menus and videos in 4:3 while racing is 16:9)
- **Color Settings**: the PS2's 16-bit picture, or the game's full 32-bit picture; **Dithering** (the PS2's dither on the
  16-bit picture)
- **Smooth Distant Textures** (less shimmer on far buildings; not something a PS2 does)
- **Skip Intro**: straight to the menu, with the game's own developer switch

**Controls** rebinds every PS2 button and each direction of both analog sticks, for a controller or the keyboard and
mouse. A controller row takes a button, a trigger or a stick direction; triggers and stick directions stay analog on a
stick, so the right stick's up and down (the game's gas and brake) can go on the triggers. Choose a row, then press the
new input. An input that another row had swaps with it. The bindings are kept in the game's `keys.ini`.

## Build

On Windows, with Git for Windows (Git Bash), CMake, Ninja, Python 3 and llvm-mingw (UCRT). The toolkit's
`scripts\bootstrap.ps1` installs them with winget; no Visual Studio is needed.

```bash
git clone --recursive https://github.com/jcoker85/MC2Recomp.git
cd MC2Recomp
bash tools/build.sh "/c/Games/Midnight Club II (USA).iso"
```

`tools/build.sh`:

1. fetches ps2-native-recomp v0.0.4 (the submodule) and applies `patches/ps2-native-recomp.patch`;
2. sets up the toolkit's build tools;
3. translates the game's code from your disc into C++ and builds the game program (`ps2run.exe`), with the patches in
   `mc2-options/`;
4. downloads the launcher's icon (`tools/make_icon.ps1`, below) and builds the launcher, `MC2Recomp.exe`.

The first build takes a while. Code the first build did not see is translated while you play and kept for next time.

## What is and is not in this repository

The repository holds the launcher, patches, build files and documentation. It does **not** contain disc images, the game's
program, game assets, cover art, the launcher's icon artwork or recompiled game code. The recompiled C++ and the game program are generated on your
machine from your own disc and stay in `ps2-native-recomp/generated` and `ps2-native-recomp/games`, which git ignores. When the
game runs, it reads its data from your disc image.

Release zips contain the built programs: the launcher and the game program, which includes a machine-translated build of
the game's own code. They contain no disc data, game assets or cover art; the launcher's icon (credited below) is built into `MC2Recomp.exe`. `tools/package_release.sh` makes one from a
finished build. Cover art is downloaded at run time from
[libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails) and is not redistributed here.

- `hub/`: the launcher (`hub.c`, ported from the TwistedMetal1234Recomp menu) and its build files. Its icon is not in the
  repository: `tools/make_icon.ps1` downloads it at build time (any square PNG at `hub/icon/hub.png` is used instead).
- `mc2-options/`:
  - `toggles.txt`: the PCSX2 community patches (Widescreen 16:9 by Arapapa, 60 FPS by PeterDelta) as instructions that
    switch at run time, plus this project's fix for the speed after a respawn.
  - `hud43.py` / `hud43.pnach`: the code behind Fixed Widescreen HUD.
  - `peds60.py` / `peds60.pnach`: the pedestrian animation speed fix at 60 FPS.
  - `menus43.txt`: the game state that tells a race from a menu, for Menu/Movie Aspect Ratio.
- `patches/ps2-native-recomp.patch`: this project's changes to the toolkit: the MC2 HUD hooks and fixes the game needed to
  run, dithering, texture filtering, smooth distant textures, FSR, the 4:3 menus switch, and faster texture uploads.
- `ps2-native-recomp/`: the toolkit, as a git submodule.
- `tools/`: the build script, the release packaging script and the icon script.

## Credits

- [ps2-native-recomp](https://github.com/exoteric33/ps2-native-recomp) by Samir Nuri and exoteric33 (MIT): the
  recompiler and runtime this project is built on.
- The PCSX2 community patches (from [PCSX2/pcsx2_patches](https://github.com/PCSX2/pcsx2_patches)): Widescreen 16:9 by
  Arapapa, 60 FPS by PeterDelta.
- AMD FidelityFX Super Resolution 1.0 (MIT, Copyright (c) 2021 Advanced Micro Devices, Inc.): the FSR picture mode is
  written after AMD's published shaders.
- Box art in the launcher: [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails).
- The launcher and game window icon: [Midnight Club II icon by RAZELIGHT](https://www.steamgriddb.com/icon/90267) on
  SteamGridDB, downloaded at build time.

## License

MIT; see `LICENSE`. ps2-native-recomp is MIT as well.

Midnight Club is a trademark of Take-Two Interactive Software, Inc., and the game is copyright its owners. This repository
contains none of its code, data or artwork, nor the icon and box art the launcher downloads.

## AI disclosure

This project was built with Claude (Anthropic) working as a coding assistant under the direction of the author.

- **Claude did:** wrote most of the code in this repository: the launcher, the changes to the toolkit, and the HUD,
  pedestrian and respawn patches. It also traced the game's code to find the addresses those patches use, and drafted this
  README.
- **Claude did not:** write ps2-native-recomp, which is another project used here as a submodule, the PCSX2 community
  patches, or the game itself, whose code the recompiler translates from your own disc. It did not decide what the project
  should be or when something was right: the author set the goals, played and tested every change, and chose what shipped.
