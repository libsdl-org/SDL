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

#ifndef SDL_toolkit_h_
#define SDL_toolkit_h_

#include "../../SDL_list.h"
#include "../../video/SDL_sysvideo.h"

struct SDL_ToolkitVideoEvent;
struct SDL_ToolkitVideoDriver;

typedef enum SDL_ToolkitVideoWindowParentType
{
    SDL_TOOLKIT_VIDEO_WINDOW_PARENT_SDL,
    SDL_TOOLKIT_VIDEO_WINDOW_PARENT_TOOLKIT
} SDL_ToolkitVideoWindowParentType;

typedef struct SDL_ToolkitVideoWindowParent
{
    SDL_ToolkitVideoWindowParentType type;

    union
    {
        SDL_Window *sdl;
        struct SDL_ToolkitVideoWindow *toolkit;
    } parent;
} SDL_ToolkitVideoWindowParent;

typedef enum SDL_ToolkitVideoWindowType
{
    SDL_TOOLKIT_VIDEO_WINDOW_TYPE_DIALOG,
    SDL_TOOLKIT_VIDEO_WINDOW_TYPE_EMBEDDED
} SDL_ToolkitVideoWindowType;

typedef struct SDL_ToolkitVideoScaleChangeListener
{
    void (*listener)(struct SDL_ToolkitVideoDriver *, float, void *);
    void *data;
} SDL_ToolkitVideoScaleChangeListener;

typedef struct SDL_ToolkitVideoDriver
{
    Uint32 type;

    SDL_VideoDevice *parent_device;

    struct SDL_ToolkitVideoWindow *(*CreateWindow)(struct SDL_ToolkitVideoDriver *driver, SDL_ToolkitVideoWindowType type, SDL_ToolkitVideoWindowParent *parent, SDL_Rect *rct, const char *title);
    void (*NextEvent)(struct SDL_ToolkitVideoDriver *driver, struct SDL_ToolkitVideoEvent *e);
    void (*SendEvent)(struct SDL_ToolkitVideoDriver *driver, struct SDL_ToolkitVideoEvent *e);
    void (*AddScaleChangeListener)(struct SDL_ToolkitVideoDriver *driver, SDL_ToolkitVideoScaleChangeListener *listener);
    void (*RemoveScaleChangeListener)(struct SDL_ToolkitVideoDriver *driver, SDL_ToolkitVideoScaleChangeListener *listener);
    void (*Destroy)(struct SDL_ToolkitVideoDriver *drv);
} SDL_ToolkitVideoDriver;

#define SDL_ToolkitVideoDriver_CreateWindow(d, z, p, r, t)     d->CreateWindow(d, z, p, r, t)
#define SDL_ToolkitVideoDriver_NextEvent(x, y)                 x->NextEvent(x, y)
#define SDL_ToolkitVideoDriver_SendEvent(x, y)                 x->SendEvent(x, y)
#define SDL_ToolkitVideoDriver_AddScaleChangeListener(x, y)    x->AddScaleChangeListener(x, y)
#define SDL_ToolkitVideoDriver_RemoveScaleChangeListener(x, y) x->RemoveScaleChangeListener(x, y)
#define SDL_ToolkitVideoDriver_Destroy(x)                      x->Destroy(x)

typedef enum SDL_ToolkitVideoWindowBufferType
{
    SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NONE,
    SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SINGLE,
    SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE,
    SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING,
    SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE,
} SDL_ToolkitVideoWindowBufferType;

typedef struct SDL_ToolkitColor
{
    SDL_Color rgba;
    unsigned long pixel; /* Unsigned long instead of Uint32 like SDL_MapRGB due to Xlib */
} SDL_ToolkitColor;

#define SDL_TOOLKIT_VARIADIC_PARAM_NONE -1

