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

/* Required to test non-aligned sort */
typedef char non_word_value[sizeof(int) * 2];

typedef struct
{
    size_t len;
    size_t item_size;
    void *aligned;
    void *unaligned;
} test_buffer;

static int a_global_var = 77;
static int qsort_is_broken;
static SDLTest_RandomContext rndctx;

static int SDLCALL
num_compare(const void *_a, const void *_b)
{
    const int a = *((const int *)_a);
    const int b = *((const int *)_b);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

static int SDLCALL
num_compare_non_transitive(const void *_a, const void *_b)
{
    const int a = *((const int *)_a);
    const int b = *((const int *)_b);
    return (a < b) ? 0 : 1;
}

static int SDLCALL
num_compare_r(void *userdata, const void *a, const void *b)
{
    if (userdata != &a_global_var) {
        SDL_Log("Uhoh, invalid userdata during qsort!");
        qsort_is_broken = 1;
    }
    return num_compare(a, b);
}

/* Will never return 0 */
static int SDLCALL
num_compare_random_any(const void *a, const void *b)
{
    (void)(a);
    (void)(b);
    return (SDLTest_Random(&rndctx) > ((Uint32)SDL_MAX_SINT32)) ? 1 : -1;
}

static int SDLCALL
num_compare_non_word(const void *_a, const void *_b)
{
    const int a = (int)(((const char *)_a)[0]);
    const int b = (int)(((const char *)_b)[0]);
    return num_compare(&a, &b);
}

static int SDLCALL
num_compare_non_word_r(void *userdata, const void *_a, const void *_b)
{
    const int a = (int)(((const char *)_a)[0]);
    const int b = (int)(((const char *)_b)[0]);
    return num_compare_r(userdata, &a, &b);
}

static int SDLCALL
num_compare_non_word_non_transitive(const void *_a, const void *_b)
{
    const int a = (int)(((char *)_a)[0]);
    const int b = (int)(((char *)_b)[0]);
    return num_compare_non_transitive(&a, &b);
}

static void
alloc_buffer(test_buffer *buffer, size_t size, size_t len)
{
    uintptr_t base;

    SDL_memset(buffer, 0, sizeof(*buffer));
    buffer->len = len;
    buffer->item_size = size;

    if (len == 0) {
        return;
    }

    buffer->aligned = SDL_malloc((len + 1) * size + sizeof(char));
    base = (uintptr_t)(buffer->aligned);
    /* Should be aligned */
    SDL_assert(((base | size) & (sizeof(int) - 1)) == 0);

    buffer->unaligned = (void *)(base + sizeof(char));
}

static void
check_sort(const int *nums, const int *r_nums, int numlen) {
    int i;
    int prev = 0;

    if (numlen > 0) {
        prev = nums[0];
    }
    for (i = 1; i < numlen; i++) {
        const int val = nums[i];
        const int val2 = r_nums[i];
        if ((val < prev) || (val != val2)) {
            SDL_Log("sort is broken!");
            qsort_is_broken = 1;
            break;
        }
        prev = val;
    }
}

static void
test_sort(const char *desc, const int *nums, int numlen)
{
    test_buffer buffer;
    test_buffer buffer_r;

    alloc_buffer(&buffer, sizeof(nums[0]), numlen);
    alloc_buffer(&buffer_r, sizeof(nums[0]), numlen);

    SDL_Log("test: %s bufferlen=%d", desc, (int)(buffer.len));

    /* Test aligned sort */
    if (buffer.aligned != NULL) {
        /* memcpy with NULL is UB */
        SDL_memcpy(buffer.aligned, nums, numlen * sizeof(nums[0]));
        SDL_memcpy(buffer_r.aligned, nums, numlen * sizeof(nums[0]));
    }
    SDL_qsort(buffer.aligned, numlen, sizeof(nums[0]), num_compare);
    SDL_qsort_r(buffer_r.aligned, numlen, sizeof(nums[0]), num_compare_r, &a_global_var);
    check_sort(buffer.aligned, buffer_r.aligned, numlen);

    /* We can't test unaligned int sort because it's UB. */

    SDL_free(buffer_r.aligned);
    SDL_free(buffer.aligned);
}

static void
check_non_word_sort(non_word_value *nums, non_word_value *r_nums, int numlen) {
    int i;
    char prev;

    if (numlen > 0) {
        prev = nums[0][0];
    }
    for (i = 1; i < numlen; i++) {
        const char val = nums[i][0];
        const char val2 = r_nums[i][0];
        if ((val < prev) || (val != val2)) {
            SDL_Log("sort is broken!");
            qsort_is_broken = 1;
            break;
        }
        prev = val;
    }
}

static void
test_sort_non_word(const char *desc, non_word_value *nums, int numlen)
{
    test_buffer buffer;
    test_buffer buffer_r;

    alloc_buffer(&buffer, sizeof(nums[0]), numlen);
    alloc_buffer(&buffer_r, sizeof(nums[0]), numlen);

    SDL_Log("test: %s non-word numlen=%d", desc, numlen);

    /* Test aligned sort */
    if (buffer.aligned != NULL) {
        SDL_memcpy(buffer.aligned, nums, numlen * sizeof(nums[0]));
        SDL_memcpy(buffer_r.aligned, nums, numlen * sizeof(nums[0]));
    }
    SDL_qsort(buffer.aligned, numlen, sizeof(nums[0]), num_compare_non_word);
    SDL_qsort_r(buffer_r.aligned, numlen, sizeof(nums[0]), num_compare_non_word_r, &a_global_var);
    check_non_word_sort(buffer.aligned, buffer_r.aligned, numlen);

    /* Test non-aligned sort */
    if (buffer.unaligned != NULL) {
        SDL_memcpy(buffer.unaligned, nums, numlen * sizeof(nums[0]));
        SDL_memcpy(buffer_r.unaligned, nums, numlen * sizeof(nums[0]));
    }
    SDL_qsort(buffer.unaligned, numlen, sizeof(nums[0]), num_compare_non_word);
    SDL_qsort_r(buffer_r.unaligned, numlen, sizeof(nums[0]), num_compare_non_word_r, &a_global_var);
    check_non_word_sort(buffer.unaligned, buffer_r.unaligned, numlen);

    SDL_free(buffer_r.aligned);
    SDL_free(buffer.aligned);
}

static void
test_sort_non_transitive(int numlen) {
    int i;
    test_buffer buffer;
    test_buffer non_word_buffer;
    int *nums;
    non_word_value *non_word_nums;

    SDL_Log("test: non-transitive numlen=%d", (numlen));

    alloc_buffer(&buffer, sizeof(nums[0]), numlen);
    alloc_buffer(&non_word_buffer, sizeof(non_word_nums[0]), numlen);

    /* Test aligned sort */
    nums = buffer.aligned;
    non_word_nums = non_word_buffer.aligned;
    for (i = 0; i < numlen; i++) {
        nums[i] = numlen - i;
        non_word_nums[i][0] = (char)nums[i];
    }

    SDL_qsort(nums, numlen, sizeof(nums[0]), num_compare_non_transitive);
    SDL_qsort(non_word_nums, numlen, sizeof(non_word_nums[0]), num_compare_non_word_non_transitive);

    /* What's inside doesn't matter for random comparison */
    SDL_qsort(nums, numlen, sizeof(nums[0]), num_compare_random_any);
    SDL_qsort(non_word_nums, numlen, sizeof(non_word_nums[0]), num_compare_random_any);

    /* Test non-aligned sort */
    non_word_nums = non_word_buffer.unaligned;
    for (i = 0; i < numlen; i++) {
        non_word_nums[i][0] = (char)(numlen - i);
    }

    SDL_qsort(non_word_nums, numlen, sizeof(non_word_nums[0]), num_compare_non_word_non_transitive);
    SDL_qsort(non_word_nums, numlen, sizeof(non_word_nums[0]), num_compare_random_any);

    SDL_free(buffer.aligned);
    SDL_free(non_word_buffer.aligned);
}

int main(int argc, char *argv[])
{
    static int nums[1024 * 128];
    static non_word_value non_word_nums[1024 * 128];

    SDL_assert(SDL_arraysize(nums) == SDL_arraysize(non_word_nums));

    /* Test truncation points */
    static const int itervals[] = { 0, 12, 15, 12 * sizeof(int), SDL_arraysize(nums) };
    /* Non-transitive sorting consumes much more CPU, use smaller value */
    static const int itervals_non_transitive[] = { 0, 12, 15, 12 * sizeof(int), 16384};

    int i = 0;
    int iteration = 0;
    SDLTest_CommonState *state = NULL;
    int seed_seen = 0;

    SDL_zero(rndctx);
    qsort_is_broken = 0;

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
                Uint64 seed = 0;
                char *endptr = NULL;

                seed = SDL_strtoull(argv[i], &endptr, 0);
                if (endptr != argv[i] && *endptr == '\0') {
                    seed_seen = 1;
                    consumed = 1;
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid seed. Use a decimal or hexadecimal number.\n");
                    return 1;
                }
                if (seed <= ((Uint64)0xffffffff)) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Seed must be equal or greater than 0x100000000.\n");
                    return 1;
                }
                SDLTest_RandomInit(&rndctx, (unsigned int)(seed >> 32), (unsigned int)(seed & 0xffffffff));
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
        SDLTest_RandomInitTime(&rndctx);
    }
    SDL_Log("Using random seed 0x%08x%08x\n", rndctx.x, rndctx.c);

    for (iteration = 0; iteration < SDL_arraysize(itervals); iteration++) {
        const int arraylen = itervals[iteration];

        for (i = 0; i < arraylen; i++) {
            nums[i] = i;
            non_word_nums[i][0] = (char)i;
        }
        test_sort("already sorted", nums, arraylen);
        test_sort_non_word("already_sorted", non_word_nums, arraylen);

        if (arraylen > 0) {
            for (i = 0; i < arraylen; i++) {
                nums[i] = i;
                non_word_nums[i][0] = (char)i;
            }
            nums[arraylen - 1] = -1;
            test_sort("already sorted except last element", nums, arraylen);
            test_sort_non_word("already sorted except last element", non_word_nums, arraylen);
        }

        for (i = 0; i < arraylen; i++) {
            nums[i] = (arraylen - 1) - i;
            non_word_nums[i][0] = (char)i;
        }
        test_sort("reverse sorted", nums, arraylen);
        test_sort_non_word("reverse sorted", non_word_nums, arraylen);

        for (i = 0; i < arraylen; i++) {
            nums[i] = SDLTest_RandomInt(&rndctx);
            non_word_nums[i][0] = (char)i;
        }
        test_sort("random sorted", nums, arraylen);
        test_sort_non_word("random sorted", non_word_nums, arraylen);
    }

    for (iteration = 0; iteration < SDL_arraysize(itervals_non_transitive); iteration++) {
        const int arraylen = itervals_non_transitive[iteration];

        test_sort_non_transitive(arraylen);
    }

    SDLTest_CommonDestroyState(state);

    return qsort_is_broken;
}
