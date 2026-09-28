#include <string.h>
#include "harness.h"
#include "tutorial.h"

static TutPlayer p;
static Match m, ref;
static u8 cap[3][HUD_W];

/* Joue depuis le début jusqu'à la pause finale de la leçon n. */
static void play_to_end_of_lesson(int n)
{
    int guard = 0;
    tut_start(&p, &m);
    while (!(p.lesson == n && tut_lesson_done(&p)) && !p.done && guard < 6000) {
        tut_update(&p, &m, 1);
        guard++;
    }
    T_EQ(p.lesson, n);
    T_FALSE(p.done);
}

static int count(Cell who)
{
    return board_count(&m.board, who);
}

static void test_lecon_1_position_de_depart(void)
{
    int x, y, same = 1;
    play_to_end_of_lesson(1);
    match_start(&ref);
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            if (board_get(&m.board, x, y) != board_get(&ref.board, x, y)) same = 0;
    T_TRUE(same);
    T_EQ(p.caption, 0);
}

static void test_lecon_2_la_cellule_seule_meurt(void)
{
    play_to_end_of_lesson(2);
    T_EQ(board_get(&m.board, 8, 10), CELL_EMPTY);
    T_EQ(board_get(&m.board, 20, 10), CELL_P1);
    T_EQ(board_get(&m.board, 21, 10), CELL_P1);
    T_EQ(board_get(&m.board, 20, 11), CELL_P1);
    T_EQ(board_get(&m.board, 21, 11), CELL_P1);
    T_EQ(count(CELL_P1), 4);
    T_EQ(match_winner(&m), WINNER_NONE);   /* une leçon n'est pas une partie */
}

static void test_lecon_3_la_surpopulation_tue(void)
{
    play_to_end_of_lesson(3);
    T_EQ(board_get(&m.board, 15, 11), CELL_EMPTY);
    T_EQ(match_winner(&m), WINNER_NONE);
}

static void test_lecon_4_la_naissance_prend_la_majorite(void)
{
    play_to_end_of_lesson(4);
    T_EQ(board_get(&m.board, 15, 11), CELL_P1);
    T_EQ(match_winner(&m), WINNER_NONE);
}

static void test_lecon_5_trois_poses_puis_la_main_au_rouge(void)
{
    play_to_end_of_lesson(5);
    T_EQ(board_get(&m.board, 8, 14), CELL_P1);
    T_EQ(board_get(&m.board, 8, 15), CELL_P1);
    T_EQ(board_get(&m.board, 9, 14), CELL_P1);
    T_EQ(m.turn, CELL_P2);
    T_EQ(m.round, 1);
}

static void test_lecon_6_la_portee_reste_figee(void)
{
    play_to_end_of_lesson(6);
    T_EQ(board_get(&m.board, 8, 14), CELL_P1);
    T_EQ(m.range_mask[14][10], 0);
    T_TRUE(p.show_range);
}

static void test_lecon_7_le_planeur_traverse_le_bord(void)
{
    int y, near_left = 0;
    play_to_end_of_lesson(7);
    T_EQ(count(CELL_P1), 5);
    for (y = 0; y < BOARD_H; y++)
        if (board_get(&m.board, 0, y) == CELL_P1 ||
            board_get(&m.board, 1, y) == CELL_P1 ||
            board_get(&m.board, 2, y) == CELL_P1) near_left = 1;
    T_TRUE(near_left);
    T_EQ(match_winner(&m), WINNER_NONE);
}

static void test_lecon_8_l_emballement(void)
{
    play_to_end_of_lesson(8);
    T_EQ(m.round, 17);
    T_EQ(match_ticks_this_round(&m), 2);
    T_EQ(match_winner(&m), WINNER_NONE);
}

static void test_lecon_9_la_victoire(void)
{
    play_to_end_of_lesson(9);
    T_EQ(match_winner(&m), WINNER_P1);
}

