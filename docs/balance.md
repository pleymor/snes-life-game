# Réglage de l'équilibrage (tâche 7)

Mesures produites par `tools/sim.c` (`./build/sim <parties> <graine> [niveau]`),
utilisées pour figer `src/core/config.h`.

## AI_NORMAL contre elle-même : des mesures dégénérées

`ai.h` le dit : au niveau `AI_NORMAL`, `rng` est ignoré — le choix est
purement déterministe (meilleur score, départage par ordre de balayage). Le
plateau de départ est symétrique (spec § 2.4). Conséquence directe pour le
simulateur : pour un jeu de réglages donné, **les 200 parties `AI_NORMAL`
contre `AI_NORMAL` d'un même run sont strictement identiques** (seule la
graine change, or `AI_NORMAL` ne la lit jamais). **Les tableaux ci-dessous
sont donc dégénérés : chaque ligne est une seule partie rejouée 200 fois,
pas une moyenne statistique.** Ils restent utiles pour comparer des
réglages entre eux (sensibles au round moyen, à la marge de population...)
mais ne peuvent pas servir de mesure de variance, d'où l'ajout des modes
`easy` et `mixed` à `tools/sim.c` pour obtenir de vraies mesures
statistiques (§ suivante).

### Mesure de départ (réglages de la spec, mode `normal`)

```
$ make sim && ./build/sim 200 1
parties        200
mode           normal
réglages       BUDGET=3 RANGE=2 RAMPUP=16 TICKS=2 CAP=40
élimination    0 (0%)
plafond        200 (100%)
victoires P1   0 (0%)
victoires P2   200 (100%)
nuls           0
round moyen    40.0
pop. gagnante  89.0
```
Sortie complète, sans plantage, en 2.2 s pour 200 parties (bien sous les
dix secondes attendues à l'étape 3).

### Balayage `normal` (dégénéré, une partie par ligne)

| RAMPUP | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------:|------------:|---:|---:|------:|------------:|--------------:|
| 8      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 91  |
| 12     | 0 %         | 0 %  | 100 % | 0 | 40.0 | 97  |
| 16     | 0 %         | 0 %  | 100 % | 0 | 40.0 | 89  |
| 20     | 0 %         | 100 %| 0 %   | 0 | 40.0 | 70  |
| 40     | 0 %         | 0 %  | 100 % | 0 | 40.0 | 112 |

| BUDGET | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------:|------------:|---:|---:|------:|------------:|--------------:|
| 2      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 55 |
| 3      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 89 |
| 4      | 0 %         | 0 %  | 100 % | 0 | 40.0 | 87 |

| RANGE_RADIUS | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------------:|------------:|---:|---:|------:|------------:|--------------:|
| 1            | 0 %         | 0 %  | 100 % | 0 | 40.0 | 161 |
| 2            | 0 %         | 0 %  | 100 % | 0 | 40.0 | 89  |
| 3            | 0 %         | 0 %  | 100 % | 0 | 40.0 | 74  |

Ces onze réglages sont exactement à égalité entre eux sur les trois
critères du brief (0 % d'élimination partout, un split 100/0 partout — P2
sauf à RAMPUP=20 — et un round moyen toujours à 40.0). Le mode `normal`
seul ne peut donc pas trancher entre réglages : il montre juste qu'une
seule partie de référence, jouée par une IA gloutonne déterministe des
deux côtés, ne suffit à rien départager. D'où les modes `easy` et `mixed`.

## Mode `easy` : de vraies 200 parties, variance réelle

`AI_EASY` lit `rng` (tirage parmi les trois meilleurs coups, § `ai.c`),
donc `easy` (les deux camps en `AI_EASY`) produit 200 parties réellement
différentes par run, avec de la variance mesurable — c'est la mesure
statistique que le brief attendait.

### Référence aux réglages de la spec, deux graines (stabilité)

```
$ ./build/sim 200 1 easy
élimination    1 (0%)
plafond        199 (100%)
victoires P1   66 (33%)
victoires P2   132 (66%)
nuls           2
round moyen    39.9
pop. gagnante  59.5

$ ./build/sim 200 1000 easy
élimination    0 (0%)
plafond        200 (100%)
victoires P1   67 (34%)
victoires P2   128 (64%)
nuls           5
round moyen    40.0
pop. gagnante  57.2
```
Les deux graines s'accordent à quelques points de pourcentage près (33-34 %
P1, 64-66 % P2, 0-1 % élimination, round moyen ~40.0, nuls 2-5) : la mesure
est stable d'une graine à l'autre.

### Balayage RAMPUP_ROUND ∈ {8,12,16,20,40} (BUDGET=3, RANGE=2, graine 1)

| RAMPUP | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------:|------------:|---:|---:|------:|------------:|--------------:|
| 8      | 1 %         | 36 % | 63 % | 2 | 40.0 | 56.4 |
| 12     | 0 %         | 32 % | 65 % | 6 | 40.0 | 57.9 |
| **16** | 0 %         | 33 % | 66 % | 2 | 39.9 | 59.5 |
| 20     | 0 %         | 39 % | 60 % | 1 | 40.0 | 57.6 |
| 40     | 0 %         | 32 % | 68 % | 1 | 40.0 | 57.9 |

