/**
 * Standard C library routine test suite
 */

#include <stdio.h>

#include "SDL.h"
#include "SDL_test.h"


/* Test case functions */

/**
 * @brief Call to SDL_strlcpy
 */
#undef SDL_strlcpy
int
stdlib_strlcpy(void *arg)
{
  size_t result;
  char text[1024];
  const char *expected;

  result = SDL_strlcpy(text, "foo", sizeof(text));
  expected = "foo";
  SDLTest_AssertPass("Call to SDL_strlcpy(\"foo\")");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_strlcpy(text, "foo", 2);
  expected = "f";
  SDLTest_AssertPass("Call to SDL_strlcpy(\"foo\") with buffer size 2");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

  return TEST_COMPLETED;
}

/**
 * @brief Call to SDL_snprintf
 */
#undef SDL_snprintf
int
stdlib_snprintf(void *arg)
{
  int result;
  char text[1024];
  const char *expected;

  result = SDL_snprintf(text, sizeof(text), "%s", "foo");
  expected = "foo";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%s\", \"foo\")");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, 2, "%s", "foo");
  expected = "f";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%s\", \"foo\") with buffer size 2");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

  result = SDL_snprintf(NULL, 0, "%s", "foo");
  SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

  result = SDL_snprintf(text, sizeof(text), "%f", 1.0);
  expected = "1.000000";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 1.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%.f", 1.0);
  expected = "1";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%.f\", 1.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%#.f", 1.0);
  expected = "1.";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%#.f\", 1.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%f", 1.0 + 1.0 / 3.0);
  expected = "1.333333";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%+f", 1.0 + 1.0 / 3.0);
  expected = "+1.333333";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%+f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%.2f", 1.0 + 1.0 / 3.0);
  expected = "1.33";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%.2f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%6.2f", 1.0 + 1.0 / 3.0);
  expected = "  1.33";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%6.2f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, sizeof(text), "%06.2f", 1.0 + 1.0 / 3.0);
  expected = "001.33";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", SDL_strlen(text), result);

  result = SDL_snprintf(text, 5, "%06.2f", 1.0 + 1.0 / 3.0);
  expected = "001.";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0) with buffer size 5");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
  SDLTest_AssertCheck(result == 6, "Check result value, expected: 6, got: %d", result);

  return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Standard C routine test cases */
static const SDLTest_TestCaseReference stdlibTest1 =
        { (SDLTest_TestCaseFp)stdlib_strlcpy, "stdlib_strlcpy", "Call to SDL_strlcpy", TEST_ENABLED };

static const SDLTest_TestCaseReference stdlibTest2 =
        { (SDLTest_TestCaseFp)stdlib_snprintf, "stdlib_snprintf", "Call to SDL_snprintf", TEST_ENABLED };

/* Sequence of Standard C routine test cases */
static const SDLTest_TestCaseReference *stdlibTests[] =  {
    &stdlibTest1, &stdlibTest2, NULL
};

/* Timer test suite (global) */
SDLTest_TestSuiteReference stdlibTestSuite = {
    "Standard C routines",
    NULL,
    stdlibTests,
    NULL
};
