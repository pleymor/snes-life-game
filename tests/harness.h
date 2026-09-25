#ifndef HARNESS_H
#define HARNESS_H

#include <stdio.h>

extern int t_checks;
extern int t_fails;
extern const char *t_current;

#define T_RUN(fn) do { t_current = #fn; fn(); } while (0)

#define T_CHECK(cond, msg) do {                                      \
    t_checks++;                                                      \
    if (!(cond)) {                                                   \
        t_fails++;                                                   \
        printf("FAIL %s:%d [%s] %s\n", __FILE__, __LINE__,           \
               t_current, (msg));                                    \
    }                                                                \
} while (0)

#define T_EQ(a, b) do {                                              \
    long ta = (long)(a);                                             \
    long tb = (long)(b);                                             \
    t_checks++;                                                      \
    if (ta != tb) {                                                  \
        t_fails++;                                                   \
        printf("FAIL %s:%d [%s] %s == %s : %ld != %ld\n",            \
               __FILE__, __LINE__, t_current, #a, #b, ta, tb);       \
    }                                                                \
} while (0)

#define T_TRUE(a)  T_CHECK((a), #a " est faux")
#define T_FALSE(a) T_CHECK(!(a), #a " est vrai")

int t_report(void);

#endif
