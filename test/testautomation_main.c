/**
 * Automated SDL subsystems management test.
 *
 * Written by J�rgen Tjern� "jorgenpt"
 *
 * Released under Public Domain.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"
#ifdef HAVE_BUILD_CONFIG
#include "SDL_build_config.h"
#endif

/**
 * Tests SDL_InitSubSystem() and SDL_QuitSubSystem()
 * \sa SDL_Init
 * \sa SDL_Quit
 */
static int SDLCALL main_testInitQuitSubSystem(void *arg)
{
    int i;
    int subsystems[] = { SDL_INIT_JOYSTICK, SDL_INIT_HAPTIC, SDL_INIT_GAMEPAD };

    for (i = 0; i < SDL_arraysize(subsystems); ++i) {
        int initialized_system;
        int subsystem = subsystems[i];

        SDLTest_AssertCheck((SDL_WasInit(subsystem) & subsystem) == 0, "SDL_WasInit(%x) before init should be false", subsystem);
        SDLTest_AssertCheck(SDL_InitSubSystem(subsystem), "SDL_InitSubSystem(%x)", subsystem);

        initialized_system = SDL_WasInit(subsystem);
        SDLTest_AssertCheck((initialized_system & subsystem) != 0, "SDL_WasInit(%x) should be true (%x)", subsystem, initialized_system);

        SDL_QuitSubSystem(subsystem);

        SDLTest_AssertCheck((SDL_WasInit(subsystem) & subsystem) == 0, "SDL_WasInit(%x) after shutdown should be false", subsystem);
    }

    return TEST_COMPLETED;
}

static const int joy_and_controller = SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
static int SDLCALL main_testImpliedJoystickInit(void *arg)
{
    int initialized_system;

    /* First initialize the controller */
    SDLTest_AssertCheck((SDL_WasInit(joy_and_controller) & joy_and_controller) == 0, "SDL_WasInit() before init should be false for joystick & controller");
    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMEPAD), "SDL_InitSubSystem(SDL_INIT_GAMEPAD)");

    /* Then make sure this implicitly initialized the joystick subsystem */
    initialized_system = SDL_WasInit(joy_and_controller);
    SDLTest_AssertCheck((initialized_system & joy_and_controller) == joy_and_controller, "SDL_WasInit() should be true for joystick & controller (%x)", initialized_system);

    /* Then quit the controller, and make sure that implicitly also quits the */
    /* joystick subsystem */
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    initialized_system = SDL_WasInit(joy_and_controller);
    SDLTest_AssertCheck((initialized_system & joy_and_controller) == 0, "SDL_WasInit() should be false for joystick & controller (%x)", initialized_system);

    return TEST_COMPLETED;
}

static int SDLCALL main_testImpliedJoystickQuit(void *arg)
{
    int initialized_system;

    /* First initialize the controller and the joystick (explicitly) */
    SDLTest_AssertCheck((SDL_WasInit(joy_and_controller) & joy_and_controller) == 0, "SDL_WasInit() before init should be false for joystick & controller");
    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_JOYSTICK), "SDL_InitSubSystem(SDL_INIT_JOYSTICK)");
    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMEPAD), "SDL_InitSubSystem(SDL_INIT_GAMEPAD)");

    /* Then make sure they're both initialized properly */
    initialized_system = SDL_WasInit(joy_and_controller);
    SDLTest_AssertCheck((initialized_system & joy_and_controller) == joy_and_controller, "SDL_WasInit() should be true for joystick & controller (%x)", initialized_system);

    /* Then quit the controller, and make sure that it does NOT quit the */
    /* explicitly initialized joystick subsystem. */
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    initialized_system = SDL_WasInit(joy_and_controller);
    SDLTest_AssertCheck((initialized_system & joy_and_controller) == SDL_INIT_JOYSTICK, "SDL_WasInit() should be false for joystick & controller (%x)", initialized_system);

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

    return TEST_COMPLETED;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-zero-length"
#endif

static int SDLCALL
main_testSetError(void *arg)
{
    size_t i;
    char error_input[1024];
    int result;
    const char *error;
    size_t expected_len;

    SDLTest_AssertPass("SDL_SetError(NULL)");
    result = SDL_SetError(NULL);
    SDLTest_AssertCheck(result == false, "SDL_SetError(NULL) -> %d (expected %d)", result, false);
    error = SDL_GetError();
    SDLTest_AssertCheck(SDL_strcmp(error, "") == 0, "SDL_GetError() -> \"%s\" (expected \"%s\")", error, "");

    SDLTest_AssertPass("SDL_SetError(\"\")");
    result = SDL_SetError("");
    SDLTest_AssertCheck(result == false, "SDL_SetError(\"\") -> %d (expected %d)", result, false);
    error = SDL_GetError();
    SDLTest_AssertCheck(SDL_strcmp(error, "") == 0, "SDL_GetError() -> \"%s\" (expected \"%s\")", error, "");

    error_input[0] = '\0';
    for (i = 0; i < (sizeof(error_input) - 1); ++i) {
        error_input[i] = 'a' + (i % 26);
    }
    error_input[i] = '\0';
    SDLTest_AssertPass("SDL_SetError(\"abc...\")");
    result = SDL_SetError("%s", error_input);
    SDLTest_AssertCheck(result == false, "SDL_SetError(\"abc...\") -> %d (expected %d)", result, false);
    error = SDL_GetError();

#ifdef SDL_THREADS_DISABLED
    expected_len = 128 - 1;
#else
    expected_len = sizeof(error_input) - 1;
#endif
    SDLTest_AssertPass("Verify SDL error is identical to the input error");
    SDLTest_CompareMemory(error, SDL_strlen(error), error_input, expected_len);

    return TEST_COMPLETED;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static const SDLTest_TestCaseReference mainTest1 = {
    main_testInitQuitSubSystem, "main_testInitQuitSubSystem", "Tests SDL_InitSubSystem/QuitSubSystem", TEST_ENABLED
};

static const SDLTest_TestCaseReference mainTest2 = {
    main_testImpliedJoystickInit, "main_testImpliedJoystickInit", "Tests that init for gamecontroller properly implies joystick", TEST_ENABLED
};

static const SDLTest_TestCaseReference mainTest3 = {
    main_testImpliedJoystickQuit, "main_testImpliedJoystickQuit", "Tests that quit for gamecontroller doesn't quit joystick if you inited it explicitly", TEST_ENABLED
};

static const SDLTest_TestCaseReference mainTest4 = {
    main_testSetError, "main_testSetError", "Tests that SDL_SetError() handles arbitrarily large strings", TEST_ENABLED
};

/* Sequence of Main test cases */
static const SDLTest_TestCaseReference *mainTests[] = {
    &mainTest1,
    &mainTest2,
    &mainTest3,
    &mainTest4,
    NULL
};

/* Main test suite (global) */
SDLTest_TestSuiteReference mainTestSuite = {
    "Main",
    NULL,
    mainTests,
    NULL
};
