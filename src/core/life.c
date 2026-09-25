#include "life.h"

void life_tick(const Board *in, Board *out)
{
    int x, y;

    for (y = 1; y <= BOARD_H; y++) {
        for (x = 1; x <= BOARD_W; x++) {
            const u8 *p = &in->c[y - 1][x - 1];
            u8 self, v;
            int n = 0;    /* voisins vivants, toutes couleurs */
            int n1 = 0;   /* ceux qui sont bleus */

            v = p[0];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[1];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[BSTRIDE];        if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[BSTRIDE + 2];    if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * BSTRIDE];    if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * BSTRIDE + 1];if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * BSTRIDE + 2];if (v) { n++; if (v == CELL_P1) n1++; }

            self = in->c[y][x];
            if (self) {
                out->c[y][x] = (n == 2 || n == 3) ? self : (u8)CELL_EMPTY;
            } else {
                /* Trois parents se répartissent en 3-0 ou 2-1 : la majorité
                   est toujours tranchée. */
                out->c[y][x] = (n == 3)
                    ? (u8)(n1 >= 2 ? CELL_P1 : CELL_P2)
                    : (u8)CELL_EMPTY;
            }
        }
    }
    board_wrap(out);
}
