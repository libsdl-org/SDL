/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_assert.h"

/* GLX implementation of SDL OpenGL support */

#if SDL_VIDEO_OPENGL_GLX
#include "SDL_loadso.h"
#include "SDL_x11opengles.h"

#if defined(__IRIX__)
/* IRIX doesn't have a GL library versioning system */
#define DEFAULT_OPENGL  "libGL.so"
#elif defined(__MACOSX__)
#define DEFAULT_OPENGL  "/usr/X11R6/lib/libGL.1.dylib"
#elif defined(__QNXNTO__)
#define DEFAULT_OPENGL  "libGL.so.3"
#else
#define DEFAULT_OPENGL  "libGL.so.1"
#endif

#ifndef GLX_NONE_EXT
#define GLX_NONE_EXT                       0x8000
#endif

#ifndef GLX_ARB_multisample
#define GLX_ARB_multisample
#define GLX_SAMPLE_BUFFERS_ARB             100000
#define GLX_SAMPLES_ARB                    100001
#endif

#ifndef GLX_EXT_visual_rating
#define GLX_EXT_visual_rating
#define GLX_VISUAL_CAVEAT_EXT              0x20
#define GLX_NONE_EXT                       0x8000
#define GLX_SLOW_VISUAL_EXT                0x8001
#define GLX_NON_CONFORMANT_VISUAL_EXT      0x800D
#endif

#ifndef GLX_EXT_visual_info
#define GLX_EXT_visual_info
#define GLX_X_VISUAL_TYPE_EXT              0x22
#define GLX_DIRECT_COLOR_EXT               0x8003
#endif

#ifndef GLX_ARB_create_context
#define GLX_ARB_create_context
#define GLX_CONTEXT_MAJOR_VERSION_ARB      0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB      0x2092
#define GLX_CONTEXT_FLAGS_ARB              0x2094
#define GLX_CONTEXT_DEBUG_BIT_ARB          0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002

/* Typedef for the GL 3.0 context creation function */
typedef GLXContext(*PFNGLXCREATECONTEXTATTRIBSARBPROC) (Display * dpy,
                                                        GLXFBConfig config,
                                                        GLXContext
                                                        share_context,
                                                        Bool direct,
                                                        const int
                                                        *attrib_list);
#endif

#ifndef GLX_ARB_create_context_profile
#define GLX_ARB_create_context_profile
#define GLX_CONTEXT_PROFILE_MASK_ARB       0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB   0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

#ifndef GLX_ARB_create_context_robustness
#define GLX_ARB_create_context_robustness
#define GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB  0x00000004
#define GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB     0x8256
#define GLX_NO_RESET_NOTIFICATION_ARB                   0x8261
#define GLX_LOSE_CONTEXT_ON_RESET_ARB                   0x8252
#endif

#ifndef GLX_EXT_create_context_es2_profile
#define GLX_EXT_create_context_es2_profile
#ifndef GLX_CONTEXT_ES2_PROFILE_BIT_EXT
#define GLX_CONTEXT_ES2_PROFILE_BIT_EXT    0x00000002
#endif
#endif

#ifndef GLX_ARB_framebuffer_sRGB
#define GLX_ARB_framebuffer_sRGB
#ifndef GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB
#define GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB                0x20B2
#endif
#endif

#ifndef GLX_EXT_swap_control
#define GLX_SWAP_INTERVAL_EXT              0x20F1
#define GLX_MAX_SWAP_INTERVAL_EXT          0x20F2
#endif

#ifndef GLX_EXT_swap_control_tear
#define GLX_LATE_SWAPS_TEAR_EXT 0x20F3
#endif

#ifndef GLX_ARB_context_flush_control
#define GLX_ARB_context_flush_control
#define GLX_CONTEXT_RELEASE_BEHAVIOR_ARB   0x2097
#define GLX_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB           0x0000
#define GLX_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB          0x2098
#endif

