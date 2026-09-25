#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "match.h"
#include "ai.h"

static Match m;
static Move mv[BUDGET];

typedef struct {
    int elim, cap, draw, p1, p2, normal_wins;
    long rounds;      /* somme des rounds atteints, pour la moyenne */
    long pop_winner;  /* somme des populations gagnantes */
} Stats;

static Winner play_one(unsigned long *rng, Stats *s, AiLevel lvl_p1, AiLevel lvl_p2)
{
    int guard = 0;
    Winner w;

    match_start(&m);
    while (match_winner(&m) == WINNER_NONE && guard < 4 * ROUND_CAP) {
        AiLevel lvl = (m.turn == CELL_P1) ? lvl_p1 : lvl_p2;
        int n = ai_choose(&m, lvl, rng, mv);
        int i;
        for (i = 0; i < n; i++) {
            match_place(&m, (int)mv[i].x, (int)mv[i].y);
        }
        match_end_turn(&m);
        guard++;
    }

    w = match_winner(&m);
    s->rounds += m.round;
    /* Se fier au round serait faux : une élimination peut très bien tomber
       sur le tick du round 40. C'est la population nulle qui tranche. */
    if (board_count(&m.board, CELL_P1) == 0 ||
        board_count(&m.board, CELL_P2) == 0) s->elim++;
    else                                     s->cap++;
    if (w == WINNER_P1)      s->p1++;
    else if (w == WINNER_P2) s->p2++;
    else                     s->draw++;
    s->pop_winner += board_count(&m.board,
                                 (w == WINNER_P2) ? CELL_P2 : CELL_P1);
    return w;
}

int main(int argc, char **argv)
{
    int games = (argc > 1) ? atoi(argv[1]) : 100;
    unsigned long seed = (argc > 2) ? strtoul(argv[2], 0, 0) : 1UL;
    const char *mode = (argc > 3) ? argv[3] : "normal";
    int is_mixed = (strcmp(mode, "mixed") == 0);
    int is_easy  = (strcmp(mode, "easy")  == 0);
    Stats s;
    int i;

    s.elim = s.cap = s.draw = s.p1 = s.p2 = s.normal_wins = 0;
    s.rounds = 0;
    s.pop_winner = 0;

    for (i = 0; i < games; i++) {
        unsigned long rng = seed + (unsigned long)i;
        AiLevel lvl_p1, lvl_p2;
        Winner w;

        if (is_mixed) {
            /* Change de camp une partie sur deux : chaque niveau joue P1 la
               moitié du temps, ce qui neutralise l'avantage du trait dans
               la mesure. */
            if (i % 2 == 0) { lvl_p1 = AI_NORMAL; lvl_p2 = AI_EASY;   }
            else            { lvl_p1 = AI_EASY;   lvl_p2 = AI_NORMAL; }
        } else if (is_easy) {
            lvl_p1 = lvl_p2 = AI_EASY;
        } else {
            lvl_p1 = lvl_p2 = AI_NORMAL;
        }

        w = play_one(&rng, &s, lvl_p1, lvl_p2);

        if (is_mixed) {
            Cell normal_seat = (i % 2 == 0) ? CELL_P1 : CELL_P2;
            if ((normal_seat == CELL_P1 && w == WINNER_P1) ||
                (normal_seat == CELL_P2 && w == WINNER_P2)) {
                s.normal_wins++;
            }
        }
    }

    printf("parties        %d\n", games);
    printf("mode           %s\n", is_mixed ? "mixed" : (is_easy ? "easy" : "normal"));
    printf("réglages       BUDGET=%d RANGE=%d RAMPUP=%d TICKS=%d CAP=%d\n",
           BUDGET, RANGE_RADIUS, RAMPUP_ROUND, TICKS_AFTER_RAMPUP, ROUND_CAP);
    printf("élimination    %d (%.0f%%)\n", s.elim, 100.0 * s.elim / games);
    printf("plafond        %d (%.0f%%)\n", s.cap,  100.0 * s.cap  / games);
    printf("victoires P1   %d (%.0f%%)\n", s.p1,   100.0 * s.p1   / games);
    printf("victoires P2   %d (%.0f%%)\n", s.p2,   100.0 * s.p2   / games);
    printf("nuls           %d\n", s.draw);
    printf("round moyen    %.1f\n", (double)s.rounds / games);
    printf("pop. gagnante  %.1f\n", (double)s.pop_winner / games);
    if (is_mixed) {
        printf("victoires NORMAL %d (%.0f%%)\n",
               s.normal_wins, 100.0 * s.normal_wins / games);
    }
    return 0;
}
