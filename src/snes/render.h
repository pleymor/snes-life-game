#ifndef RENDER_H
#define RENDER_H

#include "match.h"
#include "view.h"   /* HUD_W */

void render_init(void);

/* Ces trois fonctions ne font que préparer leurs tampons en mémoire (hors
   VBlank) et marquer un transfert DMA comme en attente ; elles n'accèdent
   jamais elles-mêmes à la VRAM ni n'attendent le VBlank. render_board_now()
   (768 cases) prend plus de temps que la fenêtre de VBlank : un DMA
   déclenché depuis cette fonction tomberait pendant l'affichage actif et
   produirait un déchirement visible. */
void render_board_now(const Match *m, bool_t show_range);

/* Prépare la tilemap de BG1 à partir d'une grille déjà construite (tâche
   12 : menu, plateau final de l'écran de fin), au lieu de la construire
   elle-même via view_board() comme le fait render_board_now() ci-dessus.
   screens.c doit passer par cette fonction plutôt que de toucher la VRAM
   directement : elle construit sa grille avec view_menu()/view_board(),
   puis la remet à ce module. Même discipline VBlank et même copie ligne
   par ligne que render_board_now() (qui s'appuie d'ailleurs dessus) : ne
   fait que préparer map_bg1 et lever board_pending.

   Pas de `const` sur `grid` malgré une lecture seule : 816-tcc rend
   « assignment from incompatible pointer type » pour l'ajout d'un `const`
   sur un paramètre tableau à deux dimensions (u8[][BOARD_W]), avertissement
   absent sur un tableau à une dimension (voir render_hud_from_row()
   ci-dessous, dont le paramètre est bien `const`). */
void render_board_from_grid(u8 grid[BOARD_H][BOARD_W]);

/* Prépare la ligne de bandeau de BG2 pour le prochain render_vblank().
   Deux chemins internes, pour un coût constant par frame (fix round 1,
   tâche 9) :
     - coûteux (deux balayages du plateau de 768 cases dans view_hud()) :
       seulement si le bandeau a été marqué à reconstruire depuis le
       dernier appel (voir render_hud_dirty() ci-dessous) ;
     - bon marché, à chaque appel : ne fait que basculer la tuile de
       l'icône du joueur actif (index 0 ou 31 du bandeau) entre sa couleur
       mise en cache et TILE_EMPTY, selon `blink_on`.
   Appeler render_hud_now() une fois par frame reste donc correct et bon
   marché même si rien d'autre que `blink_on` n'a changé — ce que fait
   main.c. */
void render_hud_now(const Match *m, bool_t blink_on);

/* Prépare la ligne de bandeau de BG2 à partir d'une rangée déjà construite
   (tâche 12 : bandeau de fin de partie, ligne vide du menu), au lieu de la
   construire elle-même via view_hud() comme le fait render_hud_now()
   ci-dessus (qui s'appuie d'ailleurs dessus pour son propre chemin
   coûteux). Même discipline VBlank : ne fait que préparer map_bg2 et lever
   hud_pending. Ne touche pas au clignotement bon marché de l'icône de
   render_hud_now() (propre au bandeau de jeu) : l'écran de fin fait
   clignoter tout son bandeau lui-même, en rappelant cette fonction tour à
   tour avec la ligne du bandeau puis une ligne vide. */
void render_hud_from_row(const u8 row[HUD_W]);

/* Marque le bandeau à reconstruire : le prochain render_hud_now() rappelle
   view_hud() au lieu de se contenter du clignotement bon marché. À appeler
   au démarrage (déjà fait, l'état initial est "à reconstruire") et, à
   partir de la tâche 10, partout où board_dirty est levé dans main.c (une
   pose, une annulation ou une fin de tour changent aussi les effectifs, les
   pastilles de budget et le round affichés par le bandeau). */
void render_hud_dirty(void);

/* Place le sprite du curseur sur la case de jeu (x, y). Écrit directement
   dans le tampon RAM `oamMemory` de PVSnesLib (via oamSet/oamSetEx), ce qui
   est sûr hors VBlank : la routine d'interruption NMI de PVSnesLib
   transfère elle-même ce tampon vers l'OAM du PPU à chaque VBlank sans
   frame perdue (voir docs/snes-notes.md). Rien à faire ici dans
   render_vblank() pour le curseur. */
void render_cursor(int x, int y, bool_t visible);

/* À appeler une fois par frame, après WaitForVBlank() et avant de reprendre
   la boucle de jeu : exécute tous les transferts DMA préparés depuis le
   dernier appel (tilemap de BG1, ligne de bandeau de BG2), puis efface les
   indicateurs "en attente". C'est la seule fonction de ce module qui écrit
   en VRAM ; l'appeler en dehors du VBlank corromprait l'affichage. */
void render_vblank(void);

#ifdef AI_MEASURE_FRAMES
/* Build de mesure seulement (`make rom-measure`) : écrit `value` sur quatre
   chiffres (plafonné à 9999) dans la ligne de bandeau, à partir de la
   colonne `at`, par-dessus ce que render_hud_now() y a mis. À rappeler à
   chaque frame après render_hud_now(), dont une reconstruction effacerait
   sinon ces chiffres. */
void render_hud_number4(int at, unsigned int value);
#endif

#endif
