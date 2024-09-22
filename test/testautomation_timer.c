/**
 * Timer test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

#ifndef SDL_PLATFORM_EMSCRIPTEN

/* Flag indicating if the param should be checked */
static int g_paramCheck = 0;

/* Userdata value to check */
static int g_paramValue = 0;

/* Flag indicating that the callback was called */
static int g_timerCallbackCalled = 0;

#endif

/* Fixture */

static void SDLCALL timerSetUp(void **arg)
{
}

/* Test case functions */

/**
 * Call to SDL_GetPerformanceCounter
 */
static int SDLCALL timer_getPerformanceCounter(void *arg)
{
    Uint64 result;

    result = SDL_GetPerformanceCounter();
    SDLTest_AssertPass("Call to SDL_GetPerformanceCounter()");
    SDLTest_AssertCheck(result > 0, "Check result value, expected: >0, got: %" SDL_PRIu64, result);

    return TEST_COMPLETED;
}

/**
 * Call to SDL_GetPerformanceFrequency
 */
static int SDLCALL timer_getPerformanceFrequency(void *arg)
{
    Uint64 result;

    result = SDL_GetPerformanceFrequency();
    SDLTest_AssertPass("Call to SDL_GetPerformanceFrequency()");
    SDLTest_AssertCheck(result > 0, "Check result value, expected: >0, got: %" SDL_PRIu64, result);

    return TEST_COMPLETED;
}

/**
 * Call to SDL_Delay and SDL_GetTicks
 */
static int SDLCALL timer_delayAndGetTicks(void *arg)
{
    const int testDelay = 100;
    const int marginOfError = 25;
    Uint64 result;
    Uint64 result2;
    Sint64 difference;

    /* Zero delay */
    SDL_Delay(0);
    SDLTest_AssertPass("Call to SDL_Delay(0)");

    /* Non-zero delay */
    SDL_Delay(1);
    SDLTest_AssertPass("Call to SDL_Delay(1)");

    SDL_Delay(SDLTest_RandomIntegerInRange(5, 15));
    SDLTest_AssertPass("Call to SDL_Delay()");

    /* Get ticks count - should be non-zero by now */
    result = SDL_GetTicks();
    SDLTest_AssertPass("Call to SDL_GetTicks()");
    SDLTest_AssertCheck(result > 0, "Check result value, expected: >0, got: %" SDL_PRIu64, result);

    /* Delay a bit longer and measure ticks and verify difference */
    SDL_Delay(testDelay);
    SDLTest_AssertPass("Call to SDL_Delay(%d)", testDelay);
    result2 = SDL_GetTicks();
    SDLTest_AssertPass("Call to SDL_GetTicks()");
    SDLTest_AssertCheck(result2 > 0, "Check result value, expected: >0, got: %" SDL_PRIu64, result2);
    difference = result2 - result;
    SDLTest_AssertCheck(difference > (testDelay - marginOfError), "Check difference, expected: >%d, got: %" SDL_PRIu64, testDelay - marginOfError, difference);
#if 0
    /* Disabled because this might fail on non-interactive systems. Moved to testtimer. */
    SDLTest_AssertCheck(difference < (testDelay + marginOfError), "Check difference, expected: <%d, got: %" SDL_PRIu64, testDelay + marginOfError, difference);
#endif

    return TEST_COMPLETED;
}

#ifndef SDL_PLATFORM_EMSCRIPTEN

/* Test callback */
static Uint32 SDLCALL timerTestCallback(void *param, SDL_TimerID timerID, Uint32 interval)
{
    g_timerCallbackCalled = 1;

    if (g_paramCheck != 0) {
        SDLTest_AssertCheck(param != NULL, "Check param pointer, expected: non-NULL, got: %s", (param != NULL) ? "non-NULL" : "NULL");
        if (param != NULL) {
            SDLTest_AssertCheck(*(int *)param == g_paramValue, "Check param value, expected: %i, got: %i", g_paramValue, *(int *)param);
        }
    }

    return 0;
}

