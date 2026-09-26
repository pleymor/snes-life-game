#include <string.h>
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

/* Dilation : au lieu d'interroger chacune des BOARD_W*BOARD_H cases avec
   rules_in_range (coût en cases*25), on part du masque à zéro et on
   marque, pour chaque cellule de `player` trouvée sur b, le carré qui
   serait à portée d'elle (coût en cellules*25). Le résultat est identique
   par symétrie de la distance de Chebyshev : (x,y) est à portée d'une
   cellule de `player` en (px,py) ssi (px,py) est dans le carré de
   RANGE_RADIUS autour de (x,y), ce qui est aussi vrai en échangeant les
   rôles de (x,y) et (px,py). */
void rules_range_mask(const Board *b, Cell player, u8 mask[BOARD_H][BOARD_W])
{
    int px, py, dx, dy;

    memset(mask, 0, BOARD_H * BOARD_W * sizeof(u8));

    for (py = 0; py < BOARD_H; py++) {
        /* Pointeur de ligne hoisté hors de la boucle sur px : une seule
           multiplication (BSTRIDE) par ligne au lieu d'un board_get() (et
           donc une multiplication) par case (fix round 1 — perf review,
           docs/snes-notes.md § 8). */
        const u8 *row = &b->c[py + 1][1];
        for (px = 0; px < BOARD_W; px++) {
            if (row[px] != (u8)player) {
                continue;
            }
            for (dy = -RANGE_RADIUS; dy <= RANGE_RADIUS; dy++) {
                int wy = BOARD_WRAP_Y(py + dy);
                for (dx = -RANGE_RADIUS; dx <= RANGE_RADIUS; dx++) {
                    int wx = BOARD_WRAP_X(px + dx);
                    mask[wy][wx] = 1;
                }
            }
        }
    }
}
