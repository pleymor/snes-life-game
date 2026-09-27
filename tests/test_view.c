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

/* Tuile attendue pour la lettre k (ordre I M G R A T O N) et le quart q. */
#define BIG(k, q) (TILE_BIG_BASE + 4 * (k) + (q))

static const int title_k[11] = { 0, 1, 1, 0, 2, 3, 4, 5, 0, 6, 7 };   /* IMMIGRATION */

static void check_title(void)
{
    int i;
    for (i = 0; i < 11; i++) {
        T_EQ(grid[3][5 + 2 * i],     BIG(title_k[i], 0));
        T_EQ(grid[3][5 + 2 * i + 1], BIG(title_k[i], 1));
        T_EQ(grid[4][5 + 2 * i],     BIG(title_k[i], 2));
        T_EQ(grid[4][5 + 2 * i + 1], BIG(title_k[i], 3));
    }
}

/* Vérifie un libellé à partir de la colonne 10 : lettre, chiffre ou vide. */
static void check_label(int row, const char *s)
{
    int i;
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z')
            T_EQ(grid[row][10 + i], TILE_LETTER_A + (s[i] - 'A'));
        else if (s[i] >= '0' && s[i] <= '9')
            T_EQ(grid[row][10 + i], TILE_DIGIT0 + (s[i] - '0'));
        else
            T_EQ(grid[row][10 + i], TILE_EMPTY);
    }
}

static void test_le_titre_s_ecrit_en_grand(void)
{
    int x, y;
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            grid[y][x] = TILE_EMPTY;
    view_title(grid);
    check_title();
    T_EQ(grid[3][4], TILE_EMPTY);
    T_EQ(grid[3][27], TILE_EMPTY);
    T_EQ(grid[2][10], TILE_EMPTY);
    T_EQ(grid[5][10], TILE_EMPTY);
}

static void test_le_menu_affiche_titre_et_modes(void)
{
    view_menu(0, grid);
    check_title();
    check_label(10, "2 PLAYERS");
    check_label(12, "VS CPU  EASY");
    check_label(14, "VS CPU  HARD");
    check_label(16, "HOW TO PLAY");
}

static void test_le_menu_marque_la_ligne_choisie(void)
{
    view_menu(0, grid);
    T_EQ(grid[10][8], TILE_PIP_ON);
    T_EQ(grid[12][8], TILE_EMPTY);
    T_EQ(grid[14][8], TILE_EMPTY);
    view_menu(2, grid);
    T_EQ(grid[10][8], TILE_EMPTY);
    T_EQ(grid[14][8], TILE_PIP_ON);
    view_menu(3, grid);
    T_EQ(grid[14][8], TILE_EMPTY);
    T_EQ(grid[16][8], TILE_PIP_ON);
}

static void test_le_menu_hors_bornes_n_a_pas_de_disque(void)
{
    view_menu(-1, grid);
    T_EQ(grid[10][8], TILE_EMPTY);
    T_EQ(grid[12][8], TILE_EMPTY);
    T_EQ(grid[14][8], TILE_EMPTY);
    check_label(12, "VS CPU  EASY");
    view_menu(4, grid);
    T_EQ(grid[16][8], TILE_EMPTY);
}

static void test_le_menu_ne_dessine_rien_d_autre(void)
{
    int x, y, n = 0;
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            grid[y][x] = 0x55;                      /* RAM non remise à zéro */
    view_menu(1, grid);
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            if (grid[y][x] != TILE_EMPTY) n++;
    /* 44 tuiles de titre + 1 disque + 8 + 9 + 9 + 9 (HOWTOPLAY) caractères non blancs */
    T_EQ(n, 44 + 1 + 8 + 9 + 9 + 9);
}

static void test_le_bandeau_de_fin_montre_le_vainqueur(void)
{
    match_start(&m);
    m.winner = WINNER_P1;
    view_result_banner(&m, hud);
    T_EQ(hud[12], TILE_P1);
    T_EQ(hud[19], TILE_P1);
    m.winner = WINNER_P2;
    view_result_banner(&m, hud);
    T_EQ(hud[12], TILE_P2);
    /* Un nul alterne les deux couleurs. */
    m.winner = WINNER_DRAW;
    view_result_banner(&m, hud);
    T_EQ(hud[12], TILE_P1);
    T_EQ(hud[13], TILE_P2);
}

