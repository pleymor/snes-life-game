#include "match.h"

/* 884 octets : bien trop pour la pile du 65816, d'où la portée fichier. */
static Board scratch;

static Winner judge_extinction(const Board *b)
{
    int p1 = board_count(b, CELL_P1);
    int p2 = board_count(b, CELL_P2);
    if (p1 == 0 && p2 == 0) return WINNER_DRAW;
    if (p1 == 0)            return WINNER_P2;
    if (p2 == 0)            return WINNER_P1;
    return WINNER_NONE;
}

static Winner judge_population(const Board *b)
{
    int p1 = board_count(b, CELL_P1);
    int p2 = board_count(b, CELL_P2);
    if (p1 > p2) return WINNER_P1;
    if (p2 > p1) return WINNER_P2;
    return WINNER_DRAW;
}

static void begin_turn(Match *m, Cell who)
{
    m->turn = who;
    m->placed = 0;
    m->range = m->board;
    rules_range_mask(&m->range, who, m->range_mask);
}

void match_start(Match *m)
{
    board_seed(&m->board);
    m->round = 1;
    m->winner = WINNER_NONE;
    begin_turn(m, CELL_P1);
}

int match_ticks_this_round(const Match *m)
{
    return (m->round < RAMPUP_ROUND) ? 1 : TICKS_AFTER_RAMPUP;
}

Winner match_winner(const Match *m)
{
    return m->winner;
}

bool_t match_place(Match *m, int x, int y)
{
    if (m->winner != WINNER_NONE)  return FALSE;
    if (m->placed >= BUDGET)       return FALSE;
    if (board_get(&m->board, x, y) != CELL_EMPTY) return FALSE;
    /* La portée se lit sur le masque figé au début du tour (begin_turn),
       pas recalculée ici : une cellule posée à l'instant ne doit pas
       étendre la zone de pose, et rules_in_range() par case serait le
       même calcul refait pour rien. */
    if (!m->range_mask[y][x]) return FALSE;

    board_set(&m->board, x, y, m->turn);
    m->history[m->placed].x = (u8)x;
    m->history[m->placed].y = (u8)y;
    m->placed++;
    return TRUE;
}

bool_t match_undo(Match *m)
{
    if (m->winner != WINNER_NONE) return FALSE;
    if (m->placed == 0)           return FALSE;
    m->placed--;
    board_set(&m->board,
              (int)m->history[m->placed].x,
              (int)m->history[m->placed].y,
              CELL_EMPTY);
    return TRUE;
}

void match_end_turn(Match *m)
{
    int i, ticks;

    if (m->winner != WINNER_NONE) return;

    if (m->turn == CELL_P1) {
        begin_turn(m, CELL_P2);
        return;
    }

    ticks = match_ticks_this_round(m);
    for (i = 0; i < ticks; i++) {
        board_wrap(&m->board);
        life_tick(&m->board, &scratch);
        m->board = scratch;
        m->winner = judge_extinction(&m->board);
        if (m->winner != WINNER_NONE) {
            return;   /* un second tick n'a pas lieu */
        }
    }

    if (m->round >= ROUND_CAP) {
        m->winner = judge_population(&m->board);
        return;
    }

    m->round++;
    begin_turn(m, CELL_P1);
}
