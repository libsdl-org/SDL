/**
 * Hints test suite
 */

#include <stdio.h>

#include "SDL.h"
#include "SDL_test.h"

const char *_HintsEnum[] = {
    SDL_HINT_ACCELEROMETER_AS_JOYSTICK,
    SDL_HINT_FRAMEBUFFER_ACCELERATION,
    SDL_HINT_GAMECONTROLLERCONFIG,
    SDL_HINT_GRAB_KEYBOARD,
    SDL_HINT_IDLE_TIMER_DISABLED,
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
    SDL_HINT_VIDEO_HIGHDPI_DISABLED,
    SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES,
    SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,
    SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT,
    SDL_HINT_VIDEO_WIN_D3DCOMPILER,
    SDL_HINT_VIDEO_X11_XRANDR,
    SDL_HINT_XINPUT_ENABLED,
};
const char *_HintsVerbose[] = {
    "SDL_ACCELEROMETER_AS_JOYSTICK",
    "SDL_FRAMEBUFFER_ACCELERATION",
    "SDL_GAMECONTROLLERCONFIG",
    "SDL_GRAB_KEYBOARD",
    "SDL_IDLE_TIMER_DISABLED",
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
    "SDL_VIDEO_HIGHDPI_DISABLED",
    "SDL_VIDEO_MAC_FULLSCREEN_SPACES",
    "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS",
    "SDL_VIDEO_WINDOW_SHARE_PIXEL_FORMAT",
    "SDL_VIDEO_WIN_D3DCOMPILER",
    "SDL_VIDEO_X11_XRANDR",
    "SDL_XINPUT_ENABLED"
};

SDL_COMPILE_TIME_ASSERT(HintsEnum, SDL_arraysize(_HintsEnum) == SDL_arraysize(_HintsVerbose));

const int _numHintsEnum = SDL_arraysize(_HintsEnum);

/* Test case functions */

/**
 * @brief Call to SDL_GetHint
 */
int hints_getHint(void *arg)
{
    const char *result1;
    const char *result2;
    int i;

    for (i = 0; i < _numHintsEnum; i++) {
        result1 = SDL_GetHint(_HintsEnum[i]);
        SDLTest_AssertPass("Call to SDL_GetHint(%s) - using define definition", (char *)_HintsEnum[i]);
        result2 = SDL_GetHint(_HintsVerbose[i]);
        SDLTest_AssertPass("Call to SDL_GetHint(%s) - using string definition", (char *)_HintsVerbose[i]);
        SDLTest_AssertCheck(
            (result1 == NULL && result2 == NULL) || (SDL_strcmp(result1, result2) == 0),
            "Verify returned values are equal; got: result1='%s' result2='%s",
            (result1 == NULL) ? "null" : result1,
            (result2 == NULL) ? "null" : result2);
    }

    return TEST_COMPLETED;
}

typedef struct {
    char *name;
    char *value;
    char *oldValue;
} HintCallbackContext;

static void SDLCALL hints_testHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    HintCallbackContext *context = userdata;

    context->name = name ? SDL_strdup(name) : NULL;
    context->value = hint ? SDL_strdup(hint) : NULL;
    context->oldValue = oldValue ? SDL_strdup(oldValue) : NULL;
}

/**
 * @brief Call to SDL_SetHint
 */
