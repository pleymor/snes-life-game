# PVSnesLib toolchain spike — notes

Task 0 output. Records the exact, verified calls tasks 7-12 need. Everything
under "Verified end-to-end" was actually built, run in an emulator, and
confirmed on a captured screenshot at the time (not archived in this repo).
Everything under "Header-verified, not exercised here" is
copied verbatim from the shipped PVSnesLib headers/examples but was not
itself run by this spike's `src/snes/main.c` — the first task that calls it
for real should treat it as unverified until it sees it work.

## 1. Toolchain install — route A (native arm64) worked, first try

No Rosetta, no Docker needed. All PVSnesLib binaries in the 4.6.0 macOS
release are native `Mach-O 64-bit … arm64`.

```bash
# Latest release asset for macOS (checked via GitHub API, not the releases
# HTML page — that page's asset list fails to render):
curl -s https://api.github.com/repos/alekmaul/pvsneslib/releases/latest
# -> pvsneslib_460_64b_darwin_release.zip

curl -sL -o pvsneslib_darwin.zip \
  https://github.com/alekmaul/pvsneslib/releases/download/4.6.0/pvsneslib_460_64b_darwin_release.zip
unzip -q pvsneslib_darwin.zip -d ~/pvsneslib
```

**Gotcha**: the zip's top level is itself a `pvsneslib/` folder, so this
leaves `~/pvsneslib/pvsneslib/devkitsnes/...` — one level too deep for
`PVSNESLIB_HOME=~/pvsneslib` to find anything. Flatten it after unzipping:

```bash
mv ~/pvsneslib/pvsneslib/* ~/pvsneslib/
rmdir ~/pvsneslib/pvsneslib
```

After that:

```bash
export PVSNESLIB_HOME=~/pvsneslib
```

Verification, both succeeded natively (no `arch -x86_64`):

```
$ /Users/<you>/pvsneslib/devkitsnes/bin/816-tcc -v
tcc version 0.9.25

$ /Users/<you>/pvsneslib/devkitsnes/bin/wla-65816 -v
----------------------------------------------------------------------
---               WLA W65816 Macro Assembler v10.7a                ---
----------------------------------------------------------------------
```

`file` on every binary under `devkitsnes/bin/` (`816-tcc`, `wla-65816`,
`wla-spc700`, `wlalink`) and `devkitsnes/tools/` (`gfx4snes`, `gfx2snes`,
`816-opt`, `smconv`, `snesbrr`, `bin2txt`, `snestools`, `tmx2snes`,
`constify`, `fnt4snes`, `snesromusage`) reports `Mach-O 64-bit executable
arm64` for all of them. Route B (Rosetta) and route C (Docker) were not
needed and were not tried.

PVSnesLib version: **4.6.0**. WLA-DX assembler/linker version: **10.7a**
(wla-65816), **5.22a** (wlalink). tcc (816-tcc) version: **0.9.25**.
gfx4snes version: **2.2.0**.

## 2. Emulator — no headless-capable option under the `mesen` cask name; RetroArch answers the question with a clear yes

- `brew install --cask mesen` — **cask does not exist** on this machine's
  homebrew-cask tap (`Error: Cask 'mesen' is unavailable`). Mesen does not
  ship a macOS build; skip it on this platform.
- Installed for **visual, human-driven verification**: `brew install --cask
  snes9x` (Snes9x.app, version 1.63). Its Cocoa build has no CLI flags, no
  AppleScript dictionary (no `.sdef`, `NSAppleScriptEnabled` not set in
  `Info.plist`), and no Lua strings anywhere in the binary or its framework —
  its "Screenshot" feature (confirmed present as a string in
  `snes9x_framework`) is GUI/hotkey-only. **Verdict for Snes9x: no headless
  screenshot path.**
- Installed for **headless, scriptable verification**:
  `brew install --cask retroarch-metal` (RetroArch.app, version 1.22.2),
  plus a libretro SNES core fetched directly (RetroArch's cask does not
  bundle cores; the in-app "Online Updater" is a GUI-only downloader):

  ```bash
  curl -sL -o snes9x_libretro.dylib.zip \
    https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/snes9x_libretro.dylib.zip
  unzip snes9x_libretro.dylib.zip
  mkdir -p ~/Library/Application\ Support/RetroArch/cores
  cp snes9x_libretro.dylib ~/Library/Application\ Support/RetroArch/cores/
  ```

  (native arm64 `Mach-O 64-bit dynamically linked shared library arm64`;
  other native arm64 SNES cores are available at the same buildbot path:
  `bsnes_libretro`, `bsnes_mercury_*`, `mednafen_snes_libretro`, etc.)

### Verdict: **YES, headless screenshots work**, via RetroArch's `--max-frames`/`--max-frames-ss` flags

```bash
# config override — RetroArch pauses core emulation by default when its
# window doesn't have OS focus (pause_nonactive, default true). Without
# this override every screenshot comes back solid black because the core
# never actually ran a frame.
cat > headless.cfg <<'EOF'
pause_nonactive = "false"
video_vsync = "false"
audio_enable = "false"
EOF

/Applications/RetroArch.app/Contents/MacOS/RetroArch \
  -L ~/Library/Application\ Support/RetroArch/cores/snes9x_libretro.dylib \
  build/life.sfc \
  --appendconfig=headless.cfg \
  --max-frames=400 \
  --max-frames-ss \
  --max-frames-ss-path=screenshot.png
```

This runs the ROM for exactly 400 frames with no window focus, no click,
no manual step, writes `screenshot.png`, and exits on its own (checked exit
code 0). The very first run of RetroArch also silently extracts a bundled
`assets.zip` (fonts/GUI images) and shows a first-run overlay that eats
whatever frame budget you gave it — run it once to let that finish before
trusting a screenshot from a real content run. `--max-frames=400` (as
opposed to the ~120-180 tried first) also clears RetroArch's own "content
loaded" on-screen notification banner, which otherwise sits across the
middle of the captured frame for its first few seconds.

Verified: `screenshot.png` from the command above shows this spike's exact
32x24 checkerboard tilemap (built by `fillTilemap()` in `src/snes/main.c`)
and the red sprite square at (100,100), with a plain, non-technical
resolution — this is a real, actionable answer, not a partial one.

