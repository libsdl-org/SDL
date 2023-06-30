/**
 * GUID test suite
 */

#include "SDL.h"
#include "SDL_test.h"

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
 * @brief Check String-to-GUID conversion
 *
 * @sa SDL_GUIDFromString
 */
static int
TestGuidFromString(void *arg)
{
    int i;

    SDLTest_AssertPass("Call to SDL_GUIDFromString");
    for (i = 0; i < NUM_TEST_GUIDS; ++i) {
        Uint8 expected[16];
        SDL_GUID guid;

        upper_lower_to_bytestring(expected,
                                  test_guids[i].upper, test_guids[i].lower);

        guid = SDL_GUIDFromString(test_guids[i].str);
        SDLTest_AssertCheck(SDL_memcmp(expected, guid.data, 16) == 0, "GUID from string, GUID was: '%s'", test_guids[i].str);
    }

    return TEST_COMPLETED;
}

/**
 * @brief Check GUID-to-String conversion
 *
 * @sa SDL_GUIDToString
 */
static int
TestGuidToString(void *arg)
{
    int i;

    SDLTest_AssertPass("Call to SDL_GUIDToString");
    for (i = 0; i < NUM_TEST_GUIDS; ++i) {
        const int guid_str_offset = 4;
        char guid_str_buf[64];
        char *guid_str = guid_str_buf + guid_str_offset;
        SDL_GUID guid;
        int size;

        upper_lower_to_bytestring(guid.data,
                                  test_guids[i].upper, test_guids[i].lower);

        /* Serialise to limited-length buffers */
        for (size = 0; size <= 36; ++size) {
            const Uint8 fill_char = size + 0xa0;
            Uint32 expected_prefix;
            Uint32 actual_prefix;
            int written_size;

            SDL_memset(guid_str_buf, fill_char, sizeof(guid_str_buf));
            SDL_GUIDToString(guid, guid_str, size);

            /* Check bytes before guid_str_buf */
            expected_prefix = fill_char | (fill_char << 8) | (fill_char << 16) | (fill_char << 24);
            SDL_memcpy(&actual_prefix, guid_str_buf, 4);
            SDLTest_AssertCheck(expected_prefix == actual_prefix, "String buffer memory before output untouched, expected: %" SDL_PRIu32 ", got: %" SDL_PRIu32 ", at size=%d", expected_prefix, actual_prefix, size);

            /* Check that we did not overwrite too much */
            written_size = 0;
            while ((guid_str[written_size] & 0xff) != fill_char && written_size < 256) {
                ++written_size;
            }
            SDLTest_AssertCheck(written_size <= size, "Output length is within expected bounds, with length %d: wrote %d of %d permitted bytes", size, written_size, size);
            if (size >= 33) {
                SDLTest_AssertCheck(SDL_strcmp(guid_str, test_guids[i].str) == 0, "GUID string equality, from string: %s", test_guids[i].str);
            }
        }
    }

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* GUID routine test cases */
static const SDLTest_TestCaseReference guidTest1 = {
    (SDLTest_TestCaseFp)TestGuidFromString, "TestGuidFromString", "Call to SDL_GUIDFromString", TEST_ENABLED
};

static const SDLTest_TestCaseReference guidTest2 = {
    (SDLTest_TestCaseFp)TestGuidToString, "TestGuidToString", "Call to SDL_GUIDToString", TEST_ENABLED
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
