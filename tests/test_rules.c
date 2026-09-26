#include "harness.h"
#include "board.h"
#include "rules.h"

static Board b;

static void test_la_portee_va_jusqua_deux_cases(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P1);
    T_TRUE(rules_in_range(&b, CELL_P1, 12, 10));
    T_TRUE(rules_in_range(&b, CELL_P1, 12, 12));   /* Chebyshev = 2 */
    T_TRUE(rules_in_range(&b, CELL_P1,  8,  8));
    T_FALSE(rules_in_range(&b, CELL_P1, 13, 10));
    T_FALSE(rules_in_range(&b, CELL_P1, 13, 13));
}

static void test_la_portee_ne_vaut_que_pour_sa_couleur(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P1);
    T_FALSE(rules_in_range(&b, CELL_P2, 11, 10));
}

static void test_la_portee_franchit_les_bords(void)
{
    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    /* Sur le tore, (31, 23) est le voisin diagonal de (0, 0). */
    T_TRUE(rules_in_range(&b, CELL_P1, BOARD_W - 1, BOARD_H - 1));
    T_TRUE(rules_in_range(&b, CELL_P1, BOARD_W - 2, 1));
    T_FALSE(rules_in_range(&b, CELL_P1, BOARD_W - 3, 0));
}

static void test_on_ne_pose_pas_sur_une_case_occupee(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P1);
    board_set(&b, 11, 10, CELL_P2);
    T_FALSE(rules_can_place(&b, CELL_P1, 10, 10));
    T_FALSE(rules_can_place(&b, CELL_P1, 11, 10));
    T_TRUE(rules_can_place(&b, CELL_P1, 12, 10));
}

static void test_sans_aucune_cellule_rien_nest_posable(void)
{
    int x, y, posables = 0;
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P2);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            if (rules_can_place(&b, CELL_P1, x, y)) {
                posables++;
            }
        }
    }
    T_EQ(posables, 0);
}

/* ---- équivalence masque / test case par case ---- */

static u8 mask[BOARD_H][BOARD_W];

static void check_mask_matches_in_range(const Board *board, Cell player)
{
    int x, y;
    rules_range_mask(board, player, mask);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            bool_t expected = rules_in_range(board, player, x, y);
            T_EQ(mask[y][x], expected ? 1 : 0);
        }
    }
}

/* xorshift32 déterministe, propre au test (pas de rand()). */
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

static void test_le_masque_de_portee_egale_le_test_case_par_case(void)
{
    board_clear(&b);
    check_mask_matches_in_range(&b, CELL_P1);
    check_mask_matches_in_range(&b, CELL_P2);

    board_seed(&b);
    check_mask_matches_in_range(&b, CELL_P1);
    check_mask_matches_in_range(&b, CELL_P2);

    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    board_wrap(&b);
    check_mask_matches_in_range(&b, CELL_P1);

    board_clear(&b);
    board_set(&b, BOARD_W - 1, 0, CELL_P1);
    board_wrap(&b);
    check_mask_matches_in_range(&b, CELL_P1);

    board_clear(&b);
    board_set(&b, 0, BOARD_H - 1, CELL_P1);
    board_wrap(&b);
    check_mask_matches_in_range(&b, CELL_P1);

    board_clear(&b);
    board_set(&b, BOARD_W - 1, BOARD_H - 1, CELL_P1);
    board_wrap(&b);
    check_mask_matches_in_range(&b, CELL_P1);

    tr = 0xC0FFEEUL;
    {
        int trial;
        for (trial = 0; trial < 8; trial++) {
            int i;
            board_clear(&b);
            for (i = 0; i < BOARD_W * BOARD_H / 4; i++) {
                int px = (int)(trand() % BOARD_W);
                int py = (int)(trand() % BOARD_H);
                board_set(&b, px, py, (trand() & 1UL) ? CELL_P1 : CELL_P2);
            }
            board_wrap(&b);
            check_mask_matches_in_range(&b, CELL_P1);
            check_mask_matches_in_range(&b, CELL_P2);
        }
    }
}

static void test_le_masque_ignore_les_cellules_de_lautre_couleur(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P2);
    board_wrap(&b);
    rules_range_mask(&b, CELL_P1, mask);
    T_EQ(mask[10][11], 0);
    T_EQ(mask[10][12], 0);
}

void suite_rules(void)
{
    T_RUN(test_la_portee_va_jusqua_deux_cases);
    T_RUN(test_la_portee_ne_vaut_que_pour_sa_couleur);
    T_RUN(test_la_portee_franchit_les_bords);
    T_RUN(test_on_ne_pose_pas_sur_une_case_occupee);
    T_RUN(test_sans_aucune_cellule_rien_nest_posable);
    T_RUN(test_le_masque_de_portee_egale_le_test_case_par_case);
    T_RUN(test_le_masque_ignore_les_cellules_de_lautre_couleur);
}