#define OPENGL_REQUIRES_DLOPEN
#if defined(OPENGL_REQUIRES_DLOPEN) && defined(SDL_LOADSO_DLOPEN)
#include <dlfcn.h>
#define GL_LoadObject(X)    dlopen(X, (RTLD_NOW|RTLD_GLOBAL))
#define GL_LoadFunction     dlsym
#define GL_UnloadObject     dlclose
#else
#define GL_LoadObject   SDL_LoadObject
#define GL_LoadFunction SDL_LoadFunction
#define GL_UnloadObject SDL_UnloadObject
#endif

static void X11_GL_InitExtensions(_THIS);


int
X11_GL_LoadLibrary(_THIS, const char *path)
{
    Display *display;
    void *handle;

    if (_this->gl_data) {
        return SDL_SetError("OpenGL context already created");
    }

    /* Load the OpenGL library */
    if (path == NULL) {
        path = SDL_getenv("SDL_OPENGL_LIBRARY");
    }
    if (path == NULL) {
        path = DEFAULT_OPENGL;
    }
    _this->gl_config.dll_handle = GL_LoadObject(path);
    if (!_this->gl_config.dll_handle) {
#if defined(OPENGL_REQUIRES_DLOPEN) && defined(SDL_LOADSO_DLOPEN)
        SDL_SetError("Failed loading %s: %s", path, dlerror());
#endif
        return -1;
    }
    SDL_strlcpy(_this->gl_config.driver_path, path,
                SDL_arraysize(_this->gl_config.driver_path));

    /* Allocate OpenGL memory */
    _this->gl_data =
        (struct SDL_GLDriverData *) SDL_calloc(1,
                                               sizeof(struct
                                                      SDL_GLDriverData));
    if (!_this->gl_data) {
        return SDL_OutOfMemory();
    }

    /* Load function pointers */
    handle = _this->gl_config.dll_handle;
    _this->gl_data->glXQueryExtension =
        (Bool (*)(Display *, int *, int *))
            GL_LoadFunction(handle, "glXQueryExtension");
    _this->gl_data->glXGetProcAddress =
        (void *(*)(const GLubyte *))
            GL_LoadFunction(handle, "glXGetProcAddressARB");
    _this->gl_data->glXChooseVisual =
        (XVisualInfo * (*)(Display *, int, int *))
            X11_GL_GetProcAddress(_this, "glXChooseVisual");
    _this->gl_data->glXCreateContext =
        (GLXContext(*)(Display *, XVisualInfo *, GLXContext, int))
            X11_GL_GetProcAddress(_this, "glXCreateContext");
    _this->gl_data->glXDestroyContext =
        (void (*)(Display *, GLXContext))
            X11_GL_GetProcAddress(_this, "glXDestroyContext");
    _this->gl_data->glXMakeCurrent =
        (int (*)(Display *, GLXDrawable, GLXContext))
            X11_GL_GetProcAddress(_this, "glXMakeCurrent");
    _this->gl_data->glXSwapBuffers =
        (void (*)(Display *, GLXDrawable))
            X11_GL_GetProcAddress(_this, "glXSwapBuffers");
    _this->gl_data->glXQueryDrawable =
        (void (*)(Display*,GLXDrawable,int,unsigned int*))
            X11_GL_GetProcAddress(_this, "glXQueryDrawable");

    if (!_this->gl_data->glXQueryExtension ||
        !_this->gl_data->glXChooseVisual ||
        !_this->gl_data->glXCreateContext ||
        !_this->gl_data->glXDestroyContext ||
        !_this->gl_data->glXMakeCurrent ||
        !_this->gl_data->glXSwapBuffers) {
        return SDL_SetError("Could not retrieve OpenGL functions");
    }

    display = ((SDL_VideoData *) _this->driverdata)->display;
    if (!_this->gl_data->glXQueryExtension(display, &_this->gl_data->errorBase, &_this->gl_data->eventBase)) {
        return SDL_SetError("GLX is not supported");
    }

    /* Initialize extensions */
    X11_GL_InitExtensions(_this);
    
    /* If we need a GL ES context and there's no  
     * GLX_EXT_create_context_es2_profile extension, switch over to X11_GLES functions  
     */
    if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES && 
        ! _this->gl_data->HAS_GLX_EXT_create_context_es2_profile ) {
#if SDL_VIDEO_OPENGL_EGL
        X11_GL_UnloadLibrary(_this);
        /* Better avoid conflicts! */
        if (_this->gl_config.dll_handle != NULL ) {
            GL_UnloadObject(_this->gl_config.dll_handle);
            _this->gl_config.dll_handle = NULL;
        }
        _this->GL_LoadLibrary = X11_GLES_LoadLibrary;
        _this->GL_GetProcAddress = X11_GLES_GetProcAddress;
        _this->GL_UnloadLibrary = X11_GLES_UnloadLibrary;
        _this->GL_CreateContext = X11_GLES_CreateContext;
        _this->GL_MakeCurrent = X11_GLES_MakeCurrent;
        _this->GL_SetSwapInterval = X11_GLES_SetSwapInterval;
        _this->GL_GetSwapInterval = X11_GLES_GetSwapInterval;
        _this->GL_SwapWindow = X11_GLES_SwapWindow;
        _this->GL_DeleteContext = X11_GLES_DeleteContext;
        return X11_GLES_LoadLibrary(_this, NULL);
#else
        return SDL_SetError("SDL not configured with EGL support");
#endif
    }

    return 0;
}

