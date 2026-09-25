#include "harness.h"
#include "board.h"

static void test_clear_donne_un_plateau_vide(void)
{
    static Board b;
    board_set(&b, 5, 5, CELL_P1);
    board_clear(&b);
    T_EQ(board_get(&b, 5, 5), CELL_EMPTY);
    T_EQ(board_count(&b, CELL_P1), 0);
    T_EQ(board_count(&b, CELL_P2), 0);
}

static void test_set_et_get_font_un_aller_retour(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    board_set(&b, BOARD_W - 1, BOARD_H - 1, CELL_P2);
    T_EQ(board_get(&b, 0, 0), CELL_P1);
    T_EQ(board_get(&b, BOARD_W - 1, BOARD_H - 1), CELL_P2);
    T_EQ(board_get(&b, 1, 0), CELL_EMPTY);
}

static void test_count_compte_par_couleur(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, 1, 1, CELL_P1);
    board_set(&b, 2, 1, CELL_P1);
    board_set(&b, 3, 1, CELL_P2);
    T_EQ(board_count(&b, CELL_P1), 2);
    T_EQ(board_count(&b, CELL_P2), 1);
}

/* Le halo doit recevoir la copie du bord opposé, coins compris. */
static void test_wrap_recopie_les_bords_opposes(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    board_wrap(&b);
    T_EQ(b.c[BOARD_H + 1][1], CELL_P1);              /* halo bas */
    T_EQ(b.c[1][BOARD_W + 1], CELL_P1);              /* halo droit */
    T_EQ(b.c[BOARD_H + 1][BOARD_W + 1], CELL_P1);    /* coin bas-droit */
}

static void test_wrap_recopie_dans_lautre_sens(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, BOARD_W - 1, BOARD_H - 1, CELL_P2);
    board_wrap(&b);
    T_EQ(b.c[0][BOARD_W], CELL_P2);   /* halo haut */
    T_EQ(b.c[BOARD_H][0], CELL_P2);   /* halo gauche */
    T_EQ(b.c[0][0], CELL_P2);         /* coin haut-gauche */
}

/* § 2.4 de la spec : symétrie exacte par rotation de 180°. */
static void test_seed_est_symetrique_par_rotation(void)
{
    static Board b;
    int x, y;
    board_seed(&b);
    T_EQ(board_count(&b, CELL_P1), 9);
    T_EQ(board_count(&b, CELL_P2), 9);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            Cell here = board_get(&b, x, y);
            Cell there = board_get(&b, BOARD_W - 1 - x, BOARD_H - 1 - y);
            if (here == CELL_P1) {
                T_EQ(there, CELL_P2);
            } else if (here == CELL_P2) {
                T_EQ(there, CELL_P1);
            } else {
                T_EQ(there, CELL_EMPTY);
            }
        }
    }
}

static void test_seed_place_le_bloc_du_joueur_1(void)
{
    static Board b;
    board_seed(&b);
    T_EQ(board_get(&b, 6, 14), CELL_P1);
    T_EQ(board_get(&b, 7, 14), CELL_P1);
    T_EQ(board_get(&b, 6, 15), CELL_P1);
    T_EQ(board_get(&b, 7, 15), CELL_P1);
}

void suite_board(void)
{
    T_RUN(test_clear_donne_un_plateau_vide);
    T_RUN(test_set_et_get_font_un_aller_retour);
    T_RUN(test_count_compte_par_couleur);
    T_RUN(test_wrap_recopie_les_bords_opposes);
    T_RUN(test_wrap_recopie_dans_lautre_sens);
    T_RUN(test_seed_est_symetrique_par_rotation);
    T_RUN(test_seed_place_le_bloc_du_joueur_1);
}