#endif

/**
 * Call to SDL_AddTimer and SDL_RemoveTimer
 */
static int SDLCALL timer_addRemoveTimer(void *arg)
{
#ifdef SDL_PLATFORM_EMSCRIPTEN
    SDLTest_Log("Timer callbacks on Emscripten require a main loop to handle events");
    return TEST_SKIPPED;
#else
    SDL_TimerID id;
    int result;
    int param;

    /* Reset state */
    g_paramCheck = 0;
    g_timerCallbackCalled = 0;

    /* Set timer with a long delay */
    id = SDL_AddTimer(10000, timerTestCallback, NULL);
    SDLTest_AssertPass("Call to SDL_AddTimer(10000,...)");
    SDLTest_AssertCheck(id > 0, "Check result value, expected: >0, got: %" SDL_PRIu32, id);

    /* Remove timer again and check that callback was not called */
    result = SDL_RemoveTimer(id);
    SDLTest_AssertPass("Call to SDL_RemoveTimer()");
    SDLTest_AssertCheck(result == true, "Check result value, expected: true, got: %i", result);
    SDLTest_AssertCheck(g_timerCallbackCalled == 0, "Check callback WAS NOT called, expected: 0, got: %i", g_timerCallbackCalled);

    /* Try to remove timer again (should be a NOOP) */
    result = SDL_RemoveTimer(id);
    SDLTest_AssertPass("Call to SDL_RemoveTimer()");
    SDLTest_AssertCheck(result == false, "Check result value, expected: false, got: %i", result);

    /* Reset state */
    param = SDLTest_RandomIntegerInRange(-1024, 1024);
    g_paramCheck = 1;
    g_paramValue = param;
    g_timerCallbackCalled = 0;

    /* Set timer with a short delay */
    id = SDL_AddTimer(10, timerTestCallback, (void *)&param);
    SDLTest_AssertPass("Call to SDL_AddTimer(10, param)");
    SDLTest_AssertCheck(id > 0, "Check result value, expected: >0, got: %" SDL_PRIu32, id);

    /* Wait to let timer trigger callback */
    SDL_Delay(100);
    SDLTest_AssertPass("Call to SDL_Delay(100)");

    /* Remove timer again and check that callback was called */
    result = SDL_RemoveTimer(id);
    SDLTest_AssertPass("Call to SDL_RemoveTimer()");
    SDLTest_AssertCheck(result == false, "Check result value, expected: false, got: %i", result);
    SDLTest_AssertCheck(g_timerCallbackCalled == 1, "Check callback WAS called, expected: 1, got: %i", g_timerCallbackCalled);

    return TEST_COMPLETED;
#endif
}

/* ================= Test References ================== */

/* Timer test cases */
static const SDLTest_TestCaseReference timerTest1 = {
    timer_getPerformanceCounter, "timer_getPerformanceCounter", "Call to SDL_GetPerformanceCounter", TEST_ENABLED
};

static const SDLTest_TestCaseReference timerTest2 = {
    timer_getPerformanceFrequency, "timer_getPerformanceFrequency", "Call to SDL_GetPerformanceFrequency", TEST_ENABLED
};

static const SDLTest_TestCaseReference timerTest3 = {
    timer_delayAndGetTicks, "timer_delayAndGetTicks", "Call to SDL_Delay and SDL_GetTicks", TEST_ENABLED
};

static const SDLTest_TestCaseReference timerTest4 = {
    timer_addRemoveTimer, "timer_addRemoveTimer", "Call to SDL_AddTimer and SDL_RemoveTimer", TEST_ENABLED
};

/* Sequence of Timer test cases */
static const SDLTest_TestCaseReference *timerTests[] = {
    &timerTest1, &timerTest2, &timerTest3, &timerTest4, NULL
};

/* Timer test suite (global) */
SDLTest_TestSuiteReference timerTestSuite = {
    "Timer",
    timerSetUp,
    timerTests,
    NULL
};
