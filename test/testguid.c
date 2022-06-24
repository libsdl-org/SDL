/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/**
 * Automated tests for GUID processing
 */

#include <stdio.h>

#include "SDL.h"

/* Helpers */

static int _error_count = 0;

static int
_require_eq(Uint64 expected, Uint64 actual, int line, char *msg)
{
    if (expected != actual) {
        _error_count += 1;
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[%s, L%d] %s: Actual %ld (0x%lx) != expected %ld (0x%lx)",
                     __FILE__, line, msg, actual, actual, expected, expected);
        return 0;
    }
    return 1;
}

#define ASSERT_EQ(MSG, EXPECTED, ACTUAL) _require_eq((EXPECTED), (ACTUAL), __LINE__, (MSG))

/* Helpers */

#define NUM_TEST_GUIDS 5

static struct {
    char *str;
    Uint64 upper, lower;
} test_guids[NUM_TEST_GUIDS] = {
    { "0000000000000000"    "ffffffffffffffff",
     0x0000000000000000,   0xfffffffffffffffflu },
    { "0011223344556677"    "8091a2b3c4d5e6f0",
     0x0011223344556677lu, 0x8091a2b3c4d5e6f0lu },
    { "a011223344556677"    "8091a2b3c4d5e6f0",
     0xa011223344556677lu, 0x8091a2b3c4d5e6f0lu },
    { "a011223344556677"    "8091a2b3c4d5e6f1",
     0xa011223344556677lu, 0x8091a2b3c4d5e6f1lu },
    { "a011223344556677"    "8191a2b3c4d5e6f0",
     0xa011223344556677lu, 0x8191a2b3c4d5e6f0lu },
};

static void
upper_lower_to_bytestring(Uint8* out, Uint64 upper, Uint64 lower)
{
    Uint64 values[2];
    int i, k;

    values[0] = upper;
    values [1] = lower;

    for (i = 0; i < 2; ++i) {
        Uint64 v = values[i];

        for (k = 0; k < 8; ++k) {
            *out++ = v >> 56;
            v <<= 8;
        }
    }
}

/* ================= Test Case Implementation ================== */

/**
 * @brief Check String-to-GUID conversion
 *
 * @sa SDL_GUIDFromString
 */
static void
TestGuidFromString(void)
{
    int i;

    for (i = 0; i < NUM_TEST_GUIDS; ++i) {
        Uint8 expected[16];
        SDL_GUID guid;

        upper_lower_to_bytestring(expected,
                                  test_guids[i].upper, test_guids[i].lower);

        guid = SDL_GUIDFromString(test_guids[i].str);
        if (!ASSERT_EQ("GUID from string", 0, SDL_memcmp(expected, guid.data, 16))) {
            SDL_Log("  GUID was: '%s'", test_guids[i].str);
        }
    }
}

/**
 * @brief Check GUID-to-String conversion
 *
 * @sa SDL_GUIDToString
 */
static void
TestGuidToString(void)
{
    int i;

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
            if (!ASSERT_EQ("String buffer memory before output untouched: ", expected_prefix, actual_prefix)) {
                SDL_Log("  at size=%d", size);
            }

            /* Check that we did not overwrite too much */
            written_size = 0;
            while ((guid_str[written_size] & 0xff) != fill_char && written_size < 256) {
                ++written_size;
            }
            if (!ASSERT_EQ("Output length is within expected bounds", 1, written_size <= size)) {
                SDL_Log("  with length %d: wrote %d of %d permitted bytes",
                        size, written_size, size);
            }
            if (size >= 33) {
                if (!ASSERT_EQ("GUID string equality", 0, SDL_strcmp(guid_str, test_guids[i].str))) {
                    SDL_Log("  from string: %s", test_guids[i].str);
                }
            }
        }
    }
}

int
main(int argc, char *argv[])
{
    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    TestGuidFromString();
    TestGuidToString();

    return _error_count > 0;
}
