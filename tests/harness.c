#include "harness.h"

int t_checks = 0;
int t_fails = 0;
const char *t_current = "";

int t_report(void)
{
    printf("%d vérifications, %d échecs\n", t_checks, t_fails);
    return t_fails ? 1 : 0;
}
