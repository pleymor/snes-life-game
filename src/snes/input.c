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
   elle prouve, la position du curseur qu'elle produit à chaque appel) est
   documenté dans WS/task-10-report.md, généré et vérifié par simulation
   avant transcription ici — la table ci-dessous n'est pas devinée à la
   main. Résumé :
     - frames    0- 216 : étape 1 (déplacement), un pas simple puis un
       maintien sur chacun des quatre bords (sort et réapparaît de l'autre
       côté) ;
     - frame       223  : A sur une case vide hors de portée (loin de toute
       figure) : refusé, rien ne se pose ;
     - frames  232- 266 : navigation fine (pas simples) jusqu'à (5,13), à
       distance de Chebyshev 1 du bloc bleu (6,14)-(7,15) ;
     - frames  274- 282 : étape 2, SELECT éteint puis rallume les points de
       portée ;
     - frames  290- 362 : étapes 3 à 6, trois poses (dont une reprise après
       une annulation), une pose refusée sur case déjà occupée, une pose
       refusée par le budget une fois les trois utilisées ;
     - frame       374  : étape 7, START passe au rouge ;
     - frame       386  : étape 8, START de nouveau (rouge ne pose rien) :
       le tick a lieu immédiatement ;
     - frames  397-1289 : étapes 9 et 10, fin de partie jouée au pas de
       course (aucune pose, chaque tour clos par START) jusqu'à dépasser
       largement ROUND_CAP. */
typedef struct {
    unsigned int   frame;
    unsigned short pad;
} ScriptStep;

