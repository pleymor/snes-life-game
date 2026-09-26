#ifndef AI_H
#define AI_H

#include "match.h"

typedef enum { AI_EASY = 0, AI_NORMAL = 1 } AiLevel;

/* rng : état d'un xorshift32 possédé par l'appelant, jamais nul.
   Ignoré au niveau normal, qui est purement déterministe.
   Rend le nombre de coups écrits dans `out`, de 0 à BUDGET. */
int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET]);

/* Task 11 : un appel à ai_choose() mesuré en jeu réel coûte environ 1900
   frames (docs/snes-notes.md § 9), bien au-delà du seuil de 30 frames fixé
   par la tâche — rendu reprenable ci-dessous. `lvl` n'est pas dans la liste
   du brief mais est indispensable à ai_step() pour choisir entre le
   départage aléatoire (AI_EASY) et le meilleur score (AI_NORMAL) sans le
   redemander à chaque appel ; seul écart au brief, consigné dans le rapport
   de la tâche 11. */
typedef struct {
    Cell    me, foe;
    AiLevel lvl;
    int     depth, k, n, top, i;   /* i : candidat en cours d'évaluation */
    int     made;
    Move    out[BUDGET];
} AiJob;

/* Démarre un tour d'IA : fige le plateau de travail et rassemble/trie les
   candidats du premier coup. Ne consomme aucun budget (le brief ne le
   compte pas dans ai_step() ; voir ai.c, la collecte d'un tour de candidats
   coûte un seul balayage du plateau, sans commune mesure avec le coût des
   32 évaluations ai_eval_local() qui suivent). */
void ai_begin(AiJob *j, const Match *m, AiLevel lvl);

/* Évalue jusqu'à `budget` candidats (le seul coût mesuré comme significatif,
   docs/snes-notes.md § 9), puis rend. TRUE quand tout est fini : `j->out`/
   `j->made` sont alors le résultat complet, au format de ai_choose(). */
bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget);

/* Gain net que produit la pose de `who` en (x, y) après `depth` ticks :
   cellules gagnées par `who` moins celles gagnées par l'adversaire, mesuré
   sur le carré de rayon `depth` autour de la pose. Exposé pour le test
   d'équivalence, qui est la garantie de correction de l'optimisation. */
int ai_eval_local(const Board *b, Cell who, int x, int y, int depth);

#endif
