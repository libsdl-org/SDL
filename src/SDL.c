/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"
#include "SDL3/SDL_revision.h"

#if defined(__WIN32__) || defined(__GDK__)
#include "core/windows/SDL_windows.h"
#elif !defined(__WINRT__)
#include <unistd.h> /* _exit(), etc. */
#endif

/* this checks for HAVE_DBUS_DBUS_H internally. */
#include "core/linux/SDL_dbus.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* Initialization code for SDL */

#include "SDL_assert_c.h"
#include "SDL_log_c.h"
#include "SDL_properties_c.h"
#include "audio/SDL_sysaudio.h"
#include "video/SDL_video_c.h"
#include "events/SDL_events_c.h"
#include "haptic/SDL_haptic_c.h"
#include "joystick/SDL_gamepad_c.h"
#include "joystick/SDL_joystick_c.h"
#include "sensor/SDL_sensor_c.h"

/* Initialization/Cleanup routines */
#ifndef SDL_TIMERS_DISABLED
#include "timer/SDL_timer_c.h"
#endif
#ifdef SDL_VIDEO_DRIVER_WINDOWS
extern int SDL_HelperWindowCreate(void);
extern int SDL_HelperWindowDestroy(void);
#endif

#ifdef SDL_BUILD_MAJOR_VERSION
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MAJOR_VERSION,
                        SDL_MAJOR_VERSION == SDL_BUILD_MAJOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MINOR_VERSION,
                        SDL_MINOR_VERSION == SDL_BUILD_MINOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MICRO_VERSION,
                        SDL_PATCHLEVEL == SDL_BUILD_MICRO_VERSION);
#endif

SDL_COMPILE_TIME_ASSERT(SDL_MAJOR_VERSION_min, SDL_MAJOR_VERSION >= 0);
/* Limited only by the need to fit in SDL_version */
SDL_COMPILE_TIME_ASSERT(SDL_MAJOR_VERSION_max, SDL_MAJOR_VERSION <= 255);

SDL_COMPILE_TIME_ASSERT(SDL_MINOR_VERSION_min, SDL_MINOR_VERSION >= 0);
/* Limited only by the need to fit in SDL_version */
SDL_COMPILE_TIME_ASSERT(SDL_MINOR_VERSION_max, SDL_MINOR_VERSION <= 255);

SDL_COMPILE_TIME_ASSERT(SDL_PATCHLEVEL_min, SDL_PATCHLEVEL >= 0);
/* Limited by its encoding in SDL_VERSIONNUM and in the ABI versions */
SDL_COMPILE_TIME_ASSERT(SDL_PATCHLEVEL_max, SDL_PATCHLEVEL <= 99);

/* This is not declared in any header, although it is shared between some
    parts of SDL, because we don't want anything calling it without an
    extremely good reason. */
extern SDL_NORETURN void SDL_ExitProcess(int exitcode);
SDL_NORETURN void SDL_ExitProcess(int exitcode)
{
#if defined(__WIN32__) || defined(__GDK__)
    /* "if you do not know the state of all threads in your process, it is
       better to call TerminateProcess than ExitProcess"
       https://msdn.microsoft.com/en-us/library/windows/desktop/ms682658(v=vs.85).aspx */
    TerminateProcess(GetCurrentProcess(), exitcode);
    /* MingW doesn't have TerminateProcess marked as noreturn, so add an
       ExitProcess here that will never be reached but make MingW happy. */
    ExitProcess(exitcode);
#elif defined(__EMSCRIPTEN__)
    emscripten_cancel_main_loop();   /* this should "kill" the app. */
    emscripten_force_exit(exitcode); /* this should "kill" the app. */
    exit(exitcode);
#elif defined(__HAIKU__)  /* Haiku has _Exit, but it's not marked noreturn. */
    _exit(exitcode);
#elif defined(HAVE__EXIT) /* Upper case _Exit() */
    _Exit(exitcode);
#else
    _exit(exitcode);
#endif
}

