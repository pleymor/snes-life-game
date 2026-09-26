#include <string.h>
#include "ai.h"

/* ---- vue d'ensemble ----

   Le score d'un candidat (spec § 6.1) compare, sur le carré de rayon
   `depth` autour de lui, le plateau après `depth` ticks avec et sans la
   pose. Simuler une fenêtre 9 x 9 par candidat coûte trop cher sur la
   console (816-tcc dépense une dizaine d'instructions par accès mémoire :
   près de trois frames par évaluation, docs/snes-notes.md § 9). D'où deux
   idées :

   1. Le plateau « sans » est le même pour tous les candidats d'un coup :
      ses deux générations suivantes (g1, g2) et les sommes de voisins qui
      les produisent (s1, s2) se calculent une seule fois par coup, sur les
      seules lignes et colonnes proches des candidats retenus.
   2. Le plateau « avec » ne diffère du « sans » que dans le cône de la
      pose : rayon 1 après un tick, rayon 2 après deux. Le tick 1 « avec »
      se recalcule sur 9 cases à partir de s1 ; ses écarts avec g1,
      reportés sur les sommes s2 des voisins, donnent le tick 2 « avec »
      sur 25 cases. Le score est la somme des écarts avec g2.

   Les plateaux de ce module stockent des codes plutôt que des Cell :
   AI_E1 pour un bleu, AI_E2 pour un rouge. La somme des huit voisins porte
   alors les deux comptes à la fois (bleus dans le quartet bas, rouges dans
   le haut), et la règle se lit dans deux tables construites depuis
   LIFE_RULE. Même stockage que Board (halo d'une case), pour lire les
   voisins à décalages constants comme life_tick(). */

#define AI_MAX_DEPTH  2
/* Plus grande portée lue autour d'une case : l'attraction regarde jusqu'à
   3, les générations précalculées jusqu'à 2 * AI_MAX_DEPTH - 1 = 3. */
#define AI_PAD        4
#define AI_E1         1
#define AI_E2         16
#define AI_SUM_SPAN   (8 * AI_E2 + 1)          /* sommes possibles : 0 à 128 */
#define AI_MAX_CANDS  256
#define AI_COLLECT_CHUNK 4   /* cases ouvertes examinées par étape de collecte */
#define AI_TOPK_EASY  8
#define AI_TOPK_MAX   32

/* Tout est en portée fichier : la pile du 65816 ne supporterait rien de
   cette taille. */
static Board wk;    /* plateau de travail, codé, halo à jour */
static Board g1b;   /* génération 1 sans la pose (codes) */
static Board s1b;   /* somme codée des voisins sur wk */
static Board g2b;   /* génération 2 sans la pose */
static Board s2b;   /* somme codée des voisins sur g1b */
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

/* ---- tables : repliage torique, codes, règle du jeu ---- */

/* wrap_col[x + AI_PAD] : colonne de stockage (halo compris) de la colonne
   de jeu x repliée ; wrap_row[y + AI_PAD] : ligne de jeu y repliée ;
   row_off[y + AI_PAD] : décalage de sa ligne de stockage depuis
   &b->c[0][0]. Valables pour -AI_PAD <= x < BOARD_W + AI_PAD (et pareil
   pour y). Plus aucun BOARD_WRAP ni aucune multiplication par BSTRIDE (34,
   qui n'est pas une puissance de deux) dans les boucles.

   rule_surv[s] : 0xFF si une cellule vivante entourée de la somme codée s
   survit, 0 sinon (masque de sa propre valeur). rule_birth[s] : code de la
   cellule qui naît dans une case vide, ou 0. Remplies depuis LIFE_RULE,
   qui reste l'unique définition de la règle. */
static u8     wrap_col[BOARD_W + 2 * AI_PAD];
static u8     wrap_row[BOARD_H + 2 * AI_PAD];
static int    row_off[BOARD_H + 2 * AI_PAD];
static u8     rule_surv[AI_SUM_SPAN];
static u8     rule_birth[AI_SUM_SPAN];
static bool_t tables_ready = FALSE;
static const u8 ai_code[3] = { 0, AI_E1, AI_E2 };

#define AI_RULE(self, s) ((self) ? (u8)((self) & rule_surv[s]) : rule_birth[s])

