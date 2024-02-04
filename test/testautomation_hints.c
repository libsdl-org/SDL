/**
 * Hints test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

static const char *HintsEnum[] = {
    SDL_HINT_ACCELEROMETER_AS_JOYSTICK,
    SDL_HINT_FRAMEBUFFER_ACCELERATION,
    SDL_HINT_GAMECONTROLLERCONFIG,
    SDL_HINT_GRAB_KEYBOARD,
    SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
    SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK,
    SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
    SDL_HINT_ORIENTATIONS,
    SDL_HINT_RENDER_DIRECT3D_THREADSAFE,
    SDL_HINT_RENDER_DRIVER,
    SDL_HINT_RENDER_OPENGL_SHADERS,
    SDL_HINT_RENDER_SCALE_QUALITY,
    SDL_HINT_RENDER_VSYNC,
    SDL_HINT_TIMER_RESOLUTION,
    SDL_HINT_VIDEO_ALLOW_SCREENSAVER,
    SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES,
    SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,
    SDL_HINT_VIDEO_WIN_D3DCOMPILER,
    SDL_HINT_VIDEO_X11_XRANDR,
    SDL_HINT_XINPUT_ENABLED,
};
static const char *HintsVerbose[] = {
    "SDL_ACCELEROMETER_AS_JOYSTICK",
    "SDL_FRAMEBUFFER_ACCELERATION",
    "SDL_GAMECONTROLLERCONFIG",
    "SDL_GRAB_KEYBOARD",
    "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS",
    "SDL_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK",
    "SDL_MOUSE_RELATIVE_MODE_WARP",
    "SDL_ORIENTATIONS",
    "SDL_RENDER_DIRECT3D_THREADSAFE",
    "SDL_RENDER_DRIVER",
    "SDL_RENDER_OPENGL_SHADERS",
    "SDL_RENDER_SCALE_QUALITY",
    "SDL_RENDER_VSYNC",
    "SDL_TIMER_RESOLUTION",
    "SDL_VIDEO_ALLOW_SCREENSAVER",
    "SDL_VIDEO_MAC_FULLSCREEN_SPACES",
    "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS",
    "SDL_VIDEO_WIN_D3DCOMPILER",
    "SDL_VIDEO_X11_XRANDR",
    "SDL_XINPUT_ENABLED"
};

SDL_COMPILE_TIME_ASSERT(HintsEnum, SDL_arraysize(HintsEnum) == SDL_arraysize(HintsVerbose));

static const int numHintsEnum = SDL_arraysize(HintsEnum);

/* Test case functions */

/**
 * Call to SDL_GetHint
 */
static int hints_getHint(void *arg)
{
    const char *result1;
    const char *result2;
    int i;

    for (i = 0; i < numHintsEnum; i++) {
        result1 = SDL_GetHint(HintsEnum[i]);
        SDLTest_AssertPass("Call to SDL_GetHint(%s) - using define definition", (char *)HintsEnum[i]);
        result2 = SDL_GetHint(HintsVerbose[i]);
        SDLTest_AssertPass("Call to SDL_GetHint(%s) - using string definition", (char *)HintsVerbose[i]);
        SDLTest_AssertCheck(
            (result1 == NULL && result2 == NULL) || (SDL_strcmp(result1, result2) == 0),
            "Verify returned values are equal; got: result1='%s' result2='%s",
            (result1 == NULL) ? "null" : result1,
            (result2 == NULL) ? "null" : result2);
    }

    return TEST_COMPLETED;
}

static void SDLCALL hints_testHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    *(char **)userdata = hint ? SDL_strdup(hint) : NULL;
}

/**
 * Call to SDL_SetHint
 */
