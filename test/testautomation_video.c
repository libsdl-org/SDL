/**
 * Video test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Private helpers */

/**
 * Create a test window
 */
static SDL_Window *createVideoSuiteTestWindow(const char *title)
{
    SDL_Window *window;
    int w, h;
    SDL_WindowFlags flags;
    SDL_bool needs_renderer = SDL_FALSE;

    /* Standard window */
    w = SDLTest_RandomIntegerInRange(320, 1024);
    h = SDLTest_RandomIntegerInRange(320, 768);
    flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS;

    window = SDL_CreateWindow(title, w, h, flags);
    SDLTest_AssertPass("Call to SDL_CreateWindow('Title',%d,%d,%d)", w, h, flags);
    SDLTest_AssertCheck(window != NULL, "Validate that returned window struct is not NULL");

    /* Wayland and XWayland windows require that a frame be presented before they are fully mapped and visible onscreen.
     * This is required for the mouse/keyboard grab tests to pass.
     */
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        needs_renderer = SDL_TRUE;
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        /* Try to detect if the x11 driver is running under XWayland */
        const char *session_type = SDL_getenv("XDG_SESSION_TYPE");
        if (session_type && SDL_strcasecmp(session_type, "wayland") == 0) {
            needs_renderer = SDL_TRUE;
        }
    }

    if (needs_renderer) {
        SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL, 0);
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);

            /* Some desktops don't display the window immediately after presentation,
             * so delay to give the window time to actually appear on the desktop.
             */
            SDL_Delay(100);
        } else {
            SDLTest_Log("Unable to create a renderer, some tests may fail on Wayland/XWayland");
        }
    }

    return window;
}

/**
 * Destroy test window
 */
static void destroyVideoSuiteTestWindow(SDL_Window *window)
{
    if (window != NULL) {
        SDL_DestroyWindow(window);
        window = NULL;
        SDLTest_AssertPass("Call to SDL_DestroyWindow()");
    }
}

/* Test case functions */

/**
 * \brief Enable and disable screensaver while checking state
 */
