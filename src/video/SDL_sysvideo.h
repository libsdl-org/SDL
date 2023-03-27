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

#ifndef SDL_sysvideo_h_
#define SDL_sysvideo_h_

#include "SDL_vulkan_internal.h"

/* The SDL video driver */

typedef struct SDL_WindowShaper SDL_WindowShaper;
typedef struct SDL_ShapeDriver SDL_ShapeDriver;
typedef struct SDL_VideoDisplay SDL_VideoDisplay;
typedef struct SDL_VideoDevice SDL_VideoDevice;
typedef struct SDL_VideoData SDL_VideoData;
typedef struct SDL_DisplayData SDL_DisplayData;
typedef struct SDL_DisplayModeData SDL_DisplayModeData;
typedef struct SDL_WindowData SDL_WindowData;

/* Define the SDL window-shaper structure */
struct SDL_WindowShaper
{
    /* The window associated with the shaper */
    SDL_Window *window;

    /* The parameters for shape calculation. */
    SDL_WindowShapeMode mode;

    /* Has this window been assigned a shape? */
    SDL_bool hasshape;

    void *driverdata;
};

/* Define the SDL shape driver structure */
struct SDL_ShapeDriver
{
    SDL_WindowShaper *(*CreateShaper)(SDL_Window *window);
    int (*SetWindowShape)(SDL_WindowShaper *shaper, SDL_Surface *shape, SDL_WindowShapeMode *shape_mode);
};

typedef struct SDL_WindowUserData
{
    char *name;
    void *data;
    struct SDL_WindowUserData *next;
} SDL_WindowUserData;

/* Define the SDL window structure, corresponding to toplevel windows */
struct SDL_Window
{
    const void *magic;
    SDL_WindowID id;
    char *title;
    SDL_Surface *icon;
    int x, y;
    int w, h;
    int min_w, min_h;
    int max_w, max_h;
    int last_pixel_w, last_pixel_h;
    Uint32 flags;
    SDL_bool fullscreen_exclusive;  /* The window is currently fullscreen exclusive */
    SDL_DisplayID last_fullscreen_exclusive_display;  /* The last fullscreen_exclusive display */
    SDL_DisplayID last_displayID;

    /* Stored position and size for windowed mode */
    SDL_Rect windowed;

    /* Whether or not the intial position was defined */
    SDL_bool undefined_x;
    SDL_bool undefined_y;

    SDL_DisplayMode requested_fullscreen_mode;
    SDL_DisplayMode current_fullscreen_mode;

    float opacity;

    SDL_Surface *surface;
    SDL_bool surface_valid;

    SDL_bool is_hiding;
    SDL_bool restore_on_show; /* Child was hidden recursively by the parent, restore when shown. */
    SDL_bool is_destroying;
    SDL_bool is_dropping; /* drag/drop in progress, expecting SDL_SendDropComplete(). */

    SDL_Rect mouse_rect;

    SDL_WindowShaper *shaper;

    SDL_HitTest hit_test;
    void *hit_test_data;

    SDL_WindowUserData *data;

    SDL_WindowData *driverdata;

    SDL_Window *prev;
    SDL_Window *next;

    SDL_Window *parent;
    SDL_Window *first_child;
    SDL_Window *prev_sibling;
    SDL_Window *next_sibling;
};
#define SDL_WINDOW_FULLSCREEN_VISIBLE(W)        \
    ((((W)->flags & SDL_WINDOW_FULLSCREEN) != 0) &&   \
     (((W)->flags & SDL_WINDOW_HIDDEN) == 0) && \
     (((W)->flags & SDL_WINDOW_MINIMIZED) == 0))

#define SDL_WINDOW_IS_POPUP(W)                   \
    ((((W)->flags & SDL_WINDOW_TOOLTIP) != 0) || \
    (((W)->flags & SDL_WINDOW_POPUP_MENU) != 0)) \
                                                 \
/*
 * Define the SDL display structure.
 * This corresponds to physical monitors attached to the system.
 */
struct SDL_VideoDisplay
{
    SDL_DisplayID id;
    char *name;
    int max_fullscreen_modes;
    int num_fullscreen_modes;
    SDL_DisplayMode *fullscreen_modes;
    SDL_DisplayMode desktop_mode;
    const SDL_DisplayMode *current_mode;
    SDL_DisplayOrientation orientation;

