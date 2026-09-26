#ifndef AI_H
#define AI_H

#include "match.h"

typedef enum { AI_EASY = 0, AI_NORMAL = 1 } AiLevel;

/* rng : état d'un xorshift32 possédé par l'appelant, jamais nul.
   Ignoré au niveau normal, qui est purement déterministe.
   Rend le nombre de coups écrits dans `out`, de 0 à BUDGET. */
int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET]);

/* Étapes d'un tour (AiJob.phase), dans l'ordre : copie codée du plateau
   (une fois par tour), puis pour chaque pose : collecte des candidats,
   générations sans la pose autour des candidats retenus (g1 puis g2),
   évaluation des candidats, pose du meilleur. */
enum { AI_PH_PREP, AI_PH_COLLECT, AI_PH_GEN1, AI_PH_GEN2, AI_PH_EVAL };

/* Un tour d'IA reprenable, étalé sur plusieurs appels à ai_step() pour que
   la boucle de jeu continue d'animer l'écran pendant la réflexion (spec
   § 6.3). `lvl` ne figurait pas dans la structure prévue : ai_step() en a
   besoin pour choisir entre le départage aléatoire (AI_EASY) et le
   meilleur score (AI_NORMAL) sans le redemander à chaque appel.

   Un seul AiJob à la fois : le plateau de travail, les candidats, leurs
   scores et les générations précalculées sont des tableaux statiques
   d'ai.c, partagés (ai_eval_local() et ai_choose() compris). Démarrer un
   second tour avant la fin du premier corrompt celui-ci. */
typedef struct {
    Cell    me, foe;
    AiLevel lvl;
    int     depth, k;
    int     phase;  /* AI_PH_... */
    int     row;    /* ligne suivante de l'étape en cours */
    int     n;      /* candidats rassemblés pour la pose courante */
    int     top;    /* candidats retenus (au plus k), collecte finie */
    int     i;      /* candidat en cours d'évaluation */
    int     made;
    Move    out[BUDGET];
} AiJob;

/* Coût de chaque étape d'ai_step(), en unités de budget. Une unité vaut à
   peu près le balayage d'une ligne du plateau à la recherche de
   candidats ; les rapports viennent de mesures sur la console
   (docs/snes-notes.md § 9). Un appel fait toujours au moins une étape, et
   s'arrête dès que la somme des coûts atteint le budget. */
#define AI_COST_PREP   1   /* une ligne de la copie codée */
#define AI_COST_ROW    1   /* une ligne de collecte */
#define AI_COST_GEN    1   /* une ligne de g1 ou de g2 */
#define AI_COST_EVAL   1   /* un candidat */
#define AI_COST_PICK   1   /* la pose du meilleur */

/* Démarre un tour d'IA. Ne lit presque rien : tout le travail se fait
   dans ai_step(), étape par étape. */
void ai_begin(AiJob *j, const Match *m, AiLevel lvl);

/* Avance le tour d'au moins une étape, et tant que `budget` n'est pas
   épuisé (voir les AI_COST_...). TRUE quand tout est fini : `j->out`/
   `j->made` sont alors le résultat complet, au format de ai_choose(). Le
   choix ne dépend pas du découpage en appels. */
bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget);

/* Gain net que produit la pose de `who` en (x, y) après `depth` ticks :
   cellules gagnées par `who` moins celles gagnées par l'adversaire, mesuré
   sur le carré de rayon `depth` autour de la pose. Même calcul que celui
   d'ai_step() (générations précalculées autour de la case, puis
   évaluation incrémentale). Exposé pour le test d'équivalence avec une
   simulation du plateau entier, qui est la garantie de correction de
   l'optimisation. */
int ai_eval_local(const Board *b, Cell who, int x, int y, int depth);

#endif
