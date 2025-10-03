/**
 * Standard C library routine test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Test case functions */

/**
 * Call to SDL_strnlen
 */
static int SDLCALL stdlib_strnlen(void *arg)
{
    size_t result;
    char *text_result;
    const char *text = "food";
    const char *expected;

    result = SDL_strnlen(text, 6);
    SDLTest_AssertPass("Call to SDL_strndup(\"food\", 6)");
    SDLTest_AssertCheck(result == 4, "Check result value, expected: 4, got: %d", (int)result);

    result = SDL_strnlen(text, 3);
    SDLTest_AssertPass("Call to SDL_strndup(\"food\", 3)");
    SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", (int)result);

    text_result = SDL_strndup(text, 3);
    expected = "foo";
    SDLTest_AssertPass("Call to SDL_strndup(\"food\", 3)");
    SDLTest_AssertCheck(SDL_strcmp(text_result, expected) == 0, "Check text, expected: %s, got: %s", expected, text_result);
    SDL_free(text_result);

    return TEST_COMPLETED;
}

/**
 * Call to SDL_strlcpy
 */
static int SDLCALL stdlib_strlcpy(void *arg)
{
    size_t result;
    char text[1024];
    const char *expected;

    result = SDL_strlcpy(text, "foo", sizeof(text));
    expected = "foo";
    SDLTest_AssertPass("Call to SDL_strlcpy(\"foo\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), (int)result);

    result = SDL_strlcpy(text, "foo", 2);
    expected = "f";
    SDLTest_AssertPass("Call to SDL_strlcpy(\"foo\") with buffer size 2");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", (int)result);

    return TEST_COMPLETED;
}

/**
 * Call to SDL_strstr
 */
static int SDLCALL stdlib_strstr(void *arg)
{
    char *result;
    const char *text = "abcdef";
    const char *expected;

    result = SDL_strstr(text, "");
    expected = text;
    SDLTest_AssertPass("Call to SDL_strstr(text, \"\")");
    SDLTest_AssertCheck(result == expected, "Check result, expected: %s, got: %s", expected, result);

    result = SDL_strstr(text, "abc");
    expected = text;
    SDLTest_AssertPass("Call to SDL_strstr(text, \"abc\")");
    SDLTest_AssertCheck(result == expected, "Check result, expected: %s, got: %s", expected, result);

    result = SDL_strstr(text, "bcd");
    expected = text+1;
    SDLTest_AssertPass("Call to SDL_strstr(text, \"bcd\")");
    SDLTest_AssertCheck(result == expected, "Check result, expected: %s, got: %s", expected, result);

    result = SDL_strstr(text, "xyz");
    expected = NULL;
    SDLTest_AssertPass("Call to SDL_strstr(text, \"xyz\")");
    SDLTest_AssertCheck(result == expected, "Check result, expected: (null), got: %s", result);

    result = SDL_strnstr(text, "", SDL_strlen(text));
    expected = text;
    SDLTest_AssertPass("Call to SDL_strnstr(text, \"\", SDL_strlen(text))");
    SDLTest_AssertCheck(result == expected, "Check result, expected: %s, got: %s", expected, result);

    result = SDL_strnstr(text, "abc", SDL_strlen(text));
    expected = text;
    SDLTest_AssertPass("Call to SDL_strnstr(text, \"abc\", SDL_strlen(text))");
    SDLTest_AssertCheck(result == expected, "Check result, expected: %s, got: %s", expected, result);

    result = SDL_strnstr(text, "bcd", SDL_strlen(text));
    expected = text+1;
    SDLTest_AssertPass("Call to SDL_strnstr(text, \"bcd\", SDL_strlen(text))");
    SDLTest_AssertCheck(result == expected, "Check result, expected: %s, got: %s", expected, result);

    result = SDL_strnstr(text, "bcd", 3);
    expected = NULL;
    SDLTest_AssertPass("Call to SDL_strnstr(text, \"bcd\", 3)");
    SDLTest_AssertCheck(result == expected, "Check result, expected: (null), got: %s", result);

    result = SDL_strnstr(text, "xyz", 3);
    expected = NULL;
    SDLTest_AssertPass("Call to SDL_strnstr(text, \"xyz\", 3)");
    SDLTest_AssertCheck(result == expected, "Check result, expected: (null), got: %s", result);

    result = SDL_strnstr(text, "xyz", SDL_strlen(text)*100000);
    expected = NULL;
    SDLTest_AssertPass("Call to SDL_strnstr(text, \"xyz\", SDL_strlen(text)*100000)");
    SDLTest_AssertCheck(result == expected, "Check result, expected: (null), got: %s", result);

    return TEST_COMPLETED;
}

#if defined(HAVE_WFORMAT) || defined(HAVE_WFORMAT_EXTRA_ARGS)
#pragma GCC diagnostic push
#ifdef HAVE_WFORMAT
#pragma GCC diagnostic ignored "-Wformat"
#endif
#ifdef HAVE_WFORMAT_EXTRA_ARGS
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif
#endif

/**
 * Call to SDL_snprintf
 */
static int SDLCALL stdlib_snprintf(void *arg)
{
    int result;
    int predicted;
    char text[1024];
    const char *expected, *expected2, *expected3, *expected4, *expected5;
    size_t size;

    result = SDL_snprintf(text, sizeof(text), "%s", "foo");
    expected = "foo";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%s\", \"foo\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);

    result = SDL_snprintf(text, sizeof(text), "%10sA", "foo");
    expected = "       fooA";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%10sA\", \"foo\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);

    result = SDL_snprintf(text, sizeof(text), "%-10sA", "foo");
    expected = "foo       A";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%-10sA\", \"foo\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);

    result = SDL_snprintf(text, sizeof(text), "%S", L"foo");
    expected = "foo";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%S\", \"foo\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);

    result = SDL_snprintf(text, sizeof(text), "%ls", L"foo");
    expected = "foo";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%ls\", \"foo\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);

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
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%f", 1.0);
    predicted = SDL_snprintf(NULL, 0, "%f", 1.0);
    expected = "1.000000";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 1.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%.f", 1.0);
    predicted = SDL_snprintf(NULL, 0, "%.f", 1.0);
    expected = "1";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%.f\", 1.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%#.f", 1.0);
    predicted = SDL_snprintf(NULL, 0, "%#.f", 1.0);
    expected = "1.";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%#.f\", 1.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%f", 1.0 + 1.0 / 3.0);
    predicted = SDL_snprintf(NULL, 0, "%f", 1.0 + 1.0 / 3.0);
    expected = "1.333333";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%+f", 1.0 + 1.0 / 3.0);
    predicted = SDL_snprintf(NULL, 0, "%+f", 1.0 + 1.0 / 3.0);
    expected = "+1.333333";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%+f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%.2f", 1.0 + 1.0 / 3.0);
    predicted = SDL_snprintf(NULL, 0, "%.2f", 1.0 + 1.0 / 3.0);
    expected = "1.33";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%.2f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: %s, got: %s", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%6.2f", 1.0 + 1.0 / 3.0);
    predicted = SDL_snprintf(NULL, 0, "%6.2f", 1.0 + 1.0 / 3.0);
    expected = "  1.33";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%6.2f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, sizeof(text), "%06.2f", 1.0 + 1.0 / 3.0);
    predicted = SDL_snprintf(NULL, 0, "%06.2f", 1.0 + 1.0 / 3.0);
    expected = "001.33";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_snprintf(text, 5, "%06.2f", 1.0 + 1.0 / 3.0);
    expected = "001.";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0) with buffer size 5");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == 6, "Check result value, expected: 6, got: %d", result);

    result = SDL_snprintf(text, sizeof(text), "%06.0f", ((double)SDL_MAX_SINT64) * 1.5);
    predicted = SDL_snprintf(NULL, 0, "%06.0f", ((double)SDL_MAX_SINT64) * 1.5);
    expected = "13835058055282163712";
    SDLTest_AssertPass("Call to SDL_snprintf(\"%%06.2f\", SDL_MAX_SINT64 * 1.5)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(text), "Check result value, expected: %d, got: %d", (int)SDL_strlen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    {
        static struct
        {
            int precision;
            float value;
            const char *expected_f;
            const char *expected_g;
        } f_and_g_test_cases[] = {
            { 6, 100.0f, "100.000000", "100" },
            { 6, -100.0f, "-100.000000", "-100" },
            { 6, 100.75f, "100.750000", "100.75" },
            { 6, -100.75f, "-100.750000", "-100.75" },
            { 6, ((100 * 60 * 1000) / 1001) / 100.0f, "59.939999", "59.94" },
            { 6, -((100 * 60 * 1000) / 1001) / 100.0f, "-59.939999", "-59.94" },
            { 6, ((100 * 120 * 1000) / 1001) / 100.0f, "119.879997", "119.88" },
            { 6, -((100 * 120 * 1000) / 1001) / 100.0f, "-119.879997", "-119.88" },
            { 6, 0.9999999f, "1.000000", "1" },
            { 6, -0.9999999f, "-1.000000", "-1" },
            { 5, 9.999999f, "10.00000", "10" },
            { 5, -9.999999f, "-10.00000", "-10" },
        };
        int i;

        for (i = 0; i < SDL_arraysize(f_and_g_test_cases); ++i) {
            float value = f_and_g_test_cases[i].value;
            int prec = f_and_g_test_cases[i].precision;

            result = SDL_snprintf(text, sizeof(text), "%.*f", prec, value);
            predicted = SDL_snprintf(NULL, 0, "%.*f", prec, value);
            expected = f_and_g_test_cases[i].expected_f;
            SDLTest_AssertPass("Call to SDL_snprintf(\"%%.5f\", %g)", value);
            SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
            SDLTest_AssertCheck(result == SDL_strlen(expected), "Check result value, expected: %d, got: %d", (int)SDL_strlen(expected), result);
            SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

            result = SDL_snprintf(text, sizeof(text), "%g", value);
            predicted = SDL_snprintf(NULL, 0, "%g", value);
            expected = f_and_g_test_cases[i].expected_g;
            SDLTest_AssertPass("Call to SDL_snprintf(\"%%g\", %g)", value);
            SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
            SDLTest_AssertCheck(result == SDL_strlen(expected), "Check result value, expected: %d, got: %d", (int)SDL_strlen(expected), result);
            SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);
        }
    }

    size = 64;
    result = SDL_snprintf(text, sizeof(text), "%zu %s", size, "test");
    expected = "64 test";
    SDLTest_AssertPass("Call to SDL_snprintf(text, sizeof(text), \"%%zu %%s\", size, \"test\")");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0, "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == 7, "Check result value, expected: 7, got: %d", result);

    result = SDL_snprintf(text, sizeof(text), "%p", (void *)0x1234abcd);
    expected = "0x1234abcd";
    expected2 = "1234ABCD";
    expected3 = "000000001234ABCD";
    expected4 = "1234abcd";
    expected5 = "000000001234abcd";
    SDLTest_AssertPass("Call to SDL_snprintf(text, sizeof(text), \"%%p\", 0x1234abcd)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0 ||
                        SDL_strcmp(text, expected2) == 0 ||
                        SDL_strcmp(text, expected3) == 0 ||
                        SDL_strcmp(text, expected4) == 0 ||
                        SDL_strcmp(text, expected5) == 0,
        "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(expected) ||
                        result == SDL_strlen(expected2) ||
                        result == SDL_strlen(expected3) ||
                        result == SDL_strlen(expected4) ||
                        result == SDL_strlen(expected5),
        "Check result value, expected: %d, got: %d", (int)SDL_strlen(expected), result);

    result = SDL_snprintf(text, sizeof(text), "A %p B", (void *)0x1234abcd);
    expected = "A 0x1234abcd B";
    expected2 = "A 1234ABCD B";
    expected3 = "A 000000001234ABCD B";
    expected4 = "A 1234abcd B";
    expected5 = "A 000000001234abcd B";
    SDLTest_AssertPass("Call to SDL_snprintf(text, sizeof(text), \"A %%p B\", 0x1234abcd)");
    SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0 ||
                        SDL_strcmp(text, expected2) == 0 ||
                        SDL_strcmp(text, expected3) == 0 ||
                        SDL_strcmp(text, expected4) == 0 ||
                        SDL_strcmp(text, expected5) == 0,
        "Check text, expected: '%s', got: '%s'", expected, text);
    SDLTest_AssertCheck(result == SDL_strlen(expected) ||
                        result == SDL_strlen(expected2) ||
                        result == SDL_strlen(expected3) ||
                        result == SDL_strlen(expected4) ||
                        result == SDL_strlen(expected5),
        "Check result value, expected: %d, got: %d", (int)SDL_strlen(expected), result);

    if (sizeof(void *) >= 8) {
        result = SDL_snprintf(text, sizeof(text), "%p", (void *)SDL_SINT64_C(0x1ba07bddf60));
        expected = "0x1ba07bddf60";
        expected2 = "000001BA07BDDF60";
        expected3 = "000001ba07bddf60";
        SDLTest_AssertPass("Call to SDL_snprintf(text, sizeof(text), \"%%p\", 0x1ba07bddf60)");
        SDLTest_AssertCheck(SDL_strcmp(text, expected) == 0 ||
                            SDL_strcmp(text, expected2) == 0 ||
                            SDL_strcmp(text, expected3) == 0,
            "Check text, expected: '%s', got: '%s'", expected, text);
        SDLTest_AssertCheck(result == SDL_strlen(expected) ||
                            result == SDL_strlen(expected2) ||
                            result == SDL_strlen(expected3),
            "Check result value, expected: %d, got: %d", (int)SDL_strlen(expected), result);
    }
    return TEST_COMPLETED;
}

