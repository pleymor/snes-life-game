#include <string.h>
#include "ai.h"

#define AI_MAX_DEPTH  2
#define AI_WIN        (4 * AI_MAX_DEPTH + 1)   /* 9 */
#define AI_MAX_CANDS  256
#define AI_TOPK_EASY  8
#define AI_TOPK_MAX   32

/* Tout est en portée fichier : la pile du 65816 ne supporterait rien de
   cette taille. */
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

/* ---- la fenêtre locale ---- */

static void win_load(const Board *b, int cx, int cy, int side)
{
    int r = side / 2, i, j;
    for (j = 0; j < side; j++) {
        for (i = 0; i < side; i++) {
            wa[j][i] = (u8)board_get(b, BOARD_WRAP_X(cx - r + i), BOARD_WRAP_Y(cy - r + j));
        }
    }
}

/* Ne calcule que l'intérieur : la couronne extérieure devient invalide.
   Le rayon utile perd une case par tick, et après `depth` ticks il reste
   exactement `depth`, ce que win_net mesure. */
static void win_tick(int side)
{
    int i, j;
    for (j = 1; j < side - 1; j++) {
        for (i = 1; i < side - 1; i++) {
            int n = 0, n1 = 0, di, dj;
            u8 self;
            for (dj = -1; dj <= 1; dj++) {
                for (di = -1; di <= 1; di++) {
                    u8 v;
                    if (di == 0 && dj == 0) continue;
                    v = wa[j + dj][i + di];
                    if (v) { n++; if (v == (u8)CELL_P1) n1++; }
                }
            }
            self = wa[j][i];
            wb[j][i] = LIFE_RULE(self, n, n1);
        }
    }
    memcpy(wa, wb, sizeof wa);
}

static int win_net(int side, int radius, Cell me)
{
    int c = side / 2, i, j, net = 0;
    for (j = c - radius; j <= c + radius; j++) {
        for (i = c - radius; i <= c + radius; i++) {
            u8 v = wa[j][i];
            if (v == (u8)me)      net++;
            else if (v != CELL_EMPTY) net--;
        }
    }
    return net;
}

int ai_eval_local(const Board *b, Cell who, int x, int y, int depth)
{
    int side = 4 * depth + 1;
    int t, without, with;

    win_load(b, x, y, side);
    for (t = 0; t < depth; t++) win_tick(side);
    without = win_net(side, depth, who);

    win_load(b, x, y, side);
    wa[side / 2][side / 2] = (u8)who;
    for (t = 0; t < depth; t++) win_tick(side);
    with = win_net(side, depth, who);

    return with - without;
}

/* ---- les candidats ---- */

static int live_neighbors(const Board *b, int x, int y)
{
    int dx, dy, n = 0;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            if (board_get(b, BOARD_WRAP_X(x + dx), BOARD_WRAP_Y(y + dy)) != CELL_EMPTY) n++;
        }
    }
    return n;
}

/* max(0, 4 - distance de Chebyshev à l'adversaire le plus proche) : pousse
   l'IA vers le contact, où l'Immigration Game permet de retourner des
   naissances. */
static int enemy_pull(const Board *b, Cell foe, int x, int y)
{
    int dx, dy, best = 0;
    for (dy = -4; dy <= 4; dy++) {
        for (dx = -4; dx <= 4; dx++) {
            int a, c;
            if (board_get(b, BOARD_WRAP_X(x + dx), BOARD_WRAP_Y(y + dy)) != foe) continue;
            a = (dx < 0) ? -dx : dx;
            c = (dy < 0) ? -dy : dy;
            if (c > a) a = c;
            if (4 - a > best) best = 4 - a;
        }
    }
    return best;
}

static int scan_index(const Cand *c) { return (int)c->y * BOARD_W + (int)c->x; }

/* Remplit `cands` dans l'ordre de balayage : y croissant puis x croissant.
   C'est cet ordre qui sert de départage. `range_mask` est déjà celui du
   joueur courant (fix round 1 : m->range_mask, tenu à jour par
   begin_turn() dans match.c pour tout le tour), donc plus recalculé ici. */