/* The initialized subsystems */
#ifdef SDL_MAIN_NEEDED
static SDL_bool SDL_MainIsReady = SDL_FALSE;
#else
static SDL_bool SDL_MainIsReady = SDL_TRUE;
#endif
static SDL_bool SDL_bInMainQuit = SDL_FALSE;
static Uint8 SDL_SubsystemRefCount[32];

/* Private helper to increment a subsystem's ref counter. */
static void SDL_IncrementSubsystemRefCount(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    SDL_assert((subsystem_index < 0) || (SDL_SubsystemRefCount[subsystem_index] < 255));
    if (subsystem_index >= 0) {
        ++SDL_SubsystemRefCount[subsystem_index];
    }
}

/* Private helper to decrement a subsystem's ref counter. */
static void SDL_DecrementSubsystemRefCount(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    if ((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] > 0)) {
        --SDL_SubsystemRefCount[subsystem_index];
    }
}

/* Private helper to check if a system needs init. */
static SDL_bool SDL_ShouldInitSubsystem(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    SDL_assert((subsystem_index < 0) || (SDL_SubsystemRefCount[subsystem_index] < 255));
    return ((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] == 0));
}

/* Private helper to check if a system needs to be quit. */
static SDL_bool SDL_ShouldQuitSubsystem(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    if ((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] == 0)) {
        return SDL_FALSE;
    }

    /* If we're in SDL_Quit, we shut down every subsystem, even if refcount
     * isn't zero.
     */
    return (((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] == 1)) || SDL_bInMainQuit);
}

/* Private helper to init video with default args. */
static int SDL_VideoInitDef()
{
    return SDL_VideoInit(NULL);
}

/* Private helper to init audio with default args. */
static int SDL_AudioInitDef()
{
    return SDL_InitAudio(NULL);
}

/* Helper struct defining a subsystem init or quit callback */
typedef struct SDL_SysInit
{
    Uint32 flag; /* subsystem's id bit */
    SDL_bool enabled;
    int (*subsys_init)();
    void (*subsys_quit)();
    Uint32 dependency; /* other subsystems required for this subsystem */
    const char *error_disabled;

} SDL_SysInit;

