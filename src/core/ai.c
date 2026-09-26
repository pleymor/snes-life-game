#include <string.h>
#include "ai.h"

#define AI_MAX_DEPTH  2
#define AI_WIN        (4 * AI_MAX_DEPTH + 1)   /* 9 */
/* Centre de la fenêtre, quelle que soit la profondeur : une fenêtre de
   profondeur 1 (5 x 5) occupe le milieu du tableau 9 x 9. */
#define AI_C          (AI_WIN / 2)
/* Décalage, dans une fenêtre aplatie, de la case (r, r) du coin : les
   lignes font AI_WIN cases. */
#define AI_CORNER(r)  ((r) * (AI_WIN + 1))
/* Plus grande portée lue autour d'une case : le rayon de la fenêtre. */
#define AI_PAD        (2 * AI_MAX_DEPTH)       /* 4 */
#define AI_MAX_CANDS  256
#define AI_TOPK_EASY  8
#define AI_TOPK_MAX   32

/* Tout est en portée fichier : la pile du 65816 ne supporterait rien de
   cette taille. */
static u8    ws[AI_WIN][AI_WIN];   /* fenêtre chargée, jamais écrite par un tick */
static u8    wa[AI_WIN][AI_WIN];
static u8    wb[AI_WIN][AI_WIN];
static Board work;
static int   scores[AI_TOPK_MAX];

typedef struct { u8 x, y; short pre; } Cand;
static Cand cands[AI_MAX_CANDS];

/* Masqué sur 32 bits pour que la suite soit identique sur l'hôte, où `long`
   fait souvent 64 bits, et sur la console, où il en fait 32. Sans quoi le
   simulateur et la ROM divergeraient. */
static unsigned long xs32(unsigned long *s)
{
    unsigned long x = *s & 0xFFFFFFFFUL;
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x <<  5) & 0xFFFFFFFFUL;
    *s = x;
    return x;
}

/* ---- repliage torique par tables ---- */

/* wrap_col[x + AI_PAD] : colonne de stockage (halo compris) de la colonne
   de jeu x repliée, pour -AI_PAD <= x < BOARD_W + AI_PAD. row_off[y +
   AI_PAD] : décalage, depuis &b->c[0][0], de la ligne de stockage de la
   ligne de jeu y repliée. Calculées une fois : plus aucun BOARD_WRAP ni
   aucune multiplication par BSTRIDE (34, qui n'est pas une puissance de
   deux) dans les boucles qui lisent le plateau autour d'une case. */
static u8     wrap_col[BOARD_W + 2 * AI_PAD];
static int    row_off[BOARD_H + 2 * AI_PAD];
static bool_t wrap_ready = FALSE;

static void wrap_init(void)
{
    int k;
    for (k = 0; k < BOARD_W + 2 * AI_PAD; k++) {
        wrap_col[k] = (u8)(BOARD_WRAP_X(k - AI_PAD) + 1);
    }
    for (k = 0; k < BOARD_H + 2 * AI_PAD; k++) {
        row_off[k] = (BOARD_WRAP_Y(k - AI_PAD) + 1) * BSTRIDE;
    }
    wrap_ready = TRUE;
}

/* ---- la fenêtre locale ---- */

/* Charge le carré de rayon 2 * depth autour de (cx, cy), centré dans ws :
   une ligne de stockage par ligne de fenêtre, une table de colonnes
   repliées, aucun appel par case. */
static void win_load(const Board *b, int cx, int cy, int depth)
{
    int r = 2 * depth, len = 2 * r + 1, i, j;
    const u8  *base = &b->c[0][0];
    const u8  *cols = &wrap_col[cx + AI_PAD - r];
    const int *rows = &row_off[cy + AI_PAD - r];
    u8 *q = &ws[0][0] + AI_CORNER(AI_C - r);

    if (!wrap_ready) wrap_init();
    for (j = 0; j < len; j++) {
        const u8 *row = base + rows[j];
        for (i = 0; i < len; i++) {
            q[i] = row[cols[i]];
        }
        q += AI_WIN;
    }
}