typedef struct SDL_ToolkitVideoWindow
{
    SDL_ToolkitVideoDriver *driver;

    SDL_ToolkitVideoWindowParent *parent;
    SDL_ToolkitVideoWindowType type;
    SDL_Rect rct;

    SDL_ToolkitVideoWindowBufferType buffer_type;
    SDL_Surface *surface;
    unsigned int buffer_w;
    unsigned int buffer_h;

    float scale;

    void *udata;

    bool (*CreateBuffer)(struct SDL_ToolkitVideoWindow *window, SDL_ToolkitVideoWindowBufferType type, ...); /* Variadic paramters are for scaling buffers only, width followed by height as unsigned int. Probably should not be called by anyone other than SDL_ToolkitRenderDriver->CreateContext unless you need a scaling buffer... */
    void (*DestroyBuffer)(struct SDL_ToolkitVideoWindow *window);

    bool (*MapColor)(struct SDL_ToolkitVideoWindow *window, SDL_ToolkitColor *color);
    void (*BeginDraw)(struct SDL_ToolkitVideoWindow *window);
    void (*EndDraw)(struct SDL_ToolkitVideoWindow *window); /* Swaps buffers if natively double buffered, blits surface to window for scaling/surface buffers */

    bool (*UpdateRect)(struct SDL_ToolkitVideoWindow *window, SDL_Rect *rct, ...); /* If this returns true, you have to update the destination of your drawing context due to buffer changes, variadic paramters are for scaling buffers only.  */
    bool (*UpdateScale)(struct SDL_ToolkitVideoWindow *window, float scale);       /* If this returns true, you need to recreate your buffers, resources, etc */
    void (*GetParentRect)(struct SDL_ToolkitVideoWindow *window, SDL_Rect *rct);

    void (*Show)(struct SDL_ToolkitVideoWindow *window);
    void (*Hide)(struct SDL_ToolkitVideoWindow *window);

    void (*Destroy)(struct SDL_ToolkitVideoWindow *window);
} SDL_ToolkitVideoWindow;

#define SDL_ToolkitVideoWindow_CreateBuffer(x, y, ...) x->CreateBuffer(x, y, __VA_ARGS__)
#define SDL_ToolkitVideoWindow_DestroyBuffer(x)        x->DestroyBuffer(x)
#define SDL_ToolkitVideoWindow_MapColor(x, y)          x->MapColor(x, y)
#define SDL_ToolkitVideoWindow_BeginDraw(x)            x->BeginDraw(x)
#define SDL_ToolkitVideoWindow_EndDraw(x)              x->EndDraw(x)
#define SDL_ToolkitVideoWindow_UpdateRect(x, y, ...)   x->UpdateRect(x, y, __VA_ARGS__)
#define SDL_ToolkitVideoWindow_UpdateScale(x, y)       x->UpdateScale(x, y)
#define SDL_ToolkitVideoWindow_GetParentRect(x, y)     x->GetParentRect(x, y)
#define SDL_ToolkitVideoWindow_Show(x)                 x->Show(x)
#define SDL_ToolkitVideoWindow_Hide(x)                 x->Hide(x)
#define SDL_ToolkitVideoWindow_IsScaleFractional(x)    SDL_roundf(x->scale) != x->scale
#define SDL_ToolkitVideoWindow_Destroy(x)              x->Destroy(x)
extern void SDL_ToolkitVideoWindow_CenterInParent(SDL_ToolkitVideoWindow *window);

typedef enum SDL_ToolkitVideoEventType
{
    SDL_TOOLKIT_VIDEO_EVENT_NONE,
    SDL_TOOLKIT_VIDEO_EVENT_EXPOSE,
    SDL_TOOLKIT_VIDEO_EVENT_CLOSE_REQUESTED,
    SDL_TOOLKIT_VIDEO_EVENT_FOCUS_IN,
    SDL_TOOLKIT_VIDEO_EVENT_FOCUS_OUT,
    SDL_TOOLKIT_VIDEO_EVENT_MOTION,
    SDL_TOOLKIT_VIDEO_EVENT_BUTTON_PRESS,
    SDL_TOOLKIT_VIDEO_EVENT_BUTTON_RELEASE,
    SDL_TOOLKIT_VIDEO_EVENT_KEY_PRESS,
    SDL_TOOLKIT_VIDEO_EVENT_KEY_RELEASE
} SDL_ToolkitVideoEventType;