void *
X11_GL_GetProcAddress(_THIS, const char *proc)
{
    if (_this->gl_data->glXGetProcAddress) {
        return _this->gl_data->glXGetProcAddress((const GLubyte *) proc);
    }
    return GL_LoadFunction(_this->gl_config.dll_handle, proc);
}

void
X11_GL_UnloadLibrary(_THIS)
{
    /* Don't actually unload the library, since it may have registered
     * X11 shutdown hooks, per the notes at:
     * http://dri.sourceforge.net/doc/DRIuserguide.html
     */
#if 0
    GL_UnloadObject(_this->gl_config.dll_handle);
    _this->gl_config.dll_handle = NULL;
#endif

    /* Free OpenGL memory */
    SDL_free(_this->gl_data);
    _this->gl_data = NULL;
}

static SDL_bool
HasExtension(const char *extension, const char *extensions)
{
    const char *start;
    const char *where, *terminator;

    if (!extensions)
        return SDL_FALSE;

    /* Extension names should not have spaces. */
    where = SDL_strchr(extension, ' ');
    if (where || *extension == '\0')
        return SDL_FALSE;

    /* It takes a bit of care to be fool-proof about parsing the
     * OpenGL extensions string. Don't be fooled by sub-strings,
     * etc. */

    start = extensions;

    for (;;) {
        where = SDL_strstr(start, extension);
        if (!where)
            break;

        terminator = where + SDL_strlen(extension);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return SDL_TRUE;

        start = terminator;
    }
    return SDL_FALSE;
}

