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
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), (int) result);

  result = SDL_strlcpy(text, "foo", 2);
  expected = "f";
  SDLTest_AssertPass("Call to SDL_strlcpy(\"foo\") with buffer size 2");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", (int) result);

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
  int predicted;
  char text[1024];
  const char *expected;

  result = SDL_snprintf(text, sizeof(text), "%s", "foo");
  expected = "foo";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%s\", \"foo\")");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);

  result = SDL_snprintf(text, 2, "%s", "foo");
  expected = "f";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%s\", \"foo\") with buffer size 2");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

  result = SDL_snprintf(NULL, 0, "%s", "foo");
  SDLTest_AssertPass("Call to SDL_snprintf(NULL, 0, \"%%s\", \"foo\")");
  SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

  result = SDL_snprintf(text, 2, "%s\n", "foo");
  expected = "f";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%s\\n\", \"foo\") with buffer size 2");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == 4, "Check result value, expected: 4, got: %d", result);

  result = SDL_snprintf(text, sizeof(text), "%f", 0.0);
  predicted = SDL_snprintf(NULL, 0, "%f", 0.0);
  expected = "0.000000";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 0.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%f", 1.0);
  predicted = SDL_snprintf(NULL, 0, "%f", 1.0);
  expected = "1.000000";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 1.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%.f", 1.0);
  predicted = SDL_snprintf(NULL, 0, "%.f", 1.0);
  expected = "1";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%.f\", 1.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%#.f", 1.0);
  predicted = SDL_snprintf(NULL, 0, "%#.f", 1.0);
  expected = "1.";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%#.f\", 1.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%f", 1.0 + 1.0 / 3.0);
  predicted = SDL_snprintf(NULL, 0, "%f", 1.0 + 1.0 / 3.0);
  expected = "1.333333";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%+f", 1.0 + 1.0 / 3.0);
  predicted = SDL_snprintf(NULL, 0, "%+f", 1.0 + 1.0 / 3.0);
  expected = "+1.333333";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%+f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%.2f", 1.0 + 1.0 / 3.0);
  predicted = SDL_snprintf(NULL, 0, "%.2f", 1.0 + 1.0 / 3.0);
  expected = "1.33";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%.2f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%6.2f", 1.0 + 1.0 / 3.0);
  predicted = SDL_snprintf(NULL, 0, "%6.2f", 1.0 + 1.0 / 3.0);
  expected = "  1.33";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%6.2f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, sizeof(text), "%06.2f", 1.0 + 1.0 / 3.0);
  predicted = SDL_snprintf(NULL, 0, "%06.2f", 1.0 + 1.0 / 3.0);
  expected = "001.33";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0)");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
  SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int) SDL_strlen(text), result);
  SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

  result = SDL_snprintf(text, 5, "%06.2f", 1.0 + 1.0 / 3.0);
  expected = "001.";
  SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0) with buffer size 5");
  SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
  SDLTest_AssertCheck(result == 6, "Check result value, expected: 6, got: %d", result);

  return TEST_COMPLETED;
}

/**
 * @brief Call to SDL_getenv and SDL_setenv
 */