typedef enum SDL_ToolkitVideoEventButton
{
    SDL_TOOLKIT_VIDEO_BUTTON_UNKNOWN,
    SDL_TOOLKIT_VIDEO_BUTTON_ONE,
    SDL_TOOLKIT_VIDEO_BUTTON_TWO,
    SDL_TOOLKIT_VIDEO_BUTTON_THREE,
    SDL_TOOLKIT_VIDEO_BUTTON_FOUR,
    SDL_TOOLKIT_VIDEO_BUTTON_FIVE
} SDL_ToolkitVideoEventButton;

typedef enum SDL_ToolkitVideoEventKey
{
    SDL_TOOLKIT_VIDEO_KEY_UNKNOWN,
    SDL_TOOLKIT_VIDEO_KEY_ESCAPE,
    SDL_TOOLKIT_VIDEO_KEY_RETURN,
    SDL_TOOLKIT_VIDEO_KEY_NUMERIC_ENTER,
    SDL_TOOLKIT_VIDEO_KEY_TAB,
    SDL_TOOLKIT_VIDEO_KEY_LEFT,
    SDL_TOOLKIT_VIDEO_KEY_RIGHT
} SDL_ToolkitVideoEventKey;

typedef struct SDL_ToolkitVideoEvent
{
    SDL_ToolkitVideoEventType type;
    SDL_ToolkitVideoWindow *wnd;

    union
    {
        SDL_Point motion;

        struct SDL_ToolkitVideoEventButtonStruct
        {
            SDL_Point coords;
            SDL_ToolkitVideoEventButton button;
        } button;

        SDL_ToolkitVideoEventKey key;
    } data;
} SDL_ToolkitVideoEvent;

typedef struct SDL_ToolkitRenderDriver
{
    SDL_ToolkitVideoDriver *viddrv;
    Uint32 type;

    struct SDL_ToolkitRenderContext *(*CreateContext)(struct SDL_ToolkitRenderDriver *drv, SDL_ToolkitVideoWindow *wnd);
    void (*Destroy)(struct SDL_ToolkitRenderDriver *drv);
} SDL_ToolkitRenderDriver;

#define SDL_ToolkitRenderDriver_CreateContext(x, y) x->CreateContext(x, y);
#define SDL_ToolkitRenderDriver_Destroy(x)          x->Destroy(x);

typedef struct SDL_ToolkitRenderContext
{
    SDL_ToolkitRenderDriver *drv;
    SDL_ToolkitVideoWindow *wnd;

    void (*WindowBufferUpdated)(struct SDL_ToolkitRenderContext *context);
    void (*SetColor)(struct SDL_ToolkitRenderContext *context, SDL_ToolkitColor *color);
    void (*FillRect)(struct SDL_ToolkitRenderContext *context, SDL_Rect *rct);
    void (*FillCircle)(struct SDL_ToolkitRenderContext *context, SDL_Rect *rct);
    void (*Destroy)(struct SDL_ToolkitRenderContext *context);
} SDL_ToolkitRenderContext;

#define SDL_ToolkitRenderContext_WindowBufferUpdated(x) x->WindowBufferUpdated(x);
#define SDL_ToolkitRenderContext_SetColor(x, y)         x->SetColor(x, y);
#define SDL_ToolkitRenderContext_FillRect(x, y)         x->FillRect(x, y);
#define SDL_ToolkitRenderContext_FillCircle(x, y)       x->FillCircle(x, y);
#define SDL_ToolkitRenderContext_Destroy(x)             x->Destroy(x);

struct SDL_ToolkitTextFont;

