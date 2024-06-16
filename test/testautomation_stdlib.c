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
#undef SDL_strnlen
static int stdlib_strnlen(void *arg)
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
#undef SDL_strlcpy
static int stdlib_strlcpy(void *arg)
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
static int stdlib_strstr(void *arg)
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
#undef SDL_snprintf
static int stdlib_snprintf(void *arg)
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
        result = SDL_snprintf(text, sizeof(text), "%p", (void *)0x1ba07bddf60L);
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
#undef SDL_swprintf
static int stdlib_swprintf(void *arg)
{
    int result;
    int predicted;
    wchar_t text[1024];
    const wchar_t *expected;
    size_t size;

    result = SDL_swprintf(text, sizeof(text), L"%s", "foo");
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
 * Call to SDL_getenv and SDL_setenv
 */
static int stdlib_getsetenv(void *arg)
{
    const int nameLen = 16;
    char name[17];
    int counter;
    int result;
    char *value1;
    char *value2;
    char *expected;
    int overwrite;
    char *text;

    /* Create a random name. This tests SDL_getenv, since we need to */
    /* make sure the variable is not set yet (it shouldn't). */
    do {
        for (counter = 0; counter < nameLen; counter++) {
            name[counter] = (char)SDLTest_RandomIntegerInRange(65, 90);
        }
        name[nameLen] = '\0';

        text = SDL_getenv(name);
        SDLTest_AssertPass("Call to SDL_getenv('%s')", name);
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
    for (overwrite = 0; overwrite <= 1; overwrite++) {
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
 * Call to SDL_sscanf
 */
#undef SDL_sscanf
static int stdlib_sscanf(void *arg)
{
    int output;
    int result;
    int expected_output;
    int expected_result;
    short short_output, expected_short_output;
    long long_output, expected_long_output;
    long long long_long_output, expected_long_long_output;
    size_t size_output, expected_size_output;
    char text[128], text2[128];

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

    output = 123;
    expected_output = 0xa;
    expected_result = 1;
    result = SDL_sscanf("aa", "%1x", &output);
    SDLTest_AssertPass("Call to SDL_sscanf(\"aa\", \"%%1x\", &output)");
    SDLTest_AssertCheck(expected_output == output, "Check output, expected: %i, got: %i", expected_output, output);
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

#define SIZED_TEST_CASE(type, var, format_specifier)                                                                                                                             \
    var##_output = 123;                                                                                                                                                          \
    expected_##var##_output = (type)(((unsigned type)(~0)) >> 1);                                                                                                                \
    expected_result = 1;                                                                                                                                                         \
    result = SDL_snprintf(text, sizeof(text), format_specifier, expected_##var##_output);                                                                                        \
    result = SDL_sscanf(text, format_specifier, &var##_output);                                                                                                                  \
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", \"%s\", &output)", text, #format_specifier);                                                                                  \
    SDLTest_AssertCheck(expected_##var##_output == var##_output, "Check output, expected: " format_specifier ", got: " format_specifier, expected_##var##_output, var##_output); \
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);                                                        \
                                                                                                                                                                                 \
    var##_output = 123;                                                                                                                                                          \
    expected_##var##_output = ~(type)(((unsigned type)(~0)) >> 1);                                                                                                               \
    expected_result = 1;                                                                                                                                                         \
    result = SDL_snprintf(text, sizeof(text), format_specifier, expected_##var##_output);                                                                                        \
    result = SDL_sscanf(text, format_specifier, &var##_output);                                                                                                                  \
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", \"%s\", &output)", text, #format_specifier);                                                                                  \
    SDLTest_AssertCheck(expected_##var##_output == var##_output, "Check output, expected: " format_specifier ", got: " format_specifier, expected_##var##_output, var##_output); \
    SDLTest_AssertCheck(expected_result == result, "Check return value, expected: %i, got: %i", expected_result, result);

    SIZED_TEST_CASE(short, short, "%hd")
    SIZED_TEST_CASE(long, long, "%ld")
    SIZED_TEST_CASE(long long, long_long, "%lld")

    size_output = 123;
    expected_size_output = ~((size_t)0);
    expected_result = 1;
    result = SDL_snprintf(text, sizeof(text), "%zu", expected_size_output);
    result = SDL_sscanf(text, "%zu", &size_output);
    SDLTest_AssertPass("Call to SDL_sscanf(\"%s\", \"%%zu\", &output)", text);
    SDLTest_AssertCheck(expected_size_output == size_output, "Check output, expected: %zu, got: %zu", expected_size_output, size_output);
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
static int stdlib_aligned_alloc(void *arg)
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
        SDLTest_AssertPass("Filling memory to alignment value");
        SDL_memset(ptr, 0xAA, alignment);
        SDL_aligned_free(ptr);
    }

    return TEST_COMPLETED;
}

typedef struct
{
    size_t a;
    size_t b;
    size_t result;
    int status;
} overflow_test;

static const overflow_test multiplications[] = {
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

static const overflow_test additions[] = {
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
static const SDLTest_TestCaseReference stdlibTest1 = {
    stdlib_strnlen, "stdlib_strnlen", "Call to SDL_strnlen", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest2 = {
    stdlib_strlcpy, "stdlib_strlcpy", "Call to SDL_strlcpy", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest3 = {
    stdlib_strstr, "stdlib_strstr", "Call to SDL_strstr", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest4 = {
    stdlib_snprintf, "stdlib_snprintf", "Call to SDL_snprintf", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest5 = {
    stdlib_swprintf, "stdlib_swprintf", "Call to SDL_swprintf", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest6 = {
    stdlib_getsetenv, "stdlib_getsetenv", "Call to SDL_getenv and SDL_setenv", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest7 = {
    stdlib_sscanf, "stdlib_sscanf", "Call to SDL_sscanf", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTest8 = {
    stdlib_aligned_alloc, "stdlib_aligned_alloc", "Call to SDL_aligned_alloc", TEST_ENABLED
};

static const SDLTest_TestCaseReference stdlibTestOverflow = {
    stdlib_overflow, "stdlib_overflow", "Overflow detection", TEST_ENABLED
};

/* Sequence of Standard C routine test cases */
static const SDLTest_TestCaseReference *stdlibTests[] = {
    &stdlibTest1,
    &stdlibTest2,
    &stdlibTest3,
    &stdlibTest4,
    &stdlibTest5,
    &stdlibTest6,
    &stdlibTest7,
    &stdlibTest8,
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