static void tables_init(void)
{
    int k, n1, n2;
    for (k = 0; k < BOARD_W + 2 * AI_PAD; k++) {
        wrap_col[k] = (u8)(BOARD_WRAP_X(k - AI_PAD) + 1);
    }
    for (k = 0; k < BOARD_H + 2 * AI_PAD; k++) {
        wrap_row[k] = (u8)BOARD_WRAP_Y(k - AI_PAD);
        row_off[k] = (wrap_row[k] + 1) * BSTRIDE;
    }
    for (n2 = 0; n2 <= 8; n2++) {
        for (n1 = 0; n1 + n2 <= 8; n1++) {
            int s = n1 * AI_E1 + n2 * AI_E2;
            int n = n1 + n2;
            rule_surv[s] = (u8)(LIFE_RULE((u8)CELL_P1, n, n1) ? 0xFF : 0);
            rule_birth[s] = ai_code[LIFE_RULE((u8)CELL_EMPTY, n, n1)];
        }
    }
    tables_ready = TRUE;
}

/* Somme codée des huit voisins de la case de stockage (x, y) quand p
   pointe sa voisine du coin haut-gauche (halo valide). */
#define AI_SUM8(p) ((p)[0] + (p)[1] + (p)[2] + (p)[BSTRIDE] + (p)[BSTRIDE + 2] + \
                    (p)[2 * BSTRIDE] + (p)[2 * BSTRIDE + 1] + (p)[2 * BSTRIDE + 2])

/* Cases vides et à portée au début du tour, dans l'ordre de balayage : les
   seules où chercher des candidats. Ni la portée ni les adversaires ne
   changent pendant le tour, et les poses de l'IA ne font qu'occuper des
   cases de cette liste : chaque collecte la reparcourt au lieu de balayer
   les 768 cases. */
static u8  open_x[BOARD_W * BOARD_H];
static u8  open_y[BOARD_W * BOARD_H];
/* Attraction de chaque case ouverte (voir enemy_pull_far()), calculée à
   la première collecte qui en a besoin puis réutilisée : elle ne dépend
   que des adversaires, qui ne bougent pas pendant le tour. AI_PULL_UNKNOWN
   tant qu'elle n'est pas calculée. */
#define AI_PULL_UNKNOWN 0xFF
static u8  open_pull[BOARD_W * BOARD_H];
static int n_open;

/* Ligne de jeu y de `src` (valeurs Cell) codée dans wk ; rend TRUE si elle
   contient une cellule de code `foe`. Si `mrow` (ligne du masque de
   portée) est donné, ajoute ses cases vides et à portée à la liste. */
static bool_t code_row(const Board *src, int y, u8 foe, const u8 *mrow)
{
    const u8 *in = &src->c[y + 1][1];
    u8 *out = &wk.c[y + 1][1];
    bool_t seen = FALSE;
    int x;
    for (x = 0; x < BOARD_W; x++) {
        u8 v = ai_code[in[x]];
        out[x] = v;
        if (v == foe) seen = TRUE;
        if (mrow && v == 0 && mrow[x]) {
            open_x[n_open] = (u8)x;
            open_y[n_open] = (u8)y;
            open_pull[n_open] = AI_PULL_UNKNOWN;
            n_open++;
        }
    }
    return seen;
}

/* ---- générations précalculées ----

   Lignes et colonnes où g1/s1 (need1) et g2/s2 (need2) sont nécessaires :
   dilatation des lignes et colonnes des candidats retenus. Leur produit
   couvre largement les carrés utiles. */
static u8  need_row1[BOARD_H], need_row2[BOARD_H];
static u8  need_col1[BOARD_W], need_col2[BOARD_W];
static u8  cols1[BOARD_W], cols2[BOARD_W];
static int ncols1, ncols2;

static void need_clear(void)
{
    memset(need_row1, 0, sizeof need_row1);
    memset(need_row2, 0, sizeof need_row2);
    memset(need_col1, 0, sizeof need_col1);
    memset(need_col2, 0, sizeof need_col2);
}

/* Marque les lignes et colonnes voisines de (x, y) : jusqu'à 2 * depth - 1
   pour g1 (le tick 2 « avec » lit g1 et s2 au rayon 2, et s2 lit g1 un
   cran plus loin), jusqu'à 2 pour g2 à la profondeur 2. */
