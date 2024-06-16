/**
 * Original code: automated SDL platform test written by Edgar Simo "bobbens"
 * Extended and updated by aschiffler at ferzkopp dot net
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Helper functions */

/**
 * Compare sizes of types.
 *
 * @note Watcom C flags these as Warning 201: "Unreachable code" if you just
 *  compare them directly, so we push it through a function to keep the
 *  compiler quiet.  --ryan.
 */
static int compareSizeOfType(size_t sizeoftype, size_t hardcodetype)
{
    return sizeoftype != hardcodetype;
}

/* Test case functions */

/**
 * Tests type sizes.
 */
static int platform_testTypes(void *arg)
{
    int ret;

    ret = compareSizeOfType(sizeof(Uint8), 1);
    SDLTest_AssertCheck(ret == 0, "sizeof(Uint8) = %u, expected  1", (unsigned int)sizeof(Uint8));

    ret = compareSizeOfType(sizeof(Uint16), 2);
    SDLTest_AssertCheck(ret == 0, "sizeof(Uint16) = %u, expected 2", (unsigned int)sizeof(Uint16));

    ret = compareSizeOfType(sizeof(Uint32), 4);
    SDLTest_AssertCheck(ret == 0, "sizeof(Uint32) = %u, expected 4", (unsigned int)sizeof(Uint32));

    ret = compareSizeOfType(sizeof(Uint64), 8);
    SDLTest_AssertCheck(ret == 0, "sizeof(Uint64) = %u, expected 8", (unsigned int)sizeof(Uint64));

    return TEST_COMPLETED;
}

/**
 * Tests platform endianness and SDL_SwapXY functions.
 */
