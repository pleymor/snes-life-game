#include <snes.h>
#include "render.h"
#include "view.h"

/* Une entrée de tilemap SNES tient sur 16 bits : bits 0 à 9 le numéro de
   tuile, 10 à 12 la palette, 13 la priorité, 14 et 15 les miroirs. On
   n'utilise que la palette 0, donc l'entrée vaut le numéro de tuile. */
static unsigned short map_bg1[32 * 32];
static unsigned short map_bg2[32 * 32];
static u8 grid[BOARD_H][BOARD_W];
static u8 hud[HUD_W];

/* Discipline VBlank : render_board_now()/render_hud_now() ne font que
   préparer map_bg1/map_bg2 et lever un indicateur "en attente" ; seule
   render_vblank(), appelée après WaitForVBlank() dans la boucle
   principale, transfère réellement ces tampons en VRAM par DMA. Trois
   WaitForVBlank() par frame diviseraient le taux d'affichage, et
   render_board_now() (768 cases) prend plus longtemps que la fenêtre de
   VBlank : un DMA déclenché juste après le calcul tomberait pendant
   l'affichage actif. */
static bool_t board_pending;
static bool_t hud_pending;
static bool_t caption_pending;

/* view_hud() balaie une fois les 768 cases du plateau (board_count_pair())
   rien que pour préparer le bandeau ; le rappeler à chaque frame pour un
   simple clignotement coûterait ce balayage 60 fois par seconde sur un
   65816, assez pour provoquer des "lag frames" (voir docs/snes-notes.md
   § 8). hud_dirty ne fait recalculer le bandeau par view_hud() (dans
   render_hud_now(), ci-dessous) qu'au démarrage et chaque fois que
   render_hud_dirty() est appelée (partout où main.c juge le plateau/HUD à
   reconstruire) ; entre deux recalculs, le clignotement ne fait que
   basculer la seule tuile de l'icône du joueur actif entre sa valeur mise
   en cache et TILE_EMPTY. */
static bool_t hud_dirty;

/* Symboles produits par gfx4snes (data/tiles.pic, data/tiles.pal) puis
   assemblés dans la ROM par src/snes/tiles.asm. Orthographe consignée dans
   docs/snes-notes.md, vérifiée à la tâche 8 : <nom-du-fichier>_til /
   _tilend / _pal / _palend, où <nom-du-fichier> est "tiles" (sans le
   chemin, ni l'extension) tiré de data/tiles.bmp. */
extern char tiles_til, tiles_tilend;
extern char tiles_pal, tiles_palend;

/* Même convention, pour data/sprites.bmp -> src/snes/sprites.asm. */
extern char sprites_til, sprites_tilend;
extern char sprites_pal, sprites_palend;

/* Carte VRAM (adresses "word", telles que passées à bgSetMapPtr/
   bgSetGfxPtr/oamInitGfxSet/dmaCopyVram) :
     0x0000 - 0x03FF  tilemap BG1 (le plateau)      32x32 entrées, 1024 mots
     0x0400 - 0x07FF  tilemap BG2 (le bandeau)       32x32 entrées, 1024 mots
     0x2000 - 0x200F  tuile du curseur (sprites)      1 tuile,        16 mots
     0x4000 - 0x47FF  jeu de tuiles de fond (BG1+BG2) 128 tuiles,   2048 mots
   Aucune zone ne recouvre une autre : les deux tilemaps se suivent, le jeu
   de tuiles de fond commence loin après, et la tuile de sprite est casée
   dans l'espace encore libre entre les deux, comme le fait l'exemple
   DynamicSprite de PVSnesLib (tuiles de sprite à 0x0000/0x1000, tuiles de
   fond à 0x2000 : même espace VRAM partagé, adressé en mots, pour les BG et
   les sprites). BG1 et BG2 partagent le même jeu de tuiles et la même
   palette (mode 1 : BG1/BG2 sont tous deux en 16 couleurs ; seul BG3, non
   utilisé ici, est en 4 couleurs) ; les sprites ont leur propre jeu de
   tuiles et leur propre palette (CGRAM objet, plage séparée de la palette
   de fond). */
#define TILES_VRAM_ADDR   0x4000
#define MAP_BG1_VRAM_ADDR 0x0000
#define MAP_BG2_VRAM_ADDR 0x0400
#define SPRITE_VRAM_ADDR  0x2000

/* Ligne de tilemap où le bandeau est écrit (sous la grille de jeu, qui
   n'occupe que les BOARD_H=24 premières lignes de tuiles). Pas de registre
   de défilement vertical : le bandeau est simplement écrit à demeure sur
   cette ligne de la tilemap de BG2. */
