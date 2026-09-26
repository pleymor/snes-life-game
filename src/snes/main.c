/*---------------------------------------------------------------------------------

    Task 0 spike, extended by task 8 (board on BG1), task 9 (HUD on BG2,
    cursor sprite) and task 10 (controls, two-player game loop).

    render_init()/render_board_now()/render_hud_now()/render_cursor()
    (src/snes/render.c) own every PVSnesLib call this file used to make
    directly (mode select, tileset + palette load, DMA, OAM) — see
    docs/snes-notes.md for the exact, verified sequence. input_init()/
    input_update() (src/snes/input.c) own the pad. This file only drives the
    game loop.

    VBlank discipline: render_board_now()/render_hud_now()/render_cursor()
    only prepare buffers; render_vblank() (called after WaitForVBlank(),
    never before) performs the actual DMA transfers. The loop below is
    exactly prepare -> WaitForVBlank() -> render_vblank(), never more than
    one wait per frame.

    Do not rewrite this file wholesale in later tasks: extend it.

---------------------------------------------------------------------------------*/
#include <snes.h>
#include "match.h"
#include "render.h"
#include "input.h"
#include "ai.h"

typedef enum { GS_TURN, GS_RESOLVE, GS_OVER } GameState;

#define RESOLVE_HOLD 45   /* frames de pause pour voir le résultat du tick */

/* A Board is 884 bytes; Match holds two of them. The SNES stack is tiny, so
   this — and Cursor, trivially small but kept alongside for the same
   reason — lives in static storage, never on the stack. */
static Match  m;
static Cursor cur;

/* -1 pour un second joueur humain ; sinon AI_EASY ou AI_NORMAL. Tâche 12 la
   renseigne depuis un menu ; ici elle est fixée en dur (task 11). */
static int cpu_level;
static unsigned long rng;

/* Le tour du CPU est étalé sur plusieurs frames (spec § 6.3) : un pas
   d'ai_step() par itération de la boucle, entre deux WaitForVBlank(), pour
   que le HUD (pastille clignotante) continue de s'animer. `job` et
   `cpu_thinking` survivent d'une frame à l'autre (portée fichier, comme
   `m`/`cur` ci-dessus), le temps que le CPU termine son tour.

   Tout chemin qui quitterait GS_TURN au milieu d'une réflexion (menu de
   pause, retour au titre, nouvelle partie...) doit remettre cpu_thinking à
   FALSE : sinon le tour suivant du CPU reprendrait un AiJob périmé au
   lieu d'appeler ai_begin(). */
static AiJob  job;
static bool_t cpu_thinking;

/* Budget d'un pas d'ai_step(), dans les unités des AI_COST_... d'ai.h (une
   unité vaut environ un centième de frame, mesuré sur la console,
   docs/snes-notes.md § 9). 90 laisse un dixième de la frame au reste de
   l'itération (HUD, curseur). Mesuré : 171 frames pour 159 itérations sur
   le premier tour du CPU du script ; les dépassements viennent des
   étapes qui coûtent à elles seules plus d'une frame (sélection) ou plus
   que leur moyenne (un candidat entouré de changements). 100 donne un
   tour plus long. */
#define AI_STEP_BUDGET 90

#ifdef AI_MEASURE_FRAMES
/* Build de mesure seulement (`make rom-measure`, jamais `make rom`) : durée
   du dernier tour du CPU, de ai_begin() jusqu'au ai_step() qui rend TRUE,
   en VBlanks réels (snes_vblank_count, incrémenté par le gestionnaire NMI
   de PVSnesLib quoi que fasse la boucle), et nombre d'itérations de la
   boucle pendant ce tour. Les deux s'affichent sur quatre chiffres à la
   place du compteur de rounds « R001/040 » : frames en colonnes 12-15,
   itérations en colonnes 16-19. Des frames égales aux itérations à peu de
   chose près montrent qu'une itération tient dans une frame. */
static u16          meas_start;
static unsigned int meas_iters;
static unsigned int meas_last_frames;
static unsigned int meas_last_iters;
static unsigned int meas_shown_frames;
static unsigned int meas_shown_iters;
#endif

/* The small piece of visible state that decides whether the board/HUD need
   rebuilding this frame (task 10 ruling): m.turn, m.round, m.placed,
   m.winner, cur.show_range, and the game state itself (GS_RESOLVE hides the
   board's range marks; returning to GS_TURN shows them again, so the state
   value belongs in the comparison too). Cursor position is deliberately
   excluded: render_cursor() runs unconditionally every frame regardless of
   board_dirty, and the range marks it might overlap do not depend on where
   the cursor sits. */
typedef struct {
    Cell      turn;
    int       round;
    int       placed;
    Winner    winner;
    bool_t    show_range;
    GameState state;
} Snapshot;

static void snapshot_take(Snapshot *s, GameState state)
{
    s->turn       = m.turn;
    s->round      = m.round;
    s->placed     = m.placed;
    s->winner     = m.winner;
    s->show_range = cur.show_range;
    s->state      = state;
}

static bool_t snapshot_differs(const Snapshot *a, const Snapshot *b)
{
    return (bool_t)(a->turn       != b->turn       ||
                     a->round      != b->round      ||
                     a->placed     != b->placed     ||
                     a->winner     != b->winner     ||
                     a->show_range != b->show_range ||
                     a->state      != b->state);
}

