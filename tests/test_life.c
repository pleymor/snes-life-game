#include "harness.h"
#include "board.h"
#include "life.h"

static Board a, b;

static void tick_once(void)
{
    board_wrap(&a);
    life_tick(&a, &b);
    a = b;
}

static void test_le_bloc_est_stable(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P1);
    board_set(&a, 11, 11, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 4);
    T_EQ(board_get(&a, 10, 10), CELL_P1);
    T_EQ(board_get(&a, 11, 11), CELL_P1);
}

static void test_le_clignotant_oscille_en_periode_2(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 12, 10, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 3);
    T_EQ(board_get(&a, 11,  9), CELL_P1);   /* devenu vertical */
    T_EQ(board_get(&a, 11, 11), CELL_P1);
    T_EQ(board_get(&a, 10, 10), CELL_EMPTY);
    tick_once();
    T_EQ(board_get(&a, 10, 10), CELL_P1);   /* de nouveau horizontal */
    T_EQ(board_get(&a, 12, 10), CELL_P1);
}

static void test_le_planeur_se_translate_de_1_1_en_4_ticks(void)
{
    int i;
    board_clear(&a);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 12, 11, CELL_P1);
    board_set(&a, 10, 12, CELL_P1);
    board_set(&a, 11, 12, CELL_P1);
    board_set(&a, 12, 12, CELL_P1);
    for (i = 0; i < 4; i++) {
        tick_once();
    }
    T_EQ(board_count(&a, CELL_P1), 5);
    T_EQ(board_get(&a, 12, 11), CELL_P1);
    T_EQ(board_get(&a, 13, 12), CELL_P1);
    T_EQ(board_get(&a, 11, 13), CELL_P1);
    T_EQ(board_get(&a, 12, 13), CELL_P1);
    T_EQ(board_get(&a, 13, 13), CELL_P1);
}

/* Un clignotant à cheval sur la couture verticale doit basculer normalement. */
static void test_le_tore_referme_le_bord_droit(void)
{
    board_clear(&a);
    board_set(&a, BOARD_W - 1, 10, CELL_P1);
    board_set(&a, 0, 10, CELL_P1);
    board_set(&a, 1, 10, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 3);
    T_EQ(board_get(&a, 0,  9), CELL_P1);
    T_EQ(board_get(&a, 0, 10), CELL_P1);
    T_EQ(board_get(&a, 0, 11), CELL_P1);
}

static void test_naissance_a_trois_parents_bleus(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P1);
    tick_once();
    T_EQ(board_get(&a, 11, 11), CELL_P1);
}

static void test_naissance_a_deux_bleus_un_rouge_est_bleue(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P2);
    tick_once();
    T_EQ(board_get(&a, 11, 11), CELL_P1);
}

static void test_naissance_a_un_bleu_deux_rouges_est_rouge(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P2);
    board_set(&a, 10, 11, CELL_P2);
    tick_once();
    T_EQ(board_get(&a, 11, 11), CELL_P2);
}

/* Une cellule qui survit garde sa couleur, quelle que soit celle des voisins. */
static void test_le_survivant_garde_sa_couleur(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P1);
    board_set(&a, 11, 11, CELL_P2);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 3);
    T_EQ(board_get(&a, 11, 11), CELL_P2);
}

static void test_la_solitude_tue(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 0);
}

static void test_la_surpopulation_tue_le_centre(void)
{
    int x, y;
    board_clear(&a);
    for (y = 9; y <= 11; y++) {
        for (x = 9; x <= 11; x++) {
            board_set(&a, x, y, CELL_P1);
        }
    }
    tick_once();
    T_EQ(board_get(&a, 10, 10), CELL_EMPTY);   /* 8 voisins */
    T_EQ(board_get(&a,  9,  9), CELL_P1);      /* 3 voisins, survit */
}

void suite_life(void)
{
    T_RUN(test_le_bloc_est_stable);
    T_RUN(test_le_clignotant_oscille_en_periode_2);
    T_RUN(test_le_planeur_se_translate_de_1_1_en_4_ticks);
    T_RUN(test_le_tore_referme_le_bord_droit);
    T_RUN(test_naissance_a_trois_parents_bleus);
    T_RUN(test_naissance_a_deux_bleus_un_rouge_est_bleue);
    T_RUN(test_naissance_a_un_bleu_deux_rouges_est_rouge);
    T_RUN(test_le_survivant_garde_sa_couleur);
    T_RUN(test_la_solitude_tue);
    T_RUN(test_la_surpopulation_tue_le_centre);
}
