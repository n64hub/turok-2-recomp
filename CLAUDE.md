# CLAUDE.md

A static recompilation of the N64 *Turok 2: Seeds of Evil* (USA 1.0) with N64Recomp, N64ModernRuntime, RecompFrontend
and RT64. No decompilation exists: the functions are found in the ROM. Almost all of it is written by AI agents, so
this file is what the next one needs: the rules, where things are, and the facts about the game the code can't say on
its own.

## What lives where

This repo is the game: its symbols, its recompilation, the patches over it and the desktop build. What the Turok games
share is `lib/turok-engine` (n64hub/turok-engine, its README says what is there and how a game uses it): change it
there, then move this repo's submodule onto the commit.

- `syms/layout.toml`: where the game's code is in the ROM and where it runs (through the TLB, at 0x00200400 and
  0x00400000). `syms/names.toml`: the names known, kept for good, and the engine's natives (`turok_*`).
  `syms/turok2.us.syms.toml`: written from both and the ROM by `lib/turok-engine/tools/gen_syms.py`, never by hand.
- `us.toml`: N64Recomp's configuration, with the functions the engine replaces (`ignored`) and the hooks;
  `tools/recompile.sh` writes `RecompiledFuncs/` with it, and the audio microcode with `aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C recompiled into PatchesLib. `ui_funcs.h`, `patch_helpers.h` and
  `recompui_event_structs.h` are RecompFrontend's.
- `src/main/main.cpp`: RecompFrontend's main with the GameEntry, which calls the engine at init, at each thread's
  creation and from the audio callbacks. `src/game/`: the settings and the natives the patches call.
- `lib/`: `turok-engine`, upstream `rt64`, `N64ModernRuntime` and `RecompFrontend` at the commits N64Recomp-browser's
  web forks are built on, and `N64Recomp-browser`, whose `patches/` `tools/build_n64recomp.sh` and `CMakeLists.txt`
  apply to the other three. `runtime-patches/` are this game's own over them.
  A checkout gets them with turok-engine's `tools/submodules.sh`, from a cache its checkouts on the machine share:
  `bash lib/turok-engine/tools/submodules.sh` after `git submodule update --init lib/turok-engine` (at a pin that has
  it), or that script from any turok-engine checkout, given this one's path. Never `git submodule update --init
  --recursive`: it downloads them all again, about 20 minutes, most of it rt64's `contrib/dxc`.
- `tools/build_web.sh` and `tools/docker/Dockerfile.web`: the web page (`docs/web.md`), built with Emscripten in
  N64Recomp-browser's image. `RECOMP_KUSEG_WINDOW` (CMakeLists.txt) gives the TLB-mapped code its memory on the web.
- `tools/release.sh` and `tools/docker/Dockerfile.linux`: the desktop releases for Windows and Linux (the script's header
  says how). Each change a player would notice adds a line to `releases/next.md`; a release moves it to
  `releases/v<version>.md`. Every fix is released: the work that lands one cuts the release once it merges, unasked,
  or waits for one already in flight, which carries it (N64Recomp-browser's CLAUDE.md, Releasing).
- `icons/`: drawn by `tools/icon.ps1` from plain text and shapes.
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Only the
  names, addresses and sizes in `syms/` and the layout file. The ROM is `roms/turok2.us.z64`, never committed.
- CI builds everything but the ROM-generated sources, without linking, on Windows and Linux. turok-engine's CI calls
  it with `engine-ref` to build this repo's `main` against an engine commit.

## Facts about the game

- The boot code copies the first MB to 0x80000400 and the game maps it at 0x00200400 with a TLB entry; a second block
  is mapped at 0x00400000. The GameEntry's entrypoint is the kseg0 address (`turok::boot_load_address`); the engine
  opens the TLB pages and fills them (`turok::map_tlb_sections`).
- The game runs its FPU in FR mode (`turok::set_fpu_mode`), maps ROM regions through its own VM server
  (`turok_vm_map_rom_region`), runs its own PI DMA (`turok_pi_raw_start_dma`) and paces its audio thread by
  osAiGetStatus (`turok_ai_get_status`).
- It wants a Controller Pak in port 1 and asks osMotorInit about a Rumble Pak in each port (`turok_motor_init`):
  without the first it stops on "Controller Pak not found".
- Widescreen (lib/turok-engine's `docs/widescreen.md`) hooks four places, each with a comment in `us.toml`: the frame
  (func_00287248) starts its display list, whose head is the pointer at 0x800D8E24, and ends it; the HUD's rectangles
  are func_0027C1E8's; the camera (func_0027D160) makes its perspective with guPerspectiveF and keeps its
  view-projection matrix as floats at +0x1CC of its struct, which the culling reads.
- The keyboard and mouse: `src/main/main.cpp` hands the mouse's movement to the engine's mouse look, which us.toml's
  hook in the player's look (func_0024EF70) adds to the yaw and pitch, and makes its left button Z; `--input <file>`
  plays a scripted controller for tests (its comment says the format). The game looks down with the stick pushed up,
  as a plane's stick (seen on the page: the up arrow held looked at the ground), so `src/game/config.cpp`'s up arrow
  and right stick push the stick down: up looks up. The menus read the stick too, as direction bits, which us.toml
  swaps back so that up moves their cursor up.
- The finder's mistakes show up as a stack that doesn't balance: a function split in two leaves $sp short after the
  call, and a crash far from it. A start found only by a jal in a data fragment split one function at its epilogue
  (gen_syms now drops those). Check a new syms file for functions whose `addiu $sp` don't balance.
