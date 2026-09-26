#include <snes.h>
#include "input.h"

#define REPEAT_FIRST 15   /* frames avant la première répétition */
#define REPEAT_NEXT   4   /* puis une case toutes les 4 frames */

static unsigned short prev;

#ifdef INPUT_SCRIPT
/* Rejeu scripté pour la vérification headless (docs/snes-notes.md §2) :
   remplace padsCurrent(0) par la relecture d'une table figée de (indice
   d'appel, masque de touches). Rien de ce bloc ne compile dans la ROM par
   défaut : CFLAGS ne reçoit -DINPUT_SCRIPT que pour la cible `rom-script`
   du Makefile racine ; `make rom` ignore entièrement ce bloc.

   Chaque entrée tient son masque jusqu'à l'entrée suivante — une fonction
   en escalier de l'indice d'appel de input_edges() (le nombre d'appels
   déjà effectués, `script_call`). Depuis la tâche 12, cet indice n'est
   plus propre à une partie : screen_menu() et screen_result()
   (src/snes/screens.c) appellent input_edges() elles aussi, au même
   rythme qu'input_update() (une fois par itération de leur boucle, donc
   une fois par frame), avant même qu'une partie n'existe et après qu'elle
   se termine. La table ci-dessous est donc une seule chronologie continue
   qui traverse menu -> partie -> écran de fin -> menu suivant -> partie
   suivante, jamais rembobinée (input_reset(), pas input_init(), la pose
   une seule fois — voir main.c).

   L'indice n'avance que lorsqu'input_edges() est réellement appelée :
   pendant GS_RESOLVE (main.c) et pendant que le CPU réfléchit (tour de
   l'IA, aucun input humain lu ce tour-là), aucun appel n'a lieu et
   l'indice reste bloqué à sa valeur, alors que des images réelles
   continuent de s'écouler.

   Même pendant le menu, l'écran de fin et le tour humain — où un appel a
   lieu à chaque image, sans aucune pause de ce genre — l'indice de la
   table ne vaut *pas* l'image réelle : chaque case, pose ou tour qui
   change l'état visible (sélection du menu, tour, round...) redéclenche
   un rafraîchissement de tuiles (`render_board_from_grid()`/
   `render_hud_now()`) dont le calcul, sur ce CPU, prend lui-même plusieurs
   dizaines à plusieurs centaines d'images réelles avant d'atteindre le
   `WaitForVBlank()` de cette même itération (docs/snes-notes.md § 8 :
   c'est le même phénomène qui rendait le rafraîchissement du plateau si
   coûteux). Un même écart d'indices entre deux entrées de la table peut
   donc correspondre à des durées réelles très différentes selon que
   l'action franchie déclenche ou non un tel rafraîchissement. La
   correspondance exacte indice -> image réelle n'est donc pas déduite
   d'une formule : chaque image citée dans les captures du rapport de
   tâche 12 a été retrouvée par capture/bisection directe sur la ROM
   scriptée, pas calculée à l'avance. Pour limiter l'effet sur la
   navigation du menu (la partie la plus sensible : ses états
   intermédiaires ne durent que le temps d'un rafraîchissement), chaque
   appui y est précédé d'une pause large et volontairement généreuse (150
   appels sans touche, eux-mêmes bon marché car sans rafraîchissement) :
   l'état qui suit chaque appui reste donc affiché largement assez
   longtemps pour être capturé sans viser une image précise.

   RetroArch affiche son propre bandeau « contenu chargé » pendant les
   ~300-350 premières images (docs/snes-notes.md § 2) : une capture prise
   avant ne montre rien d'exploitable. La table démarre donc par 400
   images d'attente (aucune touche).

   Résumé (indices de la table, pas des images réelles — voir ci-dessus) :
     - indice      400  : un appui haut, sans effet (le disque est déjà sur
       la première ligne, `2P`) ;
     - jusqu'à l'indice ~858 : trois appuis bas espacés (les deux premiers
       déplacent le disque vers `1P×1` puis `1P×2`, le troisième est
       absorbé : la brief interdit de sortir des trois lignes) ;
     - jusqu'à l'indice ~1314 : trois appuis haut espacés (les deux
       premiers ramènent vers `1P×1` puis `2P`, le troisième absorbé) ;
     - indice ~1466 : A sur `2P` (`selected == 0`) : lance une partie à
       deux ;
     - jusqu'à l'indice ~1786 : partie 1 jouée au pas de course (aucune
       pose, chaque tour clos par START) sur ses 40 rounds — sans pose, la
       partie va jusqu'au plafond ROUND_CAP et se termine par un nul
       (vérifié par simulation hôte : voir tools/sim.c pour le mécanisme,
       la partie sans pose y donne WINNER_DRAW au round 40) ;
     - indice ~1788 : START, retour au menu (le bandeau de fin, qui
       clignote toutes les 30 images réelles dans screen_result(), est
       capturé à la fois affiché et éteint pour prouver le clignotement) ;
     - jusqu'à l'indice ~2244 : deux appuis bas (`1P×1` puis `1P×2`), puis
       A : lance une seconde partie, contre le CPU niveau normal —
       plateau de départ intact, aucun reste de la partie précédente ;
     - indice ~2396 : START, bleu (P1, humain) passe son tour sans rien
       poser ; le tour suivant est celui du rouge (P2), entièrement piloté
       par l'IA — aucune touche de la table ne le concerne, la partie 2
       n'a pas besoin d'aller à son terme pour le montrer. */
