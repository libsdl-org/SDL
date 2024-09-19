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

#include "SDL_build_config.h"
#include "SDL_dynapi.h"
#include "SDL_dynapi_unsupported.h"

#if SDL_DYNAMIC_API

#define SDL_DYNAMIC_API_ENVVAR "SDL3_DYNAMIC_API"
#define SDL_SLOW_MEMCPY
#define SDL_SLOW_MEMMOVE
#define SDL_SLOW_MEMSET

#include <stdarg.h>
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#include <SDL3/SDL.h>
#define SDL_MAIN_NOIMPL // don't drag in header-only implementation of SDL_main
#include <SDL3/SDL_main.h>


// These headers have system specific definitions, so aren't included above
#include <SDL3/SDL_vulkan.h>

/* This is the version of the dynamic API. This doesn't match the SDL version
   and should not change until there's been a major revamp in API/ABI.
   So 2.0.5 adds functions over 2.0.4? This number doesn't change;
   the sizeof(jump_table) changes instead. But 2.1.0 changes how a function
   works in an incompatible way or removes a function? This number changes,
   since sizeof(jump_table) isn't sufficient anymore. It's likely
   we'll forget to bump every time we add a function, so this is the
   failsafe switch for major API change decisions. Respect it and use it
   sparingly. */
#define SDL_DYNAPI_VERSION 2

#ifdef __cplusplus
extern "C" {
#endif

static void SDL_InitDynamicAPI(void);

/* BE CAREFUL CALLING ANY SDL CODE IN HERE, IT WILL BLOW UP.
   Even self-contained stuff might call SDL_SetError() and break everything. */

// behold, the macro salsa!

// Typedefs for function pointers for jump table, and predeclare funcs
// The DEFAULT funcs will init jump table and then call real function.
// The REAL funcs are the actual functions, name-mangled to not clash.
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) \
    typedef rc (SDLCALL *SDL_DYNAPIFN_##fn) params;\
    static rc SDLCALL fn##_DEFAULT params;         \
    extern rc SDLCALL fn##_REAL params;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC

// The jump table!
typedef struct
{
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) SDL_DYNAPIFN_##fn fn;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC
} SDL_DYNAPI_jump_table;

// The actual jump table.
static SDL_DYNAPI_jump_table jump_table = {
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) fn##_DEFAULT,
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC
};

// Default functions init the function table then call right thing.
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) \
    static rc SDLCALL fn##_DEFAULT params          \
    {                                              \
        SDL_InitDynamicAPI();                      \
        ret jump_table.fn args;                    \
    }
#define SDL_DYNAPI_VPROC(rc, fn, params, args, forwardfn, last_arg, ret) \
    static rc SDLCALL fn##_DEFAULT params                                \
    {                                                                    \
        rc result;                                                       \
        va_list ap;                                                      \
                                                                         \
        SDL_InitDynamicAPI();                                            \
        va_start(ap, last_arg);                                          \
        result = jump_table.forwardfn args;                              \
        va_end(ap);                                                      \
        return result;                                                   \
    }
#define SDL_DYNAPI_VOID_VPROC(rc, fn, params, args, forwardfn, last_arg, ret) \
    static rc SDLCALL fn##_DEFAULT params                                     \
    {                                                                         \
        va_list ap;                                                           \
                                                                              \
        SDL_InitDynamicAPI();                                                 \
        va_start(ap, last_arg);                                               \
        jump_table.forwardfn args;                                            \
        va_end(ap);                                                           \
    }
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC

// Public API functions to jump into the jump table.
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) \
    rc SDLCALL fn params                           \
    {                                              \
        ret jump_table.fn args;                    \
    }
#define SDL_DYNAPI_VPROC(rc, fn, params, args, forwardfn, last_arg, ret) \
    rc SDLCALL fn params                                                 \
    {                                                                    \
        rc result;                                                       \
        va_list ap;                                                      \
                                                                         \
        va_start(ap, last_arg);                                          \
        result = jump_table.forwardfn args;                              \
        va_end(ap);                                                      \
        return result;                                                   \
    }
#define SDL_DYNAPI_VOID_VPROC(rc, fn, params, args, forwardfn, last_arg, ret) \
     rc SDLCALL fn params                                                     \
    {                                                                         \
        va_list ap;                                                           \
                                                                              \
        va_start(ap, last_arg);                                               \
        jump_table.forwardfn args;                                            \
        va_end(ap);                                                           \
    }
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC

#define ENABLE_SDL_CALL_LOGGING 0
#if ENABLE_SDL_CALL_LOGGING
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) \
    static rc SDLCALL fn##_LOGSDLCALLS params      \
    {                                              \
        SDL_Log_REAL("SDL3CALL %s", #fn);          \
        ret fn##_REAL args;                        \
    }
#define SDL_DYNAPI_VPROC(rc, fn, params, args, forwardfn, last_arg, ret) \
    static rc SDLCALL fn##_LOGSDLCALLS params                            \
    {                                                                    \
        rc result;                                                       \
        va_list ap;                                                      \
                                                                         \
        SDL_Log_REAL("SDL3CALL %s", #fn);                                \
        va_start(ap, last_arg);                                          \
        result = jump_table.fn args;                                     \
        va_end(ap);                                                      \
        return result;                                                   \
    }
#define SDL_DYNAPI_VOID_VPROC(rc, fn, params, args, forwardfn, last_arg, ret) \
    static rc SDLCALL fn##_LOGSDLCALLS params                                 \
    {                                                                         \
        va_list ap;                                                           \
                                                                              \
        SDL_Log_REAL("SDL3CALL %s", #fn);                                     \
        va_start(ap, last_arg);                                               \
        jump_table.forwardfn args;                                            \
        va_end(ap);                                                           \
    }
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC
#endif

/* we make this a static function so we can call the correct one without the
   system's dynamic linker resolving to the wrong version of this. */
static Sint32 initialize_jumptable(Uint32 apiver, void *table, Uint32 tablesize)
{
    SDL_DYNAPI_jump_table *output_jump_table = (SDL_DYNAPI_jump_table *)table;

    if (apiver != SDL_DYNAPI_VERSION) {
        // !!! FIXME: can maybe handle older versions?
        return -1; // not compatible.
    } else if (tablesize > sizeof(jump_table)) {
        return -1; // newer version of SDL with functions we can't provide.
    }

// Init our jump table first.
#if ENABLE_SDL_CALL_LOGGING
    {
        const char *env = SDL_getenv_unsafe_REAL("SDL_DYNAPI_LOG_CALLS");
        const bool log_calls = (env && SDL_atoi_REAL(env));
        if (log_calls) {
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) jump_table.fn = fn##_LOGSDLCALLS;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC
        } else {
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) jump_table.fn = fn##_REAL;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC
        }
    }
#else
#define SDL_DYNAPI_PROC(rc, fn, params, args, ret) jump_table.fn = fn##_REAL;
#include "SDL_dynapi_procs.h"
#undef SDL_DYNAPI_PROC
#undef SDL_DYNAPI_VPROC
#undef SDL_DYNAPI_VOID_VPROC
#endif

    // Then the external table...
    if (output_jump_table != &jump_table) {
        jump_table.SDL_memcpy(output_jump_table, &jump_table, tablesize);
    }

    // Safe to call SDL functions now; jump table is initialized!

    return 0; // success!
}

// Here's the exported entry point that fills in the jump table.
// Use specific types when an "int" might suffice to keep this sane.
typedef Sint32 (SDLCALL *SDL_DYNAPI_ENTRYFN)(Uint32 apiver, void *table, Uint32 tablesize);
extern SDL_DECLSPEC Sint32 SDLCALL SDL_DYNAPI_entry(Uint32, void *, Uint32);

Sint32 SDL_DYNAPI_entry(Uint32 apiver, void *table, Uint32 tablesize)
{
    return initialize_jumptable(apiver, table, tablesize);
}

#ifdef __cplusplus
}
#endif

// Obviously we can't use SDL_LoadObject() to load SDL.  :)
// Also obviously, we never close the loaded library.
#if defined(WIN32) || defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
static SDL_INLINE void *get_sdlapi_entry(const char *fname, const char *sym)
{
    HMODULE lib = LoadLibraryA(fname);
    void *result = NULL;
    if (lib) {
        result = (void *) GetProcAddress(lib, sym);
        if (!result) {
            FreeLibrary(lib);
        }
    }
    return result;
}

#elif defined(SDL_PLATFORM_UNIX) || defined(SDL_PLATFORM_APPLE) || defined(SDL_PLATFORM_HAIKU)
#include <dlfcn.h>
static SDL_INLINE void *get_sdlapi_entry(const char *fname, const char *sym)
{
    void *lib = dlopen(fname, RTLD_NOW | RTLD_LOCAL);
    void *result = NULL;
    if (lib) {
        result = dlsym(lib, sym);
        if (!result) {
            dlclose(lib);
        }
    }
    return result;
}

