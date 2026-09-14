# Immigration — plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produire une ROM SNES d'un jeu de la vie à deux couleurs, jouable à deux ou contre le CPU.

**Architecture:** Toute la logique vit dans `src/core/`, en C89 portable et sans dépendance, testée par un binaire natif sur la machine de développement. `src/snes/` est une couche mince qui traduit manettes vers appels du cœur et structures du cœur vers VRAM. Un simulateur natif rejoue des parties IA contre IA pour fixer les réglages d'équilibrage par la mesure.

**Tech Stack:** C89, PVSnesLib (816-tcc + wla-65816), clang pour l'hôte, GNU make.

**Spec:** `docs/superpowers/specs/2026-09-14-snes-life-game-design.md`

## Global Constraints

Ces règles s'appliquent à **toutes** les tâches. Les exigences de chaque tâche les incluent implicitement.

- **`src/core/` ne référence jamais PVSnesLib, le PPU, les manettes ou la VRAM.** Aucun `#include` autre que `string.h`. Aucun `printf` (sauf dans `tests/` et `tools/`).
- **C89 strict.** Pas de `//`, pas de déclaration après instruction, pas de `stdbool.h`, pas d'initialiseur désigné, pas de littéral composé, pas de `long long`, pas de flottant. Le build hôte impose `-std=c89 -pedantic -Werror`, ce qui fait échouer chez le développeur tout écart que 816-tcc refuserait ensuite.
- **`int` fait 16 bits sur 65816.** Aucune valeur manipulée par `src/core/` ne doit sortir de −32 768 à 32 767. Là où 32 bits sont nécessaires, écrire `long` explicitement. Le build hôte, où `int` fait 32 bits, ne détectera pas ces débordements : c'est une discipline, pas une vérification.
- **Aucune allocation dynamique.** Pas de `malloc`. Tout est statique ou automatique.
- **La pile du SNES est petite.** Aucune variable automatique de plus de 64 octets. Un `Board` (884 octets) ou une fenêtre d'IA se déclare `static` en portée fichier, jamais sur la pile.
- **Réglages du jeu**, tous dans `src/core/config.h` et nulle part ailleurs : `BOARD_W` 32, `BOARD_H` 24, `BUDGET` 3, `RANGE_RADIUS` 2, `RAMPUP_ROUND` 16, `TICKS_AFTER_RAMPUP` 2, `ROUND_CAP` 40.
- **Format de commit** : `<emoji> [scope] <description>`, préfixe gitmoji, scope parmi `[core]`, `[snes]`, `[tools]`, `[build]`, `[docs]`. Ne jamais nommer un outil d'assistance dans le message.
- **Test d'abord.** Chaque tâche du cœur écrit un test qui échoue avant toute implémentation.

## Raffinements par rapport à la spec

Trois écarts assumés, décidés en écrivant ce plan :

1. **`src/core/view.c` s'ajoute au découpage du § 5.** La conversion plateau vers tilemap et le formatage du HUD y deviennent des fonctions pures, donc testables sur l'hôte. `src/snes/render.c` se réduit à un transfert DMA. Sans cet ajout, le rendu serait la seule grande surface non testée.
2. **Le HUD abandonne les libellés « BLEU » et « ROUGE » du § 4.** Les pastilles de couleur portent déjà l'identité des joueurs. Le jeu de glyphes tombe de 26 lettres à 13 signes : les dix chiffres, `R`, `/` et `×`.
3. **`ai_choose` prend un `unsigned long *` et non un `unsigned int *`.** Un xorshift32 exige 32 bits, et `int` n'en fait que 16 sur la cible.

## Structure des fichiers

| Fichier | Responsabilité |
|---|---|
| `src/core/config.h` | les sept réglages, et rien d'autre |
| `src/core/types.h` | `bool_t`, `u8`, `Cell`, `Move`, `Winner` |
| `src/core/board.c/h` | grille, halo torique, lecture/écriture, comptage, position de départ |
| `src/core/life.c/h` | le tick Immigration, une seule fonction |
| `src/core/rules.c/h` | portée torique et légalité d'une pose |
| `src/core/match.c/h` | machine à états du round, emballement, arbitrage |
| `src/core/view.c/h` | plateau vers indices de tuiles, formatage du HUD |
| `src/core/ai.c/h` | candidats, évaluation par fenêtre locale, choix glouton |
| `src/snes/main.c` | initialisation console, boucle principale, VBlank |
| `src/snes/render.c/h` | transferts DMA de la tilemap et du HUD, sprite du curseur |
| `src/snes/input.c/h` | manettes vers intentions, curseur, répétition automatique |
| `src/snes/screens.c/h` | titre, choix du mode, écran de fin |
| `tests/harness.h`, `tests/harness.c` | macros d'assertion et compteur, une quarantaine de lignes |
| `tests/main.c` | appelle chaque suite, rend le code de sortie |
| `tests/test_*.c` | une suite par module du cœur |
| `tools/mktiles.py` | génère `data/tiles.bmp` de façon déterministe |
| `tools/sim.c` | parties IA contre IA sans écran, statistiques |
| `Makefile` | cibles `test`, `sim`, `rom`, `clean` |

---

### Task 0 : Spike d'outillage PVSnesLib

Cette tâche est un **spike** : son livrable est une réponse et un squelette vérifié, pas du code de production. Elle ne suit pas le cycle test d'abord, parce qu'il n'y a rien à tester tant que la chaîne ne tourne pas.

Elle est indépendante des tâches 1 à 6 : si elle résiste, poursuivre avec le cœur et y revenir.

**Files:**
- Create: `docs/snes-notes.md`
- Create: `src/snes/main.c`
- Create: `Makefile`
- Modify: `.gitignore` — y ajouter `data/*.pic`, `data/*.pal` et `*.obj`, produits par la chaîne PVSnesLib

**Interfaces:**
- Consumes: rien.
- Produces: `docs/snes-notes.md`, qui consigne les **appels PVSnesLib exacts et vérifiés** dont dépendent les tâches 7 à 12 : initialisation console, choix du mode graphique, chargement d'un jeu de tuiles et d'une palette, écriture d'une tilemap en VRAM par DMA, pose d'un sprite, lecture des manettes, attente du VBlank. Plus la cible `rom` du `Makefile`, et le squelette `src/snes/main.c` que les tâches suivantes étendent au lieu de le réécrire.

- [ ] **Step 1: Installer la chaîne**

Tenter dans cet ordre, s'arrêter au premier succès et consigner lequel a marché :

```bash
# a. binaire natif arm64 depuis les releases GitHub
open https://github.com/alekmaul/pvsneslib/releases
# télécharger la release macOS la plus récente, décompresser dans ~/pvsneslib
export PVSNESLIB_HOME=~/pvsneslib

# b. si le binaire est x86_64 uniquement, l'exécuter sous Rosetta
softwareupdate --install-rosetta --agree-to-license
arch -x86_64 "$PVSNESLIB_HOME/devkitsnes/bin/816-tcc" -v

# c. repli conteneur
docker run --rm -v "$PWD":/work -w /work ubuntu:24.04 bash -lc 'echo ok'
```

Vérifier : `"$PVSNESLIB_HOME/devkitsnes/bin/816-tcc" -v` et `"$PVSNESLIB_HOME/devkitsnes/bin/wla-65816" -v` répondent tous les deux.

- [ ] **Step 2: Installer un émulateur, et déterminer s'il sait faire une capture sans interface**

```bash
brew install --cask mesen   # sinon : ares, bsnes, snes9x
```

Chercher explicitement une option de capture d'écran pilotable en ligne de commande ou par script Lua. **Consigner la réponse dans `docs/snes-notes.md`** : si elle existe, les tâches 7 à 12 pourront se vérifier automatiquement ; sinon leur vérification restera une inspection visuelle par un humain.

- [ ] **Step 3: Écrire le programme minimal**

`src/snes/main.c` — reprendre l'exemple `hello_world` de la distribution PVSnesLib et l'adapter : mode 1, un jeu de tuiles chargé en VRAM, une tilemap de 32 × 24 remplie depuis un tableau d'octets par DMA, boucle infinie sur `WaitForVBlank`.

Le point important n'est pas le contenu affiché, c'est que **la tilemap vienne d'un tableau C mis à jour à l'exécution** — c'est le mécanisme exact dont la tâche 7 aura besoin.

- [ ] **Step 4: Écrire la cible `rom` du Makefile**

```make
PVSNESLIB_HOME ?= $(HOME)/pvsneslib

.PHONY: rom
rom:
	$(MAKE) -f $(PVSNESLIB_HOME)/devkitsnes/rules.mk
```

L'invocation exacte dépend de la distribution installée : partir du `Makefile` de l'exemple qui a fonctionné à l'étape 3 et le réduire.

- [ ] **Step 5: Compiler et exécuter**

```bash
make rom
ls -la build/*.sfc
```

Attendu : un fichier `.sfc` produit, qui s'ouvre dans l'émulateur et affiche la tilemap.

- [ ] **Step 6: Consigner**

Écrire `docs/snes-notes.md` avec, pour chaque point de l'interface « Produces » ci-dessus, **la ligne de code exacte qui a fonctionné**, pas une description. Ajouter la variante retenue à l'étape 1 (native, Rosetta ou conteneur) et la réponse sur la capture d'écran.

- [ ] **Step 7: Commit**

```bash
git add docs/snes-notes.md src/snes/main.c Makefile .gitignore
git commit -m "🧱 [build] Set up PVSnesLib toolchain and minimal ROM"
```

**Si le spike échoue après les trois replis** : arrêter, remonter le problème, et poursuivre avec les tâches 1 à 6 qui n'en dépendent pas. Le choix d'un autre SDK est une décision à prendre avec le commanditaire, pas dans ce plan.

---

### Task 1 : Harnais de test, socle du cœur, et le plateau

**Files:**
- Create: `src/core/config.h`, `src/core/types.h`, `src/core/board.h`, `src/core/board.c`
- Create: `tests/harness.h`, `tests/harness.c`, `tests/main.c`, `tests/test_board.c`
- Modify: `Makefile` (ajouter les cibles `test` et `clean`)

**Interfaces:**
- Consumes: rien.
- Produces:
  - `src/core/types.h` : `typedef unsigned char u8; typedef unsigned char bool_t;` avec `TRUE`/`FALSE` ; `typedef enum { CELL_EMPTY = 0, CELL_P1 = 1, CELL_P2 = 2 } Cell;` ; `typedef struct { u8 x, y; } Move;` ; `typedef enum { WINNER_NONE = 0, WINNER_P1, WINNER_P2, WINNER_DRAW } Winner;`
  - `src/core/board.h` : `typedef struct { u8 c[BOARD_H + 2][BOARD_W + 2]; } Board;` et `#define BSTRIDE (BOARD_W + 2)` ; `void board_clear(Board *b); void board_seed(Board *b); void board_wrap(Board *b); Cell board_get(const Board *b, int x, int y); void board_set(Board *b, int x, int y, Cell v); int board_count(const Board *b, Cell who);`
  - `tests/harness.h` : `T_RUN`, `T_CHECK`, `T_EQ`, `T_TRUE`, `T_FALSE`, `t_report`.
  - `make test` compile et exécute la suite.

Convention d'indexation, valable pour tout le projet : les coordonnées **jeu** vont de 0 à `BOARD_W-1` et 0 à `BOARD_H-1`. Le stockage décale de 1 pour loger le halo, donc jeu `(x, y)` est stocké en `c[y + 1][x + 1]`.

- [ ] **Step 1: Écrire le harnais**

`tests/harness.h` :

```c
#ifndef HARNESS_H
#define HARNESS_H

#include <stdio.h>

extern int t_checks;
extern int t_fails;
extern const char *t_current;

#define T_RUN(fn) do { t_current = #fn; fn(); } while (0)

#define T_CHECK(cond, msg) do {                                      \
    t_checks++;                                                      \
    if (!(cond)) {                                                   \
        t_fails++;                                                   \
        printf("FAIL %s:%d [%s] %s\n", __FILE__, __LINE__,           \
               t_current, (msg));                                    \
    }                                                                \
} while (0)

#define T_EQ(a, b) do {                                              \
    long ta = (long)(a);                                             \
    long tb = (long)(b);                                             \
    t_checks++;                                                      \
    if (ta != tb) {                                                  \
        t_fails++;                                                   \
        printf("FAIL %s:%d [%s] %s == %s : %ld != %ld\n",            \
               __FILE__, __LINE__, t_current, #a, #b, ta, tb);       \
    }                                                                \
} while (0)

#define T_TRUE(a)  T_CHECK((a), #a " est faux")
#define T_FALSE(a) T_CHECK(!(a), #a " est vrai")

int t_report(void);

#endif
```

`tests/harness.c` :

```c
#include "harness.h"

int t_checks = 0;
int t_fails = 0;
const char *t_current = "";

int t_report(void)
{
    printf("%d vérifications, %d échecs\n", t_checks, t_fails);
    return t_fails ? 1 : 0;
}
```

`tests/main.c` — chaque tâche suivante y ajoute une ligne :

```c
#include "harness.h"

void suite_board(void);

int main(void)
{
    suite_board();
    return t_report();
}
```

- [ ] **Step 2: Écrire les tests du plateau, qui doivent échouer**

`tests/test_board.c` :