/**
 * Call to SDL_swprintf
 */
static int SDLCALL stdlib_swprintf(void *arg)
{
    int result;
    int predicted;
    wchar_t text[1024];
    const wchar_t *expected;
    size_t size;

    result = SDL_swprintf(text, SDL_arraysize(text), L"%s", "hello, world");
    expected = L"hello, world";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%s\", \"hello, world\")");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);

    result = SDL_swprintf(text, 2, L"%s", "hello, world");
    expected = L"h";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%s\", \"hello, world\") with buffer size 2");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == 12, "Check result value, expected: 12, got: %d", result);

    result = SDL_swprintf(NULL, 0, L"%s", "hello, world");
    SDLTest_AssertPass("Call to SDL_swprintf(NULL, 0, \"%%s\", \"hello, world\")");
    SDLTest_AssertCheck(result == 12, "Check result value, expected: 12, got: %d", result);

    result = SDL_swprintf(text, SDL_arraysize(text), L"%s", "foo");
    expected = L"foo";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%s\", \"foo\")");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);

    result = SDL_swprintf(text, 2, L"%s", "foo");
    expected = L"f";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%s\", \"foo\") with buffer size 2");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

    result = SDL_swprintf(NULL, 0, L"%s", "foo");
    SDLTest_AssertPass("Call to SDL_swprintf(NULL, 0, \"%%s\", \"foo\")");
    SDLTest_AssertCheck(result == 3, "Check result value, expected: 3, got: %d", result);

    result = SDL_swprintf(text, 2, L"%s\n", "foo");
    expected = L"f";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%s\\n\", \"foo\") with buffer size 2");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == 4, "Check result value, expected: 4, got: %d", result);

    result = SDL_swprintf(text, sizeof(text), L"%f", 0.0);
    predicted = SDL_swprintf(NULL, 0, L"%f", 0.0);
    expected = L"0.000000";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%f\", 0.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%f", 1.0);
    predicted = SDL_swprintf(NULL, 0, L"%f", 1.0);
    expected = L"1.000000";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%f\", 1.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%.f", 1.0);
    predicted = SDL_swprintf(NULL, 0, L"%.f", 1.0);
    expected = L"1";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%.f\", 1.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%#.f", 1.0);
    predicted = SDL_swprintf(NULL, 0, L"%#.f", 1.0);
    expected = L"1.";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%#.f\", 1.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%f", 1.0 + 1.0 / 3.0);
    predicted = SDL_swprintf(NULL, 0, L"%f", 1.0 + 1.0 / 3.0);
    expected = L"1.333333";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%+f", 1.0 + 1.0 / 3.0);
    predicted = SDL_swprintf(NULL, 0, L"%+f", 1.0 + 1.0 / 3.0);
    expected = L"+1.333333";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%+f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%.2f", 1.0 + 1.0 / 3.0);
    predicted = SDL_swprintf(NULL, 0, L"%.2f", 1.0 + 1.0 / 3.0);
    expected = L"1.33";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%.2f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: %S, got: %S", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%6.2f", 1.0 + 1.0 / 3.0);
    predicted = SDL_swprintf(NULL, 0, L"%6.2f", 1.0 + 1.0 / 3.0);
    expected = L"  1.33";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%6.2f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: '%S', got: '%S'", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, sizeof(text), L"%06.2f", 1.0 + 1.0 / 3.0);
    predicted = SDL_swprintf(NULL, 0, L"%06.2f", 1.0 + 1.0 / 3.0);
    expected = L"001.33";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0)");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: '%S', got: '%S'", expected, text);
    SDLTest_AssertCheck(result == SDL_wcslen(text), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(text), result);
    SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

    result = SDL_swprintf(text, 5, L"%06.2f", 1.0 + 1.0 / 3.0);
    expected = L"001.";
    SDLTest_AssertPass("Call to SDL_swprintf(\"%%06.2f\", 1.0 + 1.0 / 3.0) with buffer size 5");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: '%S', got: '%S'", expected, text);
    SDLTest_AssertCheck(result == 6, "Check result value, expected: 6, got: %d", result);

    {
        static struct
        {
            float value;
            const wchar_t *expected_f;
            const wchar_t *expected_g;
        } f_and_g_test_cases[] = {
            { 100.0f, L"100.000000", L"100" },
            { -100.0f, L"-100.000000", L"-100" },
            { 100.75f, L"100.750000", L"100.75" },
            { -100.75f, L"-100.750000", L"-100.75" },
            { ((100 * 60 * 1000) / 1001) / 100.0f, L"59.939999", L"59.94" },
            { -((100 * 60 * 1000) / 1001) / 100.0f, L"-59.939999", L"-59.94" },
            { ((100 * 120 * 1000) / 1001) / 100.0f, L"119.879997", L"119.88" },
            { -((100 * 120 * 1000) / 1001) / 100.0f, L"-119.879997", L"-119.88" },
            { 9.9999999f, L"10.000000", L"10" },
            { -9.9999999f, L"-10.000000", L"-10" },
        };
        int i;

        for (i = 0; i < SDL_arraysize(f_and_g_test_cases); ++i) {
            float value = f_and_g_test_cases[i].value;

            result = SDL_swprintf(text, sizeof(text), L"%f", value);
            predicted = SDL_swprintf(NULL, 0, L"%f", value);
            expected = f_and_g_test_cases[i].expected_f;
            SDLTest_AssertPass("Call to SDL_swprintf(\"%%f\", %g)", value);
            SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: '%S', got: '%S'", expected, text);
            SDLTest_AssertCheck(result == SDL_wcslen(expected), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(expected), result);
            SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);

            result = SDL_swprintf(text, sizeof(text), L"%g", value);
            predicted = SDL_swprintf(NULL, 0, L"%g", value);
            expected = f_and_g_test_cases[i].expected_g;
            SDLTest_AssertPass("Call to SDL_swprintf(\"%%g\", %g)", value);
            SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: '%S', got: '%S'", expected, text);
            SDLTest_AssertCheck(result == SDL_wcslen(expected), "Check result value, expected: %d, got: %d", (int)SDL_wcslen(expected), result);
            SDLTest_AssertCheck(predicted == result, "Check predicted value, expected: %d, got: %d", result, predicted);
        }
    }

    size = 64;
    result = SDL_swprintf(text, sizeof(text), L"%zu %s", size, "test");
    expected = L"64 test";
    SDLTest_AssertPass("Call to SDL_swprintf(text, sizeof(text), \"%%zu %%s\", size, \"test\")");
    SDLTest_AssertCheck(SDL_wcscmp(text, expected) == 0, "Check text, expected: '%S', got: '%S'", expected, text);
    SDLTest_AssertCheck(result == 7, "Check result value, expected: 7, got: %d", result);

    return TEST_COMPLETED;
}

