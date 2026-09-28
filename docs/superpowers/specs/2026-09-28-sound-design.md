# Immigration — musique et effets sonores

Sous-projet 3 de trois, après la police et le menu (1) et le tutoriel (2).

## 1. But

Le jeu a une musique propre à chaque mode et des bruitages sur les actions du joueur, tous libres de droit, avec la licence de chaque fichier consignée dans le dépôt.

Critères de réussite :

- trois musiques du domaine public, chacune associée à un mode ;
- sept effets sonores CC0 sur les actions ;
- la musique ne coupe pas quand deux écrans consécutifs partagent le même morceau ;
- la mémoire son de la SNES (58 Ko pour musique et effets) n'est jamais dépassée ;
- le jeu garde son affichage et sa cadence : le son n'est qu'un ajout.

## 2. Les fichiers audio

### 2.1 Musiques

Modules de tracker `.it`, tous de l'artiste « Beyond », licence domaine public (Creative Commons Public Domain / CC0), vérifiée sur la fiche de chaque module de The Mod Archive le 2026-09-28.

| Fichier | Titre | Canaux | Taille | Mod Archive id | Utilisé pour |
|---|---|---|---|---|---|
| `data/audio/alonely.it` | Alonely | 4 | 4 327 o | 172186 | menu, `2 PLAYERS`, `HOW TO PLAY` |
| `data/audio/offerthelight.it` | Offer the light | 6 | 8 456 o | 213276 | `VS CPU  EASY` |
| `data/audio/purity.it` | Purity | 6 | 30 715 o | 213554 | `VS CPU  HARD` |

Les fichiers sont versionnés tels que téléchargés, sans modification.

### 2.2 Effets

Tirés du pack *Digital Audio* de Kenney (kenney.nl), licence CC0 (fichier `License.txt` du pack), téléchargé le 2026-09-28.

| Effet | Fichier source | Fichier du dépôt |
|---|---|---|
| pose | `pepSound1.ogg` | `data/audio/sfx/place.wav` |
| refus | `lowDown.ogg` | `data/audio/sfx/refuse.wav` |
| annulation | `phaserDown1.ogg` | `data/audio/sfx/undo.wav` |
| génération | `twoTone1.ogg` | `data/audio/sfx/tick.wav` |
| menu | `tone1.ogg` | `data/audio/sfx/menu.wav` |
| validation | `highUp.ogg` | `data/audio/sfx/select.wav` |
| victoire | `powerUp8.ogg` | `data/audio/sfx/win.wav` |

Chaque WAV du dépôt est mono, 8 bits ou 16 bits, rééchantillonné à **8 000 Hz**. À cette fréquence, les sept effets tiennent en environ 19 Ko une fois convertis au format BRR de la SNES, contre 38 Ko à 16 kHz. Les WAV sont produits une fois depuis les `.ogg` par une commande consignée dans `data/audio/CREDITS.md`. Le build ne dépend pas de ffmpeg.

### 2.3 Crédits

`data/audio/CREDITS.md` liste, pour chaque fichier : titre, auteur, URL de la source, licence, date, et la commande de conversion pour les effets. Le README y renvoie.

## 3. La chaîne de build

### 3.1 Le module d'effets

`smconv` ne lit que des modules `.it`, et snesmod prend les effets dans les échantillons d'un module. `tools/mksfx.py` écrit `data/audio/sfx.it` : un module IT valide, sans motif musical, dont les échantillons 1 à 7 sont les WAV du § 2.2 dans l'ordre du tableau. L'outil est déterministe : deux exécutions donnent le même fichier octet pour octet. Le fichier généré est versionné, comme `data/tiles.bmp`.

### 3.2 La banque de sons

- Le Makefile passe à `smconv` la liste `data/audio/sfx.it data/audio/alonely.it data/audio/offerthelight.it data/audio/purity.it`, dans cet ordre, selon le modèle de l'exemple PVSnesLib `snes-examples/audio/effectsandmusic`.
- La banque et son en-tête d'indices sont générés dans `data/audio/` et ignorés par git.
- Ces règles n'existent que dans la branche ROM du Makefile : `make test` ne les voit jamais.

### 3.3 Contrôles de budget au build

- **Mémoire son :** pour chaque musique, la taille des échantillons de la musique plus celle des effets, une fois convertis, doit rester sous 58 Ko. Le build échoue avec un message clair sinon. La mesure vient de la sortie de `smconv` (option `-V`) ou d'un calcul sur les fichiers ; le plan fixe laquelle.
- **ROM :** la banque 0 n'a plus que 20 octets libres. Le code et les données du son doivent aller dans les autres banques (sections `superfree`, comme les tuiles). Le build doit passer sans erreur de placement, et le garde-fou RAM (fin des statiques sous 7E:8000) aussi.

