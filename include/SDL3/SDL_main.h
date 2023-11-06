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

#ifndef SDL_main_h_
#define SDL_main_h_

#include <SDL3/SDL_stdinc.h>

/*
 * For details on how SDL_main works, and how to use it, please refer to:
 *
 * https://wiki.libsdl.org/SDL3/README/main-functions
 *
 * (or docs/README-main-functions.md in the SDL source tree)
 */

/**
 *  \file SDL_main.h
 *
 *  Redefine main() on some platforms so that it is called by SDL.
 */

#ifndef SDL_MAIN_HANDLED
#ifdef __WIN32__
/* On Windows SDL provides WinMain(), which parses the command line and passes
   the arguments to your main function.

   If you provide your own WinMain(), you may define SDL_MAIN_HANDLED
 */
#define SDL_MAIN_AVAILABLE

#elif defined(__WINRT__)
/* On WinRT, SDL provides a main function that initializes CoreApplication,
   creating an instance of IFrameworkView in the process.

   Ideally, #include'ing SDL_main.h is enough to get a main() function working.
   However, that requires the source file your main() is in to be compiled
   as C++ *and* with the /ZW compiler flag. If that's not feasible, add an
   otherwise empty .cpp file that only contains `#include <SDL3/SDL_main.h>`
   and build that with /ZW (still include SDL_main.h in your other file with main()!).
   In XAML apps, instead the function SDL_RunApp() must be called with a pointer
   to the Direct3D-hosted XAML control passed in as the "reserved" argument.
*/
#define SDL_MAIN_NEEDED

#elif defined(__GDK__)
/* On GDK, SDL provides a main function that initializes the game runtime.

   If you prefer to write your own WinMain-function instead of having SDL
   provide one that calls your main() function,
   #define SDL_MAIN_HANDLED before #include'ing SDL_main.h
   and call the SDL_RunApp function from your entry point.
*/
#define SDL_MAIN_NEEDED

#elif defined(__IOS__)
/* On iOS SDL provides a main function that creates an application delegate
   and starts the iOS application run loop.

   To use it, just #include SDL_main.h in the source file that contains your
   main() function.

   See src/video/uikit/SDL_uikitappdelegate.m for more details.
 */
#define SDL_MAIN_NEEDED

#elif defined(__ANDROID__)
/* On Android SDL provides a Java class in SDLActivity.java that is the
   main activity entry point.

   See docs/README-android.md for more details on extending that class.
 */
#define SDL_MAIN_NEEDED

/* We need to export SDL_main so it can be launched from Java */
#define SDLMAIN_DECLSPEC    DECLSPEC

#elif defined(__PSP__)
/* On PSP SDL provides a main function that sets the module info,
   activates the GPU and starts the thread required to be able to exit
   the software.

   If you provide this yourself, you may define SDL_MAIN_HANDLED
 */
#define SDL_MAIN_AVAILABLE

#elif defined(__PS2__)
#define SDL_MAIN_AVAILABLE

#define SDL_PS2_SKIP_IOP_RESET() \
   void reset_IOP(); \
   void reset_IOP() {}

#elif defined(__3DS__)
/*
  On N3DS, SDL provides a main function that sets up the screens
  and storage.

  If you provide this yourself, you may define SDL_MAIN_HANDLED
*/
#define SDL_MAIN_AVAILABLE

#elif defined(__NGAGE__)

/*
   TODO: not sure if it should be SDL_MAIN_NEEDED, in SDL2 ngage had a
        main implementation, but wasn't mentioned in SDL_main.h
 */
#define SDL_MAIN_AVAILABLE

#endif
#endif /* SDL_MAIN_HANDLED */

#ifndef SDLMAIN_DECLSPEC
#define SDLMAIN_DECLSPEC
#endif

/**
 *  \file SDL_main.h
 *
 *  The application's main() function must be called with C linkage,
 *  and should be declared like this:
 *  \code
 *  #ifdef __cplusplus
 *  extern "C"
 *  #endif
 *  int main(int argc, char *argv[])
 *  {
 *  }
 *  \endcode
 */

#if defined(SDL_MAIN_NEEDED) || defined(SDL_MAIN_AVAILABLE) || defined(SDL_MAIN_USE_CALLBACKS)
#define main    SDL_main
#endif