#define HUD_ROW 24
#define CAPTION_ROW 25

#define CURSOR_OAM_ID 0

void render_init(void)
{
    int i;

    /* La RAM n'est pas remise à zéro au démarrage (docs/snes-notes.md
       § 10) : tout l'état de ce module est posé ici. Le bandeau est à
       reconstruire (hud_dirty), rien n'est en attente de transfert. */
    for (i = 0; i < 32 * 32; i++) {
        map_bg1[i] = TILE_EMPTY;
        map_bg2[i] = TILE_EMPTY;
    }
    board_pending = FALSE;
    hud_pending = FALSE;
    caption_pending = FALSE;
    hud_dirty = TRUE;

    /* Séquence exacte consignée dans docs/snes-notes.md à la tâche 0,
       vérifiée à la tâche 8 : chargement du jeu de tuiles et de la
       palette en VRAM (bgInitTileSet), adresse de la tilemap de BG1
       (bgSetMapPtr), puis passage en mode 1. */
    bgInitTileSet(0, &tiles_til, &tiles_pal, 0,
                  (u16)(&tiles_tilend - &tiles_til),
                  (u16)(&tiles_palend - &tiles_pal),
                  BG_16COLORS, TILES_VRAM_ADDR);
    bgSetMapPtr(0, MAP_BG1_VRAM_ADDR, SC_32x32);

    /* BG2 (le bandeau) réutilise le même jeu de tuiles et la même palette
       que BG1 (déjà chargés ci-dessus) : seule sa propre tilemap doit être
       posée, à une adresse VRAM distincte de celle de BG1. */
    bgSetGfxPtr(1, TILES_VRAM_ADDR);
    bgSetMapPtr(1, MAP_BG2_VRAM_ADDR, SC_32x32);

    setMode(BG_MODE1, 0);
    bgSetEnable(1);
    bgSetDisable(2);

    /* map_bg1/map_bg2 viennent d'être remplis de TILE_EMPTY, mais la VRAM
       elle-même ne l'est pas forcément à la mise sous tension : un
       transfert complet de chaque tilemap ici, pendant le forced blank
       (setScreenOn()
       n'a pas encore été appelé, l'accès VRAM est donc libre), garantit que
       toute la VRAM des deux tilemaps est à TILE_EMPTY avant que
       render_board_now()/render_vblank() ne se mettent à ne transférer que
       les BOARD_H lignes utilisées, et render_hud_now()/render_vblank() que
       la ligne HUD_ROW. Sans ce transfert initial, les lignes 24 à 31 de la
       tilemap de BG1 (jamais réécrites ensuite) garderaient un contenu
       indéterminé, visible par-dessus le bandeau de BG2 (même priorité,
       BG1 au-dessus en mode 1). */
    dmaCopyVram((u8 *)map_bg1, MAP_BG1_VRAM_ADDR, sizeof(map_bg1));
    dmaCopyVram((u8 *)map_bg2, MAP_BG2_VRAM_ADDR, sizeof(map_bg2));

    /* Jeu de tuiles et palette du curseur : zone VRAM et palette séparées
       de celles du fond (voir la carte VRAM ci-dessus). */
    oamInitGfxSet((u8 *)&sprites_til,
                  (u16)(&sprites_tilend - &sprites_til),
                  (u8 *)&sprites_pal,
                  (u16)(&sprites_palend - &sprites_pal),
                  0, SPRITE_VRAM_ADDR, OBJ_SIZE8_L16);

    setScreenOn();
}

void render_board_from_grid(u8 grid_in[BOARD_H][BOARD_W])
{
    int x, y;
    /* Pointeurs de ligne hoistés hors de la boucle sur x : map_bg1 et
       grid_in sont tous deux de largeur 32 (une
       puissance de deux, donc déjà un simple décalage plutôt qu'une vraie
       multiplication), mais hoister évite de refaire ce calcul d'adresse
       à chaque case plutôt qu'une fois par ligne. */
    for (y = 0; y < BOARD_H; y++) {
        unsigned short *dst = &map_bg1[y << 5];
        const u8 *src = grid_in[y];
        for (x = 0; x < BOARD_W; x++) {
            dst[x] = (unsigned short)src[x];
        }
    }
    board_pending = TRUE;
}

void render_board_now(const Match *m, bool_t show_range)
{
    view_board(m, show_range, grid);
    render_board_from_grid(grid);
}

void render_hud_dirty(void)
{
    hud_dirty = TRUE;
}

void render_hud_from_row(const u8 row[HUD_W])
{
    int i;
    for (i = 0; i < HUD_W; i++) {
        map_bg2[HUD_ROW * 32 + i] = (unsigned short)row[i];
    }
    hud_pending = TRUE;
}

