# Immigration — jeu de la vie à deux sur SNES

Design validé le 2026-09-14.

## 1. Intention

Un jeu de plateau au tour par tour pour deux joueurs, sur cartouche SNES, bâti
sur les règles du jeu de la vie de Conway. Chaque joueur possède une couleur.
On sème quelques cellules par tour, puis le monde évolue tout seul. On gagne
en effaçant la couleur adverse du plateau.

Modes : deux joueurs sur la même console, ou un joueur contre le CPU.

## 2. Le modèle

### 2.1 Plateau

- 32 colonnes × 24 lignes, soit 768 cellules.
- Topologie **torique** : les bords opposés se rejoignent, en horizontal comme
  en vertical. Un planeur qui sort à droite rentre à gauche.
- Stockage : tableau de 34 × 26 octets. L'anneau extérieur est un halo recopié
  depuis les bords opposés avant chaque tick. Le comptage des voisins n'a donc
  aucun test de bord ni aucun modulo.
- Une cellule tient dans un octet :

  | Valeur | Sens |
  |---|---|
  | 0 | vide |
  | 1 | vivante, joueur 1 (bleu) |
  | 2 | vivante, joueur 2 (rouge) |

### 2.2 Le tick

Sur l'axe vivant/mort, ce sont les règles de Conway sans modification. La
couleur ne s'ajoute qu'à la naissance.

| État de départ | Voisins vivants (toutes couleurs) | Résultat |
|---|---|---|
| vivante | 2 ou 3 | survit, garde sa couleur |
| vivante | 0, 1, 4, 5, 6, 7 ou 8 | meurt |
| vide | exactement 3 | naît, couleur majoritaire parmi les 3 parents |
| vide | tout sauf 3 | reste vide |

Trois parents se répartissent forcément en 3-0 ou 2-1. La couleur d'une
naissance n'est donc jamais ambiguë et la règle ne contient aucun aléatoire.
Le tick est une fonction pure : même plateau d'entrée, même plateau de sortie.

### 2.3 Placement

À son tour, un joueur pose jusqu'à **3 cellules**. Une pose est légale si et
seulement si les trois conditions suivantes tiennent :

1. la case visée est vide ;
2. elle est **à portée** : il existe une cellule vivante de la couleur du
   joueur à une distance de Chebyshev ≤ 2, c'est-à-dire dans le carré 5 × 5
   centré sur la case. La distance se mesure sur le tore, donc la portée
   franchit les bords ;
3. il reste du budget sur le tour.

La portée est **calculée au début du tour et figée**. Une cellule posée
pendant le tour n'étend pas la portée : on ne peut pas ramper en enchaînant
les poses.

Un joueur peut terminer son tour sans avoir tout posé. Les cellules posées
apparaissent immédiatement à l'écran, avant le tick.

Cette contrainte de portée est le cœur stratégique du jeu : les planeurs
deviennent des colons. Envoyer un planeur à l'autre bout du plateau ouvre une
tête de pont où poser des cellules plusieurs tours plus tard.

### 2.4 Position de départ

Symétrique par rotation de 180° autour du centre du plateau, avec
`(x, y) → (31 - x, 23 - y)`. Coordonnées en base 0, origine en haut à gauche.

Joueur 1 (bleu) :

- bloc 2 × 2, immortel s'il n'est pas dérangé : `(6,14) (7,14) (6,15) (7,15)`
- planeur dirigé vers le centre, en bas-droite : `(7,6) (8,7) (6,8) (7,8) (8,8)`

Joueur 2 (rouge), image des précédentes par la rotation :

- bloc : `(25,9) (24,9) (25,8) (24,8)`
- planeur dirigé vers le centre, en haut-gauche : `(24,17) (23,16) (25,15) (24,15) (23,15)`

## 3. Le déroulé

### 3.1 Le round

Un round enchaîne, dans cet ordre :

1. tour du joueur 1 — jusqu'à 3 poses, puis validation ;
2. tour du joueur 2 — idem ;
3. le ou les ticks du round ;
4. contrôle de victoire.

Le joueur 1 ouvre toujours. Contre le CPU, le joueur humain est le joueur 1.

### 3.2 L'emballement

| Rounds | Ticks par round |
|---|---|
| 1 à 15 | 1 |
| 16 à 40 | 2 |

En fin de partie le monde s'accélère : les colonies fragiles s'effondrent et
un coup bien placé peut faire tomber un camp entier. C'est ce qui rend
l'élimination atteignable, et c'est le principal réglage d'équilibrage.

Le contrôle de victoire a lieu **après chaque tick**, pas seulement à la fin
du round. Une partie peut donc se terminer sur le premier des deux ticks.

### 3.3 Fin de partie

