#ifndef RULES_H
#define RULES_H

#include "board.h"

/* Une cellule de `player` existe-t-elle à distance de Chebyshev torique
   inférieure ou égale à RANGE_RADIUS de (x, y) ? */
bool_t rules_in_range(const Board *b, Cell player, int x, int y);

/* Case vide et à portée. Le budget du tour n'entre pas ici : c'est `match`
   qui le tient. */
bool_t rules_can_place(const Board *b, Cell player, int x, int y);

#endif