    SDL_Window *fullscreen_window;

    SDL_VideoDevice *device;

    SDL_DisplayData *driverdata;
};

/* Forward declaration */
struct SDL_SysWMinfo;

/* Define the SDL video driver structure */
#define _THIS SDL_VideoDevice *_this

/* Video device flags */
typedef enum
{
    VIDEO_DEVICE_QUIRK_MODE_SWITCHING_EMULATED = 0x01,
    VIDEO_DEVICE_QUIRK_DISABLE_UNSET_FULLSCREEN_ON_MINIMIZE = 0x02,
    VIDEO_DEVICE_QUIRK_HAS_POPUP_WINDOW_SUPPORT = 0x04,
} DeviceQuirkFlags;

struct SDL_VideoDevice
{
    /* * * */
    /* The name of this video driver */
    const char *name;

    /* * * */
    /* Initialization/Query functions */

    /*
     * Initialize the native video subsystem, filling in the list of
     * displays for this driver, returning 0 or -1 if there's an error.
     */
    int (*VideoInit)(_THIS);

    /*
     * Reverse the effects VideoInit() -- called if VideoInit() fails or
     * if the application is shutting down the video subsystem.
     */
    void (*VideoQuit)(_THIS);

    /*
     * Reinitialize the touch devices -- called if an unknown touch ID occurs.
     */
    void (*ResetTouch)(_THIS);

    /* * * */
    /*
     * Display functions
     */

    /*
     * Refresh the display list
     */
    void (*RefreshDisplays)(_THIS);

    /*
     * Get the bounds of a display
     */
    int (*GetDisplayBounds)(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect);

    /*
     * Get the usable bounds of a display (bounds minus menubar or whatever)
     */
    int (*GetDisplayUsableBounds)(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect);

    /*
     * Get a list of the available display modes for a display.
     */
    int (*GetDisplayModes)(_THIS, SDL_VideoDisplay *display);

    /*
     * Setting the display mode is independent of creating windows, so
     * when the display mode is changed, all existing windows should have
     * their data updated accordingly, including the display surfaces
     * associated with them.
     */
    int (*SetDisplayMode)(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);

