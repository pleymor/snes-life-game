#include "view.h"

void view_board(const Match *m, bool_t show_range, u8 out[BOARD_H][BOARD_W])
{
    int x, y;
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            Cell v = board_get(&m->board, x, y);
            if (v == CELL_P1) {
                out[y][x] = TILE_P1;
            } else if (v == CELL_P2) {
                out[y][x] = TILE_P2;
            } else if (show_range &&
                       rules_in_range(&m->range, m->turn, x, y)) {
                out[y][x] = TILE_RANGE;
            } else {
                out[y][x] = TILE_EMPTY;
            }
        }
    }
}

void view_digits3(int value, u8 out[3])
{
    if (value < 0)   value = 0;
    if (value > 999) value = 999;
    out[0] = (u8)(TILE_DIGIT0 + value / 100);
    out[1] = (u8)(TILE_DIGIT0 + (value / 10) % 10);
    out[2] = (u8)(TILE_DIGIT0 + value % 10);
}

static void pips(const Match *m, Cell who, u8 *out)
{
    int left = (m->turn == who) ? (BUDGET - m->placed) : BUDGET;
    int i;
    for (i = 0; i < BUDGET; i++) {
        out[i] = (u8)(i < left ? TILE_PIP_ON : TILE_PIP_OFF);
    }
}

void view_hud(const Match *m, u8 out[HUD_W])
{
    int i;
    int ticks = match_ticks_this_round(m);

    for (i = 0; i < HUD_W; i++) {
        out[i] = TILE_EMPTY;
    }

    out[0]  = TILE_P1;
    out[31] = TILE_P2;
    view_digits3(board_count(&m->board, CELL_P1), &out[2]);
    view_digits3(board_count(&m->board, CELL_P2), &out[27]);
    pips(m, CELL_P1, &out[6]);
    pips(m, CELL_P2, &out[23]);

    out[12] = TILE_R;
    view_digits3(m->round, &out[13]);
    out[16] = TILE_SLASH;
    view_digits3(ROUND_CAP, &out[17]);

    if (ticks > 1) {
        out[20] = TILE_TIMES;
        out[21] = (u8)(TILE_DIGIT0 + ticks);
    }
}
