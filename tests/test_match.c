#include "harness.h"
#include "match.h"

static Match m;

/* Prépare une partie sur un plateau vide, à un round choisi.
   Les tests d'arbitrage ont besoin de positions bien plus simples que
   celle de départ. */
static void arm(int round, Cell turn)
{
    match_start(&m);
    board_clear(&m.board);
    m.round = round;
    m.turn = turn;
    m.placed = 0;
}

static void commit_setup(void)
{
    board_wrap(&m.board);
    m.range = m.board;
}

static void block(int x, int y, Cell who)
{
    board_set(&m.board, x,     y,     who);
    board_set(&m.board, x + 1, y,     who);
    board_set(&m.board, x,     y + 1, who);
    board_set(&m.board, x + 1, y + 1, who);
}

/* ---- budget et annulation, sur la position de départ ---- */

static void test_trois_poses_puis_le_budget_est_epuise(void)
{
    match_start(&m);
    T_EQ(m.round, 1);
    T_EQ(m.turn, CELL_P1);
    T_TRUE(match_place(&m, 5, 13));
    T_TRUE(match_place(&m, 8, 13));
    T_TRUE(match_place(&m, 5, 16));
    T_EQ(m.placed, BUDGET);
    T_FALSE(match_place(&m, 8, 16));   /* quatrième : refusée */
    T_EQ(board_get(&m.board, 8, 16), CELL_EMPTY);
}

static void test_une_pose_hors_portee_est_refusee(void)
{
    match_start(&m);
    T_FALSE(match_place(&m, 15, 15));
    T_EQ(m.placed, 0);
}

static void test_la_portee_est_figee_pour_le_tour(void)
{
    match_start(&m);
    T_TRUE(match_place(&m, 5, 13));
    /* (3,13) est à 2 cases de (5,13) mais à 3 de toute cellule bleue
       présente au début du tour. La portée étant figée, c'est refusé. */
    T_FALSE(match_place(&m, 3, 13));
}

static void test_annuler_rend_la_case_et_le_budget(void)
{
    match_start(&m);
    T_TRUE(match_place(&m, 5, 13));
    T_TRUE(match_undo(&m));
    T_EQ(m.placed, 0);
    T_EQ(board_get(&m.board, 5, 13), CELL_EMPTY);
    T_FALSE(match_undo(&m));           /* plus rien à annuler */
}

/* ---- enchaînement des tours ---- */

static void test_finir_le_tour_du_joueur_1_ne_ticke_pas(void)
{
    arm(1, CELL_P1);
    block(2, 2, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);   /* isolée : mourrait au tick */
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.turn, CELL_P2);
    T_EQ(m.round, 1);
    T_EQ(m.placed, 0);
    T_EQ(board_get(&m.board, 20, 20), CELL_P2);   /* toujours là */
}

static void test_finir_le_tour_du_joueur_2_ticke_et_avance_le_round(void)
{
    arm(1, CELL_P2);
    block(2, 2, CELL_P1);
    block(20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.round, 2);
    T_EQ(m.turn, CELL_P1);
    T_EQ(m.winner, WINNER_NONE);
}

static void test_le_nombre_de_ticks_double_a_lemballement(void)
{
    arm(RAMPUP_ROUND - 1, CELL_P1);
    T_EQ(match_ticks_this_round(&m), 1);
    arm(RAMPUP_ROUND, CELL_P1);
    T_EQ(match_ticks_this_round(&m), TICKS_AFTER_RAMPUP);
    arm(ROUND_CAP, CELL_P1);
    T_EQ(match_ticks_this_round(&m), TICKS_AFTER_RAMPUP);
}

/* ---- arbitrage ---- */