static int video_enableDisableScreensaver(void *arg)
{
    SDL_bool initialResult;
    SDL_bool result;

    /* Get current state and proceed according to current state */
    initialResult = SDL_ScreenSaverEnabled();
    SDLTest_AssertPass("Call to SDL_ScreenSaverEnabled()");
    if (initialResult == SDL_TRUE) {

        /* Currently enabled: disable first, then enable again */

        /* Disable screensaver and check */
        SDL_DisableScreenSaver();
        SDLTest_AssertPass("Call to SDL_DisableScreenSaver()");
        result = SDL_ScreenSaverEnabled();
        SDLTest_AssertPass("Call to SDL_ScreenSaverEnabled()");
        SDLTest_AssertCheck(result == SDL_FALSE, "Verify result from SDL_ScreenSaverEnabled, expected: %i, got: %i", SDL_FALSE, result);

        /* Enable screensaver and check */
        SDL_EnableScreenSaver();
        SDLTest_AssertPass("Call to SDL_EnableScreenSaver()");
        result = SDL_ScreenSaverEnabled();
        SDLTest_AssertPass("Call to SDL_ScreenSaverEnabled()");
        SDLTest_AssertCheck(result == SDL_TRUE, "Verify result from SDL_ScreenSaverEnabled, expected: %i, got: %i", SDL_TRUE, result);

    } else {

        /* Currently disabled: enable first, then disable again */

        /* Enable screensaver and check */
        SDL_EnableScreenSaver();
        SDLTest_AssertPass("Call to SDL_EnableScreenSaver()");
        result = SDL_ScreenSaverEnabled();
        SDLTest_AssertPass("Call to SDL_ScreenSaverEnabled()");
        SDLTest_AssertCheck(result == SDL_TRUE, "Verify result from SDL_ScreenSaverEnabled, expected: %i, got: %i", SDL_TRUE, result);

        /* Disable screensaver and check */
        SDL_DisableScreenSaver();
        SDLTest_AssertPass("Call to SDL_DisableScreenSaver()");
        result = SDL_ScreenSaverEnabled();
        SDLTest_AssertPass("Call to SDL_ScreenSaverEnabled()");
        SDLTest_AssertCheck(result == SDL_FALSE, "Verify result from SDL_ScreenSaverEnabled, expected: %i, got: %i", SDL_FALSE, result);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Tests the functionality of the SDL_CreateWindow function using different sizes
 */
static int video_createWindowVariousSizes(void *arg)
{
    SDL_Window *window;
    const char *title = "video_createWindowVariousSizes Test Window";
    int w = 0, h = 0;
    int wVariation, hVariation;

    for (wVariation = 0; wVariation < 3; wVariation++) {
        for (hVariation = 0; hVariation < 3; hVariation++) {
            switch (wVariation) {
            case 0:
                /* Width of 1 */
                w = 1;
                break;
            case 1:
                /* Random "normal" width */
                w = SDLTest_RandomIntegerInRange(320, 1920);
                break;
            case 2:
                /* Random "large" width */
                w = SDLTest_RandomIntegerInRange(2048, 4095);
                break;
            }

            switch (hVariation) {
            case 0:
                /* Height of 1 */
                h = 1;
                break;
            case 1:
                /* Random "normal" height */
                h = SDLTest_RandomIntegerInRange(320, 1080);
                break;
            case 2:
                /* Random "large" height */
                h = SDLTest_RandomIntegerInRange(2048, 4095);
                break;
            }

            window = SDL_CreateWindow(title, w, h, 0);
            SDLTest_AssertPass("Call to SDL_CreateWindow('Title',%d,%d,SHOWN)", w, h);
            SDLTest_AssertCheck(window != NULL, "Validate that returned window struct is not NULL");

            /* Clean up */
            destroyVideoSuiteTestWindow(window);
        }
    }

    return TEST_COMPLETED;
}

/**
 * \brief Tests the functionality of the SDL_CreateWindow function using different flags
 */
static int video_createWindowVariousFlags(void *arg)
{
    SDL_Window *window;
    const char *title = "video_createWindowVariousFlags Test Window";
    int w, h;
    int fVariation;
    SDL_WindowFlags flags;

    /* Standard window */
    w = SDLTest_RandomIntegerInRange(320, 1024);
    h = SDLTest_RandomIntegerInRange(320, 768);

    for (fVariation = 1; fVariation < 14; fVariation++) {
        switch (fVariation) {
        default:
        case 1:
            flags = SDL_WINDOW_FULLSCREEN;
            /* Skip - blanks screen; comment out next line to run test */
            continue;
            break;
        case 2:
            flags = SDL_WINDOW_OPENGL;
            /* Skip - not every video driver supports OpenGL; comment out next line to run test */
            continue;
            break;
        case 3:
            flags = 0;
            break;
        case 4:
            flags = SDL_WINDOW_HIDDEN;
            break;
        case 5:
            flags = SDL_WINDOW_BORDERLESS;
            break;
        case 6:
            flags = SDL_WINDOW_RESIZABLE;
            break;
        case 7:
            flags = SDL_WINDOW_MINIMIZED;
            break;
        case 8:
            flags = SDL_WINDOW_MAXIMIZED;
            break;
        case 9:
            flags = SDL_WINDOW_MOUSE_GRABBED;
            break;
        case 10:
            flags = SDL_WINDOW_INPUT_FOCUS;
            break;
        case 11:
            flags = SDL_WINDOW_MOUSE_FOCUS;
            break;
        case 12:
            flags = SDL_WINDOW_FOREIGN;
            break;
        case 13:
            flags = SDL_WINDOW_KEYBOARD_GRABBED;
            break;
        }

        window = SDL_CreateWindow(title, w, h, flags);
        SDLTest_AssertPass("Call to SDL_CreateWindow('Title',%d,%d,%d)", w, h, flags);
        SDLTest_AssertCheck(window != NULL, "Validate that returned window struct is not NULL");

        /* Clean up */
        destroyVideoSuiteTestWindow(window);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Tests the functionality of the SDL_GetWindowFlags function
 */
static int video_getWindowFlags(void *arg)
{
    SDL_Window *window;
    const char *title = "video_getWindowFlags Test Window";
    SDL_WindowFlags flags;
    Uint32 actualFlags;

    /* Reliable flag set always set in test window */
    flags = 0;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window != NULL) {
        actualFlags = SDL_GetWindowFlags(window);
        SDLTest_AssertPass("Call to SDL_GetWindowFlags()");
        SDLTest_AssertCheck((flags & actualFlags) == flags, "Verify returned value has flags %d set, got: %" SDL_PRIu32, flags, actualFlags);
    }

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    return TEST_COMPLETED;
}

/**
 * \brief Tests the functionality of the SDL_GetFullscreenDisplayModes function
 */
static int video_getFullscreenDisplayModes(void *arg)
{
    SDL_DisplayID *displays;
    const SDL_DisplayMode **modes;
    int count;
    int i;

    /* Get number of displays */
    displays = SDL_GetDisplays(NULL);
    if (displays) {
        SDLTest_AssertPass("Call to SDL_GetDisplays()");

        /* Make call for each display */
        for (i = 0; displays[i]; ++i) {
            modes = SDL_GetFullscreenDisplayModes(displays[i], &count);
            SDLTest_AssertPass("Call to SDL_GetFullscreenDisplayModes(%" SDL_PRIu32 ")", displays[i]);
            SDLTest_AssertCheck(modes != NULL, "Validate returned value from function; expected != NULL; got: %p", modes);
            SDLTest_AssertCheck(count >= 0, "Validate number of modes; expected: >= 0; got: %d", count);
            SDL_free((void *)modes);
        }
        SDL_free(displays);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Tests the functionality of the SDL_GetClosestFullscreenDisplayMode function against current resolution
 */
static int video_getClosestDisplayModeCurrentResolution(void *arg)
{
    SDL_DisplayID *displays;
    const SDL_DisplayMode **modes;
    SDL_DisplayMode current;
    const SDL_DisplayMode *closest;
    int i, num_modes;

    /* Get number of displays */
    displays = SDL_GetDisplays(NULL);
    if (displays) {
        SDLTest_AssertPass("Call to SDL_GetDisplays()");

        /* Make calls for each display */
        for (i = 0; displays[i]; ++i) {
            SDLTest_Log("Testing against display: %" SDL_PRIu32 "", displays[i]);

            /* Get first display mode to get a sane resolution; this should always work */
            modes = SDL_GetFullscreenDisplayModes(displays[i], &num_modes);
            SDLTest_AssertPass("Call to SDL_GetDisplayModes()");
            SDLTest_Assert(modes != NULL, "Verify returned value is not NULL");
            if (num_modes > 0) {
                SDL_memcpy(&current, modes[0], sizeof(current));

                /* Make call */
                closest = SDL_GetClosestFullscreenDisplayMode(displays[i], current.w, current.h, current.refresh_rate, SDL_FALSE);
                SDLTest_AssertPass("Call to SDL_GetClosestFullscreenDisplayMode(target=current)");
                SDLTest_Assert(closest != NULL, "Verify returned value is not NULL");

                /* Check that one gets the current resolution back again */
                if (closest) {
                    SDLTest_AssertCheck(closest->w == current.w,
                                        "Verify returned width matches current width; expected: %d, got: %d",
                                        current.w, closest->w);
                    SDLTest_AssertCheck(closest->h == current.h,
                                        "Verify returned height matches current height; expected: %d, got: %d",
                                        current.h, closest->h);
                }
            }
            SDL_free((void *)modes);
        }
        SDL_free(displays);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Tests the functionality of the SDL_GetClosestFullscreenDisplayMode function against random resolution
 */
static int video_getClosestDisplayModeRandomResolution(void *arg)
{
    SDL_DisplayID *displays;
    SDL_DisplayMode target;
    int i;
    int variation;

    /* Get number of displays */
    displays = SDL_GetDisplays(NULL);
    if (displays) {
        SDLTest_AssertPass("Call to SDL_GetDisplays()");

        /* Make calls for each display */
        for (i = 0; displays[i]; ++i) {
            SDLTest_Log("Testing against display: %" SDL_PRIu32 "", displays[i]);

            for (variation = 0; variation < 16; variation++) {

                /* Set random constraints */
                SDL_zero(target);
                target.w = (variation & 1) ? SDLTest_RandomIntegerInRange(1, 4096) : 0;
                target.h = (variation & 2) ? SDLTest_RandomIntegerInRange(1, 4096) : 0;
                target.refresh_rate = (variation & 8) ? (float)SDLTest_RandomIntegerInRange(25, 120) : 0.0f;

                /* Make call; may or may not find anything, so don't validate any further */
                SDL_GetClosestFullscreenDisplayMode(displays[i], target.w, target.h, target.refresh_rate, SDL_FALSE);
                SDLTest_AssertPass("Call to SDL_GetClosestFullscreenDisplayMode(target=random/variation%d)", variation);
            }
        }
        SDL_free(displays);
    }

    return TEST_COMPLETED;
}

/**
 * \brief Tests call to SDL_GetWindowFullscreenMode
 *
 * \sa SDL_GetWindowFullscreenMode
 */
static int video_getWindowDisplayMode(void *arg)
{
    SDL_Window *window;
    const char *title = "video_getWindowDisplayMode Test Window";
    const SDL_DisplayMode *mode;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window != NULL) {
        mode = SDL_GetWindowFullscreenMode(window);
        SDLTest_AssertPass("Call to SDL_GetWindowFullscreenMode()");
        SDLTest_AssertCheck(mode == NULL, "Validate result value; expected: NULL, got: %p", mode);
    }

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    return TEST_COMPLETED;
}

/* Helper function that checks for an 'Invalid window' error */
static void checkInvalidWindowError(void)
{
    const char *invalidWindowError = "Invalid window";
    const char *lastError;

    lastError = SDL_GetError();
    SDLTest_AssertPass("SDL_GetError()");
    SDLTest_AssertCheck(lastError != NULL, "Verify error message is not NULL");
    if (lastError != NULL) {
        SDLTest_AssertCheck(SDL_strcmp(lastError, invalidWindowError) == 0,
                            "SDL_GetError(): expected message '%s', was message: '%s'",
                            invalidWindowError,
                            lastError);
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
    }
}

/**
 * \brief Tests call to SDL_GetWindowFullscreenMode with invalid input
 *
 * \sa SDL_GetWindowFullscreenMode
 */
static int video_getWindowDisplayModeNegative(void *arg)
{
    const SDL_DisplayMode *mode;

    /* Call against invalid window */
    mode = SDL_GetWindowFullscreenMode(NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowFullscreenMode(window=NULL)");
    SDLTest_AssertCheck(mode == NULL, "Validate result value; expected: NULL, got: %p", mode);
    checkInvalidWindowError();

    return TEST_COMPLETED;
}

/* Helper for setting and checking the window mouse grab state */
static void setAndCheckWindowMouseGrabState(SDL_Window *window, SDL_bool desiredState)
{
    SDL_bool currentState;

    /* Set state */
    SDL_SetWindowMouseGrab(window, desiredState);
    SDLTest_AssertPass("Call to SDL_SetWindowMouseGrab(%s)", (desiredState == SDL_FALSE) ? "SDL_FALSE" : "SDL_TRUE");

    /* Get and check state */
    currentState = SDL_GetWindowMouseGrab(window);
    SDLTest_AssertPass("Call to SDL_GetWindowMouseGrab()");
    SDLTest_AssertCheck(
        currentState == desiredState,
        "Validate returned state; expected: %s, got: %s",
        (desiredState == SDL_FALSE) ? "SDL_FALSE" : "SDL_TRUE",
        (currentState == SDL_FALSE) ? "SDL_FALSE" : "SDL_TRUE");

    if (desiredState) {
        SDLTest_AssertCheck(
            SDL_GetGrabbedWindow() == window,
            "Grabbed window should be to our window");
        SDLTest_AssertCheck(
            SDL_GetWindowGrab(window),
            "SDL_GetWindowGrab() should return SDL_TRUE");
        SDLTest_AssertCheck(
            SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_GRABBED,
            "SDL_WINDOW_MOUSE_GRABBED should be set");
    } else {
        SDLTest_AssertCheck(
            !(SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_GRABBED),
            "SDL_WINDOW_MOUSE_GRABBED should be unset");
    }
}

/* Helper for setting and checking the window keyboard grab state */
static void setAndCheckWindowKeyboardGrabState(SDL_Window *window, SDL_bool desiredState)
{
    SDL_bool currentState;

    /* Set state */
    SDL_SetWindowKeyboardGrab(window, desiredState);
    SDLTest_AssertPass("Call to SDL_SetWindowKeyboardGrab(%s)", (desiredState == SDL_FALSE) ? "SDL_FALSE" : "SDL_TRUE");

    /* Get and check state */
    currentState = SDL_GetWindowKeyboardGrab(window);
    SDLTest_AssertPass("Call to SDL_GetWindowKeyboardGrab()");
    SDLTest_AssertCheck(
        currentState == desiredState,
        "Validate returned state; expected: %s, got: %s",
        (desiredState == SDL_FALSE) ? "SDL_FALSE" : "SDL_TRUE",
        (currentState == SDL_FALSE) ? "SDL_FALSE" : "SDL_TRUE");

    if (desiredState) {
        SDLTest_AssertCheck(
            SDL_GetGrabbedWindow() == window,
            "Grabbed window should be set to our window");
        SDLTest_AssertCheck(
            SDL_GetWindowGrab(window),
            "SDL_GetWindowGrab() should return SDL_TRUE");
        SDLTest_AssertCheck(
            SDL_GetWindowFlags(window) & SDL_WINDOW_KEYBOARD_GRABBED,
            "SDL_WINDOW_KEYBOARD_GRABBED should be set");
    } else {
        SDLTest_AssertCheck(
            !(SDL_GetWindowFlags(window) & SDL_WINDOW_KEYBOARD_GRABBED),
            "SDL_WINDOW_KEYBOARD_GRABBED should be unset");
    }
}

/**
 * \brief Tests keyboard and mouse grab support
 *
 * \sa SDL_GetWindowGrab
 * \sa SDL_SetWindowGrab
 */
static int video_getSetWindowGrab(void *arg)
{
    const char *title = "video_getSetWindowGrab Test Window";
    SDL_Window *window;
    SDL_bool originalMouseState, originalKeyboardState;
    SDL_bool hasFocusGained = SDL_FALSE;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    /* Need to raise the window to have and SDL_EVENT_WINDOW_FOCUS_GAINED,
     * so that the window gets the flags SDL_WINDOW_INPUT_FOCUS,
     * so that it can be "grabbed" */
    SDL_RaiseWindow(window);

    {
        SDL_Event evt;
        SDL_zero(evt);
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                hasFocusGained = SDL_TRUE;
            }
        }
    }

    SDLTest_AssertCheck(hasFocusGained == SDL_TRUE, "Expectded window with focus");

    /* Get state */
    originalMouseState = SDL_GetWindowMouseGrab(window);
    SDLTest_AssertPass("Call to SDL_GetWindowMouseGrab()");
    originalKeyboardState = SDL_GetWindowKeyboardGrab(window);
    SDLTest_AssertPass("Call to SDL_GetWindowKeyboardGrab()");

    /* F */
    setAndCheckWindowKeyboardGrabState(window, SDL_FALSE);
    setAndCheckWindowMouseGrabState(window, SDL_FALSE);
    SDLTest_AssertCheck(!SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab should return SDL_FALSE");
    SDLTest_AssertCheck(SDL_GetGrabbedWindow() == NULL,
                        "Expected NULL grabbed window");

    /* F --> F */
    setAndCheckWindowMouseGrabState(window, SDL_FALSE);
    setAndCheckWindowKeyboardGrabState(window, SDL_FALSE);
    SDLTest_AssertCheck(SDL_GetGrabbedWindow() == NULL,
                        "Expected NULL grabbed window");

    /* F --> T */
    setAndCheckWindowMouseGrabState(window, SDL_TRUE);
    setAndCheckWindowKeyboardGrabState(window, SDL_TRUE);
    SDLTest_AssertCheck(SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_TRUE");

    /* T --> T */
    setAndCheckWindowKeyboardGrabState(window, SDL_TRUE);
    setAndCheckWindowMouseGrabState(window, SDL_TRUE);
    SDLTest_AssertCheck(SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_TRUE");

    /* M: T --> F */
    /* K: T --> T */
    setAndCheckWindowKeyboardGrabState(window, SDL_TRUE);
    setAndCheckWindowMouseGrabState(window, SDL_FALSE);
    SDLTest_AssertCheck(SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_TRUE");

    /* M: F --> T */
    /* K: T --> F */
    setAndCheckWindowMouseGrabState(window, SDL_TRUE);
    setAndCheckWindowKeyboardGrabState(window, SDL_FALSE);
    SDLTest_AssertCheck(SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_TRUE");

    /* M: T --> F */
    /* K: F --> F */
    setAndCheckWindowMouseGrabState(window, SDL_FALSE);
    setAndCheckWindowKeyboardGrabState(window, SDL_FALSE);
    SDLTest_AssertCheck(!SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_FALSE");
    SDLTest_AssertCheck(SDL_GetGrabbedWindow() == NULL,
                        "Expected NULL grabbed window");

    /* Using the older SDL_SetWindowGrab API should only grab mouse by default */
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDLTest_AssertPass("Call to SDL_SetWindowGrab(SDL_TRUE)");
    SDLTest_AssertCheck(SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_TRUE");
    SDLTest_AssertCheck(SDL_GetWindowMouseGrab(window),
                        "SDL_GetWindowMouseGrab() should return SDL_TRUE");
    SDLTest_AssertCheck(!SDL_GetWindowKeyboardGrab(window),
                        "SDL_GetWindowKeyboardGrab() should return SDL_FALSE");
    SDL_SetWindowGrab(window, SDL_FALSE);
    SDLTest_AssertCheck(!SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_FALSE");
    SDLTest_AssertCheck(!SDL_GetWindowMouseGrab(window),
                        "SDL_GetWindowMouseGrab() should return SDL_FALSE");
    SDLTest_AssertCheck(!SDL_GetWindowKeyboardGrab(window),
                        "SDL_GetWindowKeyboardGrab() should return SDL_FALSE");

    /* Now test with SDL_HINT_GRAB_KEYBOARD set. We should get keyboard grab now. */
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDLTest_AssertPass("Call to SDL_SetWindowGrab(SDL_TRUE)");
    SDLTest_AssertCheck(SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_TRUE");
    SDLTest_AssertCheck(SDL_GetWindowMouseGrab(window),
                        "SDL_GetWindowMouseGrab() should return SDL_TRUE");
    SDLTest_AssertCheck(SDL_GetWindowKeyboardGrab(window),
                        "SDL_GetWindowKeyboardGrab() should return SDL_TRUE");
    SDL_SetWindowGrab(window, SDL_FALSE);
    SDLTest_AssertCheck(!SDL_GetWindowGrab(window),
                        "SDL_GetWindowGrab() should return SDL_FALSE");
    SDLTest_AssertCheck(!SDL_GetWindowMouseGrab(window),
                        "SDL_GetWindowMouseGrab() should return SDL_FALSE");
    SDLTest_AssertCheck(!SDL_GetWindowKeyboardGrab(window),
                        "SDL_GetWindowKeyboardGrab() should return SDL_FALSE");

    /* Negative tests */
    SDL_GetWindowGrab(NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowGrab(window=NULL)");
    checkInvalidWindowError();

    SDL_GetWindowKeyboardGrab(NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowKeyboardGrab(window=NULL)");
    checkInvalidWindowError();

    SDL_SetWindowGrab(NULL, SDL_FALSE);
    SDLTest_AssertPass("Call to SDL_SetWindowGrab(window=NULL,SDL_FALSE)");
    checkInvalidWindowError();

    SDL_SetWindowKeyboardGrab(NULL, SDL_FALSE);
    SDLTest_AssertPass("Call to SDL_SetWindowKeyboardGrab(window=NULL,SDL_FALSE)");
    checkInvalidWindowError();

    SDL_SetWindowGrab(NULL, SDL_TRUE);
    SDLTest_AssertPass("Call to SDL_SetWindowGrab(window=NULL,SDL_TRUE)");
    checkInvalidWindowError();

    SDL_SetWindowKeyboardGrab(NULL, SDL_TRUE);
    SDLTest_AssertPass("Call to SDL_SetWindowKeyboardGrab(window=NULL,SDL_TRUE)");
    checkInvalidWindowError();

    /* Restore state */
    setAndCheckWindowMouseGrabState(window, originalMouseState);
    setAndCheckWindowKeyboardGrabState(window, originalKeyboardState);

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    return TEST_COMPLETED;
}

/**
 * \brief Tests call to SDL_GetWindowID and SDL_GetWindowFromID
 *
 * \sa SDL_GetWindowID
 * \sa SDL_SetWindowFromID
 */
static int video_getWindowId(void *arg)
{
    const char *title = "video_getWindowId Test Window";
    SDL_Window *window;
    SDL_Window *result;
    Uint32 id, randomId;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    /* Get ID */
    id = SDL_GetWindowID(window);
    SDLTest_AssertPass("Call to SDL_GetWindowID()");

    /* Get window from ID */
    result = SDL_GetWindowFromID(id);
    SDLTest_AssertPass("Call to SDL_GetWindowID(%" SDL_PRIu32 ")", id);
    SDLTest_AssertCheck(result == window, "Verify result matches window pointer");

    /* Get window from random large ID, no result check */
    randomId = SDLTest_RandomIntegerInRange(UINT8_MAX, UINT16_MAX);
    result = SDL_GetWindowFromID(randomId);
    SDLTest_AssertPass("Call to SDL_GetWindowID(%" SDL_PRIu32 "/random_large)", randomId);

    /* Get window from 0 and Uint32 max ID, no result check */
    result = SDL_GetWindowFromID(0);
    SDLTest_AssertPass("Call to SDL_GetWindowID(0)");
    result = SDL_GetWindowFromID(UINT32_MAX);
    SDLTest_AssertPass("Call to SDL_GetWindowID(UINT32_MAX)");

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    /* Get window from ID for closed window */
    result = SDL_GetWindowFromID(id);
    SDLTest_AssertPass("Call to SDL_GetWindowID(%" SDL_PRIu32 "/closed_window)", id);
    SDLTest_AssertCheck(result == NULL, "Verify result is NULL");

    /* Negative test */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    id = SDL_GetWindowID(NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowID(window=NULL)");
    checkInvalidWindowError();

    return TEST_COMPLETED;
}

/**
 * \brief Tests call to SDL_GetWindowPixelFormat
 *
 * \sa SDL_GetWindowPixelFormat
 */
static int video_getWindowPixelFormat(void *arg)
{
    const char *title = "video_getWindowPixelFormat Test Window";
    SDL_Window *window;
    Uint32 format;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    /* Get format */
    format = SDL_GetWindowPixelFormat(window);
    SDLTest_AssertPass("Call to SDL_GetWindowPixelFormat()");
    SDLTest_AssertCheck(format != SDL_PIXELFORMAT_UNKNOWN, "Verify that returned format is valid; expected: != %d, got: %" SDL_PRIu32, SDL_PIXELFORMAT_UNKNOWN, format);

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    /* Negative test */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    format = SDL_GetWindowPixelFormat(NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowPixelFormat(window=NULL)");
    checkInvalidWindowError();

    return TEST_COMPLETED;
}


static SDL_bool getPositionFromEvent(int *x, int *y)
{
    SDL_bool ret = SDL_FALSE;
    SDL_Event evt;
    SDL_zero(evt);
    while (SDL_PollEvent(&evt)) {
        if (evt.type == SDL_EVENT_WINDOW_MOVED) {
            *x = evt.window.data1;
            *y = evt.window.data2;
            ret = SDL_TRUE;
        }
    }
    return ret;
}

static SDL_bool getSizeFromEvent(int *w, int *h)
{
    SDL_bool ret = SDL_FALSE;
    SDL_Event evt;
    SDL_zero(evt);
    while (SDL_PollEvent(&evt)) {
        if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
            *w = evt.window.data1;
            *h = evt.window.data2;
            ret = SDL_TRUE;
        }
    }
    return ret;
}

/**
 * \brief Tests call to SDL_GetWindowPosition and SDL_SetWindowPosition
 *
 * \sa SDL_GetWindowPosition
 * \sa SDL_SetWindowPosition
 */
static int video_getSetWindowPosition(void *arg)
{
    const char *title = "video_getSetWindowPosition Test Window";
    SDL_Window *window;
    int xVariation, yVariation;
    int referenceX, referenceY;
    int currentX, currentY;
    int desiredX, desiredY;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    for (xVariation = 0; xVariation < 4; xVariation++) {
        for (yVariation = 0; yVariation < 4; yVariation++) {
            switch (xVariation) {
            default:
            case 0:
                /* Zero X Position */
                desiredX = 0;
                break;
            case 1:
                /* Random X position inside screen */
                desiredX = SDLTest_RandomIntegerInRange(1, 100);
                break;
            case 2:
                /* Random X position outside screen (positive) */
                desiredX = SDLTest_RandomIntegerInRange(10000, 11000);
                break;
            case 3:
                /* Random X position outside screen (negative) */
                desiredX = SDLTest_RandomIntegerInRange(-1000, -100);
                break;
            }

            switch (yVariation) {
            default:
            case 0:
                /* Zero X Position */
                desiredY = 0;
                break;
            case 1:
                /* Random X position inside screen */
                desiredY = SDLTest_RandomIntegerInRange(1, 100);
                break;
            case 2:
                /* Random X position outside screen (positive) */
                desiredY = SDLTest_RandomIntegerInRange(10000, 11000);
                break;
            case 3:
                /* Random Y position outside screen (negative) */
                desiredY = SDLTest_RandomIntegerInRange(-1000, -100);
                break;
            }

            /* Set position */
            SDL_SetWindowPosition(window, desiredX, desiredY);
            SDLTest_AssertPass("Call to SDL_SetWindowPosition(...,%d,%d)", desiredX, desiredY);

            /* Get position */
            currentX = desiredX + 1;
            currentY = desiredY + 1;
            SDL_GetWindowPosition(window, &currentX, &currentY);
            SDLTest_AssertPass("Call to SDL_GetWindowPosition()");

            if (desiredX == currentX && desiredY == currentY) {
                SDLTest_AssertCheck(desiredX == currentX, "Verify returned X position; expected: %d, got: %d", desiredX, currentX);
                SDLTest_AssertCheck(desiredY == currentY, "Verify returned Y position; expected: %d, got: %d", desiredY, currentY);
            } else {
                SDL_bool hasEvent;
                /* SDL_SetWindowPosition() and SDL_SetWindowSize() will make requests of the window manager and set the internal position and size,
                 * and then we get events signaling what actually happened, and they get passed on to the application if they're not what we expect. */
                desiredX = currentX + 1;
                desiredY = currentY + 1;
                hasEvent = getPositionFromEvent(&desiredX, &desiredY);
                SDLTest_AssertCheck(hasEvent == SDL_TRUE, "Changing position was not honored by WM, checking present of SDL_EVENT_WINDOW_MOVED");
                if (hasEvent) {
                    SDLTest_AssertCheck(desiredX == currentX, "Verify returned X position is the position from SDL event; expected: %d, got: %d", desiredX, currentX);
                    SDLTest_AssertCheck(desiredY == currentY, "Verify returned Y position is the position from SDL event; expected: %d, got: %d", desiredY, currentY);
                }
            }

            /* Get position X */
            currentX = desiredX + 1;
            SDL_GetWindowPosition(window, &currentX, NULL);
            SDLTest_AssertPass("Call to SDL_GetWindowPosition(&y=NULL)");
            SDLTest_AssertCheck(desiredX == currentX, "Verify returned X position; expected: %d, got: %d", desiredX, currentX);

            /* Get position Y */
            currentY = desiredY + 1;
            SDL_GetWindowPosition(window, NULL, &currentY);
            SDLTest_AssertPass("Call to SDL_GetWindowPosition(&x=NULL)");
            SDLTest_AssertCheck(desiredY == currentY, "Verify returned Y position; expected: %d, got: %d", desiredY, currentY);
        }
    }

    /* Dummy call with both pointers NULL */
    SDL_GetWindowPosition(window, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowPosition(&x=NULL,&y=NULL)");

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    /* Set some 'magic' value for later check that nothing was changed */
    referenceX = SDLTest_RandomSint32();
    referenceY = SDLTest_RandomSint32();
    currentX = referenceX;
    currentY = referenceY;
    desiredX = SDLTest_RandomSint32();
    desiredY = SDLTest_RandomSint32();

    /* Negative tests */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    SDL_GetWindowPosition(NULL, &currentX, &currentY);
    SDLTest_AssertPass("Call to SDL_GetWindowPosition(window=NULL)");
    SDLTest_AssertCheck(
        currentX == referenceX && currentY == referenceY,
        "Verify that content of X and Y pointers has not been modified; expected: %d,%d; got: %d,%d",
        referenceX, referenceY,
        currentX, currentY);
    checkInvalidWindowError();

    SDL_GetWindowPosition(NULL, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowPosition(NULL, NULL, NULL)");
    checkInvalidWindowError();

    SDL_SetWindowPosition(NULL, desiredX, desiredY);
    SDLTest_AssertPass("Call to SDL_SetWindowPosition(window=NULL)");
    checkInvalidWindowError();

    return TEST_COMPLETED;
}

/* Helper function that checks for an 'Invalid parameter' error */
static void checkInvalidParameterError(void)
{
    const char *invalidParameterError = "Parameter";
    const char *lastError;

    lastError = SDL_GetError();
    SDLTest_AssertPass("SDL_GetError()");
    SDLTest_AssertCheck(lastError != NULL, "Verify error message is not NULL");
    if (lastError != NULL) {
        SDLTest_AssertCheck(SDL_strncmp(lastError, invalidParameterError, SDL_strlen(invalidParameterError)) == 0,
                            "SDL_GetError(): expected message starts with '%s', was message: '%s'",
                            invalidParameterError,
                            lastError);
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
    }
}

/**
 * \brief Tests call to SDL_GetWindowSize and SDL_SetWindowSize
 *
 * \sa SDL_GetWindowSize
 * \sa SDL_SetWindowSize
 */
static int video_getSetWindowSize(void *arg)
{
    const char *title = "video_getSetWindowSize Test Window";
    SDL_Window *window;
    int result;
    SDL_Rect display;
    int maxwVariation, maxhVariation;
    int wVariation, hVariation;
    int referenceW, referenceH;
    int currentW, currentH;
    int desiredW, desiredH;

    /* Get display bounds for size range */
    result = SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display);
    SDLTest_AssertPass("SDL_GetDisplayBounds()");
    SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);
    if (result != 0) {
        return TEST_ABORTED;
    }

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

#ifdef __WIN32__
    /* Platform clips window size to screen size */
    maxwVariation = 4;
    maxhVariation = 4;
#else
    /* Platform allows window size >= screen size */
    maxwVariation = 5;
    maxhVariation = 5;
#endif

    for (wVariation = 0; wVariation < maxwVariation; wVariation++) {
        for (hVariation = 0; hVariation < maxhVariation; hVariation++) {
            switch (wVariation) {
            default:
            case 0:
                /* 1 Pixel Wide */
                desiredW = 1;
                break;
            case 1:
                /* Random width inside screen */
                desiredW = SDLTest_RandomIntegerInRange(1, 100);
                break;
            case 2:
                /* Width 1 pixel smaller than screen */
                desiredW = display.w - 1;
                break;
            case 3:
                /* Width at screen size */
                desiredW = display.w;
                break;
            case 4:
                /* Width 1 pixel larger than screen */
                desiredW = display.w + 1;
                break;
            }

            switch (hVariation) {
            default:
            case 0:
                /* 1 Pixel High */
                desiredH = 1;
                break;
            case 1:
                /* Random height inside screen */
                desiredH = SDLTest_RandomIntegerInRange(1, 100);
                break;
            case 2:
                /* Height 1 pixel smaller than screen */
                desiredH = display.h - 1;
                break;
            case 3:
                /* Height at screen size */
                desiredH = display.h;
                break;
            case 4:
                /* Height 1 pixel larger than screen */
                desiredH = display.h + 1;
                break;
            }

            /* Set size */
            SDL_SetWindowSize(window, desiredW, desiredH);
            SDLTest_AssertPass("Call to SDL_SetWindowSize(...,%d,%d)", desiredW, desiredH);

            /* Get size */
            currentW = desiredW + 1;
            currentH = desiredH + 1;
            SDL_GetWindowSize(window, &currentW, &currentH);
            SDLTest_AssertPass("Call to SDL_GetWindowSize()");

            if (desiredW == currentW && desiredH == currentH) {
                SDLTest_AssertCheck(desiredW == currentW, "Verify returned width; expected: %d, got: %d", desiredW, currentW);
                SDLTest_AssertCheck(desiredH == currentH, "Verify returned height; expected: %d, got: %d", desiredH, currentH);
            } else {
                SDL_bool hasEvent;
                /* SDL_SetWindowPosition() and SDL_SetWindowSize() will make requests of the window manager and set the internal position and size,
                 * and then we get events signaling what actually happened, and they get passed on to the application if they're not what we expect. */
                desiredW = currentW + 1;
                desiredH = currentH + 1;
                hasEvent = getSizeFromEvent(&desiredW, &desiredH);
                SDLTest_AssertCheck(hasEvent == SDL_TRUE, "Changing size was not honored by WM, checking presence of SDL_EVENT_WINDOW_RESIZED");
                if (hasEvent) {
                    SDLTest_AssertCheck(desiredW == currentW, "Verify returned width is the one from SDL event; expected: %d, got: %d", desiredW, currentW);
                    SDLTest_AssertCheck(desiredH == currentH, "Verify returned height is the one from SDL event; expected: %d, got: %d", desiredH, currentH);
                }
            }


            /* Get just width */
            currentW = desiredW + 1;
            SDL_GetWindowSize(window, &currentW, NULL);
            SDLTest_AssertPass("Call to SDL_GetWindowSize(&h=NULL)");
            SDLTest_AssertCheck(desiredW == currentW, "Verify returned width; expected: %d, got: %d", desiredW, currentW);

            /* Get just height */
            currentH = desiredH + 1;
            SDL_GetWindowSize(window, NULL, &currentH);
            SDLTest_AssertPass("Call to SDL_GetWindowSize(&w=NULL)");
            SDLTest_AssertCheck(desiredH == currentH, "Verify returned height; expected: %d, got: %d", desiredH, currentH);
        }
    }

    /* Dummy call with both pointers NULL */
    SDL_GetWindowSize(window, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowSize(&w=NULL,&h=NULL)");

    /* Negative tests for parameter input */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    for (desiredH = -2; desiredH < 2; desiredH++) {
        for (desiredW = -2; desiredW < 2; desiredW++) {
            if (desiredW <= 0 || desiredH <= 0) {
                SDL_SetWindowSize(window, desiredW, desiredH);
                SDLTest_AssertPass("Call to SDL_SetWindowSize(...,%d,%d)", desiredW, desiredH);
                checkInvalidParameterError();
            }
        }
    }

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    /* Set some 'magic' value for later check that nothing was changed */
    referenceW = SDLTest_RandomSint32();
    referenceH = SDLTest_RandomSint32();
    currentW = referenceW;
    currentH = referenceH;
    desiredW = SDLTest_RandomSint32();
    desiredH = SDLTest_RandomSint32();

    /* Negative tests for window input */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    SDL_GetWindowSize(NULL, &currentW, &currentH);
    SDLTest_AssertPass("Call to SDL_GetWindowSize(window=NULL)");
    SDLTest_AssertCheck(
        currentW == referenceW && currentH == referenceH,
        "Verify that content of W and H pointers has not been modified; expected: %d,%d; got: %d,%d",
        referenceW, referenceH,
        currentW, currentH);
    checkInvalidWindowError();

    SDL_GetWindowSize(NULL, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowSize(NULL, NULL, NULL)");
    checkInvalidWindowError();

    SDL_SetWindowSize(NULL, desiredW, desiredH);
    SDLTest_AssertPass("Call to SDL_SetWindowSize(window=NULL)");
    checkInvalidWindowError();

    return TEST_COMPLETED;
}

/**
 * \brief Tests call to SDL_GetWindowMinimumSize and SDL_SetWindowMinimumSize
 *
 */
static int video_getSetWindowMinimumSize(void *arg)
{
    const char *title = "video_getSetWindowMinimumSize Test Window";
    SDL_Window *window;
    int result;
    SDL_Rect display;
    int wVariation, hVariation;
    int referenceW, referenceH;
    int currentW, currentH;
    int desiredW = 1;
    int desiredH = 1;

    /* Get display bounds for size range */
    result = SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display);
    SDLTest_AssertPass("SDL_GetDisplayBounds()");
    SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);
    if (result != 0) {
        return TEST_ABORTED;
    }

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    for (wVariation = 0; wVariation < 5; wVariation++) {
        for (hVariation = 0; hVariation < 5; hVariation++) {
            switch (wVariation) {
            case 0:
                /* 1 Pixel Wide */
                desiredW = 1;
                break;
            case 1:
                /* Random width inside screen */
                desiredW = SDLTest_RandomIntegerInRange(2, display.w - 1);
                break;
            case 2:
                /* Width at screen size */
                desiredW = display.w;
                break;
            }

            switch (hVariation) {
            case 0:
                /* 1 Pixel High */
                desiredH = 1;
                break;
            case 1:
                /* Random height inside screen */
                desiredH = SDLTest_RandomIntegerInRange(2, display.h - 1);
                break;
            case 2:
                /* Height at screen size */
                desiredH = display.h;
                break;
            case 4:
                /* Height 1 pixel larger than screen */
                desiredH = display.h + 1;
                break;
            }

            /* Set size */
            SDL_SetWindowMinimumSize(window, desiredW, desiredH);
            SDLTest_AssertPass("Call to SDL_SetWindowMinimumSize(...,%d,%d)", desiredW, desiredH);

            /* Get size */
            currentW = desiredW + 1;
            currentH = desiredH + 1;
            SDL_GetWindowMinimumSize(window, &currentW, &currentH);
            SDLTest_AssertPass("Call to SDL_GetWindowMinimumSize()");
            SDLTest_AssertCheck(desiredW == currentW, "Verify returned width; expected: %d, got: %d", desiredW, currentW);
            SDLTest_AssertCheck(desiredH == currentH, "Verify returned height; expected: %d, got: %d", desiredH, currentH);

            /* Get just width */
            currentW = desiredW + 1;
            SDL_GetWindowMinimumSize(window, &currentW, NULL);
            SDLTest_AssertPass("Call to SDL_GetWindowMinimumSize(&h=NULL)");
            SDLTest_AssertCheck(desiredW == currentW, "Verify returned width; expected: %d, got: %d", desiredW, currentH);

            /* Get just height */
            currentH = desiredH + 1;
            SDL_GetWindowMinimumSize(window, NULL, &currentH);
            SDLTest_AssertPass("Call to SDL_GetWindowMinimumSize(&w=NULL)");
            SDLTest_AssertCheck(desiredH == currentH, "Verify returned height; expected: %d, got: %d", desiredW, currentH);
        }
    }

    /* Dummy call with both pointers NULL */
    SDL_GetWindowMinimumSize(window, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowMinimumSize(&w=NULL,&h=NULL)");

    /* Negative tests for parameter input */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    for (desiredH = -2; desiredH < 2; desiredH++) {
        for (desiredW = -2; desiredW < 2; desiredW++) {
            if (desiredW < 0 || desiredH < 0) {
                SDL_SetWindowMinimumSize(window, desiredW, desiredH);
                SDLTest_AssertPass("Call to SDL_SetWindowMinimumSize(...,%d,%d)", desiredW, desiredH);
                checkInvalidParameterError();
            }
        }
    }

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    /* Set some 'magic' value for later check that nothing was changed */
    referenceW = SDLTest_RandomSint32();
    referenceH = SDLTest_RandomSint32();
    currentW = referenceW;
    currentH = referenceH;
    desiredW = SDLTest_RandomSint32();
    desiredH = SDLTest_RandomSint32();

    /* Negative tests for window input */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    SDL_GetWindowMinimumSize(NULL, &currentW, &currentH);
    SDLTest_AssertPass("Call to SDL_GetWindowMinimumSize(window=NULL)");
    SDLTest_AssertCheck(
        currentW == referenceW && currentH == referenceH,
        "Verify that content of W and H pointers has not been modified; expected: %d,%d; got: %d,%d",
        referenceW, referenceH,
        currentW, currentH);
    checkInvalidWindowError();

    SDL_GetWindowMinimumSize(NULL, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowMinimumSize(NULL, NULL, NULL)");
    checkInvalidWindowError();

    SDL_SetWindowMinimumSize(NULL, desiredW, desiredH);
    SDLTest_AssertPass("Call to SDL_SetWindowMinimumSize(window=NULL)");
    checkInvalidWindowError();

    return TEST_COMPLETED;
}

/**
 * \brief Tests call to SDL_GetWindowMaximumSize and SDL_SetWindowMaximumSize
 *
 */
static int video_getSetWindowMaximumSize(void *arg)
{
    const char *title = "video_getSetWindowMaximumSize Test Window";
    SDL_Window *window;
    int result;
    SDL_Rect display;
    int wVariation, hVariation;
    int referenceW, referenceH;
    int currentW, currentH;
    int desiredW = 0, desiredH = 0;

    /* Get display bounds for size range */
    result = SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display);
    SDLTest_AssertPass("SDL_GetDisplayBounds()");
    SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);
    if (result != 0) {
        return TEST_ABORTED;
    }

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    for (wVariation = 0; wVariation < 3; wVariation++) {
        for (hVariation = 0; hVariation < 3; hVariation++) {
            switch (wVariation) {
            case 0:
                /* 1 Pixel Wide */
                desiredW = 1;
                break;
            case 1:
                /* Random width inside screen */
                desiredW = SDLTest_RandomIntegerInRange(2, display.w - 1);
                break;
            case 2:
                /* Width at screen size */
                desiredW = display.w;
                break;
            }

            switch (hVariation) {
            case 0:
                /* 1 Pixel High */
                desiredH = 1;
                break;
            case 1:
                /* Random height inside screen */
                desiredH = SDLTest_RandomIntegerInRange(2, display.h - 1);
                break;
            case 2:
                /* Height at screen size */
                desiredH = display.h;
                break;
            }

            /* Set size */
            SDL_SetWindowMaximumSize(window, desiredW, desiredH);
            SDLTest_AssertPass("Call to SDL_SetWindowMaximumSize(...,%d,%d)", desiredW, desiredH);

            /* Get size */
            currentW = desiredW + 1;
            currentH = desiredH + 1;
            SDL_GetWindowMaximumSize(window, &currentW, &currentH);
            SDLTest_AssertPass("Call to SDL_GetWindowMaximumSize()");
            SDLTest_AssertCheck(desiredW == currentW, "Verify returned width; expected: %d, got: %d", desiredW, currentW);
            SDLTest_AssertCheck(desiredH == currentH, "Verify returned height; expected: %d, got: %d", desiredH, currentH);

            /* Get just width */
            currentW = desiredW + 1;
            SDL_GetWindowMaximumSize(window, &currentW, NULL);
            SDLTest_AssertPass("Call to SDL_GetWindowMaximumSize(&h=NULL)");
            SDLTest_AssertCheck(desiredW == currentW, "Verify returned width; expected: %d, got: %d", desiredW, currentH);

            /* Get just height */
            currentH = desiredH + 1;
            SDL_GetWindowMaximumSize(window, NULL, &currentH);
            SDLTest_AssertPass("Call to SDL_GetWindowMaximumSize(&w=NULL)");
            SDLTest_AssertCheck(desiredH == currentH, "Verify returned height; expected: %d, got: %d", desiredW, currentH);
        }
    }

    /* Dummy call with both pointers NULL */
    SDL_GetWindowMaximumSize(window, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowMaximumSize(&w=NULL,&h=NULL)");

    /* Negative tests for parameter input */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    for (desiredH = -2; desiredH < 2; desiredH++) {
        for (desiredW = -2; desiredW < 2; desiredW++) {
            if (desiredW < 0 || desiredH < 0) {
                SDL_SetWindowMaximumSize(window, desiredW, desiredH);
                SDLTest_AssertPass("Call to SDL_SetWindowMaximumSize(...,%d,%d)", desiredW, desiredH);
                checkInvalidParameterError();
            }
        }
    }

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

    /* Set some 'magic' value for later check that nothing was changed */
    referenceW = SDLTest_RandomSint32();
    referenceH = SDLTest_RandomSint32();
    currentW = referenceW;
    currentH = referenceH;
    desiredW = SDLTest_RandomSint32();
    desiredH = SDLTest_RandomSint32();

    /* Negative tests */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    SDL_GetWindowMaximumSize(NULL, &currentW, &currentH);
    SDLTest_AssertPass("Call to SDL_GetWindowMaximumSize(window=NULL)");
    SDLTest_AssertCheck(
        currentW == referenceW && currentH == referenceH,
        "Verify that content of W and H pointers has not been modified; expected: %d,%d; got: %d,%d",
        referenceW, referenceH,
        currentW, currentH);
    checkInvalidWindowError();

    SDL_GetWindowMaximumSize(NULL, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowMaximumSize(NULL, NULL, NULL)");
    checkInvalidWindowError();

    SDL_SetWindowMaximumSize(NULL, desiredW, desiredH);
    SDLTest_AssertPass("Call to SDL_SetWindowMaximumSize(window=NULL)");
    checkInvalidWindowError();

    return TEST_COMPLETED;
}

/**
 * \brief Tests call to SDL_SetWindowData and SDL_GetWindowData
 *
 * \sa SDL_SetWindowData
 * \sa SDL_GetWindowData
 */
static int video_getSetWindowData(void *arg)
{
    int returnValue = TEST_COMPLETED;
    const char *title = "video_setGetWindowData Test Window";
    SDL_Window *window;
    const char *referenceName = "TestName";
    const char *name = "TestName";
    const char *referenceName2 = "TestName2";
    const char *name2 = "TestName2";
    int datasize;
    char *referenceUserdata = NULL;
    char *userdata = NULL;
    char *referenceUserdata2 = NULL;
    char *userdata2 = NULL;
    char *result;
    int iteration;

    /* Call against new test window */
    window = createVideoSuiteTestWindow(title);
    if (window == NULL) {
        return TEST_ABORTED;
    }

    /* Create testdata */
    datasize = SDLTest_RandomIntegerInRange(1, 32);
    referenceUserdata = SDLTest_RandomAsciiStringOfSize(datasize);
    if (referenceUserdata == NULL) {
        returnValue = TEST_ABORTED;
        goto cleanup;
    }
    userdata = SDL_strdup(referenceUserdata);
    if (userdata == NULL) {
        returnValue = TEST_ABORTED;
        goto cleanup;
    }
    datasize = SDLTest_RandomIntegerInRange(1, 32);
    referenceUserdata2 = SDLTest_RandomAsciiStringOfSize(datasize);
    if (referenceUserdata2 == NULL) {
        returnValue = TEST_ABORTED;
        goto cleanup;
    }
    userdata2 = SDL_strdup(referenceUserdata2);
    if (userdata2 == NULL) {
        returnValue = TEST_ABORTED;
        goto cleanup;
    }

    /* Get non-existent data */
    result = (char *)SDL_GetWindowData(window, name);
    SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s)", name);
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);

    /* Set data */
    result = (char *)SDL_SetWindowData(window, name, userdata);
    SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,%s)", name, userdata);
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);

    /* Get data (twice) */
    for (iteration = 1; iteration <= 2; iteration++) {
        result = (char *)SDL_GetWindowData(window, name);
        SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s) [iteration %d]", name, iteration);
        SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata, result);
        SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    }

    /* Set data again twice */
    for (iteration = 1; iteration <= 2; iteration++) {
        result = (char *)SDL_SetWindowData(window, name, userdata);
        SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,%s) [iteration %d]", name, userdata, iteration);
        SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata, result);
        SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
        SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);
    }

    /* Get data again */
    result = (char *)SDL_GetWindowData(window, name);
    SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s) [again]", name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata, result);
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);

    /* Set data with new data */
    result = (char *)SDL_SetWindowData(window, name, userdata2);
    SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,%s) [new userdata]", name, userdata2);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata, result);
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, userdata2) == 0, "Validate that userdata2 was not changed, expected: %s, got: %s", referenceUserdata2, userdata2);

    /* Set data with new data again */
    result = (char *)SDL_SetWindowData(window, name, userdata2);
    SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,%s) [new userdata again]", name, userdata2);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata2, result);
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, userdata2) == 0, "Validate that userdata2 was not changed, expected: %s, got: %s", referenceUserdata2, userdata2);

    /* Get new data */
    result = (char *)SDL_GetWindowData(window, name);
    SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s)", name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata2, result);
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);

    /* Set data with NULL to clear */
    result = (char *)SDL_SetWindowData(window, name, NULL);
    SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,NULL)", name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata2, result);
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, userdata2) == 0, "Validate that userdata2 was not changed, expected: %s, got: %s", referenceUserdata2, userdata2);

    /* Set data with NULL to clear again */
    result = (char *)SDL_SetWindowData(window, name, NULL);
    SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,NULL) [again]", name);
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata2, userdata2) == 0, "Validate that userdata2 was not changed, expected: %s, got: %s", referenceUserdata2, userdata2);

    /* Get non-existent data */
    result = (char *)SDL_GetWindowData(window, name);
    SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s)", name);
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);

    /* Get non-existent data new name */
    result = (char *)SDL_GetWindowData(window, name2);
    SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s)", name2);
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    SDLTest_AssertCheck(SDL_strcmp(referenceName2, name2) == 0, "Validate that name2 was not changed, expected: %s, got: %s", referenceName2, name2);

    /* Set data (again) */
    result = (char *)SDL_SetWindowData(window, name, userdata);
    SDLTest_AssertPass("Call to SDL_SetWindowData(...%s,%s) [again, after clear]", name, userdata);
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, userdata) == 0, "Validate that userdata was not changed, expected: %s, got: %s", referenceUserdata, userdata);

    /* Get data (again) */
    result = (char *)SDL_GetWindowData(window, name);
    SDLTest_AssertPass("Call to SDL_GetWindowData(..,%s) [again, after clear]", name);
    SDLTest_AssertCheck(SDL_strcmp(referenceUserdata, result) == 0, "Validate that correct result was returned; expected: %s, got: %s", referenceUserdata, result);
    SDLTest_AssertCheck(SDL_strcmp(referenceName, name) == 0, "Validate that name was not changed, expected: %s, got: %s", referenceName, name);

    /* Negative test */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    /* Set with invalid window */
    result = (char *)SDL_SetWindowData(NULL, name, userdata);
    SDLTest_AssertPass("Call to SDL_SetWindowData(window=NULL)");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidWindowError();

    /* Set data with NULL name, valid userdata */
    result = (char *)SDL_SetWindowData(window, NULL, userdata);
    SDLTest_AssertPass("Call to SDL_SetWindowData(name=NULL)");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidParameterError();

    /* Set data with empty name, valid userdata */
    result = (char *)SDL_SetWindowData(window, "", userdata);
    SDLTest_AssertPass("Call to SDL_SetWindowData(name='')");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidParameterError();

    /* Set data with NULL name, NULL userdata */
    result = (char *)SDL_SetWindowData(window, NULL, NULL);
    SDLTest_AssertPass("Call to SDL_SetWindowData(name=NULL,userdata=NULL)");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidParameterError();

    /* Set data with empty name, NULL userdata */
    result = (char *)SDL_SetWindowData(window, "", NULL);
    SDLTest_AssertPass("Call to SDL_SetWindowData(name='',userdata=NULL)");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidParameterError();

    /* Get with invalid window */
    result = (char *)SDL_GetWindowData(NULL, name);
    SDLTest_AssertPass("Call to SDL_GetWindowData(window=NULL)");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidWindowError();

    /* Get data with NULL name */
    result = (char *)SDL_GetWindowData(window, NULL);
    SDLTest_AssertPass("Call to SDL_GetWindowData(name=NULL)");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidParameterError();

    /* Get data with empty name */
    result = (char *)SDL_GetWindowData(window, "");
    SDLTest_AssertPass("Call to SDL_GetWindowData(name='')");
    SDLTest_AssertCheck(result == NULL, "Validate that result is NULL");
    checkInvalidParameterError();

    /* Clean up */
    destroyVideoSuiteTestWindow(window);