/* Spec § 4 : le bandeau de fin montre aussi les deux populations finales,
   aux mêmes colonnes que le bandeau de jeu (2-4 et 27-29), autour du centre
   clignotant. Plateau construit à la main : 2 bleues, 1 rouge. */
static void test_le_bandeau_de_fin_montre_les_populations_finales(void)
{
    match_start(&m);
    board_clear(&m.board);
    board_set(&m.board, 0, 0, CELL_P1);
    board_set(&m.board, 1, 0, CELL_P1);
    board_set(&m.board, 2, 0, CELL_P2);
    board_wrap(&m.board);
    m.winner = WINNER_P1;
    view_result_banner(&m, hud);
    T_EQ(hud[0], TILE_P1);
    T_EQ(hud[31], TILE_P2);
    T_EQ(hud[2], TILE_DIGIT0);
    T_EQ(hud[3], TILE_DIGIT0);
    T_EQ(hud[4], TILE_DIGIT0 + 2);    /* 002 bleues */
    T_EQ(hud[27], TILE_DIGIT0);
    T_EQ(hud[28], TILE_DIGIT0);
    T_EQ(hud[29], TILE_DIGIT0 + 1);   /* 001 rouge */
}

static u8 line[16];

static void test_le_texte_devient_des_tuiles(void)
{
    int n = view_text("AZ09-.!?:'", line, 16);
    T_EQ(n, 10);
    T_EQ(line[0], TILE_LETTER_A);
    T_EQ(line[1], TILE_LETTER_A + 25);
    T_EQ(line[2], TILE_DIGIT0);
    T_EQ(line[3], TILE_DIGIT0 + 9);
    T_EQ(line[4], TILE_DASH);
    T_EQ(line[5], TILE_DOT);
    T_EQ(line[6], TILE_EXCL);
    T_EQ(line[7], TILE_QUEST);
    T_EQ(line[8], TILE_COLON);
    T_EQ(line[9], TILE_APOS);
    T_EQ(line[10], TILE_EMPTY);   /* au-delà de la chaîne : vide */
    T_EQ(line[15], TILE_EMPTY);
}

static void test_les_caracteres_inconnus_sont_vides(void)
{
    int i;
    for (i = 0; i < 16; i++) line[i] = 0x55;    /* RAM non remise à zéro */
    view_text("a b#~", line, 16);
    T_EQ(line[0], TILE_EMPTY);    /* minuscule */
    T_EQ(line[1], TILE_EMPTY);    /* espace */
    T_EQ(line[2], TILE_EMPTY);
    T_EQ(line[3], TILE_EMPTY);    /* # */
    T_EQ(line[4], TILE_EMPTY);    /* ~ */
    T_EQ(line[5], TILE_EMPTY);
}

static void test_le_texte_est_tronque_a_la_largeur(void)
{
    int n;
    line[3] = 0x55;
    n = view_text("ABCDEF", line, 3);
    T_EQ(n, 3);
    T_EQ(line[2], TILE_LETTER_A + 2);
    T_EQ(line[3], 0x55);          /* rien écrit au-delà de width */
}

static void test_une_largeur_nulle_n_ecrit_rien(void)
{
    line[0] = 0x55;
    T_EQ(view_text("ABC", line, 0), 0);
    T_EQ(view_text("ABC", line, -4), 0);
    T_EQ(line[0], 0x55);
}

static void test_la_virgule_a_sa_tuile(void)
{
    view_text("A,B", line, 3);
    T_EQ(line[0], TILE_LETTER_A);
    T_EQ(line[1], TILE_COMMA);
    T_EQ(line[2], TILE_LETTER_A + 1);
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
    T_RUN(test_le_bandeau_de_fin_montre_le_vainqueur);
    T_RUN(test_le_bandeau_de_fin_montre_les_populations_finales);
    T_RUN(test_le_texte_devient_des_tuiles);
    T_RUN(test_les_caracteres_inconnus_sont_vides);
    T_RUN(test_le_texte_est_tronque_a_la_largeur);
    T_RUN(test_une_largeur_nulle_n_ecrit_rien);
    T_RUN(test_la_virgule_a_sa_tuile);
    T_RUN(test_le_titre_s_ecrit_en_grand);
    T_RUN(test_le_menu_affiche_titre_et_modes);
    T_RUN(test_le_menu_marque_la_ligne_choisie);
    T_RUN(test_le_menu_hors_bornes_n_a_pas_de_disque);
    T_RUN(test_le_menu_ne_dessine_rien_d_autre);
}
