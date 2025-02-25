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

#define MAX_ITERATIONS 100

/* Event filter that sets some flags and optionally checks userdata */
static bool SDLCALL events_sampleNullEventFilter(void *userdata, SDL_Event *event)
{
    g_eventFilterCalled = 1;

    if (g_userdataCheck != 0) {
        SDLTest_AssertCheck(userdata != NULL, "Check userdata pointer, expected: non-NULL, got: %s", (userdata != NULL) ? "non-NULL" : "NULL");
        if (userdata != NULL) {
            SDLTest_AssertCheck(*(int *)userdata == g_userdataValue, "Check userdata value, expected: %i, got: %i", g_userdataValue, *(int *)userdata);
        }
    }

    return true;
}

/**
 * Test pumping and peeking events.
 *
 * \sa SDL_PumpEvents
 * \sa SDL_PollEvent
 */
static int SDLCALL events_pushPumpAndPollUserevent(void *arg)
{
    SDL_Event event_in;
    SDL_Event event_out;
    int result;
    int i;
    Sint32 ref_code = SDLTest_RandomSint32();
    SDL_Window *event_window;

    /* Flush all events */
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDLTest_AssertCheck(!SDL_HasEvents(SDL_EVENT_USER, SDL_EVENT_USER), "Check SDL_HasEvents returns false");

    /* Create user event */
    event_in.type = SDL_EVENT_USER;
    event_in.user.windowID = 0;
    event_in.common.timestamp = 0;
    event_in.user.code = ref_code;
    event_in.user.data1 = (void *)&g_userdataValue1;
    event_in.user.data2 = (void *)&g_userdataValue2;

    /* Push a user event onto the queue and force queue update */
    SDL_PushEvent(&event_in);
    SDLTest_AssertPass("Call to SDL_PushEvent()");
    SDL_PumpEvents();
    SDLTest_AssertPass("Call to SDL_PumpEvents()");

    SDLTest_AssertCheck(SDL_HasEvents(SDL_EVENT_USER, SDL_EVENT_USER), "Check SDL_HasEvents returns true");

    /* Poll until we get a user event. */
    for (i = 0; i < MAX_ITERATIONS; i++) {
        result = SDL_PollEvent(&event_out);
        SDLTest_AssertPass("Call to SDL_PollEvent()");
        SDLTest_AssertCheck(result == 1, "Check result from SDL_PollEvent, expected: 1, got: %d", result);
        if (!result) {
            break;
        }
        if (event_out.type == SDL_EVENT_USER) {
            break;
        }
    }
    SDLTest_AssertCheck(i < MAX_ITERATIONS, "Check the user event is seen in less then %d polls, got %d poll", MAX_ITERATIONS, i + 1);

    SDLTest_AssertCheck(SDL_EVENT_USER == event_out.type, "Check event type is SDL_EVENT_USER, expected: 0x%x, got: 0x%" SDL_PRIx32, SDL_EVENT_USER, event_out.type);
    SDLTest_AssertCheck(ref_code == event_out.user.code, "Check SDL_Event.user.code, expected: 0x%" SDL_PRIx32 ", got: 0x%" SDL_PRIx32 , ref_code, event_out.user.code);
    SDLTest_AssertCheck(0 == event_out.user.windowID, "Check SDL_Event.user.windowID, expected: NULL , got: %" SDL_PRIu32, event_out.user.windowID);
    SDLTest_AssertCheck((void *)&g_userdataValue1 == event_out.user.data1, "Check SDL_Event.user.data1, expected: %p, got: %p", &g_userdataValue1, event_out.user.data1);
    SDLTest_AssertCheck((void *)&g_userdataValue2 == event_out.user.data2, "Check SDL_Event.user.data2, expected: %p, got: %p", &g_userdataValue2, event_out.user.data2);
    event_window = SDL_GetWindowFromEvent(&event_out);
    SDLTest_AssertCheck(NULL == SDL_GetWindowFromEvent(&event_out), "Check SDL_GetWindowFromEvent returns the window id from a user event, expected: NULL, got: %p", event_window);

    /* Need to finish getting all events and sentinel, otherwise other tests that rely on event are in bad state */
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    return TEST_COMPLETED;
}

/**
 * Adds and deletes an event watch function with NULL userdata
 *
 * \sa SDL_AddEventWatch
 * \sa SDL_RemoveEventWatch
 *
 */
static int SDLCALL events_addDelEventWatch(void *arg)
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
    SDL_RemoveEventWatch(events_sampleNullEventFilter, NULL);
    SDLTest_AssertPass("Call to SDL_RemoveEventWatch()");

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
 * \sa SDL_RemoveEventWatch
 *
 */
static int SDLCALL events_addDelEventWatchWithUserdata(void *arg)
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
    SDL_RemoveEventWatch(events_sampleNullEventFilter, (void *)&g_userdataValue);
    SDLTest_AssertPass("Call to SDL_RemoveEventWatch()");

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
 * Runs callbacks on the main thread.
 *
 * \sa SDL_IsMainThread
 * \sa SDL_RunOnMainThread
 *
 */

