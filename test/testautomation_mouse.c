/**
 * Mouse test suite
 */
#include <limits.h>
#include <float.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"
#include "testautomation_images.h"

/* ================= Test Case Implementation ================== */

/* Test case functions */

/* Helper to evaluate state returned from SDL_GetMouseState */
static int mouseStateCheck(Uint32 state)
{
    return (state == 0) ||
           (state == SDL_BUTTON(SDL_BUTTON_LEFT)) ||
           (state == SDL_BUTTON(SDL_BUTTON_MIDDLE)) ||
           (state == SDL_BUTTON(SDL_BUTTON_RIGHT)) ||
           (state == SDL_BUTTON(SDL_BUTTON_X1)) ||
           (state == SDL_BUTTON(SDL_BUTTON_X2));
}

/**
 * Check call to SDL_GetMouseState
 *
 */
static int mouse_getMouseState(void *arg)
{
    float x;
    float y;
    SDL_MouseButtonFlags state;

    /* Pump some events to update mouse state */
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");

    /* Case where x, y pointer is NULL */
    state = SDL_GetMouseState(NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetMouseState(NULL, NULL)");
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    /* Case where x pointer is not NULL */
    x = -FLT_MAX;
    state = SDL_GetMouseState(&x, NULL);
    SDLTest_AssertPass("Call to SDL_GetMouseState(&x, NULL)");
    SDLTest_AssertCheck(x > -FLT_MAX, "Validate that value of x is > -FLT_MAX, got: %g", x);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    /* Case where y pointer is not NULL */
    y = -FLT_MAX;
    state = SDL_GetMouseState(NULL, &y);
    SDLTest_AssertPass("Call to SDL_GetMouseState(NULL, &y)");
    SDLTest_AssertCheck(y > -FLT_MAX, "Validate that value of y is > -FLT_MAX, got: %g", y);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    /* Case where x and y pointer is not NULL */
    x = -FLT_MAX;
    y = -FLT_MAX;
    state = SDL_GetMouseState(&x, &y);
    SDLTest_AssertPass("Call to SDL_GetMouseState(&x, &y)");
    SDLTest_AssertCheck(x > -FLT_MAX, "Validate that value of x is > -FLT_MAX, got: %g", x);
    SDLTest_AssertCheck(y > -FLT_MAX, "Validate that value of y is > -FLT_MAX, got: %g", y);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetRelativeMouseState
 *
 */
static int mouse_getRelativeMouseState(void *arg)
{
    float x;
    float y;
    SDL_MouseButtonFlags state;

    /* Pump some events to update mouse state */
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");

    /* Case where x, y pointer is NULL */
    state = SDL_GetRelativeMouseState(NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetRelativeMouseState(NULL, NULL)");
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    /* Case where x pointer is not NULL */
    x = -FLT_MAX;
    state = SDL_GetRelativeMouseState(&x, NULL);
    SDLTest_AssertPass("Call to SDL_GetRelativeMouseState(&x, NULL)");
    SDLTest_AssertCheck(x > -FLT_MAX, "Validate that value of x is > -FLT_MAX, got: %g", x);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    /* Case where y pointer is not NULL */
    y = -FLT_MAX;
    state = SDL_GetRelativeMouseState(NULL, &y);
    SDLTest_AssertPass("Call to SDL_GetRelativeMouseState(NULL, &y)");
    SDLTest_AssertCheck(y > -FLT_MAX, "Validate that value of y is > -FLT_MAX, got: %g", y);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    /* Case where x and y pointer is not NULL */
    x = -FLT_MAX;
    y = -FLT_MAX;
    state = SDL_GetRelativeMouseState(&x, &y);
    SDLTest_AssertPass("Call to SDL_GetRelativeMouseState(&x, &y)");
    SDLTest_AssertCheck(x > -FLT_MAX, "Validate that value of x is > -FLT_MAX, got: %g", x);
    SDLTest_AssertCheck(y > -FLT_MAX, "Validate that value of y is > -FLT_MAX, got: %g", y);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    return TEST_COMPLETED;
}

/* XPM definition of mouse Cursor */
static const char *g_mouseArrowData[] = {
    /* pixels */
    "X                               ",
    "XX                              ",
    "X.X                             ",
    "X..X                            ",
    "X...X                           ",
    "X....X                          ",
    "X.....X                         ",
    "X......X                        ",
    "X.......X                       ",
    "X........X                      ",
    "X.....XXXXX                     ",
    "X..X..X                         ",
    "X.X X..X                        ",
    "XX  X..X                        ",
    "X    X..X                       ",
    "     X..X                       ",
    "      X..X                      ",
    "      X..X                      ",
    "       XX                       ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                "
};

/* Helper that creates a new mouse cursor from an XPM */
static SDL_Cursor *initArrowCursor(const char *image[])
{
    SDL_Cursor *cursor;
    int i, row, col;
    Uint8 data[4 * 32];
    Uint8 mask[4 * 32];

    i = -1;
    for (row = 0; row < 32; ++row) {
        for (col = 0; col < 32; ++col) {
            if (col % 8) {
                data[i] <<= 1;
                mask[i] <<= 1;
            } else {
                ++i;
                data[i] = mask[i] = 0;
            }
            switch (image[row][col]) {
            case 'X':
                data[i] |= 0x01;
                mask[i] |= 0x01;
                break;
            case '.':
                mask[i] |= 0x01;
                break;
            case ' ':
                break;
            }
        }
    }

    cursor = SDL_CreateCursor(data, mask, 32, 32, 0, 0);
    return cursor;
}

/**
 * Check call to SDL_CreateCursor and SDL_DestroyCursor
 *
 * \sa SDL_CreateCursor
 * \sa SDL_DestroyCursor
 */
static int mouse_createFreeCursor(void *arg)
{
    SDL_Cursor *cursor;

    /* Create a cursor */
    cursor = initArrowCursor(g_mouseArrowData);
    SDLTest_AssertPass("Call to SDL_CreateCursor()");
    SDLTest_AssertCheck(cursor != NULL, "Validate result from SDL_CreateCursor() is not NULL");
    if (cursor == NULL) {
        return TEST_ABORTED;
    }

    /* Free cursor again */
    SDL_DestroyCursor(cursor);
    SDLTest_AssertPass("Call to SDL_DestroyCursor()");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_CreateColorCursor and SDL_DestroyCursor
 *
 * \sa SDL_CreateColorCursor
 * \sa SDL_DestroyCursor
 */
static int mouse_createFreeColorCursor(void *arg)
{
    SDL_Surface *face;
    SDL_Cursor *cursor;

    /* Get sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Validate sample input image is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Create a color cursor from surface */
    cursor = SDL_CreateColorCursor(face, 0, 0);
    SDLTest_AssertPass("Call to SDL_CreateColorCursor()");
    SDLTest_AssertCheck(cursor != NULL, "Validate result from SDL_CreateColorCursor() is not NULL");
    if (cursor == NULL) {
        SDL_DestroySurface(face);
        return TEST_ABORTED;
    }

    /* Free cursor again */
    SDL_DestroyCursor(cursor);
    SDLTest_AssertPass("Call to SDL_DestroyCursor()");

    /* Clean up */
    SDL_DestroySurface(face);

    return TEST_COMPLETED;
}

/* Helper that changes cursor visibility */
static void changeCursorVisibility(SDL_bool state)
{
    SDL_bool newState;

    if (state) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
    SDLTest_AssertPass("Call to %s", state ? "SDL_ShowCursor()" : "SDL_HideCursor()");

    newState = SDL_CursorVisible();
    SDLTest_AssertPass("Call to SDL_CursorVisible()");
    SDLTest_AssertCheck(state == newState, "Validate new state, expected: %s, got: %s",
                        state ? "SDL_TRUE" : "SDL_FALSE",
                        newState ? "SDL_TRUE" : "SDL_FALSE");
}

/**
 * Check call to SDL_ShowCursor
 *
 * \sa SDL_ShowCursor
 */
static int mouse_showCursor(void *arg)
{
    SDL_bool currentState;

    /* Get current state */
    currentState = SDL_CursorVisible();
    SDLTest_AssertPass("Call to SDL_CursorVisible()");
    if (currentState) {
        /* Hide the cursor, then show it again */
        changeCursorVisibility(SDL_FALSE);
        changeCursorVisibility(SDL_TRUE);
    } else {
        /* Show the cursor, then hide it again */
        changeCursorVisibility(SDL_TRUE);
        changeCursorVisibility(SDL_FALSE);
    }

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_SetCursor
 *
 * \sa SDL_SetCursor
 */
static int mouse_setCursor(void *arg)
{
    SDL_Cursor *cursor;

    /* Create a cursor */
    cursor = initArrowCursor(g_mouseArrowData);
    SDLTest_AssertPass("Call to SDL_CreateCursor()");
    SDLTest_AssertCheck(cursor != NULL, "Validate result from SDL_CreateCursor() is not NULL");
    if (cursor == NULL) {
        return TEST_ABORTED;
    }

    /* Set the arrow cursor */
    SDL_SetCursor(cursor);
    SDLTest_AssertPass("Call to SDL_SetCursor(cursor)");

    /* Force redraw */
    SDL_SetCursor(NULL);
    SDLTest_AssertPass("Call to SDL_SetCursor(NULL)");

    /* Free cursor again */
    SDL_DestroyCursor(cursor);
    SDLTest_AssertPass("Call to SDL_DestroyCursor()");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetCursor
 *
 * \sa SDL_GetCursor
 */
static int mouse_getCursor(void *arg)
{
    SDL_Cursor *cursor;

    /* Get current cursor */
    cursor = SDL_GetCursor();
    SDLTest_AssertPass("Call to SDL_GetCursor()");
    SDLTest_AssertCheck(cursor != NULL, "Validate result from SDL_GetCursor() is not NULL");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetRelativeMouseMode and SDL_SetRelativeMouseMode
 *
 * \sa SDL_GetRelativeMouseMode
 * \sa SDL_SetRelativeMouseMode
 */
static int mouse_getSetRelativeMouseMode(void *arg)
{
    int result;
    int i;
    SDL_bool initialState;
    SDL_bool currentState;

    /* Capture original state so we can revert back to it later */
    initialState = SDL_GetRelativeMouseMode();
    SDLTest_AssertPass("Call to SDL_GetRelativeMouseMode()");

    /* Repeat twice to check D->D transition */
    for (i = 0; i < 2; i++) {
        /* Disable - should always be supported */
        result = SDL_SetRelativeMouseMode(SDL_FALSE);
        SDLTest_AssertPass("Call to SDL_SetRelativeMouseMode(FALSE)");
        SDLTest_AssertCheck(result == 0, "Validate result value from SDL_SetRelativeMouseMode, expected: 0, got: %i", result);
        currentState = SDL_GetRelativeMouseMode();
        SDLTest_AssertPass("Call to SDL_GetRelativeMouseMode()");
        SDLTest_AssertCheck(currentState == SDL_FALSE, "Validate current state is FALSE, got: %i", currentState);
    }

    /* Repeat twice to check D->E->E transition */
    for (i = 0; i < 2; i++) {
        /* Enable - may not be supported */
        result = SDL_SetRelativeMouseMode(SDL_TRUE);
        SDLTest_AssertPass("Call to SDL_SetRelativeMouseMode(TRUE)");
        if (result != -1) {
            SDLTest_AssertCheck(result == 0, "Validate result value from SDL_SetRelativeMouseMode, expected: 0, got: %i", result);
            currentState = SDL_GetRelativeMouseMode();
            SDLTest_AssertPass("Call to SDL_GetRelativeMouseMode()");
            SDLTest_AssertCheck(currentState == SDL_TRUE, "Validate current state is TRUE, got: %i", currentState);
        }
    }

    /* Disable to check E->D transition */
    result = SDL_SetRelativeMouseMode(SDL_FALSE);
    SDLTest_AssertPass("Call to SDL_SetRelativeMouseMode(FALSE)");
    SDLTest_AssertCheck(result == 0, "Validate result value from SDL_SetRelativeMouseMode, expected: 0, got: %i", result);
    currentState = SDL_GetRelativeMouseMode();
    SDLTest_AssertPass("Call to SDL_GetRelativeMouseMode()");
    SDLTest_AssertCheck(currentState == SDL_FALSE, "Validate current state is FALSE, got: %i", currentState);

    /* Revert to original state - ignore result */
    result = SDL_SetRelativeMouseMode(initialState);

    return TEST_COMPLETED;
}

#define MOUSE_TESTWINDOW_WIDTH  320
#define MOUSE_TESTWINDOW_HEIGHT 200

/**
 * Creates a test window
 */
static SDL_Window *createMouseSuiteTestWindow(void)
{
    int width = MOUSE_TESTWINDOW_WIDTH, height = MOUSE_TESTWINDOW_HEIGHT;
    SDL_Window *window;
    window = SDL_CreateWindow("mousecreateMouseSuiteTestWindow", width, height, 0);
    SDLTest_AssertPass("SDL_CreateWindow()");
    SDLTest_AssertCheck(window != NULL, "Check SDL_CreateWindow result");
    return window;
}

/**
 * Destroy test window
 */
static void destroyMouseSuiteTestWindow(SDL_Window *window)
{
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
        SDLTest_AssertPass("SDL_DestroyWindow()");
    }
}

/**
 * Check call to SDL_WarpMouseInWindow
 *
 * \sa SDL_WarpMouseInWindow
 */
static int mouse_warpMouseInWindow(void *arg)
{
    const int w = MOUSE_TESTWINDOW_WIDTH, h = MOUSE_TESTWINDOW_HEIGHT;
    int numPositions = 6;
    float xPositions[6];
    float yPositions[6];
    float x, y;
    int i, j;
    SDL_Window *window;

    xPositions[0] = -1;
    xPositions[1] = 0;
    xPositions[2] = 1;
    xPositions[3] = (float)w - 1;
    xPositions[4] = (float)w;
    xPositions[5] = (float)w + 1;
    yPositions[0] = -1;
    yPositions[1] = 0;
    yPositions[2] = 1;
    yPositions[3] = (float)h - 1;
    yPositions[4] = (float)h;
    yPositions[5] = (float)h + 1;
    /* Create test window */
    window = createMouseSuiteTestWindow();
    if (!window) {
        return TEST_ABORTED;
    }

    /* Mouse to random position inside window */
    x = (float)SDLTest_RandomIntegerInRange(1, w - 1);
    y = (float)SDLTest_RandomIntegerInRange(1, h - 1);
    SDL_WarpMouseInWindow(window, x, y);
    SDLTest_AssertPass("SDL_WarpMouseInWindow(...,%.f,%.f)", x, y);

    /* Same position again */
    SDL_WarpMouseInWindow(window, x, y);
    SDLTest_AssertPass("SDL_WarpMouseInWindow(...,%.f,%.f)", x, y);

    /* Mouse to various boundary positions */
    for (i = 0; i < numPositions; i++) {
        for (j = 0; j < numPositions; j++) {
            x = xPositions[i];
            y = yPositions[j];
            SDL_WarpMouseInWindow(window, x, y);
            SDLTest_AssertPass("SDL_WarpMouseInWindow(...,%.f,%.f)", x, y);

            /* TODO: add tracking of events and check that each call generates a mouse motion event */
            SDL_PumpEvents();
            SDLTest_AssertPass("SDL_PumpEvents()");
        }
    }

    /* Clean up test window */
    destroyMouseSuiteTestWindow(window);

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetMouseFocus
 *
 * \sa SDL_GetMouseFocus
 */
static int mouse_getMouseFocus(void *arg)
{
    const int w = MOUSE_TESTWINDOW_WIDTH, h = MOUSE_TESTWINDOW_HEIGHT;
    float x, y;
    SDL_Window *window;
    SDL_Window *focusWindow;
    const SDL_bool video_driver_is_wayland = !SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland");

    /* Get focus - focus non-deterministic */
    focusWindow = SDL_GetMouseFocus();
    SDLTest_AssertPass("SDL_GetMouseFocus()");

    /* Create test window */
    window = createMouseSuiteTestWindow();
    if (!window) {
        return TEST_ABORTED;
    }

    /* Wayland explicitly disallows warping the mouse pointer, so this test must be skipped. */
    if (!video_driver_is_wayland) {
        /* Mouse to random position inside window */
        x = (float)SDLTest_RandomIntegerInRange(1, w - 1);
        y = (float)SDLTest_RandomIntegerInRange(1, h - 1);
        SDL_WarpMouseInWindow(window, x, y);
        SDLTest_AssertPass("SDL_WarpMouseInWindow(...,%.f,%.f)", x, y);

        /* Pump events to update focus state */
        SDL_Delay(100);
        SDL_PumpEvents();
        SDLTest_AssertPass("SDL_PumpEvents()");

        /* Get focus with explicit window setup - focus deterministic */
        focusWindow = SDL_GetMouseFocus();
        SDLTest_AssertPass("SDL_GetMouseFocus()");
        SDLTest_AssertCheck(focusWindow != NULL, "Check returned window value is not NULL");
        SDLTest_AssertCheck(focusWindow == window, "Check returned window value is test window");

        /* Mouse to random position outside window */
        x = (float)SDLTest_RandomIntegerInRange(-9, -1);
        y = (float)SDLTest_RandomIntegerInRange(-9, -1);
        SDL_WarpMouseInWindow(window, x, y);
        SDLTest_AssertPass("SDL_WarpMouseInWindow(...,%.f,%.f)", x, y);
    } else {
        SDLTest_Log("Skipping mouse warp focus tests: Wayland does not support warping the mouse pointer");
    }

    /* Clean up test window */
    destroyMouseSuiteTestWindow(window);

    /* Pump events to update focus state */
    SDL_PumpEvents();
    SDLTest_AssertPass("SDL_PumpEvents()");

    /* Get focus for non-existing window */
    focusWindow = SDL_GetMouseFocus();
    SDLTest_AssertPass("SDL_GetMouseFocus()");
    SDLTest_AssertCheck(focusWindow == NULL, "Check returned window value is NULL");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetDefaultCursor
 *
 * \sa SDL_GetDefaultCursor
 */
static int mouse_getDefaultCursor(void *arg)
{
    SDL_Cursor *cursor;

    /* Get current cursor */
    cursor = SDL_GetDefaultCursor();
    SDLTest_AssertPass("Call to SDL_GetDefaultCursor()");
    SDLTest_AssertCheck(cursor != NULL, "Validate result from SDL_GetDefaultCursor() is not NULL");

    return TEST_COMPLETED;
}

/**
 * Check call to SDL_GetGlobalMouseState
 *
 * \sa SDL_GetGlobalMouseState
 */
static int mouse_getGlobalMouseState(void *arg)
{
    float x;
    float y;
    SDL_MouseButtonFlags state;

    x = -FLT_MAX;
    y = -FLT_MAX;

    /* Get current cursor */
    state = SDL_GetGlobalMouseState(&x, &y);
    SDLTest_AssertPass("Call to SDL_GetGlobalMouseState()");
    SDLTest_AssertCheck(x > -FLT_MAX, "Validate that value of x is > -FLT_MAX, got: %.f", x);
    SDLTest_AssertCheck(y > -FLT_MAX, "Validate that value of y is > -FLT_MAX, got: %.f", y);
    SDLTest_AssertCheck(mouseStateCheck(state), "Validate state returned from function, got: %" SDL_PRIu32, state);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Mouse test cases */
static const SDLTest_TestCaseReference mouseTest1 = {
    (SDLTest_TestCaseFp)mouse_getMouseState, "mouse_getMouseState", "Check call to SDL_GetMouseState", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest2 = {
    (SDLTest_TestCaseFp)mouse_getRelativeMouseState, "mouse_getRelativeMouseState", "Check call to SDL_GetRelativeMouseState", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest3 = {
    (SDLTest_TestCaseFp)mouse_createFreeCursor, "mouse_createFreeCursor", "Check call to SDL_CreateCursor and SDL_DestroyCursor", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest4 = {
    (SDLTest_TestCaseFp)mouse_showCursor, "mouse_showCursor", "Check call to SDL_ShowCursor", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest5 = {
    (SDLTest_TestCaseFp)mouse_setCursor, "mouse_setCursor", "Check call to SDL_SetCursor", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest6 = {
    (SDLTest_TestCaseFp)mouse_getCursor, "mouse_getCursor", "Check call to SDL_GetCursor", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest7 = {
    (SDLTest_TestCaseFp)mouse_warpMouseInWindow, "mouse_warpMouseInWindow", "Check call to SDL_WarpMouseInWindow", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest8 = {
    (SDLTest_TestCaseFp)mouse_getMouseFocus, "mouse_getMouseFocus", "Check call to SDL_getMouseFocus", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest9 = {
    (SDLTest_TestCaseFp)mouse_createFreeColorCursor, "mouse_createFreeColorCursor", "Check call to SDL_CreateColorCursor and SDL_DestroyCursor", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest10 = {
    (SDLTest_TestCaseFp)mouse_getSetRelativeMouseMode, "mouse_getSetRelativeMouseMode", "Check call to SDL_GetRelativeMouseMode and SDL_SetRelativeMouseMode", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest11 = {
    (SDLTest_TestCaseFp)mouse_getDefaultCursor, "mouse_getDefaultCursor", "Check call to mouse_getDefaultCursor", TEST_ENABLED
};

static const SDLTest_TestCaseReference mouseTest12 = {
    (SDLTest_TestCaseFp)mouse_getGlobalMouseState, "mouse_getGlobalMouseState", "Check call to mouse_getGlobalMouseState", TEST_ENABLED
};

/* Sequence of Mouse test cases */
static const SDLTest_TestCaseReference *mouseTests[] = {
    &mouseTest1, &mouseTest2, &mouseTest3, &mouseTest4, &mouseTest5, &mouseTest6,
    &mouseTest7, &mouseTest8, &mouseTest9, &mouseTest10, &mouseTest11, &mouseTest12, NULL
};

/* Mouse test suite (global) */
SDLTest_TestSuiteReference mouseTestSuite = {
    "Mouse",
    NULL,
    mouseTests,
    NULL
};
