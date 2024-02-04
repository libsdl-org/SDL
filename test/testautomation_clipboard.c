/**
 * New/updated tests: aschiffler at ferzkopp dot net
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

static int clipboard_update_count;

static int ClipboardEventWatch(void *userdata, SDL_Event *event)
{
    if (event->type == SDL_EVENT_CLIPBOARD_UPDATE) {
        ++clipboard_update_count;
    }
    return 0;
}

enum
{
    TEST_MIME_TYPE_TEXT,
    TEST_MIME_TYPE_CUSTOM_TEXT,
    TEST_MIME_TYPE_DATA,
    NUM_TEST_MIME_TYPES
};
static const char *test_mime_types[] = {
    "text/plain;charset=utf-8",
    "test/text",
    "test/data"
};
SDL_COMPILE_TIME_ASSERT(test_mime_types, SDL_arraysize(test_mime_types) == NUM_TEST_MIME_TYPES);

typedef struct
{
    const void *data;
    size_t data_size;
} TestClipboardData;

static int clipboard_callback_count;

static const void *ClipboardDataCallback(void *userdata, const char *mime_type, size_t *length)
{
    TestClipboardData *test_data = (TestClipboardData *)userdata;

    ++clipboard_callback_count;

    if (SDL_strcmp(mime_type, test_mime_types[TEST_MIME_TYPE_TEXT]) == 0) {
        /* We're returning the string "TEST", with no termination */
        static const char *test_text = "XXX TEST XXX";
        *length = 4;
        return test_text + 4;
    }
    if (SDL_strcmp(mime_type, test_mime_types[TEST_MIME_TYPE_CUSTOM_TEXT]) == 0) {
        /* We're returning the string "CUSTOM", with no termination */
        static const char *custom_text = "XXX CUSTOM XXX";
        *length = 6;
        return custom_text + 4;
    }
    if (SDL_strcmp(mime_type, test_mime_types[TEST_MIME_TYPE_DATA]) == 0) {
        *length = test_data->data_size;
        return test_data->data;
    }
    return NULL;
}

static int clipboard_cleanup_count;

static void ClipboardCleanupCallback(void *userdata)
{
    ++clipboard_cleanup_count;
}

/* Test case functions */

/**
 * End-to-end test of SDL_xyzClipboardData functions
 * \sa SDL_HasClipboardData
 * \sa SDL_GetClipboardData
 * \sa SDL_SetClipboardData
 */