/* Subsystem handlers must be arranged in the order of SDL_INIT_* bits. */
static SDL_SysInit SubsystemInitHandlers[32] = {
#if !defined(SDL_TIMERS_DISABLED) && !defined(SDL_TIMER_DUMMY)
    { SDL_INIT_TIMER, SDL_TRUE, SDL_InitTimers, SDL_QuitTimers, 0, "" },
#else
    { SDL_INIT_TIMER, SDL_FALSE, NULL, NULL, 0, "SDL not built with timer support" },
#endif
    { 0 }, /* undefined */
    { 0 }, /* undefined */
    { 0 }, /* undefined */
#ifndef SDL_AUDIO_DISABLED
    /* audio implies events */
    { SDL_INIT_AUDIO, SDL_TRUE, SDL_AudioInitDef, SDL_QuitAudio, SDL_INIT_EVENTS, "" },
#else
    { SDL_INIT_AUDIO, SDL_FALSE, NULL, NULL, 0, "SDL not built with audio support" },
#endif
#ifndef SDL_VIDEO_DISABLED
    /* video implies events */
    { SDL_INIT_VIDEO, SDL_TRUE, SDL_VideoInitDef, SDL_VideoQuit, SDL_INIT_EVENTS, "" },
#else
    { SDL_INIT_VIDEO, SDL_FALSE, NULL, NULL, 0, "SDL not built with video support" },
#endif
    { 0 }, /* undefined */
    { 0 }, /* undefined */
    { 0 }, /* undefined */
#ifndef SDL_JOYSTICK_DISABLED
    /* joystick implies events */
    { SDL_INIT_JOYSTICK, SDL_TRUE, SDL_InitJoysticks, SDL_QuitJoysticks, SDL_INIT_EVENTS, "" },
#else
    { SDL_INIT_JOYSTICK, SDL_FALSE, NULL, NULL, 0, "SDL not built with joystick support" },
#endif
    { 0 }, /* undefined */
    { 0 }, /* undefined */
#ifndef SDL_HAPTIC_DISABLED
    { SDL_INIT_HAPTIC, SDL_TRUE, SDL_InitHaptics, SDL_QuitHaptics, 0, "" },
#else
    { SDL_INIT_HAPTIC, SDL_FALSE, NULL, NULL, 0, "SDL not built with haptic (force feedback) support" },
#endif
#ifndef SDL_JOYSTICK_DISABLED
    /* game controller implies joystick */
    { SDL_INIT_GAMEPAD, SDL_TRUE, SDL_InitGamepads, SDL_QuitGamepads, SDL_INIT_JOYSTICK, "" },
#else
    { SDL_INIT_GAMEPAD, SDL_FALSE, NULL, NULL, 0, "SDL not built with joystick support" },
#endif
#ifndef SDL_EVENTS_DISABLED
    { SDL_INIT_EVENTS, SDL_TRUE, SDL_InitEvents, SDL_QuitEvents, 0, "" },
#else
    { SDL_INIT_EVENTS, SDL_FALSE, NULL, NULL, 0, "SDL not built with events support" },
#endif
#ifndef SDL_SENSOR_DISABLED
    { SDL_INIT_SENSOR, SDL_TRUE, SDL_InitSensors, SDL_QuitSensors, 0, "" }
#else
    { SDL_INIT_SENSOR, SDL_FALSE, NULL, NULL, 0, "SDL not built with sensor support" }
#endif
};

static SDL_bool SDL_InitSubSystemList(Uint32 flags, Uint32 *flags_initialized);
static void SDL_QuitSubSystemList(Uint32 flags);

/* Private helper to initialize one subsystem. */
static SDL_bool SDL_InitSubSystemImpl(SDL_SysInit *init, Uint32 *flags_initialized)
{
    if (!init->enabled) {
        SDL_SetError("%s", init->error_disabled);
        return SDL_FALSE;
    }

    if (SDL_ShouldInitSubsystem(init->flag)) {
        /* Try initialize any dependencies first */
        if (init->dependency != 0) {
            if (SDL_InitSubSystemList(init->dependency, flags_initialized) == SDL_FALSE) {
                return SDL_FALSE;
            }
        }

        SDL_IncrementSubsystemRefCount(init->flag);
        if (init->subsys_init() < 0) {
            SDL_DecrementSubsystemRefCount(init->flag);
            return SDL_FALSE;
        }
    } else {
        SDL_IncrementSubsystemRefCount(init->flag);
    }

    *flags_initialized |= init->flag;
    return SDL_TRUE;
}

/* Private helper to quit one subsystem. */
static void SDL_QuitSubSystemImpl(SDL_SysInit *init)
{
    if (!init->enabled || (SDL_WasInit(init->flag) == SDL_FALSE)) {
        return;
    }

    if (SDL_ShouldQuitSubsystem(init->flag)) {
        init->subsys_quit();

        /* Quit any dependencies */
        if (init->dependency != 0) {
            SDL_QuitSubSystemList(init->dependency);
        }
    }
    SDL_DecrementSubsystemRefCount(init->flag);
}

/* Private helper to init any number of systems read from flags. */
static SDL_bool SDL_InitSubSystemList(Uint32 flags, Uint32 *flags_initialized)
{
    int i;
    for (i = 0; i < 32; ++i) {
        if (flags & (1 << i)) {
            if (SDL_InitSubSystemImpl(&SubsystemInitHandlers[i], flags_initialized) == SDL_FALSE) {
                return SDL_FALSE;
            }
        }
    }
    return SDL_TRUE;
}

