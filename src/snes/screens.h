#ifndef SCREENS_H
#define SCREENS_H

#include "match.h"

/* Bloque jusqu'au choix. Rend -1 pour deux joueurs, AI_EASY ou AI_NORMAL.
   Écrit dans `*frames` le nombre d'itérations de sa propre boucle avant ce
   choix : main.c s'en sert pour semer le xorshift32 de l'IA facile avec une
   valeur qui varie d'une partie à l'autre (mais reste déterministe pour une
   chronologie d'entrées donnée) plutôt qu'avec une constante fixe. */
int  screen_menu(unsigned int *frames);
/* Bloque jusqu'à une pression de START, plateau final laissé à l'écran. */
void screen_result(const Match *m);

#endif
