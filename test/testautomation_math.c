/**
 * Math test suite
 */

#include <math.h>

#include "SDL.h"
#include "SDL_test.h"

/* ================= Test Structs ================== */

/**
 * Stores a single input and the expected result
 */
typedef struct
{
    double input;
    double expected;
} d_to_d;

/**
 * Stores a pair of inputs and the expected result
 */
typedef struct
{
    double x_input, y_input;
    double expected;
} dd_to_d;

/* ================= Test Case Implementation ================== */

/* SDL_floor tests functions */

/**
 * \brief Checks edge cases (0 and infinity) for themselves.
 */
static int
floor_edgeCases(void *args)
{
    double result;

    result = SDL_floor(INFINITY);
    SDLTest_AssertCheck(INFINITY == result, "Floor(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);
    result = SDL_floor(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result, "Floor(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    result = SDL_floor(0.0);
    SDLTest_AssertCheck(0.0 == result, "Floor(%.1f), expected %.1f, got %.1f",
                        0.0, 0.0, result);
    result = SDL_floor(-0.0);
    SDLTest_AssertCheck(-0.0 == result, "Floor(%.1f), expected %.1f, got %.1f",
                        -0.0, -0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN case.
 */
static int
floor_nanCase(void *args)
{
    SDLTest_AssertCheck(isnan(SDL_floor(NAN)), "Floor(nan), expected nan");
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
floor_roundNumbersCases(void *args)
{
    Uint32 i;
    const double round_cases[] = {
        1.0,
        -1.0,
        15.0,
        -15.0,
        125.0,
        -125.0,
        1024.0,
        -1024.0
    };
    for (i = 0; i < SDL_arraysize(round_cases); i++) {
        const double result = SDL_floor(round_cases[i]);
        SDLTest_AssertCheck(result == round_cases[i],
                            "Floor(%.1f), expected %.1f, got %.1f", round_cases[i],
                            round_cases[i], result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of fractions
 */
static int
floor_fractionCases(void *args)
{
    Uint32 i;
    const d_to_d frac_cases[] = {
        { 1.0 / 2.0, 0.0 },
        { -1.0 / 2.0, -1.0 },
        { 4.0 / 3.0, 1.0 },
        { -4.0 / 3.0, -2.0 },
        { 76.0 / 7.0, 10.0 },
        { -76.0 / 7.0, -11.0 },
        { 535.0 / 8.0, 66.0 },
        { -535.0 / 8.0, -67.0 },
        { 19357.0 / 53.0, 365.0 },
        { -19357.0 / 53.0, -366.0 }
    };
    for (i = 0; i < SDL_arraysize(frac_cases); i++) {
        const double result = SDL_floor(frac_cases[i].input);
        SDLTest_AssertCheck(result == frac_cases[i].expected,
                            "Floor(%f), expected %.1f, got %f", frac_cases[i].input,
                            frac_cases[i].expected, result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
floor_rangeTest(void *args)
{
    const Uint32 ITERATIONS = 10000000;
    const Uint32 STEP = SDL_MAX_UINT32 / ITERATIONS;
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Floor: Testing a range of %u values with %u steps",
                       ITERATIONS, STEP);

    for (i = 0; i < ITERATIONS; i++, test_value += STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        result = SDL_floor(test_value);
        if (result != test_value) { /* Only log failures to save performances */
            SDLTest_AssertPass("Floor(%.1f), expected %.1f, got %.1f", test_value,
                               test_value, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_ceil tests functions */

/**
 * \brief Checks edge cases (0 and infinity) for themselves.
 */
static int
ceil_edgeCases(void *args)
{
    double result;

    result = SDL_ceil(INFINITY);
    SDLTest_AssertCheck(INFINITY == result, "Ceil(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);
    result = SDL_ceil(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result, "Ceil(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    result = SDL_ceil(0.0);
    SDLTest_AssertCheck(0.0 == result, "Ceil(%.1f), expected %.1f, got %.1f",
                        0.0, 0.0, result);
    result = SDL_ceil(-0.0);
    SDLTest_AssertCheck(-0.0 == result, "Ceil(%.1f), expected %.1f, got %.1f",
                        -0.0, -0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN case.
 */
static int
ceil_nanCase(void *args)
{
    SDLTest_AssertCheck(isnan(SDL_ceil(NAN)), "Ceil(nan), expected nan");
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
ceil_roundNumbersCases(void *args)
{
    Uint32 i;
    const double round_cases[] = {
        1.0,
        -1.0,
        15.0,
        -15.0,
        125.0,
        -125.0,
        1024.0,
        -1024.0
    };
    for (i = 0; i < SDL_arraysize(round_cases); i++) {
        const double result = SDL_ceil(round_cases[i]);
        SDLTest_AssertCheck(result == round_cases[i],
                            "Ceil(%.1f), expected %.1f, got %.1f", round_cases[i],
                            round_cases[i], result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of fractions
 */
static int
ceil_fractionCases(void *args)
{
    Uint32 i;
    const d_to_d frac_cases[] = {
        { 1.0 / 2.0, 1.0 },
        { -1.0 / 2.0, -0.0 },
        { 4.0 / 3.0, 2.0 },
        { -4.0 / 3.0, -1.0 },
        { 76.0 / 7.0, 11.0 },
        { -76.0 / 7.0, -10.0 },
        { 535.0 / 8.0, 67.0 },
        { -535.0 / 8.0, -66.0 },
        { 19357.0 / 53.0, 366.0 },
        { -19357.0 / 53.0, -365.0 }
    };
    for (i = 0; i < SDL_arraysize(frac_cases); i++) {
        const double result = SDL_ceil(frac_cases[i].input);
        SDLTest_AssertCheck(result == frac_cases[i].expected,
                            "Ceil(%f), expected %.1f, got %f", frac_cases[i].input,
                            frac_cases[i].expected, result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
ceil_rangeTest(void *args)
{
    const Uint32 ITERATIONS = 10000000;
    const Uint32 STEP = SDL_MAX_UINT32 / ITERATIONS;
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Ceil: Testing a range of %u values with %u steps",
                       ITERATIONS, STEP);

    for (i = 0; i < ITERATIONS; i++, test_value += STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        result = SDL_ceil(test_value);
        if (result != test_value) { /* Only log failures to save performances */
            SDLTest_AssertPass("Ceil(%.1f), expected %.1f, got %.1f", test_value,
                               test_value, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_trunc tests functions */

/**
 * \brief Checks edge cases (0 and infinity) for themselves.
 */
static int
trunc_edgeCases(void *args)
{
    double result;

    result = SDL_trunc(INFINITY);
    SDLTest_AssertCheck(INFINITY == result, "Trunc(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);
    result = SDL_trunc(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result, "Trunc(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    result = SDL_trunc(0.0);
    SDLTest_AssertCheck(0.0 == result, "Trunc(%.1f), expected %.1f, got %.1f",
                        0.0, 0.0, result);
    result = SDL_trunc(-0.0);
    SDLTest_AssertCheck(-0.0 == result, "Trunc(%.1f), expected %.1f, got %.1f",
                        -0.0, -0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN case.
 */
static int
trunc_nanCase(void *args)
{
    SDLTest_AssertCheck(isnan(SDL_trunc(NAN)), "Trunc(nan), expected nan");
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
trunc_roundNumbersCases(void *args)
{
    Uint32 i;
    const double round_cases[] = {
        1.0,
        -1.0,
        15.0,
        -15.0,
        125.0,
        -125.0,
        1024.0,
        -1024.0
    };
    for (i = 0; i < SDL_arraysize(round_cases); i++) {
        const double result = SDL_trunc(round_cases[i]);
        SDLTest_AssertCheck(result == round_cases[i],
                            "Trunc(%.1f), expected %.1f, got %.1f", round_cases[i],
                            round_cases[i], result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of fractions
 */
static int
trunc_fractionCases(void *args)
{
    Uint32 i;
    const d_to_d frac_cases[] = {
        { 1.0 / 2.0, 0.0 },
        { -1.0 / 2.0, -0.0 },
        { 4.0 / 3.0, 1.0 },
        { -4.0 / 3.0, -1.0 },
        { 76.0 / 7.0, 10.0 },
        { -76.0 / 7.0, -10.0 },
        { 535.0 / 8.0, 66.0 },
        { -535.0 / 8.0, -66.0 },
        { 19357.0 / 53.0, 365.0 },
        { -19357.0 / 53.0, -365.0 }
    };
    for (i = 0; i < SDL_arraysize(frac_cases); i++) {
        const double result = SDL_trunc(frac_cases[i].input);
        SDLTest_AssertCheck(result == frac_cases[i].expected,
                            "Trunc(%f), expected %.1f, got %f", frac_cases[i].input,
                            frac_cases[i].expected, result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
trunc_rangeTest(void *args)
{
    const Uint32 ITERATIONS = 10000000;
    const Uint32 STEP = SDL_MAX_UINT32 / ITERATIONS;
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Trunc: Testing a range of %u values with %u steps",
                       ITERATIONS, STEP);

    for (i = 0; i < ITERATIONS; i++, test_value += STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        result = SDL_trunc(test_value);
        if (result != test_value) { /* Only log failures to save performances */
            SDLTest_AssertPass("Trunc(%.1f), expected %.1f, got %.1f", test_value,
                               test_value, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_round tests functions */

/**
 * \brief Checks edge cases (0 and infinity) for themselves.
 */
static int
round_edgeCases(void *args)
{
    double result;

    result = SDL_round(INFINITY);
    SDLTest_AssertCheck(INFINITY == result, "Round(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);
    result = SDL_round(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result, "Round(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    result = SDL_round(0.0);
    SDLTest_AssertCheck(0.0 == result, "Round(%.1f), expected %.1f, got %.1f",
                        0.0, 0.0, result);
    result = SDL_round(-0.0);
    SDLTest_AssertCheck(-0.0 == result, "Round(%.1f), expected %.1f, got %.1f",
                        -0.0, -0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN case.
 */
static int
round_nanCase(void *args)
{
    SDLTest_AssertCheck(isnan(SDL_round(NAN)), "Round(nan), expected nan");
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
round_roundNumbersCases(void *args)
{
    Uint32 i;
    const double round_cases[] = {
        1.0,
        -1.0,
        15.0,
        -15.0,
        125.0,
        -125.0,
        1024.0,
        -1024.0
    };
    for (i = 0; i < SDL_arraysize(round_cases); i++) {
        const double result = SDL_round(round_cases[i]);
        SDLTest_AssertCheck(result == round_cases[i],
                            "Round(%.1f), expected %.1f, got %.1f", round_cases[i],
                            round_cases[i], result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of fractions
 */
static int
round_fractionCases(void *args)
{
    Uint32 i;
    const d_to_d frac_cases[] = {
        { 1.0 / 2.0, 1.0 },
        { -1.0 / 2.0, -1.0 },
        { 4.0 / 3.0, 1.0 },
        { -4.0 / 3.0, -1.0 },
        { 76.0 / 7.0, 11.0 },
        { -76.0 / 7.0, -11.0 },
        { 535.0 / 8.0, 67.0 },
        { -535.0 / 8.0, -67.0 },
        { 19357.0 / 53.0, 365.0 },
        { -19357.0 / 53.0, -365.0 }
    };
    for (i = 0; i < SDL_arraysize(frac_cases); i++) {
        const double result = SDL_round(frac_cases[i].input);
        SDLTest_AssertCheck(result == frac_cases[i].expected,
                            "Round(%f), expected %.1f, got %f", frac_cases[i].input,
                            frac_cases[i].expected, result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
round_rangeTest(void *args)
{
    const Uint32 ITERATIONS = 10000000;
    const Uint32 STEP = SDL_MAX_UINT32 / ITERATIONS;
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Round: Testing a range of %u values with %u steps",
                       ITERATIONS, STEP);

    for (i = 0; i < ITERATIONS; i++, test_value += STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        result = SDL_round(test_value);
        if (result != test_value) { /* Only log failures to save performances */
            SDLTest_AssertPass("Round(%.1f), expected %.1f, got %.1f", test_value,
                               test_value, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_fabs tests functions */

/**
 * \brief Checks edge cases (0 and infinity).
 */
static int
fabs_edgeCases(void *args)
{
    double result;

    result = SDL_fabs(INFINITY);
    SDLTest_AssertCheck(INFINITY == result, "Fabs(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);
    result = SDL_fabs(-INFINITY);
    SDLTest_AssertCheck(INFINITY == result, "Fabs(%f), expected %f, got %f",
                        -INFINITY, INFINITY, result);

    result = SDL_fabs(0.0);
    SDLTest_AssertCheck(0.0 == result, "Fabs(%.1f), expected %.1f, got %.1f",
                        0.0, 0.0, result);
    result = SDL_fabs(-0.0);
    SDLTest_AssertCheck(0.0 == result, "Fabs(%.1f), expected %.1f, got %.1f",
                        -0.0, 0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN case.
 */
static int
fabs_nanCase(void *args)
{
    SDLTest_AssertCheck(isnan(SDL_fabs(NAN)), "Fabs(nan), expected nan");
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
fabs_rangeTest(void *args)
{
    const Uint32 ITERATIONS = 10000000;
    const Uint32 STEP = SDL_MAX_UINT32 / ITERATIONS;
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Fabs: Testing a range of %u values with %u steps",
                       ITERATIONS, STEP);

    for (i = 0; i < ITERATIONS; i++, test_value += STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        result = SDL_fabs(test_value);
        if (result != test_value) { /* Only log failures to save performances */
            SDLTest_AssertPass("Fabs(%.1f), expected %.1f, got %.1f", test_value,
                               test_value, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* SDL_floor test cases */

static const SDLTest_TestCaseReference floorTest1 = {
    (SDLTest_TestCaseFp) floor_edgeCases, "floor_edgeCases",
    "Check positive and negative infinity and 0", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTest2 = {
    (SDLTest_TestCaseFp) floor_nanCase, "floor_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTest3 = {
    (SDLTest_TestCaseFp) floor_roundNumbersCases, "floor_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTest4 = {
    (SDLTest_TestCaseFp) floor_fractionCases, "floor_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTest5 = {
    (SDLTest_TestCaseFp) floor_rangeTest, "floor_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_ceil test cases */

static const SDLTest_TestCaseReference ceilTest1 = {
    (SDLTest_TestCaseFp) ceil_edgeCases, "ceil_edgeCases",
    "Check positive and negative infinity and 0", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTest2 = {
    (SDLTest_TestCaseFp) ceil_nanCase, "ceil_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTest3 = {
    (SDLTest_TestCaseFp) ceil_roundNumbersCases, "ceil_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTest4 = {
    (SDLTest_TestCaseFp) ceil_fractionCases, "ceil_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTest5 = {
    (SDLTest_TestCaseFp) ceil_rangeTest, "ceil_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_trunc test cases */

static const SDLTest_TestCaseReference truncTest1 = {
    (SDLTest_TestCaseFp) trunc_edgeCases, "trunc_edgeCases",
    "Check positive and negative infinity and 0", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTest2 = {
    (SDLTest_TestCaseFp) trunc_nanCase, "trunc_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTest3 = {
    (SDLTest_TestCaseFp) trunc_roundNumbersCases, "trunc_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTest4 = {
    (SDLTest_TestCaseFp) trunc_fractionCases, "trunc_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTest5 = {
    (SDLTest_TestCaseFp) trunc_rangeTest, "trunc_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_round test cases */

static const SDLTest_TestCaseReference roundTest1 = {
    (SDLTest_TestCaseFp) round_edgeCases, "round_edgeCases",
    "Check positive and negative infinity and 0", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTest2 = {
    (SDLTest_TestCaseFp) round_nanCase, "round_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTest3 = {
    (SDLTest_TestCaseFp) round_roundNumbersCases, "round_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTest4 = {
    (SDLTest_TestCaseFp) round_fractionCases, "round_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTest5 = {
    (SDLTest_TestCaseFp) round_rangeTest, "round_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_fabs test cases */

static const SDLTest_TestCaseReference fabsTest1 = {
    (SDLTest_TestCaseFp) fabs_edgeCases, "fabs_edgeCases",
    "Check positive and negative infinity and 0", TEST_ENABLED
};
static const SDLTest_TestCaseReference fabsTest2 = {
    (SDLTest_TestCaseFp) fabs_nanCase, "fabs_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference fabsTest3 = {
    (SDLTest_TestCaseFp) fabs_rangeTest, "fabs_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

static const SDLTest_TestCaseReference *mathTests[] = {
    &floorTest1, &floorTest2, &floorTest3, &floorTest4, &floorTest5,
    &ceilTest1, &ceilTest2, &ceilTest3, &ceilTest4, &ceilTest5,
    &truncTest1, &truncTest2, &truncTest3, &truncTest4, &truncTest5,
    &roundTest1, &roundTest2, &roundTest3, &roundTest4, &roundTest5,
    &fabsTest1, &fabsTest2, &fabsTest3,
    NULL
};

SDLTest_TestSuiteReference mathTestSuite = { "Math", NULL, mathTests, NULL };
