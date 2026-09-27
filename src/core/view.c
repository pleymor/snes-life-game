#include "view.h"

/* m->range_mask est déjà tenu à jour pour tout le tour par begin_turn()
   (match.c) : le lire directement évite un appel à rules_range_mask() par
   rafraîchissement et, avant ça, un rules_in_range() par case vide.
   board_get(&m->board, x, y) est également remplacé par un pointeur de
   ligne hoisté hors de la boucle sur x (une seule multiplication par ligne
   au lieu d'une par case, via BSTRIDE — voir board.c). */
void view_board(const Match *m, bool_t show_range, u8 out[BOARD_H][BOARD_W])
{
    int x, y;

    for (y = 0; y < BOARD_H; y++) {
        const u8 *row = &m->board.c[y + 1][1];
        for (x = 0; x < BOARD_W; x++) {
            u8 v = row[x];
            if (v == (u8)CELL_P1) {
                out[y][x] = TILE_P1;
            } else if (v == (u8)CELL_P2) {
                out[y][x] = TILE_P2;
            } else if (show_range && m->range_mask[y][x]) {
                out[y][x] = TILE_RANGE;
            } else {
                out[y][x] = TILE_EMPTY;
            }
        }
    }
}

void view_digits3(int value, u8 out[3])
{
    if (value < 0)   value = 0;
    if (value > 999) value = 999;
    out[0] = (u8)(TILE_DIGIT0 + value / 100);
    out[1] = (u8)(TILE_DIGIT0 + (value / 10) % 10);
    out[2] = (u8)(TILE_DIGIT0 + value % 10);
}

static void pips(const Match *m, Cell who, u8 *out)
{
    int left = (m->turn == who) ? (BUDGET - m->placed) : BUDGET;
    int i;
    for (i = 0; i < BUDGET; i++) {
        out[i] = (u8)(i < left ? TILE_PIP_ON : TILE_PIP_OFF);
    }
}

void view_hud(const Match *m, u8 out[HUD_W])
{
    int i;
    int ticks = match_ticks_this_round(m);
    int p1, p2;

    for (i = 0; i < HUD_W; i++) {
        out[i] = TILE_EMPTY;
    }

    /* Un seul passage des 768 cases pour les deux couleurs, plutôt que
       deux appels séparés à board_count(). */
    board_count_pair(&m->board, &p1, &p2);

    out[0]  = TILE_P1;
    out[31] = TILE_P2;
    view_digits3(p1, &out[2]);
    view_digits3(p2, &out[27]);
    pips(m, CELL_P1, &out[6]);
    pips(m, CELL_P2, &out[23]);

    out[12] = TILE_R;
    view_digits3(m->round, &out[13]);
    out[16] = TILE_SLASH;
    view_digits3(ROUND_CAP, &out[17]);

    if (ticks > 1) {
        out[20] = TILE_TIMES;
        out[21] = (u8)(TILE_DIGIT0 + ticks);
    }
}

static u8 text_tile(char c)
{
    if (c >= 'A' && c <= 'Z') return (u8)(TILE_LETTER_A + (c - 'A'));
    if (c >= '0' && c <= '9') return (u8)(TILE_DIGIT0 + (c - '0'));
    switch (c) {
    case '-':  return TILE_DASH;
    case '.':  return TILE_DOT;
    case '!':  return TILE_EXCL;
    case '?':  return TILE_QUEST;
    case ':':  return TILE_COLON;
    case '\'': return TILE_APOS;
    default:   return TILE_EMPTY;
    }
}

int view_text(const char *s, u8 *out, int width)
{
    int i = 0, n;
    while (i < width && s[i] != '\0') {
        out[i] = text_tile(s[i]);
        i++;
    }
    n = i;
    while (i < width) {
        out[i] = TILE_EMPTY;
        i++;
    }
    return n;
}

/* Lignes de tuile des trois choix du menu (deux joueurs, CPU facile, CPU
   normal). Portée fichier, à une dimension : va en ROM (.rodata), pas dans
   globram.data (docs/snes-notes.md § 10 — un `static const` local à une
   fonction, comme cette table l'était avant, y serait allé à la place). */
static const u8 menu_rows[3] = { 10, 12, 14 };

void view_menu(int selected, u8 out[BOARD_H][BOARD_W])
{
    int x, y, i;

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            out[y][x] = TILE_EMPTY;
        }
    }
    for (i = 0; i < 3; i++) {
        u8 r = menu_rows[i];
        out[r][10] = (u8)((i == selected) ? TILE_PIP_ON : TILE_EMPTY);
        out[r][12] = (u8)(TILE_DIGIT0 + (i == 0 ? 2 : 1));
        out[r][13] = TILE_P;
        if (i > 0) {
            out[r][14] = TILE_TIMES;
            out[r][15] = (u8)(TILE_DIGIT0 + i);   /* 1 facile, 2 normal */
        }
    }
}

void view_result_banner(const Match *m, u8 out[HUD_W])
{
    int i, p1, p2;
    Winner w = match_winner(m);

    for (i = 0; i < HUD_W; i++) {
        out[i] = TILE_EMPTY;
    }

    out[0]  = TILE_P1;
    out[31] = TILE_P2;
    board_count_pair(&m->board, &p1, &p2);
    view_digits3(p1, &out[2]);
    view_digits3(p2, &out[27]);

    for (i = 12; i < 20; i++) {
        if (w == WINNER_P1)      out[i] = TILE_P1;
        else if (w == WINNER_P2) out[i] = TILE_P2;
        else                     out[i] = (u8)((i & 1) ? TILE_P2 : TILE_P1);
    }
}
