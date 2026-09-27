#ifndef MATCH_H
#define MATCH_H

#include "board.h"
#include "life.h"
#include "rules.h"

typedef struct {
    Board  board;
    Board  range;         /* instantané pris au début du tour */
    /* rules_range_mask(&range, turn, range_mask) : mis à jour par
       begin_turn() (match.c) en même temps que `range`, jamais recalculé
       ailleurs. match_place(), view_board() et la collecte des candidats
       de l'IA (ai.c : collect_chunk()/collect_update()) le lisent tel quel
       au lieu d'appeler rules_in_range() par case : recalculer un masque
       déjà valable pour tout le tour à chaque pose/annulation/bascule
       serait le même travail refait pour rien. */
    u8     range_mask[BOARD_H][BOARD_W];
    int    round;
    Cell   turn;
    int    placed;
    Move   history[BUDGET];
    Winner winner;
} Match;

/* Position de départ (spec § 2.4), round 1, tour du joueur 1. */
void   match_start(Match *m);

/* Pose une cellule du joueur au tour en (x, y) si la pose est légale (case
   vide, à portée du masque figé en début de tour, budget restant) ; ne
   fait rien et rend FALSE sinon. */
bool_t match_place(Match *m, int x, int y);

/* Annule la dernière pose du tour en cours et rend son budget ; ne fait
   rien et rend FALSE si le tour n'a encore rien posé. */
bool_t match_undo(Match *m);

/* Termine le tour du joueur courant : passe la main au joueur 2 après le
   tour du joueur 1, ou fait tourner le ou les ticks du round (spec § 3.1,
   3.2) et contrôle la victoire après celui du joueur 2. Ne fait rien si la
   partie est déjà terminée (match_winner() != WINNER_NONE). */
void   match_end_turn(Match *m);

/* Une génération : halo recopié, life_tick, puis contrôle d'extinction
   (m->winner mis à jour). Le tour, le round et les poses ne changent pas.
   Sans effet si la partie est finie. */
void match_tick(Match *m);

/* Donne la main à `who` : poses remises à zéro, instantané de portée pris
   sur le plateau courant, masque de portée recalculé. */
void match_begin_turn(Match *m, Cell who);

/* WINNER_NONE tant que la partie continue. */
Winner match_winner(const Match *m);

/* Nombre de ticks que le prochain match_end_turn() du joueur 2 fera jouer
   ce round : 1 avant RAMPUP_ROUND, TICKS_AFTER_RAMPUP à partir de là. */
int    match_ticks_this_round(const Match *m);

#endif