```c
#include "harness.h"
#include "board.h"

static void test_clear_donne_un_plateau_vide(void)
{
    static Board b;
    board_set(&b, 5, 5, CELL_P1);
    board_clear(&b);
    T_EQ(board_get(&b, 5, 5), CELL_EMPTY);
    T_EQ(board_count(&b, CELL_P1), 0);
    T_EQ(board_count(&b, CELL_P2), 0);
}

static void test_set_et_get_font_un_aller_retour(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    board_set(&b, BOARD_W - 1, BOARD_H - 1, CELL_P2);
    T_EQ(board_get(&b, 0, 0), CELL_P1);
    T_EQ(board_get(&b, BOARD_W - 1, BOARD_H - 1), CELL_P2);
    T_EQ(board_get(&b, 1, 0), CELL_EMPTY);
}

static void test_count_compte_par_couleur(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, 1, 1, CELL_P1);
    board_set(&b, 2, 1, CELL_P1);
    board_set(&b, 3, 1, CELL_P2);
    T_EQ(board_count(&b, CELL_P1), 2);
    T_EQ(board_count(&b, CELL_P2), 1);
}

/* Le halo doit recevoir la copie du bord opposé, coins compris. */
static void test_wrap_recopie_les_bords_opposes(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    board_wrap(&b);
    T_EQ(b.c[BOARD_H + 1][1], CELL_P1);              /* halo bas */
    T_EQ(b.c[1][BOARD_W + 1], CELL_P1);              /* halo droit */
    T_EQ(b.c[BOARD_H + 1][BOARD_W + 1], CELL_P1);    /* coin bas-droit */
}

static void test_wrap_recopie_dans_lautre_sens(void)
{
    static Board b;
    board_clear(&b);
    board_set(&b, BOARD_W - 1, BOARD_H - 1, CELL_P2);
    board_wrap(&b);
    T_EQ(b.c[0][BOARD_W], CELL_P2);   /* halo haut */
    T_EQ(b.c[BOARD_H][0], CELL_P2);   /* halo gauche */
    T_EQ(b.c[0][0], CELL_P2);         /* coin haut-gauche */
}

/* § 2.4 de la spec : symétrie exacte par rotation de 180°. */
static void test_seed_est_symetrique_par_rotation(void)
{
    static Board b;
    int x, y;
    board_seed(&b);
    T_EQ(board_count(&b, CELL_P1), 9);
    T_EQ(board_count(&b, CELL_P2), 9);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            Cell here = board_get(&b, x, y);
            Cell there = board_get(&b, BOARD_W - 1 - x, BOARD_H - 1 - y);
            if (here == CELL_P1) {
                T_EQ(there, CELL_P2);
            } else if (here == CELL_P2) {
                T_EQ(there, CELL_P1);
            } else {
                T_EQ(there, CELL_EMPTY);
            }
        }
    }
}

static void test_seed_place_le_bloc_du_joueur_1(void)
{
    static Board b;
    board_seed(&b);
    T_EQ(board_get(&b, 6, 14), CELL_P1);
    T_EQ(board_get(&b, 7, 14), CELL_P1);
    T_EQ(board_get(&b, 6, 15), CELL_P1);
    T_EQ(board_get(&b, 7, 15), CELL_P1);
}

void suite_board(void)
{
    T_RUN(test_clear_donne_un_plateau_vide);
    T_RUN(test_set_et_get_font_un_aller_retour);
    T_RUN(test_count_compte_par_couleur);
    T_RUN(test_wrap_recopie_les_bords_opposes);
    T_RUN(test_wrap_recopie_dans_lautre_sens);
    T_RUN(test_seed_est_symetrique_par_rotation);
    T_RUN(test_seed_place_le_bloc_du_joueur_1);
}
```

- [ ] **Step 3: Écrire le Makefile, puis constater que la compilation échoue**

Ajouter au `Makefile` :

```make
CORE_SRC  := $(wildcard src/core/*.c)
TEST_SRC  := $(wildcard tests/*.c)
HOSTFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -Isrc/core -Itests -g \
             -fsanitize=address,undefined

.PHONY: test clean
test: build/run-tests
	./build/run-tests

build/run-tests: $(CORE_SRC) $(TEST_SRC) | build
	$(CC) $(HOSTFLAGS) $^ -o $@

build:
	mkdir -p build

clean:
	rm -rf build

# `make` seul lance les tests, pour que le cycle rouge-vert reste immédiat.
# `make all` y ajoute la ROM, une fois la tâche 0 passée.
.DEFAULT_GOAL := test
.PHONY: all
all: test rom
```

Run: `make test`
Expected: échec de compilation, `fatal error: 'board.h' file not found`.

- [ ] **Step 4: Écrire les en-têtes**

`src/core/config.h` :

```c
#ifndef CONFIG_H
#define CONFIG_H

/* Les seuls réglages du jeu. Ne rien mettre d'autre ici, ne les définir
   nulle part ailleurs. Valeurs de départ ; la tâche 6 les arrête sur mesure. */

#define BOARD_W            32
#define BOARD_H            24
#define BUDGET              3   /* poses par tour */
#define RANGE_RADIUS        2   /* distance de Chebyshev */
#define RAMPUP_ROUND       16   /* premier round à deux ticks */
#define TICKS_AFTER_RAMPUP  2
#define ROUND_CAP          40

#endif
```

`src/core/types.h` :

```c
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char u8;

/* 816-tcc ne fournit pas <stdbool.h>. */
typedef unsigned char bool_t;
#define TRUE  1
#define FALSE 0

typedef enum { CELL_EMPTY = 0, CELL_P1 = 1, CELL_P2 = 2 } Cell;

typedef struct { u8 x, y; } Move;

typedef enum { WINNER_NONE = 0, WINNER_P1, WINNER_P2, WINNER_DRAW } Winner;

#endif
```

`src/core/board.h` :

```c
#ifndef BOARD_H
#define BOARD_H

#include "config.h"
#include "types.h"

/* Le stockage porte un halo d'une case : jeu (x, y) vit en c[y + 1][x + 1].
   Le halo est la copie du bord opposé, ce qui rend le tore gratuit pour le
   comptage des voisins : aucun test de bord, aucun modulo. */
#define BSTRIDE (BOARD_W + 2)

typedef struct { u8 c[BOARD_H + 2][BOARD_W + 2]; } Board;

void board_clear(Board *b);
void board_seed(Board *b);   /* position de départ, spec § 2.4 */
void board_wrap(Board *b);   /* recopie le halo depuis les bords opposés */
Cell board_get(const Board *b, int x, int y);
void board_set(Board *b, int x, int y, Cell v);
int  board_count(const Board *b, Cell who);

#endif
```

- [ ] **Step 5: Lancer les tests et voir chaque assertion échouer**

Run: `make test`
Expected: échec de l'édition de liens, symboles `board_clear` et suivants indéfinis.

- [ ] **Step 6: Implémenter le plateau**

`src/core/board.c` :

```c
#include <string.h>
#include "board.h"

void board_clear(Board *b)
{
    memset(b->c, CELL_EMPTY, sizeof b->c);
}

Cell board_get(const Board *b, int x, int y)
{
    return (Cell)b->c[y + 1][x + 1];
}

void board_set(Board *b, int x, int y, Cell v)
{
    b->c[y + 1][x + 1] = (u8)v;
}

int board_count(const Board *b, Cell who)
{
    int x, y, n = 0;
    for (y = 1; y <= BOARD_H; y++) {
        for (x = 1; x <= BOARD_W; x++) {
            if (b->c[y][x] == (u8)who) {
                n++;
            }
        }
    }
    return n;
}

void board_wrap(Board *b)
{
    int x, y;
    /* Colonnes d'abord, sur les seules lignes de jeu... */
    for (y = 1; y <= BOARD_H; y++) {
        b->c[y][0] = b->c[y][BOARD_W];
        b->c[y][BOARD_W + 1] = b->c[y][1];
    }
    /* ...puis les lignes sur toute la largeur, ce qui remplit les coins. */
    for (x = 0; x <= BOARD_W + 1; x++) {
        b->c[0][x] = b->c[BOARD_H][x];
        b->c[BOARD_H + 1][x] = b->c[1][x];
    }
}

void board_seed(Board *b)
{
    /* Spec § 2.4. Le joueur 2 est l'image du joueur 1 par (x,y) -> (31-x, 23-y).
       Bloc 2x2 : immortel tant qu'on ne le dérange pas.
       Planeur : se déplace vers le centre du plateau. */
    static const u8 p1[9][2] = {
        { 6, 14 }, { 7, 14 }, { 6, 15 }, { 7, 15 },          /* bloc */
        { 7,  6 }, { 8,  7 }, { 6,  8 }, { 7,  8 }, { 8, 8 } /* planeur */
    };
    int i;

    board_clear(b);
    for (i = 0; i < 9; i++) {
        board_set(b, p1[i][0], p1[i][1], CELL_P1);
        board_set(b, BOARD_W - 1 - p1[i][0], BOARD_H - 1 - p1[i][1], CELL_P2);
    }
    board_wrap(b);
}
```

- [ ] **Step 7: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: `N vérifications, 0 échecs`, code de sortie 0.

- [ ] **Step 8: Commit**

```bash
git add src/core tests Makefile
git commit -m "✅ [core] Add test harness and toroidal board"
```

---

### Task 2 : Le tick Immigration

**Files:**
- Create: `src/core/life.h`, `src/core/life.c`, `tests/test_life.c`
- Modify: `tests/main.c` (ajouter `suite_life`)

**Interfaces:**
- Consumes: `Board`, `board_clear`, `board_set`, `board_get`, `board_count`, `board_wrap`, `BSTRIDE` de la tâche 1.
- Produces: `void life_tick(const Board *in, Board *out);` — `in` doit être déjà wrappé ; `out` l'est en sortie, prêt à servir d'entrée au tick suivant. `in` et `out` doivent être distincts.

- [ ] **Step 1: Écrire les tests, qui doivent échouer**

`tests/test_life.c` :

```c
#include "harness.h"
#include "board.h"
#include "life.h"

static Board a, b;

static void tick_once(void)
{
    board_wrap(&a);
    life_tick(&a, &b);
    a = b;
}

static void test_le_bloc_est_stable(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P1);
    board_set(&a, 11, 11, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 4);
    T_EQ(board_get(&a, 10, 10), CELL_P1);
    T_EQ(board_get(&a, 11, 11), CELL_P1);
}

static void test_le_clignotant_oscille_en_periode_2(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 12, 10, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 3);
    T_EQ(board_get(&a, 11,  9), CELL_P1);   /* devenu vertical */
    T_EQ(board_get(&a, 11, 11), CELL_P1);
    T_EQ(board_get(&a, 10, 10), CELL_EMPTY);
    tick_once();
    T_EQ(board_get(&a, 10, 10), CELL_P1);   /* de nouveau horizontal */
    T_EQ(board_get(&a, 12, 10), CELL_P1);
}

static void test_le_planeur_se_translate_de_1_1_en_4_ticks(void)
{
    int i;
    board_clear(&a);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 12, 11, CELL_P1);
    board_set(&a, 10, 12, CELL_P1);
    board_set(&a, 11, 12, CELL_P1);
    board_set(&a, 12, 12, CELL_P1);
    for (i = 0; i < 4; i++) {
        tick_once();
    }
    T_EQ(board_count(&a, CELL_P1), 5);
    T_EQ(board_get(&a, 12, 11), CELL_P1);
    T_EQ(board_get(&a, 13, 12), CELL_P1);
    T_EQ(board_get(&a, 11, 13), CELL_P1);
    T_EQ(board_get(&a, 12, 13), CELL_P1);
    T_EQ(board_get(&a, 13, 13), CELL_P1);
}

/* Un clignotant à cheval sur la couture verticale doit basculer normalement. */
static void test_le_tore_referme_le_bord_droit(void)
{
    board_clear(&a);
    board_set(&a, BOARD_W - 1, 10, CELL_P1);
    board_set(&a, 0, 10, CELL_P1);
    board_set(&a, 1, 10, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 3);
    T_EQ(board_get(&a, 0,  9), CELL_P1);
    T_EQ(board_get(&a, 0, 10), CELL_P1);
    T_EQ(board_get(&a, 0, 11), CELL_P1);
}

static void test_naissance_a_trois_parents_bleus(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P1);
    tick_once();
    T_EQ(board_get(&a, 11, 11), CELL_P1);
}

static void test_naissance_a_deux_bleus_un_rouge_est_bleue(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P2);
    tick_once();
    T_EQ(board_get(&a, 11, 11), CELL_P1);
}

static void test_naissance_a_un_bleu_deux_rouges_est_rouge(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P2);
    board_set(&a, 10, 11, CELL_P2);
    tick_once();
    T_EQ(board_get(&a, 11, 11), CELL_P2);
}

/* Une cellule qui survit garde sa couleur, quelle que soit celle des voisins. */
static void test_le_survivant_garde_sa_couleur(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    board_set(&a, 11, 10, CELL_P1);
    board_set(&a, 10, 11, CELL_P1);
    board_set(&a, 11, 11, CELL_P2);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 3);
    T_EQ(board_get(&a, 11, 11), CELL_P2);
}

static void test_la_solitude_tue(void)
{
    board_clear(&a);
    board_set(&a, 10, 10, CELL_P1);
    tick_once();
    T_EQ(board_count(&a, CELL_P1), 0);
}

static void test_la_surpopulation_tue_le_centre(void)
{
    int x, y;
    board_clear(&a);
    for (y = 9; y <= 11; y++) {
        for (x = 9; x <= 11; x++) {
            board_set(&a, x, y, CELL_P1);
        }
    }
    tick_once();
    T_EQ(board_get(&a, 10, 10), CELL_EMPTY);   /* 8 voisins */
    T_EQ(board_get(&a,  9,  9), CELL_P1);      /* 3 voisins, survit */
}

void suite_life(void)
{
    T_RUN(test_le_bloc_est_stable);
    T_RUN(test_le_clignotant_oscille_en_periode_2);
    T_RUN(test_le_planeur_se_translate_de_1_1_en_4_ticks);
    T_RUN(test_le_tore_referme_le_bord_droit);
    T_RUN(test_naissance_a_trois_parents_bleus);
    T_RUN(test_naissance_a_deux_bleus_un_rouge_est_bleue);
    T_RUN(test_naissance_a_un_bleu_deux_rouges_est_rouge);
    T_RUN(test_le_survivant_garde_sa_couleur);
    T_RUN(test_la_solitude_tue);
    T_RUN(test_la_surpopulation_tue_le_centre);
}
```

Ajouter dans `tests/main.c` la déclaration `void suite_life(void);` et l'appel `suite_life();` après `suite_board();`.

- [ ] **Step 2: Lancer les tests et vérifier qu'ils échouent**

Run: `make test`
Expected: échec de compilation, `fatal error: 'life.h' file not found`.

- [ ] **Step 3: Implémenter le tick**

`src/core/life.h` :

```c
#ifndef LIFE_H
#define LIFE_H

#include "board.h"

/* `in` doit avoir été wrappé. `out` l'est en sortie, prêt pour le tick
   suivant. `in` et `out` doivent désigner des plateaux distincts. */
void life_tick(const Board *in, Board *out);

#endif
```

`src/core/life.c` :