    /* * * */
    /*
     * Window functions
     */
    int (*CreateSDLWindow)(_THIS, SDL_Window *window);
    int (*CreateSDLWindowFrom)(_THIS, SDL_Window *window, const void *data);
    void (*SetWindowTitle)(_THIS, SDL_Window *window);
    int (*SetWindowIcon)(_THIS, SDL_Window *window, SDL_Surface *icon);
    void (*SetWindowPosition)(_THIS, SDL_Window *window);
    void (*SetWindowSize)(_THIS, SDL_Window *window);
    void (*SetWindowMinimumSize)(_THIS, SDL_Window *window);
    void (*SetWindowMaximumSize)(_THIS, SDL_Window *window);
    int (*GetWindowBordersSize)(_THIS, SDL_Window *window, int *top, int *left, int *bottom, int *right);
    void (*GetWindowSizeInPixels)(_THIS, SDL_Window *window, int *w, int *h);
    int (*SetWindowOpacity)(_THIS, SDL_Window *window, float opacity);
    int (*SetWindowModalFor)(_THIS, SDL_Window *modal_window, SDL_Window *parent_window);
    int (*SetWindowInputFocus)(_THIS, SDL_Window *window);
    void (*ShowWindow)(_THIS, SDL_Window *window);
    void (*HideWindow)(_THIS, SDL_Window *window);
    void (*RaiseWindow)(_THIS, SDL_Window *window);
    void (*MaximizeWindow)(_THIS, SDL_Window *window);
    void (*MinimizeWindow)(_THIS, SDL_Window *window);
    void (*RestoreWindow)(_THIS, SDL_Window *window);
    void (*SetWindowBordered)(_THIS, SDL_Window *window, SDL_bool bordered);
    void (*SetWindowResizable)(_THIS, SDL_Window *window, SDL_bool resizable);
    void (*SetWindowAlwaysOnTop)(_THIS, SDL_Window *window, SDL_bool on_top);
    void (*SetWindowFullscreen)(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
    void *(*GetWindowICCProfile)(_THIS, SDL_Window *window, size_t *size);
    SDL_DisplayID (*GetDisplayForWindow)(_THIS, SDL_Window *window);
    void (*SetWindowMouseRect)(_THIS, SDL_Window *window);
    void (*SetWindowMouseGrab)(_THIS, SDL_Window *window, SDL_bool grabbed);
    void (*SetWindowKeyboardGrab)(_THIS, SDL_Window *window, SDL_bool grabbed);
    void (*DestroyWindow)(_THIS, SDL_Window *window);
    int (*CreateWindowFramebuffer)(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch);
    int (*UpdateWindowFramebuffer)(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects);
    void (*DestroyWindowFramebuffer)(_THIS, SDL_Window *window);
    void (*OnWindowEnter)(_THIS, SDL_Window *window);
    int (*FlashWindow)(_THIS, SDL_Window *window, SDL_FlashOperation operation);

    /* * * */
    /*
     * Shaped-window functions
     */
    SDL_ShapeDriver shape_driver;

    /* Get some platform dependent window information */
    int (*GetWindowWMInfo)(_THIS, SDL_Window *window, struct SDL_SysWMinfo *info);

    /* * * */
    /*
     * OpenGL support
     */
    int (*GL_LoadLibrary)(_THIS, const char *path);
    SDL_FunctionPointer (*GL_GetProcAddress)(_THIS, const char *proc);
    void (*GL_UnloadLibrary)(_THIS);
    SDL_GLContext (*GL_CreateContext)(_THIS, SDL_Window *window);
    int (*GL_MakeCurrent)(_THIS, SDL_Window *window, SDL_GLContext context);
    SDL_EGLSurface (*GL_GetEGLSurface)(_THIS, SDL_Window *window);
    int (*GL_SetSwapInterval)(_THIS, int interval);
    int (*GL_GetSwapInterval)(_THIS, int *interval);
    int (*GL_SwapWindow)(_THIS, SDL_Window *window);
    int (*GL_DeleteContext)(_THIS, SDL_GLContext context);
    void (*GL_DefaultProfileConfig)(_THIS, int *mask, int *major, int *minor);

    /* * * */
    /*
     * Vulkan support
     */
    int (*Vulkan_LoadLibrary)(_THIS, const char *path);
    void (*Vulkan_UnloadLibrary)(_THIS);
    SDL_bool (*Vulkan_GetInstanceExtensions)(_THIS, unsigned *count, const char **names);
    SDL_bool (*Vulkan_CreateSurface)(_THIS, SDL_Window *window, VkInstance instance, VkSurfaceKHR *surface);

    /* * * */
    /*
     * Metal support
     */
    SDL_MetalView (*Metal_CreateView)(_THIS, SDL_Window *window);
    void (*Metal_DestroyView)(_THIS, SDL_MetalView view);
    void *(*Metal_GetLayer)(_THIS, SDL_MetalView view);

    /* * * */
    /*
     * Event manager functions
     */
    int (*WaitEventTimeout)(_THIS, Sint64 timeoutNS);
    void (*SendWakeupEvent)(_THIS, SDL_Window *window);
    void (*PumpEvents)(_THIS);

    /* Suspend the screensaver */
    int (*SuspendScreenSaver)(_THIS);

    /* Text input */
    void (*StartTextInput)(_THIS);
    void (*StopTextInput)(_THIS);
    int (*SetTextInputRect)(_THIS, const SDL_Rect *rect);
    void (*ClearComposition)(_THIS);
    SDL_bool (*IsTextInputShown)(_THIS);

    /* Screen keyboard */
    SDL_bool (*HasScreenKeyboardSupport)(_THIS);
    void (*ShowScreenKeyboard)(_THIS, SDL_Window *window);
    void (*HideScreenKeyboard)(_THIS, SDL_Window *window);
    SDL_bool (*IsScreenKeyboardShown)(_THIS, SDL_Window *window);

    /* Clipboard */
    int (*SetClipboardText)(_THIS, const char *text);
    char *(*GetClipboardText)(_THIS);
    SDL_bool (*HasClipboardText)(_THIS);
    int (*SetPrimarySelectionText)(_THIS, const char *text);
    char *(*GetPrimarySelectionText)(_THIS);
    SDL_bool (*HasPrimarySelectionText)(_THIS);

    /* MessageBox */
    int (*ShowMessageBox)(_THIS, const SDL_MessageBoxData *messageboxdata, int *buttonid);

    /* Hit-testing */
    int (*SetWindowHitTest)(SDL_Window *window, SDL_bool enabled);

    /* Tell window that app enabled drag'n'drop events */
    void (*AcceptDragAndDrop)(SDL_Window *window, SDL_bool accept);

    /* * * */
    /* Data common to all drivers */
    SDL_threadID thread;
    SDL_bool checked_texture_framebuffer;
    SDL_bool is_dummy;
    SDL_bool suspend_screensaver;
    SDL_Window *wakeup_window;
    SDL_mutex *wakeup_lock; /* Initialized only if WaitEventTimeout/SendWakeupEvent are supported */
    int num_displays;
    SDL_VideoDisplay *displays;
    SDL_Window *windows;
    SDL_Window *grabbed_window;
    Uint8 window_magic;
    SDL_WindowID next_object_id;
    char *clipboard_text;
    char *primary_selection_text;
    SDL_bool setting_display_mode;
    Uint32 quirk_flags;
    SDL_SystemTheme system_theme;

    /* * * */
    /* Data used by the GL drivers */
    struct
    {
        int red_size;
        int green_size;
        int blue_size;
        int alpha_size;
        int depth_size;
        int buffer_size;
        int stencil_size;
        int double_buffer;
        int accum_red_size;
        int accum_green_size;
        int accum_blue_size;
        int accum_alpha_size;
        int stereo;
        int multisamplebuffers;
        int multisamplesamples;
        int floatbuffers;
        int accelerated;
        int major_version;
        int minor_version;
        int flags;
        int profile_mask;
        int share_with_current_context;
        int release_behavior;
        int reset_notification;
        int framebuffer_srgb_capable;
        int no_error;
        int retained_backing;
        int egl_platform;
        int driver_loaded;
        char driver_path[256];
        void *dll_handle;
    } gl_config;

    SDL_EGLAttribArrayCallback egl_platformattrib_callback;
    SDL_EGLIntArrayCallback egl_surfaceattrib_callback;
    SDL_EGLIntArrayCallback egl_contextattrib_callback;

    /* * * */
    /* Cache current GL context; don't call the OS when it hasn't changed. */
    /* We have the global pointers here so Cocoa continues to work the way
       it always has, and the thread-local storage for the general case.
     */
    SDL_Window *current_glwin;
    SDL_GLContext current_glctx;
    SDL_TLSID current_glwin_tls;
    SDL_TLSID current_glctx_tls;

    /* Flag that stores whether it's allowed to call SDL_GL_MakeCurrent()
     * with a NULL window, but a non-NULL context. (Not allowed in most cases,
     * except on EGL under some circumstances.) */
    SDL_bool gl_allow_no_surface;

    /* * * */
    /* Data used by the Vulkan drivers */
    struct
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
        PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
        int loader_loaded;
        char loader_path[256];
        void *loader_handle;
    } vulkan_config;