static void
X11_GL_InitExtensions(_THIS)
{
    Display *display = ((SDL_VideoData *) _this->driverdata)->display;
    const int screen = DefaultScreen(display);
    const char *(*glXQueryExtensionsStringFunc) (Display *, int);
    const char *extensions;

    glXQueryExtensionsStringFunc =
        (const char *(*)(Display *, int)) X11_GL_GetProcAddress(_this,
                                                                "glXQueryExtensionsString");
    if (glXQueryExtensionsStringFunc) {
        extensions = glXQueryExtensionsStringFunc(display, screen);
    } else {
        extensions = NULL;
    }

    /* Check for GLX_EXT_swap_control(_tear) */
    _this->gl_data->HAS_GLX_EXT_swap_control_tear = SDL_FALSE;
    if (HasExtension("GLX_EXT_swap_control", extensions)) {
        _this->gl_data->glXSwapIntervalEXT =
            (void (*)(Display*,GLXDrawable,int))
                X11_GL_GetProcAddress(_this, "glXSwapIntervalEXT");
        if (HasExtension("GLX_EXT_swap_control_tear", extensions)) {
            _this->gl_data->HAS_GLX_EXT_swap_control_tear = SDL_TRUE;
        }
    }

    /* Check for GLX_MESA_swap_control */
    if (HasExtension("GLX_MESA_swap_control", extensions)) {
        _this->gl_data->glXSwapIntervalMESA =
            (int(*)(int)) X11_GL_GetProcAddress(_this, "glXSwapIntervalMESA");
        _this->gl_data->glXGetSwapIntervalMESA =
            (int(*)(void)) X11_GL_GetProcAddress(_this,
                                                   "glXGetSwapIntervalMESA");
    }

    /* Check for GLX_SGI_swap_control */
    if (HasExtension("GLX_SGI_swap_control", extensions)) {
        _this->gl_data->glXSwapIntervalSGI =
            (int (*)(int)) X11_GL_GetProcAddress(_this, "glXSwapIntervalSGI");
    }

    /* Check for GLX_ARB_create_context */
    if (HasExtension("GLX_ARB_create_context", extensions)) {
        _this->gl_data->glXCreateContextAttribsARB =
            (GLXContext (*)(Display*,GLXFBConfig,GLXContext,Bool,const int *))
                X11_GL_GetProcAddress(_this, "glXCreateContextAttribsARB");
        _this->gl_data->glXChooseFBConfig =
            (GLXFBConfig *(*)(Display *, int, const int *, int *))
                X11_GL_GetProcAddress(_this, "glXChooseFBConfig");
    }

    /* Check for GLX_EXT_visual_rating */
    if (HasExtension("GLX_EXT_visual_rating", extensions)) {
        _this->gl_data->HAS_GLX_EXT_visual_rating = SDL_TRUE;
    }

    /* Check for GLX_EXT_visual_info */
    if (HasExtension("GLX_EXT_visual_info", extensions)) {
        _this->gl_data->HAS_GLX_EXT_visual_info = SDL_TRUE;
    }
    
    /* Check for GLX_EXT_create_context_es2_profile */
    if (HasExtension("GLX_EXT_create_context_es2_profile", extensions)) {
        _this->gl_data->HAS_GLX_EXT_create_context_es2_profile = SDL_TRUE;
    }

    /* Check for GLX_ARB_context_flush_control */
    if (HasExtension("GLX_ARB_context_flush_control", extensions)) {
        _this->gl_data->HAS_GLX_ARB_context_flush_control = SDL_TRUE;
    }
}

/* glXChooseVisual and glXChooseFBConfig have some small differences in
 * the attribute encoding, it can be chosen with the for_FBConfig parameter.
 */