int
stdlib_getsetenv(void *arg)
{
  const int nameLen = 16;
  char name[17];
  int counter;
  int result;
  char * value1;
  char * value2;
  char * expected;
  int overwrite;
  char * text;

  /* Create a random name. This tests SDL_getenv, since we need to */
  /* make sure the variable is not set yet (it shouldn't). */
  do {
    for(counter = 0; counter < nameLen; counter++) {
      name[counter] = (char)SDLTest_RandomIntegerInRange(65, 90);
    }
    name[nameLen] = '\0';
    
    text = SDL_getenv(name);
    SDLTest_AssertPass("Call to SDL_getenv('%s')", name);
    if (text != NULL) {
      SDLTest_Log("Expected: NULL, Got: '%s' (%i)", text, (int) SDL_strlen(text));
    }
  } while (text != NULL);
   
  /* Create random values to set */                    
  value1 = SDLTest_RandomAsciiStringOfSize(10);
  value2 = SDLTest_RandomAsciiStringOfSize(10);

  /* Set value 1 without overwrite */
  overwrite = 0;
  expected = value1;
  result = SDL_setenv(name, value1, overwrite);
  SDLTest_AssertPass("Call to SDL_setenv('%s','%s', %i)", name, value1, overwrite);
  SDLTest_AssertCheck(result == 0, "Check result, expected: 0, got: %i", result);

  /* Check value */
  text = SDL_getenv(name);
  SDLTest_AssertPass("Call to SDL_getenv('%s')", name);
  SDLTest_AssertCheck(text != NULL, "Verify returned text is not NULL");
  if (text != NULL) {
    SDLTest_AssertCheck(
      SDL_strcmp(text, expected) == 0, 
      "Verify returned text, expected: %s, got: %s",
      expected,
      text);
  }
  
  /* Set value 2 with overwrite */
  overwrite = 1;
  expected = value2;    
  result = SDL_setenv(name, value2, overwrite);
  SDLTest_AssertPass("Call to SDL_setenv('%s','%s', %i)", name, value2, overwrite);
  SDLTest_AssertCheck(result == 0, "Check result, expected: 0, got: %i", result);

  /* Check value */
  text = SDL_getenv(name);
  SDLTest_AssertPass("Call to SDL_getenv('%s')", name);
  SDLTest_AssertCheck(text != NULL, "Verify returned text is not NULL");
  if (text != NULL) {
    SDLTest_AssertCheck(
      SDL_strcmp(text, expected) == 0, 
      "Verify returned text, expected: %s, got: %s",
      expected,
      text);
  }

  /* Set value 1 without overwrite */
  overwrite = 0;
  expected = value2;    
  result = SDL_setenv(name, value1, overwrite);
  SDLTest_AssertPass("Call to SDL_setenv('%s','%s', %i)", name, value1, overwrite);
  SDLTest_AssertCheck(result == 0, "Check result, expected: 0, got: %i", result);

  /* Check value */
  text = SDL_getenv(name);
  SDLTest_AssertPass("Call to SDL_getenv('%s')", name);
  SDLTest_AssertCheck(text != NULL, "Verify returned text is not NULL");
  if (text != NULL) {
    SDLTest_AssertCheck(
      SDL_strcmp(text, expected) == 0, 
      "Verify returned text, expected: %s, got: %s",
      expected,
      text);
  }
  
  /* Set value 1 without overwrite */
  overwrite = 1;
  expected = value1;
  result = SDL_setenv(name, value1, overwrite);
  SDLTest_AssertPass("Call to SDL_setenv('%s','%s', %i)", name, value1, overwrite);
  SDLTest_AssertCheck(result == 0, "Check result, expected: 0, got: %i", result);

  /* Check value */
  text = SDL_getenv(name);
  SDLTest_AssertPass("Call to SDL_getenv('%s')", name);
  SDLTest_AssertCheck(text != NULL, "Verify returned text is not NULL");
  if (text != NULL) {
    SDLTest_AssertCheck(
      SDL_strcmp(text, expected) == 0, 
      "Verify returned text, expected: %s, got: %s",
      expected,
      text);
  }

  /* Negative cases */
  for (overwrite=0; overwrite <= 1; overwrite++) { 
    result = SDL_setenv(NULL, value1, overwrite);
    SDLTest_AssertPass("Call to SDL_setenv(NULL,'%s', %i)", value1, overwrite);
    SDLTest_AssertCheck(result == -1, "Check result, expected: -1, got: %i", result);
    result = SDL_setenv("", value1, overwrite);
    SDLTest_AssertPass("Call to SDL_setenv('','%s', %i)", value1, overwrite);
    SDLTest_AssertCheck(result == -1, "Check result, expected: -1, got: %i", result);
    result = SDL_setenv("=", value1, overwrite);
    SDLTest_AssertPass("Call to SDL_setenv('=','%s', %i)", value1, overwrite);
    SDLTest_AssertCheck(result == -1, "Check result, expected: -1, got: %i", result);
    result = SDL_setenv(name, NULL, overwrite);
    SDLTest_AssertPass("Call to SDL_setenv('%s', NULL, %i)", name, overwrite);
    SDLTest_AssertCheck(result == -1, "Check result, expected: -1, got: %i", result);
  }

  /* Clean up */
  SDL_free(value1);
  SDL_free(value2);
    
  return TEST_COMPLETED;
}

