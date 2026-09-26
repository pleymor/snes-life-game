#ifndef INPUT_H
#define INPUT_H

#include "match.h"

typedef struct {
    int    x, y;          /* case de jeu sous le curseur */
    bool_t show_range;    /* marquage de portée affiché */
    int    repeat;        /* frames avant la prochaine répétition */
} Cursor;

/* À appeler une seule fois, avant le tout premier screen_menu() de main() :
   pose `prev` et, sous INPUT_SCRIPT, le rejeu à leur tout début. Ni la
   manette ni le rejeu scripté ne doivent être réinitialisés entre deux
   parties (voir input_edges() ci-dessous) : la remise à zéro du curseur
   par input_init() n'y touche plus. */
void input_reset(void);

void   input_init(Cursor *c);
/* Une passe par frame. Rend TRUE si le joueur a demandé la fin de son tour. */
bool_t input_update(Cursor *c, Match *m);

/* Lecture bas niveau partagée par input_update() et par les écrans de menu
   et de fin de partie (screens.c), pour que ces derniers réutilisent la
   même bascule pad/rejeu scripté plutôt que de dupliquer la lecture de la
   manette. Bascule padsCurrent(0)/rejeu selon INPUT_SCRIPT, maintient
   `prev`. Rend le masque des touches qui viennent d'être pressées ; si
   `pad_out` n'est pas NULL, y écrit aussi le masque brut (maintenu). */
unsigned short input_edges(unsigned short *pad_out);

#endif