static int
X11_GL_GetAttributes(_THIS, Display * display, int screen, int * attribs, int size, Bool for_FBConfig)
{
    int i = 0;
    const int MAX_ATTRIBUTES = 64;

    /* assert buffer is large enough to hold all SDL attributes. */
    SDL_assert(size >= MAX_ATTRIBUTES);

    /* Setup our GLX attributes according to the gl_config. */
    if( for_FBConfig ) {
        attribs[i++] = GLX_RENDER_TYPE;
        attribs[i++] = GLX_RGBA_BIT;
    } else {
        attribs[i++] = GLX_RGBA;
    }
    attribs[i++] = GLX_RED_SIZE;
    attribs[i++] = _this->gl_config.red_size;
    attribs[i++] = GLX_GREEN_SIZE;
    attribs[i++] = _this->gl_config.green_size;
    attribs[i++] = GLX_BLUE_SIZE;
    attribs[i++] = _this->gl_config.blue_size;

    if (_this->gl_config.alpha_size) {
        attribs[i++] = GLX_ALPHA_SIZE;
        attribs[i++] = _this->gl_config.alpha_size;
    }

    if (_this->gl_config.double_buffer) {
        attribs[i++] = GLX_DOUBLEBUFFER;
        if( for_FBConfig ) {
            attribs[i++] = True;
        }
    }

    attribs[i++] = GLX_DEPTH_SIZE;
    attribs[i++] = _this->gl_config.depth_size;

    if (_this->gl_config.stencil_size) {
        attribs[i++] = GLX_STENCIL_SIZE;
        attribs[i++] = _this->gl_config.stencil_size;
    }

    if (_this->gl_config.accum_red_size) {
        attribs[i++] = GLX_ACCUM_RED_SIZE;
        attribs[i++] = _this->gl_config.accum_red_size;
    }

    if (_this->gl_config.accum_green_size) {
        attribs[i++] = GLX_ACCUM_GREEN_SIZE;
        attribs[i++] = _this->gl_config.accum_green_size;
    }

    if (_this->gl_config.accum_blue_size) {
        attribs[i++] = GLX_ACCUM_BLUE_SIZE;
        attribs[i++] = _this->gl_config.accum_blue_size;
    }

    if (_this->gl_config.accum_alpha_size) {
        attribs[i++] = GLX_ACCUM_ALPHA_SIZE;
        attribs[i++] = _this->gl_config.accum_alpha_size;
    }

    if (_this->gl_config.stereo) {
        attribs[i++] = GLX_STEREO;
        if( for_FBConfig ) {
            attribs[i++] = True;
        }
    }

    if (_this->gl_config.multisamplebuffers) {
        attribs[i++] = GLX_SAMPLE_BUFFERS_ARB;
        attribs[i++] = _this->gl_config.multisamplebuffers;
    }

    if (_this->gl_config.multisamplesamples) {
        attribs[i++] = GLX_SAMPLES_ARB;
        attribs[i++] = _this->gl_config.multisamplesamples;
    }

    if (_this->gl_config.framebuffer_srgb_capable) {
        attribs[i++] = GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB;
        attribs[i++] = True;  /* always needed, for_FBConfig or not! */
    }

    if (_this->gl_config.accelerated >= 0 &&
        _this->gl_data->HAS_GLX_EXT_visual_rating) {
        attribs[i++] = GLX_VISUAL_CAVEAT_EXT;
        attribs[i++] = _this->gl_config.accelerated ? GLX_NONE_EXT :
                                                      GLX_SLOW_VISUAL_EXT;
    }

    /* If we're supposed to use DirectColor visuals, and we've got the
       EXT_visual_info extension, then add GLX_X_VISUAL_TYPE_EXT. */
    if (X11_UseDirectColorVisuals() &&
        _this->gl_data->HAS_GLX_EXT_visual_info) {
        attribs[i++] = GLX_X_VISUAL_TYPE_EXT;
        attribs[i++] = GLX_DIRECT_COLOR_EXT;
    }

    attribs[i++] = None;

    SDL_assert(i <= MAX_ATTRIBUTES);

    return i;
}

XVisualInfo *
X11_GL_GetVisual(_THIS, Display * display, int screen)
{
    /* 64 seems nice. */
    int attribs[64];
    XVisualInfo *vinfo;

    if (!_this->gl_data) {
        /* The OpenGL library wasn't loaded, SDL_GetError() should have info */
        return NULL;
    }

    X11_GL_GetAttributes(_this, display, screen, attribs, 64, SDL_FALSE);
    vinfo = _this->gl_data->glXChooseVisual(display, screen, attribs);
    if (!vinfo) {
        SDL_SetError("Couldn't find matching GLX visual");
    }
    return vinfo;
}

#ifndef GLXBadContext
#define GLXBadContext 0
#endif
#ifndef GLXBadFBConfig
#define GLXBadFBConfig 9
#endif
#ifndef GLXBadProfileARB
#define GLXBadProfileARB 13
#endif
static int (*handler) (Display *, XErrorEvent *) = NULL;
static int errorBase = 0;
static int errorCode = 0;
static int
X11_GL_CreateContextErrorHandler(Display * d, XErrorEvent * e)
{
    char *x11_error = NULL;
    char x11_error_locale[256];

    errorCode = e->error_code;
    if (X11_XGetErrorText(d, errorCode, x11_error_locale, sizeof(x11_error_locale)) == Success)
    {
        x11_error = SDL_iconv_string("UTF-8", "", x11_error_locale, SDL_strlen(x11_error_locale)+1);
    }

    if (x11_error)
    {
        SDL_SetError("Could not create GL context: %s", x11_error);
        SDL_free(x11_error);
    }
    else
    {
        SDL_SetError("Could not create GL context: %i (Base %i)\n", errorCode, errorBase);
    }

    return (0);
}