static void test_lelimination_arrete_le_round_des_le_premier_tick(void)
{
    arm(RAMPUP_ROUND, CELL_P2);        /* round à deux ticks */
    block(2, 2, CELL_P1);              /* immortel */
    board_set(&m.board, 10, 10, CELL_P1);   /* clignotant horizontal */
    board_set(&m.board, 11, 10, CELL_P1);
    board_set(&m.board, 12, 10, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);   /* isolée : meurt au tick 1 */
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_P1);
    /* Le clignotant est vertical : exactement un tick a eu lieu. */
    T_EQ(board_get(&m.board, 11,  9), CELL_P1);
    T_EQ(board_get(&m.board, 11, 11), CELL_P1);
    T_EQ(board_get(&m.board, 10, 10), CELL_EMPTY);
}

static void test_lextinction_simultanee_donne_un_nul(void)
{
    arm(1, CELL_P2);
    board_set(&m.board, 5, 5, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_DRAW);
}

static void test_le_plafond_tranche_a_la_population(void)
{
    arm(ROUND_CAP, CELL_P2);
    block(2, 2, CELL_P1);
    block(6, 2, CELL_P1);              /* 8 cellules bleues */
    block(20, 20, CELL_P2);            /* 4 cellules rouges */
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_P1);
    T_EQ(m.round, ROUND_CAP);          /* on ne dépasse pas le plafond */
}

static void test_le_plafond_a_egalite_donne_un_nul(void)
{
    arm(ROUND_CAP, CELL_P2);
    block(2, 2, CELL_P1);
    block(20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_DRAW);
}

static void test_une_partie_finie_est_gelee(void)
{
    arm(1, CELL_P2);
    board_set(&m.board, 5, 5, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_DRAW);
    match_end_turn(&m);                /* sans effet */
    T_EQ(m.winner, WINNER_DRAW);
    T_FALSE(match_place(&m, 5, 5));
}

/* ---- fix round 1 : le masque de portée mis en cache dans Match ---- */

/* Compare m.range_mask, cellule par cellule, à ce que rules_in_range()
   rendrait sur le même instantané/joueur : la garantie que begin_turn()
   garde bien le masque à jour (perf review, docs/snes-notes.md § 8). */
static void check_range_mask_matches_in_range(void)
{
    int x, y;
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            bool_t expected = rules_in_range(&m.range, m.turn, x, y);
            T_EQ(m.range_mask[y][x], expected ? 1 : 0);
        }
    }
}

static void test_le_masque_de_portee_suit_le_tour_courant(void)
{
    match_start(&m);
    check_range_mask_matches_in_range();   /* après match_start (P1) */

    T_TRUE(match_place(&m, 5, 13));
    match_end_turn(&m);                    /* P1 -> P2, pas de tick */
    T_EQ(m.turn, CELL_P2);
    check_range_mask_matches_in_range();   /* après le changement de tour */

    match_end_turn(&m);                    /* P2 termine : un tick a lieu */
    T_EQ(m.turn, CELL_P1);
    check_range_mask_matches_in_range();   /* après un tick */
}

void suite_match(void)
{
    T_RUN(test_trois_poses_puis_le_budget_est_epuise);
    T_RUN(test_une_pose_hors_portee_est_refusee);
    T_RUN(test_la_portee_est_figee_pour_le_tour);
    T_RUN(test_annuler_rend_la_case_et_le_budget);
    T_RUN(test_finir_le_tour_du_joueur_1_ne_ticke_pas);
    T_RUN(test_finir_le_tour_du_joueur_2_ticke_et_avance_le_round);
    T_RUN(test_le_nombre_de_ticks_double_a_lemballement);
    T_RUN(test_lelimination_arrete_le_round_des_le_premier_tick);
    T_RUN(test_lextinction_simultanee_donne_un_nul);
    T_RUN(test_le_plafond_tranche_a_la_population);
    T_RUN(test_le_plafond_a_egalite_donne_un_nul);
    T_RUN(test_une_partie_finie_est_gelee);
    T_RUN(test_le_masque_de_portee_suit_le_tour_courant);
}
