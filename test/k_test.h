#ifndef K_TEST_H
#define K_TEST_H

/*
 * Tiny host-only test harness. stdlib-only, ~header-only, no external framework.
 * Never compiled into the shipped library.
 */
#include <stdio.h>
#include <math.h>

static int kt_checks = 0;
static int kt_fails = 0;

#define KT_RUN(fn) do {                                        \
    printf("-- %s\n", #fn);                                     \
    fn();                                                       \
} while (0)

#define KT_ASSERT(cond) do {                                   \
    kt_checks++;                                                \
    if (!(cond)) {                                             \
        kt_fails++;                                             \
        printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
    }                                                          \
} while (0)

#define KT_ASSERT_EQ_INT(a, b) do {                            \
    kt_checks++;                                                \
    if ((long)(a) != (long)(b)) {                             \
        kt_fails++;                                            \
        printf("  FAIL %s:%d: %s == %s (%ld vs %ld)\n",       \
               __FILE__, __LINE__, #a, #b, (long)(a), (long)(b));\
    }                                                          \
} while (0)

#define KT_ASSERT_NEAR(a, b, eps) do {                         \
    kt_checks++;                                                \
    double _da = (double)(a), _db = (double)(b);              \
    if (fabs(_da - _db) > (double)(eps)) {                    \
        kt_fails++;                                            \
        printf("  FAIL %s:%d: %s ~= %s (%g vs %g, eps %g)\n", \
               __FILE__, __LINE__, #a, #b, _da, _db, (double)(eps));\
    }                                                          \
} while (0)

#define KT_SUMMARY() (                                         \
    printf("\n%s: %d checks, %d failures\n",                  \
           kt_fails ? "FAILED" : "PASSED", kt_checks, kt_fails), \
    (kt_fails ? 1 : 0))

#endif /* K_TEST_H */
