/*---------------------------------------------------------------------------------

    Task 0 spike: minimal PVSnesLib skeleton.

    Adapted from the pvsneslib `hello_world` example. Proves the exact
    mechanism task 7 needs: a 32x24 tilemap filled from a plain C array at
    runtime and pushed to VRAM with a raw DMA call (dmaCopyVram), instead of
    a tilemap baked into the ROM by the asset pipeline. Also exercises the
    other primitives tasks 7-12 depend on: mode select, sprite placement,
    controller reads and vblank sync. See docs/snes-notes.md for the exact,
    verified call for every one of these.

    Do not rewrite this file wholesale in later tasks: extend it.

---------------------------------------------------------------------------------*/
#include <snes.h>

#define MAP_WIDTH 32
#define MAP_HEIGHT 24

// The tilemap lives in normal C memory and is (re)computed at runtime, then
// DMA'd to VRAM. This is the mechanism task 7 needs for the game board.
u16 tilemap[MAP_WIDTH * MAP_HEIGHT];

// Hand-authored 4bpp sprite tile (one 8x8 tile, SNES planar format: 32
// bytes/tile). Every pixel is set to color index 1 (bit0 of every row set,
// bits 1-3 clear) so the sprite is a solid square using palette entry 1.
const u8 spriteTile[32] = {
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 16-color sprite palette (BGR555, 2 bytes/color). Index 0 is transparent
// for sprites; index 1 is opaque red (used by spriteTile above).
const u16 spritePalette[16] = {
    0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

//---------------------------------------------------------------------------------
void fillTilemap(void)
{
    u16 x, y;
    for (y = 0; y < MAP_HEIGHT; y++)
    {
        for (x = 0; x < MAP_WIDTH; x++)
        {
            // Arbitrary procedural pattern: what is on screen does not
            // matter, only that it came from this loop and not from a
            // linked-in binary blob.
            tilemap[y * MAP_WIDTH + x] = ((x + y) & 1) ? 1 : 0;
        }
    }
}

//---------------------------------------------------------------------------------
int main(void)
{
    // Console + default tileset/palette init: reserves BG0, loads the
    // font shipped with pvsneslib as the BG0 tile graphics (VRAM $3000)
    // and its palette, and points the BG0 map at VRAM $6800.
    consoleInitDefaultText(0);

    // Re-affirm BG0 graphics/map VRAM pointers (matches consoleInitDefaultText
    // defaults; kept explicit because later tasks will change these).
    bgSetGfxPtr(0, 0x3000);
    bgSetMapPtr(0, 0x6800, SC_32x32);

    // Graphics mode: mode 1, 16-color BG0/BG1, BG1/BG2 unused for this spike.
    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);

    // Build the 32x24 tilemap in plain C memory, then DMA it to the BG0 map
    // address in one shot. This is the exact call task 7 needs.
    fillTilemap();
    dmaCopyVram((u8 *)tilemap, 0x6800, sizeof(tilemap));

    // Load one hand-authored sprite tile + its palette into VRAM and place it.
    oamInitGfxSet((u8 *)spriteTile, sizeof(spriteTile), (u8 *)spritePalette, sizeof(spritePalette), 0, 0x0000, OBJ_SIZE8_L16);
    oamSet(0, 100, 100, 3, 0, 0, 0, 0);
    oamSetEx(0, OBJ_SMALL, OBJ_SHOW);

    setScreenOn();

    while (1)
    {
        // Read pad 0 every frame; not acted upon here, just proven to work.
        u16 pad0 = padsCurrent(0);
        (void)pad0;

        WaitForVBlank();
    }
    return 0;
}