SDL_GLContext
X11_GL_CreateContext(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    int screen =
        ((SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata)->screen;
    XWindowAttributes xattr;
    XVisualInfo v, *vinfo;
    int n;
    GLXContext context = NULL, share_context;

    if (_this->gl_config.share_with_current_context) {
        share_context = (GLXContext)SDL_GL_GetCurrentContext();
    } else {
        share_context = NULL;
    }

    /* We do this to create a clean separation between X and GLX errors. */
    X11_XSync(display, False);
    errorBase = _this->gl_data->errorBase;
    errorCode = Success;
    handler = X11_XSetErrorHandler(X11_GL_CreateContextErrorHandler);
    X11_XGetWindowAttributes(display, data->xwindow, &xattr);
    v.screen = screen;
    v.visualid = X11_XVisualIDFromVisual(xattr.visual);
    vinfo = X11_XGetVisualInfo(display, VisualScreenMask | VisualIDMask, &v, &n);
    if (vinfo) {
        if (_this->gl_config.major_version < 3 &&
            _this->gl_config.profile_mask == 0 &&
            _this->gl_config.flags == 0) {
            /* Create legacy context */
            context =
                _this->gl_data->glXCreateContext(display, vinfo, share_context, True);
        } else {
            /* max 10 attributes plus terminator */
            int attribs[11] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB,
                _this->gl_config.major_version,
                GLX_CONTEXT_MINOR_VERSION_ARB,
                _this->gl_config.minor_version,
                0
            };
            int iattr = 4;

            /* SDL profile bits match GLX profile bits */
            if( _this->gl_config.profile_mask != 0 ) {
                attribs[iattr++] = GLX_CONTEXT_PROFILE_MASK_ARB;
                attribs[iattr++] = _this->gl_config.profile_mask;
            }

            /* SDL flags match GLX flags */
            if( _this->gl_config.flags != 0 ) {
                attribs[iattr++] = GLX_CONTEXT_FLAGS_ARB;
                attribs[iattr++] = _this->gl_config.flags;
            }

            /* only set if glx extension is available */
            if( _this->gl_data->HAS_GLX_ARB_context_flush_control ) {
                attribs[iattr++] = GLX_CONTEXT_RELEASE_BEHAVIOR_ARB;
                attribs[iattr++] = 
                    _this->gl_config.release_behavior ? 
                    GLX_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB : 
                    GLX_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB;
            }

            attribs[iattr++] = 0;

            /* Get a pointer to the context creation function for GL 3.0 */
            if (!_this->gl_data->glXCreateContextAttribsARB) {
                SDL_SetError("OpenGL 3.0 and later are not supported by this system");
            } else {
                int glxAttribs[64];

                /* Create a GL 3.x context */
                GLXFBConfig *framebuffer_config = NULL;
                int fbcount = 0;

                X11_GL_GetAttributes(_this,display,screen,glxAttribs,64,SDL_TRUE);

                if (!_this->gl_data->glXChooseFBConfig
                    || !(framebuffer_config =
                        _this->gl_data->glXChooseFBConfig(display,
                                          DefaultScreen(display), glxAttribs,
                                          &fbcount))) {
                    SDL_SetError("No good framebuffers found. OpenGL 3.0 and later unavailable");
                } else {
                    context = _this->gl_data->glXCreateContextAttribsARB(display,
                                                    framebuffer_config[0],
                                                    share_context, True, attribs);
                }
            }
        }
        X11_XFree(vinfo);
    }
    X11_XSync(display, False);
    X11_XSetErrorHandler(handler);

    if (!context) {
        if (errorCode == Success) {
            SDL_SetError("Could not create GL context");
        }
        return NULL;
    }

    if (X11_GL_MakeCurrent(_this, window, context) < 0) {
        X11_GL_DeleteContext(_this, context);
        return NULL;
    }

    return context;
}

int
X11_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    Display *display = ((SDL_VideoData *) _this->driverdata)->display;
    Window drawable =
        (context ? ((SDL_WindowData *) window->driverdata)->xwindow : None);
    GLXContext glx_context = (GLXContext) context;

    if (!_this->gl_data) {
        return SDL_SetError("OpenGL not initialized");
    }

    if (!_this->gl_data->glXMakeCurrent(display, drawable, glx_context)) {
        return SDL_SetError("Unable to make GL context current");
    }

    return 0;
}

