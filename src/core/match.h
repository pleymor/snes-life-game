#ifndef MATCH_H
#define MATCH_H

#include "board.h"
#include "life.h"
#include "rules.h"

typedef struct {
    Board  board;
    Board  range;    /* instantané pris au début du tour */
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
