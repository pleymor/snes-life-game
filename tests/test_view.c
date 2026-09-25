#include "harness.h"
#include "match.h"
#include "view.h"

static Match m;
static u8 grid[BOARD_H][BOARD_W];
static u8 hud[HUD_W];
static u8 d[3];

static void test_les_cellules_prennent_la_tuile_de_leur_couleur(void)
{
    match_start(&m);
    view_board(&m, FALSE, grid);
    T_EQ(grid[14][6], TILE_P1);     /* bloc bleu, spec § 2.4 */
    T_EQ(grid[9][25], TILE_P2);     /* bloc rouge */
    T_EQ(grid[0][0], TILE_EMPTY);
}

static void test_la_portee_ne_marque_que_les_cases_vides_atteignables(void)
{
    match_start(&m);
    view_board(&m, TRUE, grid);
    T_EQ(grid[13][5], TILE_RANGE);  /* vide, à 1 case du bloc bleu */
    T_EQ(grid[14][6], TILE_P1);     /* occupée : la couleur l'emporte */
    T_EQ(grid[15][15], TILE_EMPTY); /* vide mais hors de portée */
}

static void test_sans_marquage_aucune_tuile_de_portee(void)
{
    int x, y, marks = 0;
    match_start(&m);
    view_board(&m, FALSE, grid);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            if (grid[y][x] == TILE_RANGE) marks++;
        }
    }
    T_EQ(marks, 0);
}

static void test_les_chiffres_sont_cales_a_droite_sur_trois_rangs(void)
{
    view_digits3(0, d);
    T_EQ(d[0], TILE_DIGIT0); T_EQ(d[1], TILE_DIGIT0); T_EQ(d[2], TILE_DIGIT0);
    view_digits3(42, d);
    T_EQ(d[0], TILE_DIGIT0 + 0);
    T_EQ(d[1], TILE_DIGIT0 + 4);
    T_EQ(d[2], TILE_DIGIT0 + 2);
    view_digits3(768, d);
    T_EQ(d[0], TILE_DIGIT0 + 7);
    T_EQ(d[1], TILE_DIGIT0 + 6);
    T_EQ(d[2], TILE_DIGIT0 + 8);
}

static void test_les_chiffres_saturent_a_999(void)
{
    view_digits3(1000, d);
    T_EQ(d[0], TILE_DIGIT0 + 9);
    T_EQ(d[2], TILE_DIGIT0 + 9);
    view_digits3(-5, d);
    T_EQ(d[2], TILE_DIGIT0);
}

static void test_le_bandeau_montre_populations_round_et_pastilles(void)
{
    match_start(&m);
    view_hud(&m, hud);
    T_EQ(hud[0], TILE_P1);
    T_EQ(hud[31], TILE_P2);
    T_EQ(hud[4], TILE_DIGIT0 + 9);        /* 009 cellules bleues au départ */
    T_EQ(hud[29], TILE_DIGIT0 + 9);       /* 009 rouges */
    T_EQ(hud[12], TILE_R);
    T_EQ(hud[15], TILE_DIGIT0 + 1);       /* round 001 */
    T_EQ(hud[16], TILE_SLASH);
    T_EQ(hud[18], TILE_DIGIT0 + 4);       /* plafond 040 */
    T_EQ(hud[19], TILE_DIGIT0 + 0);
}

static void test_les_poses_restantes_se_vident(void)
{
    match_start(&m);
    T_TRUE(match_place(&m, 5, 13));
    view_hud(&m, hud);
    T_EQ(hud[6], TILE_PIP_ON);
    T_EQ(hud[7], TILE_PIP_ON);
    T_EQ(hud[8], TILE_PIP_OFF);
    /* Le joueur inactif garde ses trois pastilles pleines. */
    T_EQ(hud[23], TILE_PIP_ON);
    T_EQ(hud[25], TILE_PIP_ON);
}

static void test_le_marqueur_demballement_napparait_qua_partir_du_round_16(void)
{
    match_start(&m);
    view_hud(&m, hud);
    T_EQ(hud[20], TILE_EMPTY);
    T_EQ(hud[21], TILE_EMPTY);
    m.round = RAMPUP_ROUND;
    view_hud(&m, hud);
    T_EQ(hud[20], TILE_TIMES);
    T_EQ(hud[21], TILE_DIGIT0 + TICKS_AFTER_RAMPUP);
}

void suite_view(void)
{
    T_RUN(test_les_cellules_prennent_la_tuile_de_leur_couleur);
    T_RUN(test_la_portee_ne_marque_que_les_cases_vides_atteignables);
    T_RUN(test_sans_marquage_aucune_tuile_de_portee);
    T_RUN(test_les_chiffres_sont_cales_a_droite_sur_trois_rangs);
    T_RUN(test_les_chiffres_saturent_a_999);
    T_RUN(test_le_bandeau_montre_populations_round_et_pastilles);
    T_RUN(test_les_poses_restantes_se_vident);
    T_RUN(test_le_marqueur_demballement_napparait_qua_partir_du_round_16);
}
