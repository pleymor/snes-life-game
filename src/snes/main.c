/*---------------------------------------------------------------------------------

    Task 0 spike, extended by task 8 (board on BG1) and task 9 (HUD on BG2,
    cursor sprite).

    render_init()/render_board_now()/render_hud_now()/render_cursor()
    (src/snes/render.c) own every PVSnesLib call this file used to make
    directly (mode select, tileset + palette load, DMA, OAM) — see
    docs/snes-notes.md for the exact, verified sequence. This file only
    drives the game loop.

    VBlank discipline: render_board_now()/render_hud_now()/render_cursor()
    only prepare buffers; render_vblank() (called after WaitForVBlank(),
    never before) performs the actual DMA transfers. The loop below is
    exactly prepare -> WaitForVBlank() -> render_vblank(), never more than
    one wait per frame.

    Do not rewrite this file wholesale in later tasks: extend it.

---------------------------------------------------------------------------------*/
#include <snes.h>
#include "match.h"
#include "render.h"

/* A Board is 884 bytes; Match holds two of them. The SNES stack is tiny, so
   this lives in static storage, never on the stack. */
static Match m;

/* The board is only redrawn at startup and whenever a placement, an undo,
   or the end of a turn changes it. Nothing sets this flag yet besides
   startup in this task; task 10 sets it after every such change. The HUD
   is cheap (one tilemap row) and blinks, so it is prepared every frame
   regardless. The cursor is hard-coded at the board centre until task 10
   wires up movement. */
static bool_t board_dirty = TRUE;

//---------------------------------------------------------------------------------
int main(void)
{
    /* unsigned : `frame & 16` only ever needs the low bits, and a plain
       (signed) `int` would overflow undefined behaviour past 32767 on the
       16-bit `int` of the 65816 target (fix round 1, review finding). */
    unsigned int frame = 0;

    render_init();
    match_start(&m);

    while (1)
    {
        if (board_dirty) {
            render_board_now(&m, FALSE);
            board_dirty = FALSE;
        }
        render_hud_now(&m, (bool_t)((frame & 16) != 0));
        render_cursor(BOARD_W / 2, BOARD_H / 2, TRUE);

        frame++;
        WaitForVBlank();
        render_vblank();
    }
    return 0;
}
