/**
 * Math test suite
 */

#include <math.h>

#include "SDL.h"
#include "SDL_test.h"

/* ================= Test Case Implementation ================== */

/* SDL_floor tests functions */

/**
 * \brief Checks edge cases (0 and infinity) for themselves.
 */
static int floor_edgeCases(void* args) {
  static const double edge_cases[] = {INFINITY, -INFINITY, 0.0, -0.0};
  for (size_t i = 0; i < SDL_arraysize(edge_cases); i++) {
    const double result = SDL_floor(edge_cases[i]);
    SDLTest_AssertCheck(result == edge_cases[i],
                        "Floor(%.1f), expected %.1f, got %.1f", edge_cases[i],
                        edge_cases[i], result);
  }
  return TEST_COMPLETED;
}

/**
 * \brief Checks the NaN case.
 */
static int floor_nanCase(void* args) {
  SDLTest_AssertCheck(isnan(SDL_floor(NAN)), "Floor(nan), expected nan");
  return TEST_COMPLETED;
}

/**
 * \brief Checks round values (x.0) for themselves
 */
static int floor_roundNumbersCases(void* args) {
  static const double round_cases[] = {1.0,   -1.0,   15.0,   -15.0,
                                       125.0, -125.0, 1024.0, -1024.0};
  for (size_t i = 0; i < SDL_arraysize(round_cases); i++) {
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
static int floor_fractionCases(void* args) {
  static const struct {
    double input, expected;
  } frac_cases[] = {{1.0 / 2.0, 0.0},        {-1.0 / 2.0, -1.0},
                    {4.0 / 3.0, 1.0},        {-4.0 / 3.0, -2.0},
                    {76.0 / 7.0, 10.0},      {-76.0 / 7.0, -11.0},
                    {535.0 / 8.0, 66.0},     {-535.0 / 8.0, -67.0},
                    {19357.0 / 53.0, 365.0}, {-19357.0 / 53.0, -366.0}};
  for (size_t i = 0; i < SDL_arraysize(frac_cases); i++) {
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
static int floor_rangeTest(void* args) {
  static const Uint32 ITERATIONS = 10000000;
  static const Uint32 STEP = UINT32_MAX / ITERATIONS;
  double test_value = 0.0;
  SDLTest_AssertPass("Floor: Testing a range of %u values with %u steps",
                     ITERATIONS, STEP);
  for (Uint32 i = 0; i < ITERATIONS; i++, test_value += STEP) {
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

/* ================= Test References ================== */

/* SDL_floor test cases */

static const SDLTest_TestCaseReference floorTest1 = {
    (SDLTest_TestCaseFp)floor_edgeCases, "floor_edgeCases",
    "Check positive and negative infinity and 0", TEST_ENABLED};
static const SDLTest_TestCaseReference floorTest2 = {
    (SDLTest_TestCaseFp)floor_nanCase, "floor_nanCase",
    "Check the NaN special case", TEST_ENABLED};
static const SDLTest_TestCaseReference floorTest3 = {
    (SDLTest_TestCaseFp)floor_roundNumbersCases, "floor_roundNumberCases",
    "Check a set of round numbers", TEST_ENABLED};
static const SDLTest_TestCaseReference floorTest4 = {
    (SDLTest_TestCaseFp)floor_fractionCases, "floor_fractionCases",
    "Check a set of fractions", TEST_ENABLED};
static const SDLTest_TestCaseReference floorTest5 = {
    (SDLTest_TestCaseFp)floor_rangeTest, "floor_rangeTest",
    "Check a range of positive integer", TEST_ENABLED};

static const SDLTest_TestCaseReference* mathTests[] = {
    &floorTest1, &floorTest2, &floorTest3, &floorTest4, &floorTest5, NULL};

SDLTest_TestSuiteReference mathTestSuite = {"Math", NULL, mathTests, NULL};