```c
#include "life.h"

void life_tick(const Board *in, Board *out)
{
    int x, y;

    for (y = 1; y <= BOARD_H; y++) {
        for (x = 1; x <= BOARD_W; x++) {
            const u8 *p = &in->c[y - 1][x - 1];
            u8 self, v;
            int n = 0;    /* voisins vivants, toutes couleurs */
            int n1 = 0;   /* ceux qui sont bleus */

            v = p[0];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[1];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[BSTRIDE];        if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[BSTRIDE + 2];    if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * BSTRIDE];    if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * BSTRIDE + 1];if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * BSTRIDE + 2];if (v) { n++; if (v == CELL_P1) n1++; }

            self = in->c[y][x];
            if (self) {
                out->c[y][x] = (n == 2 || n == 3) ? self : (u8)CELL_EMPTY;
            } else {
                /* Trois parents se répartissent en 3-0 ou 2-1 : la majorité
                   est toujours tranchée. */
                out->c[y][x] = (n == 3)
                    ? (u8)(n1 >= 2 ? CELL_P1 : CELL_P2)
                    : (u8)CELL_EMPTY;
            }
        }
    }
    board_wrap(out);
}
```

Le parcours par pointeur `p` fonctionne parce que `Board::c` est un tableau bidimensionnel contigu : `p[BSTRIDE]` désigne la case juste en dessous de `p[0]`.

- [ ] **Step 4: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: 0 échecs.

- [ ] **Step 5: Commit**

```bash
git add src/core/life.h src/core/life.c tests/test_life.c tests/main.c
git commit -m "✨ [core] Add Immigration Game tick"
```

---

### Task 3 : Portée et légalité d'une pose

**Files:**
- Create: `src/core/rules.h`, `src/core/rules.c`, `tests/test_rules.c`
- Modify: `tests/main.c` (ajouter `suite_rules`)

**Interfaces:**
- Consumes: `Board`, `board_get`, `Cell`, `bool_t`, `RANGE_RADIUS`.
- Produces:
  - `bool_t rules_in_range(const Board *b, Cell player, int x, int y);` — vrai s'il existe une cellule de `player` à distance de Chebyshev torique ≤ `RANGE_RADIUS` de `(x, y)`. La case centrale compte si elle appartient au joueur.
  - `bool_t rules_can_place(const Board *b, Cell player, int x, int y);` — `rules_in_range` **et** case vide. Ne connaît rien du budget, qui est l'affaire de `match`.

- [ ] **Step 1: Écrire les tests, qui doivent échouer**

`tests/test_rules.c` :

```c
#include "harness.h"
#include "board.h"
#include "rules.h"

static Board b;

static void test_la_portee_va_jusqua_deux_cases(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P1);
    T_TRUE(rules_in_range(&b, CELL_P1, 12, 10));
    T_TRUE(rules_in_range(&b, CELL_P1, 12, 12));   /* Chebyshev = 2 */
    T_TRUE(rules_in_range(&b, CELL_P1,  8,  8));
    T_FALSE(rules_in_range(&b, CELL_P1, 13, 10));
    T_FALSE(rules_in_range(&b, CELL_P1, 13, 13));
}

static void test_la_portee_ne_vaut_que_pour_sa_couleur(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P1);
    T_FALSE(rules_in_range(&b, CELL_P2, 11, 10));
}

static void test_la_portee_franchit_les_bords(void)
{
    board_clear(&b);
    board_set(&b, 0, 0, CELL_P1);
    /* Sur le tore, (31, 23) est le voisin diagonal de (0, 0). */
    T_TRUE(rules_in_range(&b, CELL_P1, BOARD_W - 1, BOARD_H - 1));
    T_TRUE(rules_in_range(&b, CELL_P1, BOARD_W - 2, 1));
    T_FALSE(rules_in_range(&b, CELL_P1, BOARD_W - 3, 0));
}

static void test_on_ne_pose_pas_sur_une_case_occupee(void)
{
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P1);
    board_set(&b, 11, 10, CELL_P2);
    T_FALSE(rules_can_place(&b, CELL_P1, 10, 10));
    T_FALSE(rules_can_place(&b, CELL_P1, 11, 10));
    T_TRUE(rules_can_place(&b, CELL_P1, 12, 10));
}

static void test_sans_aucune_cellule_rien_nest_posable(void)
{
    int x, y, posables = 0;
    board_clear(&b);
    board_set(&b, 10, 10, CELL_P2);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            if (rules_can_place(&b, CELL_P1, x, y)) {
                posables++;
            }
        }
    }
    T_EQ(posables, 0);
}

void suite_rules(void)
{
    T_RUN(test_la_portee_va_jusqua_deux_cases);
    T_RUN(test_la_portee_ne_vaut_que_pour_sa_couleur);
    T_RUN(test_la_portee_franchit_les_bords);
    T_RUN(test_on_ne_pose_pas_sur_une_case_occupee);
    T_RUN(test_sans_aucune_cellule_rien_nest_posable);
}
```

Ajouter `void suite_rules(void);` et l'appel dans `tests/main.c`.

- [ ] **Step 2: Lancer les tests et vérifier qu'ils échouent**

Run: `make test`
Expected: `fatal error: 'rules.h' file not found`.

- [ ] **Step 3: Implémenter les règles**

`src/core/rules.h` :

```c
#ifndef RULES_H
#define RULES_H

#include "board.h"

/* Une cellule de `player` existe-t-elle à distance de Chebyshev torique
   inférieure ou égale à RANGE_RADIUS de (x, y) ? */
bool_t rules_in_range(const Board *b, Cell player, int x, int y);

/* Case vide et à portée. Le budget du tour n'entre pas ici : c'est `match`
   qui le tient. */
bool_t rules_can_place(const Board *b, Cell player, int x, int y);

#endif
```

`src/core/rules.c` :

```c
#include "rules.h"

/* Le halo ne fait qu'une case alors que la portée en fait deux : le
   bouclage se refait donc explicitement ici.
   Pas de modulo : le 65816 n'a pas de division câblée, et ces fonctions
   sont appelées des dizaines de milliers de fois par tour de CPU. Une
   soustraction conditionnelle suffit tant que le décalage reste inférieur
   à la dimension, ce qui est le cas de tous les appelants. */
static int wrap_x(int x)
{
    if (x < 0)              return x + BOARD_W;
    if (x >= BOARD_W)       return x - BOARD_W;
    return x;
}

static int wrap_y(int y)
{
    if (y < 0)              return y + BOARD_H;
    if (y >= BOARD_H)       return y - BOARD_H;
    return y;
}

bool_t rules_in_range(const Board *b, Cell player, int x, int y)
{
    int dx, dy;
    for (dy = -RANGE_RADIUS; dy <= RANGE_RADIUS; dy++) {
        for (dx = -RANGE_RADIUS; dx <= RANGE_RADIUS; dx++) {
            if (board_get(b, wrap_x(x + dx), wrap_y(y + dy)) == player) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

bool_t rules_can_place(const Board *b, Cell player, int x, int y)
{
    if (board_get(b, x, y) != CELL_EMPTY) {
        return FALSE;
    }
    return rules_in_range(b, player, x, y);
}
```

- [ ] **Step 4: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: 0 échecs.

- [ ] **Step 5: Commit**

```bash
git add src/core/rules.h src/core/rules.c tests/test_rules.c tests/main.c
git commit -m "✨ [core] Add placement range and legality rules"
```

---

### Task 4 : La machine à états du round

**Files:**
- Create: `src/core/match.h`, `src/core/match.c`, `tests/test_match.c`
- Modify: `tests/main.c` (ajouter `suite_match`)

**Interfaces:**
- Consumes: `Board`, `board_*`, `life_tick`, `rules_in_range`, `BUDGET`, `RAMPUP_ROUND`, `TICKS_AFTER_RAMPUP`, `ROUND_CAP`.
- Produces :

```c
typedef struct {
    Board  board;   /* état courant, halo à jour */
    Board  range;   /* copie prise au début du tour : la portée est figée */
    int    round;   /* 1 à ROUND_CAP */
    Cell   turn;    /* CELL_P1 ou CELL_P2 */
    int    placed;  /* 0 à BUDGET */
    Move   history[BUDGET];
    Winner winner;
} Match;

void   match_start(Match *m);
bool_t match_place(Match *m, int x, int y);
bool_t match_undo(Match *m);
void   match_end_turn(Match *m);
Winner match_winner(const Match *m);
int    match_ticks_this_round(const Match *m);
```

Contrat de `match_end_turn`, qui est le cœur de la tâche :
- au tour du joueur 1, passe la main au joueur 2, remet le budget à zéro, reprend l'instantané de portée, **et ne ticke pas** ;
- au tour du joueur 2, exécute `match_ticks_this_round` ticks, en arbitrant **après chaque tick** ; une élimination détectée sur le premier tick annule le second ; sinon, si le round atteint `ROUND_CAP`, arbitre à la population ; sinon incrémente le round et rend la main au joueur 1 ;
- ne fait rien si la partie est déjà finie.

- [ ] **Step 1: Écrire les tests, qui doivent échouer**

`tests/test_match.c` :

```c
#include "harness.h"
#include "match.h"

static Match m;

/* Prépare une partie sur un plateau vide, à un round choisi.
   Les tests d'arbitrage ont besoin de positions bien plus simples que
   celle de départ. */
static void arm(int round, Cell turn)
{
    match_start(&m);
    board_clear(&m.board);
    m.round = round;
    m.turn = turn;
    m.placed = 0;
}

static void commit_setup(void)
{
    board_wrap(&m.board);
    m.range = m.board;
}

static void block(int x, int y, Cell who)
{
    board_set(&m.board, x,     y,     who);
    board_set(&m.board, x + 1, y,     who);
    board_set(&m.board, x,     y + 1, who);
    board_set(&m.board, x + 1, y + 1, who);
}

/* ---- budget et annulation, sur la position de départ ---- */

static void test_trois_poses_puis_le_budget_est_epuise(void)
{
    match_start(&m);
    T_EQ(m.round, 1);
    T_EQ(m.turn, CELL_P1);
    T_TRUE(match_place(&m, 5, 13));
    T_TRUE(match_place(&m, 8, 13));
    T_TRUE(match_place(&m, 5, 16));
    T_EQ(m.placed, BUDGET);
    T_FALSE(match_place(&m, 8, 16));   /* quatrième : refusée */
    T_EQ(board_get(&m.board, 8, 16), CELL_EMPTY);
}

static void test_une_pose_hors_portee_est_refusee(void)
{
    match_start(&m);
    T_FALSE(match_place(&m, 15, 15));
    T_EQ(m.placed, 0);
}

static void test_la_portee_est_figee_pour_le_tour(void)
{
    match_start(&m);
    T_TRUE(match_place(&m, 5, 13));
    /* (3,13) est à 2 cases de (5,13) mais à 3 de toute cellule bleue
       présente au début du tour. La portée étant figée, c'est refusé. */
    T_FALSE(match_place(&m, 3, 13));
}

static void test_annuler_rend_la_case_et_le_budget(void)
{
    match_start(&m);
    T_TRUE(match_place(&m, 5, 13));
    T_TRUE(match_undo(&m));
    T_EQ(m.placed, 0);
    T_EQ(board_get(&m.board, 5, 13), CELL_EMPTY);
    T_FALSE(match_undo(&m));           /* plus rien à annuler */
}

/* ---- enchaînement des tours ---- */

static void test_finir_le_tour_du_joueur_1_ne_ticke_pas(void)
{
    arm(1, CELL_P1);
    block(2, 2, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);   /* isolée : mourrait au tick */
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.turn, CELL_P2);
    T_EQ(m.round, 1);
    T_EQ(m.placed, 0);
    T_EQ(board_get(&m.board, 20, 20), CELL_P2);   /* toujours là */
}

static void test_finir_le_tour_du_joueur_2_ticke_et_avance_le_round(void)
{
    arm(1, CELL_P2);
    block(2, 2, CELL_P1);
    block(20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.round, 2);
    T_EQ(m.turn, CELL_P1);
    T_EQ(m.winner, WINNER_NONE);
}

static void test_le_nombre_de_ticks_double_a_lemballement(void)
{
    arm(RAMPUP_ROUND - 1, CELL_P1);
    T_EQ(match_ticks_this_round(&m), 1);
    arm(RAMPUP_ROUND, CELL_P1);
    T_EQ(match_ticks_this_round(&m), TICKS_AFTER_RAMPUP);
    arm(ROUND_CAP, CELL_P1);
    T_EQ(match_ticks_this_round(&m), TICKS_AFTER_RAMPUP);
}

/* ---- arbitrage ---- */

static void test_lelimination_arrete_le_round_des_le_premier_tick(void)
{
    arm(RAMPUP_ROUND, CELL_P2);        /* round à deux ticks */
    block(2, 2, CELL_P1);              /* immortel */
    board_set(&m.board, 10, 10, CELL_P1);   /* clignotant horizontal */
    board_set(&m.board, 11, 10, CELL_P1);
    board_set(&m.board, 12, 10, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);   /* isolée : meurt au tick 1 */
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_P1);
    /* Le clignotant est vertical : exactement un tick a eu lieu. */
    T_EQ(board_get(&m.board, 11,  9), CELL_P1);
    T_EQ(board_get(&m.board, 11, 11), CELL_P1);
    T_EQ(board_get(&m.board, 10, 10), CELL_EMPTY);
}

static void test_lextinction_simultanee_donne_un_nul(void)
{
    arm(1, CELL_P2);
    board_set(&m.board, 5, 5, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_DRAW);
}

static void test_le_plafond_tranche_a_la_population(void)
{
    arm(ROUND_CAP, CELL_P2);
    block(2, 2, CELL_P1);
    block(6, 2, CELL_P1);              /* 8 cellules bleues */
    block(20, 20, CELL_P2);            /* 4 cellules rouges */
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_P1);
    T_EQ(m.round, ROUND_CAP);          /* on ne dépasse pas le plafond */
}

static void test_le_plafond_a_egalite_donne_un_nul(void)
{
    arm(ROUND_CAP, CELL_P2);
    block(2, 2, CELL_P1);
    block(20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_DRAW);
}

static void test_une_partie_finie_est_gelee(void)
{
    arm(1, CELL_P2);
    board_set(&m.board, 5, 5, CELL_P1);
    board_set(&m.board, 20, 20, CELL_P2);
    commit_setup();
    match_end_turn(&m);
    T_EQ(m.winner, WINNER_DRAW);
    match_end_turn(&m);                /* sans effet */
    T_EQ(m.winner, WINNER_DRAW);
    T_FALSE(match_place(&m, 5, 5));
}

void suite_match(void)
{
    T_RUN(test_trois_poses_puis_le_budget_est_epuise);
    T_RUN(test_une_pose_hors_portee_est_refusee);
    T_RUN(test_la_portee_est_figee_pour_le_tour);
    T_RUN(test_annuler_rend_la_case_et_le_budget);
    T_RUN(test_finir_le_tour_du_joueur_1_ne_ticke_pas);
    T_RUN(test_finir_le_tour_du_joueur_2_ticke_et_avance_le_round);
    T_RUN(test_le_nombre_de_ticks_double_a_lemballement);
    T_RUN(test_lelimination_arrete_le_round_des_le_premier_tick);
    T_RUN(test_lextinction_simultanee_donne_un_nul);
    T_RUN(test_le_plafond_tranche_a_la_population);
    T_RUN(test_le_plafond_a_egalite_donne_un_nul);
    T_RUN(test_une_partie_finie_est_gelee);
}
```

