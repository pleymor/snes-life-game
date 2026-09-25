# Réglage de l'équilibrage (tâche 7)

Mesures produites par `tools/sim.c` (IA `AI_NORMAL` contre elle-même,
`./build/sim 200 1`), utilisées pour figer `src/core/config.h`.

## Un constat préalable : l'IA normale est déterministe

`ai.h` le dit : au niveau `AI_NORMAL`, `rng` est ignoré — le choix est
purement déterministe (meilleur score, départage par ordre de balayage). Le
plateau de départ est symétrique (spec § 2.4). Conséquence directe pour le
simulateur : pour un jeu de réglages donné, **les 200 parties d'un même run
sont strictement identiques** (seule la graine change, or `AI_NORMAL` ne la
lit jamais). Les pourcentages ci-dessous sont donc chacun 0 % ou 100 %, pas
des moyennes statistiques — chaque ligne du tableau est une seule partie
rejouée 200 fois. C'est attendu : le simulateur reste utile pour comparer
des réglages entre eux, il ne produit simplement aucune variance intra-run
tant que seule `AI_NORMAL` est utilisée.

## Mesure de départ (réglages avant tâche 7)

```
$ make sim && ./build/sim 200 1
parties        200
réglages       BUDGET=3 RANGE=2 RAMPUP=16 TICKS=2 CAP=40
élimination    0 (0%)
plafond        200 (100%)
victoires P1   0 (0%)
victoires P2   200 (100%)
nuls           0
round moyen    40.0
pop. gagnante  89.0
```
Sortie complète, sans plantage, en 2.5 s pour 200 parties (bien sous les dix
secondes attendues à l'étape 3).

## Balayage (étape 4)

Chaque ligne fait varier un seul réglage, les autres restant à leur valeur
de départ (`BUDGET=3 RANGE=2 RAMPUP=16`). `make -s sim` ne recompile pas
automatiquement sur un changement de `config.h` (le `Makefile` ne le liste
pas en prérequis) : chaque mesure ci-dessous a été prise après `rm -f
build/sim` pour forcer la recompilation — sans quoi le balayage aurait
silencieusement mesuré 200 fois le même binaire.

### RAMPUP_ROUND ∈ {8, 12, 16, 20, 40}

| RAMPUP | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------:|------------:|---:|---:|------:|------------:|--------------:|
| 8      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 91  |
| 12     | 0 %         | 0 %  | 100 % | 0 | 40.0 | 97  |
| 16     | 0 %         | 0 %  | 100 % | 0 | 40.0 | 89  |
| 20     | 0 %         | 100 %| 0 %   | 0 | 40.0 | 70  |
| 40     | 0 %         | 0 %  | 100 % | 0 | 40.0 | 112 |

### BUDGET ∈ {2, 3, 4}

| BUDGET | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------:|------------:|---:|---:|------:|------------:|--------------:|
| 2      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 55 |
| 3      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 89 |
| 4      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 87 |

### RANGE_RADIUS ∈ {1, 2, 3}

| RANGE_RADIUS | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------------:|------------:|---:|---:|------:|------------:|--------------:|
| 1            | 0 %         | 0 %  | 100 % | 0 | 40.0 | 161 |
| 2            | 0 %         | 0 %  | 100 % | 0 | 40.0 | 89  |
| 3            | 0 %         | 0 %  | 100 % | 0 | 40.0 | 74  |

## Application des critères de choix (étape 5)

1. **Élimination > 50 %** — **aucun des onze réglages balayés ne l'atteint,
   dans aucune des trois dimensions.** Le taux d'élimination mesuré est de
   0 % partout : la partie va systématiquement au plafond de rounds (40),
   jamais à l'extinction d'un camp. C'est **un écart à la spec, signalé ici
   explicitement** (voir « Écart à la spec » ci-dessous), et non un motif
   pour sortir des plages demandées par le brief (RAMPUP ∈ {8,12,16,20,40},
   BUDGET ∈ {2,3,4}, RANGE_RADIUS ∈ {1,2,3}) : la consigne du contrôleur est
   d'arrêter le meilleur réglage mesuré et de signaler l'écart, pas de
   continuer à balayer.
2. **Écart P1/P2 sous 60/40** — également non atteint nulle part : chaque
   configuration est gagnée à 100 % par un seul camp (P2, sauf à
   RAMPUP_ROUND=20 où c'est P1). Comme expliqué plus haut, `AI_NORMAL` est
   déterministe, donc chaque réglage ne produit qu'une seule trajectoire de
   partie — 100 % ou 0 %, jamais entre les deux.
3. **Round moyen entre 15 et 30** — non atteint non plus : toutes les
   parties mesurées vont jusqu'au plafond (round moyen 40.0 partout).

**Les trois critères sont donc exactement à égalité sur les onze réglages
mesurés** : aucun ne l'emporte sur un autre du point de vue du brief. Faute
de signal de balance pour départager, la décision retombe sur le critère
de coût CPU (le 65816 tourne à 3.58 MHz ; la tâche 11 mesure le coût par
tour) : à résultat égal, préférer le réglage le moins cher.

- **BUDGET** : `2`, `3` et `4` sont à égalité stricte sur les trois
  critères → retenu **BUDGET = 2** (le moins cher : `ai_choose` boucle
  `BUDGET` fois sur un balayage complet du plateau plus l'évaluation des
  meilleurs candidats, donc le coût par tour croît directement avec
  `BUDGET`). Cela a nécessité de recalculer deux tests câblés sur trois
  poses (voir « Tests touchés » ci-dessous) et de vérifier la mise en page
  du HUD (voir « HUD » ci-dessous).
- **RANGE_RADIUS** : `1`, `2` et `3` sont également à égalité stricte sur
  les trois critères, et un réglage plus petit serait moins cher
  (`rules_in_range` coûte O(RANGE_RADIUS²) par case candidate scannée dans
  `collect()`, qui balaie déjà tout le plateau). **Retenu néanmoins
  RANGE_RADIUS = 2, inchangé**, par prudence plutôt que par mesure : le
  passer à 1 casse trois assertions de `tests/test_rules.c`
  (`test_la_portee_va_jusqua_deux_cases`, dont le nom même encode la
  distance 2) qui ne font partie d'aucune liste d'exception du brief ni des
  tests de portée de la tâche 4 — les recalculer aurait été un changement
  non prévu par le brief pour un gain non mesurable (égalité stricte sur
  les trois critères). Ce point est un arbitrage delta-coût non pris ; la
  tâche 11, qui mesure le coût réel par tour sur le 65816, est le bon
  endroit pour rouvrir la question si le budget CPU s'avère trop juste.
- **RAMPUP_ROUND** : les cinq valeurs balayées sont à égalité stricte elles
  aussi, et ce réglage n'a pas d'impact CPU direct (il ne fait que décaler
  le round à partir duquel `TICKS_AFTER_RAMPUP` s'applique, pas le nombre
  de candidats évalués par tour). **Retenu RAMPUP_ROUND = 16, inchangé** :
  c'est la valeur de départ de la spec, au deux cinquièmes de `ROUND_CAP`,
  et rien dans les mesures ne justifie de s'en écarter.
- **TICKS_AFTER_RAMPUP** et **ROUND_CAP** n'étaient pas dans le balayage
  demandé par le brief ; inchangés (2 et 40).

### Écart à la spec

Sur les onze réglages mesurés — dont les valeurs de départ de la spec —
**aucun n'atteint jamais l'élimination d'un camp** : la partie va toujours
au plafond de 40 rounds, tranchée par population. C'est un écart réel à
l'intention de la spec (« l'élimination arrive-t-elle vraiment ? »). Deux
causes structurelles, indépendantes des réglages balayés :

- `AI_NORMAL` est purement gloutonne et déterministe (§ ci-dessus) : elle
  ne prend jamais de risque susceptible de provoquer une extinction chez
  l'adversaire ou chez elle-même, et joue toujours la même partie pour un
  jeu de réglages donné, donc aucune moyenne sur 200 parties ne peut
  déplacer ce résultat.
- Le bloc de départ de chaque camp (spec § 2.4) est une nature morte
  immortelle tant qu'on ne la perturbe pas directement : elle sert de
  garantie de survie qu'aucun des réglages testés (portée, budget, rampup)
  ne suffit à percer en 40 rounds face à un adversaire tout aussi
  défensif.

Ce n'est pas un problème que la tâche 7 peut corriger en réglant
`config.h` seul — la cause est dans le comportement de l'IA et/ou la
position de départ, hors périmètre de cette tâche. À arbitrer par une
tâche ultérieure (revoir l'agressivité d'`AI_NORMAL`, ou accepter le
plafond de rounds comme mode de décision principal en pratique, avec
l'élimination comme cas rare plutôt que fréquent).

### Tests touchés par BUDGET = 2

- `tests/test_match.c::test_trois_poses_puis_le_budget_est_epuise` — le nom
  est resté tel quel (nommé explicitement dans le brief comme test à
  recalculer) mais son corps ne fait plus que deux poses réussies puis une
  troisième refusée, plutôt que trois puis une quatrième.
- `tests/test_view.c::test_les_poses_restantes_se_vident` — les pastilles
  ne vont plus que jusqu'à l'index 7 (P1) et 24 (P2) ; l'ancien index 8
  (resp. 25) n'est plus une pastille et reste `TILE_EMPTY`.

### HUD (BUDGET = 2, à l'attention de la tâche 9)

`pips()` dans `src/core/view.c` boucle déjà sur `BUDGET`, elle n'a pas
changé. Avec `BUDGET = 2` :
- pastilles P1 : colonnes 6 et 7 (au lieu de 6..8) ; colonne 8 reste
  `TILE_EMPTY` (espace, pas de tuile parasite) puisque `view_hud` efface
  tout le bandeau à `TILE_EMPTY` avant de le remplir.
- pastilles P2 : colonnes 23 et 24 (au lieu de 23..25) ; colonne 25 reste
  `TILE_EMPTY`.
- aucun chevauchement avec les champs voisins : `R` (round) reste en
  colonne 12, le compte de population P2 démarre en colonne 27. La marge
  entre les pastilles et ces champs est simplement un peu plus large
  qu'avant (colonnes 8-11 et 25-26 vides côté pastilles au lieu de 9-11 et
  26).
- **la tâche 9, qui rend ce HUD à l'écran, verra donc deux pastilles par
  joueur au lieu de trois — c'est le réglage attendu, pas un bug de
  rendu.**

## Réglages finaux (`src/core/config.h`)

| Réglage             | Avant | Après | Raison |
|----------------------|------:|------:|--------|
| `BUDGET`             | 3     | 2     | Égalité stricte avec 3 et 4 sur les trois critères ; le moins cher des trois. |
| `RANGE_RADIUS`       | 2     | 2     | Égalité stricte avec 1 et 3 ; conservé pour ne pas casser des tests de portée hors périmètre du brief pour un gain non mesurable. |
| `RAMPUP_ROUND`       | 16    | 16    | Égalité stricte sur les cinq valeurs testées ; pas de levier CPU ; valeur de départ de la spec conservée. |
| `TICKS_AFTER_RAMPUP` | 2     | 2     | Hors balayage du brief. |
| `ROUND_CAP`          | 40    | 40    | Hors balayage du brief. |

## Vérification (étape 6)

```
$ make test
1239 vérifications, 0 échecs
```

## Mesure finale

```
$ time ./build/sim 200 1
parties        200
réglages       BUDGET=2 RANGE=2 RAMPUP=16 TICKS=2 CAP=40
élimination    0 (0%)
plafond        200 (100%)
victoires P1   0 (0%)
victoires P2   200 (100%)
nuls           0
round moyen    40.0
pop. gagnante  55.0

real    0m1.630s (1.45s user)
```
Plus rapide que la mesure de départ à `BUDGET=3` (2.5 s), cohérent avec le
choix du réglage le moins cher à résultat égal.
