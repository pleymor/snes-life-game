/*---------------------------------------------------------------------------------

    Task 0 spike, extended by task 8: link src/core/ into the ROM and draw
    the board on BG1.

    render_init()/render_board_now() (src/snes/render.c) now own every
    PVSnesLib call this file used to make directly (mode select, tileset +
    palette load, tilemap DMA) — see docs/snes-notes.md for the exact,
    verified sequence. This file only drives the game loop.

    Do not rewrite this file wholesale in later tasks: extend it.

---------------------------------------------------------------------------------*/
#include <snes.h>
#include "match.h"
#include "render.h"

/* A Board is 884 bytes; Match holds two of them. The SNES stack is tiny, so
   this lives in static storage, never on the stack. */
static Match m;

//---------------------------------------------------------------------------------
int main(void)
{
    render_init();

    match_start(&m);
    render_board_now(&m, FALSE);

    while (1)
    {
        WaitForVBlank();
    }
    return 0;
}
