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
static Cand all_c[AI_MAX_CANDS];   /* tous les candidats, ordre de balayage */
static Cand cands[AI_TOPK_MAX];    /* les k retenus, puis triés par score */

/* ---- xorshift32 sur deux moitiés de 16 bits ----

   Chaque décalage de la valeur de 32 bits se répartit entre les deux
   moitiés, avec ce qui passe d'une moitié à l'autre. Les conversions en
   unsigned short tronquent à 16 bits, que `int` fasse 16 bits (console)
   ou 32 (hôte). */

void rng_seed(Rng *r, unsigned short hi, unsigned short lo)
{
    r->hi = hi;
    r->lo = lo;
}

void rng_next(Rng *r)
{
    unsigned short hi = r->hi, lo = r->lo;

    /* x ^= x << 13 */
    hi = (unsigned short)(hi ^ (unsigned short)((unsigned short)(hi << 13) | (lo >> 3)));
    lo = (unsigned short)(lo ^ (unsigned short)(lo << 13));
    /* x ^= x >> 17 : la moitié haute, décalée d'un bit, tombe dans la basse */
    lo = (unsigned short)(lo ^ (hi >> 1));
    /* x ^= x << 5 */
    hi = (unsigned short)(hi ^ (unsigned short)((unsigned short)(hi << 5) | (lo >> 11)));
    lo = (unsigned short)(lo ^ (unsigned short)(lo << 5));

    r->hi = hi;
    r->lo = lo;
}

