/**
 * Properties test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Test case functions */

/**
 * Test basic functionality
 */
static void SDLCALL count_properties(void *userdata, SDL_PropertiesID props, const char *name)
{
    int *count = (int *)userdata;
    ++(*count);
}
static void SDLCALL count_foo_properties(void *userdata, SDL_PropertiesID props, const char *name)
{
    int *count = (int *)userdata;
    if (SDL_strcmp(name, "foo") == 0) {
        ++(*count);
    }
}
static int properties_testBasic(void *arg)
{
    SDL_PropertiesID props;
    char key[2], expected_value[2];
    SDL_PropertyType type;
    void *value;
    const char *value_string;
    Sint64 value_number;
    float value_float;
    SDL_bool value_bool;
    int i, result, count;

    props = SDL_CreateProperties();
    SDLTest_AssertPass("Call to SDL_CreateProperties()");
    SDLTest_AssertCheck(props != 0,
        "Verify props were created, got: %" SDL_PRIu32 "", props);

    for (i = 0; i < 10; ++i) {
        SDL_snprintf(key, SDL_arraysize(key), "%c", 'a' + i);
        SDL_snprintf(expected_value, SDL_arraysize(expected_value), "%c", 'a' + i);
        result = SDL_SetProperty(props, key, expected_value);
        SDLTest_AssertPass("Call to SDL_SetProperty()");
        SDLTest_AssertCheck(result == 0,
            "Verify property value was set, got: %d", result);
        value = SDL_GetProperty(props, key, NULL);
        SDLTest_AssertPass("Call to SDL_GetProperty()");
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, expected_value) == 0,
            "Verify property value was set, got %s, expected %s", value ? (const char *)value : "NULL", expected_value);
    }

    count = 0;
    SDL_EnumerateProperties(props, count_properties, &count);
    SDLTest_AssertCheck(count == 10,
            "Verify property count, expected 10, got: %d", count);

    for (i = 0; i < 10; ++i) {
        SDL_snprintf(key, SDL_arraysize(key), "%c", 'a' + i);
        result = SDL_SetProperty(props, key, NULL);
        SDLTest_AssertPass("Call to SDL_SetProperty(NULL)");
        SDLTest_AssertCheck(result == 0,
            "Verify property value was set, got: %d", result);
        value = SDL_GetProperty(props, key, NULL);
        SDLTest_AssertPass("Call to SDL_GetProperty()");
        SDLTest_AssertCheck(value == NULL,
            "Verify property value was set, got %s, expected NULL", (const char *)value);
    }

    count = 0;
    SDL_EnumerateProperties(props, count_properties, &count);
    SDLTest_AssertCheck(count == 0,
            "Verify property count, expected 0, got: %d", count);

    /* Check default values */
    value = SDL_GetProperty(props, "foo", (void *)0xabcd);
    SDLTest_AssertCheck(value == (void *)0xabcd,
            "Verify property, expected 0xabcd, got: %p", value);
    value_string = SDL_GetStringProperty(props, "foo", "abcd");
    SDLTest_AssertCheck(value_string && SDL_strcmp(value_string, "abcd") == 0,
            "Verify string property, expected abcd, got: %s", value_string);
    value_number = SDL_GetNumberProperty(props, "foo", 1234);
    SDLTest_AssertCheck(value_number == 1234,
            "Verify number property, expected 1234, got: %" SDL_PRIu64 "", value_number);
    value_float = SDL_GetFloatProperty(props, "foo", 1234.0f);
    SDLTest_AssertCheck(value_float == 1234.0f,
            "Verify float property, expected 1234, got: %f", value_float);
    value_bool = SDL_GetBooleanProperty(props, "foo", SDL_TRUE);
    SDLTest_AssertCheck(value_bool == SDL_TRUE,
            "Verify boolean property, expected SDL_TRUE, got: %s", value_bool ? "SDL_TRUE" : "SDL_FALSE");

    /* Check data value */
    SDLTest_AssertPass("Call to SDL_SetProperty(\"foo\", 0x01)");
    SDL_SetProperty(props, "foo", (void *)0x01);
    type = SDL_GetPropertyType(props, "foo");
    SDLTest_AssertCheck(type == SDL_PROPERTY_TYPE_POINTER,
            "Verify property type, expected %d, got: %d", SDL_PROPERTY_TYPE_POINTER, type);
    value = SDL_GetProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value == (void *)0x01,
            "Verify property, expected 0x01, got: %p", value);
    value_string = SDL_GetStringProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value_string == NULL,
            "Verify string property, expected NULL, got: %s", value_string);
    value_number = SDL_GetNumberProperty(props, "foo", 0);
    SDLTest_AssertCheck(value_number == 0,
            "Verify number property, expected 0, got: %" SDL_PRIu64 "", value_number);
    value_float = SDL_GetFloatProperty(props, "foo", 0.0f);
    SDLTest_AssertCheck(value_float == 0.0f,
            "Verify float property, expected 0, got: %f", value_float);
    value_bool = SDL_GetBooleanProperty(props, "foo", SDL_FALSE);
    SDLTest_AssertCheck(value_bool == SDL_FALSE,
            "Verify boolean property, expected SDL_FALSE, got: %s", value_bool ? "SDL_TRUE" : "SDL_FALSE");

    /* Check string value */
    SDLTest_AssertPass("Call to SDL_SetStringProperty(\"foo\", \"bar\")");
    SDL_SetStringProperty(props, "foo", "bar");
    type = SDL_GetPropertyType(props, "foo");
    SDLTest_AssertCheck(type == SDL_PROPERTY_TYPE_STRING,
            "Verify property type, expected %d, got: %d", SDL_PROPERTY_TYPE_STRING, type);
    value = SDL_GetProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value == NULL,
            "Verify property, expected NULL, got: %p", value);
    value_string = SDL_GetStringProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value_string != NULL && SDL_strcmp(value_string, "bar") == 0,
            "Verify string property, expected bar, got: %s", value_string);
    value_number = SDL_GetNumberProperty(props, "foo", 0);
    SDLTest_AssertCheck(value_number == 0,
            "Verify number property, expected 0, got: %" SDL_PRIu64 "", value_number);
    value_float = SDL_GetFloatProperty(props, "foo", 0.0f);
    SDLTest_AssertCheck(value_float == 0.0f,
            "Verify float property, expected 0, got: %f", value_float);
    value_bool = SDL_GetBooleanProperty(props, "foo", SDL_FALSE);
    SDLTest_AssertCheck(value_bool == SDL_TRUE,
            "Verify boolean property, expected SDL_TRUE, got: %s", value_bool ? "SDL_TRUE" : "SDL_FALSE");

    /* Check number value */
    SDLTest_AssertPass("Call to SDL_SetNumberProperty(\"foo\", 1)");
    SDL_SetNumberProperty(props, "foo", 1);
    type = SDL_GetPropertyType(props, "foo");
    SDLTest_AssertCheck(type == SDL_PROPERTY_TYPE_NUMBER,
            "Verify property type, expected %d, got: %d", SDL_PROPERTY_TYPE_NUMBER, type);
    value = SDL_GetProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value == NULL,
            "Verify property, expected NULL, got: %p", value);
    value_string = SDL_GetStringProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value_string && SDL_strcmp(value_string, "1") == 0,
            "Verify string property, expected 1, got: %s", value_string);
    value_number = SDL_GetNumberProperty(props, "foo", 0);
    SDLTest_AssertCheck(value_number == 1,
            "Verify number property, expected 1, got: %" SDL_PRIu64 "", value_number);
    value_float = SDL_GetFloatProperty(props, "foo", 0.0f);
    SDLTest_AssertCheck(value_float == 1.0f,
            "Verify float property, expected 1, got: %f", value_float);
    value_bool = SDL_GetBooleanProperty(props, "foo", SDL_FALSE);
    SDLTest_AssertCheck(value_bool == SDL_TRUE,
            "Verify boolean property, expected SDL_TRUE, got: %s", value_bool ? "SDL_TRUE" : "SDL_FALSE");

    /* Check float value */
    SDLTest_AssertPass("Call to SDL_SetFloatProperty(\"foo\", 1)");
    SDL_SetFloatProperty(props, "foo", 1.75f);
    type = SDL_GetPropertyType(props, "foo");
    SDLTest_AssertCheck(type == SDL_PROPERTY_TYPE_FLOAT,
            "Verify property type, expected %d, got: %d", SDL_PROPERTY_TYPE_FLOAT, type);
    value = SDL_GetProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value == NULL,
            "Verify property, expected NULL, got: %p", value);
    value_string = SDL_GetStringProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value_string && SDL_strcmp(value_string, "1.750000") == 0,
            "Verify string property, expected 1.750000, got: %s", value_string);
    value_number = SDL_GetNumberProperty(props, "foo", 0);
    SDLTest_AssertCheck(value_number == 2,
            "Verify number property, expected 2, got: %" SDL_PRIu64 "", value_number);
    value_float = SDL_GetFloatProperty(props, "foo", 0.0f);
    SDLTest_AssertCheck(value_float == 1.75f,
            "Verify float property, expected 1.75, got: %f", value_float);
    value_bool = SDL_GetBooleanProperty(props, "foo", SDL_FALSE);
    SDLTest_AssertCheck(value_bool == SDL_TRUE,
            "Verify boolean property, expected SDL_TRUE, got: %s", value_bool ? "SDL_TRUE" : "SDL_FALSE");

    /* Check boolean value */
    SDLTest_AssertPass("Call to SDL_SetBooleanProperty(\"foo\", SDL_TRUE)");
    SDL_SetBooleanProperty(props, "foo", 3); /* Note we're testing non-true/false value here */
    type = SDL_GetPropertyType(props, "foo");
    SDLTest_AssertCheck(type == SDL_PROPERTY_TYPE_BOOLEAN,
            "Verify property type, expected %d, got: %d", SDL_PROPERTY_TYPE_BOOLEAN, type);
    value = SDL_GetProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value == NULL,
            "Verify property, expected NULL, got: %p", value);
    value_string = SDL_GetStringProperty(props, "foo", NULL);
    SDLTest_AssertCheck(value_string && SDL_strcmp(value_string, "true") == 0,
            "Verify string property, expected true, got: %s", value_string);
    value_number = SDL_GetNumberProperty(props, "foo", 0);
    SDLTest_AssertCheck(value_number == 1,
            "Verify number property, expected 1, got: %" SDL_PRIu64 "", value_number);
    value_float = SDL_GetFloatProperty(props, "foo", 0.0f);
    SDLTest_AssertCheck(value_float == 1.0f,
            "Verify float property, expected 1, got: %f", value_float);
    value_bool = SDL_GetBooleanProperty(props, "foo", SDL_FALSE);
    SDLTest_AssertCheck(value_bool == SDL_TRUE,
            "Verify boolean property, expected SDL_TRUE, got: %s", value_bool ? "SDL_TRUE" : "SDL_FALSE");

    /* Make sure we have exactly one property named foo */
    count = 0;
    SDL_EnumerateProperties(props, count_foo_properties, &count);
    SDLTest_AssertCheck(count == 1,
            "Verify foo property count, expected 1, got: %d", count);

    SDL_DestroyProperties(props);

    return TEST_COMPLETED;
}