Ajouter `void suite_match(void);` et l'appel dans `tests/main.c`.

- [ ] **Step 2: Lancer les tests et vérifier qu'ils échouent**

Run: `make test`
Expected: `fatal error: 'match.h' file not found`.

- [ ] **Step 3: Implémenter la machine à états**

`src/core/match.h` :

```c
#ifndef MATCH_H
#define MATCH_H

#include "board.h"
#include "life.h"
#include "rules.h"

typedef struct {
    Board  board;
    Board  range;    /* instantané pris au début du tour */
    int    round;
    Cell   turn;
    int    placed;
    Move   history[BUDGET];
    Winner winner;
} Match;

void   match_start(Match *m);
bool_t match_place(Match *m, int x, int y);
bool_t match_undo(Match *m);
void   match_end_turn(Match *m);
Winner match_winner(const Match *m);
int    match_ticks_this_round(const Match *m);

#endif
```

`src/core/match.c` :

```c
#include "match.h"

/* 884 octets : bien trop pour la pile du 65816, d'où la portée fichier. */
static Board scratch;

static Winner judge_extinction(const Board *b)
{
    int p1 = board_count(b, CELL_P1);
    int p2 = board_count(b, CELL_P2);
    if (p1 == 0 && p2 == 0) return WINNER_DRAW;
    if (p1 == 0)            return WINNER_P2;
    if (p2 == 0)            return WINNER_P1;
    return WINNER_NONE;
}

static Winner judge_population(const Board *b)
{
    int p1 = board_count(b, CELL_P1);
    int p2 = board_count(b, CELL_P2);
    if (p1 > p2) return WINNER_P1;
    if (p2 > p1) return WINNER_P2;
    return WINNER_DRAW;
}

static void begin_turn(Match *m, Cell who)
{
    m->turn = who;
    m->placed = 0;
    m->range = m->board;
}

void match_start(Match *m)
{
    board_seed(&m->board);
    m->round = 1;
    m->winner = WINNER_NONE;
    begin_turn(m, CELL_P1);
}

int match_ticks_this_round(const Match *m)
{
    return (m->round < RAMPUP_ROUND) ? 1 : TICKS_AFTER_RAMPUP;
}

Winner match_winner(const Match *m)
{
    return m->winner;
}

bool_t match_place(Match *m, int x, int y)
{
    if (m->winner != WINNER_NONE)  return FALSE;
    if (m->placed >= BUDGET)       return FALSE;
    if (board_get(&m->board, x, y) != CELL_EMPTY) return FALSE;
    /* La portée se lit sur l'instantané, pas sur le plateau courant : une
       cellule posée à l'instant ne doit pas étendre la zone de pose. */
    if (!rules_in_range(&m->range, m->turn, x, y)) return FALSE;

    board_set(&m->board, x, y, m->turn);
    m->history[m->placed].x = (u8)x;
    m->history[m->placed].y = (u8)y;
    m->placed++;
    return TRUE;
}

bool_t match_undo(Match *m)
{
    if (m->winner != WINNER_NONE) return FALSE;
    if (m->placed == 0)           return FALSE;
    m->placed--;
    board_set(&m->board,
              (int)m->history[m->placed].x,
              (int)m->history[m->placed].y,
              CELL_EMPTY);
    return TRUE;
}

void match_end_turn(Match *m)
{
    int i, ticks;

    if (m->winner != WINNER_NONE) return;

    if (m->turn == CELL_P1) {
        begin_turn(m, CELL_P2);
        return;
    }

    ticks = match_ticks_this_round(m);
    for (i = 0; i < ticks; i++) {
        board_wrap(&m->board);
        life_tick(&m->board, &scratch);
        m->board = scratch;
        m->winner = judge_extinction(&m->board);
        if (m->winner != WINNER_NONE) {
            return;   /* un second tick n'a pas lieu */
        }
    }

    if (m->round >= ROUND_CAP) {
        m->winner = judge_population(&m->board);
        return;
    }

    m->round++;
    begin_turn(m, CELL_P1);
}
```

- [ ] **Step 4: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: 0 échecs.

- [ ] **Step 5: Commit**

```bash
git add src/core/match.h src/core/match.c tests/test_match.c tests/main.c
git commit -m "✨ [core] Add round state machine and win arbitration"
```

---

### Task 5 : Le modèle d'affichage

Cette tâche ne dessine rien : elle produit les tableaux d'indices de tuiles que la tâche 8 se contentera de transférer en VRAM. Elle existe pour que le rendu soit testable sur l'hôte.

**Files:**
- Create: `src/core/view.h`, `src/core/view.c`, `tests/test_view.c`
- Modify: `tests/main.c` (ajouter `suite_view`)

**Interfaces:**
- Consumes: `Match`, `board_get`, `board_count`, `rules_in_range`, `match_ticks_this_round`.
- Produces :

```c
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

#define HUD_W 32          /* le bandeau fait une seule ligne de tuiles */

void view_board(const Match *m, bool_t show_range, u8 out[BOARD_H][BOARD_W]);
void view_digits3(int value, u8 out[3]);
void view_hud(const Match *m, u8 out[HUD_W]);
```

Disposition du bandeau, colonne par colonne, telle que `view_hud` doit la produire :

| Colonnes | Contenu |
|---|---|
| 0 | `TILE_P1` — la pastille bleue |
| 1 | `TILE_EMPTY` |
| 2 à 4 | population bleue sur 3 chiffres |
| 5 | `TILE_EMPTY` |
| 6 à 8 | poses restantes du bleu, `TILE_PIP_ON` ou `TILE_PIP_OFF` |
| 9 à 11 | `TILE_EMPTY` |
| 12 | `TILE_R` |
| 13 à 15 | numéro de round sur 3 chiffres |
| 16 | `TILE_SLASH` |
| 17 à 19 | `ROUND_CAP` sur 3 chiffres |
| 20 | `TILE_TIMES` si l'emballement est actif, sinon `TILE_EMPTY` |
| 21 | chiffre du nombre de ticks si l'emballement est actif, sinon `TILE_EMPTY` |
| 22 | `TILE_EMPTY` |
| 23 à 25 | poses restantes du rouge |
| 26 | `TILE_EMPTY` |
| 27 à 29 | population rouge sur 3 chiffres |
| 30 | `TILE_EMPTY` |
| 31 | `TILE_P2` — la pastille rouge |

Les poses restantes n'ont de sens que pour le joueur dont c'est le tour ; celles de l'autre s'affichent toutes pleines. Le clignotement de la pastille active n'est pas du ressort de `view` : la tâche 8 le fait en basculant la couleur de palette.

- [ ] **Step 1: Écrire les tests, qui doivent échouer**

`tests/test_view.c` :

```c
#include "harness.h"
#include "match.h"
#include "view.h"

static Match m;
static u8 grid[BOARD_H][BOARD_W];
static u8 hud[HUD_W];
static u8 d[3];

static void test_les_cellules_prennent_la_tuile_de_leur_couleur(void)
{
    match_start(&m);
    view_board(&m, FALSE, grid);
    T_EQ(grid[14][6], TILE_P1);     /* bloc bleu, spec § 2.4 */
    T_EQ(grid[9][25], TILE_P2);     /* bloc rouge */
    T_EQ(grid[0][0], TILE_EMPTY);
}

static void test_la_portee_ne_marque_que_les_cases_vides_atteignables(void)
{
    match_start(&m);
    view_board(&m, TRUE, grid);
    T_EQ(grid[13][5], TILE_RANGE);  /* vide, à 1 case du bloc bleu */
    T_EQ(grid[14][6], TILE_P1);     /* occupée : la couleur l'emporte */
    T_EQ(grid[15][15], TILE_EMPTY); /* vide mais hors de portée */
}

static void test_sans_marquage_aucune_tuile_de_portee(void)
{
    int x, y, marks = 0;
    match_start(&m);
    view_board(&m, FALSE, grid);
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            if (grid[y][x] == TILE_RANGE) marks++;
        }
    }
    T_EQ(marks, 0);
}

static void test_les_chiffres_sont_cales_a_droite_sur_trois_rangs(void)
{
    view_digits3(0, d);
    T_EQ(d[0], TILE_DIGIT0); T_EQ(d[1], TILE_DIGIT0); T_EQ(d[2], TILE_DIGIT0);
    view_digits3(42, d);
    T_EQ(d[0], TILE_DIGIT0 + 0);
    T_EQ(d[1], TILE_DIGIT0 + 4);
    T_EQ(d[2], TILE_DIGIT0 + 2);
    view_digits3(768, d);
    T_EQ(d[0], TILE_DIGIT0 + 7);
    T_EQ(d[1], TILE_DIGIT0 + 6);
    T_EQ(d[2], TILE_DIGIT0 + 8);
}

static void test_les_chiffres_saturent_a_999(void)
{
    view_digits3(1000, d);
    T_EQ(d[0], TILE_DIGIT0 + 9);
    T_EQ(d[2], TILE_DIGIT0 + 9);
    view_digits3(-5, d);
    T_EQ(d[2], TILE_DIGIT0);
}

static void test_le_bandeau_montre_populations_round_et_pastilles(void)
{
    match_start(&m);
    view_hud(&m, hud);
    T_EQ(hud[0], TILE_P1);
    T_EQ(hud[31], TILE_P2);
    T_EQ(hud[4], TILE_DIGIT0 + 9);        /* 009 cellules bleues au départ */
    T_EQ(hud[29], TILE_DIGIT0 + 9);       /* 009 rouges */
    T_EQ(hud[12], TILE_R);
    T_EQ(hud[15], TILE_DIGIT0 + 1);       /* round 001 */
    T_EQ(hud[16], TILE_SLASH);
    T_EQ(hud[18], TILE_DIGIT0 + 4);       /* plafond 040 */
    T_EQ(hud[19], TILE_DIGIT0 + 0);
}

static void test_les_poses_restantes_se_vident(void)
{
    match_start(&m);
    T_TRUE(match_place(&m, 5, 13));
    view_hud(&m, hud);
    T_EQ(hud[6], TILE_PIP_ON);
    T_EQ(hud[7], TILE_PIP_ON);
    T_EQ(hud[8], TILE_PIP_OFF);
    /* Le joueur inactif garde ses trois pastilles pleines. */
    T_EQ(hud[23], TILE_PIP_ON);
    T_EQ(hud[25], TILE_PIP_ON);
}

static void test_le_marqueur_demballement_napparait_qua_partir_du_round_16(void)
{
    match_start(&m);
    view_hud(&m, hud);
    T_EQ(hud[20], TILE_EMPTY);
    T_EQ(hud[21], TILE_EMPTY);
    m.round = RAMPUP_ROUND;
    view_hud(&m, hud);
    T_EQ(hud[20], TILE_TIMES);
    T_EQ(hud[21], TILE_DIGIT0 + TICKS_AFTER_RAMPUP);
}

void suite_view(void)
{
    T_RUN(test_les_cellules_prennent_la_tuile_de_leur_couleur);
    T_RUN(test_la_portee_ne_marque_que_les_cases_vides_atteignables);
    T_RUN(test_sans_marquage_aucune_tuile_de_portee);
    T_RUN(test_les_chiffres_sont_cales_a_droite_sur_trois_rangs);
    T_RUN(test_les_chiffres_saturent_a_999);
    T_RUN(test_le_bandeau_montre_populations_round_et_pastilles);
    T_RUN(test_les_poses_restantes_se_vident);
    T_RUN(test_le_marqueur_demballement_napparait_qua_partir_du_round_16);
}
```

Ajouter `void suite_view(void);` et l'appel dans `tests/main.c`.

- [ ] **Step 2: Lancer les tests et vérifier qu'ils échouent**

Run: `make test`
Expected: `fatal error: 'view.h' file not found`.

- [ ] **Step 3: Implémenter le modèle d'affichage**

`src/core/view.h` : reprendre exactement le bloc « Produces » ci-dessus, entre gardes d'inclusion, avec `#include "match.h"`.

`src/core/view.c` :

```c
#include "view.h"

void view_board(const Match *m, bool_t show_range, u8 out[BOARD_H][BOARD_W])
{
    int x, y;
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            Cell v = board_get(&m->board, x, y);
            if (v == CELL_P1) {
                out[y][x] = TILE_P1;
            } else if (v == CELL_P2) {
                out[y][x] = TILE_P2;
            } else if (show_range &&
                       rules_in_range(&m->range, m->turn, x, y)) {
                out[y][x] = TILE_RANGE;
            } else {
                out[y][x] = TILE_EMPTY;
            }
        }
    }
}

void view_digits3(int value, u8 out[3])
{
    if (value < 0)   value = 0;
    if (value > 999) value = 999;
    out[0] = (u8)(TILE_DIGIT0 + value / 100);
    out[1] = (u8)(TILE_DIGIT0 + (value / 10) % 10);
    out[2] = (u8)(TILE_DIGIT0 + value % 10);
}

static void pips(const Match *m, Cell who, u8 *out)
{
    int left = (m->turn == who) ? (BUDGET - m->placed) : BUDGET;
    int i;
    for (i = 0; i < BUDGET; i++) {
        out[i] = (u8)(i < left ? TILE_PIP_ON : TILE_PIP_OFF);
    }
}

void view_hud(const Match *m, u8 out[HUD_W])
{
    int i;
    int ticks = match_ticks_this_round(m);

    for (i = 0; i < HUD_W; i++) {
        out[i] = TILE_EMPTY;
    }

    out[0]  = TILE_P1;
    out[31] = TILE_P2;
    view_digits3(board_count(&m->board, CELL_P1), &out[2]);
    view_digits3(board_count(&m->board, CELL_P2), &out[27]);
    pips(m, CELL_P1, &out[6]);
    pips(m, CELL_P2, &out[23]);

    out[12] = TILE_R;
    view_digits3(m->round, &out[13]);
    out[16] = TILE_SLASH;
    view_digits3(ROUND_CAP, &out[17]);

    if (ticks > 1) {
        out[20] = TILE_TIMES;
        out[21] = (u8)(TILE_DIGIT0 + ticks);
    }
}
```

