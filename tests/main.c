#include "harness.h"

void suite_board(void);
void suite_life(void);
void suite_rules(void);

int main(void)
{
    suite_board();
    suite_life();
    suite_rules();
    return t_report();
}
