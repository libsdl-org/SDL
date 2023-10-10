/**
 * Properties test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Test case functions */

/**
 * \brief Test basic functionality
 */
static int properties_testBasic(void *arg)
{
    SDL_PropertiesID props;
    char key[2], expected_value[2];
    void *value;
    int i, result;

    props = SDL_CreateProperties();
    SDLTest_AssertPass("Call to SDL_CreateProperties()");
    SDLTest_AssertCheck(props != 0,
        "Verify props were created, got: %" SDL_PRIu32 "", props);

    for (i = 0; i < 10; ++i) {
        SDL_snprintf(key, SDL_arraysize(key), "%c", 'a' + i);
        SDL_snprintf(expected_value, SDL_arraysize(expected_value), "%c", 'a' + i);
        result = SDL_SetProperty(props, key, expected_value, NULL, NULL);
        SDLTest_AssertPass("Call to SDL_SetProperty()");
        SDLTest_AssertCheck(result == 0,
            "Verify property value was set, got: %d", result);
        value = SDL_GetProperty(props, key);
        SDLTest_AssertPass("Call to SDL_GetProperty()");
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, expected_value) == 0,
            "Verify property value was set, got %s, expected %s", value ? (const char *)value : "NULL", expected_value);
    }

    for (i = 0; i < 10; ++i) {
        SDL_snprintf(key, SDL_arraysize(key), "%c", 'a' + i);
        result = SDL_SetProperty(props, key, NULL, NULL, NULL);
        SDLTest_AssertPass("Call to SDL_SetProperty(NULL)");
        SDLTest_AssertCheck(result == 0,
            "Verify property value was set, got: %d", result);
        value = SDL_GetProperty(props, key);
        SDLTest_AssertPass("Call to SDL_GetProperty()");
        SDLTest_AssertCheck(value == NULL,
            "Verify property value was set, got %s, expected NULL", (const char *)value);
    }

    SDL_DestroyProperties(props);

    return TEST_COMPLETED;
}

/**
 * \brief Test cleanup functionality
 */
static void SDLCALL cleanup(void *userdata, void *value)
{
    int *count = (int *)userdata;
    ++(*count);
}
static int properties_testCleanup(void *arg)
{
    SDL_PropertiesID props;
    char key[2], expected_value[2];
    int i, count;

    props = SDL_CreateProperties();

    SDLTest_AssertPass("Call to SDL_SetProperty(cleanup)");
    count = 0;
    SDL_SetProperty(props, "a", "0", cleanup, &count);
    SDL_SetProperty(props, "a", NULL, cleanup, &count);
    SDLTest_AssertCheck(count == 1,
        "Verify cleanup for deleting property, got %d, expected 1", count);

    SDLTest_AssertPass("Call to SDL_DestroyProperties()");
    count = 0;
    for (i = 0; i < 10; ++i) {
        SDL_snprintf(key, SDL_arraysize(key), "%c", 'a' + i);
        SDL_snprintf(expected_value, SDL_arraysize(expected_value), "%c", 'a' + i);
        SDL_SetProperty(props, key, expected_value, cleanup, &count);
    }
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(count == 10,
        "Verify cleanup for destroying properties, got %d, expected 10", count);

    return TEST_COMPLETED;
}

/**
 * \brief Test locking functionality
 */
struct properties_thread_data
{
    SDL_bool done;
    SDL_PropertiesID props;
};
static int properties_thread(void *arg)
{
    struct properties_thread_data *data = (struct properties_thread_data *)arg;

    while (!data->done) {
        SDL_LockProperties(data->props);
        SDL_SetProperty(data->props, "a", "thread_loop", NULL, NULL);
        SDL_UnlockProperties(data->props);
    }
    SDL_LockProperties(data->props);
    SDL_SetProperty(data->props, "a", "thread_done", NULL, NULL);
    SDL_UnlockProperties(data->props);
    return 0;
}
static int properties_testLocking(void *arg)
{
    struct properties_thread_data data;
    SDL_Thread *thread;
    void *value;

    SDLTest_AssertPass("Testing property locking");
    data.done = SDL_FALSE;
    data.props = SDL_CreateProperties();
    SDLTest_AssertPass("Setting property to 'init'");
    SDL_SetProperty(data.props, "a", "init", NULL, NULL);
    thread = SDL_CreateThread(properties_thread, "properties_thread", &data);
    if (thread) {
        SDLTest_AssertPass("Waiting for property to change to 'thread_loop'");
        for ( ; ; )
        {
            SDL_Delay(10);
            SDL_LockProperties(data.props);
            value = SDL_GetProperty(data.props, "a");
            SDL_UnlockProperties(data.props);

            if (!value || SDL_strcmp((const char *)value, "thread_loop") == 0) {
                break;
            }
        }
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, "thread_loop") == 0,
            "After thread loop, property is %s, expected 'thread_loop'", value ? (const char *)value : "NULL");

        SDLTest_AssertPass("Setting property to 'main'");
        SDL_LockProperties(data.props);
        SDL_SetProperty(data.props, "a", "main", NULL, NULL);
        SDL_Delay(100);
        value = SDL_GetProperty(data.props, "a");
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, "main") == 0,
            "After 100ms sleep, property is %s, expected 'main'", value ? (const char *)value : "NULL");
        SDL_UnlockProperties(data.props);

        data.done = SDL_TRUE;
        SDL_WaitThread(thread, NULL);

        value = SDL_GetProperty(data.props, "a");
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, "thread_done") == 0,
            "After thread complete, property is %s, expected 'thread_done'", value ? (const char *)value : "NULL");
    }
    SDL_DestroyProperties(data.props);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Properties test cases */
static const SDLTest_TestCaseReference propertiesTest1 = {
    (SDLTest_TestCaseFp)properties_testBasic, "properties_testBasic", "Test basic property functionality", TEST_ENABLED
};

static const SDLTest_TestCaseReference propertiesTest2 = {
    (SDLTest_TestCaseFp)properties_testCleanup, "properties_testCleanup", "Test property cleanup functionality", TEST_ENABLED
};

static const SDLTest_TestCaseReference propertiesTest3 = {
    (SDLTest_TestCaseFp)properties_testLocking, "properties_testLocking", "Test property locking functionality", TEST_ENABLED
};

/* Sequence of Properties test cases */
static const SDLTest_TestCaseReference *propertiesTests[] = {
    &propertiesTest1, &propertiesTest2, &propertiesTest3, NULL
};

/* Properties test suite (global) */
SDLTest_TestSuiteReference propertiesTestSuite = {
    "Properties",
    NULL,
    propertiesTests,
    NULL
};