static void need_mark(int x, int y, int depth)
{
    int d, r1 = 2 * depth - 1;
    for (d = -r1; d <= r1; d++) {
        need_row1[wrap_row[y + AI_PAD + d]] = 1;
        need_col1[wrap_col[x + AI_PAD + d] - 1] = 1;
    }
    if (depth > 1) {
        for (d = -2; d <= 2; d++) {
            need_row2[wrap_row[y + AI_PAD + d]] = 1;
            need_col2[wrap_col[x + AI_PAD + d] - 1] = 1;
        }
    }
}

static void need_lists(void)
{
    int x;
    ncols1 = 0;
    ncols2 = 0;
    for (x = 0; x < BOARD_W; x++) {
        if (need_col1[x]) cols1[ncols1++] = (u8)x;
        if (need_col2[x]) cols2[ncols2++] = (u8)x;
    }
}

/* g1 et s1 sur la ligne y, aux colonnes cols1. wk doit être wrappé. */
static void gen1_row(int y)
{
    const u8 *in = &wk.c[y][0];      /* in + x : coin haut-gauche de (x, y) */
    u8 *g = &g1b.c[y + 1][1];
    u8 *s = &s1b.c[y + 1][1];
    int k;
    for (k = 0; k < ncols1; k++) {
        int x = cols1[k];
        const u8 *p = in + x;
        int sum = AI_SUM8(p);
        u8 self = p[BSTRIDE + 1];
        s[x] = (u8)sum;
        g[x] = AI_RULE(self, sum);
    }
}

/* g2 et s2 sur la ligne y, aux colonnes cols2. g1b doit être wrappé. */
static void gen2_row(int y)
{
    const u8 *in = &g1b.c[y][0];
    u8 *g = &g2b.c[y + 1][1];
    u8 *s = &s2b.c[y + 1][1];
    int k;
    for (k = 0; k < ncols2; k++) {
        int x = cols2[k];
        const u8 *p = in + x;
        int sum = AI_SUM8(p);
        u8 self = p[BSTRIDE + 1];
        s[x] = (u8)sum;
        g[x] = AI_RULE(self, sum);
    }
}

/* ---- l'évaluation incrémentale ----

   Le carré 5 x 5 autour du candidat est indexé k = 5 * (dy + 2) + (dx + 2),
   le candidat en k = 12. Seules les cases dont la valeur « avec » peut
   différer de g2 sont visitées : celles dont la somme de voisins change
   (voisines d'une case du rayon 1 qui a changé au tick 1) ou dont la
   valeur au tick 1 a changé. Sur un plateau clairsemé, c'est une poignée
   de cases au lieu de 25. */
static const signed char k_dy[25] = {
    -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0,
     1,  1,  1,  1,  1,  2,  2,  2,  2,  2
};
static const signed char k_dx[25] = {
    -2, -1, 0, 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2,
    -2, -1, 0, 1, 2, -2, -1, 0, 1, 2
};
static const u8 k_r1[9] = { 6, 7, 8, 11, 12, 13, 16, 17, 18 };
static const signed char k_nb[8] = { -6, -5, -4, -1, 1, 4, 5, 6 };

static int t2[25];     /* écarts reportés sur les sommes de voisins du tick 2 */
static u8  w1[25];     /* tick 1 « avec », pour les cases du rayon 1 qui ont changé */
static u8  chg[25];    /* 1 : case du rayon 1 changée au tick 1 */
static u8  hit[25];    /* 1 : case déjà dans la liste `touched` */
static u8  touched[25];

/* +1 pour `mine`, -1 pour l'autre couleur, 0 pour une case vide. */
#define AI_SCORE(v, mine) ((v) == (mine) ? 1 : ((v) ? -1 : 0))

/* Gain net de la pose du code `mine` en (x, y), g1/s1 (et g2/s2 à la
   profondeur 2) étant à jour autour de la case. */