#include <SDL3/SDL_begin_code.h>
#ifdef __cplusplus
extern "C" {
#endif

union SDL_Event;
typedef int (SDLCALL *SDL_AppInit_func)(int argc, char *argv[]);
typedef int (SDLCALL *SDL_AppIterate_func)(void);
typedef int (SDLCALL *SDL_AppEvent_func)(const union SDL_Event *event);
typedef void (SDLCALL *SDL_AppQuit_func)(void);

/**
 * You can (optionally!) define SDL_MAIN_USE_CALLBACKS before including
 * SDL_main.h, and then your application will _not_ have a standard
 * "main" entry point. Instead, it will operate as a collection of
 * functions that are called as necessary by the system. On some
 * platforms, this is just a layer where SDL drives your program
 * instead of your program driving SDL, on other platforms this might
 * hook into the OS to manage the lifecycle. Programs on most platforms
 * can use whichever approach they prefer, but the decision boils down
 * to:
 *
 * - Using a standard "main" function: this works like it always has for
 *   the past 50+ years in C programming, and your app is in control.
 * - Using the callback functions: this might clean up some code,
 *   avoid some #ifdef blocks in your program for some platforms, be more
 *   resource-friendly to the system, and possibly be the primary way to
 *   access some future platforms (but none require this at the moment).
 *
 * This is up to the app; both approaches are considered valid and supported
 * ways to write SDL apps.
 *
 * If using the callbacks, don't define a "main" function. Instead, implement
 * the functions listed below in your program.
 */
#ifdef SDL_MAIN_USE_CALLBACKS

/**
 * App-implemented initial entry point for SDL_MAIN_USE_CALLBACKS apps.
 *
 * Apps implement this function when using SDL_MAIN_USE_CALLBACKS. If
 * using a standard "main" function, you should not supply this.
 *
 * This function is called by SDL once, at startup. The function should
 * initialize whatever is necessary, possibly create windows and open
 * audio devices, etc. The `argc` and `argv` parameters work like they would
 * with a standard "main" function.
 *
 * This function should not go into an infinite mainloop; it should do any
 * one-time setup it requires and then return.
 *
 * If this function returns 0, the app will proceed to normal operation,
 * and will begin receiving repeated calls to SDL_AppIterate and SDL_AppEvent
 * for the life of the program. If this function returns < 0, SDL will
 * call SDL_AppQuit and terminate the process with an exit code that reports
 * an error to the platform. If it returns > 0, the SDL calls SDL_AppQuit
 * and terminates with an exit code that reports success to the platform.
 *
 * \param argc The standard ANSI C main's argc; number of elements in `argv`
 * \param argv The standard ANSI C main's argv; array of command line arguments.
 * \returns -1 to terminate with an error, 1 to terminate with success, 0 to continue.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AppIterate
 * \sa SDL_AppEvent
 * \sa SDL_AppQuit
 */
extern SDLMAIN_DECLSPEC int SDLCALL SDL_AppInit(int argc, char *argv[]);

/**
 * App-implemented iteration entry point for SDL_MAIN_USE_CALLBACKS apps.
 *
 * Apps implement this function when using SDL_MAIN_USE_CALLBACKS. If
 * using a standard "main" function, you should not supply this.
 *
 * This function is called repeatedly by SDL after SDL_AppInit returns 0.
 * The function should operate as a single iteration the program's primary
 * loop; it should update whatever state it needs and draw a new frame of
 * video, usually.
 *
 * On some platforms, this function will be called at the refresh rate of
 * the display (which might change during the life of your app!). There are
 * no promises made about what frequency this function might run at. You
 * should use SDL's timer functions if you need to see how much time has
 * passed since the last iteration.
 *
 * There is no need to process the SDL event queue during this function;
 * SDL will send events as they arrive in SDL_AppEvent, and in most cases
 * the event queue will be empty when this function runs anyhow.
 *
 * This function should not go into an infinite mainloop; it should do one
 * iteration of whatever the program does and return.
 *
 * If this function returns 0, the app will continue normal operation,
 * receiving repeated calls to SDL_AppIterate and SDL_AppEvent for the life
 * of the program. If this function returns < 0, SDL will call SDL_AppQuit
 * and terminate the process with an exit code that reports an error to the
 * platform. If it returns > 0, the SDL calls SDL_AppQuit and terminates with
 * an exit code that reports success to the platform.
 *
 * \returns -1 to terminate with an error, 1 to terminate with success, 0 to continue.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AppInit
 * \sa SDL_AppEvent
 * \sa SDL_AppQuit
 */
extern SDLMAIN_DECLSPEC int SDLCALL SDL_AppIterate(void);

/**
 * App-implemented event entry point for SDL_MAIN_USE_CALLBACKS apps.
 *
 * Apps implement this function when using SDL_MAIN_USE_CALLBACKS. If
 * using a standard "main" function, you should not supply this.
 *
 * This function is called as needed by SDL after SDL_AppInit returns 0;
 * It is called once for each new event.
 *
 * There is (currently) no guarantee about what thread this will be called
 * from; whatever thread pushes an event onto SDL's queue will trigger this
 * function. SDL is responsible for pumping the event queue between
 * each call to SDL_AppIterate, so in normal operation one should only
 * get events in a serial fashion, but be careful if you have a thread that
 * explicitly calls SDL_PushEvent.
 *
 * Events sent to this function are not owned by the app; if you need to
 * save the data, you should copy it.
 *
 * You do not need to free event data (such as the `file` string in
 * SDL_EVENT_DROP_FILE), as SDL will free it once this function returns.
 * Note that this is different than one might expect when using a standard
 * "main" function!
 *
 * This function should not go into an infinite mainloop; it should handle
 * the provided event appropriately and return.
 *
 * If this function returns 0, the app will continue normal operation,
 * receiving repeated calls to SDL_AppIterate and SDL_AppEvent for the life
 * of the program. If this function returns < 0, SDL will call SDL_AppQuit
 * and terminate the process with an exit code that reports an error to the
 * platform. If it returns > 0, the SDL calls SDL_AppQuit and terminates with
 * an exit code that reports success to the platform.
 *
 * \returns -1 to terminate with an error, 1 to terminate with success, 0 to continue.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AppInit
 * \sa SDL_AppIterate
 * \sa SDL_AppQuit
 */
extern SDLMAIN_DECLSPEC int SDLCALL SDL_AppEvent(const SDL_Event *event);

/**
 * App-implemented deinit entry point for SDL_MAIN_USE_CALLBACKS apps.
 *
 * Apps implement this function when using SDL_MAIN_USE_CALLBACKS. If
 * using a standard "main" function, you should not supply this.
 *
 * This function is called once by SDL before terminating the program.
 *
 * This function will be called no matter what, even if SDL_AppInit
 * requests termination.
 *
 * This function should not go into an infinite mainloop; it should
 * deinitialize any resources necessary, perform whatever shutdown
 * activities, and return.
 *
 * You do not need to call SDL_Quit() in this function, as SDL will call
 * it after this function returns and before the process terminates, but
 * it is safe to do so.
 *
 * \threadsafety This function is not thread safe.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AppInit
 * \sa SDL_AppIterate
 * \sa SDL_AppEvent
 */
extern SDLMAIN_DECLSPEC void SDLCALL SDL_AppQuit(void);

#endif  /* SDL_MAIN_USE_CALLBACKS */


/**
 *  The prototype for the application's main() function
 */
typedef int (SDLCALL *SDL_main_func)(int argc, char *argv[]);
extern SDLMAIN_DECLSPEC int SDLCALL SDL_main(int argc, char *argv[]);


/**
 * Circumvent failure of SDL_Init() when not using SDL_main() as an entry
 * point.
 *
 * This function is defined in SDL_main.h, along with the preprocessor rule to
 * redefine main() as SDL_main(). Thus to ensure that your main() function
 * will not be changed it is necessary to define SDL_MAIN_HANDLED before
 * including SDL.h.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_Init
 */
extern DECLSPEC void SDLCALL SDL_SetMainReady(void);

/**
 * Initializes and launches an SDL application, by doing platform-specific
 * initialization before calling your mainFunction and cleanups after it
 * returns, if that is needed for a specific platform, otherwise it just calls
 * mainFunction.
 *
 * You can use this if you want to use your own main() implementation without
 * using SDL_main (like when using SDL_MAIN_HANDLED). When using this, you do
 * *not* need SDL_SetMainReady().
 *
 * \param argc The argc parameter from the application's main() function, or 0
 *             if the platform's main-equivalent has no argc
 * \param argv The argv parameter from the application's main() function, or
 *             NULL if the platform's main-equivalent has no argv
 * \param mainFunction Your SDL app's C-style main(), an SDL_main_func. NOT
 *                     the function you're calling this from! Its name doesn't
 *                     matter, but its signature must be like int my_main(int
 *                     argc, char* argv[])
 * \param reserved should be NULL (reserved for future use, will probably be
 *                 platform-specific then)
 * \returns the return value from mainFunction: 0 on success, -1 on failure;
 *          SDL_GetError() might have more information on the failure
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_RunApp(int argc, char* argv[], SDL_main_func mainFunction, void * reserved);

/**
 * An entry point for SDL's use in SDL_MAIN_USE_CALLBACKS.
 *
 * Generally, you should not call this function directly. This only exists to
 * hand off work into SDL as soon as possible, where it has a lot more control
 * and functionality available, and make the inline code in SDL_main.h as
 * small as possible.
 *
 * Not all platforms use this, it's actual use is hidden in a magic
 * header-only library, and you should not call this directly unless you
 * _really_ know what you're doing.
 *
 * \param argc standard Unix main argc
 * \param argv standard Unix main argv
 * \param appinit The application's SDL_AppInit function
 * \param appiter The application's SDL_AppIterate function
 * \param appevent The application's SDL_AppEvent function
 * \param appquit The application's SDL_AppQuit function
 * \returns standard Unix main return value
 *
 * \threadsafety It is not safe to call this anywhere except as the only
 *               function call in SDL_main.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_EnterAppMainCallbacks(int argc, char* argv[], SDL_AppInit_func appinit, SDL_AppIterate_func appiter, SDL_AppEvent_func appevent, SDL_AppQuit_func appquit);


#if defined(__WIN32__) || defined(__GDK__)

/**
 * Register a win32 window class for SDL's use.
 *
 * This can be called to set the application window class at startup. It is
 * safe to call this multiple times, as long as every call is eventually
 * paired with a call to SDL_UnregisterApp, but a second registration attempt
 * while a previous registration is still active will be ignored, other than
 * to increment a counter.
 *
 * Most applications do not need to, and should not, call this directly; SDL
 * will call it when initializing the video subsystem.
 *
 * \param name the window class name, in UTF-8 encoding. If NULL, SDL
 *             currently uses "SDL_app" but this isn't guaranteed.
 * \param style the value to use in WNDCLASSEX::style. If `name` is NULL, SDL
 *              currently uses `(CS_BYTEALIGNCLIENT | CS_OWNDC)` regardless of
 *              what is specified here.
 * \param hInst the HINSTANCE to use in WNDCLASSEX::hInstance. If zero, SDL
 *              will use `GetModuleHandle(NULL)` instead.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_RegisterApp(const char *name, Uint32 style, void *hInst);

/**
 * Deregister the win32 window class from an SDL_RegisterApp call.
 *
 * This can be called to undo the effects of SDL_RegisterApp.
 *
 * Most applications do not need to, and should not, call this directly; SDL
 * will call it when deinitializing the video subsystem.
 *
 * It is safe to call this multiple times, as long as every call is eventually
 * paired with a prior call to SDL_RegisterApp. The window class will only be
 * deregistered when the registration counter in SDL_RegisterApp decrements to
 * zero through calls to this function.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_UnregisterApp(void);

#endif /* defined(__WIN32__) || defined(__GDK__) */


#ifdef __WINRT__

/* for compatibility with SDL2's function of this name */
#define SDL_WinRTRunApp(MAIN_FUNC, RESERVED)  SDL_RunApp(0, NULL, MAIN_FUNC, RESERVED)

#endif /* __WINRT__ */

#ifdef __IOS__

/* for compatibility with SDL2's function of this name */
#define SDL_UIKitRunApp(ARGC, ARGV, MAIN_FUNC)  SDL_RunApp(ARGC, ARGV, MAIN_FUNC, NULL)

#endif /* __IOS__ */

#ifdef __GDK__

/* for compatibility with SDL2's function of this name */
#define SDL_GDKRunApp(MAIN_FUNC, RESERVED)  SDL_RunApp(0, NULL, MAIN_FUNC, RESERVED)

/**
 * Callback from the application to let the suspend continue.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_GDKSuspendComplete(void);

#endif /* __GDK__ */

#ifdef __cplusplus
}
#endif

