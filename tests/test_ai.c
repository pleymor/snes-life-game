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
    rules_range_mask(&m.range, m.turn, m.range_mask);
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
    rules_range_mask(&m.range, m.turn, m.range_mask);
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
    rules_range_mask(&m.range, m.turn, m.range_mask);
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

/* ---- plateaux reproductibles pour les tests d'équivalence et
   l'épinglage : `cells` tirages, les deux couleurs mêlées. Même 40 tirages
   donnent assez de candidats pour que le niveau normal tronque à K = 32 ;
   DENSE (un quart des cases) atteint en plus le plafond AI_MAX_CANDS. ---- */

#define DENSE (BOARD_W * BOARD_H / 4)
#define SPARSE 40

static void fill_match(unsigned long seed, Cell turn, int cells)
{
    int i;
    tr = seed;
    match_start(&m);
    board_clear(&m.board);
    for (i = 0; i < cells; i++) {
        int px = (int)(trand() % BOARD_W);
        int py = (int)(trand() % BOARD_H);
        board_set(&m.board, px, py, (trand() & 1UL) ? CELL_P1 : CELL_P2);
    }
    board_wrap(&m.board);
    m.turn = turn;
    m.range = m.board;
    rules_range_mask(&m.range, m.turn, m.range_mask);
}

static void pair_match(void)
{
    match_start(&m);
    board_clear(&m.board);
    board_set(&m.board, 10, 10, CELL_P1);
    board_set(&m.board, 11, 10, CELL_P1);
    board_wrap(&m.board);
    m.turn = CELL_P1;
    m.range = m.board;
    rules_range_mask(&m.range, m.turn, m.range_mask);
}

static const unsigned long pin_seeds[3] = { 0x1234ABCDUL, 0xBEEF1234UL, 0x0F0F5A5AUL };

/* ai_step() appelé avec un budget de 1 candidat à la fois doit produire
   exactement la même séquence de coups, et laisser rng dans le même état,
   que ai_choose() d'une seule traite, pour les deux niveaux (AI_EASY
   consomme rng, AI_NORMAL non). Les plateaux denses exercent la
   troncature à K = 32 du niveau normal. */
static void check_step_equals_once(AiLevel lvl, bool_t dense, unsigned long seed, Cell turn)
{
    unsigned long rng_once, rng_stepped;
    Move once[BUDGET];
    AiJob job;
    int n_once, i;

    if (dense) fill_match(seed, turn, DENSE); else pair_match();
    rng_once = 99UL;
    n_once = ai_choose(&m, lvl, &rng_once, once);

    if (dense) fill_match(seed, turn, DENSE); else pair_match();
    rng_stepped = 99UL;
    ai_begin(&job, &m, lvl);
    while (!ai_step(&job, &m, &rng_stepped, 1)) {
        /* une étape à la fois */
        if (job.made == 0 && job.row == BOARD_H && dense && lvl == AI_NORMAL) {
            T_TRUE(job.n > job.k);
        }
    }

    T_EQ(job.made, n_once);
    for (i = 0; i < n_once; i++) {
        T_EQ(job.out[i].x, once[i].x);
        T_EQ(job.out[i].y, once[i].y);
    }
    T_EQ(rng_stepped, rng_once);
}

static void test_ai_step_par_1_egale_ai_choose_dune_traite(void)
{
    int lvl_i, s;

    for (lvl_i = 0; lvl_i <= (int)AI_NORMAL; lvl_i++) {
        check_step_equals_once((AiLevel)lvl_i, FALSE, 0UL, CELL_P1);
        for (s = 0; s < 3; s++) {
            check_step_equals_once((AiLevel)lvl_i, TRUE, pin_seeds[s], CELL_P1);
            check_step_equals_once((AiLevel)lvl_i, TRUE, pin_seeds[s], CELL_P2);
        }
    }
}

/* Épinglage : les choix de l'IA relevés sur l'implémentation d'origine,
   avant toute optimisation. Toute dérive de comportement fait échouer ce
   test. Chaque ligne : nombre de coups, puis (x, y) des trois coups. */