typedef struct SDL_ToolkitTextDriver
{
    SDL_ToolkitVideoDriver *viddrv;
    SDL_ToolkitRenderDriver *renddrv;
    Uint32 type;

    struct SDL_ToolkitTextFont *(*CreateFont)(struct SDL_ToolkitTextDriver *drv, unsigned int sz, bool icon);

    /* Complex text layout */
    struct SDL_ToolkitTextObject *(*CreateObject)(struct SDL_ToolkitTextDriver *drv, struct SDL_ToolkitTextFont *fnt, char *txt, size_t sz);

    void (*Destroy)(struct SDL_ToolkitTextDriver *drv);
} SDL_ToolkitTextDriver;

#define SDL_ToolkitTextDriver_CreateFont(x, y, z)      x->CreateFont(x, y, z);
#define SDL_ToolkitTextDriver_CreateObject(a, b, c, d) a->CreateObject(a, b, c, d);
#define SDL_ToolkitTextDriver_Destroy(x)               x->Destroy(x);

typedef struct SDL_ToolkitTextFont
{
    SDL_ToolkitTextDriver *drv;

    Uint32 type;
    int height;

    void (*Destroy)(struct SDL_ToolkitTextFont *fnt);
} SDL_ToolkitTextFont;

#define SDL_ToolkitTextFont_Destroy(x) x->Destroy(x);

typedef enum
{
    SDL_TOOLKIT_TEXT_DIRECTION_LTR,
    SDL_TOOLKIT_TEXT_DIRECTION_RTL,
    SDL_TOOLKIT_TEXT_DIRECTION_NEUTRAL
} SDL_ToolkitTextDirection;

typedef struct SDL_ToolkitTextObject
{
    SDL_ToolkitTextDriver *drv;
    SDL_ToolkitTextFont *fnt;

    SDL_ToolkitTextDirection dir;
    unsigned int w;
    unsigned int h;

    void (*Draw)(struct SDL_ToolkitTextObject *obj, SDL_ToolkitRenderContext *ctx, int x, int y);
    void (*Destroy)(struct SDL_ToolkitTextObject *obj);
} SDL_ToolkitTextObject;

#define SDL_ToolkitTextObject_Draw(a, b, c, d) a->Draw(a, b, c, d);
#define SDL_ToolkitTextObject_Destroy(x)       x->Destroy(x);

typedef struct SDL_ToolkitDriverSet
{
    SDL_ToolkitVideoDriver *video;
    SDL_ToolkitRenderDriver *render;
    SDL_ToolkitTextDriver *text;

    unsigned int window_count;
} SDL_ToolkitDriverSet;

extern SDL_ToolkitDriverSet *SDL_ToolkitDriverSet_Create(SDL_VideoDevice *parent_device);
extern bool SDL_ToolkitDriverSet_IsValid(SDL_ToolkitDriverSet *set);
extern void SDL_ToolkitDriverSet_EventLoop(SDL_ToolkitDriverSet *set);
extern void SDL_ToolkitDriverSet_Destroy(SDL_ToolkitDriverSet *set);

typedef struct SDL_ToolkitColorScheme
{
    SDL_ToolkitColor bg;
    SDL_ToolkitColor text;
    SDL_ToolkitColor button_border;
    SDL_ToolkitColor button_bg;
    SDL_ToolkitColor button_selected;

    /* Generated shades */
    SDL_ToolkitColor bevel_l1;
    SDL_ToolkitColor bevel_l2;
    SDL_ToolkitColor bevel_d;
    SDL_ToolkitColor pressed;
    SDL_ToolkitColor disabled_text;
} SDL_ToolkitColorScheme;

extern void SDL_ToolkitColorScheme_SetSystemColors(SDL_ToolkitColorScheme *scheme);
extern void SDL_ToolkitColorScheme_GenerateShades(SDL_ToolkitColorScheme *scheme);

