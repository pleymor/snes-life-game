#include "tutorial.h"

/* Opérations du script, un triplet {op, a, b} chacune. */
#define TUT_END       0
#define TUT_LESSON    1   /* a = numéro de leçon */
#define TUT_START     2   /* position de départ */
#define TUT_CLEAR     3   /* plateau vide, main au bleu */
#define TUT_CELL_P1   4   /* a, b = case */
#define TUT_CELL_P2   5
#define TUT_READY     6   /* fin de mise en place : portée recalculée */
#define TUT_ROUND     7   /* a = round */
#define TUT_TEXT      8   /* a = texte */
#define TUT_RANGE     9   /* a = 0 ou 1 */
#define TUT_CURSOR   10   /* a, b = case */
#define TUT_NOCURSOR 11
#define TUT_PLACE    12   /* a, b = case, vraie pose */
#define TUT_ENDTURN  13
#define TUT_TICK     14
#define TUT_WAIT     15   /* a = images, 1 à 255 */

#define T3(o, a, b) (o), (a), (b)
#define GLIDE T3(TUT_TICK, 0, 0), T3(TUT_WAIT, 18, 0)

/* Table plate à une dimension et de portée fichier : reste en ROM. */
static const u8 tut_script[] = {
    T3(TUT_LESSON, 1, 0), T3(TUT_START, 0, 0), T3(TUT_READY, 0, 0),
    T3(TUT_TEXT, 0, 0), T3(TUT_WAIT, 250, 0), T3(TUT_WAIT, 110, 0),

    T3(TUT_LESSON, 2, 0), T3(TUT_CLEAR, 0, 0),
    T3(TUT_CELL_P1, 8, 10),
    T3(TUT_CELL_P1, 20, 10), T3(TUT_CELL_P1, 21, 10),
    T3(TUT_CELL_P1, 20, 11), T3(TUT_CELL_P1, 21, 11),
    T3(TUT_READY, 0, 0), T3(TUT_TEXT, 1, 0), T3(TUT_WAIT, 150, 0),
    T3(TUT_TICK, 0, 0), T3(TUT_WAIT, 200, 0),

    T3(TUT_LESSON, 3, 0), T3(TUT_CLEAR, 0, 0),
    T3(TUT_CELL_P1, 15, 11), T3(TUT_CELL_P1, 14, 11), T3(TUT_CELL_P1, 16, 11),
    T3(TUT_CELL_P1, 15, 10), T3(TUT_CELL_P1, 15, 12),
    T3(TUT_READY, 0, 0), T3(TUT_TEXT, 2, 0), T3(TUT_WAIT, 150, 0),
    T3(TUT_TICK, 0, 0), T3(TUT_WAIT, 200, 0),

    T3(TUT_LESSON, 4, 0), T3(TUT_CLEAR, 0, 0),
    T3(TUT_CELL_P1, 14, 10), T3(TUT_CELL_P1, 15, 10), T3(TUT_CELL_P2, 16, 10),
    T3(TUT_READY, 0, 0), T3(TUT_TEXT, 3, 0), T3(TUT_WAIT, 180, 0),
    T3(TUT_TICK, 0, 0), T3(TUT_WAIT, 220, 0),

    T3(TUT_LESSON, 5, 0), T3(TUT_START, 0, 0), T3(TUT_READY, 0, 0),
    T3(TUT_TEXT, 4, 0), T3(TUT_RANGE, 1, 0),
    T3(TUT_CURSOR, 16, 12), T3(TUT_WAIT, 60, 0),
    T3(TUT_CURSOR, 8, 14), T3(TUT_WAIT, 40, 0), T3(TUT_PLACE, 8, 14), T3(TUT_WAIT, 60, 0),
    T3(TUT_CURSOR, 8, 15), T3(TUT_WAIT, 30, 0), T3(TUT_PLACE, 8, 15), T3(TUT_WAIT, 60, 0),
    T3(TUT_CURSOR, 9, 14), T3(TUT_WAIT, 30, 0), T3(TUT_PLACE, 9, 14), T3(TUT_WAIT, 90, 0),
    T3(TUT_NOCURSOR, 0, 0), T3(TUT_ENDTURN, 0, 0), T3(TUT_WAIT, 120, 0),

    T3(TUT_LESSON, 6, 0), T3(TUT_START, 0, 0), T3(TUT_READY, 0, 0),
    T3(TUT_TEXT, 5, 0), T3(TUT_RANGE, 1, 0),
    T3(TUT_CURSOR, 8, 14), T3(TUT_WAIT, 90, 0), T3(TUT_PLACE, 8, 14),
    T3(TUT_WAIT, 200, 0), T3(TUT_WAIT, 60, 0),

    T3(TUT_LESSON, 7, 0), T3(TUT_CLEAR, 0, 0),
    T3(TUT_CELL_P1, 29, 8), T3(TUT_CELL_P1, 30, 9), T3(TUT_CELL_P1, 28, 10),
    T3(TUT_CELL_P1, 29, 10), T3(TUT_CELL_P1, 30, 10),
    T3(TUT_READY, 0, 0), T3(TUT_TEXT, 6, 0), T3(TUT_WAIT, 90, 0),
    GLIDE, GLIDE, GLIDE, GLIDE, GLIDE, GLIDE,
    GLIDE, GLIDE, GLIDE, GLIDE, GLIDE, GLIDE,
    T3(TUT_READY, 0, 0), T3(TUT_RANGE, 1, 0), T3(TUT_WAIT, 150, 0),

    T3(TUT_LESSON, 8, 0), T3(TUT_START, 0, 0), T3(TUT_ROUND, 16, 0),
    T3(TUT_READY, 0, 0), T3(TUT_TEXT, 7, 0), T3(TUT_WAIT, 150, 0),
    T3(TUT_ENDTURN, 0, 0), T3(TUT_WAIT, 30, 0),
    T3(TUT_ENDTURN, 0, 0), T3(TUT_WAIT, 220, 0),

    T3(TUT_LESSON, 9, 0), T3(TUT_CLEAR, 0, 0),
    T3(TUT_CELL_P1, 10, 10), T3(TUT_CELL_P1, 11, 10),
    T3(TUT_CELL_P1, 10, 11), T3(TUT_CELL_P1, 11, 11),
    T3(TUT_CELL_P2, 20, 10),
    T3(TUT_READY, 0, 0), T3(TUT_TEXT, 8, 0), T3(TUT_WAIT, 150, 0),
    T3(TUT_ENDTURN, 0, 0), T3(TUT_WAIT, 30, 0),
    T3(TUT_ENDTURN, 0, 0), T3(TUT_WAIT, 240, 0),

    T3(TUT_END, 0, 0)
};

