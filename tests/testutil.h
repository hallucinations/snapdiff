#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <stdio.h>
#include <stdlib.h>

static int _tests_run    = 0;
static int _tests_failed = 0;

#define ASSERT(cond) do {                                          \
    _tests_run++;                                                  \
    if (!(cond)) {                                                 \
        fprintf(stderr, "FAIL  %s:%d  %s\n",                      \
                __FILE__, __LINE__, #cond);                        \
        _tests_failed++;                                           \
    }                                                              \
} while (0)

#define ASSERT_EQ(a, b)  ASSERT((a) == (b))
#define ASSERT_NEQ(a, b) ASSERT((a) != (b))
#define ASSERT_STR_EQ(a, b) do {                                   \
    _tests_run++;                                                  \
    if (strcmp((a), (b)) != 0) {                                   \
        fprintf(stderr, "FAIL  %s:%d  \"%s\" != \"%s\"\n",        \
                __FILE__, __LINE__, (a), (b));                     \
        _tests_failed++;                                           \
    }                                                              \
} while (0)

#define TEST_SUMMARY() do {                                        \
    if (_tests_failed == 0)                                        \
        printf("PASS  %d/%d tests passed\n",                       \
               _tests_run, _tests_run);                            \
    else                                                           \
        printf("FAIL  %d/%d tests failed\n",                       \
               _tests_failed, _tests_run);                         \
    return _tests_failed ? 1 : 0;                                  \
} while (0)

#endif