#if defined(HAVE_WFORMAT) || defined(HAVE_WFORMAT_EXTRA_ARGS)
#pragma GCC diagnostic pop
#endif

/**
 * Call to SDL_GetEnvironmentVariable() and SDL_SetEnvironmentVariable()
 */
static int SDLCALL stdlib_getsetenv(void *arg)
{
    SDL_Environment *env = SDL_GetEnvironment();
    const int nameLen = 16;
    char name[17];
    int counter;
    int result;
    char *value1;
    char *value2;
    char *expected;
    int overwrite;
    const char *text;

    /* Create a random name. This tests SDL_GetEnvironmentVariable, since we need to */
    /* make sure the variable is not set yet (it shouldn't). */
    do {
        for (counter = 0; counter < nameLen; counter++) {
            name[counter] = (char)SDLTest_RandomIntegerInRange(65, 90);
        }
        name[nameLen] = '\0';

        text = SDL_GetEnvironmentVariable(env, name);
        SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, '%s')", name);
        if (text) {
            SDLTest_Log("Expected: NULL, Got: '%s' (%i)", text, (int)SDL_strlen(text));
        }
    } while (text);

    /* Create random values to set */
    value1 = SDLTest_RandomAsciiStringOfSize(10);
    value2 = SDLTest_RandomAsciiStringOfSize(10);

    /* Set value 1 without overwrite */
    overwrite = 0;
    expected = value1;
    result = SDL_SetEnvironmentVariable(env, name, value1, overwrite);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '%s','%s', %i)", name, value1, overwrite);
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);

    /* Check value */
    text = SDL_GetEnvironmentVariable(env, name);
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, '%s')", name);
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
    result = SDL_SetEnvironmentVariable(env, name, value2, overwrite);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '%s','%s', %i)", name, value2, overwrite);
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);

    /* Check value */
    text = SDL_GetEnvironmentVariable(env, name);
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, '%s')", name);
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
    result = SDL_SetEnvironmentVariable(env, name, value1, overwrite);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '%s','%s', %i)", name, value1, overwrite);
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);

    /* Check value */
    text = SDL_GetEnvironmentVariable(env, name);
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, '%s')", name);
    SDLTest_AssertCheck(text != NULL, "Verify returned text is not NULL");
    if (text != NULL) {
        SDLTest_AssertCheck(
            SDL_strcmp(text, expected) == 0,
            "Verify returned text, expected: %s, got: %s",
            expected,
            text);
    }

    /* Set value 1 with overwrite */
    overwrite = 1;
    expected = value1;
    result = SDL_SetEnvironmentVariable(env, name, value1, overwrite);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '%s','%s', %i)", name, value1, overwrite);
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);

    /* Check value */
    text = SDL_GetEnvironmentVariable(env, name);
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, '%s')", name);
    SDLTest_AssertCheck(text != NULL, "Verify returned text is not NULL");
    if (text != NULL) {
        SDLTest_AssertCheck(
            SDL_strcmp(text, expected) == 0,
            "Verify returned text, expected: %s, got: %s",
            expected,
            text);
    }

    /* Verify setenv() with empty string vs unsetenv() */
    result = SDL_SetEnvironmentVariable(env, "FOO", "1", 1);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, 'FOO','1', 1)");
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);
    expected = "1";
    text = SDL_GetEnvironmentVariable(env, "FOO");
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, 'FOO')");
    SDLTest_AssertCheck(text && SDL_strcmp(text, expected) == 0, "Verify returned text, expected: %s, got: %s", expected, text);
    result = SDL_SetEnvironmentVariable(env, "FOO", "", 1);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, 'FOO','', 1)");
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);
    expected = "";
    text = SDL_GetEnvironmentVariable(env, "FOO");
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, 'FOO')");
    SDLTest_AssertCheck(text && SDL_strcmp(text, expected) == 0, "Verify returned text, expected: '%s', got: '%s'", expected, text);
    result = SDL_UnsetEnvironmentVariable(env, "FOO");
    SDLTest_AssertPass("Call to SDL_UnsetEnvironmentVariable(env, 'FOO')");
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);
    text = SDL_GetEnvironmentVariable(env, "FOO");
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, 'FOO')");
    SDLTest_AssertCheck(text == NULL, "Verify returned text, expected: (null), got: %s", text);
    result = SDL_SetEnvironmentVariable(env, "FOO", "0", 0);
    SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, 'FOO','0', 0)");
    SDLTest_AssertCheck(result == true, "Check result, expected: 1, got: %i", result);
    expected = "0";
    text = SDL_GetEnvironmentVariable(env, "FOO");
    SDLTest_AssertPass("Call to SDL_GetEnvironmentVariable(env, 'FOO')");
    SDLTest_AssertCheck(text && SDL_strcmp(text, expected) == 0, "Verify returned text, expected: %s, got: %s", expected, text);

    /* Negative cases */
    for (overwrite = 0; overwrite <= 1; overwrite++) {
        result = SDL_SetEnvironmentVariable(env, NULL, value1, overwrite);
        SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, NULL,'%s', %i)", value1, overwrite);
        SDLTest_AssertCheck(result == false, "Check result, expected: 0, got: %i", result);
        result = SDL_SetEnvironmentVariable(env, "", value1, overwrite);
        SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '','%s', %i)", value1, overwrite);
        SDLTest_AssertCheck(result == false, "Check result, expected: 0, got: %i", result);
        result = SDL_SetEnvironmentVariable(env, "=", value1, overwrite);
        SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '=','%s', %i)", value1, overwrite);
        SDLTest_AssertCheck(result == false, "Check result, expected: 0, got: %i", result);
        result = SDL_SetEnvironmentVariable(env, name, NULL, overwrite);
        SDLTest_AssertPass("Call to SDL_SetEnvironmentVariable(env, '%s', NULL, %i)", name, overwrite);
        SDLTest_AssertCheck(result == false, "Check result, expected: 0, got: %i", result);
    }

    /* Clean up */
    SDL_free(value1);
    SDL_free(value2);

    return TEST_COMPLETED;
}

