# PVSnesLib toolchain spike — notes

Task 0 output. Records the exact, verified calls tasks 7-12 need. Everything
under "Verified end-to-end" was actually built, run in an emulator, and
confirmed on a captured screenshot (see `.superpowers/sdd/2026-09-14-immigration/task-0-report.md`
for the image). Everything under "Header-verified, not exercised here" is
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
shows the loaded tiles and palette rendering correctly — see
`.superpowers/sdd/2026-09-14-immigration/task-8-report.md`.

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

**Headless screenshot timing caveat, sharpened**: section 2's
`--max-frames=N` recipe is confirmed to need N well past 400 in practice —
RetroArch's own "content loaded" notification banner was still visible at
`--max-frames=40` in this task's testing (it cleared by 400). More
importantly, **the raw RetroArch frame count is *not* a reliable predictor
of the ROM's own internal frame counter** (a `static int frame` incremented
once per loop iteration in `main.c`): the two counters are related but not
by a fixed additive offset — some pairs of raw frame counts exactly one
half-period apart (`--max-frames=400` vs `416`, for a 32-frame blink
period) landed on the *same* blink phase instead of the expected opposite
one, most likely from a slightly-longer-than-one-frame first loop iteration
(the initial full board render) permanently shifting the phase alignment
between "raw frames elapsed since power-on" and "loop iterations executed
so far", plus the constant startup overhead before `main()`'s loop even
starts. **Consequence for later tasks**: don't compute two `--max-frames`
values from the blink formula and assume they land on opposite phases —
run a quick sweep (a handful of candidate frame counts, sampled
programmatically) and confirm the two states differ before trusting them as
"the two blink phases". Task 9 used `--max-frames=800` (pip on) and
`--max-frames=1200` (pip off), found this way, not the naively-computed
400/416.
