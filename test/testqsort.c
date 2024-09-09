/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static int a_global_var = 77;

static int SDLCALL
num_compare(const void *_a, const void *_b)
{
    const int a = *((const int *)_a);
    const int b = *((const int *)_b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static int SDLCALL
num_compare_r(void *userdata, const void *a, const void *b)
{
    if (userdata != &a_global_var) {
        SDL_Log("Uhoh, invalid userdata during qsort!");
    }
    return num_compare(a, b);
}

static void
test_sort(const char *desc, int *nums, const int arraylen)
{
    static int nums_copy[1024 * 100];
    int i;
    int prev;

    SDL_assert(SDL_arraysize(nums_copy) >= arraylen);

    SDL_Log("test: %s arraylen=%d", desc, arraylen);

    SDL_memcpy(nums_copy, nums, arraylen * sizeof (*nums));

    SDL_qsort(nums, arraylen, sizeof(nums[0]), num_compare);
    SDL_qsort_r(nums_copy, arraylen, sizeof(nums[0]), num_compare_r, &a_global_var);

    prev = nums[0];
    for (i = 1; i < arraylen; i++) {
        const int val = nums[i];
        const int val2 = nums_copy[i];
        if ((val < prev) || (val != val2)) {
            SDL_Log("sort is broken!");
            return;
        }
        prev = val;
    }
}

int main(int argc, char *argv[])
{
    static int nums[1024 * 100];
    static const int itervals[] = { SDL_arraysize(nums), 12 };
    int i;
    int iteration;
    SDLTest_CommonState *state;
    Uint64 seed = 0;
    int seed_seen = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!seed_seen) {
                char *endptr = NULL;

                seed = (Uint64)SDL_strtoull(argv[i], &endptr, 0);
                if (endptr != argv[i] && *endptr == '\0') {
                    seed_seen = 1;
                    consumed = 1;
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid seed. Use a decimal or hexadecimal number.\n");
                    return 1;
                }
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[seed]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!seed_seen) {
        seed = SDL_GetPerformanceCounter();
    }
    SDL_Log("Using random seed 0x%" SDL_PRIx64 "\n", seed);

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
            nums[i] = SDL_rand_r(&seed, 1000000);
        }
        test_sort("random sorted", nums, arraylen);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
