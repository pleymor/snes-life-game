#include "harness.h"
#include "match.h"
#include "ai.h"

static Match m;
static Move mv[BUDGET];

/* ---- référence : la même chose, en simulant tout le plateau ---- */

static int net_after(const Board *base, Cell who, int depth)
{
    static Board a, t;
    Cell foe = (who == CELL_P1) ? CELL_P2 : CELL_P1;
    int i;
    a = *base;
    for (i = 0; i < depth; i++) {
        board_wrap(&a);
        life_tick(&a, &t);
        a = t;
    }
    return board_count(&a, who) - board_count(&a, foe);
}

static int full_delta(const Board *base, Cell who, int x, int y, int depth)
{
    static Board withp;
    int without = net_after(base, who, depth);
    withp = *base;
    board_set(&withp, x, y, who);
    return net_after(&withp, who, depth) - without;
}

/* xorshift32 local aux tests, pour fabriquer des plateaux reproductibles */
static unsigned long tr;
static unsigned long trand(void)
{
    unsigned long x = tr & 0xFFFFFFFFUL;
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x << 5) & 0xFFFFFFFFUL;
    tr = x;
    return x;
}

/* ---- LE test : la fenêtre locale doit égaler le plateau entier ---- */

static void test_la_fenetre_locale_egale_la_simulation_complete(void)
{
    static Board b;
    int trial, depth;

    tr = 0x1234ABCDUL;
    for (trial = 0; trial < 40; trial++) {
        int x, y, i;
        board_clear(&b);
        for (i = 0; i < BOARD_W * BOARD_H / 3; i++) {
            int px = (int)(trand() % BOARD_W);
            int py = (int)(trand() % BOARD_H);
            board_set(&b, px, py, (trand() & 1UL) ? CELL_P1 : CELL_P2);
        }
        do {
            x = (int)(trand() % BOARD_W);
            y = (int)(trand() % BOARD_H);
        } while (board_get(&b, x, y) != CELL_EMPTY);

        for (depth = 1; depth <= 2; depth++) {
            T_EQ(ai_eval_local(&b, CELL_P1, x, y, depth),
                 full_delta(&b, CELL_P1, x, y, depth));
            T_EQ(ai_eval_local(&b, CELL_P2, x, y, depth),
                 full_delta(&b, CELL_P2, x, y, depth));
        }
    }
}

/* ---- comportement ---- */

static void test_les_coups_rendus_sont_legaux_et_distincts(void)
{
    unsigned long rng = 1UL;
    int n, i, j;
    match_start(&m);
    m.turn = CELL_P2;
    m.range = m.board;
    n = ai_choose(&m, AI_NORMAL, &rng, mv);
    T_TRUE(n > 0);
    T_TRUE(n <= BUDGET);
    for (i = 0; i < n; i++) {
        T_TRUE(rules_in_range(&m.range, CELL_P2, (int)mv[i].x, (int)mv[i].y));
        T_EQ(board_get(&m.board, (int)mv[i].x, (int)mv[i].y), CELL_EMPTY);
        for (j = 0; j < i; j++) {
            T_FALSE(mv[i].x == mv[j].x && mv[i].y == mv[j].y);
        }
    }
}

/* Deux cellules bleues isolées meurent au tick suivant. Quatre poses
   referment un bloc immortel de 4 et valent +4 ; les deux poses qui font
   un clignotant ne valent que +3. Les quatre gagnantes sont à égalité,
   et le départage par ordre de balayage désigne (10, 9). */
static void test_lia_referme_le_bloc_plutot_que_le_clignotant(void)
{
    unsigned long rng = 1UL;
    int n;
    match_start(&m);
    board_clear(&m.board);
    board_set(&m.board, 10, 10, CELL_P1);
    board_set(&m.board, 11, 10, CELL_P1);
    board_wrap(&m.board);
    m.turn = CELL_P1;
    m.range = m.board;
    n = ai_choose(&m, AI_NORMAL, &rng, mv);
    T_TRUE(n > 0);
    T_EQ(mv[0].x, 10);
    T_EQ(mv[0].y, 9);
}

static void test_sans_aucune_cellule_lia_passe(void)
{
    unsigned long rng = 1UL;
    match_start(&m);
    board_clear(&m.board);
    board_set(&m.board, 10, 10, CELL_P2);
    board_wrap(&m.board);
    m.turn = CELL_P1;
    m.range = m.board;
    T_EQ(ai_choose(&m, AI_NORMAL, &rng, mv), 0);
}

static void test_le_niveau_facile_est_reproductible(void)
{
    unsigned long r1 = 42UL, r2 = 42UL;
    Move a[BUDGET], b[BUDGET];
    int na, nb, i;
    match_start(&m);
    na = ai_choose(&m, AI_EASY, &r1, a);
    nb = ai_choose(&m, AI_EASY, &r2, b);
    T_EQ(na, nb);
    for (i = 0; i < na; i++) {
        T_EQ(a[i].x, b[i].x);
        T_EQ(a[i].y, b[i].y);
    }
}

/* Le simulateur de la tâche 7 en dépend : une partie doit finir. */
static void test_une_partie_ia_contre_ia_se_termine(void)
{
    unsigned long rng = 7UL;
    int guard = 0;
    match_start(&m);
    while (match_winner(&m) == WINNER_NONE && guard < 4 * ROUND_CAP) {
        int n = ai_choose(&m, AI_NORMAL, &rng, mv);
        int i;
        for (i = 0; i < n; i++) {
            T_TRUE(match_place(&m, (int)mv[i].x, (int)mv[i].y));
        }
        match_end_turn(&m);
        guard++;
    }
    T_TRUE(match_winner(&m) != WINNER_NONE);
}

void suite_ai(void)
{
    T_RUN(test_la_fenetre_locale_egale_la_simulation_complete);
    T_RUN(test_les_coups_rendus_sont_legaux_et_distincts);
    T_RUN(test_lia_referme_le_bloc_plutot_que_le_clignotant);
    T_RUN(test_sans_aucune_cellule_lia_passe);
    T_RUN(test_le_niveau_facile_est_reproductible);
    T_RUN(test_une_partie_ia_contre_ia_se_termine);
}
