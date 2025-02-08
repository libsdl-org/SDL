/**
 * Keyboard test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Test case functions */

/**
 * Check call to SDL_GetKeyboardState with and without numkeys reference.
 *
 * \sa SDL_GetKeyboardState
 */
static int SDLCALL keyboard_getKeyboardState(void *arg)
{
    int numkeys;
    const bool *state;

    /* Case where numkeys pointer is NULL */
    state = SDL_GetKeyboardState(NULL);
    SDLTest_AssertPass("Call to SDL_GetKeyboardState(NULL)");
    SDLTest_AssertCheck(state != NULL, "Validate that return value from SDL_GetKeyboardState is not NULL");

    /* Case where numkeys pointer is not NULL */
    numkeys = -1;
    state = SDL_GetKeyboardState(&numkeys);
    SDLTest_AssertPass("Call to SDL_GetKeyboardState(&numkeys)");
    SDLTest_AssertCheck(state != NULL, "Validate that return value from SDL_GetKeyboardState is not NULL");
    SDLTest_AssertCheck(numkeys >= 0, "Validate that value of numkeys is >= 0, got: %d", numkeys);

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetKeyboardFocus
 *
 * \sa SDL_GetKeyboardFocus
 */
static int SDLCALL keyboard_getKeyboardFocus(void *arg)
{
    /* Call, but ignore return value */
    SDL_GetKeyboardFocus();
    SDLTest_AssertPass("Call to SDL_GetKeyboardFocus()");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetKeyFromName for known, unknown and invalid name.
 *
 * \sa SDL_GetKeyFromName
 */
static int SDLCALL keyboard_getKeyFromName(void *arg)
{
    SDL_Keycode result;

    /* Case where Key is known, 1 character input */
    result = SDL_GetKeyFromName("A");
    SDLTest_AssertPass("Call to SDL_GetKeyFromName('A', true)");
    SDLTest_AssertCheck(result == SDLK_A, "Verify result from call, expected: %d, got: %" SDL_PRIu32, SDLK_A, result);

    /* Case where Key is known, 2 character input */
    result = SDL_GetKeyFromName("F1");
    SDLTest_AssertPass("Call to SDL_GetKeyFromName(known/double)");
    SDLTest_AssertCheck(result == SDLK_F1, "Verify result from call, expected: %d, got: %" SDL_PRIu32, SDLK_F1, result);

    /* Case where Key is known, 3 character input */
    result = SDL_GetKeyFromName("End");
    SDLTest_AssertPass("Call to SDL_GetKeyFromName(known/triple)");
    SDLTest_AssertCheck(result == SDLK_END, "Verify result from call, expected: %d, got: %" SDL_PRIu32, SDLK_END, result);

    /* Case where Key is known, 4 character input */
    result = SDL_GetKeyFromName("Find");
    SDLTest_AssertPass("Call to SDL_GetKeyFromName(known/quad)");
    SDLTest_AssertCheck(result == SDLK_FIND, "Verify result from call, expected: %d, got: %" SDL_PRIu32, SDLK_FIND, result);

    /* Case where Key is known, multiple character input */
    result = SDL_GetKeyFromName("MediaStop");
    SDLTest_AssertPass("Call to SDL_GetKeyFromName(known/multi)");
    SDLTest_AssertCheck(result == SDLK_MEDIA_STOP, "Verify result from call, expected: %d, got: %" SDL_PRIu32, SDLK_MEDIA_STOP, result);

    /* Case where Key is unknown */
    result = SDL_GetKeyFromName("NotThere");
    SDLTest_AssertPass("Call to SDL_GetKeyFromName(unknown)");
    SDLTest_AssertCheck(result == SDLK_UNKNOWN, "Verify result from call is UNKNOWN, expected: %d, got: %" SDL_PRIu32, SDLK_UNKNOWN, result);

    /* Case where input is NULL/invalid */
    result = SDL_GetKeyFromName(NULL);
    SDLTest_AssertPass("Call to SDL_GetKeyFromName(NULL)");
    SDLTest_AssertCheck(result == SDLK_UNKNOWN, "Verify result from call is UNKNOWN, expected: %d, got: %" SDL_PRIu32, SDLK_UNKNOWN, result);

    return TEST_COMPLETED;
}

/*
 * Local helper to check for the invalid scancode error message
 */
static void checkInvalidScancodeError(void)
{
    const char *expectedError = "Parameter 'scancode' is invalid";
    const char *error;
    error = SDL_GetError();
    SDLTest_AssertPass("Call to SDL_GetError()");
    SDLTest_AssertCheck(error != NULL, "Validate that error message was not NULL");
    if (error != NULL) {
        SDLTest_AssertCheck(SDL_strcmp(error, expectedError) == 0,
                            "Validate error message, expected: '%s', got: '%s'", expectedError, error);
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
    }
}

/**
 * Check call to SDL_GetKeyFromScancode
 *
 * \sa SDL_GetKeyFromScancode
 */
static int SDLCALL keyboard_getKeyFromScancode(void *arg)
{
    SDL_Keycode result;

    /* Case where input is valid */
    result = SDL_GetKeyFromScancode(SDL_SCANCODE_SPACE, SDL_KMOD_NONE, false);
    SDLTest_AssertPass("Call to SDL_GetKeyFromScancode(valid)");
    SDLTest_AssertCheck(result == SDLK_SPACE, "Verify result from call, expected: %d, got: %" SDL_PRIu32, SDLK_SPACE, result);

    /* Case where input is zero */
    result = SDL_GetKeyFromScancode(SDL_SCANCODE_UNKNOWN, SDL_KMOD_NONE, false);
    SDLTest_AssertPass("Call to SDL_GetKeyFromScancode(0)");
    SDLTest_AssertCheck(result == SDLK_UNKNOWN, "Verify result from call is UNKNOWN, expected: %d, got: %" SDL_PRIu32, SDLK_UNKNOWN, result);

    /* Clear error message */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    /* Case where input is invalid (too small) */
    result = SDL_GetKeyFromScancode((SDL_Scancode)-999, SDL_KMOD_NONE, false);
    SDLTest_AssertPass("Call to SDL_GetKeyFromScancode(-999)");
    SDLTest_AssertCheck(result == SDLK_UNKNOWN, "Verify result from call is UNKNOWN, expected: %d, got: %" SDL_PRIu32, SDLK_UNKNOWN, result);
    checkInvalidScancodeError();

    /* Case where input is invalid (too big) */
    result = SDL_GetKeyFromScancode((SDL_Scancode)999, SDL_KMOD_NONE, false);
    SDLTest_AssertPass("Call to SDL_GetKeyFromScancode(999)");
    SDLTest_AssertCheck(result == SDLK_UNKNOWN, "Verify result from call is UNKNOWN, expected: %d, got: %" SDL_PRIu32, SDLK_UNKNOWN, result);
    checkInvalidScancodeError();

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetKeyName
 *
 * \sa SDL_GetKeyName
 */
static int SDLCALL keyboard_getKeyName(void *arg)
{
    const char *result;
    const char *expected;

    /* Case where key has a 1 character name */
    expected = "3";
    result = SDL_GetKeyName(SDLK_3);
    SDLTest_AssertPass("Call to SDL_GetKeyName()");
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: %s, got: %s", expected, result);

    /* Case where key has a 2 character name */
    expected = "F1";
    result = SDL_GetKeyName(SDLK_F1);
    SDLTest_AssertPass("Call to SDL_GetKeyName()");
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: %s, got: %s", expected, result);

    /* Case where key has a 3 character name */
    expected = "Cut";
    result = SDL_GetKeyName(SDLK_CUT);
    SDLTest_AssertPass("Call to SDL_GetKeyName()");
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: %s, got: %s", expected, result);

    /* Case where key has a 4 character name */
    expected = "Down";
    result = SDL_GetKeyName(SDLK_DOWN);
    SDLTest_AssertPass("Call to SDL_GetKeyName()");
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: %s, got: %s", expected, result);

    /* Case where key has a N character name */
    expected = "MediaPlay";
    result = SDL_GetKeyName(SDLK_MEDIA_PLAY);
    SDLTest_AssertPass("Call to SDL_GetKeyName()");
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: %s, got: %s", expected, result);

    /* Case where key has a N character name with space */
    expected = "Keypad MemStore";
    result = SDL_GetKeyName(SDLK_KP_MEMSTORE);
    SDLTest_AssertPass("Call to SDL_GetKeyName()");
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: %s, got: %s", expected, result);

    return TEST_COMPLETED;
}

/**
 * SDL_GetScancodeName negative cases
 *
 * \sa SDL_GetScancodeName
 */
static int SDLCALL keyboard_getScancodeNameNegative(void *arg)
{
    SDL_Scancode scancode;
    const char *result;
    const char *expected = "";

    /* Clear error message */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    /* Out-of-bounds scancode */
    scancode = (SDL_Scancode)SDL_SCANCODE_COUNT;
    result = SDL_GetScancodeName(scancode);
    SDLTest_AssertPass("Call to SDL_GetScancodeName(%d/large)", scancode);
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: '%s', got: '%s'", expected, result);
    checkInvalidScancodeError();

    return TEST_COMPLETED;
}

/**
 * SDL_GetKeyName negative cases
 *
 * \sa SDL_GetKeyName
 */
static int SDLCALL keyboard_getKeyNameNegative(void *arg)
{
    SDL_Keycode keycode;
    const char *result;
    const char *expected = "";

    /* Unknown keycode */
    keycode = SDLK_UNKNOWN;
    result = SDL_GetKeyName(keycode);
    SDLTest_AssertPass("Call to SDL_GetKeyName(%" SDL_PRIu32 "/unknown)", keycode);
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: '%s', got: '%s'", expected, result);

    /* Clear error message */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    /* Negative keycode */
    keycode = (SDL_Keycode)SDLTest_RandomIntegerInRange(-255, -1);
    result = SDL_GetKeyName(keycode);
    SDLTest_AssertPass("Call to SDL_GetKeyName(%" SDL_PRIu32 "/negative)", keycode);
    SDLTest_AssertCheck(result != NULL, "Verify result from call is not NULL");
    SDLTest_AssertCheck(SDL_strcmp(result, expected) == 0, "Verify result from call is valid, expected: '%s', got: '%s'", expected, result);
    checkInvalidScancodeError();

    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetModState and SDL_SetModState
 *
 * \sa SDL_GetModState
 * \sa SDL_SetModState
 */
static int SDLCALL keyboard_getSetModState(void *arg)
{
    SDL_Keymod result;
    SDL_Keymod currentState;
    SDL_Keymod newState;
    SDL_Keymod allStates =
        SDL_KMOD_NONE |
        SDL_KMOD_LSHIFT |
        SDL_KMOD_RSHIFT |
        SDL_KMOD_LCTRL |
        SDL_KMOD_RCTRL |
        SDL_KMOD_LALT |
        SDL_KMOD_RALT |
        SDL_KMOD_LGUI |
        SDL_KMOD_RGUI |
        SDL_KMOD_NUM |
        SDL_KMOD_CAPS |
        SDL_KMOD_MODE |
        SDL_KMOD_SCROLL;

    /* Get state, cache for later reset */
    result = SDL_GetModState();
    SDLTest_AssertPass("Call to SDL_GetModState()");
    SDLTest_AssertCheck(/*result >= 0 &&*/ result <= allStates, "Verify result from call is valid, expected: 0 <= result <= 0x%.4x, got: 0x%.4x", allStates, result);
    currentState = result;

    /* Set random state */
    newState = (SDL_Keymod)SDLTest_RandomIntegerInRange(0, allStates);
    SDL_SetModState(newState);
    SDLTest_AssertPass("Call to SDL_SetModState(0x%.4x)", newState);
    result = SDL_GetModState();
    SDLTest_AssertPass("Call to SDL_GetModState()");
    SDLTest_AssertCheck(result == newState, "Verify result from call is valid, expected: 0x%.4x, got: 0x%.4x", newState, result);

    /* Set zero state */
    SDL_SetModState(0);
    SDLTest_AssertPass("Call to SDL_SetModState(0)");
    result = SDL_GetModState();
    SDLTest_AssertPass("Call to SDL_GetModState()");
    SDLTest_AssertCheck(result == 0, "Verify result from call is valid, expected: 0, got: 0x%.4x", result);

    /* Revert back to cached current state if needed */
    if (currentState != 0) {
        SDL_SetModState(currentState);
        SDLTest_AssertPass("Call to SDL_SetModState(0x%.4x)", currentState);
        result = SDL_GetModState();
        SDLTest_AssertPass("Call to SDL_GetModState()");
        SDLTest_AssertCheck(result == currentState, "Verify result from call is valid, expected: 0x%.4x, got: 0x%.4x", currentState, result);
    }

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_StartTextInput and SDL_StopTextInput
 *
 * \sa SDL_StartTextInput
 * \sa SDL_StopTextInput
 */
static int SDLCALL keyboard_startStopTextInput(void *arg)
{
    SDL_Window *window = SDL_GetKeyboardFocus();

    /* Start-Stop */
    SDL_StartTextInput(window);
    SDLTest_AssertPass("Call to SDL_StartTextInput()");
    SDL_StopTextInput(window);
    SDLTest_AssertPass("Call to SDL_StopTextInput()");

    /* Stop-Start */
    SDL_StartTextInput(window);
    SDLTest_AssertPass("Call to SDL_StartTextInput()");

    /* Start-Start */
    SDL_StartTextInput(window);
    SDLTest_AssertPass("Call to SDL_StartTextInput()");

    /* Stop-Stop */
    SDL_StopTextInput(window);
    SDLTest_AssertPass("Call to SDL_StopTextInput()");
    SDL_StopTextInput(window);
    SDLTest_AssertPass("Call to SDL_StopTextInput()");

    return TEST_COMPLETED;
}

/* Internal function to test SDL_SetTextInputArea */
static void testSetTextInputArea(SDL_Window *window, SDL_Rect refRect)
{
    SDL_Rect testRect;

    testRect = refRect;
    SDL_SetTextInputArea(window, &testRect, 0);
    SDLTest_AssertPass("Call to SDL_SetTextInputArea with refRect(x:%d,y:%d,w:%d,h:%d)", refRect.x, refRect.y, refRect.w, refRect.h);
    SDLTest_AssertCheck(
        (refRect.x == testRect.x) && (refRect.y == testRect.y) && (refRect.w == testRect.w) && (refRect.h == testRect.h),
        "Check that input data was not modified, expected: x:%d,y:%d,w:%d,h:%d, got: x:%d,y:%d,w:%d,h:%d",
        refRect.x, refRect.y, refRect.w, refRect.h,
        testRect.x, testRect.y, testRect.w, testRect.h);
}

/**
 * Check call to SDL_SetTextInputArea
 *
 * \sa SDL_SetTextInputArea
 */
static int SDLCALL keyboard_setTextInputArea(void *arg)
{
    SDL_Window *window = SDL_GetKeyboardFocus();
    SDL_Rect refRect;

    /* Normal visible refRect, origin inside */
    refRect.x = SDLTest_RandomIntegerInRange(1, 50);
    refRect.y = SDLTest_RandomIntegerInRange(1, 50);
    refRect.w = SDLTest_RandomIntegerInRange(10, 50);
    refRect.h = SDLTest_RandomIntegerInRange(10, 50);
    testSetTextInputArea(window, refRect);

    /* Normal visible refRect, origin 0,0 */
    refRect.x = 0;
    refRect.y = 0;
    refRect.w = SDLTest_RandomIntegerInRange(10, 50);
    refRect.h = SDLTest_RandomIntegerInRange(10, 50);
    testSetTextInputArea(window, refRect);

    /* 1Pixel refRect */
    refRect.x = SDLTest_RandomIntegerInRange(10, 50);
    refRect.y = SDLTest_RandomIntegerInRange(10, 50);
    refRect.w = 1;
    refRect.h = 1;
    testSetTextInputArea(window, refRect);

    /* 0pixel refRect */
    refRect.x = 1;
    refRect.y = 1;
    refRect.w = 1;
    refRect.h = 0;
    testSetTextInputArea(window, refRect);

    /* 0pixel refRect */
    refRect.x = 1;
    refRect.y = 1;
    refRect.w = 0;
    refRect.h = 1;
    testSetTextInputArea(window, refRect);

    /* 0pixel refRect */
    refRect.x = 1;
    refRect.y = 1;
    refRect.w = 0;
    refRect.h = 0;
    testSetTextInputArea(window, refRect);

    /* 0pixel refRect */
    refRect.x = 0;
    refRect.y = 0;
    refRect.w = 0;
    refRect.h = 0;
    testSetTextInputArea(window, refRect);

    /* negative refRect */
    refRect.x = SDLTest_RandomIntegerInRange(-200, -100);
    refRect.y = SDLTest_RandomIntegerInRange(-200, -100);
    refRect.w = 50;
    refRect.h = 50;
    testSetTextInputArea(window, refRect);

    /* oversized refRect */
    refRect.x = SDLTest_RandomIntegerInRange(1, 50);
    refRect.y = SDLTest_RandomIntegerInRange(1, 50);
    refRect.w = 5000;
    refRect.h = 5000;
    testSetTextInputArea(window, refRect);

    /* NULL refRect */
    SDL_SetTextInputArea(window, NULL, 0);
    SDLTest_AssertPass("Call to SDL_SetTextInputArea(NULL)");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_SetTextInputArea with invalid data
 *
 * \sa SDL_SetTextInputArea
 */
static int SDLCALL keyboard_setTextInputAreaNegative(void *arg)
{
    /* Some platforms set also an error message; prepare for checking it */
#if defined(SDL_VIDEO_DRIVER_WINDOWS) || defined(SDL_VIDEO_DRIVER_ANDROID) || defined(SDL_VIDEO_DRIVER_COCOA)
    const char *expectedError = "Parameter 'rect' is invalid";
    const char *error;

    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
#endif

    /* NULL refRect */
    SDL_SetTextInputArea(SDL_GetKeyboardFocus(), NULL, 0);
    SDLTest_AssertPass("Call to SDL_SetTextInputArea(NULL)");

    /* Some platforms set also an error message; so check it */
#if defined(SDL_VIDEO_DRIVER_WINDOWS) || defined(SDL_VIDEO_DRIVER_ANDROID) || defined(SDL_VIDEO_DRIVER_COCOA)
    error = SDL_GetError();
    SDLTest_AssertPass("Call to SDL_GetError()");
    SDLTest_AssertCheck(error != NULL, "Validate that error message was not NULL");
    if (error != NULL) {
        SDLTest_AssertCheck(SDL_strcmp(error, expectedError) == 0,
                            "Validate error message, expected: '%s', got: '%s'", expectedError, error);
    }

    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
#endif

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetScancodeFromName
 *
 * \sa SDL_GetScancodeFromName
 * \sa SDL_Keycode
 */
static int SDLCALL keyboard_getScancodeFromName(void *arg)
{
    SDL_Scancode scancode;

    /* Regular key, 1 character, first name in list */
    scancode = SDL_GetScancodeFromName("A");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('A')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_A, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_A, scancode);

    /* Regular key, 1 character */
    scancode = SDL_GetScancodeFromName("4");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('4')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_4, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_4, scancode);

    /* Regular key, 2 characters */
    scancode = SDL_GetScancodeFromName("F1");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('F1')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_F1, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_F1, scancode);

    /* Regular key, 3 characters */
    scancode = SDL_GetScancodeFromName("End");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('End')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_END, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_END, scancode);

    /* Regular key, 4 characters */
    scancode = SDL_GetScancodeFromName("Find");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('Find')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_FIND, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_FIND, scancode);

    /* Regular key, several characters */
    scancode = SDL_GetScancodeFromName("Backspace");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('Backspace')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_BACKSPACE, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_BACKSPACE, scancode);

    /* Regular key, several characters with space */
    scancode = SDL_GetScancodeFromName("Keypad Enter");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('Keypad Enter')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_KP_ENTER, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_KP_ENTER, scancode);

    /* Regular key, last name in list */
    scancode = SDL_GetScancodeFromName("Sleep");
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('Sleep')");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_SLEEP, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_SLEEP, scancode);

    return TEST_COMPLETED;
}