### Balayage BUDGET ∈ {2,3,4} (RANGE=2, RAMPUP=16, graine 1)

| BUDGET | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------:|------------:|---:|---:|------:|------------:|--------------:|
| 2      | 0 %         | 39 % | 59 % | 4 | 40.0 | 56.9 |
| **3**  | 0 %         | 33 % | 66 % | 2 | 39.9 | 59.5 |
| 4      | 0 %         | 32 % | 68 % | 1 | 40.0 | 58.0 |

### Balayage RANGE_RADIUS ∈ {1,2,3} (BUDGET=3, RAMPUP=16, graine 1)

| RANGE_RADIUS | élimination | P1 | P2 | nuls | round moyen | pop. gagnante |
|-------------:|------------:|---:|---:|------:|------------:|--------------:|
| 1            | 0 %         | 40 % | 56 % | 6 | 40.0 | 59.3 |
| **2**        | 0 %         | 33 % | 66 % | 2 | 39.9 | 59.5 |
| 3            | 2 %         | 36 % | 63 % | 1 | 39.9 | 52.5 |

En `easy`, l'élimination reste sous 2 % partout, jamais dans les
environs de 50 %. Le split P1/P2 varie d'un réglage à l'autre : trois
lignes sur onze restent **dans** la bande 60/40 demandée par le critère 2
(BUDGET=2 : 39/59 ; RANGE_RADIUS=1 : 40/56 ; et, à la limite, RAMPUP=20 :
39/60), les huit autres — dont les trois réglages de spec (RAMPUP=16,
BUDGET=3, RANGE_RADIUS=2, tous à 33/66) — restent **hors** de la bande
(63 à 68 % pour le côté majoritaire). Cette différence ne change toutefois
rien à la décision : le critère 1 (élimination > 50 %) est évalué en
premier et aucun réglage n'y satisfait, donc le critère 2 n'est jamais
consulté pour décider. Les écarts d'un réglage à l'autre sur le split
(quelques points de pourcentage, hormis peut-être l'effet visible sur
RANGE_RADIUS) sont du même ordre que l'écart entre les deux graines de la
mesure de stabilité ci-dessus, donc en grande partie du bruit
d'échantillonnage plutôt qu'un vrai effet de réglage — sauf pour le point
suivant, qui lui est net et mérite d'être noté séparément.

**Aux réglages de spec eux-mêmes, P2 gagne nettement plus souvent que P1**
(66 % contre 33 % en `easy`, stable sur les deux graines testées ; 52 %
contre 48 % seulement en `mixed`, où le trait alterne). Ce déséquilibre
existe indépendamment de la question de l'élimination et fait partie de la
question ouverte pour un humain (§ suivante) : est-ce l'IA (l'ordre de
balayage des candidats, identique pour les deux camps, favorise
peut-être structurellement le second à jouer) ou la position de départ qui
avantage P2 ? Ce n'est pas tranché ici.

## Mode `mixed` : `AI_NORMAL` contre `AI_EASY`, sièges alternés

`mixed` fait jouer `AI_NORMAL` contre `AI_EASY`, en échangeant les sièges
P1/P2 une partie sur deux (paire → NORMAL en P1, impaire → NORMAL en P2)
pour neutraliser l'avantage du trait dans la mesure du taux de victoire de
NORMAL. Chaque partie garde sa propre graine `seed + i` comme les autres
modes.

### Balayage RAMPUP_ROUND ∈ {8,12,16,20,40} (BUDGET=3, RANGE=2, graine 1)

| RAMPUP | élimination | P1 | P2 | round moyen | pop. gagnante | victoires NORMAL |
|-------:|------------:|---:|---:|------------:|---------------:|------------------:|
| 8      | **10 %**    | 48 % | 52 % | 39.4 | 93.9  | 98 % |
| 12     | 8 %         | 46 % | 54 % | 39.6 | 94.7  | 96 % |
| **16** | 4 %         | 48 % | 52 % | 39.8 | 93.5  | 98 % |
| 20     | 3 %         | 47 % | 53 % | 39.9 | 95.0  | 97 % |
| 40     | 1 %         | 49 % | 50 % | 40.0 | 103.9 | 99 % |

Deux observations utiles :
- **`AI_NORMAL` écrase `AI_EASY`** quel que soit le siège ou `RAMPUP_ROUND`
  (96-99 % de victoires NORMAL) : le glouton déterministe est nettement
  plus fort que le tirage aléatoire parmi le top 3, ce qui confirme que le
  mode `normal` (§ ci-dessus) n'était pas une anomalie de l'IA mais son
  comportement attendu.
- **`RAMPUP_ROUND` a un effet net sur l'élimination ici**, contrairement
  aux balayages `easy` et `normal` : un emballement plus précoce (8) fait
  plus que doubler le taux d'élimination par rapport à la spec (10 %
  contre 4 %), et un emballement tardif (40, qui ne s'active quasiment
  jamais avant le plafond) le fait retomber à 1 %. C'est cohérent avec
  l'intention de la spec (l'emballement est censé précipiter une décision)
  mais reste très loin de 50 % même dans le meilleur cas mesuré.