static int hints_setHint(void *arg)
{
    const char *testHint = "SDL_AUTOMATED_TEST_HINT";
    const char *originalValue;
    char *value;
    const char *testValue;
    char *callbackValue;
    SDL_bool result;
    int i, j;

    /* Create random values to set */
    value = SDLTest_RandomAsciiStringOfSize(10);

    for (i = 0; i < numHintsEnum; i++) {
        /* Capture current value */
        originalValue = SDL_GetHint(HintsEnum[i]);
        SDLTest_AssertPass("Call to SDL_GetHint(%s)", HintsEnum[i]);

        /* Copy the original value, since it will be freed when we set it again */
        originalValue = originalValue ? SDL_strdup(originalValue) : NULL;

        /* Set value (twice) */
        for (j = 1; j <= 2; j++) {
            result = SDL_SetHint(HintsEnum[i], value);
            SDLTest_AssertPass("Call to SDL_SetHint(%s, %s) (iteration %i)", HintsEnum[i], value, j);
            SDLTest_AssertCheck(
                result == SDL_TRUE || result == SDL_FALSE,
                "Verify valid result was returned, got: %i",
                (int)result);
            testValue = SDL_GetHint(HintsEnum[i]);
            SDLTest_AssertPass("Call to SDL_GetHint(%s) - using string definition", HintsVerbose[i]);
            SDLTest_AssertCheck(
                (SDL_strcmp(value, testValue) == 0),
                "Verify returned value equals set value; got: testValue='%s' value='%s",
                (testValue == NULL) ? "null" : testValue,
                value);
        }

        /* Reset original value */
        result = SDL_SetHint(HintsEnum[i], originalValue);
        SDLTest_AssertPass("Call to SDL_SetHint(%s, originalValue)", HintsEnum[i]);
        SDLTest_AssertCheck(
            result == SDL_TRUE || result == SDL_FALSE,
            "Verify valid result was returned, got: %i",
            (int)result);
        SDL_free((void *)originalValue);
    }

    SDL_free(value);

    /* Set default value in environment */
    SDL_setenv(testHint, "original", 1);

    SDLTest_AssertPass("Call to SDL_GetHint() after saving and restoring hint");
    originalValue = SDL_GetHint(testHint);
    value = (originalValue == NULL) ? NULL : SDL_strdup(originalValue);
    SDL_SetHint(testHint, "temp");
    SDL_SetHint(testHint, value);
    SDL_free(value);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "original") == 0,
        "testValue = %s, expected \"original\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(NULL, SDL_HINT_DEFAULT)");
    SDL_SetHintWithPriority(testHint, NULL, SDL_HINT_DEFAULT);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "original") == 0,
        "testValue = %s, expected \"original\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE)");
    SDL_SetHintWithPriority(testHint, "temp", SDL_HINT_OVERRIDE);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "temp") == 0,
        "testValue = %s, expected \"temp\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(NULL, SDL_HINT_OVERRIDE)");
    SDL_SetHintWithPriority(testHint, NULL, SDL_HINT_OVERRIDE);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue == NULL,
        "testValue = %s, expected NULL",
        testValue);

    SDLTest_AssertPass("Call to SDL_ResetHint()");
    SDL_ResetHint(testHint);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "original") == 0,
        "testValue = %s, expected \"original\"",
        testValue);

    /* Make sure callback functionality works past a reset */
    SDLTest_AssertPass("Call to SDL_AddHintCallback()");
    callbackValue = NULL;
    SDL_AddHintCallback(testHint, hints_testHintChanged, &callbackValue);
    SDLTest_AssertCheck(
        callbackValue && SDL_strcmp(callbackValue, "original") == 0,
        "callbackValue = %s, expected \"original\"",
        callbackValue);
    SDL_free(callbackValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE), using callback");
    callbackValue = NULL;
    SDL_SetHintWithPriority(testHint, "temp", SDL_HINT_OVERRIDE);
    SDLTest_AssertCheck(
        callbackValue && SDL_strcmp(callbackValue, "temp") == 0,
        "callbackValue = %s, expected \"temp\"",
        callbackValue);
    SDL_free(callbackValue);

    SDLTest_AssertPass("Call to SDL_ResetHint(), using callback");
    callbackValue = NULL;
    SDL_ResetHint(testHint);
    SDLTest_AssertCheck(
        callbackValue && SDL_strcmp(callbackValue, "original") == 0,
        "callbackValue = %s, expected \"original\"",
        callbackValue);
    SDL_free(callbackValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE), using callback after reset");
    callbackValue = NULL;
    SDL_SetHintWithPriority(testHint, "temp", SDL_HINT_OVERRIDE);
    SDLTest_AssertCheck(
        callbackValue && SDL_strcmp(callbackValue, "temp") == 0,
        "callbackValue = %s, expected \"temp\"",
        callbackValue);
    SDL_free(callbackValue);

    SDLTest_AssertPass("Call to SDL_ResetHint(), after clearing callback");
    callbackValue = NULL;
    SDL_DelHintCallback(testHint, hints_testHintChanged, &callbackValue);
    SDL_ResetHint(testHint);
    SDLTest_AssertCheck(
        callbackValue == NULL,
        "callbackValue = %s, expected \"(null)\"",
        callbackValue);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Hints test cases */
static const SDLTest_TestCaseReference hintsTest1 = {
    (SDLTest_TestCaseFp)hints_getHint, "hints_getHint", "Call to SDL_GetHint", TEST_ENABLED
};

static const SDLTest_TestCaseReference hintsTest2 = {
    (SDLTest_TestCaseFp)hints_setHint, "hints_setHint", "Call to SDL_SetHint", TEST_ENABLED
};

/* Sequence of Hints test cases */
static const SDLTest_TestCaseReference *hintsTests[] = {
    &hintsTest1, &hintsTest2, NULL
};

/* Hints test suite (global) */
SDLTest_TestSuiteReference hintsTestSuite = {
    "Hints",
    NULL,
    hintsTests,
    NULL
};