typedef struct {
    unsigned int   frame;
    unsigned short pad;
} ScriptStep;

static const ScriptStep script[] = {
    {     0, 0 },
    {   400, KEY_UP },
    {   401, 0 },
    {   552, KEY_DOWN },
    {   553, 0 },
    {   704, KEY_DOWN },
    {   705, 0 },
    {   856, KEY_DOWN },
    {   857, 0 },
    {  1008, KEY_UP },
    {  1009, 0 },
    {  1160, KEY_UP },
    {  1161, 0 },
    {  1312, KEY_UP },
    {  1313, 0 },
    {  1464, KEY_A },
    {  1465, 0 },
    {  1470, KEY_START },
    {  1471, 0 },
    {  1472, KEY_START },
    {  1473, 0 },
    {  1478, KEY_START },
    {  1479, 0 },
    {  1480, KEY_START },
    {  1481, 0 },
    {  1486, KEY_START },
    {  1487, 0 },
    {  1488, KEY_START },
    {  1489, 0 },
    {  1494, KEY_START },
    {  1495, 0 },
    {  1496, KEY_START },
    {  1497, 0 },
    {  1502, KEY_START },
    {  1503, 0 },
    {  1504, KEY_START },
    {  1505, 0 },
    {  1510, KEY_START },
    {  1511, 0 },
    {  1512, KEY_START },
    {  1513, 0 },
    {  1518, KEY_START },
    {  1519, 0 },
    {  1520, KEY_START },
    {  1521, 0 },
    {  1526, KEY_START },
    {  1527, 0 },
    {  1528, KEY_START },
    {  1529, 0 },
    {  1534, KEY_START },
    {  1535, 0 },
    {  1536, KEY_START },
    {  1537, 0 },
    {  1542, KEY_START },
    {  1543, 0 },
    {  1544, KEY_START },
    {  1545, 0 },
    {  1550, KEY_START },
    {  1551, 0 },
    {  1552, KEY_START },
    {  1553, 0 },
    {  1558, KEY_START },
    {  1559, 0 },
    {  1560, KEY_START },
    {  1561, 0 },
    {  1566, KEY_START },
    {  1567, 0 },
    {  1568, KEY_START },
    {  1569, 0 },
    {  1574, KEY_START },
    {  1575, 0 },
    {  1576, KEY_START },
    {  1577, 0 },
    {  1582, KEY_START },
    {  1583, 0 },
    {  1584, KEY_START },
    {  1585, 0 },
    {  1590, KEY_START },
    {  1591, 0 },
    {  1592, KEY_START },
    {  1593, 0 },
    {  1598, KEY_START },
    {  1599, 0 },
    {  1600, KEY_START },
    {  1601, 0 },
    {  1606, KEY_START },
    {  1607, 0 },
    {  1608, KEY_START },
    {  1609, 0 },
    {  1614, KEY_START },
    {  1615, 0 },
    {  1616, KEY_START },
    {  1617, 0 },
    {  1622, KEY_START },
    {  1623, 0 },
    {  1624, KEY_START },
    {  1625, 0 },
    {  1630, KEY_START },
    {  1631, 0 },
    {  1632, KEY_START },
    {  1633, 0 },
    {  1638, KEY_START },
    {  1639, 0 },
    {  1640, KEY_START },
    {  1641, 0 },
    {  1646, KEY_START },
    {  1647, 0 },
    {  1648, KEY_START },
    {  1649, 0 },
    {  1654, KEY_START },
    {  1655, 0 },
    {  1656, KEY_START },
    {  1657, 0 },
    {  1662, KEY_START },
    {  1663, 0 },
    {  1664, KEY_START },
    {  1665, 0 },
    {  1670, KEY_START },
    {  1671, 0 },
    {  1672, KEY_START },
    {  1673, 0 },
    {  1678, KEY_START },
    {  1679, 0 },
    {  1680, KEY_START },
    {  1681, 0 },
    {  1686, KEY_START },
    {  1687, 0 },
    {  1688, KEY_START },
    {  1689, 0 },
    {  1694, KEY_START },
    {  1695, 0 },
    {  1696, KEY_START },
    {  1697, 0 },
    {  1702, KEY_START },
    {  1703, 0 },
    {  1704, KEY_START },
    {  1705, 0 },
    {  1710, KEY_START },
    {  1711, 0 },
    {  1712, KEY_START },
    {  1713, 0 },
    {  1718, KEY_START },
    {  1719, 0 },
    {  1720, KEY_START },
    {  1721, 0 },
    {  1726, KEY_START },
    {  1727, 0 },
    {  1728, KEY_START },
    {  1729, 0 },
    {  1734, KEY_START },
    {  1735, 0 },
    {  1736, KEY_START },
    {  1737, 0 },
    {  1742, KEY_START },
    {  1743, 0 },
    {  1744, KEY_START },
    {  1745, 0 },
    {  1750, KEY_START },
    {  1751, 0 },
    {  1752, KEY_START },
    {  1753, 0 },
    {  1758, KEY_START },
    {  1759, 0 },
    {  1760, KEY_START },
    {  1761, 0 },
    {  1766, KEY_START },
    {  1767, 0 },
    {  1768, KEY_START },
    {  1769, 0 },
    {  1774, KEY_START },
    {  1775, 0 },
    {  1776, KEY_START },
    {  1777, 0 },
    {  1782, KEY_START },
    {  1783, 0 },
    {  1784, KEY_START },
    {  1785, 0 },
    {  2036, KEY_START },
    {  2037, 0 },
    {  2188, KEY_DOWN },
    {  2189, 0 },
    {  2340, KEY_DOWN },
    {  2341, 0 },
    {  2492, KEY_A },
    {  2493, 0 },
    {  2644, KEY_START },
    {  2645, 0 },
};