static const u8 pinned_dense[12][7] = {
    { 3, 30,  1,  6,  8, 10,  9 },   /* graine 0, P1, facile */
    { 3,  0,  2,  7,  0,  9,  5 },   /* graine 0, P1, normal */
    { 3,  6,  1,  7,  0, 22,  1 },   /* graine 0, P2, facile */
    { 3, 22,  1,  0, 12,  0,  2 },   /* graine 0, P2, normal */
    { 3, 13,  9, 29,  1, 29,  2 },   /* graine 1, P1, facile */
    { 3, 29, 10, 31,  1,  6,  1 },   /* graine 1, P1, normal */
    { 3,  1,  1,  7,  3, 31,  1 },   /* graine 1, P2, facile */
    { 3,  9,  5, 11, 10,  7,  1 },   /* graine 1, P2, normal */
    { 3, 14, 11, 15,  1, 14,  0 },   /* graine 2, P1, facile */
    { 3, 15,  1,  1,  0, 21,  2 },   /* graine 2, P1, normal */
    { 3, 17,  4, 16,  3, 19,  1 },   /* graine 2, P2, facile */
    { 3,  8,  6, 17,  1,  7,  7 }    /* graine 2, P2, normal */
};

/* Douze tours d'une partie IA contre IA depuis la position de départ,
   niveaux normal et facile en alternance, rng = 7 au départ. */
static const u8 pinned_game[12][7] = {
    { 3,  8,  6,  9,  6,  5,  7 },
    { 3, 24, 14, 25,  7, 26,  8 },
    { 3,  6,  7,  7,  5,  7,  4 },
    { 3, 25,  6, 24, 16, 24, 13 },
    { 3,  7,  7,  7,  5,  7, 10 },
    { 3, 23,  7, 27,  7, 23,  6 },
    { 3,  8,  7,  5,  9,  5,  5 },
    { 3, 24,  6, 24, 13, 26,  5 },
    { 3,  7,  8, 10,  4,  3,  7 },
    { 3, 28,  6, 26,  4, 24,  4 },
    { 3,  9,  5,  4,  8,  4,  6 },
    { 3, 23,  5, 26,  3, 24,  3 }
};

/* Plateaux clairsemés, niveau normal : ici l'heuristique de
   présélection (voisins, attraction vers l'adversaire) décide davantage de
   qui survit à la troncature. Dernière colonne : candidats rassemblés pour
   le premier coup (job.n après ai_begin()). */
static const u8 pinned_sparse[6][8] = {
    { 3, 30,  9, 30, 11, 28,  6, 163 },   /* graine 0, P1 */
    { 3,  9,  5, 29,  5, 10,  5, 180 },   /* graine 0, P2 */
    { 3,  1,  1,  1,  2,  2,  3, 168 },   /* graine 1, P1 */
    { 3,  8,  1, 21,  1, 22, 13, 181 },   /* graine 1, P2 */
    { 3, 26,  7, 22,  0, 26,  2, 165 },   /* graine 2, P1 */
    { 3,  9,  4, 26,  7, 24,  1, 147 }    /* graine 2, P2 */
};

/* Le plafond de candidats d'ai.c (AI_MAX_CANDS, privé) : les plateaux
   denses l'atteignent, la collecte doit s'y arrêter dans l'ordre de
   balayage. */
#define AI_MAX_CANDS_PINNED 256
#define AI_TOPK_MAX_PINNED  32

/* Démarre un tour normal et le mène jusqu'à la fin de la première
   collecte : job.n est alors le nombre de candidats du premier coup. */
static void first_collect(AiJob *job)
{
    unsigned long rng = 1UL;
    ai_begin(job, &m, AI_NORMAL);
    while (job->row < BOARD_H) {
        ai_step(job, &m, &rng, 1);
    }
}

static void check_pinned(const u8 row[7], int n, const Move got[BUDGET])
{
    int i;
    T_EQ(n, row[0]);
    for (i = 0; i < n && i < (int)row[0]; i++) {
        T_EQ(got[i].x, row[1 + 2 * i]);
        T_EQ(got[i].y, row[2 + 2 * i]);
    }
}

