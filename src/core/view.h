#ifndef VIEW_H
#define VIEW_H

#include "match.h"

#define TILE_EMPTY   0
#define TILE_RANGE   1
#define TILE_P1      2
#define TILE_P2      3
#define TILE_DIGIT0  4    /* 4 à 13 : chiffres 0 à 9 */
#define TILE_PIP_ON  14
#define TILE_PIP_OFF 15
#define TILE_R       16
#define TILE_SLASH   17
#define TILE_TIMES   18
#define TILE_P       19

#define HUD_W 32          /* le bandeau fait une seule ligne de tuiles */

void view_board(const Match *m, bool_t show_range, u8 out[BOARD_H][BOARD_W]);
void view_digits3(int value, u8 out[3]);
void view_hud(const Match *m, u8 out[HUD_W]);

/* Menu dessiné sur la zone de grille. `selected` va de 0 à 2 :
   0 = deux joueurs, 1 = contre CPU facile, 2 = contre CPU normal. */
void view_menu(int selected, u8 out[BOARD_H][BOARD_W]);

/* Bandeau de fin de partie : la couleur du vainqueur répétée au centre. */
void view_result_banner(Winner w, u8 out[HUD_W]);

#endif