| Situation | Résultat |
|---|---|
| une couleur tombe à zéro cellule après un tick | l'autre gagne aussitôt ; s'il restait un tick au round, il n'a pas lieu |
| les deux couleurs tombent à zéro sur le même tick | match nul |
| le round 40 s'achève | la plus grosse population gagne |
| le round 40 s'achève à populations égales | match nul |

### 3.4 Réglages

Ces valeurs sont regroupées dans un seul en-tête de configuration et fixées
par simulation (§ 7.2), non à l'intuition :

| Réglage | Valeur initiale |
|---|---|
| poses par tour | 3 |
| rayon de portée | 2 |
| round de l'emballement | 16 |
| ticks après emballement | 2 |
| plafond de rounds | 40 |

## 4. L'écran

Résolution 256 × 224. Mode 1 : BG1 porte la grille, BG2 le HUD, un sprite
porte le curseur.

```
┌────────────────────────────────────────┐
│                                        │
│   grille 32×24 tuiles = 256×192 px     │  BG1
│                                        │
├────────────────────────────────────────┤
│ ●BLEU 042 ●●○  R17/40×2  ●●● 038 ROUGE●│  BG2, 32 px
└────────────────────────────────────────┘
```

- À gauche et à droite : la pastille de couleur, la population sur 3 chiffres,
  et les poses restantes en pastilles pleines ou creuses.
- Au centre : le numéro de round sur le plafond, et le marqueur `×2` visible
  seulement pendant l'emballement.
- La pastille du joueur actif clignote à 2 Hz.

Jeu de tuiles de fond :

| Index | Tuile |
|---|---|
| 0 | vide |
| 1 | vide, marqueur de portée (un point discret au centre) |
| 2 | cellule du joueur 1 |
| 3 | cellule du joueur 2 |
| 4 à 13 | chiffres 0 à 9 |
| 14 et suivants | libellés du HUD, pastilles pleines et creuses |

Le marqueur de portée est une simple substitution de tuile pendant le rendu :
une case vide et à portée du joueur actif prend la tuile 1 au lieu de la 0.
Coût nul au rendu, une tuile de plus en VRAM.

### 4.1 Manette

| Touche | Effet |
|---|---|
| croix directionnelle | déplace le curseur d'une case, avec répétition automatique après 15 frames puis toutes les 4 |
| A | pose une cellule sur la case du curseur, si la pose est légale |
| B | annule la dernière pose du tour et rend le budget |
| START | termine le tour, même s'il reste des poses |
| SELECT | affiche ou masque le marquage de portée |

Le curseur se déplace lui aussi sur le tore. Une pose illégale ne consomme
rien et déclenche un bref bip.

## 5. Découpage logiciel

La règle qui gouverne tout le reste : **`src/core/` ignore qu'il tourne sur un
SNES.** Ni PPU, ni manette, ni PVSnesLib, ni allocation dynamique. Des
fonctions pures sur des structures. C'est ce qui rend la logique testable sur
la machine de développement.

```
src/core/          C portable, aucune dépendance
  config.h         tous les réglages du § 3.4, et eux seuls
  board.c/h        grille, halo torique, comptage de population
  life.c/h         le tick
  rules.c/h        portée, légalité d'une pose
  match.c/h        machine à états du round, emballement, victoire
  ai.c/h           l'adversaire CPU
src/snes/          couche PVSnesLib
  main.c           initialisation, boucle principale, VBlank
  render.c         plateau vers tilemap, HUD
  input.c          manettes vers intentions
  screens.c        titre, choix du mode, écran de fin
tests/             tests unitaires, exécutés par clang natif
tools/sim.c        simulateur de parties sans écran
data/              tuiles et palettes sources
```

`src/snes/` ne fait que traduire dans les deux sens : une pression sur A
devient un appel à `match_place`, un `Board` devient une tilemap. Aucune règle
de jeu n'y est écrite.

### 5.1 Interfaces principales

