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

/**
 * Tests SDL_InitSubSystem() and SDL_QuitSubSystem()
 * \sa SDL_Init
 * \sa SDL_Quit
 */
static int main_testInitQuitSubSystem(void *arg)
{
    int i;
    int subsystems[] = { SDL_INIT_JOYSTICK, SDL_INIT_HAPTIC, SDL_INIT_GAMEPAD };

    for (i = 0; i < SDL_arraysize(subsystems); ++i) {
        int initialized_system;
        int subsystem = subsystems[i];

        SDLTest_AssertCheck((SDL_WasInit(subsystem) & subsystem) == 0, "SDL_WasInit(%x) before init should be false", subsystem);
        SDLTest_AssertCheck(SDL_InitSubSystem(subsystem) == 0, "SDL_InitSubSystem(%x)", subsystem);

        initialized_system = SDL_WasInit(subsystem);
        SDLTest_AssertCheck((initialized_system & subsystem) != 0, "SDL_WasInit(%x) should be true (%x)", subsystem, initialized_system);

        SDL_QuitSubSystem(subsystem);

        SDLTest_AssertCheck((SDL_WasInit(subsystem) & subsystem) == 0, "SDL_WasInit(%x) after shutdown should be false", subsystem);
    }

    return TEST_COMPLETED;
}

static const int joy_and_controller = SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
static int main_testImpliedJoystickInit(void *arg)
{
    int initialized_system;

    /* First initialize the controller */
    SDLTest_AssertCheck((SDL_WasInit(joy_and_controller) & joy_and_controller) == 0, "SDL_WasInit() before init should be false for joystick & controller");
    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMEPAD) == 0, "SDL_InitSubSystem(SDL_INIT_GAMEPAD)");

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

static int main_testImpliedJoystickQuit(void *arg)
{
    int initialized_system;

    /* First initialize the controller and the joystick (explicitly) */
    SDLTest_AssertCheck((SDL_WasInit(joy_and_controller) & joy_and_controller) == 0, "SDL_WasInit() before init should be false for joystick & controller");
    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0, "SDL_InitSubSystem(SDL_INIT_JOYSTICK)");
    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMEPAD) == 0, "SDL_InitSubSystem(SDL_INIT_GAMEPAD)");

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

static int
main_testSetError(void *arg)
{
    size_t i;
    char error[1024];

    error[0] = '\0';
    SDL_SetError("");
    SDLTest_AssertCheck(SDL_strcmp(error, SDL_GetError()) == 0, "SDL_SetError(\"\")");

    for (i = 0; i < (sizeof(error) - 1); ++i) {
        error[i] = 'a' + (i % 26);
    }
    error[i] = '\0';
    SDL_SetError("%s", error);
    SDLTest_AssertCheck(SDL_strcmp(error, SDL_GetError()) == 0, "SDL_SetError(\"abc...1023\")");

    return TEST_COMPLETED;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static const SDLTest_TestCaseReference mainTest1 = {
    (SDLTest_TestCaseFp)main_testInitQuitSubSystem, "main_testInitQuitSubSystem", "Tests SDL_InitSubSystem/QuitSubSystem", TEST_ENABLED
};

static const SDLTest_TestCaseReference mainTest2 = {
    (SDLTest_TestCaseFp)main_testImpliedJoystickInit, "main_testImpliedJoystickInit", "Tests that init for gamecontroller properly implies joystick", TEST_ENABLED
};

static const SDLTest_TestCaseReference mainTest3 = {
    (SDLTest_TestCaseFp)main_testImpliedJoystickQuit, "main_testImpliedJoystickQuit", "Tests that quit for gamecontroller doesn't quit joystick if you inited it explicitly", TEST_ENABLED
};

static const SDLTest_TestCaseReference mainTest4 = {
    (SDLTest_TestCaseFp)main_testSetError, "main_testSetError", "Tests that SDL_SetError() handles arbitrarily large strings", TEST_ENABLED
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