/**
 * @brief Call to SDL_sscanf
 */
#undef SDL_sscanf
int
stdlib_sscanf(void *arg)
{
  int output;
  int result;
  int expected_output;
  int expected_result;

  expected_output = output = 123;
  expected_result = -1;
  result = SDL_sscanf("", "%i", &output);
  SDLTest_AssertPass("Call to SDL_sscanf(\"\", \"%%i\", &output)");
  SDLTest_AssertCheck(expected_output == output, "Check output, expected: %i, got: %i", expected_output, output);
  SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

  expected_output = output = 123;
  expected_result = 0;
  result = SDL_sscanf("a", "%i", &output);
  SDLTest_AssertPass("Call to SDL_sscanf(\"a\", \"%%i\", &output)");
  SDLTest_AssertCheck(expected_output == output, "Check output, expected: %i, got: %i", expected_output, output);
  SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

  output = 123;
  expected_output = 2;
  expected_result = 1;
  result = SDL_sscanf("2", "%i", &output);
  SDLTest_AssertPass("Call to SDL_sscanf(\"2\", \"%%i\", &output)");
  SDLTest_AssertCheck(expected_output == output, "Check output, expected: %i, got: %i", expected_output, output);
  SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

  return TEST_COMPLETED;
}

#if defined(_WIN64)
# define SIZE_FORMAT "I64u"
#elif defined(__WIN32__)
# define SIZE_FORMAT "I32u"
#else
# define SIZE_FORMAT "zu"
#endif

typedef struct
{
    size_t a;
    size_t b;
    size_t result;
    int status;
} overflow_test;

static const overflow_test multiplications[] =
{
    { 1, 1, 1, 0 },
    { 0, 0, 0, 0 },
    { SDL_SIZE_MAX, 0, 0, 0 },
    { SDL_SIZE_MAX, 1, SDL_SIZE_MAX, 0 },
    { SDL_SIZE_MAX / 2, 2, SDL_SIZE_MAX - (SDL_SIZE_MAX % 2), 0 },
    { SDL_SIZE_MAX / 23, 23, SDL_SIZE_MAX - (SDL_SIZE_MAX % 23), 0 },

    { (SDL_SIZE_MAX / 2) + 1, 2, 0, -1 },
    { (SDL_SIZE_MAX / 23) + 42, 23, 0, -1 },
    { SDL_SIZE_MAX, SDL_SIZE_MAX, 0, -1 },
};

static const overflow_test additions[] =
{
    { 1, 1, 2, 0 },
    { 0, 0, 0, 0 },
    { SDL_SIZE_MAX, 0, SDL_SIZE_MAX, 0 },
    { SDL_SIZE_MAX - 1, 1, SDL_SIZE_MAX, 0 },
    { SDL_SIZE_MAX - 42, 23, SDL_SIZE_MAX - (42 - 23), 0 },

    { SDL_SIZE_MAX, 1, 0, -1 },
    { SDL_SIZE_MAX, 23, 0, -1 },
    { SDL_SIZE_MAX, SDL_SIZE_MAX, 0, -1 },
};