- [ ] **Step 4: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: 0 échecs.

- [ ] **Step 5: Commit**

```bash
git add src/core/view.h src/core/view.c tests/test_view.c tests/main.c
git commit -m "✨ [core] Add tile-index view model for board and HUD"
```

---

### Task 6 : L'adversaire CPU

**Files:**
- Create: `src/core/ai.h`, `src/core/ai.c`, `tests/test_ai.c`
- Modify: `tests/main.c` (ajouter `suite_ai`)

**Interfaces:**
- Consumes: `Match`, `Board`, `board_get`, `board_set`, `board_count`, `rules_in_range`, `life_tick`, `board_wrap`.
- Produces :

```c
typedef enum { AI_EASY = 0, AI_NORMAL = 1 } AiLevel;

/* rng : état d'un xorshift32 possédé par l'appelant, jamais nul.
   Ignoré au niveau normal, qui est purement déterministe.
   Rend le nombre de coups écrits dans `out`, de 0 à BUDGET. */
int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET]);

/* Gain net que produit la pose de `who` en (x, y) après `depth` ticks :
   cellules gagnées par `who` moins celles gagnées par l'adversaire, mesuré
   sur le carré de rayon `depth` autour de la pose. Exposé pour le test
   d'équivalence, qui est la garantie de correction de l'optimisation. */
int ai_eval_local(const Board *b, Cell who, int x, int y, int depth);
```

**Le point à ne pas rater.** En jeu de la vie, la différence entre « avec la pose » et « sans la pose » ne peut pas s'éloigner de plus d'une case par tick du point de la pose. Après `depth` ticks elle tient donc dans le carré de rayon `depth`, et pour la calculer exactement il suffit de simuler un carré de rayon `2 × depth` : chaque tick fait perdre une case de validité à la couronne extérieure, et après `depth` ticks il reste précisément le rayon `depth` dont on a besoin. Le test d'équivalence de l'étape 1 vérifie cette égalité contre une simulation sur le plateau entier.

- [ ] **Step 1: Écrire les tests, qui doivent échouer**

`tests/test_ai.c` :

```c
#include "harness.h"
#include "match.h"
#include "ai.h"

static Match m;
static Move mv[BUDGET];

/* ---- référence : la même chose, en simulant tout le plateau ---- */

static int net_after(const Board *base, Cell who, int depth)
{
    static Board a, t;
    Cell foe = (who == CELL_P1) ? CELL_P2 : CELL_P1;
    int i;
    a = *base;
    for (i = 0; i < depth; i++) {
        board_wrap(&a);
        life_tick(&a, &t);
        a = t;
    }
    return board_count(&a, who) - board_count(&a, foe);
}

static int full_delta(const Board *base, Cell who, int x, int y, int depth)
{
    static Board withp;
    int without = net_after(base, who, depth);
    withp = *base;
    board_set(&withp, x, y, who);
    return net_after(&withp, who, depth) - without;
}

/* xorshift32 local aux tests, pour fabriquer des plateaux reproductibles */
static unsigned long tr;
static unsigned long trand(void)
{
    unsigned long x = tr & 0xFFFFFFFFUL;
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x << 5) & 0xFFFFFFFFUL;
    tr = x;
    return x;
}

/* ---- LE test : la fenêtre locale doit égaler le plateau entier ---- */

static void test_la_fenetre_locale_egale_la_simulation_complete(void)
{
    static Board b;
    int trial, depth;

    tr = 0x1234ABCDUL;
    for (trial = 0; trial < 40; trial++) {
        int x, y, i;
        board_clear(&b);
        for (i = 0; i < BOARD_W * BOARD_H / 3; i++) {
            int px = (int)(trand() % BOARD_W);
            int py = (int)(trand() % BOARD_H);
            board_set(&b, px, py, (trand() & 1UL) ? CELL_P1 : CELL_P2);
        }
        do {
            x = (int)(trand() % BOARD_W);
            y = (int)(trand() % BOARD_H);
        } while (board_get(&b, x, y) != CELL_EMPTY);

        for (depth = 1; depth <= 2; depth++) {
            T_EQ(ai_eval_local(&b, CELL_P1, x, y, depth),
                 full_delta(&b, CELL_P1, x, y, depth));
            T_EQ(ai_eval_local(&b, CELL_P2, x, y, depth),
                 full_delta(&b, CELL_P2, x, y, depth));
        }
    }
}

/* ---- comportement ---- */

static void test_les_coups_rendus_sont_legaux_et_distincts(void)
{
    unsigned long rng = 1UL;
    int n, i, j;
    match_start(&m);
    m.turn = CELL_P2;
    m.range = m.board;
    n = ai_choose(&m, AI_NORMAL, &rng, mv);
    T_TRUE(n > 0);
    T_TRUE(n <= BUDGET);
    for (i = 0; i < n; i++) {
        T_TRUE(rules_in_range(&m.range, CELL_P2, (int)mv[i].x, (int)mv[i].y));
        T_EQ(board_get(&m.board, (int)mv[i].x, (int)mv[i].y), CELL_EMPTY);
        for (j = 0; j < i; j++) {
            T_FALSE(mv[i].x == mv[j].x && mv[i].y == mv[j].y);
        }
    }
}

/* Deux cellules bleues isolées meurent au tick suivant. Quatre poses
   referment un bloc immortel de 4 et valent +4 ; les deux poses qui font
   un clignotant ne valent que +3. Les quatre gagnantes sont à égalité,
   et le départage par ordre de balayage désigne (10, 9). */
static void test_lia_referme_le_bloc_plutot_que_le_clignotant(void)
{
    unsigned long rng = 1UL;
    int n;
    match_start(&m);
    board_clear(&m.board);
    board_set(&m.board, 10, 10, CELL_P1);
    board_set(&m.board, 11, 10, CELL_P1);
    board_wrap(&m.board);
    m.turn = CELL_P1;
    m.range = m.board;
    n = ai_choose(&m, AI_NORMAL, &rng, mv);
    T_TRUE(n > 0);
    T_EQ(mv[0].x, 10);
    T_EQ(mv[0].y, 9);
}

static void test_sans_aucune_cellule_lia_passe(void)
{
    unsigned long rng = 1UL;
    match_start(&m);
    board_clear(&m.board);
    board_set(&m.board, 10, 10, CELL_P2);
    board_wrap(&m.board);
    m.turn = CELL_P1;
    m.range = m.board;
    T_EQ(ai_choose(&m, AI_NORMAL, &rng, mv), 0);
}

static void test_le_niveau_facile_est_reproductible(void)
{
    unsigned long r1 = 42UL, r2 = 42UL;
    Move a[BUDGET], b[BUDGET];
    int na, nb, i;
    match_start(&m);
    na = ai_choose(&m, AI_EASY, &r1, a);
    nb = ai_choose(&m, AI_EASY, &r2, b);
    T_EQ(na, nb);
    for (i = 0; i < na; i++) {
        T_EQ(a[i].x, b[i].x);
        T_EQ(a[i].y, b[i].y);
    }
}

/* Le simulateur de la tâche 7 en dépend : une partie doit finir. */
static void test_une_partie_ia_contre_ia_se_termine(void)
{
    unsigned long rng = 7UL;
    int guard = 0;
    match_start(&m);
    while (match_winner(&m) == WINNER_NONE && guard < 4 * ROUND_CAP) {
        int n = ai_choose(&m, AI_NORMAL, &rng, mv);
        int i;
        for (i = 0; i < n; i++) {
            T_TRUE(match_place(&m, (int)mv[i].x, (int)mv[i].y));
        }
        match_end_turn(&m);
        guard++;
    }
    T_TRUE(match_winner(&m) != WINNER_NONE);
}

void suite_ai(void)
{
    T_RUN(test_la_fenetre_locale_egale_la_simulation_complete);
    T_RUN(test_les_coups_rendus_sont_legaux_et_distincts);
    T_RUN(test_lia_referme_le_bloc_plutot_que_le_clignotant);
    T_RUN(test_sans_aucune_cellule_lia_passe);
    T_RUN(test_le_niveau_facile_est_reproductible);
    T_RUN(test_une_partie_ia_contre_ia_se_termine);
}
```

Ajouter `void suite_ai(void);` et l'appel dans `tests/main.c`.

- [ ] **Step 2: Lancer les tests et vérifier qu'ils échouent**

Run: `make test`
Expected: `fatal error: 'ai.h' file not found`.

- [ ] **Step 3: Implémenter l'IA**

`src/core/ai.h` : reprendre le bloc « Produces » ci-dessus, entre gardes d'inclusion, avec `#include "match.h"`.

`src/core/ai.c` :

```c
#include <string.h>
#include "ai.h"

#define AI_MAX_DEPTH  2
#define AI_WIN        (4 * AI_MAX_DEPTH + 1)   /* 9 */
#define AI_MAX_CANDS  256
#define AI_TOPK_EASY  8
#define AI_TOPK_MAX   32

/* Tout est en portée fichier : la pile du 65816 ne supporterait rien de
   cette taille. */
static u8    wa[AI_WIN][AI_WIN];
static u8    wb[AI_WIN][AI_WIN];
static Board work;
static int   scores[AI_TOPK_MAX];

typedef struct { u8 x, y; short pre; } Cand;
static Cand cands[AI_MAX_CANDS];

/* Mêmes fonctions que dans rules.c, et pour la même raison : aucun modulo
   dans les boucles chaudes de l'IA. Les décalages y vont jusqu'à 4, ce qui
   reste bien en deçà des dimensions du plateau. */
static int wrap_x(int x)
{
    if (x < 0)        return x + BOARD_W;
    if (x >= BOARD_W) return x - BOARD_W;
    return x;
}

static int wrap_y(int y)
{
    if (y < 0)        return y + BOARD_H;
    if (y >= BOARD_H) return y - BOARD_H;
    return y;
}

/* Masqué sur 32 bits pour que la suite soit identique sur l'hôte, où `long`
   fait souvent 64 bits, et sur la console, où il en fait 32. Sans quoi le
   simulateur et la ROM divergeraient. */
static unsigned long xs32(unsigned long *s)
{
    unsigned long x = *s & 0xFFFFFFFFUL;
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x <<  5) & 0xFFFFFFFFUL;
    *s = x;
    return x;
}

/* ---- la fenêtre locale ---- */

static void win_load(const Board *b, int cx, int cy, int side)
{
    int r = side / 2, i, j;
    for (j = 0; j < side; j++) {
        for (i = 0; i < side; i++) {
            wa[j][i] = (u8)board_get(b, wrap_x(cx - r + i), wrap_y(cy - r + j));
        }
    }
}

/* Ne calcule que l'intérieur : la couronne extérieure devient invalide.
   Le rayon utile perd une case par tick, et après `depth` ticks il reste
   exactement `depth`, ce que win_net mesure. */
static void win_tick(int side)
{
    int i, j;
    for (j = 1; j < side - 1; j++) {
        for (i = 1; i < side - 1; i++) {
            int n = 0, n1 = 0, di, dj;
            u8 self;
            for (dj = -1; dj <= 1; dj++) {
                for (di = -1; di <= 1; di++) {
                    u8 v;
                    if (di == 0 && dj == 0) continue;
                    v = wa[j + dj][i + di];
                    if (v) { n++; if (v == (u8)CELL_P1) n1++; }
                }
            }
            self = wa[j][i];
            if (self) {
                wb[j][i] = (n == 2 || n == 3) ? self : (u8)CELL_EMPTY;
            } else {
                wb[j][i] = (n == 3)
                    ? (u8)(n1 >= 2 ? CELL_P1 : CELL_P2)
                    : (u8)CELL_EMPTY;
            }
        }
    }
    memcpy(wa, wb, sizeof wa);
}

static int win_net(int side, int radius, Cell me)
{
    int c = side / 2, i, j, net = 0;
    for (j = c - radius; j <= c + radius; j++) {
        for (i = c - radius; i <= c + radius; i++) {
            u8 v = wa[j][i];
            if (v == (u8)me)      net++;
            else if (v != CELL_EMPTY) net--;
        }
    }
    return net;
}

int ai_eval_local(const Board *b, Cell who, int x, int y, int depth)
{
    int side = 4 * depth + 1;
    int t, without, with;

    win_load(b, x, y, side);
    for (t = 0; t < depth; t++) win_tick(side);
    without = win_net(side, depth, who);

    win_load(b, x, y, side);
    wa[side / 2][side / 2] = (u8)who;
    for (t = 0; t < depth; t++) win_tick(side);
    with = win_net(side, depth, who);

    return with - without;
}

/* ---- les candidats ---- */

static int live_neighbors(const Board *b, int x, int y)
{
    int dx, dy, n = 0;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            if (board_get(b, wrap_x(x + dx), wrap_y(y + dy)) != CELL_EMPTY) n++;
        }
    }
    return n;
}

/* max(0, 4 - distance de Chebyshev à l'adversaire le plus proche) : pousse
   l'IA vers le contact, où l'Immigration Game permet de retourner des
   naissances. */
static int enemy_pull(const Board *b, Cell foe, int x, int y)
{
    int dx, dy, best = 0;
    for (dy = -4; dy <= 4; dy++) {
        for (dx = -4; dx <= 4; dx++) {
            int a, c;
            if (board_get(b, wrap_x(x + dx), wrap_y(y + dy)) != foe) continue;
            a = (dx < 0) ? -dx : dx;
            c = (dy < 0) ? -dy : dy;
            if (c > a) a = c;
            if (4 - a > best) best = 4 - a;
        }
    }
    return best;
}

static int scan_index(const Cand *c) { return (int)c->y * BOARD_W + (int)c->x; }

/* Remplit `cands` dans l'ordre de balayage : y croissant puis x croissant.
   C'est cet ordre qui sert de départage. */
static int collect(const Board *cur, const Board *range, Cell me, Cell foe)
{
    int x, y, n = 0;
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            int nb;
            if (board_get(cur, x, y) != CELL_EMPTY) continue;
            if (!rules_in_range(range, me, x, y)) continue;
            nb = live_neighbors(cur, x, y);
            if (nb == 0) continue;   /* posée dans le vide, elle meurt sans rien produire */
            if (n >= AI_MAX_CANDS) return n;
            cands[n].x = (u8)x;
            cands[n].y = (u8)y;
            cands[n].pre = (short)(2 * nb + enemy_pull(cur, foe, x, y));
            n++;
        }
    }
    return n;
}

static void swap_cand(int i, int j)
{
    Cand t = cands[i]; cands[i] = cands[j]; cands[j] = t;
}

/* Amène les k meilleurs par `pre` en tête. */
static void select_top(int n, int k)
{
    int i, j;
    for (i = 0; i < k && i < n; i++) {
        int best = i;
        for (j = i + 1; j < n; j++) {
            if (cands[j].pre > cands[best].pre ||
                (cands[j].pre == cands[best].pre &&
                 scan_index(&cands[j]) < scan_index(&cands[best]))) {
                best = j;
            }
        }
        if (best != i) swap_cand(i, best);
    }
}

/* Amène les trois meilleurs par score évalué en tête, cands et scores
   déplacés ensemble. */
static void select_top3_by_score(int top)
{
    int i, j, lim = (top < 3) ? top : 3;
    for (i = 0; i < lim; i++) {
        int best = i;
        for (j = i + 1; j < top; j++) {
            if (scores[j] > scores[best] ||
                (scores[j] == scores[best] &&
                 scan_index(&cands[j]) < scan_index(&cands[best]))) {
                best = j;
            }
        }
        if (best != i) {
            int ts = scores[i];
            scores[i] = scores[best];
            scores[best] = ts;
            swap_cand(i, best);
        }
    }
}

int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET])
{
    int depth = (lvl == AI_EASY) ? 1 : AI_MAX_DEPTH;
    int k     = (lvl == AI_EASY) ? AI_TOPK_EASY : AI_TOPK_MAX;
    Cell me   = m->turn;
    Cell foe  = (me == CELL_P1) ? CELL_P2 : CELL_P1;
    int made;

    /* `work` n'est jamais wrappé : rien ici ne lit le halo. */
    work = m->board;

    for (made = 0; made < BUDGET; made++) {
        int n, i, top, pick;

        n = collect(&work, &m->range, me, foe);
        if (n == 0) break;

        select_top(n, k);
        top = (n < k) ? n : k;
        for (i = 0; i < top; i++) {
            scores[i] = ai_eval_local(&work, me,
                                      (int)cands[i].x, (int)cands[i].y, depth);
        }

        if (lvl == AI_EASY) {
            int pool = (top < 3) ? top : 3;
            select_top3_by_score(top);
            pick = (int)(xs32(rng) % (unsigned long)pool);
        } else {
            pick = 0;
            for (i = 1; i < top; i++) {
                if (scores[i] > scores[pick] ||
                    (scores[i] == scores[pick] &&
                     scan_index(&cands[i]) < scan_index(&cands[pick]))) {
                    pick = i;
                }
            }
        }

        out[made].x = cands[pick].x;
        out[made].y = cands[pick].y;
        /* Fixée sur le plateau de travail : la pose suivante est évaluée
           en tenant compte de celle-ci, ce qui permet de trouver des
           combinaisons de deux ou trois cellules. */
        board_set(&work, (int)cands[pick].x, (int)cands[pick].y, me);
    }

    return made;
}
```