//---------------------------------------------------------------------------------
int main(void)
{
    GameState state = GS_TURN;
    /* unsigned : a plain (signed) `int` would overflow undefined behaviour
       past 32767 on the 16-bit `int` of the 65816 target (fix round 1,
       task 8 review finding — carried over here). */
    unsigned int frame = 0;
    int hold = 0;
    /* Forces the very first frame to be seen as "changed": render.c's own
       map_bg1 buffer starts at TILE_EMPTY and is only ever filled by
       render_board_now(), which the snapshot comparison below would
       otherwise never call before any visible state actually changes. */
    bool_t started = FALSE;

    /* Toutes les variables statiques de ce fichier sont posées ici, sans
       compter ni sur un initialiseur ni sur une mise à zéro : la RAM de la
       console n'est pas remise à zéro au démarrage (docs/snes-notes.md
       § 10). m, cur et job le sont par match_start(), input_init() et
       ai_begin(). */
    cpu_level = AI_NORMAL;
    /* `long` fait 16 bits pour 816-tcc (docs/snes-notes.md § 10) : seuls
       les 16 bits bas de cette graine comptent sur la console. */
    rng = 0x2545F491UL;
    cpu_thinking = FALSE;
#ifdef AI_MEASURE_FRAMES
    meas_start = 0;
    meas_iters = 0;
    meas_last_frames = 0;
    meas_last_iters = 0;
    meas_shown_frames = 0;
    meas_shown_iters = 0;
#endif
    render_init();
    match_start(&m);
    input_init(&cur);

    for (;;) {
        Snapshot before, after;
        bool_t ticked = FALSE;
        bool_t dirty;

        snapshot_take(&before, state);

        if (state == GS_TURN) {
            if (cpu_level >= 0 && m.turn == CELL_P2) {
                if (!cpu_thinking) {
#ifdef AI_MEASURE_FRAMES
                    meas_start = snes_vblank_count;
                    meas_iters = 0;
#endif
                    ai_begin(&job, &m, (AiLevel)cpu_level);
                    cpu_thinking = TRUE;
                }
#ifdef AI_MEASURE_FRAMES
                meas_iters++;
#endif
                /* Un pas par itération, budget borné (AI_STEP_BUDGET) : le
                   HUD continue de s'animer pendant que le CPU réfléchit. */
                if (ai_step(&job, &m, &rng, AI_STEP_BUDGET)) {
                    int i;
#ifdef AI_MEASURE_FRAMES
                    meas_last_frames = (unsigned int)(u16)(snes_vblank_count - meas_start);
                    meas_last_iters = meas_iters;
#endif
                    for (i = 0; i < job.made; i++) {
                        match_place(&m, (int)job.out[i].x, (int)job.out[i].y);
                    }
                    cpu_thinking = FALSE;
                    ticked = TRUE;
                    match_end_turn(&m);
                    if (match_winner(&m) != WINNER_NONE) {
                        state = GS_OVER;
                    } else {
                        state = GS_RESOLVE;
                        hold = RESOLVE_HOLD;
                    }
                }
            } else if (input_update(&cur, &m)) {
                /* Un tick n'a lieu qu'à la fin du tour du second joueur. */
                ticked = (bool_t)(m.turn == CELL_P2);
                match_end_turn(&m);
                if (match_winner(&m) != WINNER_NONE) {
                    state = GS_OVER;
                } else if (ticked) {
                    state = GS_RESOLVE;
                    hold = RESOLVE_HOLD;
                }
            }
        } else if (state == GS_RESOLVE) {
            if (--hold <= 0) state = GS_TURN;
        }

        snapshot_take(&after, state);
        dirty = (bool_t)(!started || snapshot_differs(&before, &after));
        started = TRUE;

        /* Range marks show only during GS_TURN, and only when the cursor's
           own toggle asks for them; both conditions are already part of
           `dirty` above (via `state` and `show_range`), so a change in
           either always reaches render_board_now() with the right value. */
        if (dirty) {
            render_board_now(&m, (bool_t)(state == GS_TURN && cur.show_range));
            render_hud_dirty();
        }
        render_hud_now(&m, (bool_t)((frame & 16) != 0));
#ifdef AI_MEASURE_FRAMES
        /* Seulement quand le bandeau vient d'être reconstruit (il efface
           ces chiffres) ou qu'une valeur a changé : les divisions par 10
           coûtent cher sur la console et fausseraient la mesure. */
        if (dirty || meas_last_frames != meas_shown_frames ||
            meas_last_iters != meas_shown_iters) {
            render_hud_number4(12, meas_last_frames);
            render_hud_number4(16, meas_last_iters);
            meas_shown_frames = meas_last_frames;
            meas_shown_iters = meas_last_iters;
        }
#endif
        /* Curseur masqué pendant que le CPU réfléchit (task 11) : sa
           position resterait celle du dernier tour humain, sans rapport
           avec le tour du CPU en cours. */
        render_cursor(cur.x, cur.y, (bool_t)(state == GS_TURN && !cpu_thinking));

        frame++;
        WaitForVBlank();
        render_vblank();
    }
    return 0;
}