unsigned int rng_mod(const Rng *r, unsigned int p)
{
    /* (hi * 65536 + lo) mod p, sans jamais dépasser 16 bits : 65536 mod p
       s'écrit ((65535 mod p) + 1) mod p, et p <= 255 garde le produit
       sous 255 * 255. */
    unsigned int base = ((65535U % p) + 1U) % p;
    return (((unsigned int)r->hi % p) * base + (unsigned int)r->lo % p) % p;
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
static int n_open;
/* Indice de chaque case dans la liste des cases ouvertes, ou -1. */
static short open_at[BOARD_H][BOARD_W];

/* Attraction vers l'adversaire (spec § 6.2) : max(0, 4 - distance de
   Chebyshev à l'adversaire le plus proche). Comme 4 - max(|dx|, |dy|) =
   min(4 - |dx|, 4 - |dy|), elle se sépare en lignes : foe_h[y][x] est le
   max de 4 - |dx| sur les adversaires de la ligne y à trois colonnes ou
   moins (0 sinon), et l'attraction d'une case le max, sur les lignes
   voisines à trois ou moins, de min(4 - |dy|, foe_h). Les adversaires ne
   bougent pas pendant le tour : foe_h se remplit une fois, à la copie. */
static u8 foe_h[BOARD_H][BOARD_W];
static u8 foe_row[BOARD_H];    /* la ligne contient-elle un adversaire ? */

/* Ligne de jeu y de `src` (valeurs Cell) codée dans wk. Si `mrow` (ligne
   du masque de portée) est donné, ajoute ses cases vides et à portée à la
   liste des cases ouvertes et note ses adversaires (code `foe`) dans
   foe_h/foe_row. */
static void code_row(const Board *src, int y, u8 foe, const u8 *mrow)
{
    const u8 *in = &src->c[y + 1][1];
    u8 *out = &wk.c[y + 1][1];
    u8 *h = &foe_h[y][0];
    short *at = &open_at[y][0];
    int x, d;

    if (mrow) {
        memset(h, 0, BOARD_W);
        foe_row[y] = 0;
    }
    for (x = 0; x < BOARD_W; x++) {
        u8 v = ai_code[in[x]];
        out[x] = v;
        at[x] = -1;
        if (!mrow) continue;
        if (v == 0) {
            if (mrow[x]) {
                open_x[n_open] = (u8)x;
                open_y[n_open] = (u8)y;
                at[x] = (short)n_open;
                n_open++;
            }
        } else if (v == foe) {
            foe_row[y] = 1;
            for (d = -3; d <= 3; d++) {
                u8 *c = &h[wrap_col[x + AI_PAD + d] - 1];
                u8 w = (u8)(4 - (d < 0 ? -d : d));
                if (*c < w) *c = w;
            }
        }
    }
}

static int pull_of(int x, int y)
{
    const u8 *yr = &wrap_row[y + AI_PAD];
    int d, best = 0;
    for (d = -3; d <= 3; d++) {
        int yy = yr[d], h, lim;
        if (!foe_row[yy]) continue;
        h = foe_h[yy][x];
        lim = 4 - (d < 0 ? -d : d);
        if (h > lim) h = lim;
        if (h > best) best = h;
    }
    return best;
}

/* ---- générations précalculées ----

   Lignes et colonnes où g1/s1 (need1) et g2/s2 (need2) sont nécessaires :
   dilatation des lignes et colonnes des candidats retenus. Leur produit
   couvre largement les carrés utiles. */
static u8  need_row1[BOARD_H], need_row2[BOARD_H];
static u8  need_col1[BOARD_W], need_col2[BOARD_W];
static u8  cand_row[BOARD_H], cand_col[BOARD_W];
static u8  cols1[BOARD_W], cols2[BOARD_W];
static int ncols1, ncols2;

/* out[i] = 1 si in contient un 1 à distance circulaire r ou moins de i,
   sur n cases ; `wrap` replie les indices de -AI_PAD à n + AI_PAD - 1
   (wrap_row, ou wrap_col décalé d'une case pour les colonnes). Fenêtre
   glissante : deux lectures par case, quel que soit r. */
static void dilate(const u8 *in, u8 *out, int n, int r, const u8 *wrap, int shift)
{
    int i, d, cnt = 0;
    for (d = -r; d <= r; d++) cnt += in[wrap[d + AI_PAD] - shift];
    for (i = 0; i < n; i++) {
        out[i] = (u8)(cnt > 0);
        cnt += in[wrap[i + r + 1 + AI_PAD] - shift];
        cnt -= in[wrap[i - r + AI_PAD] - shift];
    }
}

/* Lignes, puis colonnes, où les générations sont nécessaires autour des
   top premiers candidats : jusqu'à 2 * depth - 1 pour g1 (le tick 2
   « avec » lit g1 et s2 au rayon 2, et s2 lit g1 un cran plus loin),
   jusqu'à 2 pour g2 à la profondeur 2. Deux appels, pour étaler le
   travail. */
static void need_rows(int top, int depth)
{
    int i;
    memset(cand_row, 0, sizeof cand_row);
    for (i = 0; i < top; i++) cand_row[cands[i].y] = 1;
    dilate(cand_row, need_row1, BOARD_H, 2 * depth - 1, wrap_row, 0);
    if (depth > 1) dilate(cand_row, need_row2, BOARD_H, 2, wrap_row, 0);
    else memset(need_row2, 0, sizeof need_row2);
}

static void need_cols(int top, int depth)
{
    int i, x;
    memset(cand_col, 0, sizeof cand_col);
    for (i = 0; i < top; i++) cand_col[cands[i].x] = 1;
    dilate(cand_col, need_col1, BOARD_W, 2 * depth - 1, wrap_col, 1);
    if (depth > 1) dilate(cand_col, need_col2, BOARD_W, 2, wrap_col, 1);
    else memset(need_col2, 0, sizeof need_col2);
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
static const signed char k_nb[8] = { -6, -5, -4, -1, 1, 4, 5, 6 };

static int t2[25];     /* écarts reportés sur les sommes de voisins du tick 2 */
static u8  w1[25];     /* tick 1 « avec », pour les cases du rayon 1 qui ont changé */
static u8  chg[25];    /* 1 : case du rayon 1 changée au tick 1 */
static u8  hit[25];    /* 1 : case déjà dans la liste `touched` */
static u8  touched[25];

/* +1 pour `mine`, -1 pour l'autre couleur, 0 pour une case vide. */
#define AI_SCORE(v, mine) ((v) == (mine) ? 1 : ((v) ? -1 : 0))

static int ev_nt;    /* cases dans `touched` */
static int ev_net;   /* gain net accumulé */

/* eval_delta() suppose t2, chg et hit à zéro en entrée, et les y remet en
   sortie pour les seules cases qu'il a touchées : il faut donc les vider
   une fois avant la première évaluation. La RAM de la console n'est pas
   remise à zéro au démarrage (docs/snes-notes.md § 10). w1 et touched ne
   sont lus qu'aux cases marquées par chg ou comptées par ev_nt. */
static void eval_scratch_clear(void)
{
    memset(t2, 0, sizeof t2);
    memset(chg, 0, sizeof chg);
    memset(hit, 0, sizeof hit);
}

/* La case k du rayon 1 passe de g (sans la pose) à v (avec) au tick 1. */
static void r1_changed(int k, u8 v, u8 g, u8 mine, int depth)
{
    int e, d;
    if (depth == 1) {
        ev_net += AI_SCORE(v, mine) - AI_SCORE(g, mine);
        return;
    }
    w1[k] = v;
    chg[k] = 1;
    if (!hit[k]) { hit[k] = 1; touched[ev_nt++] = (u8)k; }
    d = (int)v - (int)g;
    for (e = 0; e < 8; e++) {
        int n = k + k_nb[e];
        t2[n] += d;
        if (!hit[n]) { hit[n] = 1; touched[ev_nt++] = (u8)n; }
    }
}

/* Une case du rayon 1, à la position de stockage OFF ; CENTER : c'est la
   pose elle-même. Déroulé neuf fois : sur la console, une boucle avec
   tables d'indices coûte plus cher que le calcul qu'elle répète. */
#define AI_R1(K, OFF, CENTER) do {                                        \
        int off_ = (OFF);                                                 \
        u8 self_ = (CENTER) ? mine : wkb[off_];                           \
        int sum_ = s1p[off_] + ((CENTER) ? 0 : bump);                     \
        u8 v_ = AI_RULE(self_, sum_);                                     \
        u8 g_ = g1p[off_];                                                \
        if (v_ != g_) r1_changed((K), v_, g_, mine, depth);               \
    } while (0)

/* Gain net de la pose du code `mine` en (x, y), g1/s1 (et g2/s2 à la
   profondeur 2) étant à jour autour de la case. */
static int eval_delta(int x, int y, u8 mine, int depth)
{
    const u8  *cx = &wrap_col[x + AI_PAD];
    const int *ry = &row_off[y + AI_PAD];
    const u8  *wkb = &wk.c[0][0];
    const u8  *g1p = &g1b.c[0][0];
    const u8  *s1p = &s1b.c[0][0];
    int ra = ry[-1], rb = ry[0], rc = ry[1];
    int ca = cx[-1], cb = cx[0], cc = cx[1];
    /* Ce que la pose ajoute à la somme de ses huit voisins (la case est
       vide pour un vrai candidat, mais ai_eval_local() accepte tout). */
    int bump = (int)mine - (int)wkb[rb + cb];
    int q;

    ev_nt = 0;
    ev_net = 0;

    /* Tick 1 « avec » : la pose change la somme de ses huit voisins et
       devient elle-même vivante ; hors du rayon 1 rien ne change. Chaque
       case qui change reporte son écart sur ses huit voisines. */
    AI_R1(6,  ra + ca, 0);
    AI_R1(7,  ra + cb, 0);
    AI_R1(8,  ra + cc, 0);
    AI_R1(11, rb + ca, 0);
    AI_R1(12, rb + cb, 1);
    AI_R1(13, rb + cc, 0);
    AI_R1(16, rc + ca, 0);
    AI_R1(17, rc + cb, 0);
    AI_R1(18, rc + cc, 0);
    if (depth == 1) return ev_net;

    /* Tick 2 « avec », sur les seules cases touchées ; puis remise à zéro
       des tableaux de travail pour le candidat suivant. */
    {
        const u8 *g2p = &g2b.c[0][0];
        const u8 *s2p = &s2b.c[0][0];
        for (q = 0; q < ev_nt; q++) {
            int k = touched[q];
            int off = ry[k_dy[k]] + cx[k_dx[k]];
            int sum = s2p[off] + t2[k];
            u8 self = chg[k] ? w1[k] : g1p[off];
            u8 v = AI_RULE(self, sum), g = g2p[off];
            if (v != g) ev_net += AI_SCORE(v, mine) - AI_SCORE(g, mine);
            t2[k] = 0;
            chg[k] = 0;
            hit[k] = 0;
        }
    }
    return ev_net;
}

int ai_eval_local(const Board *b, Cell who, int x, int y, int depth)
{
    int k;

    tables_init();
    eval_scratch_clear();
    for (k = 0; k < BOARD_H; k++) code_row(b, k, 0, (const u8 *)0);
    board_wrap(&wk);

    cands[0].x = (u8)x;
    cands[0].y = (u8)y;
    need_rows(1, depth);
    need_cols(1, depth);
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

static int scan_index(const Cand *c) { return (int)c->y * BOARD_W + (int)c->x; }

/* Heuristique de présélection de la case ouverte q : 2 * voisins vivants
   + attraction, ou 0 si elle n'est pas candidate (occupée par une pose du
   tour, ou sans voisin : posée dans le vide, elle meurt sans rien
   produire). */
static int cand_pre(int q, u8 foe)
{
    int x = open_x[q], y = open_y[q];
    /* Coin haut-gauche du voisinage, dans le halo : lignes de stockage y
       à y + 2, contiguës. */
    const u8 *p = &wk.c[0][0] + row_off[y + AI_PAD] - BSTRIDE + x;
    unsigned int sum, n1, n2, nf;

    if (p[BSTRIDE + 1] != 0) return 0;
    sum = (unsigned int)AI_SUM8(p);
    if (sum == 0) return 0;
    n1 = sum & (AI_E2 - 1);
    n2 = sum >> 4;            /* AI_E2 = 16 */
    nf = (foe == AI_E1) ? n1 : n2;
    /* Un adversaire voisin : distance 1, attraction maximale. */
    return 2 * (int)(n1 + n2) + (nf ? 3 : pull_of(x, y));
}

/* Examine jusqu'à AI_COLLECT_CHUNK cases de la liste des cases ouvertes à
   partir de j->pos, et ajoute à all_c (à partir de j->n) les candidates.
   La liste est dans l'ordre de balayage (y croissant puis x croissant), et
   c'est cet ordre qui sert de départage. Au plafond AI_MAX_CANDS, la
   collecte s'arrête là. */
static void collect_chunk(AiJob *j)
{
    int n = j->n, q = j->pos, end = q + AI_COLLECT_CHUNK;
    u8 f = ai_code[j->foe];

    if (end > n_open) end = n_open;
    for (; q < end; q++) {
        int pre = cand_pre(q, f);
        if (pre == 0) continue;
        if (n >= AI_MAX_CANDS) {
            q = n_open;
            break;
        }
        all_c[n].x = open_x[q];
        all_c[n].y = open_y[q];
        all_c[n].pre = (short)pre;
        n++;
    }
    j->n = n;
    j->pos = q;
}

static int scan_of(int x, int y) { return y * BOARD_W + x; }

/* Le carré 3 x 3 d'une pose, par tiers : sa ligne (centre en premier),
   celle du dessus, celle du dessous. */
static const signed char k_upd_dx[9] = { 0, -1, 1, -1, 0, 1, -1, 0, 1 };
static const signed char k_upd_dy[9] = { 0, 0, 0, -1, -1, -1, 1, 1, 1 };

/* Après une pose en (px, py), seules les cases de son carré 3 x 3 changent
   de statut : elle-même (occupée) et ses voisines (une voisine vivante de
   plus). all_c est mise à jour en place, dans l'ordre de balayage, un
   tiers du carré par appel (`part` 0, 1 puis 2). Même résultat qu'une
   collecte complète, pourvu que la précédente n'ait pas été tronquée au
   plafond (l'appelant s'en assure). */
static void collect_update(AiJob *j, int px, int py, int part)
{
    u8 f = ai_code[j->foe];
    int k, n = j->n;

    /* La case posée d'abord : c'est la seule qui peut sortir de la liste
       (ses voisines gagnent un voisin vivant, elles ne la quittent pas).
       La retirer avant toute insertion garantit qu'une insertion qui
       déborde du plafond ne fait tomber que ce qu'une collecte complète
       aurait laissé de côté. */
    for (k = 3 * part; k < 3 * part + 3; k++) {
        int y = wrap_row[py + AI_PAD + k_upd_dy[k]];
        int x = wrap_col[px + AI_PAD + k_upd_dx[k]] - 1;
        {
            int q = open_at[y][x], pre, lo, hi, key;
            if (q < 0) continue;
            pre = cand_pre(q, f);
            /* Position de (x, y) dans all_c : première entrée qui ne la
               précède pas. */
            key = scan_of(x, y);
            lo = 0;
            hi = n;
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                if (scan_of(all_c[mid].x, all_c[mid].y) < key) lo = mid + 1;
                else hi = mid;
            }
            if (lo < n && scan_of(all_c[lo].x, all_c[lo].y) == key) {
                if (pre) {
                    all_c[lo].pre = (short)pre;
                } else {
                    int q;
                    /* Décalage à la main : le memmove de la bibliothèque
                       de la console ne gère pas les zones qui se
                       recouvrent (docs/snes-notes.md § 10). */
                    for (q = lo; q < n - 1; q++) all_c[q] = all_c[q + 1];
                    n--;
                }
            } else if (pre) {
                if (n >= AI_MAX_CANDS) {
                    if (lo >= AI_MAX_CANDS) continue;   /* au-delà du plafond */
                    n = AI_MAX_CANDS - 1;               /* la dernière tombe */
                }
                {
                    int q;
                    for (q = n; q > lo; q--) all_c[q] = all_c[q - 1];
                }
                all_c[lo].x = (u8)x;
                all_c[lo].y = (u8)y;
                all_c[lo].pre = (short)pre;
                n++;
            }
        }
    }
    j->n = n;
}

/* `pre` vaut 2 * voisins (1 à 8) + attraction (0 à 3) : de 2 à 19. */
#define AI_PRE_SPAN   (2 * 8 + 3 + 1)

static int  pre_pos[AI_PRE_SPAN];

/* Copie dans cands les k meilleurs de all_c, triés par `pre` décroissant
   puis par ordre de balayage : un tri par dénombrement, stable, tronqué à
   k. Même résultat qu'un tri par sélection sur cette clé totale (all_c
   est dans l'ordre de balayage), en deux passages sur n. */
static void select_top(int n, int k)
{
    int i, p, pos = 0;

    for (p = 0; p < AI_PRE_SPAN; p++) pre_pos[p] = 0;
    for (i = 0; i < n; i++) pre_pos[all_c[i].pre]++;
    for (p = AI_PRE_SPAN - 1; p >= 0; p--) {
        int c = pre_pos[p];
        pre_pos[p] = pos;
        pos += c;
    }
    for (i = 0; i < n; i++) {
        p = all_c[i].pre;
        if (pre_pos[p] < k) {
            cands[pre_pos[p]] = all_c[i];
        }
        pre_pos[p]++;
    }
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

/* Tous les candidats sont connus : garder les k meilleurs, puis repérer
   les lignes et colonnes où les générations sont nécessaires. */
static void collect_finish(AiJob *j)
{
    if (j->pos == 0) {
        if (j->n > 0) {
            select_top(j->n, j->k);
        }
        j->top = (j->n < j->k) ? j->n : j->k;
        j->i = 0;
        need_rows(j->top, j->depth);
        j->pos = 1;
        return;
    }
    need_cols(j->top, j->depth);
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
    /* Tout ce que la suite lit sans l'avoir écrit elle-même est posé ici,
       à chaque tour : rien ne dépend de l'état initial de la RAM. */
    tables_init();
    eval_scratch_clear();
}

/* Coût de la prochaine étape, en unités de budget (ai.h). */
static int step_cost(const AiJob *j)
{
    switch (j->phase) {
    case AI_PH_PREP:    return AI_COST_PREP;
    case AI_PH_COLLECT: return AI_COST_COLLECT;
    case AI_PH_UPDATE:  return AI_COST_UPDATE;
    case AI_PH_SELECT:  return AI_COST_SELECT;
    case AI_PH_GEN1:    return AI_COST_GEN_ROW + AI_COST_GEN_2COLS * ncols1 / 2;
    case AI_PH_GEN2:    return AI_COST_GEN_ROW + AI_COST_GEN_2COLS * ncols2 / 2;
    default:            return (j->i < j->top) ? AI_COST_EVAL : AI_COST_PICK;
    }
}

bool_t ai_step(AiJob *j, const Match *m, Rng *rng, int budget)
{
    int spent = 0;

    for (;;) {
        int pick, i, cost = step_cost(j);

        /* La première étape se fait toujours ; les suivantes seulement si
           elles tiennent dans ce qui reste du budget (écrit sans somme :
           spent + cost dépasserait 32 767 avec le budget d'ai_choose() et
           un `int` de 16 bits). */
        if (spent > 0 && cost > budget - spent) {
            return FALSE;
        }
        spent += cost;

        switch (j->phase) {
        case AI_PH_PREP:
            /* Copie codée du plateau, ligne par ligne, en notant où sont
               les adversaires. m->board n'a pas forcément son halo à jour
               (match_place() ne wrappe pas) : wk est wrappé à la fin. */
            code_row(&m->board, j->pos, ai_code[j->foe], &m->range_mask[j->pos][0]);
            j->pos++;
            if (j->pos >= BOARD_H) {
                board_wrap(&wk);
                collect_start(j);
            }
            continue;

        case AI_PH_COLLECT:
            collect_chunk(j);
            if (j->pos >= n_open) {
                j->phase = AI_PH_SELECT;
                j->pos = 0;
            }
            continue;

        case AI_PH_UPDATE:
            collect_update(j, (int)j->out[j->made - 1].x, (int)j->out[j->made - 1].y,
                           j->pos);
            if (++j->pos >= 3) {
                j->phase = AI_PH_SELECT;
                j->pos = 0;
            }
            continue;

        case AI_PH_SELECT:
            collect_finish(j);
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
            rng_next(rng);
            pick = (int)rng_mod(rng, (unsigned int)pool);
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
        /* La pose suivante repart de la liste des candidats, mise à jour
           autour de celle-ci ; sauf si la liste avait atteint le plafond,
           auquel cas elle peut être incomplète : collecte complète. */
        if (j->n < AI_MAX_CANDS) {
            j->phase = AI_PH_UPDATE;
            j->pos = 0;
            j->top = 0;
            j->i = 0;
        } else {
            collect_start(j);
        }
    }
}

int ai_choose(const Match *m, AiLevel lvl, Rng *rng, Move out[BUDGET])
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

#ifdef AI_TEST_HOOKS
void ai_test_poison(u8 v)
{
    memset(&wk, v, sizeof wk);
    memset(&g1b, v, sizeof g1b);
    memset(&s1b, v, sizeof s1b);
    memset(&g2b, v, sizeof g2b);
    memset(&s2b, v, sizeof s2b);
    memset(scores, v, sizeof scores);
    memset(all_c, v, sizeof all_c);
    memset(cands, v, sizeof cands);
    memset(wrap_col, v, sizeof wrap_col);
    memset(wrap_row, v, sizeof wrap_row);
    memset(row_off, v, sizeof row_off);
    memset(rule_surv, v, sizeof rule_surv);
    memset(rule_birth, v, sizeof rule_birth);
    memset(open_x, v, sizeof open_x);
    memset(open_y, v, sizeof open_y);
    memset(&n_open, v, sizeof n_open);
    memset(open_at, v, sizeof open_at);
    memset(foe_h, v, sizeof foe_h);
    memset(foe_row, v, sizeof foe_row);
    memset(need_row1, v, sizeof need_row1);
    memset(need_row2, v, sizeof need_row2);
    memset(need_col1, v, sizeof need_col1);
    memset(need_col2, v, sizeof need_col2);
    memset(cand_row, v, sizeof cand_row);
    memset(cand_col, v, sizeof cand_col);
    memset(cols1, v, sizeof cols1);
    memset(cols2, v, sizeof cols2);
    memset(&ncols1, v, sizeof ncols1);
    memset(&ncols2, v, sizeof ncols2);
    memset(t2, v, sizeof t2);
    memset(w1, v, sizeof w1);
    memset(chg, v, sizeof chg);
    memset(hit, v, sizeof hit);
    memset(touched, v, sizeof touched);
    memset(&ev_nt, v, sizeof ev_nt);
    memset(&ev_net, v, sizeof ev_net);
    memset(pre_pos, v, sizeof pre_pos);
}
#endif