#if defined(HAVE_WFORMAT) || defined(HAVE_WFORMAT_EXTRA_ARGS)
#pragma GCC diagnostic push
#ifdef HAVE_WFORMAT
#pragma GCC diagnostic ignored "-Wformat"
#endif
#ifdef HAVE_WFORMAT_EXTRA_ARGS
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif
#endif

#define FMT_PRILLd "%" SDL_PRILLd
#define FMT_PRILLdn "%" SDL_PRILLd "%" SDL_PRILL_PREFIX "n"
#define FMT_PRILLu "%" SDL_PRILLu

/**
 * Call to SDL_sscanf
 */
static int SDLCALL stdlib_sscanf(void *arg)
{
    int output;
    int result;
    int length;
    int expected_output;
    int expected_result;
    short short_output, expected_short_output, short_length;
    long long_output, expected_long_output, long_length;
    long long long_long_output, expected_long_long_output, long_long_length;
    size_t size_output, expected_size_output;
    void *ptr_output, *expected_ptr_output;
    char text[128], text2[128];
    unsigned int r = 0, g = 0, b = 0;

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
    length = 0;
    expected_output = 2;
    expected_result = 1;
    result = SDL_sscanf("2", "%i%n", &output, &length);
    SDLTest_AssertPass("Call to SDL_sscanf(\"2\", \"%%i%%n\", &output, &length)");
    SDLTest_AssertCheck(expected_output == output, "Check output, expected: %i, got: %i", expected_output, output);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);
    SDLTest_AssertCheck(length == 1, "Check length, expected: 1, got: %i", length);

    output = 123;
    length = 0;
    expected_output = 0xa;
    expected_result = 1;
    result = SDL_sscanf("aa", "%1x%n", &output, &length);
    SDLTest_AssertPass("Call to SDL_sscanf(\"aa\", \"%%1x%%n\", &output, &length)");
    SDLTest_AssertCheck(expected_output == output, "Check output, expected: %i, got: %i", expected_output, output);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);
    SDLTest_AssertCheck(length == 1, "Check length, expected: 1, got: %i", length);

    expected_result = 3;
    result = SDL_sscanf("#026", "#%1x%1x%1x", &r, &g, &b);
    SDLTest_AssertPass("Call to SDL_sscanf(\"#026\", \"#%%1x%%1x%%1x\", &r, &g, &b)");
    expected_output = 0;
    SDLTest_AssertCheck(r == expected_output, "Check output for r, expected: %i, got: %i", expected_output, r);
    expected_output = 2;
    SDLTest_AssertCheck(g == expected_output, "Check output for g, expected: %i, got: %i", expected_output, g);
    expected_output = 6;
    SDLTest_AssertCheck(b == expected_output, "Check output for b, expected: %i, got: %i", expected_output, b);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

