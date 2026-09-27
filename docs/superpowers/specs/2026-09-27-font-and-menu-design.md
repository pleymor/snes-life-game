# Immigration — police de lettres, titre et menu clair

Sous-projet 1 de trois, après la version 0.1.0 :

1. **police de lettres, titre et menu clair** — ce document ;
2. tutoriel qui se joue tout seul, avec des commentaires à l'écran ;
3. musique et effets sonores libres de droit.

Les sous-projets 2 et 3 auront chacun leur spec. Celui-ci pose la base qu'ils réutilisent : une police de lettres et une fonction qui écrit du texte en tuiles.

## 1. But

Un joueur qui allume la console comprend les modes sans lire le README. Aujourd'hui le menu affiche `2P`, `1P×1` et `1P×2`, et le jeu n'a pas d'écran titre.

Critères de réussite :

- le menu affiche le titre **IMMIGRATION** en grand, puis trois modes en toutes lettres ;
- la navigation, la validation et les modes lancés sont inchangés ;
- tout le texte passe par le même jeu de tuiles que le reste de l'écran ;
- les textes sont en anglais, en majuscules, sans accent.

## 2. Ce que voit le joueur

```
ligne  3-4      I M M I G R A T I O N          (lettres ×2, colonnes 5 à 26)

ligne 10    ●  2 PLAYERS                       (disque colonne 8, texte colonne 10)
ligne 12       VS CPU  EASY
ligne 14       VS CPU  HARD
```

- Le disque (`TILE_PIP_ON`) marque la ligne choisie ; les autres lignes n'ont rien en colonne 8.
- Haut et bas déplacent le disque sans sortir des trois lignes ; A ou START valident.
- `2 PLAYERS` lance une partie à deux, `VS CPU  EASY` une partie contre `AI_EASY`, `VS CPU  HARD` une partie contre `AI_NORMAL`. « HARD » est un libellé : le niveau d'IA et son nom dans le code ne changent pas.
- La ligne 24 (le bandeau) reste vide sur le menu, comme aujourd'hui.
- L'entrée `HOW TO PLAY` n'est pas ajoutée ici : elle arrive avec le tutoriel (sous-projet 2), pour qu'aucune entrée ne mène à rien.

## 3. La planche de tuiles

`tools/mktiles.py` passe de 32 à **128 emplacements** (16 × 8 tuiles, soit 128 × 64 pixels). Les indices 0 à 19 ne bougent pas : le plateau, le bandeau et l'écran de fin gardent leurs tuiles.

| Indices | Contenu | Constante |
|---|---|---|
| 0 à 19 | inchangés (vide, portée, cellules, chiffres, pastilles, `R`, `/`, `×`, `P`) | inchangées |
| 20 à 45 | lettres `A` à `Z`, blanc, police 5 × 7 des chiffres | `TILE_LETTER_A` (20) ; la lettre `c` vaut `TILE_LETTER_A + (c - 'A')` |
| 46 à 51 | `-` `.` `!` `?` `:` `'` | `TILE_DASH` … `TILE_APOS` |
| 52 à 83 | titre : `I M G R A T O N` agrandies ×2, 4 tuiles par lettre (haut-gauche, haut-droite, bas-gauche, bas-droite) | `TILE_BIG_BASE` (52) |
| 84 à 127 | vides, réservés au tutoriel | — |

- Les lettres `P` et `R` existent déjà (19 et 16) pour le bandeau. Les lettres 20 à 45 en sont des doublons volontaires : l'alphabet reste contigu et `view_text` n'a pas de cas particulier.
- Une lettre agrandie est la lettre 5 × 7 dont chaque pixel devient un carré de 2 × 2, posée dans un bloc de 16 × 16.
- Coût : 128 tuiles de 32 octets, soit 4 Ko en ROM et en VRAM. Aucun octet de RAM en plus. Dans la carte VRAM de `src/snes/render.c` (adresses en mots), les tuiles de fond occupent alors 0x4000 à 0x47FF, sans toucher les tilemaps (0x0000 à 0x07FF) ni les sprites (0x2000).

## 4. Le cœur (`src/core/view.c`, `view.h`)

Trois fonctions pures, sans dépendance à la console :

```c
/* Écrit la chaîne `s` en indices de tuiles dans out[0..width-1].
   'A'..'Z' → lettres, '0'..'9' → chiffres, les six signes → leurs tuiles,
   tout le reste (espace compris) → TILE_EMPTY. Les cases au-delà de la
   chaîne sont mises à TILE_EMPTY ; une chaîne plus longue est tronquée.
   Rend le nombre de caractères écrits. */
int  view_text(const char *s, u8 *out, int width);

/* Pose IMMIGRATION en grand aux lignes 3-4, colonnes 5 à 26 de `out`. */
void view_title(u8 out[BOARD_H][BOARD_W]);

/* Vide la grille, pose le titre, puis les trois modes (§ 2).
   `selected` va de 0 à 2 ; hors de cet intervalle, aucun disque. */
void view_menu(int selected, u8 out[BOARD_H][BOARD_W]);
```

- Les libellés et la table des lettres du titre sont des tableaux `static const` à une dimension et de portée fichier, pour rester en ROM.
- `view_text` ne dépend ni de `ctype.h` ni d'aucun autre en-tête que ceux du cœur.

## 5. La console (`src/snes/screens.c`)

`screen_menu` garde sa boucle et ses bornes (0 à 2). Il appelle le nouveau `view_menu` et transmet la grille à `render_board_from_grid`, comme aujourd'hui. Rien d'autre ne change côté console.

## 6. Tests et vérification

Tests hôtes écrits d'abord, dans `tests/test_view.c` :

- `view_text` : lettres, chiffres, signes, caractère inconnu, espace, troncature, remplissage des cases restantes, valeur rendue ;
- `view_title` : les 44 tuiles attendues aux lignes 3-4, colonnes 5 à 26, et rien autour ;
- `view_menu` : disque sur la ligne choisie pour 0, 1 et 2, aucun disque pour une valeur hors bornes, libellés exacts aux positions du § 2, titre présent, reste de la grille vide ;
- les tests actuels de `view_menu` sont réécrits pour la nouvelle mise en page.

Sur la console :

- `make test`, `make rom`, `make rom-script`, `make rom-measure` passent, et le garde-fou RAM aussi ;
- captures de la ROM scriptée : le menu au démarrage, puis après un appui bas ; lecture du titre, des libellés et de la position du disque ;
- le script de la ROM de rejeu est recalé seulement si les images changent.

## 7. Documentation

- README : noms des modes et capture du menu mis à jour.
- `docs/snes-notes.md` : la nouvelle taille de la planche.

## 8. Hors du périmètre

- le tutoriel et l'entrée `HOW TO PLAY` (sous-projet 2) ;
- le son (sous-projet 3) ;
- tout changement du HUD, de l'écran de fin ou des niveaux d'IA.
