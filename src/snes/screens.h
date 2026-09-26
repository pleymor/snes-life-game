#ifndef SCREENS_H
#define SCREENS_H

#include "match.h"

/* Bloque jusqu'au choix. Rend -1 pour deux joueurs, AI_EASY ou AI_NORMAL. */
int  screen_menu(void);
/* Bloque jusqu'à une pression de START, plateau final laissé à l'écran. */
void screen_result(const Match *m);

#endif