```c
/* board.h */
#define BOARD_W 32
#define BOARD_H 24

typedef enum { CELL_EMPTY = 0, CELL_P1 = 1, CELL_P2 = 2 } Cell;

/* Halo compris : indices 0 et BOARD_x+1 sont la copie des bords opposés. */
typedef struct { unsigned char c[BOARD_H + 2][BOARD_W + 2]; } Board;

void board_clear(Board *b);
void board_seed(Board *b);              /* position de départ du § 2.4 */
void board_wrap(Board *b);              /* recopie le halo */
Cell board_get(const Board *b, int x, int y);   /* coordonnées jeu, 0..W-1 */
void board_set(Board *b, int x, int y, Cell v);
int  board_count(const Board *b, Cell who);

/* life.h */
/* `in` doit être déjà wrappé. `out` l'est en sortie, prêt pour le tick suivant. */
void life_tick(const Board *in, Board *out);

/* rules.h */
bool rules_in_range(const Board *b, Cell player, int x, int y);
bool rules_can_place(const Board *b, Cell player, int x, int y);

/* match.h */
typedef enum { WINNER_NONE, WINNER_P1, WINNER_P2, WINNER_DRAW } Winner;
typedef struct { unsigned char x, y; } Move;

typedef struct {
    Board board;
    Board range_snapshot;   /* état au début du tour, pour la portée figée */
    int   round;            /* 1..CAP */
    Cell  turn;             /* CELL_P1 ou CELL_P2 */
    int   placed;           /* 0..BUDGET */
    Move  history[BUDGET];  /* pour l'annulation */
    Winner winner;
} Match;

void   match_start(Match *m);
bool   match_place(Match *m, int x, int y);   /* faux si illégal */
bool   match_undo(Match *m);                  /* faux si rien à annuler */
void   match_end_turn(Match *m);              /* enchaîne, ticke, arbitre */
Winner match_winner(const Match *m);
int    match_ticks_this_round(const Match *m);

/* ai.h */
typedef enum { AI_EASY, AI_NORMAL } AiLevel;
/* rng : état du xorshift, possédé par l'appelant. Ignoré en AI_NORMAL. */
int ai_choose(const Match *m, AiLevel lvl, unsigned int *rng, Move out[BUDGET]);
```

## 6. L'adversaire CPU

Recherche gloutonne sur les 3 poses, avec une évaluation locale.

### 6.1 L'évaluation locale

En jeu de la vie une perturbation ne se propage que d'une case par tick. Pour
juger l'effet d'une pose sur `t` ticks, il suffit donc de simuler une fenêtre
carrée de rayon `2t` autour d'elle, et de mesurer la différence de population
dans la sous-fenêtre de rayon `t`. Le résultat est **exact**, pas approché.

| Profondeur | Fenêtre simulée | Zone mesurée | Cellules |
|---|---|---|---|
| 1 tick | 5 × 5 | 3 × 3 | 25 au lieu de 768 |
| 2 ticks | 9 × 9 | 5 × 5 | 81 au lieu de 768 |

### 6.2 L'algorithme

Pour chacune des 3 poses, dans l'ordre :

1. **Élaguer** — ne retenir que les cases vides et à portée ayant au moins une
   cellule vivante dans leur voisinage 3 × 3. Une cellule posée dans le vide
   meurt au tick suivant sans rien produire. Cet élagage fait tomber quelques
   centaines de candidats à quelques dizaines.
2. **Présélectionner** — classer le reste par une heuristique bon marché,
   `2 × (voisins vivants) + max(0, 4 − distance de Chebyshev à la cellule
   adverse la plus proche)`, et garder les `K` meilleurs.
3. **Évaluer** — pour chaque candidat, simuler la fenêtre avec et sans la
   pose. Le score est
   `(mes cellules gagnées) − (cellules adverses gagnées)` sur la zone mesurée.
4. **Fixer** le meilleur candidat sur le plateau de travail, puis passer à la
   pose suivante. C'est ce qui permet à l'IA de trouver des combinaisons de
   deux ou trois cellules et pas seulement des coups isolés.

| Niveau | Profondeur | K | Choix |
|---|---|---|---|
| Facile | 1 tick | 8 | tirage uniforme parmi les 3 meilleurs |
| Normal | 2 ticks | 32 | le meilleur |

Déterminisme : à score égal, le candidat rencontré le premier dans l'ordre de
balayage (`y` croissant, puis `x`) l'emporte. Le niveau facile tire dans un
xorshift 32 bits à graine explicite. Deux exécutions sur les mêmes entrées
donnent donc le même résultat, ce dont le simulateur a besoin.

Si aucun candidat ne subsiste après l'élagage, l'IA passe son tour.

### 6.3 Coût

Environ 96 évaluations par tour au niveau normal, soit à peu près 1,5 à 2
secondes sur la console. Le travail est découpé par une machine à états qui
en traite un lot par frame, pour que le HUD reste animé pendant la réflexion
plutôt que de geler l'image.

## 7. Vérification

### 7.1 Tests unitaires

`make test` compile `src/core/` et `tests/` avec le clang de la machine de
développement et s'exécute en moins d'une seconde. Le harnais est une
quarantaine de lignes de macros d'assertion écrites sur place : trois macros
ne justifient pas une dépendance à installer.

Développement en test d'abord, un test qui échoue avant chaque
implémentation.

Couverture attendue, par module :

**`life`** — le bloc est stable ; le clignotant oscille en période 2 ; le
planeur se translate de (1,1) en 4 ticks ; un planeur traverse le bord droit
et réapparaît à gauche ; une naissance à 3 parents bleus est bleue ; une
naissance 2 bleus / 1 rouge est bleue ; une naissance 1 bleu / 2 rouges est
rouge ; une cellule qui survit garde sa couleur quelles que soient celles de
ses voisins ; la surpopulation tue.

