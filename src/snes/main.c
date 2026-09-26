/*---------------------------------------------------------------------------------

    Task 0 spike, extended by task 8 (board on BG1), task 9 (HUD on BG2,
    cursor sprite), task 10 (controls, two-player game loop), task 11 (CPU
    turns) and task 12 (mode menu, result screen): the game now boots to a
    menu, plays, shows the result, and returns to the menu, forever.

    render_init()/render_board_now()/render_hud_now()/render_cursor()
    (src/snes/render.c) own every PVSnesLib call this file used to make
    directly (mode select, tileset + palette load, DMA, OAM) — see
    docs/snes-notes.md for the exact, verified sequence. input_init()/
    input_update()/input_reset() (src/snes/input.c) own the pad.
    screen_menu()/screen_result() (src/snes/screens.c) own the mode menu and
    result screen. This file only drives the game loop itself, bracketed
    by those two screens in an outer, never-exiting loop.

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
#include "screens.h"
#include "ai.h"

typedef enum { GS_TURN, GS_RESOLVE, GS_OVER } GameState;

#define RESOLVE_HOLD 45   /* frames de pause pour voir le résultat du tick */

/* A Board is 884 bytes; Match holds two of them. The SNES stack is tiny, so
   this — and Cursor, trivially small but kept alongside for the same
   reason — lives in static storage, never on the stack. */
static Match  m;
static Cursor cur;

/* -1 pour un second joueur humain ; sinon AI_EASY ou AI_NORMAL. Renseignée
   par screen_menu() (src/snes/screens.c) à chaque retour au menu. */
static int cpu_level;
static Rng rng;

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
    /* Initialisations valables pour tout le programme, faites une seule
       fois avant le tout premier écran de menu : render_init() pose l'état
       de render.c (VRAM, mode graphique), input_reset() celui de la
       lecture de manette (et, sous INPUT_SCRIPT, la position dans la table
       de rejeu) — voir input.h. Ni l'une ni l'autre ne doivent être
       répétées à chaque partie : render_init() reprogrammerait le PPU en
       vain, et input_reset() rembobinerait le rejeu scripté au lieu de le
       laisser traverser menu -> partie -> écran de fin -> menu suivant
       comme une seule chronologie continue. */
    render_init();
    input_reset();

    for (;;) {
        GameState state = GS_TURN;
        /* unsigned : a plain (signed) `int` would overflow undefined
           behaviour past 32767 on the 16-bit `int` of the 65816 target
           (fix round 1, task 8 review finding — carried over here). */
        unsigned int frame = 0;
        int hold = 0;
        /* Forces the very first frame of every game to be seen as
           "changed": render.c's own map_bg1/map_bg2 buffers still hold
           whatever screen_menu()/screen_result() last put there, which the
           snapshot comparison below would otherwise never call
           render_board_now()/render_hud_dirty() to replace before some
           visible match state actually changes (task 12 ruling: no
           leftover from the previous game). `state`, `frame` and `hold`
           above are automatic locals re-initialised by this same
           declaration every time this outer loop runs, for the same
           reason. */
        bool_t started = FALSE;

        /* cpu_level, m, cur et job (via cpu_thinking) sont remis à neuf à
           chaque partie, explicitement : la RAM de la console n'est pas
           remise à zéro au démarrage (docs/snes-notes.md § 10), et rien
           ici ne doit garder l'état de la partie précédente. */
        cpu_level = screen_menu();
        /* Graine 0x2545F491, en deux moitiés : `long` ne fait que 16 bits
           sur la console (docs/snes-notes.md § 10). Reposée à chaque
           partie : une IA facile déterministe et reproductible d'une
           partie à l'autre, pas seulement à l'intérieur d'une partie. */
        rng_seed(&rng, 0x2545U, 0xF491U);
        cpu_thinking = FALSE;
#ifdef AI_MEASURE_FRAMES
        meas_start = 0;
        meas_iters = 0;
        meas_last_frames = 0;
        meas_last_iters = 0;
        meas_shown_frames = 0;
        meas_shown_iters = 0;
#endif
        match_start(&m);
        input_init(&cur);
        /* Le bandeau (map_bg2) garde le contenu de l'écran précédent
           (menu ou fin de partie précédente) : forcer sa reconstruction
           dès la première image de la nouvelle partie plutôt que de
           compter implicitement sur `started` ci-dessus. */
        render_hud_dirty();

        while (state != GS_OVER) {
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
                    /* Un pas par itération, budget borné (AI_STEP_BUDGET) :
                       le HUD continue de s'animer pendant que le CPU
                       réfléchit. */
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
                    /* Un tick n'a lieu qu'à la fin du tour du second
                       joueur. */
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

            /* Range marks show only during GS_TURN, and only when the
               cursor's own toggle asks for them; both conditions are
               already part of `dirty` above (via `state` and
               `show_range`), so a change in either always reaches
               render_board_now() with the right value. */
            if (dirty) {
                render_board_now(&m, (bool_t)(state == GS_TURN && cur.show_range));
                render_hud_dirty();
            }
            render_hud_now(&m, (bool_t)((frame & 16) != 0));
#ifdef AI_MEASURE_FRAMES
            /* Seulement quand le bandeau vient d'être reconstruit (il
               efface ces chiffres) ou qu'une valeur a changé : les
               divisions par 10 coûtent cher sur la console et
               fausseraient la mesure. */
            if (dirty || meas_last_frames != meas_shown_frames ||
                meas_last_iters != meas_shown_iters) {
                render_hud_number4(12, meas_last_frames);
                render_hud_number4(16, meas_last_iters);
                meas_shown_frames = meas_last_frames;
                meas_shown_iters = meas_last_iters;
            }
#endif
            /* Curseur masqué pendant que le CPU réfléchit (task 11) : sa
               position resterait celle du dernier tour humain, sans
               rapport avec le tour du CPU en cours. */
            render_cursor(cur.x, cur.y, (bool_t)(state == GS_TURN && !cpu_thinking));

            frame++;
            WaitForVBlank();
            render_vblank();
        }

        /* Partie terminée (state == GS_OVER) : plateau final déjà à
           l'écran depuis la dernière image de la boucle ci-dessus.
           screen_result() prend le relais jusqu'à START, puis ce for(;;)
           reboucle sur un nouveau screen_menu(). */
        screen_result(&m);
    }
    return 0;
}
