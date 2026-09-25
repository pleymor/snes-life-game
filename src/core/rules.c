#include "rules.h"

/* Le halo ne fait qu'une case alors que la portée en fait deux : le
   bouclage se refait donc explicitement ici, via les macros de board.h. */
bool_t rules_in_range(const Board *b, Cell player, int x, int y)
{
    int dx, dy;
    for (dy = -RANGE_RADIUS; dy <= RANGE_RADIUS; dy++) {
        for (dx = -RANGE_RADIUS; dx <= RANGE_RADIUS; dx++) {
            if (board_get(b, BOARD_WRAP_X(x + dx), BOARD_WRAP_Y(y + dy)) == player) {
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
