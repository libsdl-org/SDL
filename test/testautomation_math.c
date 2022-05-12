/**
 * Math test suite
 */

#include <math.h>

#include "SDL.h"
#include "SDL_test.h"

/* Range tests parameters */
#define RANGE_TEST_ITERATIONS 10000000
#define RANGE_TEST_STEP       SDL_MAX_UINT32 / RANGE_TEST_ITERATIONS

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

/*
    NB: You cannot create an array of these structures containing INFINITY or NAN.
    On platforms such as OS/2, they are defined as 'extern const double' making them
    not compile-time constant.
*/

/* ================= Test Helpers ================== */

typedef double(SDLCALL *d_to_d_func)(double);
typedef double(SDLCALL *dd_to_d_func)(double, double);

/**
 * \brief Runs all the cases on a given function with a signature double -> double
 *
 * \param func_name, the name of the tested function.
 * \param func, the function to call.
 * \param cases, an array of all the cases.
 * \param cases_size, the size of the cases array.
 */
static int
helper_dtod(const char *func_name, d_to_d_func func,
            const d_to_d *cases, const size_t cases_size)
{
    Uint32 i;
    for (i = 0; i < cases_size; i++) {
        const double result = func(cases[i].input);
        SDLTest_AssertCheck(result == cases[i].expected,
                            "%s(%f), expected %f, got %f",
                            func_name,
                            cases[i].input,
                            cases[i].expected, result);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Runs all the cases on a given function with a signature (double, double) -> double
 *
 * \param func_name, the name of the tested function.
 * \param func, the function to call.
 * \param cases, an array of all the cases.
 * \param cases_size, the size of the cases array.
 */
static int
helper_ddtod(const char *func_name, dd_to_d_func func,
             const dd_to_d *cases, const size_t cases_size)
{
    Uint32 i;
    for (i = 0; i < cases_size; i++) {
        const double result = func(cases[i].x_input, cases[i].y_input);
        SDLTest_AssertCheck(result == cases[i].expected,
                            "%s(%f,%f), expected %f, got %f",
                            func_name,
                            cases[i].x_input, cases[i].y_input,
                            cases[i].expected, result);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Runs a range of values on a given function with a signature double -> double
 *
 * This function is only meant to test functions that returns the input value if it is
 * integral: f(x) -> x for x in N.
 *
 * \param func_name, the name of the tested function.
 * \param func, the function to call.
 */
static int
helper_range(const char *func_name, d_to_d_func func)
{
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("%s: Testing a range of %u values with steps of %u",
                       func_name,
                       RANGE_TEST_ITERATIONS,
                       RANGE_TEST_STEP);

    for (i = 0; i < RANGE_TEST_ITERATIONS; i++, test_value += RANGE_TEST_STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        result = func(test_value);
        if (result != test_value) { /* Only log failures to save performances */
            SDLTest_AssertPass("%s(%.1f), expected %.1f, got %.1f",
                               func_name, test_value,
                               test_value, result);
            return TEST_ABORTED;
        }
    }

    return TEST_COMPLETED;
}

/* ================= Test Case Implementation ================== */

/* SDL_floor tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
floor_infCases(void *args)
{
    double result;

    result = SDL_floor(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Floor(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_floor(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Floor(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
floor_zeroCases(void *args)
{
    const d_to_d zero_cases[] = { { 0.0, 0.0 }, { -0.0, -0.0 } };
    return helper_dtod("Floor", SDL_floor, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks the NaN case.
 */
static int
floor_nanCase(void *args)
{
    const double result = SDL_floor(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Floor(nan), expected nan, got %f",
                        result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
floor_roundNumbersCases(void *args)
{
    const d_to_d round_cases[] = {
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 15.0, 15.0 },
        { -15.0, -15.0 },
        { 125.0, 125.0 },
        { -125.0, -125.0 },
        { 1024.0, 1024.0 },
        { -1024.0, -1024.0 }
    };
    return helper_dtod("Floor", SDL_floor, round_cases, SDL_arraysize(round_cases));
}

/**
 * \brief Checks a set of fractions
 */
static int
floor_fractionCases(void *args)
{
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
    return helper_dtod("Floor", SDL_floor, frac_cases, SDL_arraysize(frac_cases));
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
floor_rangeTest(void *args)
{
    return helper_range("Floor", SDL_floor);
}

/* SDL_ceil tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
ceil_infCases(void *args)
{
    double result;

    result = SDL_ceil(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Ceil(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_ceil(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Ceil(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
ceil_zeroCases(void *args)
{
    const d_to_d zero_cases[] = { { 0.0, 0.0 }, { -0.0, -0.0 } };
    return helper_dtod("Ceil", SDL_ceil, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks the NaN case.
 */
static int
ceil_nanCase(void *args)
{
    const double result = SDL_ceil(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Ceil(nan), expected nan, got %f",
                        result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
ceil_roundNumbersCases(void *args)
{
    const d_to_d round_cases[] = {
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 15.0, 15.0 },
        { -15.0, -15.0 },
        { 125.0, 125.0 },
        { -125.0, -125.0 },
        { 1024.0, 1024.0 },
        { -1024.0, -1024.0 }
    };
    return helper_dtod("Ceil", SDL_ceil, round_cases, SDL_arraysize(round_cases));
}

/**
 * \brief Checks a set of fractions
 */
static int
ceil_fractionCases(void *args)
{
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
    return helper_dtod("Ceil", SDL_ceil, frac_cases, SDL_arraysize(frac_cases));
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
ceil_rangeTest(void *args)
{
    return helper_range("Ceil", SDL_ceil);
}

/* SDL_trunc tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
trunc_infCases(void *args)
{
    double result;

    result = SDL_trunc(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Trunc(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_trunc(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Trunc(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
trunc_zeroCases(void *args)
{
    const d_to_d zero_cases[] = { { 0.0, 0.0 }, { -0.0, -0.0 } };
    return helper_dtod("Trunc", SDL_trunc, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks the NaN case.
 */
static int
trunc_nanCase(void *args)
{
    const double result = SDL_trunc(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Trunc(nan), expected nan, got %f",
                        result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
trunc_roundNumbersCases(void *args)
{
    const d_to_d round_cases[] = {
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 15.0, 15.0 },
        { -15.0, -15.0 },
        { 125.0, 125.0 },
        { -125.0, -125.0 },
        { 1024.0, 1024.0 },
        { -1024.0, -1024.0 }
    };
    return helper_dtod("Trunc", SDL_trunc, round_cases, SDL_arraysize(round_cases));
}

/**
 * \brief Checks a set of fractions
 */
static int
trunc_fractionCases(void *args)
{
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
    return helper_dtod("Trunc", SDL_trunc, frac_cases, SDL_arraysize(frac_cases));
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
trunc_rangeTest(void *args)
{
    return helper_range("Trunc", SDL_trunc);
}

/* SDL_round tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
round_infCases(void *args)
{
    double result;

    result = SDL_round(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Round(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_round(-INFINITY);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Round(%f), expected %f, got %f",
                        -INFINITY, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
round_zeroCases(void *args)
{
    const d_to_d zero_cases[] = { { 0.0, 0.0 }, { -0.0, -0.0 } };
    return helper_dtod("Round", SDL_round, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks the NaN case.
 */
static int
round_nanCase(void *args)
{
    const double result = SDL_round(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Round(nan), expected nan, got %f",
                        result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int
round_roundNumbersCases(void *args)
{
    const d_to_d round_cases[] = {
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 15.0, 15.0 },
        { -15.0, -15.0 },
        { 125.0, 125.0 },
        { -125.0, -125.0 },
        { 1024.0, 1024.0 },
        { -1024.0, -1024.0 }
    };
    return helper_dtod("Round", SDL_round, round_cases, SDL_arraysize(round_cases));
}

/**
 * \brief Checks a set of fractions
 */
static int
round_fractionCases(void *args)
{
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
    return helper_dtod("Round", SDL_round, frac_cases, SDL_arraysize(frac_cases));
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
round_rangeTest(void *args)
{
    return helper_range("Round", SDL_round);
}

/* SDL_fabs tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
fabs_infCases(void *args)
{
    double result;

    result = SDL_fabs(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Fabs(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_fabs(-INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Fabs(%f), expected %f, got %f",
                        -INFINITY, INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero
 */
static int
fabs_zeroCases(void *args)
{
    const d_to_d zero_cases[] = { { 0.0, 0.0 }, { -0.0, 0.0 } };
    return helper_dtod("Fabs", SDL_fabs, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks the NaN case.
 */
static int
fabs_nanCase(void *args)
{
    const double result = SDL_fabs(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Fabs(nan), expected nan, got %f",
                        result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
fabs_rangeTest(void *args)
{
    return helper_range("Fabs", SDL_fabs);
}

/* SDL_copysign tests functions */

/**
 * \brief Checks positive and negative inifnity.
 */
static int
copysign_infCases(void *args)
{
    double result;

    result = SDL_copysign(INFINITY, -1.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Copysign(%f,%.1f), expected %f, got %f",
                        INFINITY, -1.0, -INFINITY, result);

    result = SDL_copysign(INFINITY, 1.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Copysign(%f,%.1f), expected %f, got %f",
                        INFINITY, 1.0, INFINITY, result);

    result = SDL_copysign(-INFINITY, -1.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Copysign(%f,%.1f), expected %f, got %f",
                        -INFINITY, -1.0, -INFINITY, result);

    result = SDL_copysign(-INFINITY, 1.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Copysign(%f,%.1f), expected %f, got %f",
                        -INFINITY, 1.0, INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
copysign_zeroCases(void *args)
{
    const dd_to_d zero_cases[] = {
        { 0.0, 1.0, 0.0 },
        { 0.0, -1.0, -0.0 },
        { -0.0, 1.0, 0.0 },
        { -0.0, -1.0, -0.0 }
    };
    return helper_ddtod("Copysign", SDL_copysign, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks the NaN cases.
 */
static int
copysign_nanCases(void *args)
{
    double result;

    result = SDL_copysign(NAN, 1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Copysign(nan,1.0), expected nan, got %f",
                        result);

    result = SDL_copysign(NAN, -1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Copysign(nan,-1.0), expected nan, got %f",
                        result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
copysign_rangeTest(void *args)
{
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Copysign: Testing a range of %u values with steps of %u",
                       RANGE_TEST_ITERATIONS,
                       RANGE_TEST_STEP);

    for (i = 0; i < RANGE_TEST_ITERATIONS; i++, test_value += RANGE_TEST_STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        /* Only log failures to save performances */
        result = SDL_copysign(test_value, 1.0);
        if (result != test_value) {
            SDLTest_AssertPass("Copysign(%.1f,%.1f), expected %.1f, got %.1f",
                               test_value, 1.0,
                               test_value, result);
            return TEST_ABORTED;
        }

        result = SDL_copysign(test_value, -1.0);
        if (result != -test_value) {
            SDLTest_AssertPass("Copysign(%.1f,%.1f), expected %.1f, got %.1f",
                               test_value, -1.0,
                               -test_value, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_fmod tests functions */

/**
 * \brief Checks division of positive and negative inifnity.
 */
static int
fmod_divOfInfCases(void *args)
{
    double result;

    result = SDL_fmod(INFINITY, -1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(%f,%.1f), expected %f, got %f",
                        INFINITY, -1.0, NAN, result);

    result = SDL_fmod(INFINITY, 1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(%f,%.1f), expected %f, got %f",
                        INFINITY, 1.0, NAN, result);

    result = SDL_fmod(-INFINITY, -1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(%f,%.1f), expected %f, got %f",
                        -INFINITY, -1.0, NAN, result);

    result = SDL_fmod(-INFINITY, 1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(%f,%.1f), expected %f, got %f",
                        -INFINITY, 1.0, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks division by positive and negative inifnity.
 */
static int
fmod_divByInfCases(void *args)
{
    double result;

    result = SDL_fmod(1.0, INFINITY);
    SDLTest_AssertCheck(1.0 == result,
                        "Fmod(%.1f,%f), expected %f, got %f",
                        1.0, INFINITY, 1.0, result);

    result = SDL_fmod(-1.0, INFINITY);
    SDLTest_AssertCheck(-1.0 == result,
                        "Fmod(%.1f,%f), expected %f, got %f",
                        -1.0, INFINITY, -1.0, result);

    result = SDL_fmod(1.0, -INFINITY);
    SDLTest_AssertCheck(1.0 == result,
                        "Fmod(%.1f,%f), expected %f, got %f",
                        1.0, -INFINITY, 1.0, result);

    result = SDL_fmod(-1.0, -INFINITY);
    SDLTest_AssertCheck(-1.0 == result,
                        "Fmod(%.1f,%f), expected %f, got %f",
                        -1.0, -INFINITY, -1.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks division of positive and negative zero.
 */
static int
fmod_divOfZeroCases(void *args)
{
    const dd_to_d zero_cases[] = {
        { 0.0, 1.0, 0.0 },
        { 0.0, -1.0, 0.0 },
        { -0.0, 1.0, -0.0 },
        { -0.0, -1.0, -0.0 }
    };
    return helper_ddtod("Fmod", SDL_fmod, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks division by positive and negative zero.
 */
static int
fmod_divByZeroCases(void *args)
{
    double result;

    result = SDL_fmod(1.0, 0.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(1.0,0.0), expected nan, got %f",
                        result);

    result = SDL_fmod(-1.0, 0.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(-1.0,0.0), expected nan, got %f",
                        result);

    result = SDL_fmod(1.0, -0.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(1.0,-0.0), expected nan, got %f",
                        result);

    result = SDL_fmod(-1.0, -0.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(-1.0,-0.0), expected nan, got %f",
                        result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN cases.
 */
static int
fmod_nanCases(void *args)
{
    double result;

    result = SDL_fmod(NAN, 1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(nan,1.0), expected nan, got %f",
                        result);

    result = SDL_fmod(NAN, -1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(nan,-1.0), expected nan, got %f",
                        result);

    result = SDL_fmod(1.0, NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(1.0,nan), expected nan, got %f",
                        result);

    result = SDL_fmod(-1.0, NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Fmod(-1.0,nan), expected nan, got %f",
                        result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular values.
 */
static int
fmod_regularCases(void *args)
{
    const dd_to_d regular_cases[] = {
        { 3.5, 2.0, 1.5 },
        { -6.25, 3.0, -0.25 },
        { 7.5, 2.5, 0.0 },
        { 2.0 / 3.0, -1.0 / 3.0, 0.0 }
    };
    return helper_ddtod("Fmod", SDL_fmod, regular_cases, SDL_arraysize(regular_cases));
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX
 */
static int
fmod_rangeTest(void *args)
{
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Fmod: Testing a range of %u values with steps of %u",
                       RANGE_TEST_ITERATIONS,
                       RANGE_TEST_STEP);

    for (i = 0; i < RANGE_TEST_ITERATIONS; i++, test_value += RANGE_TEST_STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        /* Only log failures to save performances */
        result = SDL_fmod(test_value, 1.0);
        if (0.0 != result) {
            SDLTest_AssertPass("Fmod(%.1f,%.1f), expected %.1f, got %.1f",
                               test_value, 1.0,
                               0.0, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* SDL_floor test cases */

static const SDLTest_TestCaseReference floorTestInf = {
    (SDLTest_TestCaseFp) floor_infCases, "floor_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTestZero = {
    (SDLTest_TestCaseFp) floor_zeroCases, "floor_zeroCases",
    "Check positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTestNan = {
    (SDLTest_TestCaseFp) floor_nanCase, "floor_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTestRound = {
    (SDLTest_TestCaseFp) floor_roundNumbersCases, "floor_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTestFraction = {
    (SDLTest_TestCaseFp) floor_fractionCases, "floor_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference floorTestRange = {
    (SDLTest_TestCaseFp) floor_rangeTest, "floor_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_ceil test cases */

static const SDLTest_TestCaseReference ceilTestInf = {
    (SDLTest_TestCaseFp) ceil_infCases, "ceil_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTestZero = {
    (SDLTest_TestCaseFp) ceil_zeroCases, "ceil_zeroCases",
    "Check positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTestNan = {
    (SDLTest_TestCaseFp) ceil_nanCase, "ceil_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTestRound = {
    (SDLTest_TestCaseFp) ceil_roundNumbersCases, "ceil_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTestFraction = {
    (SDLTest_TestCaseFp) ceil_fractionCases, "ceil_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference ceilTestRange = {
    (SDLTest_TestCaseFp) ceil_rangeTest, "ceil_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_trunc test cases */

static const SDLTest_TestCaseReference truncTestInf = {
    (SDLTest_TestCaseFp) trunc_infCases, "trunc_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTestZero = {
    (SDLTest_TestCaseFp) trunc_zeroCases, "trunc_zeroCases",
    "Check positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTestNan = {
    (SDLTest_TestCaseFp) trunc_nanCase, "trunc_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTestRound = {
    (SDLTest_TestCaseFp) trunc_roundNumbersCases, "trunc_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTestFraction = {
    (SDLTest_TestCaseFp) trunc_fractionCases, "trunc_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference truncTestRange = {
    (SDLTest_TestCaseFp) trunc_rangeTest, "trunc_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_round test cases */

static const SDLTest_TestCaseReference roundTestInf = {
    (SDLTest_TestCaseFp) round_infCases, "round_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTestZero = {
    (SDLTest_TestCaseFp) round_zeroCases, "round_zeroCases",
    "Check positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTestNan = {
    (SDLTest_TestCaseFp) round_nanCase, "round_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTestRound = {
    (SDLTest_TestCaseFp) round_roundNumbersCases, "round_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTestFraction = {
    (SDLTest_TestCaseFp) round_fractionCases, "round_fractionCases",
    "Check a set of fractions", TEST_ENABLED
};
static const SDLTest_TestCaseReference roundTestRange = {
    (SDLTest_TestCaseFp) round_rangeTest, "round_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_fabs test cases */

static const SDLTest_TestCaseReference fabsTestInf = {
    (SDLTest_TestCaseFp) fabs_infCases, "fabs_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference fabsTestZero = {
    (SDLTest_TestCaseFp) fabs_zeroCases, "fabs_zeroCases",
    "Check positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference fabsTestNan = {
    (SDLTest_TestCaseFp) fabs_nanCase, "fabs_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference fabsTestRange = {
    (SDLTest_TestCaseFp) fabs_rangeTest, "fabs_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_copysign test cases */

static const SDLTest_TestCaseReference copysignTestInf = {
    (SDLTest_TestCaseFp) copysign_infCases, "copysign_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference copysignTestZero = {
    (SDLTest_TestCaseFp) copysign_zeroCases, "copysign_zeroCases",
    "Check positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference copysignTestNan = {
    (SDLTest_TestCaseFp) copysign_nanCases, "copysign_nanCases",
    "Check the NaN special cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference copysignTestRange = {
    (SDLTest_TestCaseFp) copysign_rangeTest, "copysign_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_fmod test cases */

static const SDLTest_TestCaseReference fmodTestDivOfInf = {
    (SDLTest_TestCaseFp) fmod_divOfInfCases, "fmod_divOfInfCases",
    "Check division of positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference fmodTestDivByInf = {
    (SDLTest_TestCaseFp) fmod_divByInfCases, "fmod_divByInfCases",
    "Check division by positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference fmodTestDivOfZero = {
    (SDLTest_TestCaseFp) fmod_divOfZeroCases, "fmod_divOfZeroCases",
    "Check division of positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference fmodTestDivByZero = {
    (SDLTest_TestCaseFp) fmod_divByZeroCases, "fmod_divByZeroCases",
    "Check division by positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference fmodTestNan = {
    (SDLTest_TestCaseFp) fmod_nanCases, "fmod_nanCases",
    "Check the NaN special cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference fmodTestRegular = {
    (SDLTest_TestCaseFp) fmod_regularCases, "fmod_regularCases",
    "Check a set of regular values", TEST_ENABLED
};
static const SDLTest_TestCaseReference fmodTestRange = {
    (SDLTest_TestCaseFp) fmod_rangeTest, "fmod_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

static const SDLTest_TestCaseReference *mathTests[] = {
    &floorTestInf, &floorTestZero, &floorTestNan,
    &floorTestRound, &floorTestFraction, &floorTestRange,

    &ceilTestInf, &ceilTestZero, &ceilTestNan,
    &ceilTestRound, &ceilTestFraction, &ceilTestRange,

    &truncTestInf, &truncTestZero, &truncTestNan,
    &truncTestRound, &truncTestFraction, &truncTestRange,

    &roundTestInf, &roundTestZero, &roundTestNan,
    &roundTestRound, &roundTestFraction, &roundTestRange,

    &fabsTestInf, &fabsTestZero, &fabsTestNan, &fabsTestRange,

    &copysignTestInf, &copysignTestZero, &copysignTestNan, &copysignTestRange,

    &fmodTestDivOfInf, &fmodTestDivByInf, &fmodTestDivOfZero, &fmodTestDivByZero,
    &fmodTestNan, &fmodTestRegular, &fmodTestRange,

    NULL
};

SDLTest_TestSuiteReference mathTestSuite = { "Math", NULL, mathTests, NULL };
