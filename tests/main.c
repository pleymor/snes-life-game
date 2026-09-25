#include "harness.h"

void suite_board(void);
void suite_life(void);

int main(void)
{
    suite_board();
    suite_life();
    return t_report();
}
