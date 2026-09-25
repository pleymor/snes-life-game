#ifndef LIFE_H
#define LIFE_H

#include "board.h"

/* Règle de l'Immigration Game : prochaine valeur d'une cellule qui vaut
   `self`, avec `n` voisins vivants (toutes couleurs) dont `n1` bleus
   (CELL_P1). Survie à 2 ou 3 voisins ; naissance à exactement 3, dans la
   couleur majoritaire (3-0 ou 2-1, la majorité est toujours tranchée). */
#define LIFE_RULE(self, n, n1) \
    ((self) ? (((n) == 2 || (n) == 3) ? (self) : (u8)CELL_EMPTY) \
            : (((n) == 3) ? (u8)((n1) >= 2 ? CELL_P1 : CELL_P2) : (u8)CELL_EMPTY))

/* `in` doit avoir été wrappé. `out` l'est en sortie, prêt pour le tick
   suivant. `in` et `out` doivent désigner des plateaux distincts. */
void life_tick(const Board *in, Board *out);

#endif