**`rules`** — une case à distance 2 est à portée, à distance 3 non ; la portée
franchit les bords du tore ; une case occupée est refusée ; un joueur sans
aucune cellule n'a aucune pose légale.

**`match`** — une pose décrémente le budget ; la quatrième est refusée ;
l'annulation rend le budget et vide la case ; annuler sans avoir posé ne fait
rien ; finir le tour du joueur 1 passe la main sans ticker ; finir celui du
joueur 2 ticke et incrémente le round ; les rounds 1 à 15 valent un tick, les
suivants deux ; l'élimination est détectée dès le premier tick d'un round ;
l'extinction simultanée donne un nul ; le round 40 tranche à la population ;
à égalité, nul.

**`ai`** — ne rend que des coups légaux ; en rend au plus 3, et moins s'il y a
moins de coups légaux ; préfère un coup qui détruit des cellules adverses à un
coup neutre, sur un plateau construit pour ; **l'évaluation par fenêtre locale
donne le même score que l'évaluation sur le plateau entier**, vérifié sur des
configurations isolées — c'est le test qui garantit l'optimisation du § 6.1 ;
le niveau facile à graine fixée est reproductible.

### 7.2 Le simulateur d'équilibrage

`make sim` produit un binaire natif qui joue des parties IA contre IA sans
écran, à partir du même `src/core/`. Cent parties prennent quelques secondes.

Il répond par des mesures aux questions laissées ouvertes par le design :

- quelle proportion de parties se termine par élimination plutôt qu'au plafond
  de rounds ;
- à quel round déclencher l'emballement pour que cette proportion soit
  satisfaisante ;
- quelle est l'ampleur de l'avantage du joueur qui ouvre ;
- si le budget de 3 poses et le rayon de 2 sont les bonnes valeurs.

Les réglages du § 3.4 seront arrêtés sur ces sorties.

## 8. Budget console

| Poste | Coût estimé | Marge |
|---|---|---|
| un tick complet | ~150 000 cycles, soit 2 à 3 frames | invisible au tour par tour |
| tilemap vers VRAM | 1 536 octets, un DMA en VBlank | le VBlank en encaisse environ 5 Ko |
| plateau en WRAM | 2 × 884 octets, double tampon | sur 128 Ko |
| ROM | vraisemblablement moins de 64 Ko | LoROM, 256 Ko |

L'estimation du tick suppose du C compilé par 816-tcc, soit de l'ordre de 200
cycles par cellule. De l'assembleur écrit à la main descendrait vers 40, mais
rien ici ne le justifie.

Aucun poste n'est tendu. Ce jeu ne se bat pas contre la machine.

## 9. Chaîne de compilation

PVSnesLib, avec un `Makefile` à quatre cibles :

| Cible | Effet |
|---|---|
| `make test` | compile `core` + `tests` avec clang natif, exécute |
| `make sim` | compile `core` + `tools/sim.c` avec clang natif |
| `make rom` | produit la ROM via PVSnesLib |
| `make` | `test` puis `rom` |

### 9.1 Risque et parade

La machine de développement est un Mac Apple Silicon sous macOS 26.6, et rien
n'est installé : ni PVSnesLib, ni émulateur, ni assembleur 65816. L'existence
d'une distribution PVSnesLib native arm64 n'est pas vérifiée.

Le premier pas du plan est donc un **spike d'outillage** isolé : installer la
chaîne, compiler un programme minimal, l'exécuter dans un émulateur. Replis
successifs, dans l'ordre : exécution sous Rosetta, image Docker Linux, puis
changement de SDK.

Ce risque est cloisonné par construction : `src/core/`, ses tests et le
simulateur ne dépendent d'aucun outil SNES. Toute la logique de jeu peut être
écrite et validée même si l'outillage résiste plusieurs jours.

## 10. Ordre de construction

1. **Spike d'outillage** — PVSnesLib et émulateur, un programme minimal à l'écran.
2. **`core/` en test d'abord** — `board`, puis `life`, `rules`, `match`.
3. **`tools/sim`** — et arrêt des réglages du § 3.4 sur les mesures.
4. **Rendu** — grille et HUD affichés, plateau figé.
5. **Entrées et boucle** — le mode deux joueurs devient jouable de bout en bout.
6. **`ai.c` en test d'abord**, puis branchement du mode contre CPU.
7. **Écrans** — titre, choix du mode, écran de fin.

Une fois le spike passé, les étapes 2-3 et 4-5 sont indépendantes.

## 11. Hors périmètre

Écartés délibérément de cette version : la musique, les animations de mort de
cellule, l'éditeur de motifs, la sauvegarde, le jeu à plus de deux, les
niveaux d'IA au-delà de deux, et le choix de la couleur par le joueur humain
contre le CPU.