## 4. La console

### 4.1 Le module `src/snes/sound.c`, `sound.h`

Seule couche qui appelle snesmod :

```c
typedef enum { MUSIC_ALONELY, MUSIC_OFFER, MUSIC_PURITY } Music;
typedef enum { SFX_PLACE, SFX_REFUSE, SFX_UNDO, SFX_TICK,
               SFX_MENU, SFX_SELECT, SFX_WIN } Sfx;

void sound_init(void);          /* une fois au démarrage */
void sound_music(Music track);  /* charge et lance en boucle ; rien si déjà en cours */
void sound_sfx(Sfx effect);     /* joue un effet sans couper la musique */
void sound_update(void);        /* une fois par tour de boucle, après render_vblank() */
```

- `sound_init` démarre le pilote (`spcBoot`, `spcSetBank`) et pose tout l'état du module : la RAM n'est pas remise à zéro. Aucun morceau n'est « en cours » après l'init.
- `sound_music` recharge les effets après le chargement du morceau, comme le fait l'exemple PVSnesLib, pour qu'ils restent disponibles avec chaque musique.
- Le volume et la position stéréo des effets sont fixes et centrés.

### 4.2 Les déclenchements

| Moment | Son | Fichier |
|---|---|---|
| arrivée sur le menu | `MUSIC_ALONELY` | `screens.c`, `screen_menu` |
| lancement de `2 PLAYERS` | `MUSIC_ALONELY` | `main.c` |
| lancement de `VS CPU  EASY` | `MUSIC_OFFER` | `main.c` |
| lancement de `VS CPU  HARD` | `MUSIC_PURITY` | `main.c` |
| tutoriel | `MUSIC_ALONELY` | `screens.c`, `screen_tutorial` |
| écran de fin | la musique continue ; `SFX_WIN` | `screens.c`, `screen_result` |
| disque du menu déplacé | `SFX_MENU` | `screen_menu` |
| choix validé au menu | `SFX_SELECT` | `screen_menu` |
| START qui finit un tour | `SFX_SELECT` | `input.c` |
| A accepté / refusé | `SFX_PLACE` / `SFX_REFUSE` | `input.c`, selon le retour de `match_place` |
| B qui retire une cellule | `SFX_UNDO` | `input.c`, selon le retour de `match_undo` |
| génération après le tour du rouge | `SFX_TICK` | `main.c`, à l'entrée dans la pause de résolution |
| pose du CPU | `SFX_PLACE` par cellule posée | `main.c` |
| pose et génération du tutoriel | `SFX_PLACE`, `SFX_TICK` | `screens.c`, quand le lecteur a exécuté l'opération |

Pour le tutoriel, le lecteur du cœur signale les poses et générations qu'il vient d'exécuter (compteurs ou drapeaux dans `TutPlayer`), et l'écran les convertit en effets. Le cœur ne connaît pas le son.

`sound_update()` est appelé dans chaque boucle : `main.c` (partie), `screen_menu`, `screen_result`, `screen_tutorial`, juste après `render_vblank()`.

### 4.3 Ce qui ne change pas

Le cœur (`src/core/`) n'inclut rien de nouveau hors ce qu'impose le signalement du tutoriel ; règles, IA, affichage et cadence sont inchangés.

## 5. Tests et vérification

Tests hôtes :

- `tools/mksfx.py` : un test Python (ou une vérification lancée par `make test`) relit `data/audio/sfx.it` et contrôle l'en-tête IT, le nombre d'échantillons (7), leurs noms dans l'ordre et leur fréquence (8 000 Hz) ; deux exécutions donnent le même fichier ;
- le signalement des poses et générations du tutoriel : compté sur une leçon connue (leçon 5 : trois poses ; leçon 7 : douze générations).

Sur la console :

- `make test`, `make rom`, `make rom-script`, `make rom-measure`, `make rom-tutorial` passent, garde-fou RAM et contrôle de budget son compris ;
- les captures d'images existantes du menu, d'une partie et du tutoriel sont inchangées ;
- une capture audio : l'émulateur est lancé avec le son activé et sa sortie enregistrée en WAV (méthode établie dans le plan) sur la ROM scriptée, pour faire écouter la musique du menu, celle d'une partie contre le CPU, et au moins les effets de pose, de refus et de génération ;
- le README cite `data/audio/CREDITS.md`.

## 6. Hors du périmètre

- composer ou modifier la musique ;
- des réglages de volume dans le jeu, un menu d'options ;
- le son stéréo ou des effets positionnés selon la case.