#define OP(i) tut_script[3 * (i)]
#define A(i)  tut_script[3 * (i) + 1]
#define B(i)  tut_script[3 * (i) + 2]

/* Exécute une opération qui n'est pas une attente. */
static void exec_op(TutPlayer *p, Match *m, int i)
{
    switch (OP(i)) {
    case TUT_LESSON:
        p->lesson = A(i);
        p->cursor_on = FALSE;
        p->show_range = FALSE;
        break;
    case TUT_START:
        match_start(m);
        break;
    case TUT_CLEAR:
        match_start(m);
        board_clear(&m->board);
        match_begin_turn(m, CELL_P1);
        break;
    case TUT_CELL_P1:
        board_set(&m->board, A(i), B(i), CELL_P1);
        break;
    case TUT_CELL_P2:
        board_set(&m->board, A(i), B(i), CELL_P2);
        break;
    case TUT_READY:
        match_begin_turn(m, CELL_P1);
        break;
    case TUT_ROUND:
        m->round = A(i);
        break;
    case TUT_TEXT:
        p->caption = A(i);
        break;
    case TUT_RANGE:
        p->show_range = (bool_t)(A(i) != 0);
        break;
    case TUT_CURSOR:
        p->cursor_x = A(i);
        p->cursor_y = B(i);
        p->cursor_on = TRUE;
        break;
    case TUT_NOCURSOR:
        p->cursor_on = FALSE;
        break;
    case TUT_PLACE:
        p->cursor_x = A(i);
        p->cursor_y = B(i);
        if (match_place(m, A(i), B(i))) p->placements++;
        break;
    case TUT_ENDTURN:
        /* La fin du tour du rouge fait tourner les générations du round. */
        if (m->turn == CELL_P2 && m->winner == WINNER_NONE)
            p->generations += match_ticks_this_round(m);
        match_end_turn(m);
        break;
    case TUT_TICK:
        /* Sans arbitrage : une leçon n'est pas une partie, un camp absent
           ne doit pas y déclarer de vainqueur. */
        match_generation(m);
        p->generations++;
        break;
    default:
        break;
    }
}

