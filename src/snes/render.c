#include <snes.h>
#include "render.h"
#include "view.h"

/* Une entrée de tilemap SNES tient sur 16 bits : bits 0 à 9 le numéro de
   tuile, 10 à 12 la palette, 13 la priorité, 14 et 15 les miroirs. On
   n'utilise que la palette 0, donc l'entrée vaut le numéro de tuile. */
static unsigned short map_bg1[32 * 32];
static u8 grid[BOARD_H][BOARD_W];

/* Symboles produits par gfx4snes (data/tiles.pic, data/tiles.pal) puis
   assemblés dans la ROM par src/snes/tiles.asm. Orthographe consignée dans
   docs/snes-notes.md, vérifiée à la tâche 8 : <nom-du-fichier>_til /
   _tilend / _pal / _palend, où <nom-du-fichier> est "tiles" (sans le
   chemin, ni l'extension) tiré de data/tiles.bmp. */
extern char tiles_til, tiles_tilend;
extern char tiles_pal, tiles_palend;

/* Adresses VRAM (word addresses) : jeu de tuiles à 0x4000, tilemap de BG1 à
   0x0000, comme dans l'exemple Mode1 de PVSnesLib. 32 tuiles * 32 octets =
   1024 octets pour les tuiles ; 32x32 entrées * 2 octets = 2048 octets pour
   la tilemap : les deux zones ne se recouvrent pas. */
#define TILES_VRAM_ADDR 0x4000
#define MAP_VRAM_ADDR   0x0000

void render_init(void)
{
    /* Séquence exacte consignée dans docs/snes-notes.md à la tâche 0,
       vérifiée à la tâche 8 : chargement du jeu de tuiles et de la
       palette en VRAM (bgInitTileSet), adresse de la tilemap de BG1
       (bgSetMapPtr), puis passage en mode 1 avec BG2/BG3 désactivés
       (BG1 seul est utilisé ici ; BG2 reste pour la tâche 9). */
    bgInitTileSet(0, &tiles_til, &tiles_pal, 0,
                  (u16)(&tiles_tilend - &tiles_til),
                  (u16)(&tiles_palend - &tiles_pal),
                  BG_16COLORS, TILES_VRAM_ADDR);
    bgSetMapPtr(0, MAP_VRAM_ADDR, SC_32x32);

    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);

    setScreenOn();
}

void render_board_now(const Match *m, bool_t show_range)
{
    int x, y;
    view_board(m, show_range, grid);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            map_bg1[y * 32 + x] = (unsigned short)grid[y][x];
        }
    }
    /* Transfert DMA de map_bg1 vers l'adresse de tilemap de BG1, pendant le
       VBlank pour éviter tout déchirement visible. */
    WaitForVBlank();
    dmaCopyVram((u8 *)map_bg1, MAP_VRAM_ADDR, sizeof(map_bg1));
}
