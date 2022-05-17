/**
 * Math test suite
 */

#include <math.h>
#include <float.h>

#include "SDL.h"
#include "SDL_test.h"

/* Range tests parameters */
#define RANGE_TEST_ITERATIONS 10000000
#define RANGE_TEST_STEP       SDL_MAX_UINT32 / RANGE_TEST_ITERATIONS

/* Define the Euler constant */
#ifndef M_E
#define EULER 2.7182818284590450907955982984276488423347473144531250
#else
#define EULER M_E
#endif

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
            SDLTest_AssertCheck(SDL_FALSE,
                                "%s(%.1f), expected %.1f, got %.1f",
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
            SDLTest_AssertCheck(SDL_FALSE,
                                "Copysign(%.1f,%.1f), expected %.1f, got %.1f",
                                test_value, 1.0, test_value, result);
            return TEST_ABORTED;
        }

        result = SDL_copysign(test_value, -1.0);
        if (result != -test_value) {
            SDLTest_AssertCheck(SDL_FALSE,
                                "Copysign(%.1f,%.1f), expected %.1f, got %.1f",
                                test_value, -1.0, -test_value, result);
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
            SDLTest_AssertCheck(SDL_FALSE,
                                "Fmod(%.1f,%.1f), expected %.1f, got %.1f",
                                test_value, 1.0, 0.0, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_exp tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
exp_infCases(void *args)
{
    double result;

    result = SDL_exp(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Exp(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_exp(-INFINITY);
    SDLTest_AssertCheck(0.0 == result,
                        "Exp(%f), expected %f, got %f",
                        -INFINITY, 0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
exp_zeroCases(void *args)
{
    const d_to_d zero_cases[] = {
        { 0.0, 1.0 },
        { -0.0, 1.0 }
    };
    return helper_dtod("Exp", SDL_exp, zero_cases, SDL_arraysize(zero_cases));
}

/**
 * \brief Checks for overflow.
 *
 * This test is skipped for double types larger than 64 bits.
 */
static int
exp_overflowCase(void *args)
{
    double result;

    if (sizeof(double) > 8) {
        return TEST_SKIPPED;
    }

    result = SDL_exp(710.0);
    SDLTest_AssertCheck(isinf(result),
                        "Exp(%f), expected %f, got %f",
                        710.0, INFINITY, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks the base case of 1.0.
 */
static int
exp_baseCase(void *args)
{
    const double result = SDL_exp(1.0);
    SDLTest_AssertCheck(EULER == result,
                        "Exp(%f), expected %f, got %f",
                        1.0, EULER, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular cases.
 */
static int
exp_regularCases(void *args)
{
    /* Hexadecimal floating constants are not supported on C89 compilers */
    const d_to_d regular_cases[] = {
        { -101.0, 1.36853947117385291381565719268793547578002532127613087E-44 },
        { -15.73, 0.00000014741707833928422931856502906683425990763681 },
        { -1.0, 0.36787944117144233402427744294982403516769409179688 },
        { -0.5, 0.60653065971263342426311737654032185673713684082031 },
        { 0.5, 1.64872127070012819416433558217249810695648193359375 },
        { 2.25, 9.48773583635852624240669683786109089851379394531250 },
        { 34.125, 661148770968660.375 },
        { 112.89, 10653788283588960962604279261058893737879589093376.0 },
        { 539.483, 1970107755334319939701129934673541628417235942656909222826926175622435588279443011110464355295725187195188154768877850257012251677751742837992843520967922303961718983154427294786640886286983037548604937796221048661733679844353544028160.0 },
    };
    return helper_dtod("Exp", SDL_exp, regular_cases, SDL_arraysize(regular_cases));
}

/* SDL_log tests functions */

/**
 * \brief Checks limits (zeros and positive infinity).
 */
static int
log_limitCases(void *args)
{
    double result;

    result = SDL_log(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Log(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_log(0.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Log(%f), expected %f, got %f",
                        0.0, -INFINITY, result);

    result = SDL_log(-0.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Log(%f), expected %f, got %f",
                        -0.0, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks some base cases.
 */
static int
log_baseCases(void *args)
{
    double result;

    result = SDL_log(1.0);
    SDLTest_AssertCheck(0.0 == result,
                        "Log(%f), expected %f, got %f",
                        1.0, 0.0, result);

    result = SDL_log(EULER);
    SDLTest_AssertCheck(1.0 == result,
                        "Log(%f), expected %f, got %f",
                        EULER, 1.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the nan cases.
 */
static int
log_nanCases(void *args)
{
    double result;

    result = SDL_log(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Log(%f), expected %f, got %f",
                        NAN, NAN, result);

    result = SDL_log(-1234.5678);
    SDLTest_AssertCheck(isnan(result),
                        "Log(%f), expected %f, got %f",
                        -1234.5678, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular cases.
 */
static int
log_regularCases(void *args)
{
    const d_to_d regular_cases[] = {
        { 5.0, 1.60943791243410028179994242236716672778129577636718750 },
        { 10.0, 2.302585092994045901093613792909309267997741699218750 },
        { 56.32, 4.031049711849786554296315443934872746467590332031250 },
        { 789.123, 6.670922202231861497523368598194792866706848144531250 },
        { 2734.876324, 7.91384149408957959792587644187733530998229980468750 }
    };
    return helper_dtod("Log", SDL_log, regular_cases, SDL_arraysize(regular_cases));
}

/* SDL_log10 tests functions */

/**
 * \brief Checks limits (zeros and positive infinity).
 */
static int
log10_limitCases(void *args)
{
    double result;

    result = SDL_log10(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Log10(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);

    result = SDL_log10(0.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Log10(%f), expected %f, got %f",
                        0.0, -INFINITY, result);

    result = SDL_log10(-0.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Log10(%f), expected %f, got %f",
                        -0.0, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks some base cases.
 */
static int
log10_baseCases(void *args)
{
    const d_to_d base_cases[] = {
        { 1.0, 0.0 },
        { 10.0, 1.0 },
        { 100.0, 2.0 },
        { 1000.0, 3.0 },
        { 10000.0, 4.0 },
        { 100000.0, 5.0 },
        { 1000000.0, 6.0 },
        { 10000000.0, 7.0 },
        { 100000000.0, 8.0 },
        { 1000000000.0, 9.0 },
    };
    return helper_dtod("Log10", SDL_log10, base_cases, SDL_arraysize(base_cases));
}

/**
 * \brief Checks the nan cases.
 */
static int
log10_nanCases(void *args)
{
    double result;

    result = SDL_log10(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Log10(%f), expected %f, got %f",
                        NAN, NAN, result);

    result = SDL_log10(-1234.5678);
    SDLTest_AssertCheck(isnan(result),
                        "Log10(%f), expected %f, got %f",
                        -1234.5678, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular cases.
 */
static int
log10_regularCases(void *args)
{
    const d_to_d regular_cases[] = {
        { 5.0, 0.698970004336018857493684208748163655400276184082031250 },
        { 12.5, 1.09691001300805646145875016372883692383766174316406250 },
        { 56.32, 1.750662646134055755453573510749265551567077636718750 },
        { 789.123, 2.8971447016351858927407647570362314581871032714843750 },
        { 2734.876324, 3.436937691540090433761633903486654162406921386718750 }
    };
    return helper_dtod("Log10", SDL_log10, regular_cases, SDL_arraysize(regular_cases));
}

/* SDL_pow tests functions */

/* Tests with positive and negative infinities as exponents */

/**
 * \brief Checks the cases where the base is negative one and the exponent is infinity.
 */
static int
pow_baseNOneExpInfCases(void *args)
{
    double result;

    result = SDL_pow(-1.0, INFINITY);
    SDLTest_AssertCheck(1.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -1.0, INFINITY, 1.0, result);

    result = SDL_pow(-1.0, -INFINITY);
    SDLTest_AssertCheck(1.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -1.0, -INFINITY, 1.0, result);

    return TEST_COMPLETED;
}
/**
 * \brief Checks the case where the base is zero and the exponent is negative infinity.
 */
static int
pow_baseZeroExpNInfCases(void *args)
{
    double result;

    result = SDL_pow(0.0, -INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, -INFINITY, INFINITY, result);

    result = SDL_pow(-0.0, -INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, -INFINITY, INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the remaining cases where the exponent is infinity.
 */
static int
pow_expInfCases(void *args)
{
    double result;

    result = SDL_pow(0.5, INFINITY);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.5, INFINITY, 0.0, result);

    result = SDL_pow(1.5, INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        1.5, INFINITY, INFINITY, result);

    result = SDL_pow(0.5, -INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.5, INFINITY, INFINITY, result);

    result = SDL_pow(1.5, -INFINITY);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        1.5, -INFINITY, 0.0, result);

    return TEST_COMPLETED;
}

/* Tests with positive and negative infinities as base */

/**
 * \brief Checks the cases with positive infinity as base.
 */
static int
pow_basePInfCases(void *args)
{
    double result;

    result = SDL_pow(INFINITY, -3.0);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        INFINITY, -3.0, 0.0, result);

    result = SDL_pow(INFINITY, 2.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        INFINITY, 2.0, INFINITY, result);

    result = SDL_pow(INFINITY, -2.12345);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        INFINITY, -2.12345, 0.0, result);

    result = SDL_pow(INFINITY, 3.1345);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        INFINITY, 3.12345, INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the cases with negative infinity as base.
 */
static int
pow_baseNInfCases(void *args)
{
    double result;

    result = SDL_pow(-INFINITY, -3.0);
    SDLTest_AssertCheck(-0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -INFINITY, -3.0, -0.0, result);

    result = SDL_pow(-INFINITY, -2.0);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -INFINITY, -2.0, 0.0, result);

    result = SDL_pow(-INFINITY, -5.5);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -INFINITY, -5.5, 0.0, result);

    result = SDL_pow(-INFINITY, 3.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -INFINITY, 3.0, -INFINITY, result);

    result = SDL_pow(-INFINITY, 2.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -INFINITY, 2.0, INFINITY, result);

    result = SDL_pow(-INFINITY, 5.5);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -INFINITY, 5.5, INFINITY, result);

    return TEST_COMPLETED;
}

/* Tests related to nan */

/**
 * \brief Checks the case where the base is finite and negative and exponent is finite and non-integer.
 */
static int
pow_badOperationCase(void *args)
{
    const double result = SDL_pow(-2.0, 4.2);
    SDLTest_AssertCheck(isnan(result),
                        "Pow(%f,%f), expected %f, got %f",
                        -2.0, 4.2, NAN, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks the case where the exponent is nan but the base is 1.
 */
static int
pow_base1ExpNanCase(void *args)
{
    const double result = SDL_pow(1.0, NAN);
    SDLTest_AssertCheck(1.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        1.0, NAN, 1.0, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks the cases where the base is nan but the exponent is 0.
 */
static int
pow_baseNanExp0Cases(void *args)
{
    double result;

    result = SDL_pow(NAN, 0.0);
    SDLTest_AssertCheck(1.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        NAN, 0.0, 1.0, result);
    return TEST_COMPLETED;

    result = SDL_pow(NAN, -0.0);
    SDLTest_AssertCheck(1.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        NAN, -0.0, 1.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks the cases where either argument is nan.
 */
static int
pow_nanArgsCases(void *args)
{
    double result;

    result = SDL_pow(7.8, NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Pow(%f,%f), expected %f, got %f",
                        7.8, NAN, NAN, result);

    result = SDL_pow(NAN, 10.0);
    SDLTest_AssertCheck(isnan(result),
                        "Pow(%f,%f), expected %f, got %f",
                        NAN, 10.0, NAN, result);

    result = SDL_pow(NAN, NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Pow(%f,%f), expected %f, got %f",
                        NAN, NAN, NAN, result);

    return TEST_COMPLETED;
}

/* Tests with positive and negative zeros as base */

/**
 * \brief Checks cases with negative zero as base and an odd exponent.
 */
static int
pow_baseNZeroExpOddCases(void *args)
{
    double result;

    result = SDL_pow(-0.0, -3.0);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, -3.0, -INFINITY, result);

    result = SDL_pow(-0.0, 3.0);
    SDLTest_AssertCheck(-0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, 3.0, -0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks cases with positive zero as base and an odd exponent.
 */
static int
pow_basePZeroExpOddCases(void *args)
{
    double result;

    result = SDL_pow(0.0, -5.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, -5.0, INFINITY, result);

    result = SDL_pow(0.0, 5.0);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, 5.0, 0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks cases with negative zero as base and the exponent is finite and even or non-integer.
 */
static int
pow_baseNZeroCases(void *args)
{
    double result;

    result = SDL_pow(-0.0, -3.5);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, -3.5, INFINITY, result);

    result = SDL_pow(-0.0, -4.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, -4.0, INFINITY, result);

    result = SDL_pow(-0.0, 3.5);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, 3.5, 0.0, result);

    result = SDL_pow(-0.0, 4.0);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        -0.0, 4.0, 0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks cases with positive zero as base and the exponent is finite and even or non-integer.
 */
static int
pow_basePZeroCases(void *args)
{
    double result;

    result = SDL_pow(0.0, -3.5);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, -3.5, INFINITY, result);

    result = SDL_pow(0.0, -4.0);
    SDLTest_AssertCheck(INFINITY == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, -4.0, INFINITY, result);

    result = SDL_pow(0.0, 3.5);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, 3.5, 0.0, result);

    result = SDL_pow(0.0, 4.0);
    SDLTest_AssertCheck(0.0 == result,
                        "Pow(%f,%f), expected %f, got %f",
                        0.0, 4.0, 0.0, result);

    return TEST_COMPLETED;
}

/* Remaining tests */

/**
 * \brief Checks a set of regular values.
 */
static int
pow_regularCases(void *args)
{
    const dd_to_d regular_cases[] = {
        { -391.25, -2.0, 0.00000653267870448815438463212659780943170062528224661946296691894531250 },
        { -72.3, 12.0, 20401381050275984310272.0 },
        { -5.0, 3.0, -125.0 },
        { 3.0, 2.5, 15.58845726811989607085706666111946105957031250 },
        { 39.23, -1.5, 0.0040697950366865498147972424192175822099670767784118652343750 },
        { 478.972, 12.125, 315326359630449587856007411793920.0 }
    };
    return helper_ddtod("Pow", SDL_pow, regular_cases, SDL_arraysize(regular_cases));
}

/**
 * \brief Checks the powers of two from 1 to 8.
 */
static int
pow_powerOfTwo(void *args)
{
    const dd_to_d power_of_two_cases[] = {
        { 2.0, 1, 2.0 },
        { 2.0, 2, 4.0 },
        { 2.0, 3, 8.0 },
        { 2.0, 4, 16.0 },
        { 2.0, 5, 32.0 },
        { 2.0, 6, 64.0 },
        { 2.0, 7, 128.0 },
        { 2.0, 8, 256.0 },
    };
    return helper_ddtod("Pow", SDL_pow, power_of_two_cases, SDL_arraysize(power_of_two_cases));
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX to the power of 0.
 */
static int
pow_rangeTest(void *args)
{
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Pow: Testing a range of %u values with steps of %u",
                       RANGE_TEST_ITERATIONS,
                       RANGE_TEST_STEP);

    for (i = 0; i < RANGE_TEST_ITERATIONS; i++, test_value += RANGE_TEST_STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        /* Only log failures to save performances */
        result = SDL_pow(test_value, 0.0);
        if (result != 1.0) {
            SDLTest_AssertCheck(SDL_FALSE,
                                "Pow(%.1f,%.1f), expected %.1f, got %.1f",
                                test_value, 1.0, 1.0, result);
            return TEST_ABORTED;
        }

        result = SDL_pow(test_value, -0.0);
        if (result != 1.0) {
            SDLTest_AssertCheck(SDL_FALSE,
                                "Pow(%.1f,%.1f), expected %.1f, got %.1f",
                                test_value, -0.0, 1.0, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_sqrt tests functions */

/**
 * \brief Checks for positive infinity.
 */
static int
sqrt_infCase(void *args)
{
    const double result = SDL_sqrt(INFINITY);
    SDLTest_AssertCheck(INFINITY == result,
                        "Sqrt(%f), expected %f, got %f",
                        INFINITY, INFINITY, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks for the nan case.
 */
static int
sqrt_nanCase(void *args)
{
    const double result = SDL_sqrt(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Sqrt(%f), expected %f, got %f",
                        NAN, NAN, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks for out of domain values (<0).
 */
static int
sqrt_outOfDomainCases(void *args)
{
    double result;

    result = SDL_sqrt(-1.0);
    SDLTest_AssertCheck(isnan(result),
                        "Sqrt(%f), expected %f, got %f",
                        -1.0, NAN, result);

    result = SDL_sqrt(-12345.6789);
    SDLTest_AssertCheck(isnan(result),
                        "Sqrt(%f), expected %f, got %f",
                        -12345.6789, NAN, result);

    result = SDL_sqrt(-INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Sqrt(%f), expected %f, got %f",
                        -INFINITY, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of base cases.
 */
static int
sqrt_baseCases(void *args)
{
    const d_to_d base_cases[] = {
        { -0.0, -0.0 },
        { 0.0, 0.0 },
        { 1.0, 1.0 }
    };
    return helper_dtod("Sqrt", SDL_sqrt, base_cases, SDL_arraysize(base_cases));
}

/**
 * \brief Checks a set of regular cases.
 */
static int
sqrt_regularCases(void *args)
{
    const d_to_d regular_cases[] = {
        { 4.0, 2.0 },
        { 9.0, 3.0 },
        { 27.2, 5.21536192416211896727418206864967942237854003906250 },
        { 240.250, 15.5 },
        { 1337.0, 36.565010597564445049556525191292166709899902343750 },
        { 2887.12782400000014604302123188972473144531250, 53.732 },
        { 65600.0156250, 256.125 }
    };
    return helper_dtod("Sqrt", SDL_sqrt, regular_cases, SDL_arraysize(regular_cases));
}

/* SDL_scalbn tests functions */

/**
 * \brief Checks for positive and negative infinity arg.
 */
static int
scalbn_infCases(void *args)
{
    double result;

    result = SDL_scalbn(INFINITY, 1);
    SDLTest_AssertCheck(INFINITY == result,
                        "Scalbn(%f,%d), expected %f, got %f",
                        INFINITY, 1, INFINITY, result);

    result = SDL_scalbn(-INFINITY, 1);
    SDLTest_AssertCheck(-INFINITY == result,
                        "Scalbn(%f,%d), expected %f, got %f",
                        -INFINITY, 1, -INFINITY, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks for positive and negative zero arg.
 */
static int
scalbn_baseZeroCases(void *args)
{
    double result;

    result = SDL_scalbn(0.0, 1);
    SDLTest_AssertCheck(0.0 == result,
                        "Scalbn(%f,%d), expected %f, got %f",
                        0.0, 1, 0.0, result);

    result = SDL_scalbn(-0.0, 1);
    SDLTest_AssertCheck(-0.0 == result,
                        "Scalbn(%f,%d), expected %f, got %f",
                        -0.0, 1, -0.0, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks for zero exp.
 */
static int
scalbn_expZeroCase(void *args)
{
    const double result = SDL_scalbn(42.0, 0);
    SDLTest_AssertCheck(42.0 == result,
                        "Scalbn(%f,%d), expected %f, got %f",
                        42.0, 0, 42.0, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks for NAN arg.
 */
static int
scalbn_nanCase(void *args)
{
    const double result = SDL_scalbn(NAN, 2);
    SDLTest_AssertCheck(isnan(result),
                        "Scalbn(%f,%d), expected %f, got %f",
                        NAN, 2, NAN, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular values.
 *
 * This test depends on SDL_pow functionning.
 */
static int
scalbn_regularCases(void *args)
{
    double result, expected;

    result = SDL_scalbn(2.0, 2);
    expected = 2.0 * SDL_pow(FLT_RADIX, 2);
    SDLTest_AssertCheck(result == expected,
                        "Scalbn(%f,%d), expected %f, got %f",
                        2.0, 2, expected, result);

    result = SDL_scalbn(1.0, 13);
    expected = 1.0 * SDL_pow(FLT_RADIX, 13);
    SDLTest_AssertCheck(result == expected,
                        "Scalbn(%f,%d), expected %f, got %f",
                        1.0, 13, expected, result);

    result = SDL_scalbn(2.0, -5);
    expected = 2.0 * SDL_pow(FLT_RADIX, -5);
    SDLTest_AssertCheck(result == expected,
                        "Scalbn(%f,%d), expected %f, got %f",
                        2.0, -5, expected, result);

    result = SDL_scalbn(-1.0, -13);
    expected = -1.0 * SDL_pow(FLT_RADIX, -13);
    SDLTest_AssertCheck(result == expected,
                        "Scalbn(%f,%d), expected %f, got %f",
                        -1.0, -13, expected, result);

    return TEST_COMPLETED;
}

/* SDL_cos tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
cos_infCases(void *args)
{
    double result;

    result = SDL_cos(INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Cos(%f), expected %f, got %f",
                        INFINITY, NAN, result);

    result = SDL_cos(-INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Cos(%f), expected %f, got %f",
                        -INFINITY, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks for nan.
 */
static int
cos_nanCase(void *args)
{
    const double result = SDL_cos(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Cos(%f), expected %f, got %f",
                        NAN, NAN, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular values.
 */
static int
cos_regularCases(void *args)
{
    const d_to_d regular_cases[] = {
        { -M_PI, -1.0 },
        { -0.0, 1.0 },
        { 0.0, 1.0 },
        { M_PI, -1.0 }
    };
    return helper_dtod("Cos", SDL_cos, regular_cases, SDL_arraysize(regular_cases));
}

/**
 * \brief Checks cosine precision for the first 10 decimals.
 *
 * This function depends on SDL_floor functioning.
 */
static int
cos_precisionTest(void *args)
{
    Uint32 i;
    Uint32 iterations = 20;
    double angle = 0.0;
    double step = 2.0 * M_PI / iterations;
    const double expected[] = {
        10000000000.0, 9510565162.0, 8090169943.0, 5877852522.0, 3090169943.0,
        0.0, -3090169943.0, -5877852522.0, -8090169943.0, -9510565162.0,
        -10000000000.0, -9510565162.0, -8090169943.0, -5877852522.0, -3090169943.0,
        0.0, 3090169943.0, 5877852522.0, 8090169943.0, 9510565162.0
    };
    for (i = 0; i < iterations; i++, angle += step) {
        double result = SDL_cos(angle) * 1.0E10;
        SDLTest_AssertCheck(SDL_trunc(result) == expected[i],
                            "Cos(%f), expected %f, got %f",
                            angle, expected[i], result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX.
 */
static int
cos_rangeTest(void *args)
{
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Cos: Testing a range of %u values with steps of %u",
                       RANGE_TEST_ITERATIONS,
                       RANGE_TEST_STEP);

    for (i = 0; i < RANGE_TEST_ITERATIONS; i++, test_value += RANGE_TEST_STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        /* Only log failures to save performances */
        result = SDL_cos(test_value);
        if (result < -1.0 || result > 1.0) {
            SDLTest_AssertCheck(SDL_FALSE,
                                "Cos(%.1f), expected [%.1f,%.1f], got %.1f",
                                test_value, -1.0, 1.0, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_sin tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
sin_infCases(void *args)
{
    double result;

    result = SDL_sin(INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Sin(%f), expected %f, got %f",
                        INFINITY, NAN, result);

    result = SDL_sin(-INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Sin(%f), expected %f, got %f",
                        -INFINITY, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks for nan.
 */
static int
sin_nanCase(void *args)
{
    const double result = SDL_sin(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Sin(%f), expected %f, got %f",
                        NAN, NAN, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks a set of regular values.
 */
static int
sin_regularCases(void *args)
{
    const d_to_d regular_cases[] = {
        { -M_PI / 2, -1.0 },
        { -0.0, -0.0 },
        { 0.0, 0.0 },
        { M_PI / 2, 1.0 }
    };
    return helper_dtod("Sin", SDL_sin, regular_cases, SDL_arraysize(regular_cases));
}

/**
 * \brief Checks sine precision for the first 10 decimals.
 */
static int
sin_precisionTest(void *args)
{
    Uint32 i;
    Uint32 iterations = 20;
    double angle = 0.0;
    double step = 2.0 * M_PI / iterations;
    const double expected[] = {
        0, 3090169943, 5877852522, 8090169943, 9510565162,
        10000000000, 9510565162, 8090169943, 5877852522, 3090169943,
        0, -3090169943, -5877852522, -8090169943, -9510565162,
        -10000000000, -9510565162, -8090169943, -5877852522, -3090169943
    };
    for (i = 0; i < iterations; i++, angle += step) {
        double result = SDL_sin(angle) * 1.0E10;
        SDLTest_AssertCheck(SDL_trunc(result) == expected[i],
                            "Sin(%f), expected %f, got %f",
                            angle, expected[i], result);
    }
    return TEST_COMPLETED;
}

/**
 * \brief Checks a range of values between 0 and UINT32_MAX.
 */
static int
sin_rangeTest(void *args)
{
    Uint32 i;
    double test_value = 0.0;

    SDLTest_AssertPass("Sin: Testing a range of %u values with steps of %u",
                       RANGE_TEST_ITERATIONS,
                       RANGE_TEST_STEP);

    for (i = 0; i < RANGE_TEST_ITERATIONS; i++, test_value += RANGE_TEST_STEP) {
        double result;
        /* These are tested elsewhere */
        if (isnan(test_value) || isinf(test_value)) {
            continue;
        }

        /* Only log failures to save performances */
        result = SDL_sin(test_value);
        if (result < -1.0 || result > 1.0) {
            SDLTest_AssertCheck(SDL_FALSE,
                                "Sin(%.1f), expected [%.1f,%.1f], got %.1f",
                                test_value, -1.0, 1.0, result);
            return TEST_ABORTED;
        }
    }
    return TEST_COMPLETED;
}

/* SDL_tan tests functions */

/**
 * \brief Checks positive and negative infinity.
 */
static int
tan_infCases(void *args)
{
    double result;

    result = SDL_tan(INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Tan(%f), expected %f, got %f",
                        INFINITY, NAN, result);

    result = SDL_tan(-INFINITY);
    SDLTest_AssertCheck(isnan(result),
                        "Tan(%f), expected %f, got %f",
                        -INFINITY, NAN, result);

    return TEST_COMPLETED;
}

/**
 * \brief Checks for nan.
 */
static int
tan_nanCase(void *args)
{
    const double result = SDL_tan(NAN);
    SDLTest_AssertCheck(isnan(result),
                        "Tan(%f), expected %f, got %f",
                        NAN, NAN, result);
    return TEST_COMPLETED;
}

/**
 * \brief Checks positive and negative zero.
 */
static int
tan_zeroCases(void *args)
{
    const d_to_d regular_cases[] = {
        { -0.0, -0.0 },
        { 0.0, 0.0 }
    };
    return helper_dtod("Tan", SDL_tan, regular_cases, SDL_arraysize(regular_cases));
}

/**
 * \brief Checks tangent precision for the first 10 decimals.
 */
static int
tan_precisionTest(void *args)
{
    Uint32 i;
    Uint32 iterations = 10;
    double angle = 0.0;
    double step = 2.0 * M_PI / iterations;
    const double expected[] = {
        0.0, 7265425280.0, 30776835371.0, -30776835371.0, -7265425280.0,
        -0.0, 7265425280.0, 30776835371.0, -30776835371.0, -7265425280.0
    };
    for (i = 0; i < iterations; i++, angle += step) {
        double result = SDL_tan(angle) * 1.0E10;
        SDLTest_AssertCheck(SDL_trunc(result) == expected[i],
                            "Tan(%f), expected %f, got %f",
                            angle, expected[i], result);
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

/* SDL_exp test cases */

static const SDLTest_TestCaseReference expTestInf = {
    (SDLTest_TestCaseFp) exp_infCases, "exp_infCases",
    "Check positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference expTestZero = {
    (SDLTest_TestCaseFp) exp_zeroCases, "exp_zeroCases",
    "Check for positive and negative zero", TEST_ENABLED
};
static const SDLTest_TestCaseReference expTestOverflow = {
    (SDLTest_TestCaseFp) exp_overflowCase, "exp_overflowCase",
    "Check for overflow", TEST_ENABLED
};
static const SDLTest_TestCaseReference expTestBase = {
    (SDLTest_TestCaseFp) exp_baseCase, "exp_baseCase",
    "Check the base case of 1.0", TEST_ENABLED
};
static const SDLTest_TestCaseReference expTestRegular = {
    (SDLTest_TestCaseFp) exp_regularCases, "exp_regularCases",
    "Check a set of regular values", TEST_ENABLED
};

/* SDL_log test cases */

static const SDLTest_TestCaseReference logTestLimit = {
    (SDLTest_TestCaseFp) log_limitCases, "log_limitCases",
    "Check for limits", TEST_ENABLED
};
static const SDLTest_TestCaseReference logTestNan = {
    (SDLTest_TestCaseFp) log_nanCases, "log_nanCases",
    "Check for the nan cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference logTestBase = {
    (SDLTest_TestCaseFp) log_baseCases, "log_baseCases",
    "Check for base cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference logTestRegular = {
    (SDLTest_TestCaseFp) log_regularCases, "log_regularCases",
    "Check a set of regular values", TEST_ENABLED
};

/* SDL_log10 test cases */

static const SDLTest_TestCaseReference log10TestLimit = {
    (SDLTest_TestCaseFp) log10_limitCases, "log10_limitCases",
    "Check for limits", TEST_ENABLED
};
static const SDLTest_TestCaseReference log10TestNan = {
    (SDLTest_TestCaseFp) log10_nanCases, "log10_nanCases",
    "Check for the nan cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference log10TestBase = {
    (SDLTest_TestCaseFp) log10_baseCases, "log10_baseCases",
    "Check for base cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference log10TestRegular = {
    (SDLTest_TestCaseFp) log10_regularCases, "log10_regularCases",
    "Check a set of regular values", TEST_ENABLED
};

/* SDL_pow test cases */

static const SDLTest_TestCaseReference powTestExpInf1 = {
    (SDLTest_TestCaseFp) pow_baseNOneExpInfCases, "pow_baseNOneExpInfCases",
    "Check for pow(-1, +/-inf)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestExpInf2 = {
    (SDLTest_TestCaseFp) pow_baseZeroExpNInfCases, "pow_baseZeroExpNInfCases",
    "Check for pow(+/-0, -inf)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestExpInf3 = {
    (SDLTest_TestCaseFp) pow_expInfCases, "pow_expInfCases",
    "Check for pow(x, +/-inf)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestBaseInf1 = {
    (SDLTest_TestCaseFp) pow_basePInfCases, "pow_basePInfCases",
    "Check for pow(inf, x)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestBaseInf2 = {
    (SDLTest_TestCaseFp) pow_baseNInfCases, "pow_baseNInfCases",
    "Check for pow(-inf, x)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestNan1 = {
    (SDLTest_TestCaseFp) pow_badOperationCase, "pow_badOperationCase",
    "Check for negative finite base and non-integer finite exponent", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestNan2 = {
    (SDLTest_TestCaseFp) pow_base1ExpNanCase, "pow_base1ExpNanCase",
    "Check for pow(1.0, nan)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestNan3 = {
    (SDLTest_TestCaseFp) pow_baseNanExp0Cases, "pow_baseNanExp0Cases",
    "Check for pow(nan, +/-0)", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestNan4 = {
    (SDLTest_TestCaseFp) pow_nanArgsCases, "pow_nanArgsCases",
    "Check for pow(x, y) with either x or y being nan", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestZero1 = {
    (SDLTest_TestCaseFp) pow_baseNZeroExpOddCases, "pow_baseNZeroExpOddCases",
    "Check for pow(-0.0, y), with y an odd integer.", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestZero2 = {
    (SDLTest_TestCaseFp) pow_basePZeroExpOddCases, "pow_basePZeroExpOddCases",
    "Check for pow(0.0, y), with y an odd integer.", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestZero3 = {
    (SDLTest_TestCaseFp) pow_baseNZeroCases, "pow_baseNZeroCases",
    "Check for pow(-0.0, y), with y finite and even or non-integer number", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestZero4 = {
    (SDLTest_TestCaseFp) pow_basePZeroCases, "pow_basePZeroCases",
    "Check for pow(0.0, y), with y finite and even or non-integer number", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestRegular = {
    (SDLTest_TestCaseFp) pow_regularCases, "pow_regularCases",
    "Check a set of regular values", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestPowOf2 = {
    (SDLTest_TestCaseFp) pow_powerOfTwo, "pow_powerOfTwo",
    "Check the powers of two from 1 to 8", TEST_ENABLED
};
static const SDLTest_TestCaseReference powTestRange = {
    (SDLTest_TestCaseFp) pow_rangeTest, "pow_rangeTest",
    "Check a range of positive integer to the power of 0", TEST_ENABLED
};

/* SDL_sqrt test cases */

static const SDLTest_TestCaseReference sqrtTestInf = {
    (SDLTest_TestCaseFp) sqrt_infCase, "sqrt_infCase",
    "Check positive infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference sqrtTestNan = {
    (SDLTest_TestCaseFp) sqrt_nanCase, "sqrt_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference sqrtTestDomain = {
    (SDLTest_TestCaseFp) sqrt_outOfDomainCases, "sqrt_outOfDomainCases",
    "Check for out of domain values", TEST_ENABLED
};
static const SDLTest_TestCaseReference sqrtTestBase = {
    (SDLTest_TestCaseFp) sqrt_baseCases, "sqrt_baseCases",
    "Check the base cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference sqrtTestRegular = {
    (SDLTest_TestCaseFp) sqrt_regularCases, "sqrt_regularCases",
    "Check a set of regular values", TEST_ENABLED
};

/* SDL_scalbn test cases */

static const SDLTest_TestCaseReference scalbnTestInf = {
    (SDLTest_TestCaseFp) scalbn_infCases, "scalbn_infCases",
    "Check positive and negative infinity arg", TEST_ENABLED
};
static const SDLTest_TestCaseReference scalbnTestBaseZero = {
    (SDLTest_TestCaseFp) scalbn_baseZeroCases, "scalbn_baseZeroCases",
    "Check for positive and negative zero arg", TEST_ENABLED
};
static const SDLTest_TestCaseReference scalbnTestExpZero = {
    (SDLTest_TestCaseFp) scalbn_expZeroCase, "scalbn_expZeroCase",
    "Check for zero exp", TEST_ENABLED
};
static const SDLTest_TestCaseReference scalbnTestNan = {
    (SDLTest_TestCaseFp) scalbn_nanCase, "scalbn_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference scalbnTestRegular = {
    (SDLTest_TestCaseFp) scalbn_regularCases, "scalbn_regularCases",
    "Check a set of regular cases", TEST_ENABLED
};

/* SDL_cos test cases */

static const SDLTest_TestCaseReference cosTestInf = {
    (SDLTest_TestCaseFp) cos_infCases, "cos_infCases",
    "Check for positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference cosTestNan = {
    (SDLTest_TestCaseFp) cos_nanCase, "cos_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference cosTestRegular = {
    (SDLTest_TestCaseFp) cos_regularCases, "cos_regularCases",
    "Check a set of regular cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference cosTestPrecision = {
    (SDLTest_TestCaseFp) cos_precisionTest, "cos_precisionTest",
    "Check cosine precision to the tenth decimal", TEST_ENABLED
};
static const SDLTest_TestCaseReference cosTestRange = {
    (SDLTest_TestCaseFp) cos_rangeTest, "cos_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_sin test cases */

static const SDLTest_TestCaseReference sinTestInf = {
    (SDLTest_TestCaseFp) sin_infCases, "sin_infCases",
    "Check for positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference sinTestNan = {
    (SDLTest_TestCaseFp) sin_nanCase, "sin_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference sinTestRegular = {
    (SDLTest_TestCaseFp) sin_regularCases, "sin_regularCases",
    "Check a set of regular cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference sinTestPrecision = {
    (SDLTest_TestCaseFp) sin_precisionTest, "sin_precisionTest",
    "Check sine precision to the tenth decimal", TEST_ENABLED
};
static const SDLTest_TestCaseReference sinTestRange = {
    (SDLTest_TestCaseFp) sin_rangeTest, "sin_rangeTest",
    "Check a range of positive integer", TEST_ENABLED
};

/* SDL_tan test cases */

static const SDLTest_TestCaseReference tanTestInf = {
    (SDLTest_TestCaseFp) tan_infCases, "tan_infCases",
    "Check for positive and negative infinity", TEST_ENABLED
};
static const SDLTest_TestCaseReference tanTestNan = {
    (SDLTest_TestCaseFp) tan_nanCase, "tan_nanCase",
    "Check the NaN special case", TEST_ENABLED
};
static const SDLTest_TestCaseReference tanTestZero = {
    (SDLTest_TestCaseFp) tan_zeroCases, "tan_zeroCases",
    "Check a set of regular cases", TEST_ENABLED
};
static const SDLTest_TestCaseReference tanTestPrecision = {
    (SDLTest_TestCaseFp) tan_precisionTest, "tan_precisionTest",
    "Check tane precision to the tenth decimal", TEST_ENABLED
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

    &expTestInf, &expTestZero, &expTestOverflow,
    &expTestBase, &expTestRegular,

    &logTestLimit, &logTestNan,
    &logTestBase, &logTestRegular,

    &log10TestLimit, &log10TestNan,
    &log10TestBase, &log10TestRegular,

    &powTestExpInf1, &powTestExpInf2, &powTestExpInf3,
    &powTestBaseInf1, &powTestBaseInf2,
    &powTestNan1, &powTestNan2, &powTestNan3, &powTestNan4,
    &powTestZero1, &powTestZero2, &powTestZero3, &powTestZero4,
    &powTestRegular, &powTestPowOf2, &powTestRange,

    &sqrtTestInf, &sqrtTestNan, &sqrtTestDomain,
    &sqrtTestBase, &sqrtTestRegular,

    &scalbnTestInf, &scalbnTestBaseZero, &scalbnTestExpZero,
    &scalbnTestNan, &scalbnTestRegular,

    &cosTestInf, &cosTestNan, &cosTestRegular,
    &cosTestPrecision, &cosTestRange,

    &sinTestInf, &sinTestNan, &sinTestRegular,
    &sinTestPrecision, &sinTestRange,

    &tanTestInf, &tanTestNan, &tanTestZero, &tanTestPrecision,

    NULL
};

SDLTest_TestSuiteReference mathTestSuite = { "Math", NULL, mathTests, NULL };