#include <SDL3/SDL_close_code.h>

#if !defined(SDL_MAIN_HANDLED) && !defined(SDL_MAIN_NOIMPL)
/* include header-only SDL_main implementations */
#if defined(SDL_MAIN_USE_CALLBACKS) \
    || defined(__WIN32__) || defined(__GDK__) || defined(__IOS__) || defined(__TVOS__) \
    || defined(__3DS__) || defined(__NGAGE__) || defined(__PS2__) || defined(__PSP__)

/* platforms which main (-equivalent) can be implemented in plain C */
#include <SDL3/SDL_main_impl.h>

#elif defined(__WINRT__) /* C++ platforms */

#ifdef __cplusplus
#include <SDL3/SDL_main_impl.h>
#else
/* Note: to get rid of the following warning, you can #define SDL_MAIN_NOIMPL before including SDL_main.h
 *  in your C sourcefile that contains the standard main. Do *not* use SDL_MAIN_HANDLED for that, then SDL_main won't find your main()!
 */
#ifdef _MSC_VER
#pragma message("Note: Your platform needs the SDL_main implementation in a C++ source file. You can keep your main() in plain C (then continue including SDL_main.h there!) and create a fresh .cpp file that only contains #include <SDL3/SDL_main.h>")
#elif defined(__GNUC__) /* gcc, clang, mingw and compatible are matched by this and have #warning */
#warning "Note: Your platform needs the SDL_main implementation in a C++ source file. You can keep your main() in plain C and create a fresh .cpp file that only contains #include <SDL3/SDL_main.h>"
#endif /* __GNUC__ */
#endif /* __cplusplus */

#endif /* C++ platforms like __WINRT__ etc */

#endif /* SDL_MAIN_HANDLED */

#endif /* SDL_main_h_ */