static int eval_delta(int x, int y, u8 mine, int depth)
{
    const u8  *cx = &wrap_col[x + AI_PAD];
    const int *ry = &row_off[y + AI_PAD];
    const u8  *wkb = &wk.c[0][0];
    const u8  *g1p = &g1b.c[0][0];
    const u8  *s1p = &s1b.c[0][0];
    /* Ce que la pose ajoute à la somme de ses huit voisins (la case est
       vide pour un vrai candidat, mais ai_eval_local() accepte tout). */
    int bump = (int)mine - (int)wkb[ry[0] + cx[0]];
    int q, e, nt = 0, net = 0;

    /* Tick 1 « avec » : la pose change la somme de ses huit voisins et
       devient elle-même vivante ; hors du rayon 1 rien ne change. Chaque
       case qui change reporte son écart sur ses huit voisines. */
    for (q = 0; q < 9; q++) {
        int k = k_r1[q];
        int off = ry[k_dy[k]] + cx[k_dx[k]];
        u8 self = wkb[off];
        int sum = s1p[off];
        u8 v, g = g1p[off];
        int d;
        if (k == 12) self = mine;
        else sum += bump;
        v = AI_RULE(self, sum);
        if (v == g) continue;
        if (depth == 1) {
            net += AI_SCORE(v, mine) - AI_SCORE(g, mine);
            continue;
        }
        w1[k] = v;
        chg[k] = 1;
        if (!hit[k]) { hit[k] = 1; touched[nt++] = (u8)k; }
        d = (int)v - (int)g;
        for (e = 0; e < 8; e++) {
            int n = k + k_nb[e];
            t2[n] += d;
            if (!hit[n]) { hit[n] = 1; touched[nt++] = (u8)n; }
        }
    }
    if (depth == 1) return net;

    /* Tick 2 « avec », sur les seules cases touchées ; puis remise à zéro
       des tableaux de travail pour le candidat suivant. */
    {
        const u8 *g2p = &g2b.c[0][0];
        const u8 *s2p = &s2b.c[0][0];
        for (q = 0; q < nt; q++) {
            int k = touched[q];
            int off = ry[k_dy[k]] + cx[k_dx[k]];
            int sum = s2p[off] + t2[k];
            u8 self = chg[k] ? w1[k] : g1p[off];
            u8 v = AI_RULE(self, sum), g = g2p[off];
            if (v != g) net += AI_SCORE(v, mine) - AI_SCORE(g, mine);
            t2[k] = 0;
            chg[k] = 0;
            hit[k] = 0;
        }
    }
    return net;
}

int ai_eval_local(const Board *b, Cell who, int x, int y, int depth)
{
    int k;

    if (!tables_ready) tables_init();
    for (k = 0; k < BOARD_H; k++) code_row(b, k, 0, (const u8 *)0);
    board_wrap(&wk);

    need_clear();
    need_mark(x, y, depth);
    need_lists();
    for (k = 0; k < BOARD_H; k++) {
        if (need_row1[k]) gen1_row(k);
    }
    if (depth > 1) {
        board_wrap(&g1b);
        for (k = 0; k < BOARD_H; k++) {
            if (need_row2[k]) gen2_row(k);
        }
    }
    return eval_delta(x, y, ai_code[who], depth);
}

/* ---- les candidats ---- */

static u8 foe_row[BOARD_H];    /* la ligne contient-elle un adversaire ? */
static u8 foe_near[BOARD_H];   /* ... ou l'une des lignes à 3 ou moins ? */

/* max(0, 4 - distance de Chebyshev à l'adversaire le plus proche) : pousse
   l'IA vers le contact, où l'Immigration Game permet de retourner des
   naissances. L'anneau de distance 1 est déjà connu de l'appelant (la
   somme des voisins le dit) ; celle-ci balaie les anneaux 2 puis 3 et rend
   dès le premier qui contient un adversaire, en sautant les lignes qui
   n'en ont aucun (les adversaires ne bougent pas pendant le tour). */