static int clipboard_testClipboardDataFunctions(void *arg)
{
    int result = -1;
    SDL_bool boolResult;
    int last_clipboard_update_count;
    int last_clipboard_callback_count;
    int last_clipboard_cleanup_count;
    void *data;
    size_t size;
    char *text;
    const char *expected_text;

    TestClipboardData test_data1 = {
        &test_data1,
        sizeof(test_data1)
    };
    TestClipboardData test_data2 = {
        &last_clipboard_callback_count,
        sizeof(last_clipboard_callback_count)
    };

    SDL_AddEventWatch(ClipboardEventWatch, NULL);

    /* Test clearing clipboard data */
    result = SDL_ClearClipboardData();
    SDLTest_AssertCheck(
        result == 0,
        "Validate SDL_ClearClipboardData result, expected 0, got %i",
        result);

    /* Test clearing clipboard data when it's already clear */
    last_clipboard_update_count = clipboard_update_count;
    result = SDL_ClearClipboardData();
    SDLTest_AssertCheck(
        result == 0,
        "Validate SDL_ClearClipboardData result, expected 0, got %i",
        result);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count,
        "Verify clipboard update unchanged, got %d",
        clipboard_update_count - last_clipboard_update_count);

    /* Validate error handling */
    last_clipboard_update_count = clipboard_update_count;
    result = SDL_SetClipboardData(NULL, NULL, NULL, test_mime_types, SDL_arraysize(test_mime_types));
    SDLTest_AssertCheck(
        result == -1,
        "Validate SDL_SetClipboardData(invalid) result, expected -1, got %i",
        result);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count,
        "Verify clipboard update count unchanged, got %d",
        clipboard_update_count - last_clipboard_update_count);

    last_clipboard_update_count = clipboard_update_count;
    result = SDL_SetClipboardData(ClipboardDataCallback, ClipboardCleanupCallback, NULL, NULL, 0);
    SDLTest_AssertCheck(
        result == -1,
        "Validate SDL_SetClipboardData(invalid) result, expected -1, got %i",
        result);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count,
        "Verify clipboard update count unchanged, got %d",
        clipboard_update_count - last_clipboard_update_count);

    /* Test setting and getting clipboard data */
    last_clipboard_update_count = clipboard_update_count;
    last_clipboard_callback_count = clipboard_callback_count;
    last_clipboard_cleanup_count = clipboard_cleanup_count;
    result = SDL_SetClipboardData(ClipboardDataCallback, ClipboardCleanupCallback, &test_data1, test_mime_types, SDL_arraysize(test_mime_types));
    SDLTest_AssertCheck(
        result == 0,
        "Validate SDL_SetClipboardData(test_data1) result, expected 0, got %i",
        result);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count + 1,
        "Verify clipboard update count incremented by 1, got %d",
        clipboard_update_count - last_clipboard_update_count);
    SDLTest_AssertCheck(
        clipboard_cleanup_count == last_clipboard_cleanup_count,
        "Verify clipboard cleanup count unchanged, got %d",
        clipboard_cleanup_count - last_clipboard_cleanup_count);

    expected_text = "TEST";
    text = SDL_GetClipboardText();
    SDLTest_AssertCheck(
        text && SDL_strcmp(text, expected_text) == 0,
        "Verify clipboard text, expected \"%s\", got \"%s\"",
        expected_text, text);

    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_TEXT]);
    SDLTest_AssertCheck(
        boolResult,
        "Verify has test text data, expected SDL_TRUE, got SDL_FALSE");
    text = SDL_GetClipboardData(test_mime_types[TEST_MIME_TYPE_TEXT], &size);
    SDLTest_AssertCheck(
        text && text[size] == '\0',
        "Verify test text data, expected null termination, got %c",
        text[size]);
    SDLTest_AssertCheck(
        text && SDL_strcmp(text, expected_text) == 0,
        "Verify test text data, expected \"%s\", got \"%s\"",
        expected_text, text);
    SDLTest_AssertCheck(
        size == SDL_strlen(expected_text),
        "Verify test text size, expected %d, got %d",
        (int)SDL_strlen(expected_text), (int)size);
    SDL_free(text);

    expected_text = "CUSTOM";
    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_CUSTOM_TEXT]);
    SDLTest_AssertCheck(
        boolResult,
        "Verify has test text data, expected SDL_TRUE, got SDL_FALSE");
    text = SDL_GetClipboardData(test_mime_types[TEST_MIME_TYPE_CUSTOM_TEXT], &size);
    SDLTest_AssertCheck(
        text && text[size] == '\0',
        "Verify test text data, expected null termination, got %c",
        text[size]);
    SDLTest_AssertCheck(
        text && SDL_strcmp(text, expected_text) == 0,
        "Verify test text data, expected \"%s\", got \"%s\"",
        expected_text, text);
    SDLTest_AssertCheck(
        size == SDL_strlen(expected_text),
        "Verify test text size, expected %d, got %d",
        (int)SDL_strlen(expected_text), (int)size);
    SDL_free(text);

    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_DATA]);
    SDLTest_AssertCheck(
        boolResult,
        "Verify has test text data, expected SDL_TRUE, got SDL_FALSE");
    data = SDL_GetClipboardData(test_mime_types[TEST_MIME_TYPE_DATA], &size);
    SDLTest_AssertCheck(
        SDL_memcmp(data, test_data1.data, test_data1.data_size) == 0,
        "Verify test data");
    SDLTest_AssertCheck(
        size == test_data1.data_size,
        "Verify test data size, expected %d, got %d",
        (int)test_data1.data_size, (int)size);
    SDL_free(data);

    boolResult = SDL_HasClipboardData("test/invalid");
    SDLTest_AssertCheck(
        !boolResult,
        "Verify has test text data, expected SDL_FALSE, got SDL_TRUE");
    data = SDL_GetClipboardData("test/invalid", &size);
    SDLTest_AssertCheck(
        data == NULL,
        "Verify invalid data, expected NULL, got %p",
        data);
    SDLTest_AssertCheck(
        size == 0,
        "Verify invalid data size, expected 0, got %d",
        (int)size);
    SDL_free(data);

#if 0 /* There's no guarantee how or when the callback is called */
    SDLTest_AssertCheck(
        (clipboard_callback_count == last_clipboard_callback_count + 3) ||
        (clipboard_callback_count == last_clipboard_callback_count + 4),
        "Verify clipboard callback count incremented by 3 or 4, got %d",
        clipboard_callback_count - last_clipboard_callback_count);
