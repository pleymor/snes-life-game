#ifndef LIFE_H
#define LIFE_H

#include "board.h"

/* `in` doit avoir été wrappé. `out` l'est en sortie, prêt pour le tick
   suivant. `in` et `out` doivent désigner des plateaux distincts. */
void life_tick(const Board *in, Board *out);

#endif