/* Private helper to quit any number of systems read from flags. */
static void SDL_QuitSubSystemList(Uint32 flags)
{
    int i;
    for (i = 0; i < 32; ++i) {
        if (flags & (1 << i)) {
            SDL_QuitSubSystemImpl(&SubsystemInitHandlers[i]);
        }
    }
}

void SDL_SetMainReady(void)
{
    SDL_MainIsReady = SDL_TRUE;
}

int SDL_InitSubSystem(Uint32 flags)
{
    Uint32 flags_initialized = 0;

    if (!SDL_MainIsReady) {
        return SDL_SetError("Application didn't initialize properly, did you include SDL_main.h in the file containing your main() function?");
    }

    SDL_InitLog();
    SDL_InitProperties();

    /* Clear the error message */
    SDL_ClearError();

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_Init();
#endif

#ifdef SDL_VIDEO_DRIVER_WINDOWS
    if (flags & (SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK)) {
        if (SDL_HelperWindowCreate() < 0) {
            goto quit_and_error;
        }
    }
#endif

#ifndef SDL_TIMERS_DISABLED
    SDL_InitTicks();
#endif

    /* Iterate over a list of subsystems and perform init for the required ones. */
    if (SDL_InitSubSystemList(flags, &flags_initialized) == SDL_FALSE) {
        goto quit_and_error;
    }

    (void)flags_initialized; /* make static analysis happy, since this only gets used in error cases. */

    return 0;

quit_and_error:
    SDL_QuitSubSystem(flags_initialized);
    return -1;
}

int SDL_Init(Uint32 flags)
{
    return SDL_InitSubSystem(flags);
}

void SDL_QuitSubSystem(Uint32 flags)
{
    /* Iterate over a list of subsystems and perform quit for the required ones. */
    SDL_QuitSubSystemList(flags);
}

Uint32 SDL_WasInit(Uint32 flags)
{
    int i;
    int num_subsystems = SDL_arraysize(SDL_SubsystemRefCount);
    Uint32 initialized = 0;

    /* Fast path for checking one flag */
    if (SDL_HasExactlyOneBitSet32(flags)) {
        int subsystem_index = SDL_MostSignificantBitIndex32(flags);
        return SDL_SubsystemRefCount[subsystem_index] ? flags : 0;
    }

    if (!flags) {
        flags = SDL_INIT_EVERYTHING;
    }

    num_subsystems = SDL_min(num_subsystems, SDL_MostSignificantBitIndex32(flags) + 1);

    /* Iterate over each bit in flags, and check the matching subsystem. */
    for (i = 0; i < num_subsystems; ++i) {
        if ((flags & 1) && SDL_SubsystemRefCount[i] > 0) {
            initialized |= (1 << i);
        }

        flags >>= 1;
    }

    return initialized;
}

void SDL_Quit(void)
{
    SDL_bInMainQuit = SDL_TRUE;

    /* Quit all subsystems */
#ifdef SDL_VIDEO_DRIVER_WINDOWS
    SDL_HelperWindowDestroy();
#endif
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);

#ifndef SDL_TIMERS_DISABLED
    SDL_QuitTicks();
#endif

    SDL_ClearHints();
    SDL_AssertionsQuit();

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_Quit();
#endif

    SDL_QuitProperties();
    SDL_QuitLog();

    /* Now that every subsystem has been quit, we reset the subsystem refcount
     * and the list of initialized subsystems.
     */
    SDL_memset(SDL_SubsystemRefCount, 0x0, sizeof(SDL_SubsystemRefCount));

    SDL_CleanupTLS();

    SDL_bInMainQuit = SDL_FALSE;
}

/* Assume we can wrap SDL_AtomicInt values and cast to Uint32 */
SDL_COMPILE_TIME_ASSERT(sizeof_object_id, sizeof(int) == sizeof(Uint32));

