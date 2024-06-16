/**
 * Events test suite
 */
#include "testautomation_suites.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

/* ================= Test Case Implementation ================== */

/* Fixture */

static void subsystemsSetUp(void *arg)
{
    /* Reset each one of the SDL subsystems */
    /* CHECKME: can we use SDL_Quit here, or this will break the flow of tests? */
    SDL_Quit();
    /* Alternate variant without SDL_Quit:
        while (SDL_WasInit(0) != 0) {
            SDL_QuitSubSystem(~0U);
        }
    */
    SDLTest_AssertPass("Reset all subsystems before subsystems test");
    SDLTest_AssertCheck(SDL_WasInit(0) == 0, "Check result from SDL_WasInit(0)");
}

static void subsystemsTearDown(void *arg)
{
    /* Reset each one of the SDL subsystems */
    SDL_Quit();

    SDLTest_AssertPass("Cleanup of subsystems test completed");
}

/* Test case functions */

/**
 * Inits and Quits particular subsystem, checking its Init status.
 *
 * \sa SDL_InitSubSystem
 * \sa SDL_QuitSubSystem
 *
 */
static int subsystems_referenceCount(void *arg)
{
    const int system = SDL_INIT_VIDEO;
    int result;
    /* Ensure that we start with a non-initialized subsystem. */
    SDLTest_AssertCheck(SDL_WasInit(system) == 0, "Check result from SDL_WasInit(0x%x)", system);

    /* Init subsystem once, and quit once */
    SDL_InitSubSystem(system);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(0x%x)", system);
    result = SDL_WasInit(system);
    SDLTest_AssertCheck(result == system, "Check result from SDL_WasInit(0x%x), expected: 0x%x, got: 0x%x", system, system, result);

    SDL_QuitSubSystem(system);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(0x%x)", system);
    result = SDL_WasInit(system);
    SDLTest_AssertCheck(result == 0, "Check result from SDL_WasInit(0x%x), expected: 0, got: 0x%x", system, result);

    /* Init subsystem number of times, then decrement reference count until it's disposed of. */
    SDL_InitSubSystem(system);
    SDL_InitSubSystem(system);
    SDL_InitSubSystem(system);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(0x%x) x3 times", system);
    result = SDL_WasInit(system);
    SDLTest_AssertCheck(result == system, "Check result from SDL_WasInit(0x%x), expected: 0x%x, got: 0x%x", system, system, result);

    SDL_QuitSubSystem(system);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(0x%x) x1", system);
    result = SDL_WasInit(system);
    SDLTest_AssertCheck(result == system, "Check result from SDL_WasInit(0x%x), expected: 0x%x, got: 0x%x", system, system, result);
    SDL_QuitSubSystem(system);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(0x%x) x2", system);
    result = SDL_WasInit(system);
    SDLTest_AssertCheck(result == system, "Check result from SDL_WasInit(0x%x), expected: 0x%x, got: 0x%x", system, system, result);
    SDL_QuitSubSystem(system);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(0x%x) x3", system);
    result = SDL_WasInit(system);
    SDLTest_AssertCheck(result == 0, "Check result from SDL_WasInit(0x%x), expected: 0, got: 0x%x", system, result);

    return TEST_COMPLETED;
}

/**
 * Inits and Quits subsystems that have another as dependency;
 *        check that the dependency is not removed before the last of its dependents.
 *
 * \sa SDL_InitSubSystem
 * \sa SDL_QuitSubSystem
 *
 */
static int subsystems_dependRefCountInitAllQuitByOne(void *arg)
{
    int result;
    /* Ensure that we start with reset subsystems. */
    SDLTest_AssertCheck(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) == 0,
                        "Check result from SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS)");

    /* Following should init SDL_INIT_EVENTS and give it +3 ref counts. */
    SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);

    /* Quit systems one by one. */
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_VIDEO)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_JOYSTICK)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == 0, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0, got: 0x%x", result);

    return TEST_COMPLETED;
}

/**
 * Inits and Quits subsystems that have another as dependency;
 *        check that the dependency is not removed before the last of its dependents.
 *
 * \sa SDL_InitSubSystem
 * \sa SDL_QuitSubSystem
 *
 */
static int subsystems_dependRefCountInitByOneQuitAll(void *arg)
{
    int result;
    /* Ensure that we start with reset subsystems. */
    SDLTest_AssertCheck(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) == 0,
                        "Check result from SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS)");

    /* Following should init SDL_INIT_EVENTS and give it +3 ref counts. */
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_VIDEO)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_AUDIO)");
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_JOYSTICK)");

    /* Quit systems all at once. */
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == 0, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0, got: 0x%x", result);

    return TEST_COMPLETED;
}

/**
 * Inits and Quits subsystems that have another as dependency,
 *        but also inits that dependency explicitly, giving it extra ref count.
 *        Check that the dependency is not removed before the last reference is gone.
 *
 * \sa SDL_InitSubSystem
 * \sa SDL_QuitSubSystem
 *
 */
static int subsystems_dependRefCountWithExtraInit(void *arg)
{
    int result;
    /* Ensure that we start with reset subsystems. */
    SDLTest_AssertCheck(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) == 0,
                        "Check result from SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS)");

    /* Init EVENTS explicitly, +1 ref count. */
    SDL_InitSubSystem(SDL_INIT_EVENTS);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_EVENTS)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);
    /* Following should init SDL_INIT_EVENTS and give it +3 ref counts. */
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_VIDEO)");
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_AUDIO)");
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_JOYSTICK)");

    /* Quit EVENTS explicitly, -1 ref count. */
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_EVENTS)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);

    /* Quit systems one by one. */
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_VIDEO)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == SDL_INIT_EVENTS, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0x4000, got: 0x%x", result);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_JOYSTICK)");
    result = SDL_WasInit(SDL_INIT_EVENTS);
    SDLTest_AssertCheck(result == 0, "Check result from SDL_WasInit(SDL_INIT_EVENTS), expected: 0, got: 0x%x", result);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Subsystems test cases */
static const SDLTest_TestCaseReference subsystemsTest1 = {
    subsystems_referenceCount, "subsystems_referenceCount", "Makes sure that subsystem stays until number of quits matches inits.", TEST_ENABLED
};

static const SDLTest_TestCaseReference subsystemsTest2 = {
    subsystems_dependRefCountInitAllQuitByOne, "subsystems_dependRefCountInitAllQuitByOne", "Check reference count of subsystem dependencies.", TEST_ENABLED
};

static const SDLTest_TestCaseReference subsystemsTest3 = {
    subsystems_dependRefCountInitByOneQuitAll, "subsystems_dependRefCountInitByOneQuitAll", "Check reference count of subsystem dependencies.", TEST_ENABLED
};

static const SDLTest_TestCaseReference subsystemsTest4 = {
    subsystems_dependRefCountWithExtraInit, "subsystems_dependRefCountWithExtraInit", "Check reference count of subsystem dependencies.", TEST_ENABLED
};

/* Sequence of Events test cases */
static const SDLTest_TestCaseReference *subsystemsTests[] = {
    &subsystemsTest1, &subsystemsTest2, &subsystemsTest3, &subsystemsTest4, NULL
};

/* Events test suite (global) */
SDLTest_TestSuiteReference subsystemsTestSuite = {
    "Subsystems",
    subsystemsSetUp,
    subsystemsTests,
    subsystemsTearDown
};
