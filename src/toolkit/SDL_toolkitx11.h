/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_X11

#ifndef SDL_toolkitx11_h_
#define SDL_toolkitx11_h_

#include "../../SDL_list.h"
#include "../../video/x11/SDL_x11video.h"
#include "../../video/x11/xsettings-client.h"
#ifdef HAVE_FRIBIDI_H
#include "../../core/unix/SDL_fribidi.h"
#endif
#ifdef HAVE_LIBTHAI_H
#include "../../core/unix/SDL_libthai.h"
#endif
#include "SDL_toolkit.h"

typedef enum SDL_ToolkitVideoDriverX11Flags
{
    SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAGS_NONE = 0,
    SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_CLOSE_DISPLAY = 1 << 0,
    SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED = 1 << 1,
    SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED = 1 << 2,
    SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_DBE_SUPPORTED = 1 << 3,
    SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_RANDR_SUPPORTED = 1 << 4,
} SDL_ToolkitVideoDriverX11Flags;

typedef struct SDL_ToolkitVideoDriverX11
{
    SDL_ToolkitVideoDriver _parent;

    SDL_ToolkitVideoDriverX11Flags flags;

    Display *dpy;
    int screen;
    Window root;
    XContext ctx;

    XSettingsClient *xsettings;
    SDL_ListNode *scale_change_listeners;
} SDL_ToolkitVideoDriverX11;

typedef struct SDL_ToolkitVideoWindowX11
{
    SDL_ToolkitVideoWindow _parent;

    Window wnd;

    Drawable drw;
    XImage *img;
#ifndef NO_SHARED_MEMORY
    XShmSegmentInfo shm;
#endif
    GC blit_gc;
    SDL_Rect scaling_rct;

    XVisualInfo vi;
    Colormap cmap;

    Atom wm_protocols;
    Atom wm_delete_message;
} SDL_ToolkitVideoWindowX11;

typedef struct SDL_ToolkitRenderContextX11
{
    SDL_ToolkitRenderContext _parent;

    GC gc;
} SDL_ToolkitRenderContextX11;

typedef struct SDL_ToolkitTextDriverX11
{
    SDL_ToolkitTextDriver _parent;

#ifdef HAVE_FRIBIDI_H
    SDL_FriBidi *fribidi;
#endif

#ifdef HAVE_LIBTHAI_H
    SDL_LibThai *th;
#endif
} SDL_ToolkitTextDriverX11;

typedef enum SDL_ToolkitThaiEncodingX11
{
    SDL_TOOLKIT_THAI_ENCODING_X11_NONE,
    SDL_TOOLKIT_THAI_ENCODING_X11_TIS,     /* -0 */
    SDL_TOOLKIT_THAI_ENCODING_X11_TIS_WIN, /* -2 */
    SDL_TOOLKIT_THAI_ENCODING_X11_TIS_MAC, /* -1 */
    SDL_TOOLKIT_THAI_ENCODING_X11_8859,
    SDL_TOOLKIT_THAI_ENCODING_X11_UNICODE
} SDL_ToolkitThaiEncodingX11;

typedef enum SDL_ToolkitThaiFontX11
{
    SDL_TOOLKIT_THAI_FONT_X11_OFFSET,
    SDL_TOOLKIT_THAI_FONT_X11_CELL
} SDL_ToolkitThaiFontX11;

typedef struct SDL_ToolkitTextFontX11Unicode
{
    SDL_ToolkitTextFont _parent;

    XFontSet font;
    Bool do_shaping;
    SDL_ToolkitThaiEncodingX11 thai_encoding;
    SDL_ToolkitThaiFontX11 thai_font;
} SDL_ToolkitTextFontX11Unicode;

typedef struct SDL_ToolkitTextFontX11
{
    SDL_ToolkitTextFont _parent;

    XFontStruct *font;
} SDL_ToolkitTextFontX11;

typedef enum SDL_ToolkitTextObjectTypeX11
{
    SDL_TOOLKIT_TEXT_OBJECT_ELEMENT_TYPE_X11_GENERIC,
    SDL_TOOLKIT_TEXT_OBJECT_ELEMENT_TYPE_X11_THAI
} SDL_ToolkitTextObjectElementTypeX11;

typedef struct SDL_ToolkitTextObjectElementX11
{
    SDL_ToolkitTextObjectElementTypeX11 type;
    char *str;
    size_t sz;
    int asc;
    SDL_Rect rct;

    SDL_ListNode *overlays;
} SDL_ToolkitTextObjectElementX11;

typedef struct SDL_ToolkitTextObjectX11Unicode
{
    SDL_ToolkitTextObject _parent;

    SDL_ListNode *elems;
} SDL_ToolkitTextObjectX11Unicode;

typedef struct SDL_ToolkitTextObjectX11
{
    SDL_ToolkitTextObject _parent;

    char *str;
    size_t sz;
    int asc;
} SDL_ToolkitTextObjectX11;

#define SDL_TOOLKIT_DRIVER_TYPE_X11       SDL_FOURCC('X', 'L', 'I', 'B')
#define SDL_TOOLKIT_FONT_TYPE_X11         SDL_FOURCC('X', 'F', 'N', 'T')
#define SDL_TOOLKIT_FONT_TYPE_X11_UNICODE SDL_FOURCC('X', 'U', 'C', 'S')

extern SDL_ToolkitVideoDriver *SDL_ToolkitVideoDriverX11_Create(SDL_VideoDevice *device);
extern SDL_ToolkitRenderDriver *SDL_ToolkitRenderDriverX11_Create(SDL_ToolkitVideoDriver *viddrv);
extern SDL_ToolkitTextDriver *SDL_ToolkitTextDriverX11_Create(SDL_ToolkitVideoDriver *viddrv, SDL_ToolkitRenderDriver *renddrv);

#endif // SDL_toolkitx11_h_
#endif // SDL_VIDEO_DRIVER_X11