static int collect(const Board *cur, const u8 (*range_mask)[BOARD_W], Cell foe)
{
    int x, y, n = 0;

    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++) {
            int nb;
            if (board_get(cur, x, y) != CELL_EMPTY) continue;
            if (!range_mask[y][x]) continue;
            nb = live_neighbors(cur, x, y);
            if (nb == 0) continue;   /* posée dans le vide, elle meurt sans rien produire */
            if (n >= AI_MAX_CANDS) return n;
            cands[n].x = (u8)x;
            cands[n].y = (u8)y;
            cands[n].pre = (short)(2 * nb + enemy_pull(cur, foe, x, y));
            n++;
        }
    }
    return n;
}

static void swap_cand(int i, int j)
{
    Cand t = cands[i]; cands[i] = cands[j]; cands[j] = t;
}

/* Amène les k meilleurs par `pre` en tête. */
static void select_top(int n, int k)
{
    int i, j;
    for (i = 0; i < k && i < n; i++) {
        int best = i;
        for (j = i + 1; j < n; j++) {
            if (cands[j].pre > cands[best].pre ||
                (cands[j].pre == cands[best].pre &&
                 scan_index(&cands[j]) < scan_index(&cands[best]))) {
                best = j;
            }
        }
        if (best != i) swap_cand(i, best);
    }
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

/* Rassemble et trie les candidats du coup courant (j->made) dans le plateau
   de travail. Appelée par ai_begin() pour le premier coup, puis par
   ai_step() lui-même après chaque pose tant qu'il reste des coups à jouer
   dans le budget du tour (`BUDGET`). */
static void collect_round(AiJob *j, const Match *m)
{
    /* Cast expliqué à l'identique de l'ancien ai_choose() : nécessaire à
       816-tcc seulement (voir plus bas), sans changement de comportement. */
    j->n = collect(&work, (const u8 (*)[BOARD_W])m->range_mask, j->foe);
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

    /* `work` n'est jamais wrappé : rien ici ne lit le halo. */
    work = m->board;

    collect_round(j, m);
}

bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget)
{
    while (budget > 0) {
        int pick, i;

        if (j->i < j->top) {
            /* Le seul coût mesuré comme significatif (docs/snes-notes.md
               § 9) : c'est lui, et lui seul, que `budget` limite. */
            scores[j->i] = ai_eval_local(&work, j->me,
                                        (int)cands[j->i].x, (int)cands[j->i].y,
                                        j->depth);
            j->i++;
            budget--;
            continue;
        }

        /* Tous les candidats du coup courant sont notés (ou il n'y en
           avait aucun) : fixer la pose et enchaîner ne consomme aucun
           budget, comme la collecte elle-même (ai_begin()/collect_round(),
           ci-dessus). */
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
        j->made++;

        if (j->made >= BUDGET) {
            return TRUE;
        }
        collect_round(j, m);
    }

    return FALSE;
}

int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET])
{
    AiJob job;
    int i;

    ai_begin(&job, m, lvl);
    /* Budget "infini" : le plus grand nombre de candidats qu'un seul
       collect() puisse produire (AI_MAX_CANDS), très au-dessus du total
       réellement possible sur un tour entier (BUDGET * AI_TOPK_MAX = 96) ;
       un seul appel à ai_step() termine donc tout le tour. Garde ai_choose()
       identique en comportement à avant la tâche 11 : tous les tests de la
       tâche 6 restent valides sans changement (task 11 brief). */
    while (!ai_step(&job, m, rng, AI_MAX_CANDS)) {
        /* rien : ai_step() a déjà tout consommé en un appel dans ce cas ;
           la boucle n'existe que pour rester correcte si ça changeait. */
    }

    for (i = 0; i < job.made; i++) out[i] = job.out[i];
    return job.made;
}
