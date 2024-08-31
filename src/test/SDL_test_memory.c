/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_config.h"
#include "SDL_assert.h"
#include "SDL_atomic.h"
#include "SDL_stdinc.h"
#include "SDL_log.h"
#include "SDL_test_crc32.h"
#include "SDL_test_memory.h"

#ifdef HAVE_LIBUNWIND_H
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#ifndef unw_get_proc_name_by_ip
#define SDLTEST_UNWIND_NO_PROC_NAME_BY_IP
static SDL_bool s_unwind_symbol_names = SDL_TRUE;
#endif
#endif

/* This is a simple tracking allocator to demonstrate the use of SDL's
   memory allocation replacement functionality.

   It gets slow with large numbers of allocations and shouldn't be used
   for production code.
*/

typedef struct SDL_tracked_allocation
{
    void *mem;
    size_t size;
    Uint64 stack[10];
    struct SDL_tracked_allocation *next;
#ifdef SDLTEST_UNWIND_NO_PROC_NAME_BY_IP
    char stack_names[10][256];
#endif
} SDL_tracked_allocation;

static SDLTest_Crc32Context s_crc32_context;
static SDL_malloc_func SDL_malloc_orig = NULL;
static SDL_calloc_func SDL_calloc_orig = NULL;
static SDL_realloc_func SDL_realloc_orig = NULL;
static SDL_free_func SDL_free_orig = NULL;
static int s_previous_allocations = 0;
static SDL_tracked_allocation *s_tracked_allocations[256];
static SDL_atomic_t s_lock;

#define LOCK_ALLOCATOR()                               \
    do {                                               \
        if (SDL_AtomicCAS(&s_lock, 0, 1)) {            \
            break;                                     \
        }                                              \
        SDL_CPUPauseInstruction();                     \
    } while (SDL_TRUE)
#define UNLOCK_ALLOCATOR() do { SDL_AtomicSet(&s_lock, 0); } while (0)

static unsigned int get_allocation_bucket(void *mem)
{
    CrcUint32 crc_value;
    unsigned int index;
    SDLTest_Crc32Calc(&s_crc32_context, (CrcUint8 *)&mem, sizeof(mem), &crc_value);
    index = (crc_value & (SDL_arraysize(s_tracked_allocations) - 1));
    return index;
}

static SDL_bool SDL_IsAllocationTracked(void *mem)
{
    SDL_tracked_allocation *entry;
    int index;

    LOCK_ALLOCATOR();
    index = get_allocation_bucket(mem);
    for (entry = s_tracked_allocations[index]; entry; entry = entry->next) {
        if (mem == entry->mem) {
            UNLOCK_ALLOCATOR();
            return SDL_TRUE;
        }
    }
    UNLOCK_ALLOCATOR();
    return SDL_FALSE;
}

static void SDL_TrackAllocation(void *mem, size_t size)
{
    SDL_tracked_allocation *entry;
    int index = get_allocation_bucket(mem);

    if (SDL_IsAllocationTracked(mem)) {
        return;
    }
    LOCK_ALLOCATOR();
    entry = (SDL_tracked_allocation *)SDL_malloc_orig(sizeof(*entry));
    if (!entry) {
        UNLOCK_ALLOCATOR();
        return;
    }
    entry->mem = mem;
    entry->size = size;

    /* Generate the stack trace for the allocation */
    SDL_zeroa(entry->stack);
#ifdef HAVE_LIBUNWIND_H
    {
        int stack_index;
        unw_cursor_t cursor;
        unw_context_t context;

        unw_getcontext(&context);
        unw_init_local(&cursor, &context);

        stack_index = 0;
        while (unw_step(&cursor) > 0) {
            unw_word_t pc;
#ifdef SDLTEST_UNWIND_NO_PROC_NAME_BY_IP
            unw_word_t offset;
            char sym[236];
#endif

            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            entry->stack[stack_index] = pc;

#ifdef SDLTEST_UNWIND_NO_PROC_NAME_BY_IP
            if (s_unwind_symbol_names && unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
                SDL_snprintf(entry->stack_names[stack_index], sizeof(entry->stack_names[stack_index]), "%s+0x%llx", sym, (unsigned long long)offset);
            }
#endif
            ++stack_index;

            if (stack_index == SDL_arraysize(entry->stack)) {
                break;
            }
        }
    }
#endif /* HAVE_LIBUNWIND_H */

    entry->next = s_tracked_allocations[index];
    s_tracked_allocations[index] = entry;
    UNLOCK_ALLOCATOR();
}

static void SDL_UntrackAllocation(void *mem)
{
    SDL_tracked_allocation *entry, *prev;
    int index = get_allocation_bucket(mem);

    LOCK_ALLOCATOR();
    prev = NULL;
    for (entry = s_tracked_allocations[index]; entry; entry = entry->next) {
        if (mem == entry->mem) {
            if (prev) {
                prev->next = entry->next;
            } else {
                s_tracked_allocations[index] = entry->next;
            }
            SDL_free_orig(entry);
            UNLOCK_ALLOCATOR();
            return;
        }
        prev = entry;
    }
    UNLOCK_ALLOCATOR();
}

static void *SDLCALL SDLTest_TrackedMalloc(size_t size)
{
    void *mem;

    mem = SDL_malloc_orig(size);
    if (mem) {
        SDL_TrackAllocation(mem, size);
    }
    return mem;
}

