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

/* Tout le halo, pas seulement les coins : chaque case de stockage vaut la
   case de jeu repliée qu'elle représente. */
static void test_wrap_remplit_tout_le_halo(void)
{
    static Board b;
    int x, y;
    board_clear(&b);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            board_set(&b, x, y, (Cell)((x * 7 + y * 3) % 3));
        }
    }
    board_wrap(&b);
    for (y = -1; y <= BOARD_H; y++) {
        for (x = -1; x <= BOARD_W; x++) {
            T_EQ(b.c[y + 1][x + 1], board_get(&b, BOARD_WRAP_X(x), BOARD_WRAP_Y(y)));
        }
    }
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

/* ---- fix round 1 : board_count() épinglé avant le hoist du pointeur de
   ligne (perf review, docs/snes-notes.md § 8) ---- */

/* xorshift32 propre à ce fichier, pour un plateau aléatoire reproductible
   (pas de rand()). */
static unsigned long tb;
static unsigned long trand(void)
{
    unsigned long x = tb & 0xFFFFFFFFUL;
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x << 5) & 0xFFFFFFFFUL;
    tb = x;
    return x;
}

/* Compte "à la main", par un simple board_get() case par case (jamais
   touché par le hoist), la vérité de référence à laquelle comparer
   board_count()/board_count_pair(). */
static void tally(const Board *b, int *p1, int *p2)
{
    int x, y;
    *p1 = 0;
    *p2 = 0;
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            Cell v = board_get(b, x, y);
            if (v == CELL_P1) (*p1)++;
            else if (v == CELL_P2) (*p2)++;
        }
    }
}

static void test_count_sur_un_plateau_aleatoire_egale_un_comptage_independant(void)
{
    static Board b;
    int i, want1, want2;

    tb = 0x5EED1234UL;
    board_clear(&b);
    for (i = 0; i < BOARD_W * BOARD_H / 2; i++) {
        int px = (int)(trand() % BOARD_W);
        int py = (int)(trand() % BOARD_H);
        board_set(&b, px, py, (trand() & 1UL) ? CELL_P1 : CELL_P2);
    }

    tally(&b, &want1, &want2);
    T_EQ(board_count(&b, CELL_P1), want1);
    T_EQ(board_count(&b, CELL_P2), want2);
}

/* board_count_pair() : les deux comptes en un seul passage (fix round 1,
   § c : évite les deux balayages séparés de view_hud()). */
static void test_count_pair_egale_deux_appels_de_count(void)
{
    static Board b;
    int p1, p2, i;

    board_seed(&b);
    board_count_pair(&b, &p1, &p2);
    T_EQ(p1, board_count(&b, CELL_P1));
    T_EQ(p2, board_count(&b, CELL_P2));

    tb = 0xABCDEF01UL;
    board_clear(&b);
    for (i = 0; i < BOARD_W * BOARD_H / 2; i++) {
        int px = (int)(trand() % BOARD_W);
        int py = (int)(trand() % BOARD_H);
        board_set(&b, px, py, (trand() & 1UL) ? CELL_P1 : CELL_P2);
    }
    board_count_pair(&b, &p1, &p2);
    T_EQ(p1, board_count(&b, CELL_P1));
    T_EQ(p2, board_count(&b, CELL_P2));
}

void suite_board(void)
{
    T_RUN(test_clear_donne_un_plateau_vide);
    T_RUN(test_set_et_get_font_un_aller_retour);
    T_RUN(test_count_compte_par_couleur);
    T_RUN(test_wrap_recopie_les_bords_opposes);
    T_RUN(test_wrap_recopie_dans_lautre_sens);
    T_RUN(test_wrap_remplit_tout_le_halo);
    T_RUN(test_seed_est_symetrique_par_rotation);
    T_RUN(test_seed_place_le_bloc_du_joueur_1);
    T_RUN(test_count_sur_un_plateau_aleatoire_egale_un_comptage_independant);
    T_RUN(test_count_pair_egale_deux_appels_de_count);
}