static void test_les_choix_de_lia_sont_epingles(void)
{
    unsigned long rng;
    int s, t, l, k, i, n, row = 0;

    for (s = 0; s < 3; s++) {
        for (t = (int)CELL_P1; t <= (int)CELL_P2; t++) {
            for (l = 0; l <= (int)AI_NORMAL; l++) {
                fill_match(pin_seeds[s], (Cell)t, DENSE);
                rng = 12345UL;
                n = ai_choose(&m, (AiLevel)l, &rng, mv);
                check_pinned(pinned_dense[row], n, mv);
                T_EQ(rng, (l == (int)AI_EASY) ? 2816511904UL : 12345UL);
                row++;
            }
        }
    }

    row = 0;
    for (s = 0; s < 3; s++) {
        for (t = (int)CELL_P1; t <= (int)CELL_P2; t++) {
            AiJob job;
            fill_match(pin_seeds[s], (Cell)t, SPARSE);
            first_collect(&job);
            T_EQ(job.n, pinned_sparse[row][7]);
            rng = 12345UL;
            n = ai_choose(&m, AI_NORMAL, &rng, mv);
            check_pinned(pinned_sparse[row], n, mv);
            row++;
        }
    }
    for (s = 0; s < 3; s++) {
        AiJob job;
        fill_match(pin_seeds[s], CELL_P1, DENSE);
        first_collect(&job);
        T_EQ(job.n, AI_MAX_CANDS_PINNED);
    }

    match_start(&m);
    rng = 7UL;
    for (k = 0; k < 12; k++) {
        n = ai_choose(&m, (k & 1) ? AI_EASY : AI_NORMAL, &rng, mv);
        check_pinned(pinned_game[k], n, mv);
        for (i = 0; i < n; i++) {
            match_place(&m, (int)mv[i].x, (int)mv[i].y);
        }
        match_end_turn(&m);
    }
    T_EQ(rng, 3843456730UL);
}

/* La collecte des candidats est étalée elle aussi : ai_begin() ne balaie
   rien, et chaque pas d'une unité de budget balaie une seule ligne du
   plateau. Une évaluation coûte AI_COST_EVAL unités ; un budget plus
   petit en fait quand même une (jamais de pas qui n'avance pas). */
static void test_la_collecte_est_etalee_ligne_par_ligne(void)
{
    AiJob job;
    unsigned long rng = 1UL;
    int y;

    fill_match(pin_seeds[0], CELL_P1, SPARSE);
    ai_begin(&job, &m, AI_NORMAL);
    T_EQ(job.row, 0);
    T_EQ(job.n, 0);
    for (y = 1; y <= BOARD_H; y++) {
        T_FALSE(ai_step(&job, &m, &rng, AI_COST_ROW));
        T_EQ(job.row, y);
        T_EQ(job.i, 0);
    }
    T_EQ(job.n, pinned_sparse[0][7]);
    T_EQ(job.top, AI_TOPK_MAX_PINNED);

    T_FALSE(ai_step(&job, &m, &rng, AI_COST_EVAL));
    T_EQ(job.i, 1);
    T_FALSE(ai_step(&job, &m, &rng, 1));
    T_EQ(job.i, 2);
    T_FALSE(ai_step(&job, &m, &rng, 2 * AI_COST_EVAL));
    T_EQ(job.i, 4);
}

void suite_ai(void)
{
    T_RUN(test_la_fenetre_locale_egale_la_simulation_complete);
    T_RUN(test_les_coups_rendus_sont_legaux_et_distincts);
    T_RUN(test_lia_referme_le_bloc_plutot_que_le_clignotant);
    T_RUN(test_sans_aucune_cellule_lia_passe);
    T_RUN(test_le_niveau_facile_est_reproductible);
    T_RUN(test_une_partie_ia_contre_ia_se_termine);
    T_RUN(test_ai_step_par_1_egale_ai_choose_dune_traite);
    T_RUN(test_les_choix_de_lia_sont_epingles);
    T_RUN(test_la_collecte_est_etalee_ligne_par_ligne);
}