**Consequence for tasks 7-12**: automated screenshot verification is
available and is the RetroArch recipe above (swap the `.sfc` path and, if a
task cares about a specific frame, tune `--max-frames`). It does not need a
human at a screen. Prefer this over the GUI Snes9x install for anything that
needs to run unattended (CI, agent-driven verification); keep Snes9x around
only for a human to sanity-check something interactively.

**Foreground launches can hang forever and ignore `SIGALRM` (task 10:
several runs hung on this)**: running the RetroArch command above directly
in the foreground, even wrapped in a shell-level timeout (`perl -e 'alarm
N; exec ...'` or equivalent), is not safe — RetroArch sometimes sits past
the alarm without ever receiving it, and the wrapping call then blocks
forever, taking down whatever launched it. Always launch RetroArch in the
**background** and poll for it from the outside instead of trusting any
in-process alarm: start the process with `&`, remember its PID (`$!`),
poll `kill -0 $pid` once a second up to a hard limit, and `kill -9 $pid` if
it is still alive past that limit — printing something like `frames=N
rc=0 <bytes>` on success or `frames=N HUNG (killed after LIMITs)` on
timeout is enough to tell the two cases apart automatically. This
background-launch + external-poll + `SIGKILL`-watchdog pattern (never a
foreground wait) is the reusable lesson: it is the only reliable way found
so far to bound a RetroArch headless capture from an automated job.

## 3. Produces — the exact, verified PVSnesLib API

All symbols below come from `$PVSNESLIB_HOME/pvsneslib/include/snes/*.h`
(shipped with the 4.6.0 release) and, where marked, from
`src/snes/main.c` in this repo.

### Console / system initialization

`consoleInit(void)` (declared in `snes/console.h`) is called **automatically
by the crt0 startup code before `main()` runs** — confirmed by `strings` on
`$PVSNESLIB_HOME/pvsneslib/lib/LoROM_SlowROM/crt0_snes.obj`, which
references the `consoleInit` symbol. No PVSnesLib example calls it from
`main()`. Do not call it again unless you specifically need to
re-initialize.

**Verified** (`src/snes/main.c`) — the practical "console init" call every
example actually uses is the text-console+font initializer:

```c
consoleInitDefaultText(0);
```

This reserves a BG, loads the font shipped with PVSnesLib into VRAM as the
tile graphics, loads its palette, and points that BG's map at VRAM `$6800`
and its graphics at VRAM `$3000` (offset 0) — this is also this spike's
answer for "load a tileset and a palette".

### Graphics mode

**Verified** (`src/snes/main.c`):

```c
setMode(BG_MODE1, 0);
bgSetDisable(1);
bgSetDisable(2);
```

`BG_MODE1` is defined in `snes/background.h` (16-color BG0/BG1, 4-color
BG2). `setMode`/`bgSetDisable` are declared in `snes/video.h` /
`snes/background.h` respectively.

### Loading a tileset + a palette