typedef struct IncrementCounterData_t
{
    Uint32 delay;
    int counter;
} IncrementCounterData_t;

static void SDLCALL IncrementCounter(void *userdata)
{
    IncrementCounterData_t *data = (IncrementCounterData_t *)userdata;
    ++data->counter;
}

#ifndef SDL_PLATFORM_EMSCRIPTEN /* Emscripten doesn't have threads */
static int SDLCALL IncrementCounterThread(void *userdata)
{
    IncrementCounterData_t *data = (IncrementCounterData_t *)userdata;
    SDL_Event event;

    SDL_assert(!SDL_IsMainThread());

    if (data->delay > 0) {
        SDL_Delay(data->delay);
    }

    if (!SDL_RunOnMainThread(IncrementCounter, userdata, false)) {
        SDLTest_LogError("Couldn't run IncrementCounter asynchronously on main thread: %s", SDL_GetError());
    }
    if (!SDL_RunOnMainThread(IncrementCounter, userdata, true)) {
        SDLTest_LogError("Couldn't run IncrementCounter synchronously on main thread: %s", SDL_GetError());
    }

    /* Send an event to unblock the main thread, which is waiting in SDL_WaitEvent() */
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);

    return 0;
}
#endif /* !SDL_PLATFORM_EMSCRIPTEN */

static int SDLCALL events_mainThreadCallbacks(void *arg)
{
    IncrementCounterData_t data = { 0, 0 };

    /* Make sure we're on the main thread */
    SDLTest_AssertCheck(SDL_IsMainThread(), "Verify we're on the main thread");

    SDL_RunOnMainThread(IncrementCounter, &data, true);
    SDLTest_AssertCheck(data.counter == 1, "Incremented counter on main thread, expected 1, got %d", data.counter);

#ifndef SDL_PLATFORM_EMSCRIPTEN /* Emscripten doesn't have threads */
    {
        SDL_Window *window;
        SDL_Thread *thread;
        SDL_Event event;

        window = SDL_CreateWindow("test", 0, 0, SDL_WINDOW_HIDDEN);
        SDLTest_AssertCheck(window != NULL, "Create window, expected non-NULL, got %p", window);

        /* Flush any pending events */
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        /* Increment the counter on a thread, waiting for both calls to be queued */
        thread = SDL_CreateThread(IncrementCounterThread, NULL, &data);
        SDLTest_AssertCheck(thread != NULL, "Create counter thread");

        /* Wait for both increment calls to be queued up */
        SDL_Delay(100);

        /* Run the main callbacks */
        SDL_WaitEvent(&event);
        SDLTest_AssertCheck(event.type == SDL_EVENT_USER, "Expected user event (0x%.4x), got 0x%.4x", SDL_EVENT_USER, (int)event.type);
        SDL_WaitThread(thread, NULL);
        SDLTest_AssertCheck(data.counter == 3, "Incremented counter on main thread, expected 3, got %d", data.counter);

        /* Try again, but this time delay the calls until we've started waiting for events */
        data.delay = 100;
        thread = SDL_CreateThread(IncrementCounterThread, NULL, &data);
        SDLTest_AssertCheck(thread != NULL, "Create counter thread");

        /* Run the main callbacks */
        SDL_WaitEvent(&event);
        SDLTest_AssertCheck(event.type == SDL_EVENT_USER, "Expected user event (0x%.4x), got 0x%.4x", SDL_EVENT_USER, (int)event.type);
        SDL_WaitThread(thread, NULL);
        SDLTest_AssertCheck(data.counter == 5, "Incremented counter on main thread, expected 5, got %d", data.counter);

        SDL_DestroyWindow(window);
    }
#endif /* !SDL_PLATFORM_EMSCRIPTEN */

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Events test cases */
static const SDLTest_TestCaseReference eventsTest_pushPumpAndPollUserevent = {
    events_pushPumpAndPollUserevent, "events_pushPumpAndPollUserevent", "Pushes, pumps and polls a user event", TEST_ENABLED
};

static const SDLTest_TestCaseReference eventsTest_addDelEventWatch = {
    events_addDelEventWatch, "events_addDelEventWatch", "Adds and deletes an event watch function with NULL userdata", TEST_ENABLED
};

static const SDLTest_TestCaseReference eventsTest_addDelEventWatchWithUserdata = {
    events_addDelEventWatchWithUserdata, "events_addDelEventWatchWithUserdata", "Adds and deletes an event watch function with userdata", TEST_ENABLED
};

static const SDLTest_TestCaseReference eventsTest_mainThreadCallbacks = {
    events_mainThreadCallbacks, "events_mainThreadCallbacks", "Run callbacks on the main thread", TEST_ENABLED
};

/* Sequence of Events test cases */
static const SDLTest_TestCaseReference *eventsTests[] = {
    &eventsTest_pushPumpAndPollUserevent,
    &eventsTest_addDelEventWatch,
    &eventsTest_addDelEventWatchWithUserdata,
    &eventsTest_mainThreadCallbacks,
    NULL
};

/* Events test suite (global) */
SDLTest_TestSuiteReference eventsTestSuite = {
    "Events",
    NULL,
    eventsTests,
    NULL
};
