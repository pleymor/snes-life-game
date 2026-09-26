#ifndef RULES_H
#define RULES_H

#include "board.h"

/* Une cellule de `player` existe-t-elle à distance de Chebyshev torique
   inférieure ou égale à RANGE_RADIUS de (x, y) ? */
bool_t rules_in_range(const Board *b, Cell player, int x, int y);

/* Case vide et à portée. Le budget du tour n'entre pas ici : c'est `match`
   qui le tient. */
bool_t rules_can_place(const Board *b, Cell player, int x, int y);

/* Équivalent de rules_in_range appliqué à chaque case du plateau, mais
   par dilation plutôt que par balayage cellule par cellule : mask[y][x]
   vaut 1 si et seulement si rules_in_range(b, player, x, y) serait vrai,
   0 sinon. Le masque est d'abord mis à zéro, puis, pour chaque cellule de
   `player` sur b, le carré de (2*RANGE_RADIUS+1) côté qui l'entoure est
   marqué, replié sur le tore via BOARD_WRAP_X/Y. Coût proportionnel au
   nombre de cellules de `player` fois (2*RANGE_RADIUS+1)^2, au lieu de
   BOARD_W*BOARD_H fois ce même facteur. `mask` est fourni par l'appelant
   (jamais alloué ici) : rien de plus de 64 octets ne doit être automatique. */
void rules_range_mask(const Board *b, Cell player, u8 mask[BOARD_H][BOARD_W]);

#endif