typedef enum SDL_ToolkitWindowFlags
{
    SDL_TOOLKIT_WINDOW_FLAGS_NONE = 0,
    SDL_TOOLKIT_WINDOW_FLAGS_FOCUSED = 1 << 0,
    SDL_TOOLKIT_WINDOW_FLAGS_ACTIVE = 1 << 1,
    SDL_TOOLKIT_WINDOW_FLAGS_RTL = 1 << 2,
} SDL_ToolkitWindowFlags;

typedef struct SDL_ToolkitWindow
{
    SDL_ToolkitWindowFlags flags;

    SDL_ToolkitDriverSet *set;
    SDL_ToolkitTextFont *fnt;
    SDL_ToolkitColorScheme colors;

    SDL_ToolkitVideoWindow *vid_wnd;
    SDL_ToolkitRenderContext *ctx;

    unsigned int int_scale;
    void (*scale_change_cb)(struct SDL_ToolkitWindow *, void *);
    void *scale_change_cbdata;
    SDL_ToolkitVideoScaleChangeListener drv_scale_listener;

    SDL_ListNode *controls;
    SDL_ListNode *interactive_controls;
    struct SDL_ToolkitControl *selected_control;
    struct SDL_ToolkitControl *selected_control_before_focus_changed;
    struct SDL_ToolkitControl *interacted_control;
    SDL_ToolkitVideoEventKey last_key;
} SDL_ToolkitWindow;

extern SDL_ToolkitWindow *SDL_ToolkitWindow_Create(SDL_ToolkitDriverSet *set, SDL_ToolkitColorScheme *scheme, SDL_ToolkitVideoWindowParent *parent, SDL_ToolkitVideoWindowType type, unsigned int w, unsigned int h, const char *title);
extern void SDL_ToolkitWindow_Realize(SDL_ToolkitWindow *wnd);
#define SDL_ToolkitWindow_Show(x)                 SDL_ToolkitVideoWindow_Show(x->vid_wnd);
#define SDL_ToolkitWindow_CenterInParent(x)       SDL_ToolkitVideoWindow_CenterInParent(x->vid_wnd);
#define SDL_ToolkitWindow_UpdateRect(x, y, ...)   SDL_ToolkitVideoWindow_UpdateRect(x->vid_wnd, y, __VA_ARGS__);
#define SDL_ToolkitWindow_ScaleValue(w, x)        SDL_lroundf((x / w->vid_wnd->scale) * w->int_scale)
#define SDL_ToolkitWindow_ReverseScaleValue(w, x) SDL_lroundf((x / w->int_scale) * w->vid_wnd->scale)
extern void SDL_ToolkitWindow_Close(SDL_ToolkitWindow *wnd);
extern void SDL_ToolkitWindow_Destroy(SDL_ToolkitWindow *wnd);

#define SDL_TOOLKIT_PADDING_1 4
#define SDL_TOOLKIT_PADDING_2 12
#define SDL_TOOLKIT_PADDING_3 8
#define SDL_TOOLKIT_PADDING_4 16
#define SDL_TOOLKIT_PADDING_5 3
#define SDL_TOOLKIT_PADDING_6 6

typedef enum SDL_ToolkitControlState
{
    SDL_TOOLKIT_CONTROL_STATE_NORMAL,
    SDL_TOOLKIT_CONTROL_STATE_HOVER,
    SDL_TOOLKIT_CONTROL_STATE_PRESSED,      /* Key/Button Up */
    SDL_TOOLKIT_CONTROL_STATE_PRESSED_HELD, /* Key/Button Down */
    SDL_TOOLKIT_CONTROL_STATE_DISABLED
} SDL_ToolkitControlState;

typedef enum SDL_ToolkitControlFlags
{
    SDL_TOOLKIT_CONTROL_FLAGS_NONE = 0,
    SDL_TOOLKIT_CONTROL_FLAGS_DYNAMIC = 1 << 0,
    SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ENTER = 1 << 1,
    SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ESCAPE = 1 << 2,
    SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE = 1 << 3,
    SDL_TOOLKIT_CONTROL_FLAGS_RESIZABLE = 1 << 4
} SDL_ToolkitControlFlags;

