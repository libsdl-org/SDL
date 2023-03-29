/**
 * SysWM test suite
 */

/* Avoid inclusion of e.g. X11 headers when these are not installed */
#include <build_config/SDL_build_config.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_syswm.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Test case functions */

/**
 * \brief Call to SDL_GetWindowWMInfo
 */
static int syswm_getWindowWMInfo(void *arg)
{
    int result;
    SDL_Window *window;
    SDL_SysWMinfo info;

    window = SDL_CreateWindow("", 0, 0, SDL_WINDOW_HIDDEN);
    SDLTest_AssertPass("Call to SDL_CreateWindow()");
    SDLTest_AssertCheck(window != NULL, "Check that value returned from SDL_CreateWindow is not NULL");
    if (window == NULL) {
        return TEST_ABORTED;
    }

    /* Make call */
    result = SDL_GetWindowWMInfo(window, &info, SDL_SYSWM_CURRENT_VERSION);
    SDLTest_AssertPass("Call to SDL_GetWindowWMInfo()");
    SDLTest_Log((result == 0) ? "Got window information" : "Couldn't get window information");

    SDL_DestroyWindow(window);
    SDLTest_AssertPass("Call to SDL_DestroyWindow()");

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* SysWM test cases */
static const SDLTest_TestCaseReference syswmTest1 = {
    (SDLTest_TestCaseFp)syswm_getWindowWMInfo, "syswm_getWindowWMInfo", "Call to SDL_GetWindowWMInfo", TEST_ENABLED
};

/* Sequence of SysWM test cases */
static const SDLTest_TestCaseReference *syswmTests[] = {
    &syswmTest1, NULL
};

/* SysWM test suite (global) */
SDLTest_TestSuiteReference syswmTestSuite = {
    "SysWM",
    NULL,
    syswmTests,
    NULL
};