    /* * * */
    /* Data private to this driver */
    SDL_VideoData *driverdata;
    struct SDL_GLDriverData *gl_data;

#ifdef SDL_VIDEO_OPENGL_EGL
    struct SDL_EGL_VideoData *egl_data;
#endif

#if defined(SDL_VIDEO_OPENGL_ES) || defined(SDL_VIDEO_OPENGL_ES2)
    struct SDL_PrivateGLESData *gles_data;
#endif

    /* * * */
    /* The function used to dispose of this structure */
    void (*free)(_THIS);
};

typedef struct VideoBootStrap
{
    const char *name;
    const char *desc;
    SDL_VideoDevice *(*create)(void);
} VideoBootStrap;

/* Not all of these are available in a given build. Use #ifdefs, etc. */
extern VideoBootStrap COCOA_bootstrap;
extern VideoBootStrap X11_bootstrap;
extern VideoBootStrap WINDOWS_bootstrap;
extern VideoBootStrap WINRT_bootstrap;
extern VideoBootStrap HAIKU_bootstrap;
extern VideoBootStrap PND_bootstrap;
extern VideoBootStrap UIKIT_bootstrap;
extern VideoBootStrap Android_bootstrap;
extern VideoBootStrap PS2_bootstrap;
extern VideoBootStrap PSP_bootstrap;
extern VideoBootStrap VITA_bootstrap;
extern VideoBootStrap RISCOS_bootstrap;
extern VideoBootStrap N3DS_bootstrap;
extern VideoBootStrap RPI_bootstrap;
extern VideoBootStrap KMSDRM_bootstrap;
extern VideoBootStrap KMSDRM_LEGACY_bootstrap;
extern VideoBootStrap DUMMY_bootstrap;
extern VideoBootStrap DUMMY_evdev_bootstrap;
extern VideoBootStrap Wayland_bootstrap;
extern VideoBootStrap VIVANTE_bootstrap;
extern VideoBootStrap Emscripten_bootstrap;
extern VideoBootStrap OFFSCREEN_bootstrap;
extern VideoBootStrap NGAGE_bootstrap;
extern VideoBootStrap QNX_bootstrap;