static int enemy_pull_far(u8 foe, int x, int y)
{
    const u8  *base = &wk.c[0][0];
    const u8  *cx = &wrap_col[x + AI_PAD];
    const u8  *yr = &wrap_row[y + AI_PAD];
    const int *ry = &row_off[y + AI_PAD];
    int d, k;

    if (!foe_near[y]) return 0;
    for (d = 2; d <= 3; d++) {
        int left = cx[-d], right = cx[d];
        if (foe_row[yr[-d]]) {
            const u8 *row = base + ry[-d];
            for (k = -d; k <= d; k++) if (row[cx[k]] == foe) return 4 - d;
        }
        if (foe_row[yr[d]]) {
            const u8 *row = base + ry[d];
            for (k = -d; k <= d; k++) if (row[cx[k]] == foe) return 4 - d;
        }
        for (k = 1 - d; k <= d - 1; k++) {
            if (foe_row[yr[k]]) {
                const u8 *row = base + ry[k];
                if (row[left] == foe || row[right] == foe) return 4 - d;
            }
        }
    }
    return 0;
}

static int scan_index(const Cand *c) { return (int)c->y * BOARD_W + (int)c->x; }

/* Examine jusqu'à AI_COLLECT_CHUNK cases de la liste des cases ouvertes à
   partir de j->pos, et ajoute à `cands` (à partir de j->n) celles qui sont
   encore vides et ont au moins un voisin vivant. La liste est dans l'ordre
   de balayage (y croissant puis x croissant), et c'est cet ordre qui sert
   de départage. Au plafond AI_MAX_CANDS, la collecte s'arrête là. */