- [ ] **Step 4: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: 0 échecs. Le test d'équivalence à lui seul fait 320 comparaisons.

- [ ] **Step 5: Commit**

```bash
git add src/core/ai.h src/core/ai.c tests/test_ai.c tests/main.c
git commit -m "✨ [core] Add greedy CPU opponent with exact local evaluation"
```

---

### Task 7 : Le simulateur et le réglage de l'équilibrage

Cette tâche produit des **mesures**, puis fige `config.h` sur ces mesures. C'est elle qui répond à la question laissée ouverte par la spec : l'élimination arrive-t-elle vraiment, et à quel round faut-il déclencher l'emballement ?

**Files:**
- Create: `tools/sim.c`
- Create: `docs/balance.md`
- Modify: `Makefile` (cible `sim`)
- Modify: `src/core/config.h` (valeurs finales, à l'étape 5)

**Interfaces:**
- Consumes: `match_*`, `ai_choose`, `board_count`, tout `src/core/`.
- Produces: `make sim` produit `build/sim`, et `docs/balance.md` consigne les mesures et le raisonnement derrière les valeurs retenues.

- [ ] **Step 1: Écrire le simulateur**

`tools/sim.c` :

```c
#include <stdio.h>
#include <stdlib.h>
#include "match.h"
#include "ai.h"

static Match m;
static Move mv[BUDGET];

typedef struct {
    int elim, cap, draw, p1, p2;
    long rounds;      /* somme des rounds atteints, pour la moyenne */
    long pop_winner;  /* somme des populations gagnantes */
} Stats;

static void play_one(unsigned long *rng, Stats *s)
{
    int guard = 0;
    Winner w;

    match_start(&m);
    while (match_winner(&m) == WINNER_NONE && guard < 4 * ROUND_CAP) {
        int n = ai_choose(&m, AI_NORMAL, rng, mv);
        int i;
        for (i = 0; i < n; i++) {
            match_place(&m, (int)mv[i].x, (int)mv[i].y);
        }
        match_end_turn(&m);
        guard++;
    }

    w = match_winner(&m);
    s->rounds += m.round;
    /* Se fier au round serait faux : une élimination peut très bien tomber
       sur le tick du round 40. C'est la population nulle qui tranche. */
    if (board_count(&m.board, CELL_P1) == 0 ||
        board_count(&m.board, CELL_P2) == 0) s->elim++;
    else                                     s->cap++;
    if (w == WINNER_P1)      s->p1++;
    else if (w == WINNER_P2) s->p2++;
    else                     s->draw++;
    s->pop_winner += board_count(&m.board,
                                 (w == WINNER_P2) ? CELL_P2 : CELL_P1);
}

int main(int argc, char **argv)
{
    int games = (argc > 1) ? atoi(argv[1]) : 100;
    unsigned long seed = (argc > 2) ? strtoul(argv[2], 0, 0) : 1UL;
    Stats s;
    int i;

    s.elim = s.cap = s.draw = s.p1 = s.p2 = 0;
    s.rounds = 0;
    s.pop_winner = 0;

    for (i = 0; i < games; i++) {
        unsigned long rng = seed + (unsigned long)i;
        play_one(&rng, &s);
    }

    printf("parties        %d\n", games);
    printf("réglages       BUDGET=%d RANGE=%d RAMPUP=%d TICKS=%d CAP=%d\n",
           BUDGET, RANGE_RADIUS, RAMPUP_ROUND, TICKS_AFTER_RAMPUP, ROUND_CAP);
    printf("élimination    %d (%.0f%%)\n", s.elim, 100.0 * s.elim / games);
    printf("plafond        %d (%.0f%%)\n", s.cap,  100.0 * s.cap  / games);
    printf("victoires P1   %d (%.0f%%)\n", s.p1,   100.0 * s.p1   / games);
    printf("victoires P2   %d (%.0f%%)\n", s.p2,   100.0 * s.p2   / games);
    printf("nuls           %d\n", s.draw);
    printf("round moyen    %.1f\n", (double)s.rounds / games);
    printf("pop. gagnante  %.1f\n", (double)s.pop_winner / games);
    return 0;
}
```

`tools/sim.c` n'est pas soumis à la règle C89 : il ne part jamais sur la console. Il utilise `stdio.h` et des flottants, ce que `src/core/` s'interdit.

- [ ] **Step 2: Ajouter la cible `sim` au Makefile**

```make
SIMFLAGS := -std=c99 -Wall -Wextra -O2 -Isrc/core

.PHONY: sim
sim: build/sim
build/sim: $(CORE_SRC) tools/sim.c | build
	$(CC) $(SIMFLAGS) $^ -o $@
```

- [ ] **Step 3: Mesurer la configuration de départ**

```bash
make sim && ./build/sim 200 1
```

Attendu : une sortie complète, sans plantage, en moins de dix secondes.

- [ ] **Step 4: Balayer les réglages**

Faire varier `RAMPUP_ROUND` dans `src/core/config.h` et relancer, en notant chaque fois le taux d'élimination et le round moyen :

```bash
for r in 8 12 16 20 40; do
  sed -i '' "s/#define RAMPUP_ROUND .*/#define RAMPUP_ROUND      $r/" src/core/config.h
  make -s sim && echo "== RAMPUP=$r ==" && ./build/sim 200 1 | grep -E 'élimination|round moyen'
done
```

Répéter le balayage pour `BUDGET` sur 2, 3, 4 et pour `RANGE_RADIUS` sur 1, 2, 3.

Ce `sed` écrase le commentaire de fin de ligne : c'est sans importance, l'étape 5 réécrit `config.h` proprement.

- [ ] **Step 5: Arrêter les valeurs et les consigner**

Critères de choix, dans cet ordre :

1. **le taux d'élimination dépasse 50 %** — c'est la condition de victoire principale voulue par la spec ; si aucun réglage n'y parvient, retenir le meilleur et le signaler explicitement dans `docs/balance.md` comme un écart à la spec à arbitrer ;
2. **l'écart entre victoires P1 et P2 reste sous 60/40** — au-delà, l'avantage du joueur qui ouvre est trop lourd ;
3. **le round moyen tombe entre 15 et 30** — une partie de cinq à dix minutes.

Écrire `docs/balance.md` avec le tableau complet des mesures, les valeurs retenues, et pour chacune la raison du choix. Reporter les valeurs retenues dans `src/core/config.h`.

- [ ] **Step 6: Vérifier que les tests tiennent toujours**

Run: `make test`
Expected: 0 échecs.

Les tests du cœur sont écrits en fonction de `RAMPUP_ROUND`, `BUDGET` et `ROUND_CAP` et non de leurs valeurs littérales : ils doivent survivre au changement. **Si un test casse, c'est le test qu'il faut corriger, pas le réglage** — sauf `test_trois_poses_puis_le_budget_est_epuise` et les tests de portée de la tâche 4, dont les coordonnées codées en dur dépendent de `RANGE_RADIUS` : les recalculer si ce rayon change.

- [ ] **Step 7: Commit**

```bash
git add tools/sim.c docs/balance.md src/core/config.h Makefile
git commit -m "📊 [tools] Add headless match simulator and tune balance"
```

---

## Note sur la vérification des tâches 8 à 12

Ces tâches produisent de l'affichage : elles ne se vérifient pas par assertion mais par observation. Chaque étape de vérification décrit **précisément ce qui doit apparaître à l'écran**. Si la tâche 0 a établi que l'émulateur retenu sait capturer sans interface, automatiser ces contrôles ; sinon un humain regarde et confirme.

La logique testable de ces tâches a déjà été couverte par les tâches 5 et 6 : `view_board` et `view_hud` sont vérifiées sur l'hôte. Ce qui reste ici est du transfert vers la VRAM et de la lecture de manette, qui ne s'abstrait pas utilement.

---

### Task 8 : Les tuiles et l'affichage de la grille

**Files:**
- Create: `tools/mktiles.py`, `data/tiles.bmp` (généré)
- Create: `src/snes/render.h`, `src/snes/render.c`
- Modify: `src/snes/main.c`, `Makefile`

**Interfaces:**
- Consumes: `view_board`, `view_hud`, les constantes `TILE_*` de la tâche 5 ; les appels PVSnesLib consignés dans `docs/snes-notes.md` à la tâche 0.
- Produces:
  - `data/tiles.bmp`, planche indexée 16 couleurs de 128 × 16 pixels, soit 32 emplacements de tuiles dont 19 utilisés, dans l'ordre exact des constantes `TILE_*`.
  - `void render_init(void);` — charge tuiles et palette en VRAM, configure le mode 1.
  - `void render_board_now(const Match *m, bool_t show_range);` — construit la tilemap de BG1 depuis `view_board` et la transfère.

- [ ] **Step 1: Écrire le générateur de planche**

`tools/mktiles.py` — les tuiles sont générées, pas dessinées à la main, pour que la planche soit reproductible et versionnable en diff lisible :

```python
#!/usr/bin/env python3
"""Génère data/tiles.bmp : planche 4bpp de 32 emplacements de 8x8.

L'ordre des tuiles suit exactement les constantes TILE_* de src/core/view.h.
Relancer après toute modification de cet ordre.
"""
import struct, pathlib

W_TILES, H_TILES, TS = 16, 2, 8
W, H = W_TILES * TS, H_TILES * TS

PALETTE = [
    (0x10, 0x18, 0x28),   # 0 fond
    (0x2A, 0x34, 0x48),   # 1 point de portée
    (0x3B, 0x82, 0xF6),   # 2 bleu clair
    (0x1D, 0x4E, 0xD8),   # 3 bleu foncé
    (0xEF, 0x44, 0x44),   # 4 rouge clair
    (0xB9, 0x1C, 0x1C),   # 5 rouge foncé
    (0xE8, 0xEC, 0xF4),   # 6 blanc
    (0x4A, 0x55, 0x68),   # 7 gris
]
PALETTE += [(0, 0, 0)] * (16 - len(PALETTE))

FONT = {
 '0': ["#####","#...#","#...#","#...#","#...#","#...#","#####"],
 '1': ["..#..",".##..","..#..","..#..","..#..","..#..",".###."],
 '2': ["#####","....#","....#","#####","#....","#....","#####"],
 '3': ["#####","....#","....#","#####","....#","....#","#####"],
 '4': ["#...#","#...#","#...#","#####","....#","....#","....#"],
 '5': ["#####","#....","#....","#####","....#","....#","#####"],
 '6': ["#####","#....","#....","#####","#...#","#...#","#####"],
 '7': ["#####","....#","....#","...#.","..#..","..#..","..#.."],
 '8': ["#####","#...#","#...#","#####","#...#","#...#","#####"],
 '9': ["#####","#...#","#...#","#####","....#","....#","#####"],
 'R': ["####.","#...#","#...#","####.","#.#..","#..#.","#...#"],
 '/': ["....#","....#","...#.","..#..",".#...","#....","#...."],
 'X': [".....","#...#",".#.#.","..#..",".#.#.","#...#","....."],
}

def blank():
    return [[0] * TS for _ in range(TS)]

def cell(light, dark):
    """Carré de 6x6 bordé, laissant un pixel de gouttière sur deux côtés."""
    t = blank()
    for y in range(1, 7):
        for x in range(1, 7):
            edge = y in (1, 6) or x in (1, 6)
            t[y][x] = dark if edge else light
    return t

def glyph(ch, colour):
    t = blank()
    for y, row in enumerate(FONT[ch]):
        for x, c in enumerate(row):
            if c == '#':
                t[y][x + 1] = colour
    return t

def disc(colour, filled):
    t = blank()
    pts = [(2,3),(2,4),(3,2),(3,5),(4,2),(4,5),(5,3),(5,4)]
    if filled:
        pts += [(3,3),(3,4),(4,3),(4,4)]
    for y, x in pts:
        t[y][x] = colour
    return t

def range_dot():
    t = blank()
    for y, x in ((3,3),(3,4),(4,3),(4,4)):
        t[y][x] = 1
    return t

tiles = [blank(), range_dot(), cell(2, 3), cell(4, 5)]      # 0..3
tiles += [glyph(str(d), 6) for d in range(10)]              # 4..13
tiles += [disc(6, True), disc(7, False)]                    # 14, 15
tiles += [glyph('R', 6), glyph('/', 6), glyph('X', 6)]      # 16, 17, 18
tiles += [blank()] * (W_TILES * H_TILES - len(tiles))

# Composition de la planche
img = [[0] * W for _ in range(H)]
for i, t in enumerate(tiles):
    ox, oy = (i % W_TILES) * TS, (i // W_TILES) * TS
    for y in range(TS):
        for x in range(TS):
            img[oy + y][ox + x] = t[y][x]

# BMP 8 bits indexé, lignes du bas vers le haut, chaque ligne alignée sur 4
row_pad = (-W) % 4
pixels = b''.join(bytes(img[y]) + b'\0' * row_pad for y in reversed(range(H)))
palette = b''.join(struct.pack('<BBBB', b, g, r, 0) for (r, g, b) in PALETTE)
offset = 14 + 40 + len(palette)
out = (struct.pack('<2sIHHI', b'BM', offset + len(pixels), 0, 0, offset)
       + struct.pack('<IiiHHIIiiII', 40, W, H, 1, 8, 0, len(pixels), 2835, 2835, 16, 16)
       + palette + pixels)

path = pathlib.Path(__file__).resolve().parent.parent / 'data' / 'tiles.bmp'
path.parent.mkdir(exist_ok=True)
path.write_bytes(out)
print(f"{path} : {W}x{H}, {len(tiles)} emplacements")
```

- [ ] **Step 2: Générer la planche et la contrôler**

```bash
python3 tools/mktiles.py
open data/tiles.bmp
```

Attendu : une bande de 128 × 16 pixels. Rangée du haut : une case vide, un point gris, un carré bleu bordé, un carré rouge bordé, puis les chiffres 0 à 9 en blanc, puis un disque plein et un anneau gris. Rangée du bas : `R`, `/`, `×`, puis du vide.

- [ ] **Step 3: Convertir la planche et l'intégrer au build**

Ajouter au `Makefile`, avant la cible `rom` :

```make
GFX4SNES := $(PVSNESLIB_HOME)/devkitsnes/tools/gfx4snes

data/tiles.pic data/tiles.pal: data/tiles.bmp
	$(GFX4SNES) -s 8 -o 16 -u 16 -t bmp -e 0 -p -i $<

data/tiles.bmp: tools/mktiles.py
	python3 $<
```

Les options exactes de `gfx4snes` dépendent de la version installée : partir de celles de l'exemple validé à la tâche 0 et les adapter à une planche 4bpp de 16 couleurs.

- [ ] **Step 4: Écrire le module de rendu**

`src/snes/render.h` :

```c
#ifndef RENDER_H
#define RENDER_H

#include "match.h"

void render_init(void);
void render_board_now(const Match *m, bool_t show_range);

#endif
```

`src/snes/render.c` :

```c
#include <snes.h>
#include "render.h"
#include "view.h"

/* Une entrée de tilemap SNES tient sur 16 bits : bits 0 à 9 le numéro de
   tuile, 10 à 12 la palette, 13 la priorité, 14 et 15 les miroirs. On
   n'utilise que la palette 0, donc l'entrée vaut le numéro de tuile. */
static unsigned short map_bg1[32 * 32];
static u8 grid[BOARD_H][BOARD_W];

/* Symboles produits par gfx4snes puis assemblés dans la ROM. Leur
   orthographe exacte dépend de la version de l'outil : reprendre celle
   consignée dans docs/snes-notes.md à la tâche 0. */
extern char tiles_pic[], tiles_pal[];

void render_init(void)
{
    /* Séquence exacte consignée dans docs/snes-notes.md à la tâche 0 :
       initialisation console, chargement du jeu de tuiles et de la palette
       en VRAM, réglage des adresses de BG1 et BG2, passage en mode 1. */
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
    /* Transfert DMA de map_bg1 vers l'adresse de tilemap de BG1, appel
       consigné à la tâche 0. À faire pendant le VBlank. */
}
```

- [ ] **Step 5: Afficher la position de départ**

Dans `src/snes/main.c` : appeler `render_init`, `match_start`, `render_board_now(&m, FALSE)`, puis boucler sur l'attente du VBlank.

- [ ] **Step 6: Vérifier à l'écran**

```bash
make rom && open build/immigration.sfc
```

Attendu, précisément : sur les 192 pixels du haut, un damier de 32 × 24 cases sombres. En bas à gauche, un carré bleu de 2 × 2 cases vers la colonne 6, ligne 14. En haut à gauche, cinq cases bleues formant un planeur vers la colonne 7, ligne 6. Et les deux mêmes figures en rouge, en symétrie exacte par rotation de 180° : le bloc rouge en haut à droite, le planeur rouge en bas à droite. Le bas de l'écran est encore vide.

- [ ] **Step 7: Commit**

```bash
git add tools/mktiles.py data/tiles.bmp src/snes/render.h src/snes/render.c src/snes/main.c Makefile
git commit -m "🎨 [snes] Render the board on BG1"
```

---

### Task 9 : Le bandeau et le curseur

**Files:**
- Modify: `src/snes/render.h`, `src/snes/render.c`, `src/snes/main.c`

**Interfaces:**
- Consumes: `view_hud`, `HUD_W`, `TILE_*` ; `render_init` de la tâche 8.
- Produces:
  - `void render_hud_now(const Match *m, bool_t blink_on);` — construit la tilemap de BG2 depuis `view_hud` et la transfère. `blink_on` fait clignoter la pastille du joueur actif en la remplaçant par `TILE_EMPTY` une alternance sur deux.
  - `void render_cursor(int x, int y, bool_t visible);` — place le sprite du curseur sur la case de jeu `(x, y)`.

- [ ] **Step 1: Étendre le module de rendu**

Ajouter à `src/snes/render.c` :

```c
static unsigned short map_bg2[32 * 32];
static u8 hud[HUD_W];

void render_hud_now(const Match *m, bool_t blink_on)
{
    int i;
    view_hud(m, hud);
    if (!blink_on) {
        /* La pastille du joueur actif s'éteint une alternance sur deux :
           c'est ce qui signale à qui est le tour. */
        hud[(m->turn == CELL_P1) ? 0 : 31] = TILE_EMPTY;
    }
    for (i = 0; i < 32 * 32; i++) {
        map_bg2[i] = TILE_EMPTY;
    }
    for (i = 0; i < HUD_W; i++) {
        map_bg2[i] = (unsigned short)hud[i];
    }
    /* Transfert DMA vers la tilemap de BG2, appel consigné à la tâche 0.
       BG2 est décalé verticalement pour que sa première ligne de tuiles
       tombe sous la grille, à y = 192. */
}

void render_cursor(int x, int y, bool_t visible)
{
    /* Un sprite 8x8 posé en (x * 8, y * 8). Appel oamSet consigné à la
       tâche 0 ; masquer le sprite quand `visible` est faux. */
}
```

Le sprite du curseur est un cadre de 8 × 8 : quatre segments d'angle en blanc sur fond transparent. L'ajouter à `tools/mktiles.py` comme une planche d'objets séparée, `data/sprites.bmp`, de 8 × 8 pixels, car les sprites et les fonds ne partagent pas la même VRAM ni les mêmes palettes.

- [ ] **Step 2: Câbler dans la boucle principale**

Dans `src/snes/main.c`, tenir un compteur de frames et appeler à chaque VBlank :

```c
frame++;
render_board_now(&m, FALSE);
render_hud_now(&m, (frame & 16) != 0);   /* alternance à ~1,9 Hz */
render_cursor(cursor_x, cursor_y, TRUE);
```

Positionner le curseur en dur au centre pour cette tâche : le déplacement vient à la tâche 10.

- [ ] **Step 3: Vérifier à l'écran**

```bash
make rom && open build/immigration.sfc
```

Attendu, précisément : sous la grille, une ligne de bandeau. À l'extrême gauche un carré bleu qui clignote environ deux fois par seconde, puis `009`, puis trois disques blancs pleins. Au centre `R001/040`, sans marqueur `×`. À droite, en miroir, trois disques pleins, `009`, et un carré rouge fixe. Un cadre blanc de curseur est visible au centre de la grille.

- [ ] **Step 4: Commit**

```bash
git add tools/mktiles.py data/sprites.bmp src/snes/render.h src/snes/render.c src/snes/main.c
git commit -m "🎨 [snes] Render the HUD and the cursor sprite"
```

---

### Task 10 : Les entrées et la partie à deux

À la fin de cette tâche, le jeu est **jouable de bout en bout à deux**. C'est le premier jalon où le projet est un jeu.

**Files:**
- Create: `src/snes/input.h`, `src/snes/input.c`
- Modify: `src/snes/main.c`

**Interfaces:**
- Consumes: `match_place`, `match_undo`, `match_end_turn`, `match_winner` ; `render_*` des tâches 8 et 9 ; les noms de touches PVSnesLib consignés à la tâche 0.
- Produces:

```c
typedef struct {
    int    x, y;          /* case de jeu sous le curseur */
    bool_t show_range;    /* marquage de portée affiché */
    int    repeat;        /* frames avant la prochaine répétition */
} Cursor;

void   input_init(Cursor *c);
/* Une passe par frame. Rend TRUE si le joueur a demandé la fin de son tour. */
bool_t input_update(Cursor *c, Match *m);
```

- [ ] **Step 1: Écrire le module d'entrées**

`src/snes/input.c` :

```c
#include <snes.h>
#include "input.h"

#define REPEAT_FIRST 15   /* frames avant la première répétition */
#define REPEAT_NEXT   4   /* puis une case toutes les 4 frames */

static unsigned short prev;

void input_init(Cursor *c)
{
    c->x = BOARD_W / 2;
    c->y = BOARD_H / 2;
    c->show_range = TRUE;
    c->repeat = 0;
    prev = 0;
}

/* Le curseur circule sur le tore, comme le plateau. */
static void move(Cursor *c, int dx, int dy)
{
    c->x = (c->x + dx + BOARD_W) % BOARD_W;
    c->y = (c->y + dy + BOARD_H) % BOARD_H;
}

bool_t input_update(Cursor *c, Match *m)
{
    unsigned short pad = padsCurrent(0);
    unsigned short hit = (unsigned short)(pad & ~prev);
    int dx = 0, dy = 0;
    prev = pad;

    if (pad & KEY_LEFT)  dx = -1;
    if (pad & KEY_RIGHT) dx =  1;
    if (pad & KEY_UP)    dy = -1;
    if (pad & KEY_DOWN)  dy =  1;

    if (dx != 0 || dy != 0) {
        if (hit & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)) {
            c->repeat = REPEAT_FIRST;
            move(c, dx, dy);
        } else if (--c->repeat <= 0) {
            c->repeat = REPEAT_NEXT;
            move(c, dx, dy);
        }
    } else {
        c->repeat = 0;
    }

    if (hit & KEY_A)      match_place(m, c->x, c->y);
    if (hit & KEY_B)      match_undo(m);
    if (hit & KEY_SELECT) c->show_range = (bool_t)!c->show_range;

    return (bool_t)((hit & KEY_START) ? TRUE : FALSE);
}
```

Une pose refusée par `match_place` ne consomme rien : la fonction rend simplement `FALSE`, que l'appelant ignore. Le bip d'erreur mentionné au § 4.1 de la spec arrive à la tâche 12 avec le reste du son.

- [ ] **Step 2: Écrire la boucle de jeu**

`src/snes/main.c` :

```c
#include <snes.h>
#include "match.h"
#include "render.h"
#include "input.h"

typedef enum { GS_TURN, GS_RESOLVE, GS_OVER } GameState;

#define RESOLVE_HOLD 45   /* frames de pause pour voir le résultat du tick */

static Match m;
static Cursor cur;

int main(void)
{
    GameState state = GS_TURN;
    int frame = 0;
    int hold = 0;

    render_init();
    match_start(&m);
    input_init(&cur);

    for (;;) {
        bool_t ticked = FALSE;

        if (state == GS_TURN) {
            if (input_update(&cur, &m)) {
                /* Un tick n'a lieu qu'à la fin du tour du second joueur. */
                ticked = (bool_t)(m.turn == CELL_P2);
                match_end_turn(&m);
                if (match_winner(&m) != WINNER_NONE) {
                    state = GS_OVER;
                } else if (ticked) {
                    state = GS_RESOLVE;
                    hold = RESOLVE_HOLD;
                }
            }
        } else if (state == GS_RESOLVE) {
            if (--hold <= 0) state = GS_TURN;
        }

        render_board_now(&m, (bool_t)(state == GS_TURN && cur.show_range));
        render_hud_now(&m, (bool_t)((frame & 16) != 0));
        render_cursor(cur.x, cur.y, (bool_t)(state == GS_TURN));

        frame++;
        WaitForVBlank();
    }
}
```

- [ ] **Step 3: Jouer une partie complète à deux**

```bash
make rom && open build/immigration.sfc
```

Contrôler dans l'ordre, chaque point devant se vérifier à l'écran :

1. Le curseur se déplace case par case, puis en continu quand on maintient la direction, et il ressort de l'autre côté du plateau aux quatre bords.
2. `SELECT` fait apparaître et disparaître les points de portée. Ils forment deux amas autour des figures bleues quand c'est au bleu de jouer.
3. `A` sur une case pointillée pose une cellule bleue et éteint une pastille du bandeau.
4. `A` sur une case sans point ne fait rien, et n'éteint aucune pastille.
5. `B` retire la dernière cellule posée et rallume la pastille.
6. Après trois poses, `A` ne fait plus rien.
7. `START` passe au rouge : la pastille clignotante change de côté, les points de portée se déplacent sur les figures rouges, les trois pastilles rouges sont pleines.
8. `START` de nouveau : le plateau évolue d'un pas, les populations du bandeau changent, le round passe à `R002/040`, et le curseur disparaît pendant environ trois quarts de seconde.
9. Au round 16, le marqueur `×2` apparaît au centre du bandeau et chaque round fait évoluer le plateau de deux pas.
10. Une partie va jusqu'à son terme sans blocage ni corruption d'affichage.

- [ ] **Step 4: Commit**

```bash
git add src/snes/input.h src/snes/input.c src/snes/main.c
git commit -m "✨ [snes] Add controls and the two-player game loop"
```

---

### Task 11 : Brancher l'adversaire CPU

**Files:**
- Modify: `src/snes/main.c`
- Modify: `docs/snes-notes.md` (consigner la mesure de l'étape 3)

**Interfaces:**
- Consumes: `ai_choose`, `AiLevel` de la tâche 6 ; la boucle de la tâche 10.
- Produces: une variable de partie `cpu_level`, valant `-1` pour un adversaire humain, `AI_EASY` ou `AI_NORMAL` sinon. La tâche 12 la renseigne depuis le menu ; ici elle est fixée en dur à `AI_NORMAL`.

**Écart assumé par rapport au § 6.3 de la spec.** La spec prévoit d'étaler la réflexion de l'IA sur plusieurs frames pour que le HUD reste animé. Cette tâche fait d'abord le plus simple : un appel bloquant, précédé de l'affichage d'un indicateur. L'étape 3 **mesure** le coût réel, et l'étape 4 ne construit la machine à états que si la mesure le justifie. Construire d'abord et mesurer ensuite reviendrait à payer une complexité peut-être inutile.

- [ ] **Step 1: Ajouter l'état de réflexion à la boucle**

Dans `src/snes/main.c` :

```c
static int cpu_level = AI_NORMAL;   /* -1 pour un second joueur humain */
static unsigned long rng = 0x2545F491UL;
static Move mv[BUDGET];

/* Rendu une fois avant l'appel bloquant : l'indicateur doit être à l'écran
   pendant que le CPU calcule. */
static void cpu_take_turn(void)
{
    int n, i;
    render_hud_now(&m, FALSE);          /* pastille rouge éteinte : il réfléchit */
    render_cursor(0, 0, FALSE);
    WaitForVBlank();

    n = ai_choose(&m, (AiLevel)cpu_level, &rng, mv);
    for (i = 0; i < n; i++) {
        match_place(&m, (int)mv[i].x, (int)mv[i].y);
    }
}
```

Dans `GS_TURN`, avant de lire la manette :

```c
if (cpu_level >= 0 && m.turn == CELL_P2) {
    cpu_take_turn();
    ticked = TRUE;
    match_end_turn(&m);
    if (match_winner(&m) != WINNER_NONE) state = GS_OVER;
    else { state = GS_RESOLVE; hold = RESOLVE_HOLD; }
} else if (input_update(&cur, &m)) {
    ...
}
```

- [ ] **Step 2: Vérifier que le CPU joue**

```bash
make rom && open build/immigration.sfc
```

Attendu : après le `START` du joueur bleu, la pastille rouge s'éteint, une pause visible se produit, trois cellules rouges apparaissent d'un coup dans la zone de portée rouge, puis le plateau évolue. Le CPU ne pose jamais hors de sa portée ni sur une case occupée.

- [ ] **Step 3: Mesurer le temps de réflexion**

Compter les frames que dure l'appel : incrémenter un compteur global dans le gestionnaire de VBlank, lire sa valeur avant et après `ai_choose`, et afficher la différence sur trois chiffres à la place du numéro de round. **Consigner le nombre mesuré dans `docs/snes-notes.md`.**

- [ ] **Step 4: Étaler la réflexion, seulement si la mesure dépasse 30 frames**

Si la mesure de l'étape 3 est **inférieure ou égale à 30 frames** (une demi-seconde), s'arrêter ici : l'attente est acceptable pour un jeu au tour par tour, et la machine à états ne serait que du poids mort. Retirer le compteur de mesure et passer à l'étape 5.

Si elle **dépasse 30 frames**, rendre l'IA reprenable. Découper `ai_choose` en :

```c
typedef struct {
    Cell me, foe;
    int  depth, k, n, top, i;   /* i : candidat en cours d'évaluation */
    int  made;
    Move out[BUDGET];
} AiJob;

void   ai_begin(AiJob *j, const Match *m, AiLevel lvl);
/* Évalue jusqu'à `budget` candidats, puis rend. TRUE quand tout est fini. */
bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget);
```

`ai_choose` devient une boucle sur `ai_step` avec un budget infini, ce qui garde tous les tests de la tâche 6 valides sans en changer une ligne. La boucle principale, elle, appelle `ai_step` avec un budget de 4 candidats par frame et continue d'animer le HUD entre deux appels. Ajouter un test qui vérifie que `ai_step` par pas de 1 donne exactement le même résultat que `ai_choose` d'une traite.

- [ ] **Step 5: Commit**

```bash
git add src/snes/main.c docs/snes-notes.md
git commit -m "✨ [snes] Wire the CPU opponent into the game loop"
```

---

### Task 12 : Le menu et l'écran de fin

**Files:**
- Modify: `src/core/view.h`, `src/core/view.c`, `tests/test_view.c`
- Modify: `tools/mktiles.py`, `data/tiles.bmp`
- Create: `src/snes/screens.h`, `src/snes/screens.c`
- Modify: `src/snes/main.c`

**Interfaces:**
- Consumes: `view_board`, `view_hud`, `render_*`, `input_*`, `cpu_level` de la tâche 11.
- Produces :

```c
#define TILE_P 19    /* nouvelle tuile, la lettre P */

/* Menu dessiné sur la zone de grille. `selected` va de 0 à 2 :
   0 = deux joueurs, 1 = contre CPU facile, 2 = contre CPU normal. */
void view_menu(int selected, u8 out[BOARD_H][BOARD_W]);

/* Bandeau de fin de partie : la couleur du vainqueur répétée au centre. */
void view_result_banner(Winner w, u8 out[HUD_W]);
```

Le menu tient en trois lignes, sur les lignes 10, 12 et 14 de la grille, à partir de la colonne 12 :

| Ligne | Contenu | Signification |
|---|---|---|
| 10 | `2P` | deux joueurs |
| 12 | `1P×1` | contre le CPU, niveau facile |
| 14 | `1P×2` | contre le CPU, niveau normal |

La ligne sélectionnée porte un `TILE_PIP_ON` en colonne 10. Le reste de la grille est vide. Aucun mot n'est nécessaire : seuls les chiffres, `P` et `×` servent, et la lettre `P` est le seul glyphe à ajouter à la planche.

- [ ] **Step 1: Ajouter le glyphe P à la planche**

Dans `tools/mktiles.py`, ajouter à `FONT` :

```python
 'P': ["####.","#...#","#...#","####.","#....","#....","#...."],
```

et à la liste des tuiles, après `glyph('X', 6)` :

```python
tiles += [glyph('P', 6)]                                    # 19
```

Puis `python3 tools/mktiles.py`.

- [ ] **Step 2: Écrire les tests des deux nouvelles vues, qui doivent échouer**

Ajouter à `tests/test_view.c` :

```c
static void test_le_menu_affiche_les_trois_modes(void)
{
    view_menu(0, grid);
    T_EQ(grid[10][12], TILE_DIGIT0 + 2);   /* 2P */
    T_EQ(grid[10][13], TILE_P);
    T_EQ(grid[12][12], TILE_DIGIT0 + 1);   /* 1P x1 */
    T_EQ(grid[12][13], TILE_P);
    T_EQ(grid[12][14], TILE_TIMES);
    T_EQ(grid[12][15], TILE_DIGIT0 + 1);
    T_EQ(grid[14][15], TILE_DIGIT0 + 2);   /* 1P x2 */
    T_EQ(grid[0][0], TILE_EMPTY);
}

static void test_le_menu_marque_la_ligne_choisie(void)
{
    view_menu(0, grid);
    T_EQ(grid[10][10], TILE_PIP_ON);
    T_EQ(grid[12][10], TILE_EMPTY);
    view_menu(2, grid);
    T_EQ(grid[10][10], TILE_EMPTY);
    T_EQ(grid[14][10], TILE_PIP_ON);
}

static void test_le_bandeau_de_fin_montre_le_vainqueur(void)
{
    view_result_banner(WINNER_P1, hud);
    T_EQ(hud[12], TILE_P1);
    T_EQ(hud[19], TILE_P1);
    T_EQ(hud[0], TILE_EMPTY);
    view_result_banner(WINNER_P2, hud);
    T_EQ(hud[12], TILE_P2);
    /* Un nul alterne les deux couleurs. */
    view_result_banner(WINNER_DRAW, hud);
    T_EQ(hud[12], TILE_P1);
    T_EQ(hud[13], TILE_P2);
}

/* à ajouter dans suite_view :
    T_RUN(test_le_menu_affiche_les_trois_modes);
    T_RUN(test_le_menu_marque_la_ligne_choisie);
    T_RUN(test_le_bandeau_de_fin_montre_le_vainqueur);
*/
```

Run: `make test`
Expected: échec de compilation, `view_menu` et `view_result_banner` non déclarées.

- [ ] **Step 3: Implémenter les deux vues**

Ajouter à `src/core/view.c` :

```c
void view_menu(int selected, u8 out[BOARD_H][BOARD_W])
{
    static const u8 rows[3] = { 10, 12, 14 };
    int x, y, i;

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            out[y][x] = TILE_EMPTY;
        }
    }
    for (i = 0; i < 3; i++) {
        u8 r = rows[i];
        out[r][10] = (u8)((i == selected) ? TILE_PIP_ON : TILE_EMPTY);
        out[r][12] = (u8)(TILE_DIGIT0 + (i == 0 ? 2 : 1));
        out[r][13] = TILE_P;
        if (i > 0) {
            out[r][14] = TILE_TIMES;
            out[r][15] = (u8)(TILE_DIGIT0 + i);   /* 1 facile, 2 normal */
        }
    }
}

void view_result_banner(Winner w, u8 out[HUD_W])
{
    int i;
    for (i = 0; i < HUD_W; i++) {
        out[i] = TILE_EMPTY;
    }
    for (i = 12; i < 20; i++) {
        if (w == WINNER_P1)      out[i] = TILE_P1;
        else if (w == WINNER_P2) out[i] = TILE_P2;
        else                     out[i] = (u8)((i & 1) ? TILE_P2 : TILE_P1);
    }
}
```

Déclarer les deux fonctions et `TILE_P` dans `src/core/view.h`.

- [ ] **Step 4: Lancer les tests et vérifier qu'ils passent**

Run: `make test`
Expected: 0 échecs.

- [ ] **Step 5: Écrire la couche écrans**

`src/snes/screens.h` :

```c
#ifndef SCREENS_H
#define SCREENS_H

#include "match.h"

/* Bloque jusqu'au choix. Rend -1 pour deux joueurs, AI_EASY ou AI_NORMAL. */
int  screen_menu(void);
/* Bloque jusqu'à une pression de START, plateau final laissé à l'écran. */
void screen_result(const Match *m);

#endif
```

`src/snes/screens.c` : deux boucles sur le même modèle que la boucle principale. Le menu déplace `selected` sur les touches haut et bas, valide sur `A` ou `START`, et transfère `view_menu` vers BG1 en laissant BG2 vide. L'écran de fin transfère le plateau final vers BG1 et `view_result_banner` vers BG2, en faisant clignoter le bandeau une alternance sur trente frames.

Dans `src/snes/main.c`, encadrer la partie :

```c
for (;;) {
    cpu_level = screen_menu();
    match_start(&m);
    input_init(&cur);
    /* ... la boucle de jeu des tâches 10 et 11, qui sort sur GS_OVER ... */
    screen_result(&m);
}
```

- [ ] **Step 6: Vérifier le parcours complet**

```bash
make rom && open build/immigration.sfc
```

Contrôler dans l'ordre :

1. Au démarrage, trois lignes au centre de l'écran : `2P`, `1P×1`, `1P×2`, la première marquée d'un disque blanc.
2. Haut et bas déplacent le disque, sans sortir des trois lignes.
3. `A` sur `2P` lance une partie à deux ; `A` sur `1P×2` lance une partie où le rouge joue seul.
4. En fin de partie, le plateau final reste affiché et le centre du bandeau clignote dans la couleur du vainqueur — les deux couleurs en alternance sur un nul.
5. `START` ramène au menu, et une nouvelle partie repart de la position de départ, sans reste de la précédente.

- [ ] **Step 7: Commit**

```bash
git add src/core/view.h src/core/view.c tests/test_view.c tools/mktiles.py data/tiles.bmp src/snes/screens.h src/snes/screens.c src/snes/main.c
git commit -m "✨ [snes] Add mode menu and result screen"
```

---

## Ce que le plan ne couvre pas

Conformément au § 11 de la spec : musique, animation de mort des cellules, éditeur de motifs, sauvegarde, plus de deux joueurs, niveaux d'IA supplémentaires, choix de la couleur par le joueur humain.

Le bip d'erreur sur pose illégale, mentionné au § 4.1 de la spec, n'a de tâche dans aucun des douze lots : il demande d'initialiser le moteur sonore de PVSnesLib, ce qui est un sujet en soi. **À traiter dans un lot ultérieur, avec le reste du son.**