#else
#error Please define your platform.
#endif

static void dynapi_warn(const char *msg)
{
    const char *caption = "SDL Dynamic API Failure!";
    (void)caption;
// SDL_ShowSimpleMessageBox() is a too heavy for here.
#if (defined(WIN32) || defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN)) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    MessageBoxA(NULL, msg, caption, MB_OK | MB_ICONERROR);
#elif defined(HAVE_STDIO_H)
    fprintf(stderr, "\n\n%s\n%s\n\n", caption, msg);
    fflush(stderr);
#endif
}

/* This is not declared in any header, although it is shared between some
    parts of SDL, because we don't want anything calling it without an
    extremely good reason. */
#ifdef __cplusplus
extern "C" {
#endif
extern SDL_NORETURN void SDL_ExitProcess(int exitcode);
#ifdef __WATCOMC__
#pragma aux SDL_ExitProcess aborts;
#endif
#ifdef __cplusplus
}
#endif

static void SDL_InitDynamicAPILocked(void)
{
    const char *libname = SDL_getenv_unsafe_REAL(SDL_DYNAMIC_API_ENVVAR);
    SDL_DYNAPI_ENTRYFN entry = NULL; // funcs from here by default.
    bool use_internal = true;

    if (libname) {
        while (*libname && !entry) {
            // This is evil, but we're not making any permanent changes...
            char *ptr = (char *)libname;
            while (true) {
                char ch = *ptr;
                if ((ch == ',') || (ch == '\0')) {
                    *ptr = '\0';
                    entry = (SDL_DYNAPI_ENTRYFN)get_sdlapi_entry(libname, "SDL_DYNAPI_entry");
                    *ptr = ch;
                    libname = (ch == '\0') ? ptr : (ptr + 1);
                    break;
                } else {
                    ptr++;
                }
            }
        }
        if (!entry) {
            dynapi_warn("Couldn't load an overriding SDL library. Please fix or remove the " SDL_DYNAMIC_API_ENVVAR " environment variable. Using the default SDL.");
            // Just fill in the function pointers from this library, later.
        }
    }

    if (entry) {
        if (entry(SDL_DYNAPI_VERSION, &jump_table, sizeof(jump_table)) < 0) {
            dynapi_warn("Couldn't override SDL library. Using a newer SDL build might help. Please fix or remove the " SDL_DYNAMIC_API_ENVVAR " environment variable. Using the default SDL.");
            // Just fill in the function pointers from this library, later.
        } else {
            use_internal = false; // We overrode SDL! Don't use the internal version!
        }
    }

    // Just fill in the function pointers from this library.
    if (use_internal) {
        if (initialize_jumptable(SDL_DYNAPI_VERSION, &jump_table, sizeof(jump_table)) < 0) {
            // Now we're screwed. Should definitely abort now.
            dynapi_warn("Failed to initialize internal SDL dynapi. As this would otherwise crash, we have to abort now.");
            SDL_ExitProcess(86);
        }
    }

    // we intentionally never close the newly-loaded lib, of course.
}

static void SDL_InitDynamicAPI(void)
{
    /* So the theory is that every function in the jump table defaults to
     *  calling this function, and then replaces itself with a version that
     *  doesn't call this function anymore. But it's possible that, in an
     *  extreme corner case, you can have a second thread hit this function
     *  while the jump table is being initialized by the first.
     * In this case, a spinlock is really painful compared to what spinlocks
     *  _should_ be used for, but this would only happen once, and should be
     *  insanely rare, as you would have to spin a thread outside of SDL (as
     *  SDL_CreateThread() would also call this function before building the
     *  new thread).
     */
    static bool already_initialized = false;

    static SDL_SpinLock lock = 0;
    SDL_LockSpinlock_REAL(&lock);

    if (!already_initialized) {
        SDL_InitDynamicAPILocked();
        already_initialized = true;
    }

    SDL_UnlockSpinlock_REAL(&lock);
}

#else // SDL_DYNAMIC_API

#include <SDL3/SDL.h>

Sint32 SDL_DYNAPI_entry(Uint32 apiver, void *table, Uint32 tablesize);
Sint32 SDL_DYNAPI_entry(Uint32 apiver, void *table, Uint32 tablesize)
{
    (void)apiver;
    (void)table;
    (void)tablesize;
    return -1; // not compatible.
}

#endif // SDL_DYNAMIC_API
