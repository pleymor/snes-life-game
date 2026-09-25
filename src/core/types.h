#ifndef TYPES_H
#define TYPES_H

typedef unsigned char u8;

/* 816-tcc ne fournit pas <stdbool.h>. */
typedef unsigned char bool_t;
#define TRUE  1
#define FALSE 0

typedef enum { CELL_EMPTY = 0, CELL_P1 = 1, CELL_P2 = 2 } Cell;

typedef struct { u8 x, y; } Move;

typedef enum { WINNER_NONE = 0, WINNER_P1, WINNER_P2, WINNER_DRAW } Winner;

#endif