static int platform_testEndianessAndSwap(void *arg)
{
    int real_byteorder;
    int real_floatwordorder = 0;
    Uint16 value = 0x1234;
    Uint16 value16 = 0xCDAB;
    Uint16 swapped16 = 0xABCD;
    Uint32 value32 = 0xEFBEADDE;
    Uint32 swapped32 = 0xDEADBEEF;

    union
    {
        double d;
        Uint32 ui32[2];
    } value_double;

    Uint64 value64, swapped64;
    value64 = 0xEFBEADDE;
    value64 <<= 32;
    value64 |= 0xCDAB3412;
    swapped64 = 0x1234ABCD;
    swapped64 <<= 32;
    swapped64 |= 0xDEADBEEF;
    value_double.d = 3.141593;

    if ((*((char *)&value) >> 4) == 0x1) {
        real_byteorder = SDL_BIG_ENDIAN;
    } else {
        real_byteorder = SDL_LIL_ENDIAN;
    }

    /* Test endianness. */
    SDLTest_AssertCheck(real_byteorder == SDL_BYTEORDER,
                        "Machine detected as %s endian, appears to be %s endian.",
                        (SDL_BYTEORDER == SDL_LIL_ENDIAN) ? "little" : "big",
                        (real_byteorder == SDL_LIL_ENDIAN) ? "little" : "big");

    if (value_double.ui32[0] == 0x82c2bd7f && value_double.ui32[1] == 0x400921fb) {
        real_floatwordorder = SDL_LIL_ENDIAN;
    } else if (value_double.ui32[0] == 0x400921fb && value_double.ui32[1] == 0x82c2bd7f) {
        real_floatwordorder = SDL_BIG_ENDIAN;
    }

    /* Test endianness. */
    SDLTest_AssertCheck(real_floatwordorder == SDL_FLOATWORDORDER,
                        "Machine detected as having %s endian float word order, appears to be %s endian.",
                        (SDL_FLOATWORDORDER == SDL_LIL_ENDIAN) ? "little" : "big",
                        (real_floatwordorder == SDL_LIL_ENDIAN) ? "little" : (real_floatwordorder == SDL_BIG_ENDIAN) ? "big"
                                                                                                                     : "unknown");

    /* Test 16 swap. */
    SDLTest_AssertCheck(SDL_Swap16(value16) == swapped16,
                        "SDL_Swap16(): 16 bit swapped: 0x%X => 0x%X",
                        value16, SDL_Swap16(value16));

    /* Test 32 swap. */
    SDLTest_AssertCheck(SDL_Swap32(value32) == swapped32,
                        "SDL_Swap32(): 32 bit swapped: 0x%" SDL_PRIX32 " => 0x%" SDL_PRIX32,
                        value32, SDL_Swap32(value32));

    /* Test 64 swap. */
    SDLTest_AssertCheck(SDL_Swap64(value64) == swapped64,
                        "SDL_Swap64(): 64 bit swapped: 0x%" SDL_PRIX64 " => 0x%" SDL_PRIX64,
                        value64, SDL_Swap64(value64));

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetXYZ() functions
 * \sa SDL_GetPlatform
 * \sa SDL_GetCPUCount
 * \sa SDL_GetRevision
 * \sa SDL_GetCPUCacheLineSize
 */
static int platform_testGetFunctions(void *arg)
{
    const char *platform;
    const char *revision;
    int ret;
    size_t len;

    platform = SDL_GetPlatform();
    SDLTest_AssertPass("SDL_GetPlatform()");
    SDLTest_AssertCheck(platform != NULL, "SDL_GetPlatform() != NULL");
    if (platform != NULL) {
        len = SDL_strlen(platform);
        SDLTest_AssertCheck(len > 0,
                            "SDL_GetPlatform(): expected non-empty platform, was platform: '%s', len: %i",
                            platform,
                            (int)len);
    }

    ret = SDL_GetCPUCount();
    SDLTest_AssertPass("SDL_GetCPUCount()");
    SDLTest_AssertCheck(ret > 0,
                        "SDL_GetCPUCount(): expected count > 0, was: %i",
                        ret);

    ret = SDL_GetCPUCacheLineSize();
    SDLTest_AssertPass("SDL_GetCPUCacheLineSize()");
    SDLTest_AssertCheck(ret >= 0,
                        "SDL_GetCPUCacheLineSize(): expected size >= 0, was: %i",
                        ret);

    revision = SDL_GetRevision();
    SDLTest_AssertPass("SDL_GetRevision()");
    SDLTest_AssertCheck(revision != NULL, "SDL_GetRevision() != NULL");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_HasXYZ() functions
 * \sa SDL_HasAltiVec
 * \sa SDL_HasMMX
 * \sa SDL_HasSSE
 * \sa SDL_HasSSE2
 * \sa SDL_HasSSE3
 * \sa SDL_HasSSE41
 * \sa SDL_HasSSE42
 * \sa SDL_HasAVX
 */
static int platform_testHasFunctions(void *arg)
{
    /* TODO: independently determine and compare values as well */

    SDL_HasAltiVec();
    SDLTest_AssertPass("SDL_HasAltiVec()");

    SDL_HasMMX();
    SDLTest_AssertPass("SDL_HasMMX()");

    SDL_HasSSE();
    SDLTest_AssertPass("SDL_HasSSE()");

    SDL_HasSSE2();
    SDLTest_AssertPass("SDL_HasSSE2()");

    SDL_HasSSE3();
    SDLTest_AssertPass("SDL_HasSSE3()");

    SDL_HasSSE41();
    SDLTest_AssertPass("SDL_HasSSE41()");

    SDL_HasSSE42();
    SDLTest_AssertPass("SDL_HasSSE42()");

    SDL_HasAVX();
    SDLTest_AssertPass("SDL_HasAVX()");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetVersion
 * \sa SDL_GetVersion
 */
static int platform_testGetVersion(void *arg)
{
    int linked = SDL_GetVersion();
    SDLTest_AssertCheck(linked >= SDL_VERSION,
                        "SDL_GetVersion(): returned version %d (>= %d)",
                        linked,
                        SDL_VERSION);

    return TEST_COMPLETED;
}

/**
 * Tests default SDL_Init
 */
static int platform_testDefaultInit(void *arg)
{
    int ret;
    int subsystem;

    subsystem = SDL_WasInit(0);
    SDLTest_AssertCheck(subsystem != 0,
                        "SDL_WasInit(0): returned %i, expected != 0",
                        subsystem);

    ret = SDL_Init(0);
    SDLTest_AssertCheck(ret == 0,
                        "SDL_Init(0): returned %i, expected 0, error: %s",
                        ret,
                        SDL_GetError());

    return TEST_COMPLETED;
}

/**
 * Tests SDL_Get/Set/ClearError
 * \sa SDL_GetError
 * \sa SDL_SetError
 * \sa SDL_ClearError
 */
static int platform_testGetSetClearError(void *arg)
{
    int result;
    const char *testError = "Testing";
    const char *lastError;
    size_t len;

    SDL_ClearError();
    SDLTest_AssertPass("SDL_ClearError()");

    lastError = SDL_GetError();
    SDLTest_AssertPass("SDL_GetError()");
    SDLTest_AssertCheck(lastError != NULL,
                        "SDL_GetError() != NULL");
    if (lastError != NULL) {
        len = SDL_strlen(lastError);
        SDLTest_AssertCheck(len == 0,
                            "SDL_GetError(): no message expected, len: %i", (int)len);
    }

    result = SDL_SetError("%s", testError);
    SDLTest_AssertPass("SDL_SetError()");
    SDLTest_AssertCheck(result == -1, "SDL_SetError: expected -1, got: %i", result);
    lastError = SDL_GetError();
    SDLTest_AssertCheck(lastError != NULL,
                        "SDL_GetError() != NULL");
    if (lastError != NULL) {
        len = SDL_strlen(lastError);
        SDLTest_AssertCheck(len == SDL_strlen(testError),
                            "SDL_GetError(): expected message len %i, was len: %i",
                            (int)SDL_strlen(testError),
                            (int)len);
        SDLTest_AssertCheck(SDL_strcmp(lastError, testError) == 0,
                            "SDL_GetError(): expected message %s, was message: %s",
                            testError,
                            lastError);
    }

    /* Clean up */
    SDL_ClearError();
    SDLTest_AssertPass("SDL_ClearError()");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_SetError with empty input
 * \sa SDL_SetError
 */
static int platform_testSetErrorEmptyInput(void *arg)
{
    int result;
    const char *testError = "";
    const char *lastError;
    size_t len;

    result = SDL_SetError("%s", testError);
    SDLTest_AssertPass("SDL_SetError()");
    SDLTest_AssertCheck(result == -1, "SDL_SetError: expected -1, got: %i", result);
    lastError = SDL_GetError();
    SDLTest_AssertCheck(lastError != NULL,
                        "SDL_GetError() != NULL");
    if (lastError != NULL) {
        len = SDL_strlen(lastError);
        SDLTest_AssertCheck(len == SDL_strlen(testError),
                            "SDL_GetError(): expected message len %i, was len: %i",
                            (int)SDL_strlen(testError),
                            (int)len);
        SDLTest_AssertCheck(SDL_strcmp(lastError, testError) == 0,
                            "SDL_GetError(): expected message '%s', was message: '%s'",
                            testError,
                            lastError);
    }

    /* Clean up */
    SDL_ClearError();
    SDLTest_AssertPass("SDL_ClearError()");

    return TEST_COMPLETED;
}

#ifdef HAVE_WFORMAT_OVERFLOW
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
#endif

/**
 * Tests SDL_SetError with invalid input
 * \sa SDL_SetError
 */
static int platform_testSetErrorInvalidInput(void *arg)
{
    int result;
    const char *invalidError = "";
    const char *probeError = "Testing";
    const char *lastError;
    size_t len;

    /* Reset */
    SDL_ClearError();
    SDLTest_AssertPass("SDL_ClearError()");

    /* Check for no-op */
    result = SDL_SetError("%s", invalidError);
    SDLTest_AssertPass("SDL_SetError()");
    SDLTest_AssertCheck(result == -1, "SDL_SetError: expected -1, got: %i", result);
    lastError = SDL_GetError();
    SDLTest_AssertCheck(lastError != NULL,
                        "SDL_GetError() != NULL");
    if (lastError != NULL) {
        len = SDL_strlen(lastError);
        SDLTest_AssertCheck(len == 0 || SDL_strcmp(lastError, "(null)") == 0,
                            "SDL_GetError(): expected message len 0, was len: %i",
                            (int)len);
    }

    /* Set */
    result = SDL_SetError("%s", probeError);
    SDLTest_AssertPass("SDL_SetError('%s')", probeError);
    SDLTest_AssertCheck(result == -1, "SDL_SetError: expected -1, got: %i", result);

    /* Check for no-op */
    result = SDL_SetError("%s", invalidError);
    SDLTest_AssertPass("SDL_SetError(NULL)");
    SDLTest_AssertCheck(result == -1, "SDL_SetError: expected -1, got: %i", result);
    lastError = SDL_GetError();
    SDLTest_AssertCheck(lastError != NULL,
                        "SDL_GetError() != NULL");
    if (lastError != NULL) {
        len = SDL_strlen(lastError);
        SDLTest_AssertCheck(len == 0 || SDL_strcmp(lastError, "(null)") == 0,
                            "SDL_GetError(): expected message len 0, was len: %i",
                            (int)len);
    }

    /* Reset */
    SDL_ClearError();
    SDLTest_AssertPass("SDL_ClearError()");

    /* Set and check */
    result = SDL_SetError("%s", probeError);
    SDLTest_AssertPass("SDL_SetError()");
    SDLTest_AssertCheck(result == -1, "SDL_SetError: expected -1, got: %i", result);
    lastError = SDL_GetError();
    SDLTest_AssertCheck(lastError != NULL,
                        "SDL_GetError() != NULL");
    if (lastError != NULL) {
        len = SDL_strlen(lastError);
        SDLTest_AssertCheck(len == SDL_strlen(probeError),
                            "SDL_GetError(): expected message len %i, was len: %i",
                            (int)SDL_strlen(probeError),
                            (int)len);
        SDLTest_AssertCheck(SDL_strcmp(lastError, probeError) == 0,
                            "SDL_GetError(): expected message '%s', was message: '%s'",
                            probeError,
                            lastError);
    }

    /* Clean up */
    SDL_ClearError();
    SDLTest_AssertPass("SDL_ClearError()");

    return TEST_COMPLETED;
}

#ifdef HAVE_WFORMAT_OVERFLOW
#pragma GCC diagnostic pop
#endif

/**
 * Tests SDL_GetPowerInfo
 * \sa SDL_GetPowerInfo
 */
static int platform_testGetPowerInfo(void *arg)
{
    SDL_PowerState state;
    SDL_PowerState stateAgain;
    int secs;
    int secsAgain;
    int pct;
    int pctAgain;

    state = SDL_GetPowerInfo(&secs, &pct);
    SDLTest_AssertPass("SDL_GetPowerInfo()");
    SDLTest_AssertCheck(
        state == SDL_POWERSTATE_UNKNOWN ||
            state == SDL_POWERSTATE_ON_BATTERY ||
            state == SDL_POWERSTATE_NO_BATTERY ||
            state == SDL_POWERSTATE_CHARGING ||
            state == SDL_POWERSTATE_CHARGED,
        "SDL_GetPowerInfo(): state %i is one of the expected values",
        (int)state);

    if (state == SDL_POWERSTATE_ON_BATTERY) {
        SDLTest_AssertCheck(
            secs >= 0,
            "SDL_GetPowerInfo(): on battery, secs >= 0, was: %i",
            secs);
        SDLTest_AssertCheck(
            (pct >= 0) && (pct <= 100),
            "SDL_GetPowerInfo(): on battery, pct=[0,100], was: %i",
            pct);
    }

    if (state == SDL_POWERSTATE_UNKNOWN ||
        state == SDL_POWERSTATE_NO_BATTERY) {
        SDLTest_AssertCheck(
            secs == -1,
            "SDL_GetPowerInfo(): no battery, secs == -1, was: %i",
            secs);
        SDLTest_AssertCheck(
            pct == -1,
            "SDL_GetPowerInfo(): no battery, pct == -1, was: %i",
            pct);
    }

    /* Partial return value variations */
    stateAgain = SDL_GetPowerInfo(&secsAgain, NULL);
    SDLTest_AssertCheck(
        state == stateAgain,
        "State %i returned when only 'secs' requested",
        stateAgain);
    SDLTest_AssertCheck(
        secs == secsAgain,
        "Value %i matches when only 'secs' requested",
        secsAgain);
    stateAgain = SDL_GetPowerInfo(NULL, &pctAgain);
    SDLTest_AssertCheck(
        state == stateAgain,
        "State %i returned when only 'pct' requested",
        stateAgain);
    SDLTest_AssertCheck(
        pct == pctAgain,
        "Value %i matches when only 'pct' requested",
        pctAgain);
    stateAgain = SDL_GetPowerInfo(NULL, NULL);
    SDLTest_AssertCheck(
        state == stateAgain,
        "State %i returned when no value requested",
        stateAgain);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Platform test cases */
static const SDLTest_TestCaseReference platformTest1 = {
    (SDLTest_TestCaseFp)platform_testTypes, "platform_testTypes", "Tests predefined types", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest2 = {
    (SDLTest_TestCaseFp)platform_testEndianessAndSwap, "platform_testEndianessAndSwap", "Tests endianness and swap functions", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest3 = {
    (SDLTest_TestCaseFp)platform_testGetFunctions, "platform_testGetFunctions", "Tests various SDL_GetXYZ functions", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest4 = {
    (SDLTest_TestCaseFp)platform_testHasFunctions, "platform_testHasFunctions", "Tests various SDL_HasXYZ functions", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest5 = {
    (SDLTest_TestCaseFp)platform_testGetVersion, "platform_testGetVersion", "Tests SDL_GetVersion function", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest6 = {
    (SDLTest_TestCaseFp)platform_testDefaultInit, "platform_testDefaultInit", "Tests default SDL_Init", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest7 = {
    (SDLTest_TestCaseFp)platform_testGetSetClearError, "platform_testGetSetClearError", "Tests SDL_Get/Set/ClearError", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest8 = {
    (SDLTest_TestCaseFp)platform_testSetErrorEmptyInput, "platform_testSetErrorEmptyInput", "Tests SDL_SetError with empty input", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest9 = {
    (SDLTest_TestCaseFp)platform_testSetErrorInvalidInput, "platform_testSetErrorInvalidInput", "Tests SDL_SetError with invalid input", TEST_ENABLED
};

static const SDLTest_TestCaseReference platformTest10 = {
    (SDLTest_TestCaseFp)platform_testGetPowerInfo, "platform_testGetPowerInfo", "Tests SDL_GetPowerInfo function", TEST_ENABLED
};

/* Sequence of Platform test cases */
static const SDLTest_TestCaseReference *platformTests[] = {
    &platformTest1,
    &platformTest2,
    &platformTest3,
    &platformTest4,
    &platformTest5,
    &platformTest6,
    &platformTest7,
    &platformTest8,
    &platformTest9,
    &platformTest10,
    NULL
};

/* Platform test suite (global) */
SDLTest_TestSuiteReference platformTestSuite = {
    "Platform",
    NULL,
    platformTests,
    NULL
};
