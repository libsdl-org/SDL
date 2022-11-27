/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_syswm.h
 *
 *  Include file for SDL custom system window manager hooks.
 */

#ifndef SDL_syswm_h_
#define SDL_syswm_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_version.h>

/**
 *  \brief SDL_syswm.h
 *
 *  Your application has access to a special type of event ::SDL_SYSWMEVENT,
 *  which contains window-manager specific information and arrives whenever
 *  an unhandled window event occurs.  This event is ignored by default, but
 *  you can enable it with SDL_EventState().
 *
 *  As of SDL 3.0, this file no longer includes the platform specific headers
 *  and types. You should include the headers you need and define one or more
 *  of the following for the subsystems you're working with:
 *
 *      SDL_ENABLE_SYSWM_ANDROID
 *      SDL_ENABLE_SYSWM_COCOA
 *      SDL_ENABLE_SYSWM_KMSDRM
 *      SDL_ENABLE_SYSWM_UIKIT
 *      SDL_ENABLE_SYSWM_VIVANTE
 *      SDL_ENABLE_SYSWM_WAYLAND
 *      SDL_ENABLE_SYSWM_WINDOWS
 *      SDL_ENABLE_SYSWM_WINRT
 *      SDL_ENABLE_SYSWM_X11
 */
struct SDL_SysWMinfo;

#include <SDL3/begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* This is the current version of structures in this file */
#define SDL_SYSWM_CURRENT_VERSION   1
#define SDL_SYSWM_INFO_SIZE_V1      (16 * (sizeof (void *) >= 8 ? sizeof (void *) : sizeof(Uint64)))
#define SDL_SYSWM_CURRENT_INFO_SIZE SDL_SYSWM_INFO_SIZE_V1

/* This is the tag associated with a Metal view so you can find it */
#define SDL_METALVIEW_TAG 255


#if !defined(SDL_PROTOTYPES_ONLY)
/**
 *  These are the various supported windowing subsystems
 */
typedef enum
{
    SDL_SYSWM_UNKNOWN,
    SDL_SYSWM_ANDROID,
    SDL_SYSWM_COCOA,
    SDL_SYSWM_HAIKU,
    SDL_SYSWM_KMSDRM,
    SDL_SYSWM_RISCOS,
    SDL_SYSWM_UIKIT,
    SDL_SYSWM_VIVANTE,
    SDL_SYSWM_WAYLAND,
    SDL_SYSWM_WINDOWS,
    SDL_SYSWM_WINRT,
    SDL_SYSWM_X11
} SDL_SYSWM_TYPE;

/**
 *  The custom event structure.
 */
struct SDL_SysWMmsg
{
    Uint32 version;
    Uint32 subsystem;                   /**< SDL_SYSWM_TYPE */

    Uint32 padding[(2 * (sizeof (void *) >= 8 ? sizeof (void *) : sizeof(Uint64)) - 2 * sizeof(Uint32)) / sizeof(Uint32)];

    union
    {
#if defined(SDL_ENABLE_SYSWM_WINDOWS)
        struct {
            HWND hwnd;                  /**< The window for the message */
            UINT msg;                   /**< The type of message */
            WPARAM wParam;              /**< WORD message parameter */
            LPARAM lParam;              /**< LONG message parameter */
        } win;
#endif
#if defined(SDL_ENABLE_SYSWM_X11)
        struct {
            XEvent event;
        } x11;
#endif
        /* Can't have an empty union */
        int dummy;
    } msg;
};

/**
 *  The custom window manager information structure.
 *
 *  When this structure is returned, it holds information about which
 *  low level system it is using, and will be one of SDL_SYSWM_TYPE.
 */
struct SDL_SysWMinfo
{
    Uint32 version;
    Uint32 subsystem;                   /**< SDL_SYSWM_TYPE */

    Uint32 padding[(2 * (sizeof (void *) >= 8 ? sizeof (void *) : sizeof(Uint64)) - 2 * sizeof(Uint32)) / sizeof(Uint32)];

