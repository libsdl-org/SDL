/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#ifdef TEST_STDLIB_QSORT
#define _GNU_SOURCE
#include <stdlib.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

typedef struct {
    Uint8 major;
    Uint8 minor;
    Uint8 micro;
} VersionTuple;

static int a_global_var = 77;
static int (SDLCALL * global_compare_cbfn)(const void *_a, const void *_b);

static unsigned long arraylens[16] = { 12, 1024 * 100 };
static unsigned int count_arraylens = 2;

static int SDLCALL
compare_int(const void *_a, const void *_b)
{
    const int a = *((const int *)_a);
    const int b = *((const int *)_b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static int SDLCALL
compare_float(const void *_a, const void *_b)
{
    const float a = *((const float *)_a);
    const float b = *((const float *)_b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static int SDLCALL
compare_double(const void *_a, const void *_b)
{
    const double a = *((const double *)_a);
    const double b = *((const double *)_b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static int SDLCALL
compare_intptr(const void *_a, const void *_b)
{
    const int* a = *((const int **)_a);
    const int* b = *((const int **)_b);
    return compare_int(a, b);
}

static int SDLCALL
compare_version(const void *_a, const void *_b)
{
    const VersionTuple *a = ((const VersionTuple *)_a);
    const VersionTuple *b = ((const VersionTuple *)_b);
    int d;
    d = (int)a->major - (int)b->major;
    if (d != 0) {
        return d;
    }
    d = (int)a->minor - (int)b->minor;
    if (d != 0) {
        return d;
    }
    return (int)a->micro - (int)b->micro;
}

#ifdef TEST_STDLIB_QSORT
#define SDL_qsort qsort
#define SDL_qsort_r qsort_r
#endif

static int SDLCALL
#ifdef TEST_STDLIB_QSORT
generic_compare_r(const void *a, const void *b, void *userdata)
#else
generic_compare_r(void *userdata, const void *a, const void *b)
#endif
{
    if (userdata != &a_global_var) {
        SDLTest_AssertCheck(userdata == &a_global_var, "User data of callback must be identical to global data");
    }
    return global_compare_cbfn(a, b);
}

#define STR2(S) "[" #S "] "
#define STR(S) STR2(S)

#define TEST_ARRAY_IS_SORTED(TYPE, ARRAY, SIZE, IS_LE)                                                    \
    do {                                                                                                  \
        size_t sorted_index;                                                                              \
        Uint64 count_non_sorted = 0;                                                                      \
        for (sorted_index = 0; sorted_index < (SIZE) - 1; sorted_index++) {                               \
            if (!IS_LE((ARRAY)[sorted_index], (ARRAY)[sorted_index + 1])) {                               \
                count_non_sorted += 1;                                                                    \
            }                                                                                             \
        }                                                                                                 \
        SDLTest_AssertCheck(count_non_sorted == 0,                                                        \
            STR(TYPE) "Array (size=%d) is sorted (bad count=%" SDL_PRIu64 ")", (SIZE), count_non_sorted); \
    } while (0)

/* This test is O(n^2), so very slow (a hashmap can speed this up):
 * - we cannot trust qsort
 * - the arrays can contain duplicate items
 * - the arrays are not int
 */
#define TEST_ARRAY_LOST_NO_ELEMENTS(TYPE, ORIGINAL, SORTED, SIZE, IS_SAME)                       \
    do {                                                                                         \
        size_t original_index;                                                                   \
        bool *original_seen = SDL_calloc((SIZE), sizeof(bool));                                  \
        Uint64 lost_count = 0;                                                                   \
        SDL_assert(original_seen != NULL);                                                       \
        for (original_index = 0; original_index < (SIZE); original_index++) {                    \
            size_t sorted_index;                                                                 \
            for (sorted_index = 0; sorted_index < (SIZE); sorted_index++) {                      \
                if (IS_SAME((ORIGINAL)[original_index], (SORTED)[sorted_index])) {               \
                    original_seen[original_index] = true;                                        \
                    break;                                                                       \
                }                                                                                \
            }                                                                                    \
        }                                                                                        \
        for (original_index = 0; original_index < (SIZE); original_index++) {                    \
            if (!original_seen[original_index]) {                                                \
                SDLTest_AssertCheck(original_seen[original_index],                               \
                    STR(TYPE) "Element %d is missing in the sorted array", (int)original_index); \
                lost_count += 1;                                                                 \
            }                                                                                    \
        }                                                                                        \
        SDLTest_AssertCheck(lost_count == 0,                                                     \
            STR(TYPE) "No elements were lost (lost count=%" SDL_PRIu64 ")", lost_count);         \
        SDL_free(original_seen);                                                                 \
    } while (0)

#define TEST_QSORT_ARRAY_GENERIC(TYPE, ARRAY, SIZE, QSORT_CALL, CHECK_ARRAY_ELEMS, IS_LE, WHAT) \
    do {                                                                                        \
        SDL_memcpy(sorted, (ARRAY), sizeof(TYPE) * (SIZE));                                     \
        SDLTest_AssertPass(STR(TYPE) "About to call " WHAT "(%d, %d)",                          \
            (int)(SIZE), (int)sizeof(TYPE));                                                    \
        QSORT_CALL;                                                                             \
        SDLTest_AssertPass(STR(TYPE) WHAT " finished");                                         \
        TEST_ARRAY_IS_SORTED(TYPE, sorted, SIZE, IS_LE);                                        \
        SDLTest_AssertPass(STR(TYPE) "Verifying element preservation...");                      \
        CHECK_ARRAY_ELEMS(TYPE, sorted, (ARRAY), (SIZE));                                       \
    } while (0)

#define TEST_QSORT_ARRAY_QSORT(TYPE, ARRAY, SIZE, COMPARE_CBFN, CHECK_ARRAY_ELEMS, IS_LE)          \
    do {                                                                                           \
        SDLTest_AssertPass(STR(TYPE) "Testing SDL_qsort of array with size %u", (unsigned)(SIZE)); \
        global_compare_cbfn = NULL;                                                                \
        TEST_QSORT_ARRAY_GENERIC(TYPE, ARRAY, SIZE,                                                \
            SDL_qsort(sorted, (SIZE), sizeof(TYPE), COMPARE_CBFN),                                 \
            CHECK_ARRAY_ELEMS, IS_LE, "SDL_qsort");                                                \
    } while (0);

#if defined(SDL_PLATFORM_WINDOWS) && defined(TEST_STDLIB_QSORT)
#define TEST_QSORT_ARRAY_QSORT_R(TYPE, ARRAY, SIZE, COMPARE_CBFN, CHECK_ARRAY_ELEMS, IS_LE) \
    do {                                                                                    \
        SDLTest_AssertPass(STR(TYPE) "qsort_r is not available on current platform");       \
    } while (0)
#else
#define TEST_QSORT_ARRAY_QSORT_R(TYPE, ARRAY, SIZE, COMPARE_CBFN, CHECK_ARRAY_ELEMS, IS_LE)          \
    do {                                                                                             \
        SDLTest_AssertPass(STR(TYPE) "Testing SDL_qsort_r of array with size %u", (unsigned)(SIZE)); \
        global_compare_cbfn = (COMPARE_CBFN);                                                        \
        TEST_QSORT_ARRAY_GENERIC(TYPE, ARRAY, SIZE,                                                  \
            SDL_qsort_r(sorted, (SIZE), sizeof(TYPE), generic_compare_r, &a_global_var),             \
            CHECK_ARRAY_ELEMS, IS_LE, "SDL_qsort_r");                                                \
    } while (0);
#endif

#define TEST_QSORT_ARRAY(TYPE, ARRAY, SIZE, COMPARE_CBFN, CHECK_ARRAY_ELEMS, IS_LE)          \
    do {                                                                                     \
        TYPE *sorted = SDL_calloc((SIZE), sizeof(TYPE));                                     \
        SDL_assert(sorted != NULL);                                                          \
                                                                                             \
        TEST_QSORT_ARRAY_QSORT(TYPE, ARRAY, SIZE, COMPARE_CBFN, CHECK_ARRAY_ELEMS, IS_LE);   \
                                                                                             \
        TEST_QSORT_ARRAY_QSORT_R(TYPE, ARRAY, SIZE, COMPARE_CBFN, CHECK_ARRAY_ELEMS, IS_LE); \
                                                                                             \
        SDL_free(sorted);                                                                    \
    } while (0)

#define INT_ISLE(A, B) ((A) <= (B))

#define INTPTR_ISLE(A, B) (*(A) <= *(B))

#define FLOAT_ISLE(A, B) ((A) <= (B))

#define DOUBLE_ISLE(A, B) ((A) <= (B))

#define VERSION_ISLE(A, B) (compare_version(&(A), &(B)) <= 0)

#define CHECK_ELEMS_SORTED_ARRAY(TYPE, SORTED, INPUT, SIZE)          \
    do {                                                             \
        unsigned int check_index;                                    \
        for (check_index = 0; check_index < (SIZE); check_index++) { \
            if ((SORTED)[check_index] != (INPUT)[check_index]) {     \
              SDLTest_AssertCheck(false,                             \
                "sorted[%u] == input[%u]",                           \
                check_index, check_index);                           \
            }                                                        \
        }                                                            \
    } while (0)

static int SDLCALL qsort_testAlreadySorted(void *arg)
{
    unsigned int iteration;
    (void)arg;

    for (iteration = 0; iteration < count_arraylens; iteration++) {
        const unsigned int arraylen = arraylens[iteration];
        unsigned int i;
        int *ints = SDL_malloc(sizeof(int) * arraylen);
        int **intptrs = SDL_malloc(sizeof(int *) * arraylen);
        double *doubles = SDL_malloc(sizeof(double) * arraylen);

        for (i = 0; i < arraylen; i++) {
            ints[i] = i;
            intptrs[i] = &ints[i];
            doubles[i] = (double)i * SDL_PI_D;
        }
        TEST_QSORT_ARRAY(int, ints, arraylen, compare_int, CHECK_ELEMS_SORTED_ARRAY, INT_ISLE);
        TEST_QSORT_ARRAY(int *, intptrs, arraylen, compare_intptr, CHECK_ELEMS_SORTED_ARRAY, INTPTR_ISLE);
        TEST_QSORT_ARRAY(double, doubles, arraylen, compare_double, CHECK_ELEMS_SORTED_ARRAY, DOUBLE_ISLE);

        SDL_free(ints);
        SDL_free(intptrs);
        SDL_free(doubles);
    }
    return TEST_COMPLETED;
}

#define CHECK_ELEMS_SORTED_ARRAY_EXCEPT_LAST(TYPE, SORTED, INPUT, SIZE)                             \
    do {                                                                                            \
        unsigned int check_index;                                                                   \
        for (check_index = 0; check_index < (SIZE) - 1; check_index++) {                            \
            if (SDL_memcmp(&(SORTED)[check_index + 1], &(INPUT)[check_index], sizeof(TYPE)) != 0) { \
              SDLTest_AssertCheck(false,                                                            \
                STR(TYPE) "sorted[%u] == input[%u]",                                                \
                check_index + 1, check_index);                                                      \
            }                                                                                       \
        }                                                                                           \
    } while (0)

static int SDLCALL qsort_testAlreadySortedExceptLast(void *arg)
{
    unsigned int iteration;
    (void)arg;

    for (iteration = 0; iteration < count_arraylens; iteration++) {
        const unsigned int arraylen = arraylens[iteration];
        unsigned int i;
        int *ints = SDL_malloc(sizeof(int) * arraylen);
        int **intptrs = SDL_malloc(sizeof(int *) * arraylen);
        double *doubles = SDL_malloc(sizeof(double) * arraylen);
        VersionTuple *versions = SDL_calloc(arraylen, sizeof(VersionTuple));

        for (i = 0; i < arraylen; i++) {
            ints[i] = i;
            intptrs[i] = &ints[i];
            doubles[i] = (double)i * SDL_PI_D;
            versions[i].micro = (i + 1) % 256;
            versions[i].minor = (i + 1) % (256 * 256) / 256;
            versions[i].major = (i + 1) % (256 * 256 * 256) / 256 / 256;
        }
        ints[arraylen - 1] = -1;
        doubles[arraylen - 1] = -1.;
        versions[arraylen - 1].major = 0;
        versions[arraylen - 1].minor = 0;
        versions[arraylen - 1].micro = 0;
        TEST_QSORT_ARRAY(int, ints, arraylen, compare_int, CHECK_ELEMS_SORTED_ARRAY_EXCEPT_LAST, INT_ISLE);
        TEST_QSORT_ARRAY(int *, intptrs, arraylen, compare_intptr, CHECK_ELEMS_SORTED_ARRAY_EXCEPT_LAST, INTPTR_ISLE);
        TEST_QSORT_ARRAY(double, doubles, arraylen, compare_double, CHECK_ELEMS_SORTED_ARRAY_EXCEPT_LAST, DOUBLE_ISLE);
        TEST_QSORT_ARRAY(VersionTuple, versions, arraylen, compare_version, CHECK_ELEMS_SORTED_ARRAY_EXCEPT_LAST, VERSION_ISLE);

        SDL_free(ints);
        SDL_free(intptrs);
        SDL_free(doubles);
        SDL_free(versions);
    }
    return TEST_COMPLETED;
}

#define CHECK_ELEMS_SORTED_ARRAY_REVERSED(TYPE, SORTED, INPUT, SIZE)                                         \
    do {                                                                                                     \
        unsigned int check_index;                                                                            \
        for (check_index = 0; check_index < (SIZE); check_index++) {                                         \
            if (SDL_memcmp(&(SORTED)[check_index], &(INPUT)[(SIZE) - check_index - 1], sizeof(TYPE)) != 0) { \
              SDLTest_AssertCheck(false,                                                                     \
                STR(TYPE) "sorted[%u] != input[%u]",                                                         \
                check_index, (SIZE) - check_index - 1);                                                      \
            }                                                                                                \
        }                                                                                                    \
    } while (0)

static int SDLCALL qsort_testReverseSorted(void *arg)
{
    unsigned int iteration;
    (void)arg;

    for (iteration = 0; iteration < count_arraylens; iteration++) {
        const unsigned int arraylen = arraylens[iteration];
        unsigned int i;
        int *ints = SDL_malloc(sizeof(int) * arraylen);
        int **intptrs = SDL_malloc(sizeof(int *) * arraylen);
        double *doubles = SDL_malloc(sizeof(double) * arraylen);
        VersionTuple *versions = SDL_calloc(arraylen, sizeof(VersionTuple));

        for (i = 0; i < arraylen; i++) {
            ints[i] = (arraylen - 1) - i;
            intptrs[i] = &ints[i];
            doubles[i] = (double)((arraylen - 1) - i) * SDL_PI_D;
            versions[i].micro = ints[i] % 256;
            versions[i].minor = ints[i] % (256 * 256) / 256;
            versions[i].major = ints[i] % (256 * 256 * 256) / 256 / 256;
        }
        TEST_QSORT_ARRAY(int, ints, arraylen, compare_int, CHECK_ELEMS_SORTED_ARRAY_REVERSED, INT_ISLE);
        TEST_QSORT_ARRAY(int *, intptrs, arraylen, compare_intptr, CHECK_ELEMS_SORTED_ARRAY_REVERSED, INTPTR_ISLE);
        TEST_QSORT_ARRAY(double, doubles, arraylen, compare_double, CHECK_ELEMS_SORTED_ARRAY_REVERSED, DOUBLE_ISLE);
        TEST_QSORT_ARRAY(VersionTuple, versions, arraylen, compare_version, CHECK_ELEMS_SORTED_ARRAY_REVERSED, VERSION_ISLE);

        SDL_free(ints);
        SDL_free(intptrs);
        SDL_free(doubles);
        SDL_free(versions);
    }
    return TEST_COMPLETED;
}

#define MAX_RANDOM_INT_VALUE (1024 * 1024)

#define CHECK_ELEMS_SORTED_ARRAY_RANDOM_INT(TYPE, SORTED, INPUT, SIZE)             \
    do {                                                                           \
        int *presences = SDL_calloc(MAX_RANDOM_INT_VALUE, sizeof(int));            \
        unsigned int check_index;                                                  \
        for (check_index = 0; check_index < (SIZE); check_index++) {               \
            presences[(SORTED)[check_index]] += 1;                                 \
            presences[(INPUT)[check_index]] -= 1;                                  \
        }                                                                          \
        for (check_index = 0; check_index < MAX_RANDOM_INT_VALUE; check_index++) { \
            if (presences[check_index] != 0) {                                     \
                SDLTest_AssertCheck(false, "Value %d appears %s in sorted array",  \
                    check_index,                                                   \
                    presences[check_index] > 0 ? "MORE" : "LESS");                 \
            }                                                                      \
        }                                                                          \
        SDL_free(presences);                                                       \
    } while (0)

#define VERSION_TO_INT(VERSION) (((VERSION).major * 256 + (VERSION).minor) * 256 + (VERSION).micro)
#define INT_VERSION_MAJOR(V)       (((V) / (256 * 256) % 256))
#define INT_VERSION_MINOR(V)       (((V) / (256)) % 256)
#define INT_VERSION_MICRO(V)       ((V) % 256)

#define CHECK_ELEMS_SORTED_ARRAY_RANDOM_VERSION(TYPE, SORTED, INPUT, SIZE)                          \
    do {                                                                                            \
        int *presences = SDL_calloc(256 * 256 * 256, sizeof(int));                                  \
        unsigned int check_index;                                                                   \
        for (check_index = 0; check_index < (SIZE); check_index++) {                                \
            presences[VERSION_TO_INT((SORTED)[check_index])] += 1;                                  \
            presences[VERSION_TO_INT((INPUT)[check_index])] -= 1;                                   \
        }                                                                                           \
        for (check_index = 0; check_index < 256 * 256 * 256; check_index++) {                       \
            if (presences[check_index] != 0) {                                                      \
                SDLTest_AssertCheck(false, STR(TYPE) "Version %d.%d.%d appears %s in sorted array", \
                    INT_VERSION_MAJOR(check_index),                                                 \
                    INT_VERSION_MINOR(check_index),                                                 \
                    INT_VERSION_MICRO(check_index),                                                 \
                    presences[check_index] > 0 ? "MORE" : "LESS");                                  \
            }                                                                                       \
        }                                                                                           \
        SDL_free(presences);                                                                        \
    } while (0)

#define CHECK_ELEMS_SORTED_ARRAY_RPS(TYPE, SORTED, INPUT, SIZE)                        \
    do {                                                                               \
        int presences[3] = { 0 };                                                      \
        unsigned int check_index;                                                      \
        for (check_index = 0; check_index < (SIZE); check_index++) {                   \
            presences[(SORTED)[check_index]] += 1;                                     \
            presences[(INPUT)[check_index]] -= 1;                                      \
        }                                                                              \
        for (check_index = 0; check_index < SDL_arraysize(presences); check_index++) { \
            if (presences[check_index] != 0) {                                         \
                SDLTest_AssertCheck(false, STR(TYPE) "%d appeared or disappeared",     \
                    check_index);                                                      \
            }                                                                          \
        }                                                                              \
    } while (0)

#define CHECK_ELEMS_SORTED_ARRAY_RANDOM_NOP(TYPE, SORTED, INPUT, SIZE) \
    SDLTest_AssertPass(STR(TYPE) "Skipping elements presence check")

static int SDLCALL qsort_testRandomSorted(void *arg)
{
    unsigned int iteration;
    (void)arg;

    for (iteration = 0; iteration < count_arraylens; iteration++) {
        const unsigned int arraylen = arraylens[iteration];
        unsigned int i;
        int *ints = SDL_malloc(sizeof(int) * arraylen);
        float *floats = SDL_malloc(sizeof(float) * arraylen);
        VersionTuple *versions = SDL_calloc(arraylen, sizeof(VersionTuple));

        for (i = 0; i < arraylen; i++) {
            ints[i] = SDLTest_RandomIntegerInRange(0, MAX_RANDOM_INT_VALUE - 1);
            floats[i] = SDLTest_RandomFloat() * SDL_PI_F;
            versions[i].micro = SDLTest_RandomIntegerInRange(0, 255);
            versions[i].minor = SDLTest_RandomIntegerInRange(0, 255);
            versions[i].major = SDLTest_RandomIntegerInRange(0, 255);
        }
        TEST_QSORT_ARRAY(int, ints, arraylen, compare_int, CHECK_ELEMS_SORTED_ARRAY_RANDOM_INT, INT_ISLE);
        TEST_QSORT_ARRAY(float, floats, arraylen, compare_float, CHECK_ELEMS_SORTED_ARRAY_RANDOM_NOP, FLOAT_ISLE);
        TEST_QSORT_ARRAY(VersionTuple, versions, arraylen, compare_version, CHECK_ELEMS_SORTED_ARRAY_RANDOM_VERSION, VERSION_ISLE);

        SDL_free(ints);
        SDL_free(floats);
        SDL_free(versions);
    }
    return TEST_COMPLETED;
}

static const SDLTest_TestCaseReference qsortTestAlreadySorted = {
    qsort_testAlreadySorted, "qsort_testAlreadySorted", "Test sorting already sorted array", TEST_ENABLED
};

static const SDLTest_TestCaseReference qsortTestAlreadySortedExceptLast = {
    qsort_testAlreadySortedExceptLast, "qsort_testAlreadySortedExceptLast", "Test sorting nearly sorted array (last item is not in order)", TEST_ENABLED
};

static const SDLTest_TestCaseReference qsortTestReverseSorted = {
    qsort_testReverseSorted, "qsort_testReverseSorted", "Test sorting an array in reverse order", TEST_ENABLED
};

static const SDLTest_TestCaseReference qsortTestRandomSorted = {
    qsort_testRandomSorted, "qsort_testRandomSorted", "Test sorting a random array", TEST_ENABLED
};

static const SDLTest_TestCaseReference *qsortTests[] = {
    &qsortTestAlreadySorted,
    &qsortTestAlreadySortedExceptLast,
    &qsortTestReverseSorted,
    &qsortTestRandomSorted,
    NULL
};

static SDLTest_TestSuiteReference qsortTestSuite = {
    "qsort",
    NULL,
    qsortTests,
    NULL
};

static SDLTest_TestSuiteReference *testSuites[] = {
    &qsortTestSuite,
    NULL
};

int main(int argc, char *argv[])
{
    int i;
    int result;
    SDLTest_CommonState *state;
    SDLTest_TestSuiteRunner *runner;
    bool list = false;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    runner = SDLTest_CreateTestSuiteRunner(state, testSuites);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {

            if (SDL_strcasecmp(argv[i], "--array-lengths") == 0) {
                count_arraylens = 0;
                consumed = 1;
                while (argv[i + consumed] && argv[i + consumed][0] != '-') {
                    char *endptr = NULL;
                    unsigned int arraylen = (unsigned int)SDL_strtoul(argv[i + 1], &endptr, 10);
                    if (*endptr != '\0') {
                        count_arraylens = 0;
                        break;
                    }
                    if (count_arraylens >= SDL_arraysize(arraylens)) {
                        SDL_LogWarn(SDL_LOG_CATEGORY_TEST, "Dropping array length %d", arraylen);
                    } else {
                        arraylens[count_arraylens] = arraylen;
                    }
                    count_arraylens++;
                    consumed++;
                }
                if (consumed == 0) {
                    SDL_LogError(SDL_LOG_CATEGORY_TEST, "--array-lengths needs positive int numbers");
                }
            } else if (SDL_strcasecmp(argv[i], "--list") == 0) {
                consumed = 1;
                list = true;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[--list]",
                "[--array-lengths N1 [N2 [N3 [...]]]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    /* List all suites. */
    if (list) {
        int suiteCounter;
        for (suiteCounter = 0; testSuites[suiteCounter]; ++suiteCounter) {
            int testCounter;
            SDLTest_TestSuiteReference *testSuite = testSuites[suiteCounter];
            SDL_Log("Test suite: %s", testSuite->name);
            for (testCounter = 0; testSuite->testCases[testCounter]; ++testCounter) {
                const SDLTest_TestCaseReference *testCase = testSuite->testCases[testCounter];
                SDL_Log("      test: %s%s", testCase->name, testCase->enabled ? "" : " (disabled)");
            }
        }
        result = 0;
    } else {
        result = SDLTest_ExecuteTestSuiteRunner(runner);
    }

    SDL_Quit();
    SDLTest_DestroyTestSuiteRunner(runner);
    SDLTest_CommonDestroyState(state);
    return result;
}
