# Immigration — tutoriel qui se joue tout seul

Sous-projet 2 de trois, après la police, le titre et le menu (sous-projet 1). Le son est le sous-projet 3.

## 1. But

Un joueur qui découvre le jeu choisit `HOW TO PLAY` au menu et regarde une démo commentée d'environ une minute. Il en sort en sachant comment gagner, comment une génération fonctionne, comment poser et pourquoi les planeurs comptent.

Critères de réussite :

- neuf mini-leçons, chacune sur un plateau préparé, jouées par le vrai moteur du jeu ;
- un texte en anglais de trois lignes au plus sous le HUD pendant chaque leçon ;
- la démo avance seule ; **A** passe à la leçon suivante ; **START** revient au menu ;
- chaque leçon montre ce qu'elle annonce, et un test hôte le vérifie.

## 2. Les leçons

Chaque leçon commence par une mise en place, affiche son texte, puis déroule ses actions. Les durées sont en images (60 par seconde) et visent 5 à 8 secondes par leçon. Coordonnées en cases, origine en haut à gauche.

| # | Mise en place | Déroulé | Texte | Ce que vérifie le test |
|---|---|---|---|---|
| 1 | position de départ normale | attente | `TWO COLONIES SHARE A SMALL` / `WORLD. WIPE OUT THE OTHER` / `ONE TO WIN.` | le plateau est la position de départ |
| 2 | plateau vide ; bleue seule en (8,10) ; bloc bleu (20,10)-(21,11) | attente, une génération, attente | `A CELL WITH 2 OR 3 NEIGHBOURS` / `LIVES ON. ALONE, IT DIES.` | (8,10) vide ; les 4 cases du bloc bleues |
| 3 | plateau vide ; croix bleue : (15,11) et ses 4 voisines orthogonales | attente, une génération, attente | `WITH 4 OR MORE NEIGHBOURS,` / `A CELL IS CROWDED OUT.` | (15,11) vide |
| 4 | plateau vide ; bleues (14,10) et (15,10), rouge (16,10) | attente, une génération, attente | `3 NEIGHBOURS GIVE BIRTH.` / `THE NEW CELL TAKES THE` / `MAJORITY COLOUR.` | (15,11) bleue |
| 5 | position de départ, portée affichée | le curseur va poser (8,14), (8,15) puis (9,14), une attente entre chaque ; fin du tour du bleu | `ON YOUR TURN, PLACE UP TO 3` / `CELLS ON THE DOTS, THEN` / `PRESS START.` | les trois cases bleues ; la main au rouge |
| 6 | position de départ, portée affichée | pose en (8,14) ; attente | `YOUR REACH IS SET AT THE` / `START OF YOUR TURN.` | (10,14) hors portée malgré la cellule en (8,14) |
| 7 | plateau vide ; planeur bleu (29,8) (30,9) (28,10) (29,10) (30,10) | douze générations espacées, puis portée affichée | `A GLIDER CRAWLS ACROSS THE` / `WORLD. THE EDGES WRAP` / `AROUND.` | 5 cellules bleues, dont au moins une en colonne 0 à 2 |
| 8 | position de départ ; round 16 | fin du tour du bleu, fin du tour du rouge (deux générations), attente | `FROM ROUND 16 THE WORLD` / `RUNS TWICE AS FAST.` | round 17 ; deux générations par round |
| 9 | plateau vide ; bloc bleu (10,10)-(11,11) ; rouge seule (20,10) | fin du tour du bleu, fin du tour du rouge, attente | `NO CELLS LEFT: YOU WIN.` / `AFTER ROUND 40, THE BIGGEST` / `COLONY WINS.` | vainqueur : bleu |

Après la leçon 9, la démo revient au menu.

## 3. Le moteur du tutoriel (`src/core/tutorial.c`, `tutorial.h`)

### 3.1 Le script

