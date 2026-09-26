#ifndef AI_H
#define AI_H

#include "match.h"

typedef enum { AI_EASY = 0, AI_NORMAL = 1 } AiLevel;

/* rng : état d'un xorshift32 possédé par l'appelant, jamais nul.
   Ignoré au niveau normal, qui est purement déterministe.
   Rend le nombre de coups écrits dans `out`, de 0 à BUDGET. */
int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET]);

/* Un tour d'IA reprenable, étalé sur plusieurs appels à ai_step() pour que
   la boucle de jeu continue d'animer l'écran pendant la réflexion (spec
   § 6.3). `lvl` ne figurait pas dans la structure prévue : ai_step() en a
   besoin pour choisir entre le départage aléatoire (AI_EASY) et le
   meilleur score (AI_NORMAL) sans le redemander à chaque appel.

   Un seul AiJob à la fois : le plateau de travail, les candidats et leurs
   scores sont des tableaux statiques d'ai.c, partagés. Démarrer un second
   tour (ou appeler ai_choose()) avant la fin du premier corrompt celui-ci. */
typedef struct {
    Cell    me, foe;
    AiLevel lvl;
    int     depth, k;
    int     row;    /* ligne suivante à balayer ; BOARD_H : collecte finie */
    int     n;      /* candidats rassemblés pour le coup courant */
    int     top;    /* candidats retenus (au plus k) une fois la collecte finie */
    int     i;      /* candidat en cours d'évaluation */
    int     made;
    Move    out[BUDGET];
} AiJob;

/* Coût de chaque étape d'ai_step(), en unités de budget. Une unité vaut à
   peu près le balayage d'une ligne du plateau à la recherche de
   candidats ; les rapports viennent de mesures sur la console
   (docs/snes-notes.md § 9). Un appel fait toujours au moins une étape, et
   s'arrête dès que la somme des coûts atteint le budget. */
#define AI_COST_ROW    1
#define AI_COST_EVAL   4
#define AI_COST_PICK   2

/* Démarre un tour d'IA : fige et wrappe le plateau de travail. Ne balaie
   rien : la collecte des candidats se fait ligne par ligne dans ai_step(). */
void ai_begin(AiJob *j, const Match *m, AiLevel lvl);

/* Avance le tour d'au moins une étape, et tant que `budget` n'est pas
   épuisé : une ligne de collecte (AI_COST_ROW), une évaluation de
   candidat (AI_COST_EVAL), ou la pose du meilleur candidat
   (AI_COST_PICK). TRUE quand tout est fini : `j->out`/`j->made` sont alors
   le résultat complet, au format de ai_choose(). Le choix ne dépend pas
   du découpage en appels. */
bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget);

/* Gain net que produit la pose de `who` en (x, y) après `depth` ticks :
   cellules gagnées par `who` moins celles gagnées par l'adversaire, mesuré
   sur le carré de rayon `depth` autour de la pose. Exposé pour le test
   d'équivalence, qui est la garantie de correction de l'optimisation. */
int ai_eval_local(const Board *b, Cell who, int x, int y, int depth);

#endif
