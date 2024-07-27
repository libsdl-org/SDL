/**
 * Events test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Test case functions */

/* Flag indicating if the userdata should be checked */
static int g_userdataCheck = 0;

/* Userdata value to check */
static int g_userdataValue = 0;

/* Flag indicating that the filter was called */
static int g_eventFilterCalled = 0;

/* Userdata values for event */
static int g_userdataValue1 = 1;
static int g_userdataValue2 = 2;

/* Event filter that sets some flags and optionally checks userdata */
static int SDLCALL events_sampleNullEventFilter(void *userdata, SDL_Event *event)
{
    g_eventFilterCalled = 1;

    if (g_userdataCheck != 0) {
        SDLTest_AssertCheck(userdata != NULL, "Check userdata pointer, expected: non-NULL, got: %s", (userdata != NULL) ? "non-NULL" : "NULL");
        if (userdata != NULL) {
            SDLTest_AssertCheck(*(int *)userdata == g_userdataValue, "Check userdata value, expected: %i, got: %i", g_userdataValue, *(int *)userdata);
        }
    }

    return 0;
}

/**
 * Test pumping and peeking events.
 *
 * \sa SDL_PumpEvents
 * \sa SDL_PollEvent
 */
static int events_pushPumpAndPollUserevent(void *arg)
{
    SDL_Event event1;
    SDL_Event event2;
    int result;

    /* Create user event */
    event1.type = SDL_EVENT_USER;
    event1.common.timestamp = 0;
    event1.user.code = SDLTest_RandomSint32();
    event1.user.data1 = (void *)&g_userdataValue1;
    event1.user.data2 = (void *)&g_userdataValue2;

    /* Push a user event onto the queue and force queue update */
    SDL_PushEvent(&event1);
    SDLTest_AssertPass("Call to SDL_PushEvent()");
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");

    /* Poll for user event */
    result = SDL_PollEvent(&event2);
    SDLTest_AssertPass("Call to SDL_PollEvent()");
    SDLTest_AssertCheck(result == 1, "Check result from SDL_PollEvent, expected: 1, got: %d", result);

    /* Need to finish getting all events and sentinel, otherwise other tests that rely on event are in bad state */
    while (SDL_PollEvent(&event2)) {
    }

    return TEST_COMPLETED;
}

/**
 * Adds and deletes an event watch function with NULL userdata
 *
 * \sa SDL_AddEventWatch
 * \sa SDL_DelEventWatch
 *
 */
static int events_addDelEventWatch(void *arg)
{
    SDL_Event event;

    /* Create user event */
    event.type = SDL_EVENT_USER;
    event.common.timestamp = 0;
    event.user.code = SDLTest_RandomSint32();
    event.user.data1 = (void *)&g_userdataValue1;
    event.user.data2 = (void *)&g_userdataValue2;

    /* Disable userdata check */
    g_userdataCheck = 0;

    /* Reset event filter call tracker */
    g_eventFilterCalled = 0;

    /* Add watch */
    SDL_AddEventWatch(events_sampleNullEventFilter, NULL);
    SDLTest_AssertPass("Call to SDL_AddEventWatch()");

    /* Push a user event onto the queue and force queue update */
    SDL_PushEvent(&event);
    SDLTest_AssertPass("Call to SDL_PushEvent()");
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");
    SDLTest_AssertCheck(g_eventFilterCalled == 1, "Check that event filter was called");

    /* Delete watch */
    SDL_DelEventWatch(events_sampleNullEventFilter, NULL);
    SDLTest_AssertPass("Call to SDL_DelEventWatch()");

    /* Push a user event onto the queue and force queue update */
    g_eventFilterCalled = 0;
    SDL_PushEvent(&event);
    SDLTest_AssertPass("Call to SDL_PushEvent()");
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");
    SDLTest_AssertCheck(g_eventFilterCalled == 0, "Check that event filter was NOT called");

    return TEST_COMPLETED;
}

/**
 * Adds and deletes an event watch function with userdata
 *
 * \sa SDL_AddEventWatch
 * \sa SDL_DelEventWatch
 *
 */
static int events_addDelEventWatchWithUserdata(void *arg)
{
    SDL_Event event;

    /* Create user event */
    event.type = SDL_EVENT_USER;
    event.common.timestamp = 0;
    event.user.code = SDLTest_RandomSint32();
    event.user.data1 = (void *)&g_userdataValue1;
    event.user.data2 = (void *)&g_userdataValue2;

    /* Enable userdata check and set a value to check */
    g_userdataCheck = 1;
    g_userdataValue = SDLTest_RandomIntegerInRange(-1024, 1024);

    /* Reset event filter call tracker */
    g_eventFilterCalled = 0;

    /* Add watch */
    SDL_AddEventWatch(events_sampleNullEventFilter, (void *)&g_userdataValue);
    SDLTest_AssertPass("Call to SDL_AddEventWatch()");

    /* Push a user event onto the queue and force queue update */
    SDL_PushEvent(&event);
    SDLTest_AssertPass("Call to SDL_PushEvent()");
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");
    SDLTest_AssertCheck(g_eventFilterCalled == 1, "Check that event filter was called");

    /* Delete watch */
    SDL_DelEventWatch(events_sampleNullEventFilter, (void *)&g_userdataValue);
    SDLTest_AssertPass("Call to SDL_DelEventWatch()");

    /* Push a user event onto the queue and force queue update */
    g_eventFilterCalled = 0;
    SDL_PushEvent(&event);
    SDLTest_AssertPass("Call to SDL_PushEvent()");
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");
    SDLTest_AssertCheck(g_eventFilterCalled == 0, "Check that event filter was NOT called");

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Events test cases */
static const SDLTest_TestCaseReference eventsTest1 = {
    (SDLTest_TestCaseFp)events_pushPumpAndPollUserevent, "events_pushPumpAndPollUserevent", "Pushes, pumps and polls a user event", TEST_ENABLED
};

static const SDLTest_TestCaseReference eventsTest2 = {
    (SDLTest_TestCaseFp)events_addDelEventWatch, "events_addDelEventWatch", "Adds and deletes an event watch function with NULL userdata", TEST_ENABLED
};

static const SDLTest_TestCaseReference eventsTest3 = {
    (SDLTest_TestCaseFp)events_addDelEventWatchWithUserdata, "events_addDelEventWatchWithUserdata", "Adds and deletes an event watch function with userdata", TEST_ENABLED
};

/* Sequence of Events test cases */
static const SDLTest_TestCaseReference *eventsTests[] = {
    &eventsTest1, &eventsTest2, &eventsTest3, NULL
};

/* Events test suite (global) */
SDLTest_TestSuiteReference eventsTestSuite = {
    "Events",
    NULL,
    eventsTests,
    NULL
};