/* Un tick de la fenêtre, de `src` vers `dst`, limité au carré de rayon `r`
   autour du centre : le cône de lumière. Une perturbation ne voyage que
   d'une case par tick, donc au tick t (compté depuis 0) d'une évaluation
   de profondeur `depth`, seules les cases à moins de 2 * depth - 1 - t du
   centre peuvent encore influencer la zone mesurée ; les autres ne sont
   jamais relues. Comptage des voisins déroulé, à décalages constants
   depuis un pointeur qui avance d'une case, exactement comme life_tick()
   (aucune multiplication par case, aucun test de bord). */
static void win_tick(const u8 *src, u8 *dst, int r)
{
    int i, j, len = 2 * r + 1;
    const u8 *p = src + AI_CORNER(AI_C - r - 1);
    u8 *q = dst + AI_CORNER(AI_C - r);

    for (j = 0; j < len; j++) {
        for (i = 0; i < len; i++) {
            u8 self, v;
            int n = 0, n1 = 0;

            v = p[0];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[1];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2];              if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[AI_WIN];         if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[AI_WIN + 2];     if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * AI_WIN];     if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * AI_WIN + 1]; if (v) { n++; if (v == CELL_P1) n1++; }
            v = p[2 * AI_WIN + 2]; if (v) { n++; if (v == CELL_P1) n1++; }

            self = p[AI_WIN + 1];
            *q = LIFE_RULE(self, n, n1);
            p++;
            q++;
        }
        p += AI_WIN - len;
        q += AI_WIN - len;
    }
}

/* Bilan de `me` sur le carré de rayon `radius` autour du centre. */
static int win_net(const u8 *w, int radius, Cell me)
{
    int i, j, len = 2 * radius + 1, net = 0;
    const u8 *p = w + AI_CORNER(AI_C - radius);

    for (j = 0; j < len; j++) {
        for (i = 0; i < len; i++) {
            u8 v = p[i];
            if (v == (u8)me)          net++;
            else if (v != CELL_EMPTY) net--;
        }
        p += AI_WIN;
    }
    return net;
}

/* Simule `depth` ticks en partant de ws, en alternant wb et wa comme
   destinations, et rend le bilan sur la zone mesurée. ws n'est jamais
   écrit : la passe suivante repart de la même fenêtre sans la recharger. */
static int win_run(int depth, Cell me)
{
    const u8 *src = &ws[0][0];
    int t;

    for (t = 0; t < depth; t++) {
        u8 *dst = (t & 1) ? &wa[0][0] : &wb[0][0];
        win_tick(src, dst, 2 * depth - 1 - t);
        src = dst;
    }
    return win_net(src, depth, me);
}

int ai_eval_local(const Board *b, Cell who, int x, int y, int depth)
{
    int without, with;

    win_load(b, x, y, depth);
    without = win_run(depth, who);

    /* Une seule lecture du plateau par évaluation : la seconde passe pose
       la cellule directement dans ws, que la prochaine évaluation
       rechargera de toute façon. */
    ws[AI_C][AI_C] = (u8)who;
    with = win_run(depth, who);

    return with - without;
}

/* ---- les candidats ---- */

/* max(0, 4 - distance de Chebyshev à l'adversaire le plus proche) : pousse
   l'IA vers le contact, où l'Immigration Game permet de retourner des
   naissances. L'anneau de distance 1 est déjà connu de l'appelant (le
   comptage des voisins le lit) ; celle-ci balaie les anneaux 2 puis 3 et
   rend dès le premier qui contient un adversaire. Au-delà, 4 - d <= 0 :
   inutile de regarder. Lecture par les tables de repliage. */
static int enemy_pull_far(const Board *b, u8 foe, int x, int y)
{
    const u8  *base = &b->c[0][0];
    const u8  *cx = &wrap_col[x + AI_PAD];   /* cx[dx], |dx| <= AI_PAD */
    const int *ry = &row_off[y + AI_PAD];    /* ry[dy], |dy| <= AI_PAD */
    int d, k;

    for (d = 2; d <= 3; d++) {
        const u8 *top = base + ry[-d];
        const u8 *bot = base + ry[d];
        int left = cx[-d], right = cx[d];
        for (k = -d; k <= d; k++) {
            int c = cx[k];
            if (top[c] == foe || bot[c] == foe) return 4 - d;
        }
        for (k = 1 - d; k <= d - 1; k++) {
            const u8 *row = base + ry[k];
            if (row[left] == foe || row[right] == foe) return 4 - d;
        }
    }
    return 0;
}