## Application des critères du brief

**Règle de décision retenue** : ne s'écarter d'une valeur de la spec que
si les mesures favorisent clairement une autre valeur sur le critère 1
(taux d'élimination > 50 %), puis le critère 2 (split sous 60/40), puis le
critère 3 (round moyen entre 15 et 30). Si rien n'atteint 50 %
d'élimination sur aucun réglage mesuré, garder les valeurs de la spec.

Sur les **dix-sept mesures statistiquement valables** (onze `easy` + cinq
`mixed`, plus la mesure de stabilité à deux graines), **le meilleur taux
d'élimination observé est 10 %** (`mixed`, RAMPUP_ROUND=8) — loin des
50 % du critère 1. Aucun réglage, dans aucun mode, ne s'en approche.
**Conclusion : les trois réglages restent à leur valeur de spec.**

| Réglage | Spec | Retenu | Raison |
|---|---:|---:|---|
| `BUDGET` | 3 | **3** | Aucune valeur testée (2, 3, 4) n'approche 50 % d'élimination en `easy` ; les écarts entre elles sont de l'ordre du bruit d'échantillonnage (comparé aux deux graines de la mesure de stabilité). Rien ne favorise clairement 2 ou 4 sur le critère 1 : la spec l'emporte. |
| `RANGE_RADIUS` | 2 | **2** | Même constat : 1, 2 et 3 restent tous sous 2 % d'élimination en `easy`. |
| `RAMPUP_ROUND` | 16 | **16** | C'est le réglage où un effet réel existe (`mixed` : 1 % à 40, 10 % à 8), mais même le meilleur cas (8) reste à 10 %, très loin du seuil de 50 % qui déclencherait un changement. La spec l'emporte par défaut de la règle de décision. |
| `TICKS_AFTER_RAMPUP`, `ROUND_CAP` | 2, 40 | inchangés | Hors balayage du brief. |

`src/core/config.h` reste donc identique à son état d'avant la tâche 7,
à l'exception du commentaire d'en-tête (qui documente que la tâche 7 a
mesuré et confirmé ces valeurs plutôt que de les avoir choisies au
hasard). Aucun test câblé sur `BUDGET`, `RANGE_RADIUS` ou leurs
coordonnées dérivées n'a donc eu besoin d'être retouché : `make test`
passe sans aucune modification des suites de tests.

## Écart à la spec (question ouverte, non tranchée ici)

Même dans la meilleure configuration mesurée, l'élimination reste rare
(10 % au mieux, en `mixed` avec un emballement précoce). C'est un écart
réel à l'intention de la spec (« l'élimination arrive-t-elle vraiment ? »)
que cette tâche ne peut pas corriger en réglant seulement `config.h` — les
leviers disponibles (portée, budget, timing de l'emballement) ont chacun
été mesurés sur toute leur plage prévue par le brief sans jamais
s'approcher de 50 %. Trois points, **non tranchés, à arbitrer par un
humain** :

- **Les blocs 2×2 de départ (spec § 2.4) sont des natures mortes
  immortelles** tant qu'on ne les perturbe pas directement : ils
  fournissent à chaque camp un noyau de survie qu'aucun réglage testé ne
  suffit à percer en 40 rounds face à un adversaire qui défend le sien
  aussi bien.
- **Le style des deux IA est conservateur.** `AI_NORMAL` ne prend jamais
  de risque (elle maximise un gain net évalué localement, sans jamais
  chercher à provoquer une extinction adverse comme objectif explicite) ;
  `AI_EASY` introduit de la variance mais reste tirée du même pool de
  candidats pré-filtrés par la même heuristique gloutonne — aucune des
  deux ne joue jamais un coup délibérément agressif ou sacrificiel qui
  viserait l'élimination plutôt que l'avantage local.
- **P2 gagne nettement plus souvent que P1 aux réglages de spec**, en
  dehors même de la question de l'élimination : 66 % contre 33 % en
  `easy` (stable sur deux graines), hors de la bande 60/40 du critère 2 —
  voir le détail par réglage plus haut. Le critère 1 n'étant satisfait par
  aucun réglage, ce déséquilibre n'a pas pesé sur la décision de cette
  tâche, mais il reste réel et pourrait valoir la peine d'être creusé pour
  lui-même (biais de l'IA en faveur du second joueur ? de la position de
  départ ?).

Si l'élimination doit devenir le mode de décision courant plutôt que rare,
la piste la plus prometteuse d'après ces mesures est `RAMPUP_ROUND` (seul
réglage à montrer un effet net), combinée à une révision du comportement
de l'IA ou de la position de départ — hors périmètre d'une tâche de
réglage de `config.h`.

## Vérification

```
$ make test
1327 vérifications, 0 échecs
```

```
$ time ./build/sim 200 1
mode           normal
réglages       BUDGET=3 RANGE=2 RAMPUP=16 TICKS=2 CAP=40
...
real    0m2.24s (2.17s user)
```