static void test_le_tutoriel_se_termine(void)
{
    int frames = 0;
    tut_start(&p, &m);
    while (!p.done && frames < 5000) {
        tut_update(&p, &m, 1);
        frames++;
    }
    T_TRUE(p.done);
    T_TRUE(frames < 5000);
}

/* Même lecteur et même partie, champ par champ. */
static TutPlayer p2;

static int same_state(const TutPlayer *a, const Match *ma, const TutPlayer *b, const Match *mb)
{
    int x, y;
    if (a->pc != b->pc || a->wait != b->wait || a->lesson != b->lesson ||
        a->placements != b->placements || a->generations != b->generations ||
        a->caption != b->caption || a->cursor_x != b->cursor_x ||
        a->cursor_y != b->cursor_y || a->cursor_on != b->cursor_on ||
        a->show_range != b->show_range || a->done != b->done) return 0;
    if (ma->round != mb->round || ma->turn != mb->turn ||
        ma->winner != mb->winner || ma->placed != mb->placed) return 0;
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            if (board_get(&ma->board, x, y) != board_get(&mb->board, x, y)) return 0;
    return 1;
}

static void test_le_decoupage_des_images_ne_change_rien(void)
{
    /* Le lecteur découpé avance en parallèle d'une référence image par
       image ; l'état complet doit coïncider après chaque morceau. */
    static const int chunks[3] = { 7, 1000, 1 };
    int c, k, diverged, guard;
    for (c = 0; c < 3; c++) {
        tut_start(&p, &m);
        tut_start(&p2, &ref);
        diverged = 0;
        guard = 0;
        while (!p2.done && guard < 6000) {
            tut_update(&p, &m, chunks[c]);
            for (k = 0; k < chunks[c]; k++) tut_update(&p2, &ref, 1);
            if (!same_state(&p, &m, &p2, &ref)) diverged = 1;
            guard++;
        }
        T_FALSE(diverged);
        T_TRUE(p.done);
    }
}

static void test_passer_mene_au_debut_de_chaque_lecon(void)
{
    int n;
    tut_start(&p, &m);
    T_EQ(p.lesson, 1);
    for (n = 2; n <= TUT_LESSONS; n++) {
        tut_skip(&p, &m);
        T_EQ(p.lesson, n);
        T_EQ(p.caption, n - 1);
        T_FALSE(p.done);
    }
}

static void test_passer_au_dela_de_la_derniere_lecon_termine(void)
{
    int n;
    tut_start(&p, &m);
    for (n = 1; n < TUT_LESSONS; n++) tut_skip(&p, &m);
    tut_skip(&p, &m);
    T_TRUE(p.done);
    tut_skip(&p, &m);                  /* encore : sans effet */
    T_TRUE(p.done);
    T_FALSE(tut_update(&p, &m, 10));
}

static void test_le_tutoriel_ne_depend_pas_de_la_memoire(void)
{
    /* Un départ depuis une mémoire remplie de 0x55 suit exactement un
       départ propre, image par image, jusqu'à la fin. */
    int diverged = 0, guard = 0;
    memset(&p2, 0, sizeof(p2));
    memset(&ref, 0, sizeof(ref));
    tut_start(&p2, &ref);
    memset(&p, 0x55, sizeof(p));
    memset(&m, 0x55, sizeof(m));
    tut_start(&p, &m);
    if (!same_state(&p, &m, &p2, &ref)) diverged = 1;
    while (!p2.done && guard < 6000) {
        tut_update(&p, &m, 1);
        tut_update(&p2, &ref, 1);
        if (!same_state(&p, &m, &p2, &ref)) diverged = 1;
        guard++;
    }
    T_FALSE(diverged);
    T_TRUE(p.done);
}