    union
    {
#if defined(SDL_ENABLE_SYSWM_WINDOWS)
        struct
        {
            HWND window;                /**< The window handle */
            HDC hdc;                    /**< The window device context */
            HINSTANCE hinstance;        /**< The instance handle */
        } win;
#endif
#if defined(SDL_ENABLE_SYSWM_WINRT)
        struct
        {
            IInspectable * window;      /**< The WinRT CoreWindow */
        } winrt;
#endif
#if defined(SDL_ENABLE_SYSWM_X11)
        struct
        {
            Display *display;           /**< The X11 display */
            int screen;                 /**< The X11 screen */
            Window window;              /**< The X11 window */
        } x11;
#endif
#if defined(SDL_ENABLE_SYSWM_COCOA)
        struct
        {
#if defined(__OBJC__) && defined(__has_feature)
        #if __has_feature(objc_arc)
            NSWindow __unsafe_unretained *window; /**< The Cocoa window */
        #else
            NSWindow *window;                     /**< The Cocoa window */
        #endif
#else
            NSWindow *window;                     /**< The Cocoa window */
#endif
        } cocoa;
#endif
#if defined(SDL_ENABLE_SYSWM_UIKIT)
        struct
        {
#if defined(__OBJC__) && defined(__has_feature)
        #if __has_feature(objc_arc)
            UIWindow __unsafe_unretained *window; /**< The UIKit window */
        #else
            UIWindow *window;                     /**< The UIKit window */
        #endif
#else
            UIWindow *window;                     /**< The UIKit window */
#endif
            GLuint framebuffer; /**< The GL view's Framebuffer Object. It must be bound when rendering to the screen using GL. */
            GLuint colorbuffer; /**< The GL view's color Renderbuffer Object. It must be bound when SDL_GL_SwapWindow is called. */
            GLuint resolveFramebuffer; /**< The Framebuffer Object which holds the resolve color Renderbuffer, when MSAA is used. */
        } uikit;
#endif
#if defined(SDL_ENABLE_SYSWM_WAYLAND)
        struct
        {
            struct wl_display *display;             /**< Wayland display */
            struct wl_surface *surface;             /**< Wayland surface */
            struct wl_egl_window *egl_window;       /**< Wayland EGL window (native window) */
            struct xdg_surface *xdg_surface;        /**< Wayland xdg surface (window manager handle) */
            struct xdg_toplevel *xdg_toplevel;      /**< Wayland xdg toplevel role */
            struct xdg_popup *xdg_popup;            /**< Wayland xdg popup role */
            struct xdg_positioner *xdg_positioner;  /**< Wayland xdg positioner, for popup */
        } wl;
#endif

#if defined(SDL_ENABLE_SYSWM_ANDROID)
        struct
        {
            ANativeWindow *window;
            EGLSurface surface;
        } android;
#endif

#if defined(SDL_ENABLE_SYSWM_VIVANTE)
        struct
        {
            EGLNativeDisplayType display;
            EGLNativeWindowType window;
        } vivante;
#endif

#if defined(SDL_ENABLE_SYSWM_KMSDRM)
        struct
        {
            int dev_index;               /**< Device index (ex: the X in /dev/dri/cardX) */
            int drm_fd;                  /**< DRM FD (unavailable on Vulkan windows) */
            struct gbm_device *gbm_dev;  /**< GBM device (unavailable on Vulkan windows) */
        } kmsdrm;
#endif

        /* Make sure this union has enough room for 14 pointers */
        void *dummy_ptrs[14];
        Uint64 dummy_ints[14];
    } info;
};
SDL_COMPILE_TIME_ASSERT(SDL_SysWMinfo_size, sizeof(struct SDL_SysWMinfo) == SDL_SYSWM_CURRENT_INFO_SIZE);

#endif /* SDL_PROTOTYPES_ONLY */

typedef struct SDL_SysWMinfo SDL_SysWMinfo;


/**
 * Get driver-specific information about a window.
 *
 * You must include SDL_syswm.h for the declaration of SDL_SysWMinfo.
 *
 * \param window the window about which information is being requested
 * \param info an SDL_SysWMinfo structure filled in with window information
 * \param version the version of info being requested, should be
 *                SDL_SYSWM_CURRENT_VERSION
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetWindowWMInfo(SDL_Window *window, SDL_SysWMinfo *info, Uint32 version);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/close_code.h>

#endif /* SDL_syswm_h_ */

/* vi: set ts=4 sw=4 expandtab: */