int hints_setHint(void *arg)
{
    const char *testHint = "SDL_AUTOMATED_TEST_HINT";
    const char *originalValue;
    char *value;
    const char *testValue;
    HintCallbackContext callback_data;
    SDL_bool result;
    int i, j;

    /* Create random values to set */
    value = SDLTest_RandomAsciiStringOfSize(10);

    for (i = 0; i < _numHintsEnum; i++) {
        /* Capture current value */
        originalValue = SDL_GetHint(_HintsEnum[i]);
        SDLTest_AssertPass("Call to SDL_GetHint(%s)", _HintsEnum[i]);

        /* Copy the original value, since it will be freed when we set it again */
        originalValue = originalValue ? SDL_strdup(originalValue) : NULL;

        /* Set value (twice) */
        for (j = 1; j <= 2; j++) {
            result = SDL_SetHint(_HintsEnum[i], value);
            SDLTest_AssertPass("Call to SDL_SetHint(%s, %s) (iteration %i)", _HintsEnum[i], value, j);
            SDLTest_AssertCheck(
                result == SDL_TRUE || result == SDL_FALSE,
                "Verify valid result was returned, got: %i",
                (int)result);
            testValue = SDL_GetHint(_HintsEnum[i]);
            SDLTest_AssertPass("Call to SDL_GetHint(%s) - using string definition", _HintsVerbose[i]);
            SDLTest_AssertCheck(
                (SDL_strcmp(value, testValue) == 0),
                "Verify returned value equals set value; got: testValue='%s' value='%s",
                (testValue == NULL) ? "null" : testValue,
                value);
        }

        /* Reset original value */
        result = SDL_SetHint(_HintsEnum[i], originalValue);
        SDLTest_AssertPass("Call to SDL_SetHint(%s, originalValue)", _HintsEnum[i]);
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
    result = SDL_SetHint(testHint, "temp");
    SDLTest_AssertCheck(!result, "SDL_SetHint(\"%s\", \"temp\") should return false", testHint);
    result = SDL_SetHint(testHint, value);
    SDLTest_AssertCheck(!result, "SDL_SetHint(\"%s\", \"%s\" should return false", testHint, value);
    SDL_free(value);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "original") == 0,
        "testValue = %s, expected \"original\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(NULL, SDL_HINT_DEFAULT)");
    result = SDL_SetHintWithPriority(testHint, NULL, SDL_HINT_DEFAULT);
    SDLTest_AssertCheck(!result, "SDL_SetHintWithPriority(\"%s\", NULL, SDL_HINT_DEFAULT) should return false", testHint);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "original") == 0,
        "testValue = %s, expected \"original\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHint(\"\", \"data\")");
    result = SDL_SetHint("", "data");
    SDLTest_AssertCheck(result, "SDL_SetHint(\"\", \"data\") should return true");
    testValue = SDL_GetHint("");
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "data") == 0,
        "testValue = %s, expected \"data\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE)");
    result = SDL_SetHintWithPriority(testHint, "temp", SDL_HINT_OVERRIDE);
    SDLTest_AssertCheck(result, "SDL_SetHintWithPriority(\"%s\", \"temp\", SDL_HINT_OVERRIDE) should return true", testHint);
    testValue = SDL_GetHint(testHint);
    SDLTest_AssertCheck(
        testValue && SDL_strcmp(testValue, "temp") == 0,
        "testValue = %s, expected \"temp\"",
        testValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(NULL, SDL_HINT_OVERRIDE)");
    result = SDL_SetHintWithPriority(testHint, NULL, SDL_HINT_OVERRIDE);
    SDLTest_AssertCheck(result, "SDL_SetHintWithPriority(\"%s\", NULL, SDL_HINT_OVERRIDE) should return true", testHint);
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
    SDL_zero(callback_data);
    SDLTest_AssertPass("Call to SDL_AddHintCallback()");
    SDL_AddHintCallback(testHint, hints_testHintChanged, &callback_data);
    SDLTest_AssertCheck(
        callback_data.name && SDL_strcmp(callback_data.name, testHint) == 0,
        "callback_data.name = \"%s\", expected \"%s\"",
        callback_data.name, testHint);
    SDLTest_AssertCheck(
        callback_data.value && SDL_strcmp(callback_data.value, "original") == 0,
        "callback_data.value = \"%s\", expected \"%s\"",
        callback_data.value, "original");
    SDL_free(callback_data.name);
    SDL_free(callback_data.value);
    SDL_free(callback_data.oldValue);

    SDLTest_AssertPass("Call to SDL_ResetHint(), using callback");
    SDL_zero(callback_data);
    SDL_ResetHint(testHint);
    SDLTest_AssertCheck(
        callback_data.value && SDL_strcmp(callback_data.value, "original") == 0,
        "callbackValue = %s, expected \"original\"",
        callback_data.value);
    SDL_free(callback_data.name);
    SDL_free(callback_data.value);
    SDL_free(callback_data.oldValue);

    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE), using callback after reset");
    SDL_zero(callback_data);
    result = SDL_SetHintWithPriority(testHint, "temp", SDL_HINT_OVERRIDE);
    SDLTest_AssertCheck(result, "SDL_SetHintWithPriority(\"%s\", \"temp\", SDL_HINT_OVERRIDE) should return true", testHint);
    SDLTest_AssertCheck(
        callback_data.value && SDL_strcmp(callback_data.value, "temp") == 0,
        "callbackValue = %s, expected \"temp\"",
        callback_data.value);
    SDL_free(callback_data.name);
    SDL_free(callback_data.value);
    SDL_free(callback_data.oldValue);

    SDLTest_AssertPass("Call to SDL_ResetHint(), after clearing callback");
    SDL_zero(callback_data);
    SDL_DelHintCallback(testHint, hints_testHintChanged, &callback_data);
    SDL_ResetHint(testHint);
    SDLTest_AssertCheck(
        !callback_data.value,
        "callbackValue = %s, expected \"(null)\"",
        callback_data.value);
    SDL_free(callback_data.name);
    SDL_free(callback_data.value);
    SDL_free(callback_data.oldValue);

    /* Make sure callback functionality work with hint renamed in sdl3 */
    SDLTest_AssertPass("Call to SDL_AddHintCallback()");
    SDL_AddHintCallback(SDL_HINT_ALLOW_TOPMOST, hints_testHintChanged, &callback_data);
    SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE), using callback");
    SDL_zero(callback_data);
    result = SDL_SetHintWithPriority(SDL_HINT_ALLOW_TOPMOST, "temp", SDL_HINT_OVERRIDE);
    SDLTest_AssertCheck(result, "SDL_SetHintWithPriority(\"%s\", \"temp\", SDL_HINT_OVERRIDE) should return true", testHint);
    SDLTest_AssertCheck(
        callback_data.name && SDL_strcmp(callback_data.name, SDL_HINT_ALLOW_TOPMOST) == 0,
        "callback_data.name = \"%s\", expected \"%s\"",
        callback_data.name, SDL_HINT_ALLOW_TOPMOST);
    SDLTest_AssertCheck(
        callback_data.value && SDL_strcmp(callback_data.value, "temp") == 0,
        "callback_data.value = \"%s\", expected \"%s\"",
        callback_data.value, "temp");
    SDL_free(callback_data.name);
    SDL_free(callback_data.value);
    SDL_free(callback_data.oldValue);
    SDL_ResetHint(testHint);

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
