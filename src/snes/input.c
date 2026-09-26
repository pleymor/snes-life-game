#include <snes.h>
#include "input.h"

#define REPEAT_FIRST 15   /* frames avant la première répétition */
#define REPEAT_NEXT   4   /* puis une case toutes les 4 frames */

static unsigned short prev;

#ifdef INPUT_SCRIPT
/* Rejeu scripté pour la vérification headless (docs/snes-notes.md §2) :
   remplace padsCurrent(0) par la relecture d'une table figée de (frame,
   masque de touches). Rien de ce bloc ne compile dans la ROM par défaut :
   CFLAGS ne reçoit -DINPUT_SCRIPT que pour la cible `rom-script` du
   Makefile racine ; `make rom` ignore entièrement ce bloc.

   Chaque entrée tient son masque jusqu'à l'entrée suivante — une fonction
   en escalier de l'indice d'appel de input_update(). Cet indice n'avance
   que pendant GS_TURN (main.c n'appelle input_update() que dans cet état),
   donc les pauses GS_RESOLVE ne consomment aucune entrée de la table : le
   nombre réel de frames PPU écoulées entre deux entrées de la table est
   toujours au moins égal à la différence de leurs `frame`, mais peut être
   plus grand de 45 (RESOLVE_HOLD, main.c) à chaque tour de rouge terminé.

   Le détail de chaque étape (quelle vérification du § Step 3 de la tâche
   elle prouve, la position du curseur qu'elle produit à chaque appel) a
   été généré et vérifié par simulation avant transcription ici — la table
   ci-dessous n'est pas devinée à la main. RetroArch affiche son propre
   bandeau « contenu chargé » pendant les
   ~300-350 premières images (docs/snes-notes.md § 2) : une capture prise
   avant ne montre rien d'exploitable. La table démarre donc par 400 images
   d'attente (aucune touche), qui ne font que laisser ce bandeau se
   dissiper ; toute la suite est décalée d'autant par rapport à une
   première version sans cette attente. Résumé (valeurs de la table, égales
   à l'image réelle tant qu'aucun tick n'a eu lieu, c'est-à-dire jusqu'à
   l'étape 8 incluse) :
     - frames  400- 616 : étape 1 (déplacement), un pas simple puis un
       maintien sur chacun des quatre bords (sort et réapparaît de l'autre
       côté) ;
     - frame       623  : A sur une case vide hors de portée (loin de toute
       figure) : refusé, rien ne se pose ;
     - frames  632- 666 : navigation fine (pas simples) jusqu'à (5,13), à
       distance de Chebyshev 1 du bloc bleu (6,14)-(7,15) ;
     - frames  674- 682 : étape 2, SELECT éteint puis rallume les points de
       portée ;
     - frames  690- 762 : étapes 3 à 6, trois poses (dont une reprise après
       une annulation), une pose refusée sur case déjà occupée, une pose
       refusée par le budget une fois les trois utilisées ;
     - frame       774  : étape 7, START passe au rouge ;
     - frame       786  : étape 8, START de nouveau (rouge ne pose rien) :
       le tick a lieu immédiatement ;
     - frames  797-1689 : étapes 9 et 10, fin de partie jouée au pas de
       course (aucune pose, chaque tour clos par START) jusqu'à dépasser
       largement ROUND_CAP. Chaque GS_RESOLVE (45 images) qui s'ensuit
       retarde d'autant l'image réelle par rapport à la valeur de la table
       (voir le paragraphe ci-dessus) ; l'image réelle exacte de chaque
       image capturée a été retrouvée par capture/bisection, pas déduite
       de la valeur brute de la table. */
typedef struct {
    unsigned int   frame;
    unsigned short pad;
} ScriptStep;