**Verified** (BG, via the console font — see "Console/system
initialization" above):

```c
consoleInitDefaultText(0);
bgSetGfxPtr(0, 0x3000);              // redundant with the above, kept explicit
bgSetMapPtr(0, 0x6800, SC_32x32);    // redundant with the above, kept explicit
```

**Verified** (sprite tile + palette, hand-authored inline — no PNG/gfx4snes
needed for a single solid-color tile):

```c
const u8 spriteTile[32] = { /* 4bpp SNES planar, see src/snes/main.c */ };
const u16 spritePalette[16] = { 0x0000, 0x001F, /* ... zeros ... */ };

oamInitGfxSet((u8 *)spriteTile, sizeof(spriteTile),
              (u8 *)spritePalette, sizeof(spritePalette),
              0, 0x0000, OBJ_SIZE8_L16);
```

This is the exact call — signature, argument order, `sizeof()` for the two
size params — that put the red square on screen in the verification
screenshot.

**Header-verified, not exercised here** — loading a *custom* (non-default,
non-inline) BG tileset + palette from real art, the way a later task will need
to for actual game graphics, is a two-step pipeline:

```bash
# 1. PNG -> .pic (tile graphics) + .pal (palette) + <name>.inc (C externs)
#    + <name>_data.as (assembly .incbin glue), all auto-named from the
#    input file. Confirmed working (ran against a real 256x224 PNG):
$PVSNESLIB_HOME/devkitsnes/tools/gfx4snes -s 8 -o 16 -u 16 -e 0 -p -i path/to/tiles.png
# -> path/to/tiles.pic, path/to/tiles.pal, path/to/tiles.inc, path/to/tiles_data.as
```

`tiles.inc` (generated, exact content):

```c
extern unsigned char tiles_til,tiles_tilend;
extern unsigned char tiles_pal,tiles_palend;
```

`tiles_data.as` (generated, exact content):

```
tiles_til:
.incbin "path/to/tiles.pic"
tiles_tilend:

tiles_pal:
.incbin "path/to/tiles.pal"
tiles_palend:
```

Per the shipped `Mode1` and `SimpleSprite` examples, that generated `.as`
file is not compiled directly — wrap it in a hand-written `.asm` (matches
`SimpleSprite/data.asm` verbatim) so WLA has the `.MEMORYMAP`/section
context it needs:

```
.include "hdr.asm"

.section ".rodata_tiles" superfree
.include "tiles_data.as"
.ends
```

Then, in C:

```c
#include "tiles.inc"   // or the two extern lines above, by hand
bgInitTileSet(0, &tiles_til, &tiles_pal, 0,
              (&tiles_tilend - &tiles_til), (&tiles_palend - &tiles_pal),
              BG_16COLORS, 0x4000);
```

`bgInitTileSet`'s signature above is copied from `snes/background.h`.
**This spike did not link a `bgInitTileSet` call into `build/life.sfc`** —
it deliberately avoids touching `data/`, which is out of scope for Task 0 —
so treat it as header-verified only until the task that owns `data/`
builds and screenshots it for real. `.asm`/`_data.as` files placed under
`src/snes/*/*.asm` are picked up automatically by `snes_rules` (see
Makefile section below); nothing needs registering by hand.

#### Addendum — verified in Task 8: the full pipeline, real symbol names, gotchas

Task 8 ran the pipeline above for real against `data/tiles.bmp` (a
generated 128×16 8bpp indexed BMP, 32 tile slots, 16 colors) and linked the
result into `build/life.sfc`, confirmed on a RetroArch screenshot. Exact,
working command (gfx4snes 2.2.0):

```bash
$PVSNESLIB_HOME/devkitsnes/tools/gfx4snes -s 8 -o 16 -u 16 -t bmp -e 0 -p -i data/tiles.bmp
```

(`-s 8` 8×8 tiles, `-o 16`/`-u 16` output and use 16 colors, `-t bmp`
explicit input type, `-e 0` palette entry 0, `-p` also emit the `.pal`.)
Output, real content:

```
data/tiles.pic       1024 bytes  (32 tiles * 32 bytes/tile, 4bpp planar)
data/tiles.pal         32 bytes  (16 colors * 2 bytes BGR555)
data/tiles.inc         (extern unsigned char tiles_til,tiles_tilend;
                         extern unsigned char tiles_pal,tiles_palend;)
data/tiles_data.as     (tiles_til:/.incbin "data/tiles.pic"/tiles_tilend:
                         tiles_pal:/.incbin "data/tiles.pal"/tiles_palend:)
```

Symbol names are exactly `<basename>_til` / `<basename>_tilend` /
`<basename>_pal` / `<basename>_palend`, `<basename>` being the `-i` filename
without directory or extension (`tiles`, from `data/tiles.bmp`) — confirmed
by reading the generated `data/tiles.inc` after the real run, not assumed.
`src/snes/render.c` declares these four symbols by hand (matching
`tiles.inc`'s content) instead of `#include`-ing `data/tiles.inc`, to avoid
adding a `data/` include path to `CFLAGS` for one header.

**`.incbin` paths are relative to the assembler's process CWD (the repo
root, since `make` always runs from there), not to the `.asm` file's own
directory.** gfx4snes bakes the exact path given to `-i` (here `data/`)
into the generated `.incbin "data/tiles.pic"` line, so the wrapper file
must live somewhere `wla-65816` is invoked from the root — it does not need
to sit next to the `.pic`. Task 8's wrapper, `src/snes/tiles.asm` (matches
`SimpleSprite/data.asm`'s shape, `.include` path adjusted):

```
.include "hdr.asm"

.section ".rodata_tiles" superfree
.include "data/tiles_data.as"
.ends
```

C load sequence that worked, unmodified from the header-verified draft
above, `TILES_VRAM_ADDR = 0x4000` (tile data) / `MAP_VRAM_ADDR = 0x0000`
(BG1 tilemap), same split as the shipped `Mode1` example:

```c
extern char tiles_til, tiles_tilend;
extern char tiles_pal, tiles_palend;

bgInitTileSet(0, &tiles_til, &tiles_pal, 0,
              (u16)(&tiles_tilend - &tiles_til),
              (u16)(&tiles_palend - &tiles_pal),
              BG_16COLORS, 0x4000);
bgSetMapPtr(0, 0x0000, SC_32x32);
setMode(BG_MODE1, 0);
```

Two build-system gotchas found only by actually wiring this in, both now
handled in the root `Makefile`:

- `CFLAGS` from `snes_rules` only adds `-I$(PVSNESLIB_HOME)/...` and
  `-I$(CURDIR)` (the repo root) — a `src/snes/*.c` file that
  `#include "match.h"` (a `src/core/` header, by quoted name with no
  relative path) fails to compile without an explicit `CFLAGS += -Isrc/core`
  added in the project `Makefile`.
- `make`'s dependency graph does not see through `.include` inside a
  `.asm` file, so nothing forces `data/tiles.pic`/`data/tiles.pal` to exist
  before `wla-65816` assembles `src/snes/tiles.asm` on a from-clean build.
  Fixed with an explicit extra prerequisite line:
  `src/snes/tiles.obj: data/tiles.pic data/tiles.pal`.

gfx4snes also always emits `data/tiles.inc` and `data/tiles_data.as`
alongside `.pic`/`.pal`; `.gitignore` needed two more patterns
(`data/*.inc`, `data/*_data.as`) beyond Task 0's anticipatory
`data/*.pic`/`data/*.pal` to keep `git status` clean after `make rom`.

Verified end-to-end: `make rom` from a fully clean tree (no `data/*`,
`build/`, `hdr.asm`, `linkfile`, `life.log` present) regenerates
`data/tiles.bmp` from `tools/mktiles.py`, converts it, assembles, links,
and produces `build/life.sfc` in one pass with zero compiler/assembler/
linker warnings; a RetroArch headless screenshot (recipe in section 2)
shows the loaded tiles and palette rendering correctly (verified during
that task, screenshot not archived in this repo).

### Writing a tilemap to VRAM by DMA (the task 7 mechanism)

**Verified** (`src/snes/main.c`) — this is the one the whole spike exists to
prove:

```c
#define MAP_WIDTH 32
#define MAP_HEIGHT 24
u16 tilemap[MAP_WIDTH * MAP_HEIGHT];   // plain C memory, not a linked-in asset

void fillTilemap(void)
{
    u16 x, y;
    for (y = 0; y < MAP_HEIGHT; y++)
        for (x = 0; x < MAP_WIDTH; x++)
            tilemap[y * MAP_WIDTH + x] = ((x + y) & 1) ? 1 : 0;
}

// in main(), after fillTilemap():
dmaCopyVram((u8 *)tilemap, 0x6800, sizeof(tilemap));
```

`dmaCopyVram(u8 *source, u16 address, u16 size)` is declared in
`snes/dma.h`. `source` is cast to `u8 *` regardless of the map's element
type; `address` is the same VRAM word-address value passed to
`bgSetMapPtr`/`bgInitMapSet` elsewhere (`0x6800` here, matching this
spike's BG0 map pointer); `size` is a **byte** count (`sizeof(tilemap)`,
not the tile count). The checkerboard pattern this produces is visible,
unmodified, in the verification screenshot.

### Placing a sprite

**Verified** (`src/snes/main.c`, immediately after the `oamInitGfxSet` call
above):

```c
oamSet(0, 100, 100, 3, 0, 0, 0, 0);
oamSetEx(0, OBJ_SMALL, OBJ_SHOW);
```

`oamSet(u16 id, u16 xspr, u16 yspr, u8 priority, u8 hflip, u8 vflip, u16
gfxoffset, u8 paletteoffset)` and `oamSetEx(u16 id, u8 size, u8 hide)` are
declared in `snes/sprite.h`. This is the exact call (id 0, position
100,100, priority 3, gfx offset 0, palette offset 0) that produced the red
square at the expected screen position in the verification screenshot.

### Reading the controllers

**Verified to compile, link, and run every frame** (`src/snes/main.c`):

```c
u16 pad0 = padsCurrent(0);
```

`padsCurrent` is a macro (`snes/input.h`): `#define padsCurrent(value)
(pad_keys[value])`. Button bit values are an enum in the same header —
`KEY_A`, `KEY_B`, `KEY_X`, `KEY_Y`, `KEY_L`, `KEY_R`, `KEY_START`,
`KEY_SELECT`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT` — matching the
shipped `input/controller/controller.c` example (`switch (pad0) { case
KEY_A: ... }`). This spike reads the pad every frame but does not act on
the value, so this is confirmed to run without fault, not confirmed
visually — the first task that branches on a button press should be the
first real check of the exact bit values.

### Waiting for VBlank

**Verified** (`src/snes/main.c`, the main loop):

```c
while (1)
{
    u16 pad0 = padsCurrent(0);
    (void)pad0;
    WaitForVBlank();
}
```

`WaitForVBlank(void)` is declared in `snes/interrupt.h`. Confirmed
indirectly: the emulator produced a clean, uncorrupted frame after hundreds
of frames of headless execution (a missing/broken vblank gate around a VRAM
DMA reliably shows up as tearing/garbage on real hardware and in accurate
emulator cores).

## 4. Makefile — the `rom` target

Root `Makefile` (this task owns only `PVSNESLIB_HOME` and `rom` —
task 1 appends `test`/`clean`/`all` and `.DEFAULT_GOAL` on top of this):

```make
PVSNESLIB_HOME ?= $(HOME)/pvsneslib

ifneq (,$(filter rom,$(MAKECMDGOALS))$(BUILD_ROM))

export PVSNESLIB_HOME
export BUILD_ROM := 1
export ROMNAME := life
export ROMTITLE := SNES LIFE GAME

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

.PHONY: rom
rom: buildWithSummary
	mkdir -p build
	mv $(ROMNAME).sfc build/
	mv $(ROMNAME).sym build/
	mv $(ROMNAME).symfull build/

buildActual: $(OFILES) $(ROMNAME).sfc

else

.PHONY: rom
rom:
	@echo "Run 'make rom' on its own (not combined with other targets)."
	@exit 1

endif
```

Notes for whoever extends this:

- **Why the `ifneq` guard**: this distribution's build system is
  `devkitsnes/snes_rules` (**not** `devkitsnes/rules.mk` — the brief's step
  4 example names a file this release does not ship; `snes_rules` is what
  actually exists and works). Including it unconditionally would run its
  parse-time side effects (shelling out to generate `hdr.asm`, requiring
  `ROMNAME` and a real `PVSNESLIB_HOME`) on **every** `make` invocation,
  including `make test`/`make clean` from task 1 — breaking the host-side
  tasks on any machine without PVSnesLib installed. The guard keeps the
  SNES toolchain fully opt-in.
- **Why `BUILD_ROM` and not just `$(MAKECMDGOALS)`**: `snes_rules`' own
  `.sfc`-building recipe (`buildWithSummary`) re-invokes `make` as a child
  process (`make buildActual`), whose own `$(MAKECMDGOALS)` is
  `buildActual`, not `rom` — the guard would otherwise exclude that child
  invocation and fail with `No rule to make target 'buildActual'`.
  `export BUILD_ROM := 1` propagates through the environment to that child.
- `snes_rules` finds C sources at `src/*.c`, `src/*/*.c`, `src/*/*/*.c` — so
  `src/snes/main.c` is picked up with no extra `SRC`/`CFILES` configuration.
  The same globs apply to `.asm` (not `.as`) files, so a future
  `src/snes/data.asm` (see "Loading a tileset + a palette" above) needs no
  registration either.
- `snes_rules` places its outputs (`$(ROMNAME).sfc/.sym/.symfull`,
  `hdr.asm`, `linkfile`, `$(ROMNAME).log`) in the working directory (i.e.
  the repo root), not in `build/`; the `rom` target itself moves the three
  ROM artifacts into `build/` afterwards. `hdr.asm`, `linkfile` and
  `life.log` are left at the repo root and gitignored (see below) rather
  than moved, since `snes_rules` regenerates/expects them there on every
  build.
- `buildWithSummary`'s own recipe has a bug-for-us: one of its steps is a
  bare `grep -E " free" $(ROMNAME).log` with no `|| true`, so **a
  successful build with no bank-usage lines in the log still exits
  non-zero**. This did not bite here because the linker always prints "X
  free" bank usage lines on success, but if a future `rom` build fails with
  a make error yet an apparently-clean `$(ROMNAME).log`, check for this
  before assuming the actual compile/link failed.

Verified:

```
$ export PVSNESLIB_HOME=~/pvsneslib && make rom
...
ROM: 252449 bytes (96.30%) free of total 262144.
RAM: 154451 bytes (94.27%) free of total 163840.
mkdir -p build
mv life.sfc build/
mv life.sym build/
mv life.symfull build/
$ ls -la build/*.sfc
-rw-r--r--  1 ...  256K ... build/life.sfc
```

`build/life.sfc` loads in RetroArch's snes9x core as `"SNES LIFE GAME"
(NTSC) … LoROM: 2 Mbit … Checksum OK`, and the screenshot recipe in section
2 shows the tilemap and sprite from `src/snes/main.c` rendered correctly.

## 5. `.gitignore` additions

Brief-specified, plus a few more build byproducts discovered while actually
building (`hdr.asm`, `linkfile`, `life.log`, `*.symfull` — none of the brief's
three patterns cover these, and without ignoring them `git status` is dirty
after every single `make rom`):

```
data/*.pic
data/*.pal
*.obj
/hdr.asm
/linkfile
/life.log
*.symfull
```

`data/*.pic`/`data/*.pal` are anticipatory: this spike does not itself put
anything under `data/` (out of scope for Task 0 — see `main.c`'s comment),
but a later task will run `gfx4snes` against real art there and these are
exactly the two binary output extensions it produces (confirmed in section
3 above).

## 6. Do not rewrite `src/snes/main.c` from scratch

Task 7 extends this file — the tilemap array, `fillTilemap()`, and the
`dmaCopyVram` call are the mechanism it needs; the sprite/pad/vblank code is
there so every "Produces" item has one real, verified call to point at.

## 7. Addendum — verified in Task 9: VBlank discipline, sharing a BG's tile
set with a second BG, and the OAM/NMI question

Task 9 added the HUD (BG2) and the cursor sprite, and needed a firm answer
to "does `oamSet`/`oamSetEx` need a manual VRAM/OAM DMA the way
`dmaCopyVram` does, or does PVSnesLib already move sprites to the PPU on
its own?"

**Answer, read straight out of the shipped header**
(`$PVSNESLIB_HOME/pvsneslib/include/snes/interrupt.h`, the `\brief` block
above `WaitForVBlank`): PVSnesLib installs a default NMI (VBlank interrupt)
handler that, on every VBlank that isn't a "lag-frame", **automatically
transfers the `oamMemory` RAM buffer to the PPU's OAM**, before calling the
optional `nmi_handler` callback. `oamSet`/`oamSetEx`/`oamSetXY`/etc. only
write into that RAM buffer (confirmed by the `oamGetX`/`oamGetY` macros in
`snes/sprite.h`, which read straight out of `oamMemory[id + ...]`) — they
never touch the PPU directly. So: **no manual step is needed for sprites**.
Calling `oamSet`/`oamSetEx` at any point in the frame (not just inside a
VBlank window) is safe and is in fact how every PVSnesLib example does it;
there is nothing for a `render_vblank()`-style function to do for the
cursor. This is different from tilemap/CGRAM updates, which really do need
`dmaCopyVram`/`dmaCopyCGram` and really do need to happen during VBlank (or
forced blank) to avoid tearing — `oamMemory` is the one exception, because
PVSnesLib's own ISR does that DMA for you every frame.

**VBlank discipline pattern used for the tilemap DMAs**: `render_board_now()`
and `render_hud_now()` (`src/snes/render.c`) only compute into a static
buffer and set a `static bool_t ..._pending = TRUE` flag; a new
`render_vblank()` function, called right after `WaitForVBlank()` returns
(never before), does the actual `dmaCopyVram()` calls and clears the flags.
Reason: `render_board_now()` walks all `BOARD_H * BOARD_W` (768) cells
through `view_board()` before it can even start a DMA, which is measurably
longer than the ~2273-cycle VBlank window on NTSC — issuing the DMA
immediately after that computation (the way Task 8's `render_board_now()`
used to, with its own `WaitForVBlank()` call baked in) risks landing the
transfer after the PPU has already left VBlank, which shows up as tearing.
Splitting "prepare" from "transfer" and doing the actual DMA calls back to
back right after a single `WaitForVBlank()` keeps every VRAM write inside
one VBlank window per frame, and keeps the game loop to exactly one
`WaitForVBlank()` call per frame (three separate waits, one per render
call, would have divided the effective frame rate by three).

**Sharing one BG's tile set with a second BG**: `bgInitTileSet()` (used
once, for BG1/`bgNumber 0`) both uploads the tiles/palette to VRAM/CGRAM
*and* points BG1's own tile-graphics register at them. A second BG that
wants to reuse the same uploaded tiles (Task 9's BG2/HUD, `bgNumber 1`,
reusing BG1's tile art) does **not** call `bgInitTileSet()` again — that
would re-upload — it calls `bgSetGfxPtr(bgNumber, address)` alone, with the
exact same VRAM `address` value used by the original `bgInitTileSet()`
call. Confirmed against the shipped `DynamicSprite` example's pattern of
independently pointing multiple layers/objects at shared VRAM addresses,
and confirmed working on screen (both BG1 and BG2 render the digits/glyphs
identically from the one tile sheet). `bgSetMapPtr(bgNumber, address,
SC_32x32)` is still called separately per BG (each BG needs its own
tilemap address; that part isn't shared). `bgSetEnable(bgNumber)` /
`bgSetDisable(bgNumber)` (declared in `snes/background.h`) are the
counterparts to flip a BG's visibility on/off after `setMode()`.

**Sprite VRAM address is a plain word address in the same space as BG
tiles**, not a distinct "8K-word steps" unit as one line of its doc comment
suggests — confirmed against the shipped `DynamicSprite` example, which
puts sprite graphics at `0x0000`/`0x1000` and BG tile graphics at `0x2000`
in the same call sequence, i.e. the same word-addressed VRAM space BG
tiles/maps use (matching `dmaCopyVram`'s own `address` convention recorded
in section 3 above). Task 9 places the cursor's one 8×8 tile at `0x2000`,
between the two BG tilemaps (`0x0000`–`0x07FF`) and the shared BG tile data
(`0x4000`+), with room to spare on both sides.

**Headless screenshot timing caveat, sharpened, then root-caused (fix round
1)**: section 2's `--max-frames=N` recipe is confirmed to need N well past
400 in practice — RetroArch's own "content loaded" notification banner was
still visible at `--max-frames=40` in this task's testing (it cleared by
400).

Task 9's first pass also found that the raw RetroArch frame count did not
reliably predict the ROM's own internal frame counter (a `frame` variable
incremented once per loop iteration in `main.c`): pairs of raw frame counts
exactly one half-period apart (e.g. `--max-frames=400` vs `416`, for a
32-frame blink period) sometimes landed on the *same* blink phase instead
of the expected opposite one. First hypothesis (wrong, logged here as a
worked example of an unproven guess being called out in review) was a
one-off phase shift from a slower-than-one-frame first loop iteration. The
real cause, found by a reviewer and confirmed by measurement: every loop
iteration called `render_hud_now()`, which called `view_hud()`
unconditionally — `view_hud()` calls `board_count()` twice, each a full
`BOARD_H * BOARD_W` = 768-cell scan, so every single frame paid ~1536 cell
reads (through `board_get()`'s halo-relative indexing) purely to support a
blink that only ever changes one tile. On 816-tcc-compiled 65816 code this
recurring per-frame cost was enough, on some frames, to make the loop take
longer than one VBlank period end-to-end, producing a "lag frame" (see
`WaitForVBlank`'s own doc comment on lag-frames, §3 above) — a *recurring*,
not one-off, source of drift between raw elapsed frames and completed loop
iterations, which explains the non-monotonic on/off pattern originally
observed across `400/416/800/1200/2000/4000/8000`.

**Fix**: gate the expensive `view_hud()` recomputation behind a `hud_dirty`
flag (`src/snes/render.c`), refreshed only at startup and whenever
`render_hud_dirty()` is called (from task 10 on, alongside `board_dirty`);
the per-frame blink itself only toggles one cached tile value between its
stored color and `TILE_EMPTY` — no board walk. After this fix, `--max-frames`
pairs exactly 16 apart reliably land on opposite blink phases (confirmed at
`600`/`616`, `1000`/`1016`, `2000`/`2016`, all three pairs opposite, by the
actual screenshots/pixel readings captured during that fix, not archived
in this repo). **Consequence for later
tasks**: per-frame render-preparation functions must stay O(1) (or at least
comfortably sub-frame) — anything that re-walks the board or does
comparable work every single frame, gated by nothing, is a lag-frame risk
on this CPU, not just a screenshot-timing inconvenience. Still worth a
quick sweep/measurement rather than a guess when verifying a blink or
any other periodic effect on screen, since a regression here silently
un-does the 1:1 loop/VBlank guarantee without any compiler warning.

## 8. Task 10 — a range-shown board rebuild costs ~350 raw frames (~5.8 s), not sub-frame (fixed, see update below)

`render_board_now()`'s `view_board()` (`src/core/view.c`) calls
`rules_in_range()` for every empty cell of the 768-cell board when
`show_range` is true (dozens of cells around each figure, on this board
roughly 700+ of the 768). Unlike §7's HUD case, this call is already
gated by `board_dirty`/the snapshot comparison in `main.c` — it does *not*
run every frame — but every time it *does* run (every placement, undo, and
turn switch, since `cur.show_range` defaults `TRUE` and this task's script
only ever turns it off and back on once), the call itself is expensive
enough to blow through many VBlank periods in one go, not just tip one
iteration slightly over budget. The whole computation happens **inside**
the loop iteration that reads the input, *before* that iteration's own
`WaitForVBlank()` — so the PPU keeps showing the previous frame's
VRAM/OAM contents (nothing has been DMA'd yet) for as many raw frames as
the computation takes, and the screen visibly "jumps" to the new state
all at once once `render_vblank()`'s DMA finally runs.

Measured (task 10 verification) by bisecting headless captures (background
RetroArch launch, `--max-frames` swept and polled from the outside per §2)
around a single `SELECT` toggle (off = cheap, occupancy-only scan; back on
= expensive, with `rules_in_range`) at a fixed cursor position, using the range-dot
pixel count as the visible signal: the cheap (no-range) rebuild costs
roughly ~20 raw frames; the expensive (range-shown) rebuild costs roughly
**~350 raw frames**, cross-checked against an independent transition (the
first placement) landing within the predicted window. 350 raw NTSC frames
is ~5.8 real seconds of total visual freeze on every single placement,
undo, and turn switch — confirmed, not fixed (per this task's explicit
">30 frames, don't optimise, report it" ruling). **Consequence for later
tasks**: a per-*dirty-event* cost, not just a per-*frame* cost, can also
blow the loop's real-time budget by orders of magnitude on this CPU;
`rules_in_range`'s O(cells × radius²) toroidal Chebyshev-distance check
run ~700 times per rebuild is the likely culprit and is a candidate for
a future optimisation task (e.g. only rescanning cells near the figures
that actually moved, instead of the whole board) — out of scope here.

### Update — fixed by computing the range as a mask by dilation

`rules_range_mask()` (`src/core/rules.h`/`.c`) replaces the per-empty-cell
`rules_in_range()` scan with a dilation: clear an `[BOARD_H][BOARD_W]`
mask, then for each of `player`'s own cells on the board, stamp the
`(2*RANGE_RADIUS+1)²` square around it (wrapped via `BOARD_WRAP_X/Y`).
Cost is now proportional to *occupied cells* (~9 for a fresh side) × 25,
not *board cells* (768) × 25. Both `view_board` (`src/core/view.c`) and
`ai.c`'s `collect()` compute the mask once per call (into a file-scope
static buffer — the mask itself is 768 bytes, too big for the 65816's
stack) instead of calling `rules_in_range()` per cell; `rules_in_range()`
itself is untouched and still backs `match_place`'s single-cell check.
An equivalence test (`tests/test_rules.c`) checks `rules_range_mask()`
against `rules_in_range()` cell-by-cell on the empty board, `board_seed()`
for both colours, single cells at all four corners/edges (seam wrap), and
several pseudo-random boards (a small in-test xorshift32, not `rand()`),
plus a check that the other colour's cells don't contribute. `make test`
(1327 → 19761 checks, all passing — the equivalence tests each check
every one of the 768 cells on several boards, which is where almost all
the new check count comes from) and `PVSNESLIB_HOME=/nonexistent make
test` both stay pristine; `make rom` and `make rom-script` build clean
(816-tcc initially hit an internal compiler error on the
`mask[BOARD_WRAP_Y(...)][BOARD_WRAP_X(...)]` indexing expression —
worked around by hoisting the wrapped coordinates into named `int`
locals before indexing).

Re-measured with the same method as above (bisecting headless captures
around the `SELECT` toggle, range-dot pixel count as the signal), on
`build/life-script.sfc`:

- **before**: the toggle-on (expensive) rebuild cost **~350 raw frames**
  (~5.8 s).
- **after**: toggle-off (cheap, unaffected by this fix — it never called
  `rules_in_range`/`rules_range_mask` at all) still completes at raw
  frame 754; toggle-on (now mask-based) completes at raw frame 804 — a
  delta of 50 raw frames, of which 8 are the idle gap between the two
  `SELECT` presses in the script, leaving **~42 raw frames (~0.7 s)** for
  the mask-based rebuild itself. Cross-checked against the first
  placement's own (also mask-based) rebuild: predicted to land around raw
  854 (804 + 8-call gap + ~42), observed at raw 856 — matches within a
  couple of frames. Screenshots captured for this measurement (off/on
  transition and the placement cross-check) are not archived in this
  repo.
- That is roughly an **8× reduction** (~350 → ~42 raw frames), turning a
  ~5.8 s total-visual-freeze per placement/undo/turn-switch into ~0.7 s.
  Still not sub-frame — a mask recompute still touches every one of the
  768 cells once (to find `player`'s cells) even though it no longer
  tests each one against every neighbour — but it is proportional to the
  board's cell count once, not to cell count × neighbourhood size, and it
  no longer dominates the frame budget by two orders of magnitude.

### Fix round 1 — row-pointer indexing, and the mask cached once per turn

A follow-up review, compiling `board.c`/`view.c`/`rules.c` with 816-tcc
directly, pinned the remaining ~42-frame cost on two things:

1. **The multiply per `Board` access.** `board_get()`'s `c[y + 1][x + 1]`
   indexing (and the equivalent direct indexing in `board_count()` and
   `rules_range_mask()`'s scan) multiplies by `BSTRIDE` (34) on every
   single access — 34 is not a power of two, so 816-tcc emits a real
   `jsr.l` to a multiply routine, not a shift. A range-shown rebuild was 5
   full passes of the 768-cell board (the mask scan, `view_board()`'s own
   classification pass, `render_board_now()`'s tilemap copy, and
   `view_hud()`'s two separate `board_count()` calls), and this multiply
   dominated every one of them.
2. **The mask was being recomputed on every rebuild, not once per turn.**
   `rules_range_mask()` depends only on the turn's board snapshot and the
   current player, both fixed for the whole turn by `begin_turn()`
   (`src/core/match.c`) — yet `view_board()` was calling it again on
   *every* placement, undo, and `SELECT` toggle within that same turn.

Fix, all in `src/core/`: `board_count()`, `rules_range_mask()`'s scan,
`view_board()`, and `render_board_now()`'s tilemap copy (`src/snes/
render.c`) all hoist a row pointer out of their inner loop (one multiply
per row, none per cell, no `board_get()` call per cell) — e.g. `const u8
*row = &b->c[y + 1][1];` then `row[x]`. `Match` (`src/core/match.h`) gains
a `u8 range_mask[BOARD_H][BOARD_W]` field, computed once by `begin_turn()`
right where the range snapshot itself is taken; `view_board()`, `ai.c`'s
`collect()`, and `match_place()` all read `m->range_mask` directly instead
of calling `rules_in_range()`/`rules_range_mask()` per rebuild. `view_hud()`
now calls a new `board_count_pair()` (one pass counting both colours)
instead of two separate `board_count()` calls. `rules_in_range()` and
`rules_range_mask()` themselves are unchanged in behaviour and still exist
(`rules_in_range()` for `match_place`'s pre-cache path in tests,
`rules_range_mask()` for `begin_turn()`'s once-per-turn call).

New equivalence tests (`tests/test_match.c`) assert `m->range_mask` equals
`rules_in_range()` cell-by-cell after `match_start()`, after a P1→P2 turn
switch, and after a tick; new pinning tests (`tests/test_board.c`) check
`board_count()`/the new `board_count_pair()` against an independent,
`board_get()`-based tally on `board_seed()` and pseudo-random boards
(in-test xorshift32, not `rand()`). `make test` went from 19761 → 22074
checks, all passing (see the fix-round-1 report for the exact RED/GREEN
transcript). `make test` and `PVSNESLIB_HOME=/nonexistent
make test` both stay pristine; `make rom`/`make rom-script` build clean
(one more 816-tcc-only quirk: passing `m->range_mask`, read through a
`const Match *`, straight into a function expecting `const u8
(*)[BOARD_W]` triggered a spurious "assignment from incompatible pointer
type" warning that GCC/Clang never raised — worked around with an explicit
`(const u8 (*)[BOARD_W])` cast at the call site in `ai.c`, no behaviour
change).

Re-measured the same way (bisecting headless captures around the same
`SELECT` toggle, range-dot pixel count as the signal), on the rebuilt
`build/life-script.sfc`:

- toggle-off completes at raw frame **713**; toggle-on completes at raw
  frame **735** — a total delta of 22 raw frames, of which up to 8 are
  simply the idle gap between the two scripted key-presses, leaving
  **~14 raw frames** unaccounted for.
- Cross-check: the first placement (also an ordinary dirty rebuild, and
  never a mask recompute either, since placements don't call
  `begin_turn()`) completes at raw frame **757**, landing the same ~14
  raw frames past its naive call-index prediction (735 + 8-call gap =
  743) — the same residual both times.
- Because `SELECT` no longer triggers *any* mask computation (the mask is
  only ever computed once, at the turn boundary), toggling range display
  on and off are now, algorithmically, **the same rebuild** — the ~14-raw-
  frame residual measured on both the toggle-on and the first-placement
  transition is therefore best read as the current *range-independent*
  baseline cost of a dirty rebuild (the row-pointer-hoisted `view_board()`
  pass, `render_board_now()`'s tilemap copy, and `board_count_pair()`,
  one pass each), not as a cost specific to showing the range.
- Net: **~350 → ~42 → ~14 raw frames**, roughly a further 3× reduction on
  top of the dilation fix (an ~25× reduction from the original defect),
  turning a ~5.8 s freeze into ~0.23 s. This is close to, but not quite
  under, the 10-raw-frame target set for this round; the three remaining
  full-board passes above are the next candidate if a further reduction
  is ever needed. Screenshots captured for this measurement are not
  archived in this repo.

## 9. Task 11 — the CPU turn: ~1900 raw frames at first, ~110 now

### 9.1 The first measurement: ~1900 frames for one blocking `ai_choose()`

Task 11 first wired `ai_choose()` (`src/core/ai.c`) into the game loop as
one blocking call, preceded by a "thinking" indicator (red HUD icon off,
cursor hidden). The task brief asked to measure that call before deciding
whether to make it resumable (threshold: 30 raw frames).

**Method**: a build with `AI_MEASURE_FRAMES` read the free-running
`snes_vblank_count` (declared in `snes/console.h`, incremented by
PVSnesLib's NMI handler on every real VBlank, whatever the main loop is
doing) before and after the call. On the scripted-input ROM (§2), with
`cpu_level = AI_NORMAL`, headless captures were bisected around the moment
the blue player's scripted turn hands over to the CPU: the screen stayed
frozen from raw frame 917 to raw frame 2822, and changed at 2824. **About
1900 raw frames, roughly 32 s**, for a turn of 89 candidate evaluations
(29 + 29 + 31, from a host replay of the same script).

The cost: a 9 x 9 window loaded and ticked twice (with and without the
candidate), per candidate, per placement, in 816-tcc output on a 65816 that
runs at about 2.68 MHz effective (SlowROM, 8 master cycles per memory
access), that is about 44 700 CPU cycles per NTSC frame.

### 9.2 Fix round 1 — what the time went to, and what was changed

`make rom-measure` (opt-in, never part of `make rom`) builds
`build/life-measure.sfc`: the scripted input of `rom-script` plus the
`AI_MEASURE_FRAMES` counters in `src/snes/main.c`. After each CPU turn the
HUD shows, in place of the round counter, the turn's length in real
VBlanks (from `ai_begin()` to the `ai_step()` that returns `TRUE`) in
columns 12-15 and the number of main-loop iterations of that turn in
columns 16-19. Frames close to iterations mean an iteration fits in a
frame. The first CPU turn of the script ends before raw frame 1150.

Measured on that turn, step by step (raw frames per CPU turn):

| State | Frames |
|---|---|
| Before (blocking call) | ~1900 |
| Window tick unrolled (life_tick's pointer pattern) and limited to its light cone; window loaded once through wrap tables; candidate collection by row pointer, halo and ring scan | 508 |
| Second pass recomputes only the placed cell's cone | 425 |
| Redesign below, first version, iterations not yet fitted to a frame | 247 |
| Final (below), `AI_STEP_BUDGET` 90 | **110 (100 iterations)** |

The window-based evaluation stayed near three frames per candidate even
after these changes. An on-console micro-benchmark (60 repetitions each)
gave: one depth-2 evaluation 2.9 frames, one window load 0.6 frame (about
300 cycles per cell), one 7 x 7 tick 0.85 frame (about 780 cycles per
cell). 816-tcc spends around ten instructions per memory access (pointer
reloaded from the stack, `sep`/`rep` around every byte), so no per-candidate
simulation of a 9 x 9 window fits in a frame. A coded window with sliding
column sums gained only 425 -> 415 frames and was not kept.

**The redesign** (`src/core/ai.c`, overview at the top of the file):

- The board without the candidate is the same for every candidate of a
  placement. Its next two generations and the neighbour sums behind them
  (g1, s1, g2, s2) are computed once per placement, on the rows and
  columns within 3 (g1) or 2 (g2) of the retained candidates.
- Each candidate is then evaluated incrementally: tick 1 "with" on its 9
  cells from s1; the changes against g1 are added to the s2 sums of their
  neighbours; tick 2 "with" is computed only on the touched cells and
  compared with g2. About 0.37 frame per candidate instead of 2.9.
- The work board is coded (blue 1, red 16): the sum of eight neighbours
  carries both counts, and the rule is read from two tables built from
  `LIFE_RULE`.
- Candidates are searched only among the empty in-range cells listed once
  per turn; placements 2 and 3 update the candidate list only in the 3 x 3
  of the previous placement. The pull towards the opponent is separable by
  rows (4 - max(|dx|, |dy|) = min(4 - |dx|, 4 - |dy|)): one map per turn,
  seven reads per candidate.
- `board_wrap()` walks rows by pointer (it multiplied by 34 on every
  access before).

Choices are unchanged: the pinned-choice test (`tests/test_ai.c`, recorded
on the original implementation), the full-board equivalence test and the
step-by-one test guard it; a differential run against the original
`ai.c` (40 000 `ai_choose()` calls and 480 000 `ai_eval_local()` calls on
random boards) found no difference.

Work per phase on that turn, measured by running each phase blocking
(about +-1 frame per block): copy 9, candidate collection 4, list updates
3, selection 7, generations 18, evaluations 33 — about 77 frames of work.

**Spreading it over frames.** `ai_step()` takes a budget in units of about
1/100 frame; each step (a board row copied, four open cells examined, a
third of a list update, half of the selection, a generation row, a
candidate, a placement) has a measured cost in `src/core/ai.h`. A call
always does its first step, then only the steps that still fit. Whole-turn
results: budget 60: 246 frames / 230 iterations; 80: 114 / 105; 90: 110 /
100; 100: 112 / 92. `AI_STEP_BUDGET` is 90: about one iteration in ten
runs one frame over. Two captures 16 frames apart during the CPU turn
(raw frames 940 and 956 on `build/life-script.sfc`, then 972) show the red
HUD icon on, off, on, with the board unchanged and the cursor hidden.

### 9.3 Measurement pitfalls met on the way

- Timing a step by the change of `snes_vblank_count` around it is biased:
  a step always starts just after a VBlank, so a step shorter than a frame
  counts 0. Time whole phases or whole turns instead.
- Reading the V counter through the latch gave inconsistent values here;
  it was not used.
- Redrawing four-digit numbers on the HUD every frame cost about a quarter
  of a frame (software division by 10). The measurement build redraws them
  only when they change.
- Statics without an initializer did not start at zero in this build (the
  counters showed 9999): initialize them explicitly.
- Headless captures past about 500 frames hung while the host display was
  asleep, even for an unchanged ROM. Wake the display and keep it awake
  (for example `caffeinate -u`) before a long capture.
