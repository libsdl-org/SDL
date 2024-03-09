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
#include <SDL3/SDL_test.h>

#ifdef HAVE_LIBUNWIND_H
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#ifdef SDL_PLATFORM_WIN32
#include <windows.h>
#include <dbghelp.h>

static void *s_dbghelp;

typedef BOOL (__stdcall *dbghelp_SymInitialize_fn)(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);

typedef BOOL (__stdcall *dbghelp_SymFromAddr_fn)(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);
static dbghelp_SymFromAddr_fn dbghelp_SymFromAddr;

#ifdef _WIN64
typedef BOOL (__stdcall *dbghelp_SymGetLineFromAddr_fn)(HANDLE hProcess, DWORD64 qwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line);
#else
typedef BOOL (__stdcall *dbghelp_SymGetLineFromAddr_fn)(HANDLE hProcess, DWORD qwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE Line);
#endif
static dbghelp_SymGetLineFromAddr_fn dbghelp_SymGetLineFromAddr;

/* older SDKs might not have this: */
__declspec(dllimport) USHORT WINAPI RtlCaptureStackBackTrace(ULONG FramesToSkip, ULONG FramesToCapture, PVOID* BackTrace, PULONG BackTraceHash);
#define CaptureStackBackTrace RtlCaptureStackBackTrace

#endif

/* This is a simple tracking allocator to demonstrate the use of SDL's
   memory allocation replacement functionality.

   It gets slow with large numbers of allocations and shouldn't be used
   for production code.
*/

#define MAXIMUM_TRACKED_STACK_DEPTH 32

typedef struct SDL_tracked_allocation
{
    void *mem;
    size_t size;
    Uint64 stack[MAXIMUM_TRACKED_STACK_DEPTH];
    char stack_names[MAXIMUM_TRACKED_STACK_DEPTH][256];
    struct SDL_tracked_allocation *next;
} SDL_tracked_allocation;

static SDLTest_Crc32Context s_crc32_context;
static SDL_malloc_func SDL_malloc_orig = NULL;
static SDL_calloc_func SDL_calloc_orig = NULL;
static SDL_realloc_func SDL_realloc_orig = NULL;
static SDL_free_func SDL_free_orig = NULL;
static int s_previous_allocations = 0;
static SDL_tracked_allocation *s_tracked_allocations[256];
static SDL_bool s_randfill_allocations = SDL_FALSE;

static unsigned int get_allocation_bucket(void *mem)
{
    CrcUint32 crc_value;
    unsigned int index;
    SDLTest_Crc32Calc(&s_crc32_context, (CrcUint8 *)&mem, sizeof(mem), &crc_value);
    index = (crc_value & (SDL_arraysize(s_tracked_allocations) - 1));
    return index;
}

static SDL_tracked_allocation* SDL_GetTrackedAllocation(void *mem)
{
    SDL_tracked_allocation *entry;
    int index = get_allocation_bucket(mem);
    for (entry = s_tracked_allocations[index]; entry; entry = entry->next) {
        if (mem == entry->mem) {
            return entry;
        }
    }
    return NULL;
}

static size_t SDL_GetTrackedAllocationSize(void *mem)
{
    SDL_tracked_allocation *entry = SDL_GetTrackedAllocation(mem);

    return entry ? entry->size : SIZE_MAX;
}

static SDL_bool SDL_IsAllocationTracked(void *mem)
{
    return SDL_GetTrackedAllocation(mem) != NULL;
}

static void SDL_TrackAllocation(void *mem, size_t size)
{
    SDL_tracked_allocation *entry;
    int index = get_allocation_bucket(mem);

    if (SDL_IsAllocationTracked(mem)) {
        return;
    }
    entry = (SDL_tracked_allocation *)SDL_malloc_orig(sizeof(*entry));
    if (!entry) {
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
            unw_word_t offset, pc;
            char sym[236];

            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            entry->stack[stack_index] = pc;

            if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
                SDL_snprintf(entry->stack_names[stack_index], sizeof(entry->stack_names[stack_index]), "%s+0x%llx", sym, (unsigned long long)offset);
            }
            ++stack_index;

            if (stack_index == SDL_arraysize(entry->stack)) {
                break;
            }
        }
    }
#elif defined(SDL_PLATFORM_WIN32)
    {
        Uint32 count;
        PVOID frames[63];
        Uint32 i;

        count = CaptureStackBackTrace(1, SDL_arraysize(frames), frames, NULL);

        entry->size = SDL_min(count, MAXIMUM_TRACKED_STACK_DEPTH);
        for (i = 0; i < entry->size; i++) {
            char symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbol_buffer;
            DWORD64 dwDisplacement = 0;
            DWORD lineColumn = 0;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;
            IMAGEHLP_LINE line;
            line.SizeOfStruct = sizeof(line);

            entry->stack[i] = (Uint64)(uintptr_t)frames[i];
            if (s_dbghelp) {
                if (!dbghelp_SymFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)frames[i], &dwDisplacement, pSymbol)) {
                    SDL_strlcpy(pSymbol->Name, "???", MAX_SYM_NAME);
                    dwDisplacement = 0;
                }
                if (!dbghelp_SymGetLineFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)frames[i], &lineColumn, &line)) {
                    line.FileName = "";
                    line.LineNumber = 0;
                }

                SDL_snprintf(entry->stack_names[i], sizeof(entry->stack_names[i]), "%s+0x%I64x %s:%u", pSymbol->Name, dwDisplacement, line.FileName, (Uint32)line.LineNumber);
            }
        }
    }