Uint32 SDL_GetNextObjectID(void)
{
    static SDL_AtomicInt last_id;

    Uint32 id = (Uint32)SDL_AtomicIncRef(&last_id) + 1;
    if (id == 0) {
        id = (Uint32)SDL_AtomicIncRef(&last_id) + 1;
    }
    return id;
}

/* Get the library version number */
int SDL_GetVersion(SDL_version *ver)
{
    static SDL_bool check_hint = SDL_TRUE;
    static SDL_bool legacy_version = SDL_FALSE;

    if (ver == NULL) {
        return SDL_InvalidParamError("ver");
    }

    SDL_VERSION(ver);

    if (check_hint) {
        check_hint = SDL_FALSE;
        legacy_version = SDL_GetHintBoolean("SDL_LEGACY_VERSION", SDL_FALSE);
    }

    if (legacy_version) {
        /* Prior to SDL 2.24.0, the patch version was incremented with every release */
        ver->patch = ver->minor;
        ver->minor = 0;
    }
    return 0;
}

/* Get the library source revision */
const char *SDL_GetRevision(void)
{
    return SDL_REVISION;
}

/* Get the name of the platform */
const char *SDL_GetPlatform(void)
{
#ifdef __AIX__
    return "AIX";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__BSDI__)
    return "BSDI";
#elif defined(__DREAMCAST__)
    return "Dreamcast";
#elif defined(__EMSCRIPTEN__)
    return "Emscripten";
#elif defined(__FREEBSD__)
    return "FreeBSD";
#elif defined(__HAIKU__)
    return "Haiku";
#elif defined(__HPUX__)
    return "HP-UX";
#elif defined(__IRIX__)
    return "Irix";
#elif defined(__LINUX__)
    return "Linux";
#elif defined(__MINT__)
    return "Atari MiNT";
#elif defined(__MACOS__)
    return "macOS";
#elif defined(__NACL__)
    return "NaCl";
#elif defined(__NETBSD__)
    return "NetBSD";
#elif defined(__OPENBSD__)
    return "OpenBSD";
#elif defined(__OS2__)
    return "OS/2";
#elif defined(__OSF__)
    return "OSF/1";
#elif defined(__QNXNTO__)
    return "QNX Neutrino";
#elif defined(__RISCOS__)
    return "RISC OS";
#elif defined(__SOLARIS__)
    return "Solaris";
#elif defined(__WIN32__)
    return "Windows";
#elif defined(__WINRT__)
    return "WinRT";
#elif defined(__WINGDK__)
    return "WinGDK";
#elif defined(__XBOXONE__)
    return "Xbox One";
#elif defined(__XBOXSERIES__)
    return "Xbox Series X|S";
#elif defined(__IOS__)
    return "iOS";
#elif defined(__TVOS__)
    return "tvOS";
#elif defined(__PS2__)
    return "PlayStation 2";
#elif defined(__PSP__)
    return "PlayStation Portable";
#elif defined(__VITA__)
    return "PlayStation Vita";
#elif defined(__NGAGE__)
    return "Nokia N-Gage";
#elif defined(__3DS__)
    return "Nintendo 3DS";
#elif defined(__managarm__)
    return "Managarm";
#else
    return "Unknown (see SDL_platform.h)";
#endif
}

SDL_bool SDL_IsTablet(void)
{
#ifdef __ANDROID__
    extern SDL_bool SDL_IsAndroidTablet(void);
    return SDL_IsAndroidTablet();
#elif defined(__IOS__)
    extern SDL_bool SDL_IsIPad(void);
    return SDL_IsIPad();
#else
    return SDL_FALSE;
#endif
}

#ifdef __WIN32__

#if (!defined(HAVE_LIBC) || defined(__WATCOMC__)) && !defined(SDL_STATIC_LIB)
/* FIXME: Still need to include DllMain() on Watcom C ? */

BOOL APIENTRY MINGW32_FORCEALIGN _DllMainCRTStartup(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
#endif /* Building DLL */

#endif /* defined(__WIN32__) || defined(__GDK__) */