/* Use SDL_OnVideoThread() sparingly, to avoid regressions in use cases that currently happen to work */
extern SDL_bool SDL_OnVideoThread(void);
extern SDL_VideoDevice *SDL_GetVideoDevice(void);
extern SDL_bool SDL_IsVideoContextExternal(void);
extern void SDL_SetSystemTheme(SDL_SystemTheme theme);
extern SDL_DisplayID SDL_AddBasicVideoDisplay(const SDL_DisplayMode *desktop_mode);
extern SDL_DisplayID SDL_AddVideoDisplay(const SDL_VideoDisplay *display, SDL_bool send_event);
extern void SDL_DelVideoDisplay(SDL_DisplayID display, SDL_bool send_event);
extern SDL_bool SDL_AddFullscreenDisplayMode(SDL_VideoDisplay *display, const SDL_DisplayMode *mode);
extern void SDL_ResetFullscreenDisplayModes(SDL_VideoDisplay *display);
extern void SDL_SetDesktopDisplayMode(SDL_VideoDisplay *display, const SDL_DisplayMode *mode);
extern void SDL_SetCurrentDisplayMode(SDL_VideoDisplay *display, const SDL_DisplayMode *mode);
extern SDL_VideoDisplay *SDL_GetVideoDisplay(SDL_DisplayID display);
extern SDL_VideoDisplay *SDL_GetVideoDisplayForWindow(SDL_Window *window);
extern int SDL_GetDisplayIndex(SDL_DisplayID displayID);
extern SDL_DisplayData *SDL_GetDisplayDriverData(SDL_DisplayID display);
extern SDL_DisplayData *SDL_GetDisplayDriverDataForWindow(SDL_Window *window);
extern int SDL_GetMessageBoxCount(void);

extern void SDL_GL_DeduceMaxSupportedESProfile(int *major, int *minor);

extern int SDL_RecreateWindow(SDL_Window *window, Uint32 flags);
extern SDL_bool SDL_HasWindows(void);
extern void SDL_RelativeToGlobalForWindow(SDL_Window *window, int rel_x, int rel_y, int *abs_x, int *abs_y);
extern void SDL_GlobalToRelativeForWindow(SDL_Window *window, int abs_x, int abs_y, int *rel_x, int *rel_y);

extern void SDL_OnWindowShown(SDL_Window *window);
extern void SDL_OnWindowHidden(SDL_Window *window);
extern void SDL_OnWindowMoved(SDL_Window *window);
extern void SDL_OnWindowResized(SDL_Window *window);
extern void SDL_CheckWindowPixelSizeChanged(SDL_Window *window);
extern void SDL_OnWindowPixelSizeChanged(SDL_Window *window);
extern void SDL_OnWindowMinimized(SDL_Window *window);
extern void SDL_OnWindowMaximized(SDL_Window *window);
extern void SDL_OnWindowRestored(SDL_Window *window);
extern void SDL_OnWindowEnter(SDL_Window *window);
extern void SDL_OnWindowLeave(SDL_Window *window);
extern void SDL_OnWindowFocusGained(SDL_Window *window);
extern void SDL_OnWindowFocusLost(SDL_Window *window);
extern void SDL_OnWindowDisplayChanged(SDL_Window *window);
extern void SDL_UpdateWindowGrab(SDL_Window *window);
extern SDL_Window *SDL_GetFocusWindow(void);

extern SDL_bool SDL_ShouldAllowTopmost(void);

extern void SDL_ToggleDragAndDropSupport(void);

/* This has been moved out of the public API, but is still available for now */
#define SDL_WINDOW_ALLOW_HIGHDPI    0x00002000

#endif /* SDL_sysvideo_h_ */
