#ifndef INPUT_H
#define INPUT_H

#include "match.h"

typedef struct {
    int    x, y;          /* case de jeu sous le curseur */
    bool_t show_range;    /* marquage de portée affiché */
    int    repeat;        /* frames avant la prochaine répétition */
} Cursor;

void   input_init(Cursor *c);
/* Une passe par frame. Rend TRUE si le joueur a demandé la fin de son tour. */
bool_t input_update(Cursor *c, Match *m);

#endif
