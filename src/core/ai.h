#ifndef AI_H
#define AI_H

#include "match.h"

typedef enum { AI_EASY = 0, AI_NORMAL = 1 } AiLevel;

/* rng : état d'un xorshift32 possédé par l'appelant, jamais nul.
   Ignoré au niveau normal, qui est purement déterministe.
   Rend le nombre de coups écrits dans `out`, de 0 à BUDGET. */
int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET]);

/* Gain net que produit la pose de `who` en (x, y) après `depth` ticks :
   cellules gagnées par `who` moins celles gagnées par l'adversaire, mesuré
   sur le carré de rayon `depth` autour de la pose. Exposé pour le test
   d'équivalence, qui est la garantie de correction de l'optimisation. */
int ai_eval_local(const Board *b, Cell who, int x, int y, int depth);

#endif
