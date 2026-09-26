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
       ailleurs. match_place(), view_board() et ai.c's collect() le lisent
       tel quel au lieu d'appeler rules_in_range() par case (fix round 1 —
       perf review, docs/snes-notes.md § 8 : recalculer un masque déjà
       valable pour tout le tour à chaque pose/annulation/bascule était le
       gros du coût du rafraîchissement). */
    u8     range_mask[BOARD_H][BOARD_W];
    int    round;
    Cell   turn;
    int    placed;
    Move   history[BUDGET];
    Winner winner;
} Match;

void   match_start(Match *m);
bool_t match_place(Match *m, int x, int y);
bool_t match_undo(Match *m);
void   match_end_turn(Match *m);
Winner match_winner(const Match *m);
int    match_ticks_this_round(const Match *m);

#endif