static void collect_chunk(AiJob *j)
{
    const u8 *base = &wk.c[0][0];
    int n = j->n, q = j->pos, end = q + AI_COLLECT_CHUNK;
    u8 f = ai_code[j->foe];

    if (end > n_open) end = n_open;
    for (; q < end; q++) {
        int x = open_x[q], y = open_y[q];
        /* Coin haut-gauche du voisinage, dans le halo : lignes de
           stockage y à y + 2, contiguës. */
        const u8 *p = base + row_off[y + AI_PAD] - BSTRIDE + x;
        unsigned int sum, n1, n2, nf;
        u8 pull;

        if (p[BSTRIDE + 1] != 0) continue;   /* occupée par une pose du tour */
        sum = (unsigned int)AI_SUM8(p);
        if (sum == 0) continue;   /* posée dans le vide, elle meurt sans rien produire */
        if (n >= AI_MAX_CANDS) {
            q = n_open;
            break;
        }
        n1 = sum & (AI_E2 - 1);
        n2 = sum >> 4;            /* AI_E2 = 16 */
        nf = (f == AI_E1) ? n1 : n2;
        cands[n].x = (u8)x;
        cands[n].y = (u8)y;
        if (nf) {
            pull = 3;
        } else {
            pull = open_pull[q];
            if (pull == AI_PULL_UNKNOWN) {
                pull = (u8)enemy_pull_far(f, x, y);
                open_pull[q] = pull;
            }
        }
        cands[n].pre = (short)(2 * (int)(n1 + n2) + pull);
        n++;
    }
    j->n = n;
    j->pos = q;
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

/* ---- le tour, étape par étape ---- */

/* Prochaine ligne de `need` à partir de `y`, ou BOARD_H. */
static int next_needed(const u8 *need, int y)
{
    while (y < BOARD_H && !need[y]) y++;
    return y;
}

static void collect_start(AiJob *j)
{
    j->phase = AI_PH_COLLECT;
    j->pos = 0;
    j->n = 0;
    j->top = 0;
    j->i = 0;
}

/* La dernière ligne est balayée : garder les k meilleurs, puis repérer
   les lignes et colonnes où les générations sont nécessaires. */
static void collect_finish(AiJob *j)
{
    int i;
    if (j->n > 0) {
        select_top(j->n, j->k);
    }
    j->top = (j->n < j->k) ? j->n : j->k;
    j->i = 0;
    need_clear();
    for (i = 0; i < j->top; i++) {
        need_mark((int)cands[i].x, (int)cands[i].y, j->depth);
    }
    need_lists();
    j->phase = AI_PH_GEN1;
    j->pos = next_needed(need_row1, 0);
}

void ai_begin(AiJob *j, const Match *m, AiLevel lvl)
{
    j->lvl   = lvl;
    j->depth = (lvl == AI_EASY) ? 1 : AI_MAX_DEPTH;
    j->k     = (lvl == AI_EASY) ? AI_TOPK_EASY : AI_TOPK_MAX;
    j->me    = m->turn;
    j->foe   = (j->me == CELL_P1) ? CELL_P2 : CELL_P1;
    j->made  = 0;
    j->phase = AI_PH_PREP;
    j->pos   = 0;
    j->n     = 0;
    n_open   = 0;
    j->top   = 0;
    j->i     = 0;
    if (!tables_ready) tables_init();
}

/* Coût de la prochaine étape, en unités de budget (ai.h). */
static int step_cost(const AiJob *j)
{
    switch (j->phase) {
    case AI_PH_PREP:    return AI_COST_PREP;
    case AI_PH_COLLECT: return AI_COST_COLLECT;
    case AI_PH_GEN1:    return AI_COST_GEN_ROW + AI_COST_GEN_2COLS * ncols1 / 2;
    case AI_PH_GEN2:    return AI_COST_GEN_ROW + AI_COST_GEN_2COLS * ncols2 / 2;
    default:            return (j->i < j->top) ? AI_COST_EVAL : AI_COST_PICK;
    }
}

bool_t ai_step(AiJob *j, const Match *m, unsigned long *rng, int budget)
{
    int spent = 0;

    for (;;) {
        int pick, i, cost = step_cost(j);

        /* La première étape se fait toujours ; les suivantes seulement si
           elles tiennent dans ce qui reste du budget. */
        if (spent > 0 && spent + cost > budget) {
            return FALSE;
        }
        spent += cost;

        switch (j->phase) {
        case AI_PH_PREP:
            /* Copie codée du plateau, ligne par ligne, en notant où sont
               les adversaires. m->board n'a pas forcément son halo à jour
               (match_place() ne wrappe pas) : wk est wrappé à la fin. */
            foe_row[j->pos] = code_row(&m->board, j->pos, ai_code[j->foe],
                                       &m->range_mask[j->pos][0]);
            j->pos++;
            if (j->pos >= BOARD_H) {
                int y, d;
                board_wrap(&wk);
                for (y = 0; y < BOARD_H; y++) {
                    u8 near = 0;
                    for (d = -3; d <= 3; d++) near |= foe_row[wrap_row[y + AI_PAD + d]];
                    foe_near[y] = near;
                }
                collect_start(j);
            }
            continue;

        case AI_PH_COLLECT:
            collect_chunk(j);
            if (j->pos >= n_open) {
                collect_finish(j);
            }
            continue;

        case AI_PH_GEN1:
            /* collect_finish() et chaque ligne laissent `row` sur la
               prochaine ligne utile, ou BOARD_H : l'étape passe alors à la
               suivante sans rien coûter. */
            if (j->pos < BOARD_H) {
                gen1_row(j->pos);
                j->pos = next_needed(need_row1, j->pos + 1);
            }
            if (j->pos >= BOARD_H) {
                if (j->depth > 1) {
                    board_wrap(&g1b);
                    j->phase = AI_PH_GEN2;
                    j->pos = next_needed(need_row2, 0);
                } else {
                    j->phase = AI_PH_EVAL;
                }
            }
            continue;

        case AI_PH_GEN2:
            if (j->pos < BOARD_H) {
                gen2_row(j->pos);
                j->pos = next_needed(need_row2, j->pos + 1);
            }
            if (j->pos >= BOARD_H) {
                j->phase = AI_PH_EVAL;
            }
            continue;

        default:
            break;
        }

        if (j->i < j->top) {
            scores[j->i] = eval_delta((int)cands[j->i].x, (int)cands[j->i].y,
                                      ai_code[j->me], j->depth);
            j->i++;
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
        wk.c[cands[pick].y + 1][cands[pick].x + 1] = ai_code[j->me];
        board_wrap(&wk);
        j->made++;

        if (j->made >= BUDGET) {
            return TRUE;
        }
        collect_start(j);
    }
}

int ai_choose(const Match *m, AiLevel lvl, unsigned long *rng, Move out[BUDGET])
{
    AiJob job;
    int i;

    ai_begin(&job, m, lvl);
    /* Le plus grand budget qu'un `int` de 16 bits puisse porter : un tour
       entier y tient largement, donc un seul appel à ai_step() suffit ; la
       boucle ne sert qu'à rester correcte si les coûts changeaient. */
    while (!ai_step(&job, m, rng, 32767)) {
        /* rien */
    }

    for (i = 0; i < job.made; i++) out[i] = job.out[i];
    return job.made;
}