#endif

    /* Test setting and getting clipboard data again */
    last_clipboard_update_count = clipboard_update_count;
    last_clipboard_callback_count = clipboard_callback_count;
    last_clipboard_cleanup_count = clipboard_cleanup_count;
    result = SDL_SetClipboardData(ClipboardDataCallback, ClipboardCleanupCallback, &test_data2, test_mime_types, SDL_arraysize(test_mime_types));
    SDLTest_AssertCheck(
        result == 0,
        "Validate SDL_SetClipboardData(test_data2) result, expected 0, got %i",
        result);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count + 1,
        "Verify clipboard update count incremented by 1, got %d",
        clipboard_update_count - last_clipboard_update_count);
    SDLTest_AssertCheck(
        clipboard_cleanup_count == last_clipboard_cleanup_count + 1,
        "Verify clipboard cleanup count incremented by 1, got %d",
        clipboard_cleanup_count - last_clipboard_cleanup_count);

    expected_text = "TEST";
    text = SDL_GetClipboardText();
    SDLTest_AssertCheck(
        text && SDL_strcmp(text, expected_text) == 0,
        "Verify clipboard text, expected \"%s\", got \"%s\"",
        expected_text, text);

    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_TEXT]);
    SDLTest_AssertCheck(
        boolResult,
        "Verify has test text data, expected SDL_TRUE, got SDL_FALSE");
    text = SDL_GetClipboardData(test_mime_types[TEST_MIME_TYPE_TEXT], &size);
    SDLTest_AssertCheck(
        text && text[size] == '\0',
        "Verify test text data, expected null termination, got %c",
        text[size]);
    SDLTest_AssertCheck(
        text && SDL_strcmp(text, expected_text) == 0,
        "Verify test text data, expected \"%s\", got \"%s\"",
        expected_text, text);
    SDLTest_AssertCheck(
        size == SDL_strlen(expected_text),
        "Verify test text size, expected %d, got %d",
        (int)SDL_strlen(expected_text), (int)size);
    SDL_free(text);

    expected_text = "CUSTOM";
    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_CUSTOM_TEXT]);
    SDLTest_AssertCheck(
        boolResult,
        "Verify has test text data, expected SDL_TRUE, got SDL_FALSE");
    text = SDL_GetClipboardData(test_mime_types[TEST_MIME_TYPE_CUSTOM_TEXT], &size);
    SDLTest_AssertCheck(
        text && text[size] == '\0',
        "Verify test text data, expected null termination, got %c",
        text[size]);
    SDLTest_AssertCheck(
        text && SDL_strcmp(text, expected_text) == 0,
        "Verify test text data, expected \"%s\", got \"%s\"",
        expected_text, text);
    SDLTest_AssertCheck(
        size == SDL_strlen(expected_text),
        "Verify test text size, expected %d, got %d",
        (int)SDL_strlen(expected_text), (int)size);
    SDL_free(text);

    data = SDL_GetClipboardData(test_mime_types[TEST_MIME_TYPE_DATA], &size);
    SDLTest_AssertCheck(
        SDL_memcmp(data, test_data2.data, test_data2.data_size) == 0,
        "Verify test data");
    SDLTest_AssertCheck(
        size == test_data2.data_size,
        "Verify test data size, expected %d, got %d",
        (int)test_data2.data_size, (int)size);
    SDL_free(data);

    data = SDL_GetClipboardData("test/invalid", &size);
    SDLTest_AssertCheck(
        data == NULL,
        "Verify invalid data, expected NULL, got %p",
        data);
    SDLTest_AssertCheck(
        size == 0,
        "Verify invalid data size, expected 0, got %d",
        (int)size);
    SDL_free(data);

#if 0 /* There's no guarantee how or when the callback is called */
    SDLTest_AssertCheck(
        (clipboard_callback_count == last_clipboard_callback_count + 3) ||
            (clipboard_callback_count == last_clipboard_callback_count + 4),
        "Verify clipboard callback count incremented by 3 or 4, got %d",
        clipboard_callback_count - last_clipboard_callback_count);