Le tutoriel est une seule table plate de triplets d'octets `{op, a, b}`, en `static const` à une dimension et de portée fichier, donc en ROM.

| Opération | Arguments | Effet |
|---|---|---|
| `TUT_LESSON` | n | début de la leçon n : point d'arrivée de A |
| `TUT_START` | — | `match_start` : position de départ, round 1, main au bleu |
| `TUT_CLEAR` | — | `match_start`, puis plateau vidé |
| `TUT_CELL` | x, y + couleur | pose directe d'une cellule de mise en place (sans budget ni portée) |
| `TUT_READY` | — | fin de mise en place : `match_begin_turn(m, CELL_P1)` recalcule la portée |
| `TUT_ROUND` | r | fixe le numéro de round |
| `TUT_TEXT` | i | affiche le texte i |
| `TUT_RANGE` | 0 ou 1 | masque ou affiche les points de portée |
| `TUT_CURSOR` | x, y | montre le curseur en (x, y) |
| `TUT_NOCURSOR` | — | cache le curseur |
| `TUT_PLACE` | x, y | vraie pose par `match_place` : éteint une pastille |
| `TUT_ENDTURN` | — | vraie fin de tour par `match_end_turn` |
| `TUT_TICK` | — | une génération par `match_tick` |
| `TUT_WAIT` | n | attendre n images (n de 1 à 255) |
| `TUT_END` | — | fin du tutoriel |

`TUT_CELL` code la couleur dans l'opération elle-même : `TUT_CELL_P1` et `TUT_CELL_P2`.

### 3.2 Le lecteur

```c
typedef struct {
    int    pc;          /* indice de l'opération suivante */
    int    wait;        /* images restantes du WAIT en cours */
    int    lesson;      /* leçon en cours, 1 à 9 */
    int    caption;     /* texte affiché, -1 pour aucun */
    int    cursor_x, cursor_y;
    bool_t cursor_on, show_range;
    bool_t done;
} TutPlayer;

void   tut_start(TutPlayer *p, Match *m);
/* Avance le script de `frames` images écoulées. Exécute les opérations
   jusqu'au prochain WAIT non écoulé. Rend TRUE si le plateau, la portée,
   le curseur ou le texte ont changé. */
bool_t tut_update(TutPlayer *p, Match *m, int frames);
/* Saute au début de la leçon suivante et joue sa mise en place ;
   après la dernière, met `done`. */
void   tut_skip(TutPlayer *p, Match *m);
```

- `tut_start` pose chaque champ explicitement : la RAM de la console n'est pas mise à zéro.
- Une attente plus longue que les images écoulées garde son reste pour l'appel suivant. Des images en trop débordent sur les opérations suivantes, sans en sauter aucune.
- Le lecteur n'a aucune règle de jeu : toutes les générations, poses et fins de tour passent par `match.c`.

### 3.3 Les textes

```c
/* Écrit le texte i sur trois lignes de 32 tuiles, par view_text.
   i hors bornes, ou -1 : trois lignes vides. */
void view_caption(int i, u8 out[3][HUD_W]);
```

Les chaînes sont rendues par une fonction à `switch` (texte, ligne) : pas de table de pointeurs en ROM.

### 3.4 La virgule

Plusieurs textes contiennent une virgule, que la police du sous-projet 1 n'a pas. Elle s'ajoute à la planche en tuile 84 (`TILE_COMMA`, la première place libre), dessinée dans la police 5 × 7 : `["....." × 5, "..#..", ".#..."]`. `view_text` la reconnaît ; les tuiles 0 à 83 ne bougent pas.

### 3.5 Ajouts au moteur de jeu (`src/core/match.c`)

