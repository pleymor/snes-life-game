#ifndef RENDER_H
#define RENDER_H

#include "match.h"

void render_init(void);

/* Ces trois fonctions ne font que préparer leurs tampons en mémoire (hors
   VBlank) et marquer un transfert DMA comme en attente ; elles n'accèdent
   jamais elles-mêmes à la VRAM ni n'attendent le VBlank. render_board_now()
   (768 cases) prend plus de temps que la fenêtre de VBlank : un DMA
   déclenché depuis cette fonction tomberait pendant l'affichage actif et
   produirait un déchirement visible. */
void render_board_now(const Match *m, bool_t show_range);

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

#endif
