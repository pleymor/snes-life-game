#include <string.h>
#include "board.h"

void board_clear(Board *b)
{
    memset(b->c, CELL_EMPTY, sizeof b->c);
}

Cell board_get(const Board *b, int x, int y)
{
    return (Cell)b->c[y + 1][x + 1];
}

void board_set(Board *b, int x, int y, Cell v)
{
    b->c[y + 1][x + 1] = (u8)v;
}

int board_count(const Board *b, Cell who)
{
    int x, y, n = 0;
    for (y = 1; y <= BOARD_H; y++) {
        for (x = 1; x <= BOARD_W; x++) {
            if (b->c[y][x] == (u8)who) {
                n++;
            }
        }
    }
    return n;
}

/* Les deux couleurs en un seul passage des BOARD_H*BOARD_W cases : évite
   à un appelant qui veut les deux comptes (view_hud) de balayer le
   plateau deux fois de suite pour ça (fix round 1, § c). */
void board_count_pair(const Board *b, int *p1, int *p2)
{
    int x, y, c1 = 0, c2 = 0;
    for (y = 1; y <= BOARD_H; y++) {
        for (x = 1; x <= BOARD_W; x++) {
            u8 v = b->c[y][x];
            if (v == (u8)CELL_P1) {
                c1++;
            } else if (v == (u8)CELL_P2) {
                c2++;
            }
        }
    }
    *p1 = c1;
    *p2 = c2;
}

void board_wrap(Board *b)
{
    int x, y;
    /* Colonnes d'abord, sur les seules lignes de jeu... */
    for (y = 1; y <= BOARD_H; y++) {
        b->c[y][0] = b->c[y][BOARD_W];
        b->c[y][BOARD_W + 1] = b->c[y][1];
    }
    /* ...puis les lignes sur toute la largeur, ce qui remplit les coins. */
    for (x = 0; x <= BOARD_W + 1; x++) {
        b->c[0][x] = b->c[BOARD_H][x];
        b->c[BOARD_H + 1][x] = b->c[1][x];
    }
}

void board_seed(Board *b)
{
    /* Spec § 2.4. Le joueur 2 est l'image du joueur 1 par (x,y) -> (31-x, 23-y).
       Bloc 2x2 : immortel tant qu'on ne le dérange pas.
       Planeur : se déplace vers le centre du plateau. */
    static const u8 p1[9][2] = {
        { 6, 14 }, { 7, 14 }, { 6, 15 }, { 7, 15 },          /* bloc */
        { 7,  6 }, { 8,  7 }, { 6,  8 }, { 7,  8 }, { 8, 8 } /* planeur */
    };
    int i;

    board_clear(b);
    for (i = 0; i < 9; i++) {
        board_set(b, p1[i][0], p1[i][1], CELL_P1);
        board_set(b, BOARD_W - 1 - p1[i][0], BOARD_H - 1 - p1[i][1], CELL_P2);
    }
    board_wrap(b);
}