static const ScriptStep script[] = {
    {     0, 0 },
    {   400, KEY_RIGHT },
    {   401, 0 },
    {   405, KEY_RIGHT },
    {   485, 0 },
    {   490, KEY_UP },
    {   554, 0 },
    {   559, KEY_DOWN },
    {   583, 0 },
    {   588, KEY_LEFT },
    {   616, 0 },
    {   623, KEY_A },
    {   624, 0 },
    {   632, KEY_RIGHT },
    {   633, 0 },
    {   634, KEY_RIGHT },
    {   635, 0 },
    {   636, KEY_RIGHT },
    {   637, 0 },
    {   638, KEY_RIGHT },
    {   639, 0 },
    {   640, KEY_RIGHT },
    {   641, 0 },
    {   642, KEY_RIGHT },
    {   643, 0 },
    {   644, KEY_RIGHT },
    {   645, 0 },
    {   646, KEY_DOWN },
    {   647, 0 },
    {   648, KEY_DOWN },
    {   649, 0 },
    {   650, KEY_DOWN },
    {   651, 0 },
    {   652, KEY_DOWN },
    {   653, 0 },
    {   654, KEY_DOWN },
    {   655, 0 },
    {   656, KEY_DOWN },
    {   657, 0 },
    {   658, KEY_DOWN },
    {   659, 0 },
    {   660, KEY_DOWN },
    {   661, 0 },
    {   662, KEY_DOWN },
    {   663, 0 },
    {   664, KEY_DOWN },
    {   665, 0 },
    {   666, KEY_DOWN },
    {   667, 0 },
    {   674, KEY_SELECT },
    {   675, 0 },
    {   682, KEY_SELECT },
    {   683, 0 },
    {   690, KEY_A },
    {   691, 0 },
    {   698, KEY_A },
    {   699, 0 },
    {   706, KEY_RIGHT },
    {   707, 0 },
    {   714, KEY_A },
    {   715, 0 },
    {   722, KEY_B },
    {   723, 0 },
    {   730, KEY_A },
    {   731, 0 },
    {   738, KEY_RIGHT },
    {   739, 0 },
    {   746, KEY_A },
    {   747, 0 },
    {   754, KEY_RIGHT },
    {   755, 0 },
    {   762, KEY_A },
    {   763, 0 },
    {   774, KEY_START },
    {   775, 0 },
    {   786, KEY_START },
    {   787, 0 },
    {   797, KEY_START },
    {   798, 0 },
    {   808, KEY_START },
    {   809, 0 },
    {   819, KEY_START },
    {   820, 0 },
    {   830, KEY_START },
    {   831, 0 },
    {   841, KEY_START },
    {   842, 0 },
    {   852, KEY_START },
    {   853, 0 },
    {   863, KEY_START },
    {   864, 0 },
    {   874, KEY_START },
    {   875, 0 },
    {   885, KEY_START },
    {   886, 0 },
    {   896, KEY_START },
    {   897, 0 },
    {   907, KEY_START },
    {   908, 0 },
    {   918, KEY_START },
    {   919, 0 },
    {   929, KEY_START },
    {   930, 0 },
    {   940, KEY_START },
    {   941, 0 },
    {   951, KEY_START },
    {   952, 0 },
    {   962, KEY_START },
    {   963, 0 },
    {   973, KEY_START },
    {   974, 0 },
    {   984, KEY_START },
    {   985, 0 },
    {   995, KEY_START },
    {   996, 0 },
    {  1006, KEY_START },
    {  1007, 0 },
    {  1017, KEY_START },
    {  1018, 0 },
    {  1028, KEY_START },
    {  1029, 0 },
    {  1039, KEY_START },
    {  1040, 0 },
    {  1050, KEY_START },
    {  1051, 0 },
    {  1061, KEY_START },
    {  1062, 0 },
    {  1072, KEY_START },
    {  1073, 0 },
    {  1083, KEY_START },
    {  1084, 0 },
    {  1094, KEY_START },
    {  1095, 0 },
    {  1105, KEY_START },
    {  1106, 0 },
    {  1116, KEY_START },
    {  1117, 0 },
    {  1127, KEY_START },
    {  1128, 0 },
    {  1138, KEY_START },
    {  1139, 0 },
    {  1149, KEY_START },
    {  1150, 0 },
    {  1160, KEY_START },
    {  1161, 0 },
    {  1171, KEY_START },
    {  1172, 0 },
    {  1182, KEY_START },
    {  1183, 0 },
    {  1193, KEY_START },
    {  1194, 0 },
    {  1204, KEY_START },
    {  1205, 0 },
    {  1215, KEY_START },
    {  1216, 0 },
    {  1226, KEY_START },
    {  1227, 0 },
    {  1237, KEY_START },
    {  1238, 0 },
    {  1248, KEY_START },
    {  1249, 0 },
    {  1259, KEY_START },
    {  1260, 0 },
    {  1270, KEY_START },
    {  1271, 0 },
    {  1281, KEY_START },
    {  1282, 0 },
    {  1292, KEY_START },
    {  1293, 0 },
    {  1303, KEY_START },
    {  1304, 0 },
    {  1314, KEY_START },
    {  1315, 0 },
    {  1325, KEY_START },
    {  1326, 0 },
    {  1336, KEY_START },
    {  1337, 0 },
    {  1347, KEY_START },
    {  1348, 0 },
    {  1358, KEY_START },
    {  1359, 0 },
    {  1369, KEY_START },
    {  1370, 0 },
    {  1380, KEY_START },
    {  1381, 0 },
    {  1391, KEY_START },
    {  1392, 0 },
    {  1402, KEY_START },
    {  1403, 0 },
    {  1413, KEY_START },
    {  1414, 0 },
    {  1424, KEY_START },
    {  1425, 0 },
    {  1435, KEY_START },
    {  1436, 0 },
    {  1446, KEY_START },
    {  1447, 0 },
    {  1457, KEY_START },
    {  1458, 0 },
    {  1468, KEY_START },
    {  1469, 0 },
    {  1479, KEY_START },
    {  1480, 0 },
    {  1490, KEY_START },
    {  1491, 0 },
    {  1501, KEY_START },
    {  1502, 0 },
    {  1512, KEY_START },
    {  1513, 0 },
    {  1523, KEY_START },
    {  1524, 0 },
    {  1534, KEY_START },
    {  1535, 0 },
    {  1545, KEY_START },
    {  1546, 0 },
    {  1556, KEY_START },
    {  1557, 0 },
    {  1567, KEY_START },
    {  1568, 0 },
    {  1578, KEY_START },
    {  1579, 0 },
    {  1589, KEY_START },
    {  1590, 0 },
    {  1600, KEY_START },
    {  1601, 0 },
    {  1611, KEY_START },
    {  1612, 0 },
    {  1622, KEY_START },
    {  1623, 0 },
    {  1633, KEY_START },
    {  1634, 0 },
    {  1644, KEY_START },
    {  1645, 0 },
    {  1655, KEY_START },
    {  1656, 0 },
    {  1666, KEY_START },
    {  1667, 0 },
    {  1677, KEY_START },
    {  1678, 0 },
    {  1688, KEY_START },
    {  1689, 0 }
};
#define SCRIPT_LEN (sizeof(script) / sizeof(script[0]))

/* Posés par input_init() : la RAM n'est pas remise à zéro au démarrage. */
static unsigned int script_call;   /* nombre d'appels à input_update() */
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

void input_init(Cursor *c)
{
    c->x = BOARD_W / 2;
    c->y = BOARD_H / 2;
    c->show_range = TRUE;
    c->repeat = 0;
    prev = 0;
#ifdef INPUT_SCRIPT
    script_call = 0;
    script_index = 0;
#endif
}

/* Le curseur circule sur le tore, comme le plateau. */
static void move(Cursor *c, int dx, int dy)
{
    c->x = (c->x + dx + BOARD_W) % BOARD_W;
    c->y = (c->y + dy + BOARD_H) % BOARD_H;
}

bool_t input_update(Cursor *c, Match *m)
{
#ifdef INPUT_SCRIPT
    unsigned short pad = script_pad();
#else
    unsigned short pad = padsCurrent(0);
#endif
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