/**
 * Test cleanup functionality
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
    SDL_SetPropertyWithCleanup(props, "a", "0", cleanup, &count);
    SDL_SetPropertyWithCleanup(props, "a", NULL, cleanup, &count);
    SDLTest_AssertCheck(count == 1,
        "Verify cleanup for deleting property, got %d, expected 1", count);

    SDLTest_AssertPass("Call to SDL_DestroyProperties()");
    count = 0;
    for (i = 0; i < 10; ++i) {
        SDL_snprintf(key, SDL_arraysize(key), "%c", 'a' + i);
        SDL_snprintf(expected_value, SDL_arraysize(expected_value), "%c", 'a' + i);
        SDL_SetPropertyWithCleanup(props, key, expected_value, cleanup, &count);
    }
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(count == 10,
        "Verify cleanup for destroying properties, got %d, expected 10", count);

    return TEST_COMPLETED;
}

/**
 * Test locking functionality
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
        SDL_SetProperty(data->props, "a", "thread_loop");
        SDL_UnlockProperties(data->props);
    }
    SDL_LockProperties(data->props);
    SDL_SetProperty(data->props, "a", "thread_done");
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
    SDL_SetProperty(data.props, "a", "init");
    thread = SDL_CreateThread(properties_thread, "properties_thread", &data);
    if (thread) {
        SDLTest_AssertPass("Waiting for property to change to 'thread_loop'");
        for ( ; ; )
        {
            SDL_Delay(10);
            SDL_LockProperties(data.props);
            value = SDL_GetProperty(data.props, "a", NULL);
            SDL_UnlockProperties(data.props);

            if (!value || SDL_strcmp((const char *)value, "thread_loop") == 0) {
                break;
            }
        }
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, "thread_loop") == 0,
            "After thread loop, property is %s, expected 'thread_loop'", value ? (const char *)value : "NULL");

        SDLTest_AssertPass("Setting property to 'main'");
        SDL_LockProperties(data.props);
        SDL_SetProperty(data.props, "a", "main");
        SDL_Delay(100);
        value = SDL_GetProperty(data.props, "a", NULL);
        SDLTest_AssertCheck(value && SDL_strcmp((const char *)value, "main") == 0,
            "After 100ms sleep, property is %s, expected 'main'", value ? (const char *)value : "NULL");
        SDL_UnlockProperties(data.props);

        data.done = SDL_TRUE;
        SDL_WaitThread(thread, NULL);

        value = SDL_GetProperty(data.props, "a", NULL);
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