/*
   0 is a valid argument to glxSwapInterval(MESA|EXT) and setting it to 0
   will undo the effect of a previous call with a value that is greater
   than zero (or at least that is what the docs say). OTOH, 0 is an invalid
   argument to glxSwapIntervalSGI and it returns an error if you call it
   with 0 as an argument.
*/

static int swapinterval = -1;
int
X11_GL_SetSwapInterval(_THIS, int interval)
{
    int status = -1;

    if ((interval < 0) && (!_this->gl_data->HAS_GLX_EXT_swap_control_tear)) {
        SDL_SetError("Negative swap interval unsupported in this GL");
    } else if (_this->gl_data->glXSwapIntervalEXT) {
        Display *display = ((SDL_VideoData *) _this->driverdata)->display;
        const SDL_WindowData *windowdata = (SDL_WindowData *)
            SDL_GL_GetCurrentWindow()->driverdata;

        Window drawable = windowdata->xwindow;

        /*
         * This is a workaround for a bug in NVIDIA drivers. Bug has been reported
         * and will be fixed in a future release (probably 319.xx).
         *
         * There's a bug where glXSetSwapIntervalEXT ignores updates because
         * it has the wrong value cached. To work around it, we just run a no-op
         * update to the current value.
         */
        int currentInterval = X11_GL_GetSwapInterval(_this);
        _this->gl_data->glXSwapIntervalEXT(display, drawable, currentInterval);
        _this->gl_data->glXSwapIntervalEXT(display, drawable, interval);

        status = 0;
        swapinterval = interval;
    } else if (_this->gl_data->glXSwapIntervalMESA) {
        status = _this->gl_data->glXSwapIntervalMESA(interval);
        if (status != 0) {
            SDL_SetError("glxSwapIntervalMESA failed");
        } else {
            swapinterval = interval;
        }
    } else if (_this->gl_data->glXSwapIntervalSGI) {
        status = _this->gl_data->glXSwapIntervalSGI(interval);
        if (status != 0) {
            SDL_SetError("glxSwapIntervalSGI failed");
        } else {
            swapinterval = interval;
        }
    } else {
        SDL_Unsupported();
    }
    return status;
}

int
X11_GL_GetSwapInterval(_THIS)
{
    if (_this->gl_data->glXSwapIntervalEXT) {
        Display *display = ((SDL_VideoData *) _this->driverdata)->display;
        const SDL_WindowData *windowdata = (SDL_WindowData *)
            SDL_GL_GetCurrentWindow()->driverdata;
        Window drawable = windowdata->xwindow;
        unsigned int allow_late_swap_tearing = 0;
        unsigned int interval = 0;

        if (_this->gl_data->HAS_GLX_EXT_swap_control_tear) {
            _this->gl_data->glXQueryDrawable(display, drawable,
                                            GLX_LATE_SWAPS_TEAR_EXT,
                                            &allow_late_swap_tearing);
        }

        _this->gl_data->glXQueryDrawable(display, drawable,
                                         GLX_SWAP_INTERVAL_EXT, &interval);

        if ((allow_late_swap_tearing) && (interval > 0)) {
            return -((int) interval);
        }

        return (int) interval;
    } else if (_this->gl_data->glXGetSwapIntervalMESA) {
        return _this->gl_data->glXGetSwapIntervalMESA();
    } else {
        return swapinterval;
    }
}

void
X11_GL_SwapWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    _this->gl_data->glXSwapBuffers(display, data->xwindow);
}

void
X11_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    Display *display = ((SDL_VideoData *) _this->driverdata)->display;
    GLXContext glx_context = (GLXContext) context;

    if (!_this->gl_data) {
        return;
    }
    _this->gl_data->glXDestroyContext(display, glx_context);
    X11_XSync(display, False);
}

#endif /* SDL_VIDEO_OPENGL_GLX */

#endif /* SDL_VIDEO_DRIVER_X11 */

/* vi: set ts=4 sw=4 expandtab: */