static void test_les_textes_tiennent_sur_trois_lignes(void)
{
    int i, l;
    for (i = 0; i < TUT_LESSONS; i++)
        for (l = 0; l < 3; l++)
            T_TRUE(strlen(tut_caption_line(i, l)) <= HUD_W);
    T_TRUE(strcmp(tut_caption_line(3, 2), "MAJORITY COLOUR.") == 0);
    T_TRUE(strcmp(tut_caption_line(0, 0), "TWO COLONIES SHARE A SMALL") == 0);
    T_TRUE(strcmp(tut_caption_line(8, 0), "NO CELLS LEFT: YOU WIN.") == 0);
}

static void test_un_texte_hors_bornes_est_vide(void)
{
    int l, x, empty = 1;
    view_caption(-1, cap);
    for (l = 0; l < 3; l++)
        for (x = 0; x < HUD_W; x++)
            if (cap[l][x] != TILE_EMPTY) empty = 0;
    view_caption(TUT_LESSONS, cap);
    for (l = 0; l < 3; l++)
        for (x = 0; x < HUD_W; x++)
            if (cap[l][x] != TILE_EMPTY) empty = 0;
    T_TRUE(empty);
    view_caption(1, cap);
    T_EQ(cap[0][0], TILE_LETTER_A);             /* "A CELL WITH…" */
    /* la virgule de "LIVES ON. ALONE, IT DIES." */
    T_EQ(cap[1][(int)(strchr(tut_caption_line(1, 1), ',') - tut_caption_line(1, 1))], TILE_COMMA);
}

static void test_le_lecteur_compte_les_poses(void)
{
    int before;
    play_to_end_of_lesson(4);
    before = p.placements;
    while (!(p.lesson == 5 && tut_lesson_done(&p))) tut_update(&p, &m, 1);
    T_EQ(p.placements - before, 3);            /* leçon 5 : trois poses */
}

static void test_le_lecteur_compte_les_generations(void)
{
    int before;
    play_to_end_of_lesson(6);
    before = p.generations;
    while (!(p.lesson == 7 && tut_lesson_done(&p))) tut_update(&p, &m, 1);
    T_EQ(p.generations - before, 12);          /* leçon 7 : douze générations */
    before = p.generations;
    while (!(p.lesson == 8 && tut_lesson_done(&p))) tut_update(&p, &m, 1);
    T_EQ(p.generations - before, 2);           /* leçon 8 : deux au round 16 */
}

static void test_les_compteurs_partent_de_zero(void)
{
    memset(&p, 0x55, sizeof(p));
    tut_start(&p, &m);
    T_EQ(p.placements, 0);
    T_EQ(p.generations, 0);
}

void suite_tutorial(void)
{
    T_RUN(test_le_lecteur_compte_les_poses);
    T_RUN(test_le_lecteur_compte_les_generations);
    T_RUN(test_les_compteurs_partent_de_zero);
    T_RUN(test_lecon_1_position_de_depart);
    T_RUN(test_lecon_2_la_cellule_seule_meurt);
    T_RUN(test_lecon_3_la_surpopulation_tue);
    T_RUN(test_lecon_4_la_naissance_prend_la_majorite);
    T_RUN(test_lecon_5_trois_poses_puis_la_main_au_rouge);
    T_RUN(test_lecon_6_la_portee_reste_figee);
    T_RUN(test_lecon_7_le_planeur_traverse_le_bord);
    T_RUN(test_lecon_8_l_emballement);
    T_RUN(test_lecon_9_la_victoire);
    T_RUN(test_le_tutoriel_se_termine);
    T_RUN(test_le_decoupage_des_images_ne_change_rien);
    T_RUN(test_passer_mene_au_debut_de_chaque_lecon);
    T_RUN(test_passer_au_dela_de_la_derniere_lecon_termine);
    T_RUN(test_le_tutoriel_ne_depend_pas_de_la_memoire);
    T_RUN(test_les_textes_tiennent_sur_trois_lignes);
    T_RUN(test_un_texte_hors_bornes_est_vide);
}