#define SIZED_TEST_CASE(type, var, printf_specifier, scanf_specifier)                                                                                                            \
    var##_output = 123;                                                                                                                                                          \
    var##_length = 0;                                                                                                                                                            \
    expected_##var##_output = (type)(((unsigned type)(~0)) >> 1);                                                                                                                \
    expected_result = 1;                                                                                                                                                         \
    result = SDL_snprintf(text, sizeof(text), printf_specifier, expected_##var##_output);                                                                                        \
    result = SDL_sscanf(text, scanf_specifier, &var##_output, &var##_length);                                                                                                    \
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", %s, &output, &length)", text, #scanf_specifier);                                                                              \
    SDLTest_AssertCheck(expected_##var##_output == var##_output, "Check output, expected: " printf_specifier ", got: " printf_specifier, expected_##var##_output, var##_output); \
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);                                                        \
    SDLTest_AssertCheck(var##_length == (type)SDL_strlen(text), "Check length, expected: %i, got: %i", (int)SDL_strlen(text), (int)var##_length);                                \
                                                                                                                                                                                 \
    var##_output = 123;                                                                                                                                                          \
    var##_length = 0;                                                                                                                                                            \
    expected_##var##_output = ~(type)(((unsigned type)(~0)) >> 1);                                                                                                               \
    expected_result = 1;                                                                                                                                                         \
    result = SDL_snprintf(text, sizeof(text), printf_specifier, expected_##var##_output);                                                                                        \
    result = SDL_sscanf(text, scanf_specifier, &var##_output, &var##_length);                                                                                                    \
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", %s, &output, &length)", text, #scanf_specifier);                                                                              \
    SDLTest_AssertCheck(expected_##var##_output == var##_output, "Check output, expected: " printf_specifier ", got: " printf_specifier, expected_##var##_output, var##_output); \
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);                                                        \
    SDLTest_AssertCheck(var##_length == (type)SDL_strlen(text), "Check length, expected: %i, got: %i", (int)SDL_strlen(text), (int)var##_length);                                \

    SIZED_TEST_CASE(short, short, "%hd", "%hd%hn")
    SIZED_TEST_CASE(long, long, "%ld", "%ld%ln")
    SIZED_TEST_CASE(long long, long_long, FMT_PRILLd, FMT_PRILLdn)

    size_output = 123;
    expected_size_output = ~((size_t)0);
    expected_result = 1;
    result = SDL_snprintf(text, sizeof(text), "%zu", expected_size_output);
    result = SDL_sscanf(text, "%zu", &size_output);
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", \"%%zu\", &output)", text);
    SDLTest_AssertCheck(expected_size_output == size_output, "Check output, expected: %zu, got: %zu", expected_size_output, size_output);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    ptr_output = (void *)123;
    expected_ptr_output = (void *)0x1234567;
    expected_result = 1;
    result = SDL_snprintf(text, sizeof(text), "%p", expected_ptr_output);
    result = SDL_sscanf(text, "%p", &ptr_output);
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", \"%%p\", &output)", text);
    SDLTest_AssertCheck(expected_ptr_output == ptr_output, "Check output, expected: %p, got: %p", expected_ptr_output, ptr_output);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 1;
    text[0] = '\0';
    result = SDL_sscanf("abc def", "%s", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc def\", \"%%s\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 1;
    text[0] = '\0';
    result = SDL_sscanf("abc,def", "%s", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%s\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc,def") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 1;
    text[0] = '\0';
    result = SDL_sscanf("abc,def", "%[cba]", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%[cba]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 1;
    text[0] = '\0';
    result = SDL_sscanf("abc,def", "%[a-z]", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%[z-a]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 1;
    text[0] = '\0';
    result = SDL_sscanf("abc,def", "%[^,]", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%[^,]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 0;
    text[0] = '\0';
    result = SDL_sscanf("abc,def", "%[A-Z]", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%[A-Z]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "") == 0, "Check output, expected: \"\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 2;
    text[0] = '\0';
    text2[0] = '\0';
    result = SDL_sscanf("abc,def", "%[abc],%[def]", text, text2);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%[abc],%%[def]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(SDL_strcmp(text2, "def") == 0, "Check output, expected: \"def\", got: \"%s\"", text2);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 2;
    text[0] = '\0';
    text2[0] = '\0';
    result = SDL_sscanf("abc,def", "%[abc]%*[,]%[def]", text, text2);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc,def\", \"%%[abc]%%*[,]%%[def]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(SDL_strcmp(text2, "def") == 0, "Check output, expected: \"def\", got: \"%s\"", text2);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 2;
    text[0] = '\0';
    text2[0] = '\0';
    result = SDL_sscanf("abc   def", "%[abc] %[def]", text, text2);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc   def\", \"%%[abc] %%[def]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc") == 0, "Check output, expected: \"abc\", got: \"%s\"", text);
    SDLTest_AssertCheck(SDL_strcmp(text2, "def") == 0, "Check output, expected: \"def\", got: \"%s\"", text2);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    expected_result = 1;
    text[0] = '\0';
    result = SDL_sscanf("abc123XYZ", "%[a-zA-Z0-9]", text);
    SDLTest_AssertPass("Call to SDL_sscanf(\"abc123XYZ\", \"%%[a-zA-Z0-9]\", text)");
    SDLTest_AssertCheck(SDL_strcmp(text, "abc123XYZ") == 0, "Check output, expected: \"abc123XYZ\", got: \"%s\"", text);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    return TEST_COMPLETED;
}

#if defined(HAVE_WFORMAT) || defined(HAVE_WFORMAT_EXTRA_ARGS)
#pragma GCC diagnostic pop
#endif

#ifdef _WIN64
#define SIZE_FORMAT "I64u"
#elif defined(SDL_PLATFORM_WIN32)
#define SIZE_FORMAT "I32u"
#else
#define SIZE_FORMAT "zu"
#endif

/**
 * Call to SDL_aligned_alloc
 */
static int SDLCALL stdlib_aligned_alloc(void *arg)
{
    size_t i, alignment;
    void *ptr;

    for (i = 0; i < 2*sizeof(void *); ++i) {
        SDLTest_AssertPass("Call to SDL_aligned_alloc(%"SIZE_FORMAT")", i);
        ptr = SDL_aligned_alloc(i, 1);
        if (i < sizeof(void *)) {
            alignment = sizeof(void *);
        } else {
            alignment = i;
        }
        SDLTest_AssertCheck(ptr != NULL, "Check output, expected non-NULL, got: %p", ptr);
        SDLTest_AssertCheck((((size_t)ptr) % alignment) == 0, "Check output, expected aligned pointer, actual offset: %"SIZE_FORMAT, (((size_t)ptr) % alignment));
        if (ptr != NULL) {
            SDLTest_AssertPass("Filling memory to alignment value");
            SDL_memset(ptr, 0xAA, alignment);
            SDL_aligned_free(ptr);
        }
    }

    return TEST_COMPLETED;
}

typedef struct
{
    size_t a;
    size_t b;
    size_t result;
    bool status;
} overflow_test;

static const overflow_test multiplications[] = {
    { 1, 1, 1, true },
    { 0, 0, 0, true },
    { SDL_SIZE_MAX, 0, 0, true },
    { SDL_SIZE_MAX, 1, SDL_SIZE_MAX, true },
    { SDL_SIZE_MAX / 2, 2, SDL_SIZE_MAX - (SDL_SIZE_MAX % 2), true },
    { SDL_SIZE_MAX / 23, 23, SDL_SIZE_MAX - (SDL_SIZE_MAX % 23), true },

    { (SDL_SIZE_MAX / 2) + 1, 2, 0, false },
    { (SDL_SIZE_MAX / 23) + 42, 23, 0, false },
    { SDL_SIZE_MAX, SDL_SIZE_MAX, 0, false },
};

static const overflow_test additions[] = {
    { 1, 1, 2, true },
    { 0, 0, 0, true },
    { SDL_SIZE_MAX, 0, SDL_SIZE_MAX, true },
    { SDL_SIZE_MAX - 1, 1, SDL_SIZE_MAX, true },
    { SDL_SIZE_MAX - 42, 23, SDL_SIZE_MAX - (42 - 23), true },

    { SDL_SIZE_MAX, 1, 0, false },
    { SDL_SIZE_MAX, 23, 0, false },
    { SDL_SIZE_MAX, SDL_SIZE_MAX, 0, false },
};

static int SDLCALL
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
                status = SDL_size_mul_check_overflow(t->a, t->b, &result);
            } else {
                /* This disables the macro that tries to use a gcc/clang
                 * builtin, so we test the fallback implementation instead. */
                status = (SDL_size_mul_check_overflow)(t->a, t->b, &result);
            }

            if (t->status) {
                SDLTest_AssertCheck(status,
                                    "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should succeed",
                                    t->a, t->b);
                SDLTest_AssertCheck(result == t->result,
                                    "(%" SIZE_FORMAT " * %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                    t->a, t->b, t->result, result);
            } else {
                SDLTest_AssertCheck(!status,
                                    "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should fail",
                                    t->a, t->b);
            }

            if (t->a == t->b) {
                continue;
            }

            result = ~t->result;

            if (useBuiltin) {
                status = SDL_size_mul_check_overflow(t->b, t->a, &result);
            } else {
                status = (SDL_size_mul_check_overflow)(t->b, t->a, &result);
            }

            if (t->status) {
                SDLTest_AssertCheck(status,
                                    "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should succeed",
                                    t->b, t->a);
                SDLTest_AssertCheck(result == t->result,
                                    "(%" SIZE_FORMAT " * %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                    t->b, t->a, t->result, result);
            } else {
                SDLTest_AssertCheck(!status,
                                    "(%" SIZE_FORMAT " * %" SIZE_FORMAT ") should fail",
                                    t->b, t->a);
            }
        }

        for (i = 0; i < SDL_arraysize(additions); i++) {
            const overflow_test *t = &additions[i];
            bool status;
            size_t result = ~t->result;

            if (useBuiltin) {
                status = SDL_size_add_check_overflow(t->a, t->b, &result);
            } else {
                status = (SDL_size_add_check_overflow)(t->a, t->b, &result);
            }

            if (t->status) {
                SDLTest_AssertCheck(status,
                                    "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should succeed",
                                    t->a, t->b);
                SDLTest_AssertCheck(result == t->result,
                                    "(%" SIZE_FORMAT " + %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                    t->a, t->b, t->result, result);
            } else {
                SDLTest_AssertCheck(!status,
                                    "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should fail",
                                    t->a, t->b);
            }

            if (t->a == t->b) {
                continue;
            }

            result = ~t->result;

            if (useBuiltin) {
                status = SDL_size_add_check_overflow(t->b, t->a, &result);
            } else {
                status = (SDL_size_add_check_overflow)(t->b, t->a, &result);
            }

            if (t->status) {
                SDLTest_AssertCheck(status,
                                    "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should succeed",
                                    t->b, t->a);
                SDLTest_AssertCheck(result == t->result,
                                    "(%" SIZE_FORMAT " + %" SIZE_FORMAT "): expected %" SIZE_FORMAT ", got %" SIZE_FORMAT,
                                    t->b, t->a, t->result, result);
            } else {
                SDLTest_AssertCheck(!status,
                                    "(%" SIZE_FORMAT " + %" SIZE_FORMAT ") should fail",
                                    t->b, t->a);
            }
        }
    }

    return TEST_COMPLETED;
}

static void format_for_description(char *buffer, size_t buflen, const char *text) {
    if (text == NULL) {
        SDL_strlcpy(buffer, "NULL", buflen);
    } else {
        SDL_snprintf(buffer, buflen, "\"%s\"", text);
    }
}

static int SDLCALL
stdlib_iconv(void *arg)
{
    struct {
        bool expect_success;
        const char *from_encoding;
        const char *text;
        const char *to_encoding;
        const char *expected;
    } inputs[] = {
        { false, "bogus-from-encoding", NULL,                           "bogus-to-encoding",   NULL },
        { false, "bogus-from-encoding", "hello world",                  "bogus-to-encoding",   NULL },
        { false, "bogus-from-encoding", "hello world",                  "ascii",               NULL },
        { true,  "utf-8",               NULL,                           "ascii",               "" },
        { true,  "utf-8",               "hello world",                  "ascii",               "hello world" },
        { true,  "utf-8",               "\xe2\x8c\xa8\xf0\x9f\x92\xbb", "utf-16le",            "\x28\x23\x3d\xd8\xbb\xdc\x00" },
    };
    SDL_iconv_t cd;
    size_t i;

    for (i = 0; i < SDL_arraysize(inputs); i++) {
        char to_encoding_str[32];
        char from_encoding_str[32];
        char text_str[32];
        size_t len_text = 0;
        int r;
        char out_buffer[6];
        const char *in_ptr;
        size_t in_pos;
        char *out_ptr;
        char *output;
        size_t iconv_result;
        size_t out_len;
        bool is_error;
        size_t out_pos;

        SDLTest_AssertPass("case %d", (int)i);
        format_for_description(to_encoding_str, SDL_arraysize(to_encoding_str), inputs[i].to_encoding);
        format_for_description(from_encoding_str, SDL_arraysize(from_encoding_str), inputs[i].from_encoding);
        format_for_description(text_str, SDL_arraysize(text_str), inputs[i].text);

        if (inputs[i].text) {
            len_text = SDL_strlen(inputs[i].text) + 1;
        }

        SDLTest_AssertPass("About to call SDL_iconv_open(%s, %s)", to_encoding_str, from_encoding_str);
        cd = SDL_iconv_open(inputs[i].to_encoding, inputs[i].from_encoding);
        if (inputs[i].expect_success) {
            SDLTest_AssertCheck(cd != (SDL_iconv_t)SDL_ICONV_ERROR, "result must NOT be SDL_ICONV_ERROR");
        } else {
            SDLTest_AssertCheck(cd == (SDL_iconv_t)SDL_ICONV_ERROR, "result must be SDL_ICONV_ERROR");
        }

        in_ptr = inputs[i].text;
        in_pos = 0;
        out_pos = 0;
        do {
            size_t in_left;
            size_t count_written;
            size_t count_read;

            in_left = len_text - in_pos;
            out_ptr = out_buffer;
            out_len = SDL_arraysize(out_buffer);
            SDLTest_AssertPass("About to call SDL_iconv(cd, %s+%d, .., dest, ..)", text_str, (int)in_pos);
            iconv_result = SDL_iconv(cd, &in_ptr, &in_left, &out_ptr, &out_len);
            count_written = SDL_arraysize(out_buffer) - out_len;
            count_read = in_ptr - inputs[i].text - in_pos;
            in_pos += count_read;

            is_error = iconv_result == SDL_ICONV_ERROR
                       || iconv_result == SDL_ICONV_EILSEQ
                       || iconv_result == SDL_ICONV_EINVAL;
            if (inputs[i].expect_success) {
                SDLTest_AssertCheck(!is_error, "result must NOT be an error code");
                SDLTest_AssertCheck(count_written > 0 || inputs[i].expected[out_pos] == '\0', "%" SDL_PRIu64 " bytes have been written", (Uint64)count_written);
                SDLTest_AssertCheck(out_pos <= SDL_strlen(inputs[i].expected), "Data written by SDL_iconv cannot be longer then reference output");
                SDLTest_CompareMemory(out_buffer, count_written, inputs[i].expected + out_pos, count_written);
            } else {
                SDLTest_AssertCheck(is_error, "result must be an error code");
                break;
            }
            out_pos += count_written;
            if (count_written == 0) {
                break;
            }
            if (count_read == 0) {
                SDLTest_AssertCheck(false, "SDL_iconv wrote data, but read no data");
                break;
            }
        } while (!is_error && in_pos < len_text);

        SDLTest_AssertPass("About to call SDL_iconv_close(cd)");
        r = SDL_iconv_close(cd);
        if (inputs[i].expect_success) {
            SDLTest_AssertCheck(r == 0, "result must be 0");
        } else {
            SDLTest_AssertCheck(r == -1, "result must be -1");
        }

        SDLTest_AssertPass("About to call SDL_iconv_string(%s, %s, %s, %" SDL_PRIu64 ")",
                           to_encoding_str, from_encoding_str, text_str, (Uint64)len_text);
        output = SDL_iconv_string(inputs[i].to_encoding, inputs[i].from_encoding, inputs[i].text, len_text);
        if (inputs[i].expect_success) {
            SDLTest_AssertCheck(output != NULL, "result must NOT be NULL");
            SDLTest_AssertCheck(SDL_strncmp(inputs[i].expected, output, SDL_strlen(inputs[i].expected)) == 0,
                                "converted string should be correct");
        } else {
            SDLTest_AssertCheck(output == NULL, "result must be NULL");
        }
        SDL_free(output);
    }

    return TEST_COMPLETED;
}


static int SDLCALL
stdlib_strpbrk(void *arg)
{
    struct {
        const char *input;
        const char *accept;
        int expected[3]; /* negative if NULL */
    } test_cases[] = {
        { "",               "",             { -1, -1, -1  } },
        { "abc",            "",             { -1, -1, -1  } },
        { "Abc",            "a",            { -1, -1, -1  } },
        { "abc",            "a",            {  0, -1, -1  } },
        { "abcbd",          "bbbb",         {  1,  3, -1  } },
        { "a;b;c",          ";",            {  1,  3, -1  } },
        { "a;b;c",          ",",            { -1, -1, -1  } },
        { "a:bbbb;c",       ";:",           {  1,  6, -1  } },
        { "Hello\tS DL\n",   " \t\r\n",     {  5,  7,  10 } },
    };
    int i;

    for (i = 0; i < SDL_arraysize(test_cases); i++) {
        int j;
        const char *input = test_cases[i].input;

        for (j = 0; j < SDL_arraysize(test_cases[i].expected); j++) {
            char *result;

            SDLTest_AssertPass("About to call SDL_strpbrk(\"%s\", \"%s\")", input, test_cases[i].accept);
            result = SDL_strpbrk(input, test_cases[i].accept);
            if (test_cases[i].expected[j] < 0) {
                SDLTest_AssertCheck(result == NULL, "Expected NULL, got %p", result);
            } else {
                SDLTest_AssertCheck(result == test_cases[i].input + test_cases[i].expected[j], "Expected %p, got %p", test_cases[i].input + test_cases[i].expected[j], result);
                input = test_cases[i].input + test_cases[i].expected[j] + 1;
            }
        }
    }
    return TEST_COMPLETED;
}

static int SDLCALL stdlib_wcstol(void *arg)
{
    const long long_max = (~0UL) >> 1;
    const long long_min = ((~0UL) >> 1) + 1UL;

#define WCSTOL_TEST_CASE(str, base, expected_result, expected_endp_offset) do {                             \
        const wchar_t *s = str;                                                                             \
        long r, expected_r = expected_result;                                                               \
        wchar_t *ep, *expected_ep = (wchar_t *)s + expected_endp_offset;                                    \
        r = SDL_wcstol(s, &ep, base);                                                                       \
        SDLTest_AssertPass("Call to SDL_wcstol(" #str ", &endp, " #base ")");                               \
        SDLTest_AssertCheck(r == expected_r, "Check result value, expected: %ld, got: %ld", expected_r, r); \
        SDLTest_AssertCheck(ep == expected_ep, "Check endp value, expected: %p, got: %p", expected_ep, ep); \
    } while (0)

    // infer decimal
    WCSTOL_TEST_CASE(L"\t  123abcxyz", 0, 123, 6); // skip leading space
    WCSTOL_TEST_CASE(L"+123abcxyz", 0, 123, 4);
    WCSTOL_TEST_CASE(L"-123abcxyz", 0, -123, 4);
    WCSTOL_TEST_CASE(L"99999999999999999999abcxyz", 0, long_max, 20);
    WCSTOL_TEST_CASE(L"-99999999999999999999abcxyz", 0, long_min, 21);

    // infer hexadecimal
    WCSTOL_TEST_CASE(L"0x123abcxyz", 0, 0x123abc, 8);
    WCSTOL_TEST_CASE(L"0X123ABCXYZ", 0, 0x123abc, 8); // uppercase X

    // infer octal
    WCSTOL_TEST_CASE(L"0123abcxyz", 0, 0123, 4);

    // arbitrary bases
    WCSTOL_TEST_CASE(L"00110011", 2, 51, 8);
    WCSTOL_TEST_CASE(L"-uvwxyz", 32, -991, 3);
    WCSTOL_TEST_CASE(L"ZzZzZzZzZzZzZ", 36, long_max, 13);

    WCSTOL_TEST_CASE(L"-0", 10, 0, 2);
    WCSTOL_TEST_CASE(L" - 1", 0, 0, 0); // invalid input

    // values near the bounds of the type
    if (sizeof(long) == 4) {
        WCSTOL_TEST_CASE(L"2147483647", 10, 2147483647, 10);
        WCSTOL_TEST_CASE(L"2147483648", 10, 2147483647, 10);
        WCSTOL_TEST_CASE(L"-2147483648", 10, -2147483647L - 1, 11);
        WCSTOL_TEST_CASE(L"-2147483649", 10, -2147483647L - 1, 11);
        WCSTOL_TEST_CASE(L"-9999999999999999999999999999999999999999", 10, -2147483647L - 1, 41);
    }

#undef WCSTOL_TEST_CASE

    return TEST_COMPLETED;
}

static int SDLCALL stdlib_strtox(void *arg)
{
    const unsigned long long ullong_max = ~0ULL;

#define STRTOX_TEST_CASE(func_name, type, format_spec, str, base, expected_result, expected_endp_offset) do {                    \
        const char *s = str;                                                                                                     \
        type r, expected_r = expected_result;                                                                                    \
        char *ep, *expected_ep = (char *)s + expected_endp_offset;                                                               \
        r = func_name(s, &ep, base);                                                                                             \
        SDLTest_AssertPass("Call to " #func_name "(" #str ", &endp, " #base ")");                                                \
        SDLTest_AssertCheck(r == expected_r, "Check result value, expected: " format_spec ", got: " format_spec, expected_r, r); \
        SDLTest_AssertCheck(ep == expected_ep, "Check endp value, expected: %p, got: %p", expected_ep, ep);                      \
    } while (0)

    // infer decimal
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "\t  123abcxyz", 0, 123, 6); // skip leading space
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "+123abcxyz", 0, 123, 4);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "+123abcxyz", 0, 123, 4);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "-123abcxyz", 0, -123, 4);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "9999999999999999999999999999999999999999abcxyz", 0, ullong_max, 40);

    // infer hexadecimal
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "0x123abcxyz", 0, 0x123abc, 8);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "0X123ABCXYZ", 0, 0x123abc, 8); // uppercase X

    // infer octal
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "0123abcxyz", 0, 0123, 4);

    // arbitrary bases
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "00110011", 2, 51, 8);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "-uvwxyz", 32, -991, 3);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "ZzZzZzZzZzZzZzZzZzZzZzZzZ", 36, ullong_max, 25);

    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "0", 0, 0, 1);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "0", 10, 0, 1);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "-0", 0, 0, 2);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, "-0", 10, 0, 2);
    STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLu, " - 1", 0, 0, 0); // invalid input

    // We know that SDL_strtol, SDL_strtoul and SDL_strtoll share the same code path as SDL_strtoull under the hood,
    // so the most interesting test cases are those close to the bounds of the integer type.

    // For simplicity, we only run long/long long tests when they are 32-bit/64-bit, respectively.
    // Suppressing warnings would be difficult otherwise.
    // Since the CI runs the tests against a variety of targets, this should be fine in practice.

    if (sizeof(long) == 4) {
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "0", 0, 0, 1);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "0", 10, 0, 1);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "-0", 0, 0, 2);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "-0", 10, 0, 2);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "2147483647", 10, 2147483647, 10);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "2147483648", 10, 2147483647, 10);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "-2147483648", 10, -2147483647L - 1, 11);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "-2147483649", 10, -2147483647L - 1, 11);
        STRTOX_TEST_CASE(SDL_strtol, long, "%ld", "-9999999999999999999999999999999999999999", 10, -2147483647L - 1, 41);

        STRTOX_TEST_CASE(SDL_strtoul, unsigned long, "%lu", "4294967295", 10, 4294967295UL, 10);
        STRTOX_TEST_CASE(SDL_strtoul, unsigned long, "%lu", "4294967296", 10, 4294967295UL, 10);
        STRTOX_TEST_CASE(SDL_strtoul, unsigned long, "%lu", "-4294967295", 10, 1, 11);
    }

    if (sizeof(long long) == 8) {
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "0", 0, 0LL, 1);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "0", 10, 0LL, 1);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "-0", 0, 0LL, 2);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "-0", 10, 0LL, 2);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "9223372036854775807", 10, 9223372036854775807LL, 19);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "9223372036854775808", 10, 9223372036854775807LL, 19);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "-9223372036854775808", 10, -9223372036854775807LL - 1, 20);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "-9223372036854775809", 10, -9223372036854775807LL - 1, 20);
        STRTOX_TEST_CASE(SDL_strtoll, long long, FMT_PRILLd, "-9999999999999999999999999999999999999999", 10, -9223372036854775807LL - 1, 41);

        STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLd, "18446744073709551615", 10, 18446744073709551615ULL, 20);
        STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLd, "18446744073709551616", 10, 18446744073709551615ULL, 20);
        STRTOX_TEST_CASE(SDL_strtoull, unsigned long long, FMT_PRILLd, "-18446744073709551615", 10, 1, 21);
    }

