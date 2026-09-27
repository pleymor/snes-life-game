/*---------------------------------------------------------------------------------

    The mode menu and the result screen, bracketing the game loop of
    main.c.

    Both loops below follow the exact same model as main.c's game loop:
    logic -> prepare (render_board_from_grid()/render_hud_from_row(), the
    generic counterparts of render_board_now()/render_hud_now()) ->
    WaitForVBlank() -> render_vblank(). Neither loop ever touches VRAM
    itself: view_menu()/view_board()/view_result_banner() (src/core/view.c)
    build the tile grids and rows, render.c owns every DMA transfer.

    Input is edge-triggered (input_edges(), src/snes/input.c), shared with
    input_update() so both the scripted-replay table (INPUT_SCRIPT) and a
    real pad stay a single continuous timeline across menu -> game ->
    result -> menu again.

---------------------------------------------------------------------------------*/
#include <snes.h>
#include "screens.h"
#include "render.h"
#include "view.h"
#include "input.h"
#include "ai.h"

/* Bandeau de fin de partie : une alternance toutes les trente images
   réelles entre la couleur du vainqueur et une ligne vide. */
#define RESULT_BLINK_PERIOD 30

int screen_menu(unsigned int *frames)
{
    /* Le plateau (768 octets) et la ligne de bandeau ne tiennent pas sur
       la pile (règle du projet : aucune automatique de plus de 64 octets),
       et sont donc en portée fichier, comme partout ailleurs (render.c,
       main.c). Posés explicitement à chaque appel ci-dessous : la RAM
       n'est pas remise à zéro (docs/snes-notes.md § 10), et cette fonction
       est rappelée à chaque retour au menu. */
    static u8 grid[BOARD_H][BOARD_W];
    static u8 hud_row[HUD_W];
    int selected = 0;
    bool_t dirty = TRUE;   /* force la toute première image */
    unsigned int frame = 0;
    int i;

    /* Le bandeau du menu reste vide tout du long, la sélection se lit sur
       le plateau (view_menu()) : préparé une seule fois, jamais reconstruit
       ensuite. */
    for (i = 0; i < HUD_W; i++) hud_row[i] = TILE_EMPTY;
    render_hud_from_row(hud_row);
    /* Curseur de jeu caché sur le menu. */
    render_cursor(0, 0, FALSE);

    for (;;) {
        unsigned short hit = input_edges((unsigned short *)0);

        if ((hit & KEY_UP) && selected > 0)   { selected--; dirty = TRUE; }
        if ((hit & KEY_DOWN) && selected < 2) { selected++; dirty = TRUE; }

        if (dirty) {
            view_menu(selected, grid);
            render_board_from_grid(grid);
            dirty = FALSE;
        }
        render_cursor(0, 0, FALSE);

        frame++;
        WaitForVBlank();
        render_vblank();

        if (hit & (KEY_A | KEY_START)) {
            *frames = frame;
            if (selected == 0) return -1;
            return (selected == 1) ? (int)AI_EASY : (int)AI_NORMAL;
        }
    }
}

void screen_result(const Match *m)
{
    static u8 grid[BOARD_H][BOARD_W];
    static u8 banner_row[HUD_W];
    static u8 blank_row[HUD_W];
    /* Période courante de snes_vblank_count (images réelles écoulées / 30) :
       comparée d'un tour de boucle à l'autre, son changement dit quand
       basculer — pilotage par le temps réel plutôt que par les itérations
       de cette boucle (voir main.c pour le même choix sur le clignotement
       du HUD). */
    unsigned int last_period = (unsigned int)(u16)snes_vblank_count / RESULT_BLINK_PERIOD;
    bool_t blink_on = TRUE;   /* le bandeau démarre affiché */
    int i;

    /* Plateau final : construit une fois, jamais reconstruit (rien ne
       change plus une fois la partie terminée). Pas de marquage de
       portée : la partie est finie, il n'y a plus de tour en cours. */
    view_board(m, FALSE, grid);
    render_board_from_grid(grid);

    view_result_banner(m, banner_row);
    for (i = 0; i < HUD_W; i++) blank_row[i] = TILE_EMPTY;
    render_hud_from_row(banner_row);

    render_cursor(0, 0, FALSE);

    for (;;) {
        unsigned short hit = input_edges((unsigned short *)0);
        unsigned int period = (unsigned int)(u16)snes_vblank_count / RESULT_BLINK_PERIOD;

        if (period != last_period) {
            last_period = period;
            blink_on = (bool_t)!blink_on;
            render_hud_from_row(blink_on ? banner_row : blank_row);
        }
        render_cursor(0, 0, FALSE);

        WaitForVBlank();
        render_vblank();

        if (hit & KEY_START) return;
    }
}
