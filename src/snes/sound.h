#ifndef SOUND_HDR
#define SOUND_HDR

/* Seule couche qui appelle snesmod (PVSnesLib). */

typedef enum { MUSIC_ALONELY, MUSIC_OFFER, MUSIC_PURITY } Music;

/* Même ordre que les échantillons de data/audio/sfx.it (tools/mksfx.py). */
typedef enum { SFX_PLACE, SFX_REFUSE, SFX_UNDO, SFX_TICK,
               SFX_MENU, SFX_SELECT, SFX_WIN } Sfx;

/* Démarre le pilote son et déclare la banque. Une fois, au démarrage ;
   long (quelques images). Aucune musique n'est en cours ensuite. */
void sound_init(void);

/* Charge et lance `track` en boucle, avec les effets. Sans effet si
   `track` joue déjà : pas de coupure entre deux écrans qui le partagent. */
void sound_music(Music track);

/* Joue un effet, centré, sans couper la musique. */
void sound_sfx(Sfx effect);

/* Transmet les commandes en attente au processeur son. Une fois par tour
   de boucle, juste après render_vblank(). */
void sound_update(void);

#endif