/*
 * Local helper to check for the invalid scancode error message
 */
static void checkInvalidNameError(void)
{
    const char *expectedError = "Parameter 'name' is invalid";
    const char *error;
    error = SDL_GetError();
    SDLTest_AssertPass("Call to SDL_GetError()");
    SDLTest_AssertCheck(error != NULL, "Validate that error message was not NULL");
    if (error != NULL) {
        SDLTest_AssertCheck(SDL_strcmp(error, expectedError) == 0,
                            "Validate error message, expected: '%s', got: '%s'", expectedError, error);
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
    }
}

/**
 * Check call to SDL_GetScancodeFromName with invalid data
 *
 * \sa SDL_GetScancodeFromName
 * \sa SDL_Keycode
 */
static int SDLCALL keyboard_getScancodeFromNameNegative(void *arg)
{
    char *name;
    SDL_Scancode scancode;

    /* Clear error message */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    /* Random string input */
    name = SDLTest_RandomAsciiStringOfSize(32);
    SDLTest_Assert(name != NULL, "Check that random name is not NULL");
    if (name == NULL) {
        return TEST_ABORTED;
    }
    scancode = SDL_GetScancodeFromName(name);
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName('%s')", name);
    SDL_free(name);
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_UNKNOWN, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_UNKNOWN, scancode);
    checkInvalidNameError();

    /* Zero length string input */
    name = "";
    scancode = SDL_GetScancodeFromName(name);
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName(NULL)");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_UNKNOWN, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_UNKNOWN, scancode);
    checkInvalidNameError();

    /* NULL input */
    name = NULL;
    scancode = SDL_GetScancodeFromName(name);
    SDLTest_AssertPass("Call to SDL_GetScancodeFromName(NULL)");
    SDLTest_AssertCheck(scancode == SDL_SCANCODE_UNKNOWN, "Validate return value from SDL_GetScancodeFromName, expected: %d, got: %d", SDL_SCANCODE_UNKNOWN, scancode);
    checkInvalidNameError();

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Keyboard test cases */
static const SDLTest_TestCaseReference keyboardTestGetKeyboardState = {
    keyboard_getKeyboardState, "keyboard_getKeyboardState", "Check call to SDL_GetKeyboardState with and without numkeys reference", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetKeyboardFocus = {
    keyboard_getKeyboardFocus, "keyboard_getKeyboardFocus", "Check call to SDL_GetKeyboardFocus", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetKeyFromName = {
    keyboard_getKeyFromName, "keyboard_getKeyFromName", "Check call to SDL_GetKeyFromName for known, unknown and invalid name", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetKeyFromScancode = {
    keyboard_getKeyFromScancode, "keyboard_getKeyFromScancode", "Check call to SDL_GetKeyFromScancode", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetKeyName = {
    keyboard_getKeyName, "keyboard_getKeyName", "Check call to SDL_GetKeyName", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetSetModState = {
    keyboard_getSetModState, "keyboard_getSetModState", "Check call to SDL_GetModState and SDL_SetModState", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestStartStopTextInput = {
    keyboard_startStopTextInput, "keyboard_startStopTextInput", "Check call to SDL_StartTextInput and SDL_StopTextInput", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestSetTextInputArea = {
    keyboard_setTextInputArea, "keyboard_setTextInputArea", "Check call to SDL_SetTextInputArea", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestSetTextInputAreaNegative = {
    keyboard_setTextInputAreaNegative, "keyboard_setTextInputAreaNegative", "Check call to SDL_SetTextInputArea with invalid data", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetScancodeFromName = {
    keyboard_getScancodeFromName, "keyboard_getScancodeFromName", "Check call to SDL_GetScancodeFromName", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetScancodeFromNameNegative = {
    keyboard_getScancodeFromNameNegative, "keyboard_getScancodeFromNameNegative", "Check call to SDL_GetScancodeFromName with invalid data", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetKeyNameNegative = {
    keyboard_getKeyNameNegative, "keyboard_getKeyNameNegative", "Check call to SDL_GetKeyName with invalid data", TEST_ENABLED
};

static const SDLTest_TestCaseReference keyboardTestGetScancodeNameNegative = {
    keyboard_getScancodeNameNegative, "keyboard_getScancodeNameNegative", "Check call to SDL_GetScancodeName with invalid data", TEST_ENABLED
};

/* Sequence of Keyboard test cases */
static const SDLTest_TestCaseReference *keyboardTests[] = {
    &keyboardTestGetKeyboardState,
    &keyboardTestGetKeyboardFocus,
    &keyboardTestGetKeyFromName,
    &keyboardTestGetKeyFromScancode,
    &keyboardTestGetKeyName,
    &keyboardTestGetSetModState,
    &keyboardTestStartStopTextInput,
    &keyboardTestSetTextInputArea,
    &keyboardTestSetTextInputAreaNegative,
    &keyboardTestGetScancodeFromName,
    &keyboardTestGetScancodeFromNameNegative,
    &keyboardTestGetKeyNameNegative,
    &keyboardTestGetScancodeNameNegative,
    NULL
};

/* Keyboard test suite (global) */
SDLTest_TestSuiteReference keyboardTestSuite = {
    "Keyboard",
    NULL,
    keyboardTests,
    NULL
};
