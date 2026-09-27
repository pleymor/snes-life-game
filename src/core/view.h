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

/* Construit la tilemap 32x24 de la grille de jeu : une tuile par case,
   couleur du joueur si occupée, TILE_RANGE si vide, à portée du joueur
   actif et `show_range` demandé, TILE_EMPTY sinon. */
void view_board(const Match *m, bool_t show_range, u8 out[BOARD_H][BOARD_W]);

/* `value` (borné à 0..999) sur trois tuiles de chiffre, calé à droite. */
void view_digits3(int value, u8 out[3]);

/* Construit la ligne de bandeau de jeu (32 tuiles) : pastille et population
   de chaque joueur, poses restantes en pastilles, round courant et
   marqueur d'emballement. */
void view_hud(const Match *m, u8 out[HUD_W]);

/* Menu dessiné sur la zone de grille. `selected` va de 0 à 2 :
   0 = deux joueurs, 1 = contre CPU facile, 2 = contre CPU normal. */
void view_menu(int selected, u8 out[BOARD_H][BOARD_W]);

/* Bandeau de fin de partie : la couleur du vainqueur répétée au centre
   (colonnes 12-19, clignotant à l'appel de screens.c), et les deux
   populations finales de `m` aux mêmes colonnes que view_hud() (2-4 et
   27-29), pour que le résultat reste lisible sans revenir au jeu. */
void view_result_banner(const Match *m, u8 out[HUD_W]);

#endif
