/*
 * check.h - minimal unit test helpers shared by the tests in tests/unit/.
 *
 * CHECK records a failure and carries on, so one run shows every broken
 * check. Each test program ends with `return check_report("name");`.
 */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <string.h>

static int check_failures;
static int check_count;

#define CHECK(cond)                                                       \
    do {                                                                  \
        check_count++;                                                    \
        if (!(cond)) {                                                    \
            check_failures++;                                             \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                 \
    } while (0)

#define CHECK_STR(got, want)                                              \
    do {                                                                  \
        check_count++;                                                    \
        if (strcmp((got), (want)) != 0) {                                 \
            check_failures++;                                             \
            printf("  FAIL %s:%d: got \"%s\", want \"%s\"\n", __FILE__,   \
                   __LINE__, (got), (want));                              \
        }                                                                 \
    } while (0)

static int check_report(const char *name)
{
    printf("%-14s %d checks, %d failed\n", name, check_count, check_failures);
    return check_failures == 0 ? 0 : 1;
}

#endif /* CHECK_H */