static int scan_index(const Cand *c) { return (int)c->y * BOARD_W + (int)c->x; }

/* Ajoute à `cands` (à partir de j->n) les candidats de la ligne j->row,
   dans l'ordre de balayage : y croissant puis x croissant, et c'est cet
   ordre qui sert de départage. `range_mask` est déjà celui du joueur
   courant (fix round 1 : m->range_mask, tenu à jour par begin_turn() dans
   match.c pour tout le tour), donc plus recalculé ici. `work` est wrappé :
   les voisins se lisent dans le halo, à décalages constants depuis un
   pointeur de ligne, comme life_tick(). Au plafond AI_MAX_CANDS, la
   collecte s'arrête là, lignes suivantes comprises. */
static void collect_row(AiJob *j, const u8 *mrow)
{
    int x, n = j->n, y = j->row;
    const u8 *row = &work.c[y + 1][1];
    u8 f = (u8)j->foe;

    for (x = 0; x < BOARD_W; x++) {
        const u8 *p;
        u8 v;
        int nb = 0;
        bool_t adj = FALSE;

        if (row[x] != CELL_EMPTY) continue;
        if (!mrow[x]) continue;

        p = row + x - BSTRIDE - 1;
        v = p[0];               if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[1];               if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[2];               if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[BSTRIDE];         if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[BSTRIDE + 2];     if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[2 * BSTRIDE];     if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[2 * BSTRIDE + 1]; if (v) { nb++; if (v == f) adj = TRUE; }
        v = p[2 * BSTRIDE + 2]; if (v) { nb++; if (v == f) adj = TRUE; }

        if (nb == 0) continue;   /* posée dans le vide, elle meurt sans rien produire */
        if (n >= AI_MAX_CANDS) {
            j->row = BOARD_H - 1;   /* ai_step() l'avance à BOARD_H : fini */
            break;
        }
        cands[n].x = (u8)x;
        cands[n].y = (u8)y;
        cands[n].pre = (short)(2 * nb + (adj ? 3 : enemy_pull_far(&work, f, x, y)));
        n++;
    }
    j->n = n;
}

/* `pre` vaut 2 * voisins (1 à 8) + attraction (0 à 3) : de 2 à 19. */
#define AI_PRE_SPAN   (2 * 8 + 3 + 1)

static int  pre_pos[AI_PRE_SPAN];
static Cand picked[AI_TOPK_MAX];

/* Amène les k meilleurs en tête, triés par `pre` décroissant puis par
   ordre de balayage : un tri par dénombrement, stable, tronqué à k. Même
   résultat qu'un tri par sélection sur cette clé totale (cands arrive dans
   l'ordre de balayage), mais en deux passages sur n au lieu de k. */
static void select_top(int n, int k)
{
    int i, p, pos = 0;

    for (p = 0; p < AI_PRE_SPAN; p++) pre_pos[p] = 0;
    for (i = 0; i < n; i++) pre_pos[cands[i].pre]++;
    for (p = AI_PRE_SPAN - 1; p >= 0; p--) {
        int c = pre_pos[p];
        pre_pos[p] = pos;
        pos += c;
    }
    for (i = 0; i < n; i++) {
        p = cands[i].pre;
        if (pre_pos[p] < k) {
            picked[pre_pos[p]] = cands[i];
        }
        pre_pos[p]++;
    }
    if (n < k) k = n;
    for (i = 0; i < k; i++) cands[i] = picked[i];
}

static void swap_cand(int i, int j)
{
    Cand t = cands[i]; cands[i] = cands[j]; cands[j] = t;
}

/* Amène les trois meilleurs par score évalué en tête, cands et scores
   déplacés ensemble. */