typedef struct SDL_ToolkitControl
{
    SDL_ToolkitWindow *wnd;
    SDL_ToolkitControlState state;
    SDL_ToolkitControlFlags flags;
    SDL_Rect rct;

    void *udata;
    void (*udata_free)(struct SDL_ToolkitControl *);

    void (*Draw)(struct SDL_ToolkitControl *);
    void (*Calculate)(struct SDL_ToolkitControl *);
    void (*InitResources)(struct SDL_ToolkitControl *);
    void (*StateUpdated)(struct SDL_ToolkitControl *);
    void (*FlagsUpdated)(struct SDL_ToolkitControl *);
    void (*Destroy)(struct SDL_ToolkitControl *);
} SDL_ToolkitControl;

#define SDL_ToolkitControl_Draw(x)          x->Draw(x);
#define SDL_ToolkitControl_Calculate(x)     x->Calculate(x);
#define SDL_ToolkitControl_InitResources(x) x->InitResources(x);
#define SDL_ToolkitControl_StateUpdated(x)  x->StateUpdated(x);
#define SDL_ToolkitControl_FlagsUpdated(x)  x->FlagsUpdated(x);
extern void SDL_ToolkitControl_Destroy(SDL_ToolkitControl *base);

typedef enum SDL_ToolkitIcon
{
    SDL_TOOLKIT_ICON_ERROR,
    SDL_TOOLKIT_ICON_WARNING,
    SDL_TOOLKIT_ICON_INFORMATION
} SDL_ToolkitIcon;

typedef struct SDL_ToolkitIconControl
{
    SDL_ToolkitControl _parent;

    SDL_ToolkitIcon icon;

    SDL_ToolkitColor black;
    SDL_ToolkitColor red;
    SDL_ToolkitColor dark_red;
    SDL_ToolkitColor white;
    SDL_ToolkitColor yellow;
    SDL_ToolkitColor blue;
    SDL_ToolkitColor shadow;

    SDL_ToolkitTextFont *font;
    SDL_ToolkitTextObject *object;

    unsigned int disc_w;
    unsigned int disc_h;
    SDL_Point object_pos;
    SDL_Point shadow_pos;
} SDL_ToolkitIconControl;

extern SDL_ToolkitControl *SDL_ToolkitIconControl_Create(SDL_ToolkitWindow *wnd, SDL_ToolkitIcon icon);

typedef struct SDL_ToolkitLabelControlLine
{
    SDL_ToolkitTextObject *object;
    SDL_Point pos;
} SDL_ToolkitLabelControlLine;

typedef struct SDL_ToolkitLabelControl
{
    SDL_ToolkitControl _parent;

    const char *utf8;

    SDL_ToolkitLabelControlLine *lines;
    size_t lines_sz;
} SDL_ToolkitLabelControl;

extern SDL_ToolkitControl *SDL_ToolkitLabelControl_Create(SDL_ToolkitWindow *wnd, const char *utf8);
extern unsigned int SDL_ToolkitLabelControl_GetHeightOfFirstLine(SDL_ToolkitControl *control);

typedef struct SDL_ToolkitButtonControl
{
    SDL_ToolkitControl _parent;

    const char *text;

    SDL_ToolkitTextObject *obj;
    SDL_Point obj_pos;

    void (*cb)(struct SDL_ToolkitControl *, void *);
    void *cbdata;
} SDL_ToolkitButtonControl;

extern SDL_ToolkitControl *SDL_ToolkitButtonControl_Create(SDL_ToolkitWindow *wnd, const char *text, void (*cb)(struct SDL_ToolkitControl *, void *), void *cbdata);

#endif // SDL_x11toolkit_h_
