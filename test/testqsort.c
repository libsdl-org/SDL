/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "SDL_test.h"

static int SDLCALL
num_compare(const void *_a, const void *_b)
{
    const int a = *((const int *)_a);
    const int b = *((const int *)_b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static void
test_sort(const char *desc, int *nums, const int arraylen)
{
    int i;
    int prev;

    SDL_Log("test: %s arraylen=%d", desc, arraylen);

    SDL_qsort(nums, arraylen, sizeof(nums[0]), num_compare);

    prev = nums[0];
    for (i = 1; i < arraylen; i++) {
        const int val = nums[i];
        if (val < prev) {
            SDL_Log("sort is broken!");
            return;
        }
        prev = val;
    }
}

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    static int nums[1024 * 100];
    static const int itervals[] = { SDL_arraysize(nums), 12 };
    int iteration;
    int i;
    SDL_bool custom_seed = SDL_FALSE;
    Uint64 seed;
    SDLTest_RandomContext rndctx;

    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDLTest_CommonCreateState failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!custom_seed) {
                int success;

                if (argv[i][0] == '0' && argv[i][1] == 'x') {
                    success = SDL_sscanf(argv[i] + 2, "%" SDL_PRIx64, &seed);
                } else {
                    success = SDL_sscanf(argv[i], "%" SDL_PRIu64, &seed);
                }
                if (!success) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid seed. Use a decimal or hexadecimal number.\n");
                    return 1;
                }
                if (seed <= ((Uint64)0xffffffff)) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Seed must be equal or greater than 0x100000000.\n");
                    return 1;
                }
                custom_seed = SDL_TRUE;
                consumed = 1;
            }
        }

        if (consumed <= 0) {
            static const char *options[] = { "[SEED]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (custom_seed) {
        SDLTest_RandomInit(&rndctx, (unsigned int)(seed >> 32), (unsigned int)(seed & 0xffffffff));
    } else {
        SDLTest_RandomInitTime(&rndctx);
    }
    SDL_Log("Using random seed 0x%08x%08x\n", rndctx.x, rndctx.c);

    if (!SDLTest_CommonInit(state)) {
        return 1;
    }

    for (iteration = 0; iteration < SDL_arraysize(itervals); iteration++) {
        const int arraylen = itervals[iteration];

        for (i = 0; i < arraylen; i++) {
            nums[i] = i;
        }
        test_sort("already sorted", nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = i;
        }
        nums[arraylen - 1] = -1;
        test_sort("already sorted except last element", nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = (arraylen - 1) - i;
        }
        test_sort("reverse sorted", nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = SDLTest_RandomInt(&rndctx);
        }
        test_sort("random sorted", nums, arraylen);
    }

    SDLTest_CommonQuit(state);

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */

