#include "harness.h"

void suite_board(void);
void suite_life(void);
void suite_rules(void);
void suite_match(void);

int main(void)
{
    suite_board();
    suite_life();
    suite_rules();
    suite_match();
    return t_report();
}
