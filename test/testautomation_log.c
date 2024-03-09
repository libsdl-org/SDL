/**
 * Log test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

static SDL_LogOutputFunction original_function;
static void *original_userdata;

static void SDLCALL TestLogOutput(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    int *message_count = (int *)userdata;
    ++(*message_count);
}

static void EnableTestLog(int *message_count)
{
    *message_count = 0;
    SDL_LogGetOutputFunction(&original_function, &original_userdata);
    SDL_LogSetOutputFunction(TestLogOutput, message_count);
}

static void DisableTestLog()
{
    SDL_LogSetOutputFunction(original_function, original_userdata);
}

/* Fixture */

/* Test case functions */

/**
 * Check SDL_HINT_LOGGING functionality
 */
static int log_testHint(void *arg)
{
    int count;

    SDL_SetHint(SDL_HINT_LOGGING, NULL);
    SDLTest_AssertPass("SDL_SetHint(SDL_HINT_LOGGING, NULL)");
    {
        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);
    }

    SDL_SetHint(SDL_HINT_LOGGING, "debug");
    SDLTest_AssertPass("SDL_SetHint(SDL_HINT_LOGGING, \"debug\")");
    {
        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);
    }

    SDL_SetHint(SDL_HINT_LOGGING, "system=debug");
    SDLTest_AssertPass("SDL_SetHint(SDL_HINT_LOGGING, \"system=debug\")");
    {
        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);
    }

    SDL_SetHint(SDL_HINT_LOGGING, "app=warn,system=debug,assert=quiet,*=info");
    SDLTest_AssertPass("SDL_SetHint(SDL_HINT_LOGGING, \"app=warn,system=debug,assert=quiet,*=info\")");
    {
        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_ASSERT, SDL_LOG_PRIORITY_CRITICAL, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_ASSERT, SDL_LOG_PRIORITY_CRITICAL, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_INFO, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_INFO, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

    }

    SDL_SetHint(SDL_HINT_LOGGING, "0=4,3=2,2=0,*=3");
    SDLTest_AssertPass("SDL_SetHint(SDL_HINT_LOGGING, \"0=4,3=2,2=0,*=3\")");
    {
        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_ASSERT, SDL_LOG_PRIORITY_CRITICAL, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_ASSERT, SDL_LOG_PRIORITY_CRITICAL, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_INFO, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_INFO, \"test\")");
        SDLTest_AssertCheck(count == 1, "Check result value, expected: 1, got: %d", count);

        EnableTestLog(&count);
        SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_DEBUG, "test");
        DisableTestLog();
        SDLTest_AssertPass("SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM, SDL_LOG_PRIORITY_DEBUG, \"test\")");
        SDLTest_AssertCheck(count == 0, "Check result value, expected: 0, got: %d", count);

    }

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Log test cases */
static const SDLTest_TestCaseReference logTestHint = {
    (SDLTest_TestCaseFp)log_testHint, "log_testHint", "Check SDL_HINT_LOGGING functionality", TEST_ENABLED
};

/* Sequence of Log test cases */
static const SDLTest_TestCaseReference *logTests[] = {
    &logTestHint, NULL
};

/* Timer test suite (global) */
SDLTest_TestSuiteReference logTestSuite = {
    "Log",
    NULL,
    logTests,
    NULL
};