- `void match_tick(Match *m)` : une génération (halo, `life_tick`, contrôle d'extinction), sans changer le tour ni le round. `match_end_turn` l'appelle dans sa boucle ; son comportement ne change pas.
- `void match_begin_turn(Match *m, Cell who)` : la fonction interne `begin_turn`, rendue publique (main à `who`, budget remis à zéro, portée et masque recalculés).

## 4. La console

### 4.1 Le menu

- Une quatrième entrée, `HOW TO PLAY`, en ligne 16 colonne 10 ; la sélection va de 0 à 3.
- `view_menu` : décompte des cases non vides augmenté de 9 (`HOWTOPLAY`).
- `screen_menu` rend une valeur dédiée pour le tutoriel ; `main.c` lance alors `screen_tutorial()` puis revient au menu.

### 4.2 L'écran du tutoriel (`screen_tutorial`, `src/snes/screens.c`)

Boucle identique à celle du jeu :

1. lire la manette par détection de front (A passe, START quitte) ;
2. `tut_update(p, m, images)` avec les images écoulées d'après `snes_vblank_count` ;
3. si quelque chose a changé : reconstruire le plateau (`render_board_now`, portée selon le lecteur) et le HUD, préparer les textes et le curseur ;
4. `WaitForVBlank()` puis `render_vblank()`.

- Le HUD de la ligne 24 est le vrai HUD du `Match` : pastilles, round, `×2`.
- Quand le `Match` a un vainqueur (leçon 9), le HUD montre `view_result_banner` à la place.
- En sortant, les lignes de texte sont effacées et le curseur caché.

### 4.3 Les textes à l'écran

- Lignes 25 à 27 de la tilemap de BG2, sous le HUD.
- Nouvelle fonction de préparation dans `src/snes/render.c`, dans le modèle en deux temps : elle copie les trois lignes et les transfère au VBlank suivant (192 octets), seulement quand le texte change.
- `render_init` efface déjà toute la tilemap de BG2, donc ces lignes sont vides en jeu.

## 5. Mémoire et performance

- Le script, les textes et les tables sont en ROM. En RAM : un `TutPlayer` (quelques octets) et le `Match` existant de `main.c`, réutilisé.
- Le garde-fou RAM doit passer ; la marge actuelle est d'environ 4 Ko.
- Chaque reconstruction coûte environ 14 images : les attentes sont décomptées en images réelles, donc le rythme reste juste.

## 6. Tests et vérification

Tests hôtes, écrits d'abord :

- `match_tick` : une génération sur un clignotant ; extinction détectée ; tour et round inchangés ; `match_end_turn` garde tous ses tests actuels ;
- le lecteur : pour chaque leçon, jouer jusqu'à son dernier `WAIT` et vérifier la colonne « Ce que vérifie le test » du § 2 ;
- `tut_update` : des images en gros morceaux (1, 7, 1000) donnent le même état final qu'image par image ;
- `tut_skip` : mène au début de chaque leçon dans l'ordre, puis à `done` ;
- le script entier se termine en moins de 5 000 images ;
- `view_caption` : chaque ligne de chaque texte tient en 32 caractères et correspond au § 2 ; texte hors bornes vide ;
- `view_text` : la virgule donne `TILE_COMMA` ; régénérer la planche laisse les tuiles 0 à 83 identiques ;
- `tut_start` depuis une mémoire remplie de 0x55 donne le même déroulé.

Sur la console :

- `make test`, `make rom`, `make rom-script`, `make rom-measure` passent, garde-fou RAM compris ;
- nouvelle variante `make rom-tutorial` (`-DBOOT_TUTORIAL`, ROM `build/life-tutorial.sfc`) qui démarre directement sur le tutoriel, pour la vérification ; jamais dans `make rom` ;
- une capture par leçon, lue en texte et à l'œil : plateau, texte, HUD ; début de chaque leçon repéré avec `find_frame.py` ;
- README : une capture du tutoriel et la ligne `HOW TO PLAY` du menu.

## 7. Hors du périmètre

- le son (sous-projet 3) ;
- toute modification des règles, de l'IA ou de l'équilibrage ;
- l'interaction dans le tutoriel au-delà de A et START.