#endif

    /* Test clearing clipboard data when has data */
    last_clipboard_update_count = clipboard_update_count;
    last_clipboard_cleanup_count = clipboard_cleanup_count;
    result = SDL_ClearClipboardData();
    SDLTest_AssertCheck(
        result == 0,
        "Validate SDL_ClearClipboardData result, expected 0, got %i",
        result);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count + 1,
        "Verify clipboard update count incremented by 1, got %d",
        clipboard_update_count - last_clipboard_update_count);
    SDLTest_AssertCheck(
        clipboard_cleanup_count == last_clipboard_cleanup_count + 1,
        "Verify clipboard cleanup count incremented by 1, got %d",
        clipboard_cleanup_count - last_clipboard_cleanup_count);
    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_TEXT]);
    SDLTest_AssertCheck(
        !boolResult,
        "Verify has test text data, expected SDL_FALSE, got SDL_TRUE");
    boolResult = SDL_HasClipboardData(test_mime_types[TEST_MIME_TYPE_DATA]);
    SDLTest_AssertCheck(
        !boolResult,
        "Verify has test text data, expected SDL_FALSE, got SDL_TRUE");
    boolResult = SDL_HasClipboardData("test/invalid");
    SDLTest_AssertCheck(
        !boolResult,
        "Verify has test text data, expected SDL_FALSE, got SDL_TRUE");

    SDL_DelEventWatch(ClipboardEventWatch, NULL);

    return TEST_COMPLETED;
}

/**
 * End-to-end test of SDL_xyzClipboardText functions
 * \sa SDL_HasClipboardText
 * \sa SDL_GetClipboardText
 * \sa SDL_SetClipboardText
 */
static int clipboard_testClipboardTextFunctions(void *arg)
{
    char *textRef = SDLTest_RandomAsciiString();
    char *text = SDL_strdup(textRef);
    SDL_bool boolResult;
    int intResult;
    char *charResult;
    int last_clipboard_update_count;

    SDL_AddEventWatch(ClipboardEventWatch, NULL);

    /* Empty clipboard text */
    last_clipboard_update_count = clipboard_update_count;
    intResult = SDL_SetClipboardText(NULL);
    SDLTest_AssertCheck(
        intResult == 0,
        "Verify result from SDL_SetClipboardText(NULL), expected 0, got %i",
        intResult);
    charResult = SDL_GetClipboardText();
    SDLTest_AssertCheck(
        charResult && SDL_strcmp(charResult, "") == 0,
        "Verify SDL_GetClipboardText returned \"\", got %s",
        charResult);
    SDL_free(charResult);
    boolResult = SDL_HasClipboardText();
    SDLTest_AssertCheck(
        boolResult == SDL_FALSE,
        "Verify SDL_HasClipboardText returned SDL_FALSE, got %s",
        (boolResult) ? "SDL_TRUE" : "SDL_FALSE");
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count,
        "Verify clipboard update unchanged, got %d",
        clipboard_update_count - last_clipboard_update_count);


    /* Set clipboard text  */
    last_clipboard_update_count = clipboard_update_count;
    intResult = SDL_SetClipboardText(text);
    SDLTest_AssertCheck(
        intResult == 0,
        "Verify result from SDL_SetClipboardText(%s), expected 0, got %i", text,
        intResult);
    SDLTest_AssertCheck(
        SDL_strcmp(textRef, text) == 0,
        "Verify SDL_SetClipboardText did not modify input string, expected '%s', got '%s'",
        textRef, text);
    boolResult = SDL_HasClipboardText();
    SDLTest_AssertCheck(
        boolResult == SDL_TRUE,
        "Verify SDL_HasClipboardText returned SDL_TRUE, got %s",
        (boolResult) ? "SDL_TRUE" : "SDL_FALSE");
    charResult = SDL_GetClipboardText();
    SDLTest_AssertCheck(
        charResult && SDL_strcmp(textRef, charResult) == 0,
        "Verify SDL_GetClipboardText returned correct string, expected '%s', got '%s'",
        textRef, charResult);
    SDL_free(charResult);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count + 1,
        "Verify clipboard update count incremented by 1, got %d",
        clipboard_update_count - last_clipboard_update_count);

    /* Reset clipboard text */
    intResult = SDL_SetClipboardText(NULL);
    SDLTest_AssertCheck(
        intResult == 0,
        "Verify result from SDL_SetClipboardText(NULL), expected 0, got %i",
        intResult);

    /* Cleanup */
    SDL_free(textRef);
    SDL_free(text);

    SDL_DelEventWatch(ClipboardEventWatch, NULL);

    return TEST_COMPLETED;
}

/**
 * End-to-end test of SDL_xyzPrimarySelectionText functions
 * \sa SDL_HasPrimarySelectionText
 * \sa SDL_GetPrimarySelectionText
 * \sa SDL_SetPrimarySelectionText
 */