void render_caption(u8 rows[3][HUD_W])
{
    int r, i;
    for (r = 0; r < 3; r++) {
        unsigned short *dst = &map_bg2[(CAPTION_ROW + r) * 32];
        const u8 *src = rows[r];
        for (i = 0; i < HUD_W; i++) dst[i] = (unsigned short)src[i];
    }
    caption_pending = TRUE;
}

void render_hud_now(const Match *m, bool_t blink_on)
{
    int icon_index;

    if (hud_dirty) {
        /* Chemin coûteux (deux balayages de 768 cases dans view_hud()) :
           seulement au démarrage, puis chaque fois que render_hud_dirty()
           a été appelée depuis le dernier appel. */
        view_hud(m, hud);
        render_hud_from_row(hud);
        hud_dirty = FALSE;
    }

    /* Chemin bon marché, exécuté à chaque appel : la pastille du joueur
       actif s'éteint une alternance sur deux, c'est ce qui signale à qui
       est le tour. `hud[icon_index]` garde la couleur mise en cache par
       le dernier recalcul (jamais éteinte par view_hud() elle-même), donc
       aucune relecture du plateau n'est nécessaire pour l'allumer ou
       l'éteindre. */
    icon_index = (m->turn == CELL_P1) ? 0 : 31;
    map_bg2[HUD_ROW * 32 + icon_index] =
        (unsigned short)(blink_on ? hud[icon_index] : TILE_EMPTY);
    hud_pending = TRUE;
}

#ifdef AI_MEASURE_FRAMES
void render_hud_number4(int at, unsigned int value)
{
    unsigned short *out = &map_bg2[HUD_ROW * 32 + at];
    int i;
    if (value > 9999) value = 9999;
    for (i = 3; i >= 0; i--) {
        out[i] = (unsigned short)(TILE_DIGIT0 + value % 10);
        value /= 10;
    }
    hud_pending = TRUE;
}
#endif

void render_cursor(int x, int y, bool_t visible)
{
    /* oamSet() positionne toujours le sprite avant qu'oamSetEx() ne décide
       de le montrer ou de le cacher : nécessaire car OBJ_HIDE déplace le
       sprite à (255, 240), et le remontrer sans repositionner le laisserait
       hors écran (avertissement de sprite.h). Écrit dans oamMemory (RAM),
       pas dans le PPU lui-même : sûr à tout moment de la frame. */
    oamSet(CURSOR_OAM_ID, (u16)(x * 8), (u16)(y * 8), 3, 0, 0, 0, 0);
    oamSetEx(CURSOR_OAM_ID, OBJ_SMALL, visible ? OBJ_SHOW : OBJ_HIDE);
}

void render_vblank(void)
{
    /* Uniquement les BOARD_H=24 lignes utilisées de la tilemap de BG1 (les
       8 lignes restantes ne sont jamais écrites par render_board_now() et
       restent à TILE_EMPTY depuis le tout premier appel). */
    if (board_pending) {
        dmaCopyVram((u8 *)map_bg1, MAP_BG1_VRAM_ADDR,
                    (u16)(BOARD_H * 32 * sizeof(unsigned short)));
        board_pending = FALSE;
    }
    /* Seule la ligne HUD_ROW change d'une frame à l'autre : 32 entrées,
       64 octets, à l'adresse VRAM de cette ligne dans la tilemap de BG2. */
    if (hud_pending) {
        dmaCopyVram((u8 *)&map_bg2[HUD_ROW * 32],
                    (u16)(MAP_BG2_VRAM_ADDR + HUD_ROW * 32),
                    (u16)(HUD_W * sizeof(unsigned short)));
        hud_pending = FALSE;
    }
    /* Trois lignes de texte du tutoriel : 96 entrées, 192 octets. */
    if (caption_pending) {
        dmaCopyVram((u8 *)&map_bg2[CAPTION_ROW * 32],
                    (u16)(MAP_BG2_VRAM_ADDR + CAPTION_ROW * 32),
                    (u16)(3 * HUD_W * sizeof(unsigned short)));
        caption_pending = FALSE;
    }
    /* Rien à faire ici pour le curseur : la routine d'interruption NMI que
       PVSnesLib installe par défaut transfère elle-même oamMemory vers
       l'OAM du PPU à chaque VBlank qui n'est pas une "lag-frame"
       (snes/interrupt.h, section "VBlank ISR") ; render_cursor() écrit déjà
       dans oamMemory, donc ce transfert est automatique et n'a pas besoin
       d'être déclenché depuis ce module. */
}