static const ScriptStep script[] = {
    {     0, KEY_RIGHT },
    {     1, 0 },
    {     5, KEY_RIGHT },
    {    85, 0 },
    {    90, KEY_UP },
    {   154, 0 },
    {   159, KEY_DOWN },
    {   183, 0 },
    {   188, KEY_LEFT },
    {   216, 0 },
    {   223, KEY_A },
    {   224, 0 },
    {   232, KEY_RIGHT },
    {   233, 0 },
    {   234, KEY_RIGHT },
    {   235, 0 },
    {   236, KEY_RIGHT },
    {   237, 0 },
    {   238, KEY_RIGHT },
    {   239, 0 },
    {   240, KEY_RIGHT },
    {   241, 0 },
    {   242, KEY_RIGHT },
    {   243, 0 },
    {   244, KEY_RIGHT },
    {   245, 0 },
    {   246, KEY_DOWN },
    {   247, 0 },
    {   248, KEY_DOWN },
    {   249, 0 },
    {   250, KEY_DOWN },
    {   251, 0 },
    {   252, KEY_DOWN },
    {   253, 0 },
    {   254, KEY_DOWN },
    {   255, 0 },
    {   256, KEY_DOWN },
    {   257, 0 },
    {   258, KEY_DOWN },
    {   259, 0 },
    {   260, KEY_DOWN },
    {   261, 0 },
    {   262, KEY_DOWN },
    {   263, 0 },
    {   264, KEY_DOWN },
    {   265, 0 },
    {   266, KEY_DOWN },
    {   267, 0 },
    {   274, KEY_SELECT },
    {   275, 0 },
    {   282, KEY_SELECT },
    {   283, 0 },
    {   290, KEY_A },
    {   291, 0 },
    {   298, KEY_A },
    {   299, 0 },
    {   306, KEY_RIGHT },
    {   307, 0 },
    {   314, KEY_A },
    {   315, 0 },
    {   322, KEY_B },
    {   323, 0 },
    {   330, KEY_A },
    {   331, 0 },
    {   338, KEY_RIGHT },
    {   339, 0 },
    {   346, KEY_A },
    {   347, 0 },
    {   354, KEY_RIGHT },
    {   355, 0 },
    {   362, KEY_A },
    {   363, 0 },
    {   374, KEY_START },
    {   375, 0 },
    {   386, KEY_START },
    {   387, 0 },
    {   397, KEY_START },
    {   398, 0 },
    {   408, KEY_START },
    {   409, 0 },
    {   419, KEY_START },
    {   420, 0 },
    {   430, KEY_START },
    {   431, 0 },
    {   441, KEY_START },
    {   442, 0 },
    {   452, KEY_START },
    {   453, 0 },
    {   463, KEY_START },
    {   464, 0 },
    {   474, KEY_START },
    {   475, 0 },
    {   485, KEY_START },
    {   486, 0 },
    {   496, KEY_START },
    {   497, 0 },
    {   507, KEY_START },
    {   508, 0 },
    {   518, KEY_START },
    {   519, 0 },
    {   529, KEY_START },
    {   530, 0 },
    {   540, KEY_START },
    {   541, 0 },
    {   551, KEY_START },
    {   552, 0 },
    {   562, KEY_START },
    {   563, 0 },
    {   573, KEY_START },
    {   574, 0 },
    {   584, KEY_START },
    {   585, 0 },
    {   595, KEY_START },
    {   596, 0 },
    {   606, KEY_START },
    {   607, 0 },
    {   617, KEY_START },
    {   618, 0 },
    {   628, KEY_START },
    {   629, 0 },
    {   639, KEY_START },
    {   640, 0 },
    {   650, KEY_START },
    {   651, 0 },
    {   661, KEY_START },
    {   662, 0 },
    {   672, KEY_START },
    {   673, 0 },
    {   683, KEY_START },
    {   684, 0 },
    {   694, KEY_START },
    {   695, 0 },
    {   705, KEY_START },
    {   706, 0 },
    {   716, KEY_START },
    {   717, 0 },
    {   727, KEY_START },
    {   728, 0 },
    {   738, KEY_START },
    {   739, 0 },
    {   749, KEY_START },
    {   750, 0 },
    {   760, KEY_START },
    {   761, 0 },
    {   771, KEY_START },
    {   772, 0 },
    {   782, KEY_START },
    {   783, 0 },
    {   793, KEY_START },
    {   794, 0 },
    {   804, KEY_START },
    {   805, 0 },
    {   815, KEY_START },
    {   816, 0 },
    {   826, KEY_START },
    {   827, 0 },
    {   837, KEY_START },
    {   838, 0 },
    {   848, KEY_START },
    {   849, 0 },
    {   859, KEY_START },
    {   860, 0 },
    {   870, KEY_START },
    {   871, 0 },
    {   881, KEY_START },
    {   882, 0 },
    {   892, KEY_START },
    {   893, 0 },
    {   903, KEY_START },
    {   904, 0 },
    {   914, KEY_START },
    {   915, 0 },
    {   925, KEY_START },
    {   926, 0 },
    {   936, KEY_START },
    {   937, 0 },
    {   947, KEY_START },
    {   948, 0 },
    {   958, KEY_START },
    {   959, 0 },
    {   969, KEY_START },
    {   970, 0 },
    {   980, KEY_START },
    {   981, 0 },
    {   991, KEY_START },
    {   992, 0 },
    {  1002, KEY_START },
    {  1003, 0 },
    {  1013, KEY_START },
    {  1014, 0 },
    {  1024, KEY_START },
    {  1025, 0 },
    {  1035, KEY_START },
    {  1036, 0 },
    {  1046, KEY_START },
    {  1047, 0 },
    {  1057, KEY_START },
    {  1058, 0 },
    {  1068, KEY_START },
    {  1069, 0 },
    {  1079, KEY_START },
    {  1080, 0 },
    {  1090, KEY_START },
    {  1091, 0 },
    {  1101, KEY_START },
    {  1102, 0 },
    {  1112, KEY_START },
    {  1113, 0 },
    {  1123, KEY_START },
    {  1124, 0 },
    {  1134, KEY_START },
    {  1135, 0 },
    {  1145, KEY_START },
    {  1146, 0 },
    {  1156, KEY_START },
    {  1157, 0 },
    {  1167, KEY_START },
    {  1168, 0 },
    {  1178, KEY_START },
    {  1179, 0 },
    {  1189, KEY_START },
    {  1190, 0 },
    {  1200, KEY_START },
    {  1201, 0 },
    {  1211, KEY_START },
    {  1212, 0 },
    {  1222, KEY_START },
    {  1223, 0 },
    {  1233, KEY_START },
    {  1234, 0 },
    {  1244, KEY_START },
    {  1245, 0 },
    {  1255, KEY_START },
    {  1256, 0 },
    {  1266, KEY_START },
    {  1267, 0 },
    {  1277, KEY_START },
    {  1278, 0 },
    {  1288, KEY_START },
    {  1289, 0 }
};
#define SCRIPT_LEN (sizeof(script) / sizeof(script[0]))

static unsigned int script_call = 0;   /* nombre d'appels à input_update() */
static unsigned int script_index = 0;

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