cleanup:
    SDL_free(referenceUserdata);
    SDL_free(referenceUserdata2);
    SDL_free(userdata);
    SDL_free(userdata2);

    return returnValue;
}

/**
 * \brief Tests the functionality of the SDL_WINDOWPOS_CENTERED_DISPLAY along with SDL_WINDOW_FULLSCREEN.
 *
 * Especially useful when run on a multi-monitor system with different DPI scales per monitor,
 * to test that the window size is maintained when moving between monitors.
 *
 * As the Wayland windowing protocol does not allow application windows to control their position in the
 * desktop space, coupled with the general asynchronous nature of Wayland compositors, the positioning
 * tests don't work in windowed mode and are unreliable in fullscreen mode, thus are disabled when using
 * the Wayland video driver. All that can be done is check that the windows are the expected size.
 */
static int video_setWindowCenteredOnDisplay(void *arg)
{
    SDL_DisplayID *displays;
    SDL_Window *window;
    const char *title = "video_setWindowCenteredOnDisplay Test Window";
    int x, y, w, h;
    int xVariation, yVariation;
    int displayNum;
    int result;
    SDL_Rect display0, display1;
    SDL_bool video_driver_is_wayland = SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0;

    displays = SDL_GetDisplays(&displayNum);
    if (displays) {

        /* Get display bounds */
        result = SDL_GetDisplayBounds(displays[0 % displayNum], &display0);
        SDLTest_AssertPass("SDL_GetDisplayBounds()");
        SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);
        if (result != 0) {
            return TEST_ABORTED;
        }

        result = SDL_GetDisplayBounds(displays[1 % displayNum], &display1);
        SDLTest_AssertPass("SDL_GetDisplayBounds()");
        SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);
        if (result != 0) {
            return TEST_ABORTED;
        }

        for (xVariation = 0; xVariation < 2; xVariation++) {
            for (yVariation = 0; yVariation < 2; yVariation++) {
                int currentX = 0, currentY = 0;
                int currentW = 0, currentH = 0;
                int expectedX = 0, expectedY = 0;
                int currentDisplay;
                int expectedDisplay;
                SDL_Rect expectedDisplayRect;

                /* xVariation is the display we start on */
                expectedDisplay = displays[xVariation % displayNum];
                x = SDL_WINDOWPOS_CENTERED_DISPLAY(expectedDisplay);
                y = SDL_WINDOWPOS_CENTERED_DISPLAY(expectedDisplay);
                w = SDLTest_RandomIntegerInRange(640, 800);
                h = SDLTest_RandomIntegerInRange(400, 600);
                expectedDisplayRect = (xVariation == 0) ? display0 : display1;
                expectedX = (expectedDisplayRect.x + ((expectedDisplayRect.w - w) / 2));
                expectedY = (expectedDisplayRect.y + ((expectedDisplayRect.h - h) / 2));

                window = SDL_CreateWindowWithPosition(title, x, y, w, h, 0);
                SDLTest_AssertPass("Call to SDL_CreateWindow('Title',%d,%d,%d,%d,SHOWN)", x, y, w, h);
                SDLTest_AssertCheck(window != NULL, "Validate that returned window struct is not NULL");

                /* Wayland windows require that a frame be presented before they are fully mapped and visible onscreen. */
                if (video_driver_is_wayland) {
                    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL, 0);

                    if (renderer) {
                        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
                        SDL_RenderClear(renderer);
                        SDL_RenderPresent(renderer);

                        /* Some desktops don't display the window immediately after presentation,
                         * so delay to give the window time to actually appear on the desktop.
                         */
                        SDL_Delay(100);
                    } else {
                        SDLTest_Log("Unable to create a renderer, tests may fail under Wayland");
                    }
                }

                /* Check the window is centered on the requested display */
                currentDisplay = SDL_GetDisplayForWindow(window);
                SDL_GetWindowSize(window, &currentW, &currentH);
                SDL_GetWindowPosition(window, &currentX, &currentY);

                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentDisplay == expectedDisplay, "Validate display ID (current: %d, expected: %d)", currentDisplay, expectedDisplay);
                } else {
                    SDLTest_Log("Skipping display ID validation: Wayland driver does not support window positioning");
                }
                SDLTest_AssertCheck(currentW == w, "Validate width (current: %d, expected: %d)", currentW, w);
                SDLTest_AssertCheck(currentH == h, "Validate height (current: %d, expected: %d)", currentH, h);
                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentX == expectedX, "Validate x (current: %d, expected: %d)", currentX, expectedX);
                    SDLTest_AssertCheck(currentY == expectedY, "Validate y (current: %d, expected: %d)", currentY, expectedY);
                } else {
                    SDLTest_Log("Skipping window position validation: Wayland driver does not support window positioning");
                }

                /* Enter fullscreen desktop */
                SDL_SetWindowPosition(window, x, y);
                result = SDL_SetWindowFullscreen(window, SDL_TRUE);
                SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);

                /* Check we are filling the full display */
                currentDisplay = SDL_GetDisplayForWindow(window);
                SDL_GetWindowSize(window, &currentW, &currentH);
                SDL_GetWindowPosition(window, &currentX, &currentY);

                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentDisplay == expectedDisplay, "Validate display ID (current: %d, expected: %d)", currentDisplay, expectedDisplay);
                } else {
                    SDLTest_Log("Skipping display ID validation: Wayland driver does not support window positioning");
                }
                SDLTest_AssertCheck(currentW == expectedDisplayRect.w, "Validate width (current: %d, expected: %d)", currentW, expectedDisplayRect.w);
                SDLTest_AssertCheck(currentH == expectedDisplayRect.h, "Validate height (current: %d, expected: %d)", currentH, expectedDisplayRect.h);
                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentX == expectedDisplayRect.x, "Validate x (current: %d, expected: %d)", currentX, expectedDisplayRect.x);
                    SDLTest_AssertCheck(currentY == expectedDisplayRect.y, "Validate y (current: %d, expected: %d)", currentY, expectedDisplayRect.y);
                } else {
                    SDLTest_Log("Skipping window position validation: Wayland driver does not support window positioning");
                }

                /* Leave fullscreen desktop */
                result = SDL_SetWindowFullscreen(window, SDL_FALSE);
                SDLTest_AssertCheck(result == 0, "Verify return value; expected: 0, got: %d", result);

                /* Check window was restored correctly */
                currentDisplay = SDL_GetDisplayForWindow(window);
                SDL_GetWindowSize(window, &currentW, &currentH);
                SDL_GetWindowPosition(window, &currentX, &currentY);

                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentDisplay == expectedDisplay, "Validate display index (current: %d, expected: %d)", currentDisplay, expectedDisplay);
                } else {
                    SDLTest_Log("Skipping display ID validation: Wayland driver does not support window positioning");
                }
                SDLTest_AssertCheck(currentW == w, "Validate width (current: %d, expected: %d)", currentW, w);
                SDLTest_AssertCheck(currentH == h, "Validate height (current: %d, expected: %d)", currentH, h);
                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentX == expectedX, "Validate x (current: %d, expected: %d)", currentX, expectedX);
                    SDLTest_AssertCheck(currentY == expectedY, "Validate y (current: %d, expected: %d)", currentY, expectedY);
                } else {
                    SDLTest_Log("Skipping window position validation: Wayland driver does not support window positioning");
                }

                /* Center on display yVariation, and check window properties */

                expectedDisplay = displays[yVariation % displayNum];
                x = SDL_WINDOWPOS_CENTERED_DISPLAY(expectedDisplay);
                y = SDL_WINDOWPOS_CENTERED_DISPLAY(expectedDisplay);
                expectedDisplayRect = (yVariation == 0) ? display0 : display1;
                expectedX = (expectedDisplayRect.x + ((expectedDisplayRect.w - w) / 2));
                expectedY = (expectedDisplayRect.y + ((expectedDisplayRect.h - h) / 2));
                SDL_SetWindowPosition(window, x, y);

                currentDisplay = SDL_GetDisplayForWindow(window);
                SDL_GetWindowSize(window, &currentW, &currentH);
                SDL_GetWindowPosition(window, &currentX, &currentY);

                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentDisplay == expectedDisplay, "Validate display ID (current: %d, expected: %d)", currentDisplay, expectedDisplay);
                } else {
                    SDLTest_Log("Skipping display ID validation: Wayland driver does not support window positioning");
                }
                SDLTest_AssertCheck(currentW == w, "Validate width (current: %d, expected: %d)", currentW, w);
                SDLTest_AssertCheck(currentH == h, "Validate height (current: %d, expected: %d)", currentH, h);
                if (!video_driver_is_wayland) {
                    SDLTest_AssertCheck(currentX == expectedX, "Validate x (current: %d, expected: %d)", currentX, expectedX);
                    SDLTest_AssertCheck(currentY == expectedY, "Validate y (current: %d, expected: %d)", currentY, expectedY);
                } else {
                    SDLTest_Log("Skipping window position validation: Wayland driver does not support window positioning");
                }

                /* Clean up */
                destroyVideoSuiteTestWindow(window);
            }
        }

        SDL_free(displays);
    }

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Video test cases */
static const SDLTest_TestCaseReference videoTest1 = {
    (SDLTest_TestCaseFp)video_enableDisableScreensaver, "video_enableDisableScreensaver", "Enable and disable screenaver while checking state", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest2 = {
    (SDLTest_TestCaseFp)video_createWindowVariousSizes, "video_createWindowVariousSizes", "Create windows with various sizes", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest3 = {
    (SDLTest_TestCaseFp)video_createWindowVariousFlags, "video_createWindowVariousFlags", "Create windows using various flags", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest4 = {
    (SDLTest_TestCaseFp)video_getWindowFlags, "video_getWindowFlags", "Get window flags set during SDL_CreateWindow", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest5 = {
    (SDLTest_TestCaseFp)video_getFullscreenDisplayModes, "video_getFullscreenDisplayModes", "Use SDL_GetFullscreenDisplayModes function to get number of display modes", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest6 = {
    (SDLTest_TestCaseFp)video_getClosestDisplayModeCurrentResolution, "video_getClosestDisplayModeCurrentResolution", "Use function to get closes match to requested display mode for current resolution", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest7 = {
    (SDLTest_TestCaseFp)video_getClosestDisplayModeRandomResolution, "video_getClosestDisplayModeRandomResolution", "Use function to get closes match to requested display mode for random resolution", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest8 = {
    (SDLTest_TestCaseFp)video_getWindowDisplayMode, "video_getWindowDisplayMode", "Get window display mode", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest9 = {
    (SDLTest_TestCaseFp)video_getWindowDisplayModeNegative, "video_getWindowDisplayModeNegative", "Get window display mode with invalid input", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest10 = {
    (SDLTest_TestCaseFp)video_getSetWindowGrab, "video_getSetWindowGrab", "Checks SDL_GetWindowGrab and SDL_SetWindowGrab positive and negative cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest11 = {
    (SDLTest_TestCaseFp)video_getWindowId, "video_getWindowId", "Checks SDL_GetWindowID and SDL_GetWindowFromID", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest12 = {
    (SDLTest_TestCaseFp)video_getWindowPixelFormat, "video_getWindowPixelFormat", "Checks SDL_GetWindowPixelFormat", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest13 = {
    (SDLTest_TestCaseFp)video_getSetWindowPosition, "video_getSetWindowPosition", "Checks SDL_GetWindowPosition and SDL_SetWindowPosition positive and negative cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest14 = {
    (SDLTest_TestCaseFp)video_getSetWindowSize, "video_getSetWindowSize", "Checks SDL_GetWindowSize and SDL_SetWindowSize positive and negative cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest15 = {
    (SDLTest_TestCaseFp)video_getSetWindowMinimumSize, "video_getSetWindowMinimumSize", "Checks SDL_GetWindowMinimumSize and SDL_SetWindowMinimumSize positive and negative cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest16 = {
    (SDLTest_TestCaseFp)video_getSetWindowMaximumSize, "video_getSetWindowMaximumSize", "Checks SDL_GetWindowMaximumSize and SDL_SetWindowMaximumSize positive and negative cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest17 = {
    (SDLTest_TestCaseFp)video_getSetWindowData, "video_getSetWindowData", "Checks SDL_SetWindowData and SDL_GetWindowData positive and negative cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference videoTest18 = {
    (SDLTest_TestCaseFp)video_setWindowCenteredOnDisplay, "video_setWindowCenteredOnDisplay", "Checks using SDL_WINDOWPOS_CENTERED_DISPLAY centers the window on a display", TEST_ENABLED
};

/* Sequence of Video test cases */
static const SDLTest_TestCaseReference *videoTests[] = {
    &videoTest1, &videoTest2, &videoTest3, &videoTest4, &videoTest5, &videoTest6,
    &videoTest7, &videoTest8, &videoTest9, &videoTest10, &videoTest11, &videoTest12,
    &videoTest13, &videoTest14, &videoTest15, &videoTest16, &videoTest17,
    &videoTest18, NULL
};

/* Video test suite (global) */
SDLTest_TestSuiteReference videoTestSuite = {
    "Video",
    NULL,
    videoTests,
    NULL
};