bool_t tut_update(TutPlayer *p, Match *m, int frames)
{
    bool_t changed = FALSE;

    if (p->done) return FALSE;
    for (;;) {
        if (p->wait > 0) {
            if (frames < p->wait) {
                p->wait -= frames;
                return changed;
            }
            frames -= p->wait;
            p->wait = 0;
        }
        if (OP(p->pc) == TUT_END) {
            p->done = TRUE;
            return TRUE;
        }
        if (OP(p->pc) == TUT_WAIT) {
            p->wait = A(p->pc);
        } else {
            exec_op(p, m, p->pc);
            changed = TRUE;
        }
        p->pc++;
    }
}

void tut_start(TutPlayer *p, Match *m)
{
    p->pc = 0;
    p->wait = 0;
    p->lesson = 0;
    p->caption = -1;
    p->cursor_x = 0;
    p->cursor_y = 0;
    p->cursor_on = FALSE;
    p->show_range = FALSE;
    p->done = FALSE;
    p->placements = 0;
    p->generations = 0;
    match_start(m);
    tut_update(p, m, 0);
}

void tut_skip(TutPlayer *p, Match *m)
{
    int i = p->pc;
    if (p->done) return;
    while (OP(i) != TUT_LESSON && OP(i) != TUT_END) i++;
    if (OP(i) == TUT_END) {
        p->done = TRUE;
        return;
    }
    p->pc = i;
    p->wait = 0;
    tut_update(p, m, 0);
}

bool_t tut_lesson_done(const TutPlayer *p)
{
    return (bool_t)(!p->done && p->wait > 0 &&
                    (OP(p->pc) == TUT_LESSON || OP(p->pc) == TUT_END));
}

const char *tut_caption_line(int i, int line)
{
    if (i < 0 || i >= TUT_LESSONS || line < 0 || line > 2) return "";
    switch (i * 3 + line) {
    case 0:  return "TWO COLONIES SHARE A SMALL";
    case 1:  return "WORLD. WIPE OUT THE OTHER";
    case 2:  return "ONE TO WIN.";
    case 3:  return "A CELL WITH 2 OR 3 NEIGHBOURS";
    case 4:  return "LIVES ON. ALONE, IT DIES.";
    case 6:  return "WITH 4 OR MORE NEIGHBOURS,";
    case 7:  return "A CELL IS CROWDED OUT.";
    case 9:  return "3 NEIGHBOURS GIVE BIRTH.";
    case 10: return "THE NEW CELL TAKES THE";
    case 11: return "MAJORITY COLOUR.";
    case 12: return "ON YOUR TURN, PLACE UP TO 3";
    case 13: return "CELLS ON THE DOTS, THEN";
    case 14: return "PRESS START.";
    case 15: return "YOUR REACH IS SET AT THE";
    case 16: return "START OF YOUR TURN.";
    case 18: return "A GLIDER CRAWLS ACROSS THE";
    case 19: return "WORLD. THE EDGES WRAP";
    case 20: return "AROUND.";
    case 21: return "FROM ROUND 16 THE WORLD";
    case 22: return "RUNS TWICE AS FAST.";
    case 24: return "NO CELLS LEFT: YOU WIN.";
    case 25: return "AFTER ROUND 40, THE BIGGEST";
    case 26: return "COLONY WINS.";
    default: return "";
    }
}

void view_caption(int i, u8 out[3][HUD_W])
{
    int line;
    for (line = 0; line < 3; line++) {
        view_text(tut_caption_line(i, line), out[line], HUD_W);
    }
}