#define SCRIPT_LEN (sizeof(script) / sizeof(script[0]))

/* Posés par input_reset() : la RAM n'est pas remise à zéro au démarrage.
   `script_call` n'est plus propre au tour de jeu (task 12) : les écrans de
   menu et de fin de partie (screens.c) appellent input_edges() eux aussi,
   au même rythme (une fois par itération de leur boucle, donc une fois par
   frame), avant même qu'une partie n'existe. Toute la table ci-dessus est
   donc une seule chronologie continue d'appels à input_edges(), qui
   traverse menu -> partie -> écran de fin -> menu suivant -> partie
   suivante, jamais rembobinée : input_reset() ne s'exécute qu'une fois, au
   tout début de main(), avant le premier screen_menu(). */
static unsigned int script_call;   /* nombre d'appels à input_edges() */
static unsigned int script_index;

static unsigned short script_pad(void)
{
    while (script_index + 1 < SCRIPT_LEN &&
           script[script_index + 1].frame <= script_call) {
        script_index++;
    }
    script_call++;
    return script[script_index].pad;
}
#endif

void input_reset(void)
{
    prev = 0;
#ifdef INPUT_SCRIPT
    script_call = 0;
    script_index = 0;
#endif
}

unsigned short input_edges(unsigned short *pad_out)
{
#ifdef INPUT_SCRIPT
    unsigned short pad = script_pad();
#else
    unsigned short pad = padsCurrent(0);
#endif
    unsigned short hit = (unsigned short)(pad & ~prev);
    prev = pad;
    if (pad_out != (unsigned short *)0) *pad_out = pad;
    return hit;
}

void input_init(Cursor *c)
{
    c->x = BOARD_W / 2;
    c->y = BOARD_H / 2;
    c->show_range = TRUE;
    c->repeat = 0;
}

/* Le curseur circule sur le tore, comme le plateau. */
static void move(Cursor *c, int dx, int dy)
{
    c->x = (c->x + dx + BOARD_W) % BOARD_W;
    c->y = (c->y + dy + BOARD_H) % BOARD_H;
}

bool_t input_update(Cursor *c, Match *m)
{
    unsigned short pad;
    unsigned short hit = input_edges(&pad);
    int dx = 0, dy = 0;

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