#endif /* HAVE_LIBUNWIND_H */

    entry->next = s_tracked_allocations[index];
    s_tracked_allocations[index] = entry;
}

static void SDL_UntrackAllocation(void *mem)
{
    SDL_tracked_allocation *entry, *prev;
    int index = get_allocation_bucket(mem);

    prev = NULL;
    for (entry = s_tracked_allocations[index]; entry; entry = entry->next) {
        if (mem == entry->mem) {
            if (prev) {
                prev->next = entry->next;
            } else {
                s_tracked_allocations[index] = entry->next;
            }
            SDL_free_orig(entry);
            return;
        }
        prev = entry;
    }
}

static void rand_fill_memory(void* ptr, size_t start, size_t end)
{
    Uint8* mem = (Uint8*) ptr;
    size_t i;

    if (!s_randfill_allocations)
        return;

    for (i = start; i < end; ++i) {
        mem[i] = SDLTest_RandomUint8();
    }
}

static void *SDLCALL SDLTest_TrackedMalloc(size_t size)
{
    void *mem;

    mem = SDL_malloc_orig(size);
    if (mem) {
        SDL_TrackAllocation(mem, size);
        rand_fill_memory(mem, 0, size);
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
    size_t old_size = 0;
    if (ptr) {
         old_size = SDL_GetTrackedAllocationSize(ptr);
         SDL_assert(old_size != SIZE_MAX);
    }
    mem = SDL_realloc_orig(ptr, size);
    if (ptr) {
        SDL_UntrackAllocation(ptr);
    }
    if (mem) {
        SDL_TrackAllocation(mem, size);
        if (size > old_size) {
            rand_fill_memory(mem, old_size, size);
        }
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

void SDLTest_TrackAllocations(void)
{
    if (SDL_malloc_orig) {
        return;
    }

    SDLTest_Crc32Init(&s_crc32_context);

    s_previous_allocations = SDL_GetNumAllocations();
    if (s_previous_allocations != 0) {
        SDL_Log("SDLTest_TrackAllocations(): There are %d previous allocations, disabling free() validation", s_previous_allocations);
    }
#ifdef SDL_PLATFORM_WIN32
    {
        s_dbghelp = SDL_LoadObject("dbghelp.dll");
        if (s_dbghelp) {
            dbghelp_SymInitialize_fn dbghelp_SymInitialize;
            dbghelp_SymInitialize = (dbghelp_SymInitialize_fn)SDL_LoadFunction(s_dbghelp, "SymInitialize");
            dbghelp_SymFromAddr = (dbghelp_SymFromAddr_fn)SDL_LoadFunction(s_dbghelp, "SymFromAddr");
#ifdef _WIN64
            dbghelp_SymGetLineFromAddr = (dbghelp_SymGetLineFromAddr_fn)SDL_LoadFunction(s_dbghelp, "SymGetLineFromAddr64");
#else
            dbghelp_SymGetLineFromAddr = (dbghelp_SymGetLineFromAddr_fn)SDL_LoadFunction(s_dbghelp, "SymGetLineFromAddr");
#endif
            if (!dbghelp_SymInitialize || !dbghelp_SymFromAddr || !dbghelp_SymGetLineFromAddr) {
                SDL_UnloadObject(s_dbghelp);
                s_dbghelp = NULL;
            } else {
                if (!dbghelp_SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
                    SDL_UnloadObject(s_dbghelp);
                    s_dbghelp = NULL;
                }
            }
        }
    }
#endif

    SDL_GetMemoryFunctions(&SDL_malloc_orig,
                           &SDL_calloc_orig,
                           &SDL_realloc_orig,
                           &SDL_free_orig);

    SDL_SetMemoryFunctions(SDLTest_TrackedMalloc,
                           SDLTest_TrackedCalloc,
                           SDLTest_TrackedRealloc,
                           SDLTest_TrackedFree);
}

void SDLTest_RandFillAllocations()
{
    SDLTest_TrackAllocations();

    s_randfill_allocations = SDL_TRUE;
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

    count = 0;
    total_allocated = 0;
    for (index = 0; index < SDL_arraysize(s_tracked_allocations); ++index) {
        for (entry = s_tracked_allocations[index]; entry; entry = entry->next) {
            (void)SDL_snprintf(line, sizeof(line), "Allocation %d: %d bytes\n", count, (int)entry->size);
            ADD_LINE();
            /* Start at stack index 1 to skip our tracking functions */
            for (stack_index = 1; stack_index < SDL_arraysize(entry->stack); ++stack_index) {
                if (!entry->stack[stack_index]) {
                    break;
                }
                (void)SDL_snprintf(line, sizeof(line), "\t0x%" SDL_PRIx64 ": %s\n", entry->stack[stack_index], entry->stack_names[stack_index]);
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
