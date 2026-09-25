#include "rules.h"

/* Le halo ne fait qu'une case alors que la portée en fait deux : le
   bouclage se refait donc explicitement ici.
   Pas de modulo : le 65816 n'a pas de division câblée, et ces fonctions
   sont appelées des dizaines de milliers de fois par tour de CPU. Une
   soustraction conditionnelle suffit tant que le décalage reste inférieur
   à la dimension, ce qui est le cas de tous les appelants. */
static int wrap_x(int x)
{
    if (x < 0)              return x + BOARD_W;
    if (x >= BOARD_W)       return x - BOARD_W;
    return x;
}

static int wrap_y(int y)
{
    if (y < 0)              return y + BOARD_H;
    if (y >= BOARD_H)       return y - BOARD_H;
    return y;
}

bool_t rules_in_range(const Board *b, Cell player, int x, int y)
{
    int dx, dy;
    for (dy = -RANGE_RADIUS; dy <= RANGE_RADIUS; dy++) {
        for (dx = -RANGE_RADIUS; dx <= RANGE_RADIUS; dx++) {
            if (board_get(b, wrap_x(x + dx), wrap_y(y + dy)) == player) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

bool_t rules_can_place(const Board *b, Cell player, int x, int y)
{
    if (board_get(b, x, y) != CELL_EMPTY) {
        return FALSE;
    }
    return rules_in_range(b, player, x, y);
}