static void *SDLCALL SDLTest_TrackedCalloc(size_t nmemb, size_t size)
{
    void *mem;

    mem = SDL_calloc_orig(nmemb, size);
    if (mem) {
        SDL_TrackAllocation(mem, nmemb * size);
    }
    return mem;
}

static void *SDLCALL SDLTest_TrackedRealloc(void *ptr, size_t size)
{
    void *mem;

    SDL_assert(ptr == NULL || SDL_IsAllocationTracked(ptr));
    mem = SDL_realloc_orig(ptr, size);
    if (mem && mem != ptr) {
        if (ptr) {
            SDL_UntrackAllocation(ptr);
        }
        SDL_TrackAllocation(mem, size);
    }
    return mem;
}

static void SDLCALL SDLTest_TrackedFree(void *ptr)
{
    if (!ptr) {
        return;
    }

    if (!s_previous_allocations) {
        SDL_assert(SDL_IsAllocationTracked(ptr));
    }
    SDL_UntrackAllocation(ptr);
    SDL_free_orig(ptr);
}

int SDLTest_TrackAllocations(void)
{
    if (SDL_malloc_orig) {
        return 0;
    }

    SDLTest_Crc32Init(&s_crc32_context);

    s_previous_allocations = SDL_GetNumAllocations();
    if (s_previous_allocations != 0) {
        SDL_Log("SDLTest_TrackAllocations(): There are %d previous allocations, disabling free() validation", s_previous_allocations);
    }
#ifdef SDLTEST_UNWIND_NO_PROC_NAME_BY_IP
    do {
        /* Don't use SDL_GetHint: SDL_malloc is off limits. */
        const char *env_trackmem = SDL_getenv("SDL_TRACKMEM_SYMBOL_NAMES");
        if (env_trackmem) {
            if (SDL_strcasecmp(env_trackmem, "1") == 0 || SDL_strcasecmp(env_trackmem, "yes") == 0 || SDL_strcasecmp(env_trackmem, "true") == 0) {
                s_unwind_symbol_names = SDL_TRUE;
            } else if (SDL_strcasecmp(env_trackmem, "0") == 0 || SDL_strcasecmp(env_trackmem, "no") == 0 || SDL_strcasecmp(env_trackmem, "false") == 0) {
                s_unwind_symbol_names = SDL_FALSE;
            }
        }
    } while (0);
#endif

    SDL_GetMemoryFunctions(&SDL_malloc_orig,
                           &SDL_calloc_orig,
                           &SDL_realloc_orig,
                           &SDL_free_orig);

    SDL_SetMemoryFunctions(SDLTest_TrackedMalloc,
                           SDLTest_TrackedCalloc,
                           SDLTest_TrackedRealloc,
                           SDLTest_TrackedFree);
    return 0;
}

void SDLTest_LogAllocations(void)
{
    char *message = NULL;
    size_t message_size = 0;
    char line[128], *tmp;
    SDL_tracked_allocation *entry;
    int index, count, stack_index;
    Uint64 total_allocated;

    if (!SDL_malloc_orig) {
        return;
    }

    message = SDL_realloc_orig(NULL, 1);
    if (!message) {
        return;
    }
    *message = 0;

#define ADD_LINE()                                         \
    message_size += (SDL_strlen(line) + 1);                \
    tmp = (char *)SDL_realloc_orig(message, message_size); \
    if (!tmp) {                                            \
        return;                                            \
    }                                                      \
    message = tmp;                                         \
    SDL_strlcat(message, line, message_size)

    SDL_strlcpy(line, "Memory allocations:\n", sizeof(line));
    ADD_LINE();
    SDL_strlcpy(line, "Expect 2 allocations from within SDL_GetErrBuf()\n", sizeof(line));
    ADD_LINE();

    count = 0;
    total_allocated = 0;
    for (index = 0; index < SDL_arraysize(s_tracked_allocations); ++index) {
        for (entry = s_tracked_allocations[index]; entry; entry = entry->next) {
            (void)SDL_snprintf(line, sizeof(line), "Allocation %d: %d bytes\n", count, (int)entry->size);
            ADD_LINE();
            /* Start at stack index 1 to skip our tracking functions */
            for (stack_index = 1; stack_index < SDL_arraysize(entry->stack); ++stack_index) {
                char stack_entry_description[256] = "???";
                if (!entry->stack[stack_index]) {
                    break;
                }
#ifdef HAVE_LIBUNWIND_H
                {
#ifdef SDLTEST_UNWIND_NO_PROC_NAME_BY_IP
                    if (s_unwind_symbol_names) {
                        (void)SDL_snprintf(stack_entry_description, sizeof(stack_entry_description), "%s", entry->stack_names[stack_index]);
                    }
#else
                    char name[256] = "???";
                    unw_word_t offset = 0;
                    unw_get_proc_name_by_ip(unw_local_addr_space, entry->stack[stack_index], name, sizeof(name), &offset, NULL);
                    (void)SDL_snprintf(stack_entry_description, sizeof(stack_entry_description), "%s+0x%llx", name, (long long unsigned int)offset);
#endif
                }
#endif
                (void)SDL_snprintf(line, sizeof(line), "\t0x%" SDL_PRIx64 ": %s\n", entry->stack[stack_index], stack_entry_description);

                ADD_LINE();
            }
            total_allocated += entry->size;
            ++count;
        }
    }
    (void)SDL_snprintf(line, sizeof(line), "Total: %.2f Kb in %d allocations\n", total_allocated / 1024.0, count);
    ADD_LINE();
#undef ADD_LINE

    SDL_Log("%s", message);
}

/* vi: set ts=4 sw=4 expandtab: */
