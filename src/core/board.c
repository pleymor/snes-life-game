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

/* Un pointeur de ligne hoisté hors de la boucle sur x : une seule
   multiplication (l'adresse de la ligne, via BSTRIDE) par ligne, plus
   aucune par case (b->c[y][x] indexé directement multiplie par BSTRIDE=34
   à chaque accès, 34 n'étant pas une puissance de deux). */
int board_count(const Board *b, Cell who)
{
    int x, y, n = 0;
    for (y = 1; y <= BOARD_H; y++) {
        const u8 *row = &b->c[y][1];
        for (x = 0; x < BOARD_W; x++) {
            if (row[x] == (u8)who) {
                n++;
            }
        }
    }
    return n;
}

void board_count_pair(const Board *b, int *p1, int *p2)
{
    int x, y, c1 = 0, c2 = 0;
    for (y = 1; y <= BOARD_H; y++) {
        const u8 *row = &b->c[y][1];
        for (x = 0; x < BOARD_W; x++) {
            u8 v = row[x];
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

/* Par pointeur de ligne, comme board_count() : b->c[y][x] indexé
   directement multiplie par BSTRIDE (34) à chaque accès sur la console, et
   board_wrap() est appelée plusieurs fois par tour de l'IA. */
void board_wrap(Board *b)
{
    u8 *row = &b->c[1][0];
    int y;
    /* Colonnes d'abord, sur les seules lignes de jeu... */
    for (y = 1; y <= BOARD_H; y++) {
        row[0] = row[BOARD_W];
        row[BOARD_W + 1] = row[1];
        row += BSTRIDE;
    }
    /* ...puis les lignes sur toute la largeur, ce qui remplit les coins. */
    memcpy(&b->c[0][0], &b->c[BOARD_H][0], BSTRIDE);
    memcpy(&b->c[BOARD_H + 1][0], &b->c[1][0], BSTRIDE);
}

/* Spec § 2.4. Le joueur 2 est l'image du joueur 1 par (x,y) -> (31-x, 23-y).
   Bloc 2x2 : immortel tant qu'on ne le dérange pas.
   Planeur : se déplace vers le centre du plateau.
   Paires (x, y) à plat, en portée fichier : 816-tcc ne place en ROM que
   les tables `static const` de fichier à une dimension ; une table locale
   à une fonction ou à deux dimensions part en RAM, recopiée depuis la ROM
   au démarrage (docs/snes-notes.md § 10). */
static const u8 seed_p1[2 * 9] = {
    6, 14,  7, 14,  6, 15,  7, 15,          /* bloc */
    7,  6,  8,  7,  6,  8,  7,  8,  8,  8   /* planeur */
};

void board_seed(Board *b)
{
    int i;

    board_clear(b);
    for (i = 0; i < 9; i++) {
        board_set(b, seed_p1[2 * i], seed_p1[2 * i + 1], CELL_P1);
        board_set(b, BOARD_W - 1 - seed_p1[2 * i], BOARD_H - 1 - seed_p1[2 * i + 1], CELL_P2);
    }
    board_wrap(b);
}