static void select_top3_by_score(int top)
{
    int i, j, lim = (top < 3) ? top : 3;
    for (i = 0; i < lim; i++) {
        int best = i;
        for (j = i + 1; j < top; j++) {
            if (scores[j] > scores[best] ||
                (scores[j] == scores[best] &&
                 scan_index(&cands[j]) < scan_index(&cands[best]))) {
                best = j;
            }
        }
        if (best != i) {
            int ts = scores[i];
            scores[i] = scores[best];
            scores[best] = ts;
            swap_cand(i, best);
        }
    }
}

/* Prépare la collecte des candidats du coup courant (j->made) : ai_step()
   balaiera ensuite une ligne du plateau de travail par étape. */
static void collect_start(AiJob *j)
{
    j->row = 0;
    j->n = 0;
    j->top = 0;
    j->i = 0;
}

/* La dernière ligne est balayée : garder les k meilleurs. */
static void collect_finish(AiJob *j)
{
    if (j->n > 0) {
        select_top(j->n, j->k);
    }
    j->top = (j->n < j->k) ? j->n : j->k;
    j->i = 0;
}

void ai_begin(AiJob *j, const Match *m, AiLevel lvl)
{
    j->lvl   = lvl;
    j->depth = (lvl == AI_EASY) ? 1 : AI_MAX_DEPTH;
    j->k     = (lvl == AI_EASY) ? AI_TOPK_EASY : AI_TOPK_MAX;
    j->me    = m->turn;
    j->foe   = (j->me == CELL_P1) ? CELL_P2 : CELL_P1;
    j->made  = 0;

    if (!wrap_ready) wrap_init();
    /* collect_row() lit les voisins dans le halo : m->board ne l'a pas
       forcément à jour (match_place() ne wrappe pas). */
    work = m->board;
    board_wrap(&work);

    collect_start(j);
}

bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget)
{
    while (budget > 0) {
        int pick, i;

        if (j->row < BOARD_H) {
            collect_row(j, &m->range_mask[j->row][0]);
            j->row++;
            budget -= AI_COST_ROW;
            if (j->row >= BOARD_H) {
                collect_finish(j);
            }
            continue;
        }

        if (j->i < j->top) {
            scores[j->i] = ai_eval_local(&work, j->me,
                                        (int)cands[j->i].x, (int)cands[j->i].y,
                                        j->depth);
            j->i++;
            budget -= AI_COST_EVAL;
            continue;
        }

        /* Tous les candidats du coup courant sont notés (ou il n'y en
           avait aucun) : fixer la pose, puis relancer la collecte. */
        if (j->top == 0 || j->made >= BUDGET) {
            return TRUE;
        }

        if (j->lvl == AI_EASY) {
            int pool = (j->top < 3) ? j->top : 3;
            select_top3_by_score(j->top);
            pick = (int)(xs32(rng) % (unsigned long)pool);
        } else {
            pick = 0;
            for (i = 1; i < j->top; i++) {
                if (scores[i] > scores[pick] ||
                    (scores[i] == scores[pick] &&
                     scan_index(&cands[i]) < scan_index(&cands[pick]))) {
                    pick = i;
                }
            }
        }

        j->out[j->made].x = cands[pick].x;
        j->out[j->made].y = cands[pick].y;
        /* Fixée sur le plateau de travail : la pose suivante est évaluée
           en tenant compte de celle-ci, ce qui permet de trouver des
           combinaisons de deux ou trois cellules. */
        board_set(&work, (int)cands[pick].x, (int)cands[pick].y, j->me);
        board_wrap(&work);
        j->made++;
        budget -= AI_COST_PICK;

        if (j->made >= BUDGET) {
            return TRUE;
        }
        collect_start(j);
    }

    return FALSE;
}

int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET])
{
    AiJob job;
    int i;

    ai_begin(&job, m, lvl);
    /* Le plus grand budget qu'un `int` de 16 bits puisse porter : un tour
       entier (72 lignes, 96 évaluations, 3 poses au plus) y tient
       largement, donc un seul appel à ai_step() suffit ; la boucle ne sert
       qu'à rester correcte si les coûts changeaient. */
    while (!ai_step(&job, m, rng, 32767)) {
        /* rien */
    }

    for (i = 0; i < job.made; i++) out[i] = job.out[i];
    return job.made;
}