static int
stdlib_overflow(void *arg)
{
  size_t i;
  size_t useBuiltin;

  for (useBuiltin = 0; useBuiltin < 2; useBuiltin++) {
      if (useBuiltin) {
          SDLTest_Log("Using gcc/clang builtins if possible");
      } else {
          SDLTest_Log("Not using gcc/clang builtins");
      }

      for (i = 0; i < SDL_arraysize(multiplications); i++) {
          const overflow_test *t = &multiplications[i];
          int status;
          size_t result = ~t->result;

          if (useBuiltin) {
              status = SDL_size_mul_overflow(t->a, t->b, &result);
          } else {
              /* This disables the macro that tries to use a gcc/clang
               * builtin, so we test the fallback implementation instead. */
              status = (SDL_size_mul_overflow)(t->a, t->b, &result);
          }

          if (t->status == 0) {
              SDLTest_AssertCheck(status == 0,
                                  "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should succeed",
                                  t->a, t->b);
              SDLTest_AssertCheck(result == t->result,
                                  "(%" SIZE_FORMAT " * %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                  t->a, t->b, t->result, result);
          } else {
              SDLTest_AssertCheck(status == -1,
                                  "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should fail",
                                  t->a, t->b);
          }

          if (t->a == t->b) {
              continue;
          }

          result = ~t->result;

          if (useBuiltin) {
              status = SDL_size_mul_overflow(t->b, t->a, &result);
          } else {
              status = (SDL_size_mul_overflow)(t->b, t->a, &result);
          }

          if (t->status == 0) {
              SDLTest_AssertCheck(status == 0,
                                  "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should succeed",
                                  t->b, t->a);
              SDLTest_AssertCheck(result == t->result,
                                  "(%" SIZE_FORMAT " * %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                  t->b, t->a, t->result, result);
          } else {
              SDLTest_AssertCheck(status == -1,
                                  "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should fail",
                                  t->b, t->a);
          }
      }

      for (i = 0; i < SDL_arraysize(additions); i++) {
          const overflow_test *t = &additions[i];
          int status;
          size_t result = ~t->result;

          if (useBuiltin) {
              status = SDL_size_add_overflow(t->a, t->b, &result);
          } else {
              status = (SDL_size_add_overflow)(t->a, t->b, &result);
          }

          if (t->status == 0) {
              SDLTest_AssertCheck(status == 0,
                                  "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should succeed",
                                  t->a, t->b);
              SDLTest_AssertCheck(result == t->result,
                                  "(%" SIZE_FORMAT " + %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                  t->a, t->b, t->result, result);
          } else {
              SDLTest_AssertCheck(status == -1,
                                  "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should fail",
                                  t->a, t->b);
          }

          if (t->a == t->b) {
              continue;
          }

          result = ~t->result;

          if (useBuiltin) {
              status = SDL_size_add_overflow(t->b, t->a, &result);
          } else {
              status = (SDL_size_add_overflow)(t->b, t->a, &result);
          }

          if (t->status == 0) {
              SDLTest_AssertCheck(status == 0,
                                  "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should succeed",
                                  t->b, t->a);
              SDLTest_AssertCheck(result == t->result,
                                  "(%" SIZE_FORMAT " + %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                  t->b, t->a, t->result, result);
          } else {
              SDLTest_AssertCheck(status == -1,
                                  "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should fail",
                                  t->b, t->a);
          }
      }
  }

  return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Standard C routine test cases */
static const SDLTest_TestCaseReference stdlibTest1 =
        { (SDLTest_TestCaseFp)stdlib_strlcpy, "stdlib_strlcpy", "Call to SDL_strlcpy", TEST_ENABLED };

static const SDLTest_TestCaseReference stdlibTest2 =
        { (SDLTest_TestCaseFp)stdlib_snprintf, "stdlib_snprintf", "Call to SDL_snprintf", TEST_ENABLED };

static const SDLTest_TestCaseReference stdlibTest3 =
        { (SDLTest_TestCaseFp)stdlib_getsetenv, "stdlib_getsetenv", "Call to SDL_getenv and SDL_setenv", TEST_ENABLED };

static const SDLTest_TestCaseReference stdlibTest4 =
        { (SDLTest_TestCaseFp)stdlib_sscanf, "stdlib_sscanf", "Call to SDL_sscanf", TEST_ENABLED };

static const SDLTest_TestCaseReference stdlibTestOverflow =
        { stdlib_overflow, "stdlib_overflow", "Overflow detection", TEST_ENABLED };

/* Sequence of Standard C routine test cases */
static const SDLTest_TestCaseReference *stdlibTests[] =  {
    &stdlibTest1,
    &stdlibTest2,
    &stdlibTest3,
    &stdlibTest4,
    &stdlibTestOverflow,
    NULL
};

/* Standard C routine test suite (global) */
SDLTest_TestSuiteReference stdlibTestSuite = {
    "Stdlib",
    NULL,
    stdlibTests,
    NULL
};