static int clipboard_testPrimarySelectionTextFunctions(void *arg)
{
    char *textRef = SDLTest_RandomAsciiString();
    char *text = SDL_strdup(textRef);
    SDL_bool boolResult;
    int intResult;
    char *charResult;
    int last_clipboard_update_count;

    SDL_AddEventWatch(ClipboardEventWatch, NULL);

    /* Empty primary selection */
    last_clipboard_update_count = clipboard_update_count;
    intResult = SDL_SetPrimarySelectionText(NULL);
    SDLTest_AssertCheck(
        intResult == 0,
        "Verify result from SDL_SetPrimarySelectionText(NULL), expected 0, got %i",
        intResult);
    charResult = SDL_GetPrimarySelectionText();
    SDLTest_AssertCheck(
        charResult && SDL_strcmp(charResult, "") == 0,
        "Verify SDL_GetPrimarySelectionText returned \"\", got %s",
        charResult);
    SDL_free(charResult);
    boolResult = SDL_HasPrimarySelectionText();
    SDLTest_AssertCheck(
        boolResult == SDL_FALSE,
        "Verify SDL_HasPrimarySelectionText returned SDL_FALSE, got %s",
        (boolResult) ? "SDL_TRUE" : "SDL_FALSE");
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count + 1,
        "Verify clipboard update count incremented by 1, got %d",
        clipboard_update_count - last_clipboard_update_count);

    /* Set primary selection  */
    last_clipboard_update_count = clipboard_update_count;
    intResult = SDL_SetPrimarySelectionText(text);
    SDLTest_AssertCheck(
        intResult == 0,
        "Verify result from SDL_SetPrimarySelectionText(%s), expected 0, got %i", text,
        intResult);
    SDLTest_AssertCheck(
        SDL_strcmp(textRef, text) == 0,
        "Verify SDL_SetPrimarySelectionText did not modify input string, expected '%s', got '%s'",
        textRef, text);
    boolResult = SDL_HasPrimarySelectionText();
    SDLTest_AssertCheck(
        boolResult == SDL_TRUE,
        "Verify SDL_HasPrimarySelectionText returned SDL_TRUE, got %s",
        (boolResult) ? "SDL_TRUE" : "SDL_FALSE");
    charResult = SDL_GetPrimarySelectionText();
    SDLTest_AssertCheck(
        charResult && SDL_strcmp(textRef, charResult) == 0,
        "Verify SDL_GetPrimarySelectionText returned correct string, expected '%s', got '%s'",
        textRef, charResult);
    SDL_free(charResult);
    SDLTest_AssertCheck(
        clipboard_update_count == last_clipboard_update_count + 1,
        "Verify clipboard update count incremented by 1, got %d",
        clipboard_update_count - last_clipboard_update_count);

    /* Reset primary selection */
    intResult = SDL_SetPrimarySelectionText(NULL);
    SDLTest_AssertCheck(
        intResult == 0,
        "Verify result from SDL_SetPrimarySelectionText(NULL), expected 0, got %i",
        intResult);

    /* Cleanup */
    SDL_free(textRef);
    SDL_free(text);

    SDL_DelEventWatch(ClipboardEventWatch, NULL);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

static const SDLTest_TestCaseReference clipboardTest1 = {
    (SDLTest_TestCaseFp)clipboard_testClipboardDataFunctions, "clipboard_testClipboardDataFunctions", "End-to-end test of SDL_xyzClipboardData functions", TEST_ENABLED
};

static const SDLTest_TestCaseReference clipboardTest2 = {
    (SDLTest_TestCaseFp)clipboard_testClipboardTextFunctions, "clipboard_testClipboardTextFunctions", "End-to-end test of SDL_xyzClipboardText functions", TEST_ENABLED
};

static const SDLTest_TestCaseReference clipboardTest3 = {
    (SDLTest_TestCaseFp)clipboard_testPrimarySelectionTextFunctions, "clipboard_testPrimarySelectionTextFunctions", "End-to-end test of SDL_xyzPrimarySelectionText functions", TEST_ENABLED
};

/* Sequence of Clipboard test cases */
static const SDLTest_TestCaseReference *clipboardTests[] = {
    &clipboardTest1, &clipboardTest2, &clipboardTest3,  NULL
};

/* Clipboard test suite (global) */
SDLTest_TestSuiteReference clipboardTestSuite = {
    "Clipboard",
    NULL,
    clipboardTests,
    NULL
};
