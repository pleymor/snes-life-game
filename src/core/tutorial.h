#ifndef TUTORIAL_HDR
#define TUTORIAL_HDR

#include "view.h"

#define TUT_LESSONS 9

/* Lecteur du tutoriel. Tous les champs sont posés par tut_start() : la RAM
   de la console n'est pas remise à zéro. */
typedef struct {
    int    pc;          /* indice (en triplets) de l'opération suivante */
    int    wait;        /* images restantes de l'attente en cours */
    int    lesson;      /* leçon en cours, 1 à TUT_LESSONS */
    int    caption;     /* texte affiché, -1 pour aucun */
    int    cursor_x, cursor_y;
    bool_t cursor_on;   /* curseur montré */
    bool_t show_range;  /* points de portée montrés */
    bool_t done;        /* tutoriel fini */
} TutPlayer;

/* Démarre à la leçon 1 et joue sa mise en place. */
void tut_start(TutPlayer *p, Match *m);

/* Avance le script de `frames` images écoulées : exécute les opérations
   jusqu'à la prochaine attente non écoulée. Rend TRUE si le plateau, la
   portée, le curseur ou le texte ont changé. Sans effet une fois fini. */
bool_t tut_update(TutPlayer *p, Match *m, int frames);

/* Saute au début de la leçon suivante et joue sa mise en place ; après la
   dernière leçon, met `done`. Sans effet une fois fini. */
void tut_skip(TutPlayer *p, Match *m);

/* TRUE pendant la pause finale de la leçon en cours : toutes ses actions
   ont eu lieu. */
bool_t tut_lesson_done(const TutPlayer *p);

/* Ligne `line` (0 à 2) du texte i (0 à TUT_LESSONS - 1) ; "" hors bornes. */
const char *tut_caption_line(int i, int line);

/* Écrit le texte i sur trois lignes de tuiles ; i hors bornes : vide. */
void view_caption(int i, u8 out[3][HUD_W]);

#endif
