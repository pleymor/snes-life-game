#ifndef CONFIG_H
#define CONFIG_H

/* Les seuls réglages du jeu. Ne rien mettre d'autre ici, ne les définir
   nulle part ailleurs. Valeurs de départ ; la tâche 6 les arrête sur mesure. */

#define BOARD_W            32
#define BOARD_H            24
#define BUDGET              3   /* poses par tour */
#define RANGE_RADIUS        2   /* distance de Chebyshev */
#define RAMPUP_ROUND       16   /* premier round à deux ticks */
#define TICKS_AFTER_RAMPUP  2
#define ROUND_CAP          40

#endif
