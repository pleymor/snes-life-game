#ifndef BOARD_HDR
#define BOARD_HDR

#include "config.h"
#include "types.h"

/* Le stockage porte un halo d'une case : jeu (x, y) vit en c[y + 1][x + 1].
   Le halo est la copie du bord opposé, ce qui rend le tore gratuit pour le
   comptage des voisins : aucun test de bord, aucun modulo. */
#define BSTRIDE (BOARD_W + 2)

typedef struct { u8 c[BOARD_H + 2][BOARD_W + 2]; } Board;

void board_clear(Board *b);
void board_seed(Board *b);   /* position de départ, spec § 2.4 */
void board_wrap(Board *b);   /* recopie le halo depuis les bords opposés */
Cell board_get(const Board *b, int x, int y);
void board_set(Board *b, int x, int y, Cell v);
int  board_count(const Board *b, Cell who);

#endif
