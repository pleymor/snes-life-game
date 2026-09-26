/*---------------------------------------------------------------------------------

    Task 0 spike, extended by task 8 (board on BG1), task 9 (HUD on BG2,
    cursor sprite) and task 10 (controls, two-player game loop).

    render_init()/render_board_now()/render_hud_now()/render_cursor()
    (src/snes/render.c) own every PVSnesLib call this file used to make
    directly (mode select, tileset + palette load, DMA, OAM) — see
    docs/snes-notes.md for the exact, verified sequence. input_init()/
    input_update() (src/snes/input.c) own the pad. This file only drives the
    game loop.

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
#include "input.h"

typedef enum { GS_TURN, GS_RESOLVE, GS_OVER } GameState;

#define RESOLVE_HOLD 45   /* frames de pause pour voir le résultat du tick */

/* A Board is 884 bytes; Match holds two of them. The SNES stack is tiny, so
   this — and Cursor, trivially small but kept alongside for the same
   reason — lives in static storage, never on the stack. */
static Match  m;
static Cursor cur;

/* The small piece of visible state that decides whether the board/HUD need
   rebuilding this frame (task 10 ruling): m.turn, m.round, m.placed,
   m.winner, cur.show_range, and the game state itself (GS_RESOLVE hides the
   board's range marks; returning to GS_TURN shows them again, so the state
   value belongs in the comparison too). Cursor position is deliberately
   excluded: render_cursor() runs unconditionally every frame regardless of
   board_dirty, and the range marks it might overlap do not depend on where
   the cursor sits. */
typedef struct {
    Cell      turn;
    int       round;
    int       placed;
    Winner    winner;
    bool_t    show_range;
    GameState state;
} Snapshot;

static void snapshot_take(Snapshot *s, GameState state)
{
    s->turn       = m.turn;
    s->round      = m.round;
    s->placed     = m.placed;
    s->winner     = m.winner;
    s->show_range = cur.show_range;
    s->state      = state;
}

static bool_t snapshot_differs(const Snapshot *a, const Snapshot *b)
{
    return (bool_t)(a->turn       != b->turn       ||
                     a->round      != b->round      ||
                     a->placed     != b->placed     ||
                     a->winner     != b->winner     ||
                     a->show_range != b->show_range ||
                     a->state      != b->state);
}

//---------------------------------------------------------------------------------
int main(void)
{
    GameState state = GS_TURN;
    /* unsigned : a plain (signed) `int` would overflow undefined behaviour
       past 32767 on the 16-bit `int` of the 65816 target (fix round 1,
       task 8 review finding — carried over here). */
    unsigned int frame = 0;
    int hold = 0;
    /* Forces the very first frame to be seen as "changed": render.c's own
       map_bg1 buffer starts at TILE_EMPTY and is only ever filled by
       render_board_now(), which the snapshot comparison below would
       otherwise never call before any visible state actually changes. */
    bool_t started = FALSE;

    render_init();
    match_start(&m);
    input_init(&cur);

    for (;;) {
        Snapshot before, after;
        bool_t ticked = FALSE;
        bool_t dirty;

        snapshot_take(&before, state);

        if (state == GS_TURN) {
            if (input_update(&cur, &m)) {
                /* Un tick n'a lieu qu'à la fin du tour du second joueur. */
                ticked = (bool_t)(m.turn == CELL_P2);
                match_end_turn(&m);
                if (match_winner(&m) != WINNER_NONE) {
                    state = GS_OVER;
                } else if (ticked) {
                    state = GS_RESOLVE;
                    hold = RESOLVE_HOLD;
                }
            }
        } else if (state == GS_RESOLVE) {
            if (--hold <= 0) state = GS_TURN;
        }

        snapshot_take(&after, state);
        dirty = (bool_t)(!started || snapshot_differs(&before, &after));
        started = TRUE;

        /* Range marks show only during GS_TURN, and only when the cursor's
           own toggle asks for them; both conditions are already part of
           `dirty` above (via `state` and `show_range`), so a change in
           either always reaches render_board_now() with the right value. */
        if (dirty) {
            render_board_now(&m, (bool_t)(state == GS_TURN && cur.show_range));
            render_hud_dirty();
        }
        render_hud_now(&m, (bool_t)((frame & 16) != 0));
        render_cursor(cur.x, cur.y, (bool_t)(state == GS_TURN));

        frame++;
        WaitForVBlank();
        render_vblank();
    }
    return 0;
}
