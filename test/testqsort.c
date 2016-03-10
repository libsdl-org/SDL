/*
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"

static int
num_compare(const void *_a, const void *_b)
{
    const int a = *((const int *) _a);
    const int b = *((const int *) _b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static void
test_sort(const char *desc, int *nums, const int arraylen)
{
    int i;
    int prev;

    SDL_Log("test: %s arraylen=%d", desc, arraylen);

    SDL_qsort(nums, arraylen, sizeof (nums[0]), num_compare);

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

int
main(int argc, char *argv[])
{
    static int nums[1024 * 100];
    static const int itervals[] = { SDL_arraysize(nums), 12 };
    int iteration;

    for (iteration = 0; iteration < SDL_arraysize(itervals); iteration++) {
        const int arraylen = itervals[iteration];
        int i;

        for (i = 0; i < arraylen; i++) {
            nums[i] = i;
        }
        test_sort("already sorted", nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = i;
        }
        nums[arraylen-1] = -1;
        test_sort("already sorted except last element", nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = (arraylen-1) - i;
        }
        test_sort("reverse sorted", nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = random();
        }
        test_sort("random sorted", nums, arraylen);
    }

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */

