/*
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <stdio.h>

#include "SDL.h"

/*
 * Watcom C flags these as Warning 201: "Unreachable code" if you just
 *  compare them directly, so we push it through a function to keep the
 *  compiler quiet.  --ryan.
 */
static int
badsize(size_t sizeoftype, size_t hardcodetype)
{
    return sizeoftype != hardcodetype;
}

int
TestTypes(SDL_bool verbose)
{
    int error = 0;

    if (badsize(sizeof(Uint8), 1)) {
        if (verbose)
            SDL_Log("sizeof(Uint8) != 1, instead = %u\n",
                   (unsigned int)sizeof(Uint8));
        ++error;
    }
    if (badsize(sizeof(Uint16), 2)) {
        if (verbose)
            SDL_Log("sizeof(Uint16) != 2, instead = %u\n",
                   (unsigned int)sizeof(Uint16));
        ++error;
    }
    if (badsize(sizeof(Uint32), 4)) {
        if (verbose)
            SDL_Log("sizeof(Uint32) != 4, instead = %u\n",
                   (unsigned int)sizeof(Uint32));
        ++error;
    }
    if (badsize(sizeof(Uint64), 8)) {
        if (verbose)
            SDL_Log("sizeof(Uint64) != 8, instead = %u\n",
                   (unsigned int)sizeof(Uint64));
        ++error;
    }
    if (verbose && !error)
        SDL_Log("All data types are the expected size.\n");

    return (error ? 1 : 0);
}

int
TestEndian(SDL_bool verbose)
{
    int error = 0;
    Uint16 value = 0x1234;
    int real_byteorder;
    Uint16 value16 = 0xCDAB;
    Uint16 swapped16 = 0xABCD;
    Uint32 value32 = 0xEFBEADDE;
    Uint32 swapped32 = 0xDEADBEEF;
    Uint64 value64, swapped64;

    value64 = 0xEFBEADDE;
    value64 <<= 32;
    value64 |= 0xCDAB3412;
    swapped64 = 0x1234ABCD;
    swapped64 <<= 32;
    swapped64 |= 0xDEADBEEF;

    if (verbose) {
        SDL_Log("Detected a %s endian machine.\n",
               (SDL_BYTEORDER == SDL_LIL_ENDIAN) ? "little" : "big");
    }
    if ((*((char *) &value) >> 4) == 0x1) {
        real_byteorder = SDL_BIG_ENDIAN;
    } else {
        real_byteorder = SDL_LIL_ENDIAN;
    }
    if (real_byteorder != SDL_BYTEORDER) {
        if (verbose) {
            SDL_Log("Actually a %s endian machine!\n",
                   (real_byteorder == SDL_LIL_ENDIAN) ? "little" : "big");
        }
        ++error;
    }
    if (verbose) {
        SDL_Log("Value 16 = 0x%X, swapped = 0x%X\n", value16,
               SDL_Swap16(value16));
    }
    if (SDL_Swap16(value16) != swapped16) {
        if (verbose) {
            SDL_Log("16 bit value swapped incorrectly!\n");
        }
        ++error;
    }
    if (verbose) {
        SDL_Log("Value 32 = 0x%X, swapped = 0x%X\n", value32,
               SDL_Swap32(value32));
    }
    if (SDL_Swap32(value32) != swapped32) {
        if (verbose) {
            SDL_Log("32 bit value swapped incorrectly!\n");
        }
        ++error;
    }
    if (verbose) {
        SDL_Log("Value 64 = 0x%"SDL_PRIX64", swapped = 0x%"SDL_PRIX64"\n", value64,
               SDL_Swap64(value64));
    }
    if (SDL_Swap64(value64) != swapped64) {
        if (verbose) {
            SDL_Log("64 bit value swapped incorrectly!\n");
        }
        ++error;
    }
    return (error ? 1 : 0);
}


int
TestCPUInfo(SDL_bool verbose)
{
    if (verbose) {
        SDL_Log("CPU count: %d\n", SDL_GetCPUCount());
        SDL_Log("CPU cache line size: %d\n", SDL_GetCPUCacheLineSize());
        SDL_Log("RDTSC %s\n", SDL_HasRDTSC()? "detected" : "not detected");
        SDL_Log("AltiVec %s\n", SDL_HasAltiVec()? "detected" : "not detected");
        SDL_Log("MMX %s\n", SDL_HasMMX()? "detected" : "not detected");
        SDL_Log("3DNow! %s\n", SDL_Has3DNow()? "detected" : "not detected");
        SDL_Log("SSE %s\n", SDL_HasSSE()? "detected" : "not detected");
        SDL_Log("SSE2 %s\n", SDL_HasSSE2()? "detected" : "not detected");
        SDL_Log("SSE3 %s\n", SDL_HasSSE3()? "detected" : "not detected");
        SDL_Log("SSE4.1 %s\n", SDL_HasSSE41()? "detected" : "not detected");
        SDL_Log("SSE4.2 %s\n", SDL_HasSSE42()? "detected" : "not detected");
        SDL_Log("AVX %s\n", SDL_HasAVX()? "detected" : "not detected");
        SDL_Log("System RAM %d MB\n", SDL_GetSystemRAM());
    }
    return (0);
}

int
TestAssertions(SDL_bool verbose)
{
    SDL_assert(1);
    SDL_assert_release(1);
    SDL_assert_paranoid(1);
    SDL_assert(0 || 1);
    SDL_assert_release(0 || 1);
    SDL_assert_paranoid(0 || 1);

#if 0   /* enable this to test assertion failures. */
    SDL_assert_release(1 == 2);
    SDL_assert_release(5 < 4);
    SDL_assert_release(0 && "This is a test");
#endif

    {
        const SDL_AssertData *item = SDL_GetAssertionReport();
        while (item) {
            SDL_Log("'%s', %s (%s:%d), triggered %u times, always ignore: %s.\n",
                item->condition, item->function, item->filename,
                item->linenum, item->trigger_count,
                item->always_ignore ? "yes" : "no");
            item = item->next;
        }
    }
    return (0);
}

int
main(int argc, char *argv[])
{
    SDL_bool verbose = SDL_TRUE;
    int status = 0;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argv[1] && (SDL_strcmp(argv[1], "-q") == 0)) {
        verbose = SDL_FALSE;
    }
    if (verbose) {
        SDL_Log("This system is running %s\n", SDL_GetPlatform());
    }

    status += TestTypes(verbose);
    status += TestEndian(verbose);
    status += TestCPUInfo(verbose);
    status += TestAssertions(verbose);

    return status;
}