#undef STRTOX_TEST_CASE

    return TEST_COMPLETED;
}

static int SDLCALL stdlib_strtod(void *arg)
{
#define STRTOD_TEST_CASE(str, expected_result, expected_endp_offset) do {                                   \
        const char *s = str;                                                                                \
        double r, expected_r = expected_result;                                                             \
        char *ep, *expected_ep = (char *)s + expected_endp_offset;                                          \
        r = SDL_strtod(s, &ep);                                                                             \
        SDLTest_AssertPass("Call to SDL_strtod(" #str ", &endp)");                                          \
        SDLTest_AssertCheck(r == expected_r, "Check result value, expected: %f, got: %f", expected_r, r);   \
        SDLTest_AssertCheck(ep == expected_ep, "Check endp value, expected: %p, got: %p", expected_ep, ep); \
    } while (0)

    STRTOD_TEST_CASE("\t  123.75abcxyz", 123.75, 9); // skip leading space
    STRTOD_TEST_CASE("+999.555", 999.555, 8);
    STRTOD_TEST_CASE("-999.555", -999.555, 8);

#undef STRTOD_TEST_CASE

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Standard C routine test cases */
static const SDLTest_TestCaseReference stdlibTest_strnlen = {
    stdlib_strnlen, "stdlib_strnlen", "Call to SDL_strnlen", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_strlcpy = {
    stdlib_strlcpy, "stdlib_strlcpy", "Call to SDL_strlcpy", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_strstr = {
    stdlib_strstr, "stdlib_strstr", "Call to SDL_strstr", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_snprintf = {
    stdlib_snprintf, "stdlib_snprintf", "Call to SDL_snprintf", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_swprintf = {
    stdlib_swprintf, "stdlib_swprintf", "Call to SDL_swprintf", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_getsetenv = {
    stdlib_getsetenv, "stdlib_getsetenv", "Call to SDL_GetEnvironmentVariable and SDL_SetEnvironmentVariable", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_sscanf = {
    stdlib_sscanf, "stdlib_sscanf", "Call to SDL_sscanf", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_aligned_alloc = {
    stdlib_aligned_alloc, "stdlib_aligned_alloc", "Call to SDL_aligned_alloc", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTestOverflow = {
    stdlib_overflow, "stdlib_overflow", "Overflow detection", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_iconv = {
    stdlib_iconv, "stdlib_iconv", "Calls to SDL_iconv", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_strpbrk = {
    stdlib_strpbrk, "stdlib_strpbrk", "Calls to SDL_strpbrk", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_wcstol = {
    stdlib_wcstol, "stdlib_wcstol", "Calls to SDL_wcstol", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_strtox = {
    stdlib_strtox, "stdlib_strtox", "Calls to SDL_strtol, SDL_strtoul, SDL_strtoll and SDL_strtoull", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest_strtod = {
    stdlib_strtod, "stdlib_strtod", "Calls to SDL_strtod", TEST_ENABLED
};

/* Sequence of Standard C routine test cases */
static const SDLTest_TestCaseReference *stdlibTests[] = {
    &stdlibTest_strnlen,
    &stdlibTest_strlcpy,
    &stdlibTest_strstr,
    &stdlibTest_snprintf,
    &stdlibTest_swprintf,
    &stdlibTest_getsetenv,
    &stdlibTest_sscanf,
    &stdlibTest_aligned_alloc,
    &stdlibTestOverflow,
    &stdlibTest_iconv,
    &stdlibTest_strpbrk,
    &stdlibTest_wcstol,
    &stdlibTest_strtox,
    &stdlibTest_strtod,
    NULL
};

/* Standard C routine test suite (global) */
SDLTest_TestSuiteReference stdlibTestSuite = {
    "Stdlib",
    NULL,
    stdlibTests,
    NULL
};
