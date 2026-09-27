#ifndef SCREENS_H
#define SCREENS_H

#include "match.h"

/* Valeur rendue par screen_menu() pour l'entrée HOW TO PLAY. */
#define MENU_TUTORIAL (-2)

/* Bloque jusqu'au choix. Rend -1 pour deux joueurs, AI_EASY, AI_NORMAL ou
   MENU_TUTORIAL.
   Écrit dans `*frames` le nombre d'itérations de sa propre boucle avant ce
   choix : main.c s'en sert pour semer le xorshift32 de l'IA facile avec une
   valeur qui varie d'une partie à l'autre (mais reste déterministe pour une
   chronologie d'entrées donnée) plutôt qu'avec une constante fixe. */
int  screen_menu(unsigned int *frames);
/* Bloque jusqu'à une pression de START, plateau final laissé à l'écran. */
void screen_result(const Match *m);

/* Joue le tutoriel dans `m` jusqu'à sa fin ou jusqu'à START ; A passe à la
   leçon suivante. Efface ses textes et cache le curseur en sortant. */
void screen_tutorial(Match *m);

#endif
