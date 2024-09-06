/**
 * GUID test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* ================= Test Case Implementation ================== */

/* Helper functions */

#define NUM_TEST_GUIDS 5

#ifndef UINT64_C
#ifdef _MSC_VER
#define UINT64_C(x) x##ui64
#elif defined(_LP64)
#define UINT64_C(x) x##UL
#else
#define UINT64_C(x) x##ULL
#endif
#endif

static struct
{
    char *str;
    Uint64 upper, lower;
} test_guids[NUM_TEST_GUIDS] = {
    { "0000000000000000"
      "ffffffffffffffff",
      UINT64_C(0x0000000000000000), UINT64_C(0xffffffffffffffff) },
    { "0011223344556677"
      "8091a2b3c4d5e6f0",
      UINT64_C(0x0011223344556677), UINT64_C(0x8091a2b3c4d5e6f0) },
    { "a011223344556677"
      "8091a2b3c4d5e6f0",
      UINT64_C(0xa011223344556677), UINT64_C(0x8091a2b3c4d5e6f0) },
    { "a011223344556677"
      "8091a2b3c4d5e6f1",
      UINT64_C(0xa011223344556677), UINT64_C(0x8091a2b3c4d5e6f1) },
    { "a011223344556677"
      "8191a2b3c4d5e6f0",
      UINT64_C(0xa011223344556677), UINT64_C(0x8191a2b3c4d5e6f0) },
};

static void
upper_lower_to_bytestring(Uint8 *out, Uint64 upper, Uint64 lower)
{
    Uint64 values[2];
    int i, k;

    values[0] = upper;
    values[1] = lower;

    for (i = 0; i < 2; ++i) {
        Uint64 v = values[i];

        for (k = 0; k < 8; ++k) {
            *out++ = v >> 56;
            v <<= 8;
        }
    }
}

/* Test case functions */

/**
 * Check String-to-GUID conversion
 *
 * \sa SDL_StringToGUID
 */
static int SDLCALL
TestStringToGUID(void *arg)
{
    int i;

    SDLTest_AssertPass("Call to SDL_StringToGUID");
    for (i = 0; i < NUM_TEST_GUIDS; ++i) {
        Uint8 expected[16];
        SDL_GUID guid;

        upper_lower_to_bytestring(expected,
                                  test_guids[i].upper, test_guids[i].lower);

        guid = SDL_StringToGUID(test_guids[i].str);
        SDLTest_AssertCheck(SDL_memcmp(expected, guid.data, 16) == 0, "GUID from string, GUID was: '%s'", test_guids[i].str);
    }

    return TEST_COMPLETED;
}

/**
 * Check GUID-to-String conversion
 *
 * \sa SDL_GUIDToString
 */
static int SDLCALL
TestGUIDToString(void *arg)
{
    int i;

    SDLTest_AssertPass("Call to SDL_GUIDToString");
    for (i = 0; i < NUM_TEST_GUIDS; ++i) {
        char guid_str[33];
        SDL_GUID guid;

        upper_lower_to_bytestring(guid.data,
                                  test_guids[i].upper, test_guids[i].lower);

        SDL_GUIDToString(guid, guid_str, sizeof(guid_str));
        SDLTest_AssertCheck(SDL_strcmp(guid_str, test_guids[i].str) == 0, "Checking whether strings match, expected %s, got %s\n", test_guids[i].str, guid_str);
    }

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* GUID routine test cases */
static const SDLTest_TestCaseReference guidTest1 = {
    TestStringToGUID, "TestStringToGUID", "Call to SDL_StringToGUID", TEST_ENABLED
};

static const SDLTest_TestCaseReference guidTest2 = {
    TestGUIDToString, "TestGUIDToString", "Call to SDL_GUIDToString", TEST_ENABLED
};

/* Sequence of GUID routine test cases */
static const SDLTest_TestCaseReference *guidTests[] = {
    &guidTest1,
    &guidTest2,
    NULL
};

/* GUID routine test suite (global) */
SDLTest_TestSuiteReference guidTestSuite = {
    "GUID",
    NULL,
    guidTests,
    NULL
};
