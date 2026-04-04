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
#include "SDL_toolkit.h"
#include "SDL_toolkitx11.h"
#ifdef SDL_USE_LIBDBUS
#include "../../core/linux/SDL_system_theme.h"
#endif

void SDL_ToolkitVideoWindow_CenterInParent(SDL_ToolkitVideoWindow *window)
{
    SDL_Rect parent_rct;
    SDL_Rect new_rct;

    SDL_ToolkitVideoWindow_GetParentRect(window, &parent_rct);
    new_rct.w = window->rct.w;
    new_rct.h = window->rct.h;
    new_rct.x = parent_rct.x + (parent_rct.w - new_rct.w) / 2;
    new_rct.y = parent_rct.y + (parent_rct.h - new_rct.h) / 2;
    SDL_ToolkitVideoWindow_UpdateRect(window, &new_rct, SDL_TOOLKIT_VARIADIC_PARAM_NONE);
}

SDL_ToolkitDriverSet *SDL_ToolkitDriverSet_Create(SDL_VideoDevice *parent_device)
{
    SDL_ToolkitDriverSet *set;

    set = SDL_malloc(sizeof(SDL_ToolkitDriverSet));
    if (!set) {
        return NULL;
    }

    set->video = NULL;
    set->render = NULL;
    set->text = NULL;

#ifdef SDL_VIDEO_DRIVER_X11
    set->video = SDL_ToolkitVideoDriverX11_Create(parent_device);
    set->render = SDL_ToolkitRenderDriverX11_Create(set->video);
    set->text = SDL_ToolkitTextDriverX11_Create(set->video, set->render);
#endif

    set->window_count = 0;

    return set;
}

bool SDL_ToolkitDriverSet_IsValid(SDL_ToolkitDriverSet *set)
{
    if (set->video && set->render && set->text) {
        return true;
    } else {
        return false;
    }
}

void SDL_ToolkitDriverSet_Destroy(SDL_ToolkitDriverSet *set)
{
    if (set->text) {
        SDL_ToolkitTextDriver_Destroy(set->text);
    }

    if (set->render) {
        SDL_ToolkitRenderDriver_Destroy(set->render);
    }

    if (set->video) {
        SDL_ToolkitVideoDriver_Destroy(set->video);
    }

    SDL_free(set);
}

static void SetDefaultColors(SDL_ToolkitColorScheme *theme)
{
    theme->bg.rgba.r = 191;
    theme->bg.rgba.g = 184;
    theme->bg.rgba.b = 191;

    theme->text.rgba.r = 0;
    theme->text.rgba.g = 0;
    theme->text.rgba.b = 0;

    theme->button_border.rgba.r = 127;
    theme->button_border.rgba.g = 120;
    theme->button_border.rgba.b = 127;

    theme->button_bg.rgba.r = 191;
    theme->button_bg.rgba.g = 184;
    theme->button_bg.rgba.b = 191;

    theme->button_selected.rgba.r = 235;
    theme->button_selected.rgba.g = 235;
    theme->button_selected.rgba.b = 235;
}

static void SetDarkColors(SDL_ToolkitColorScheme *theme)
{
    theme->bg.rgba.r = 30;
    theme->bg.rgba.g = 30;
    theme->bg.rgba.b = 30;

    theme->text.rgba.r = 192;
    theme->text.rgba.g = 192;
    theme->text.rgba.b = 192;

    theme->button_border.rgba.r = 32;
    theme->button_border.rgba.g = 32;
    theme->button_border.rgba.b = 32;

    theme->button_bg.rgba.r = 30;
    theme->button_bg.rgba.g = 30;
    theme->button_bg.rgba.b = 30;

    theme->button_selected.rgba.r = 47;
    theme->button_selected.rgba.g = 47;
    theme->button_selected.rgba.b = 47;
}

#if 0
static void SetAccessibleLightColors(SDL_ToolkitColorScheme *theme) {
	theme->bg.rgba.r = 255;
	theme->bg.rgba.g = 255;
	theme->bg.rgba.b = 255;

	theme->text.rgba.r = 0;
	theme->text.rgba.g = 0;
	theme->text.rgba.b = 0;

	theme->button_border.rgba.r = 0;
	theme->button_border.rgba.g = 0;
	theme->button_border.rgba.b = 0;

	theme->button_bg.rgba.r = 255;
	theme->button_bg.rgba.g = 255;
	theme->button_bg.rgba.b = 255;

	theme->button_selected.rgba.r = 20;
	theme->button_selected.rgba.g = 230;
	theme->button_selected.rgba.b = 255;
}

static void SetAccessibleDarkColors(SDL_ToolkitColorScheme *theme) {
	theme->bg.rgba.r = 0;
	theme->bg.rgba.g = 0;
	theme->bg.rgba.b = 0;

	theme->text.rgba.r = 255;
	theme->text.rgba.g = 255;
	theme->text.rgba.b = 255;

	theme->button_border.rgba.r = 20;
	theme->button_border.rgba.g = 235;
	theme->button_border.rgba.b = 255;

	theme->button_bg.rgba.r = 0;
	theme->button_bg.rgba.g = 0;
	theme->button_bg.rgba.b = 0;

	theme->button_selected.rgba.r = 125;
	theme->button_selected.rgba.g = 5;
	theme->button_selected.rgba.b = 125;
}
#endif

void SDL_ToolkitColorScheme_SetSystemColors(SDL_ToolkitColorScheme *theme)
{
#ifdef SDL_USE_LIBDBUS
    SDL_SystemTheme systheme;

    systheme = SDL_SYSTEM_THEME_LIGHT;
    if (SDL_SystemTheme_Init()) {
        systheme = SDL_SystemTheme_Get();
    }

    switch (systheme) {
    case SDL_SYSTEM_THEME_DARK:
        SetDarkColors(theme);
        break;
    default:
        SetDefaultColors(theme);
        break;
    }
#else
    SetDefaultColors(theme);
#endif
}

void SDL_ToolkitColorScheme_GenerateShades(SDL_ToolkitColorScheme *theme)
{
    theme->bevel_l1.rgba.r = SDL_clamp(theme->button_border.rgba.r + 48, 0, 255);
    theme->bevel_l1.rgba.g = SDL_clamp(theme->button_border.rgba.g + 48, 0, 255);
    theme->bevel_l1.rgba.b = SDL_clamp(theme->button_border.rgba.b + 48, 0, 255);

    theme->bevel_l2.rgba.r = SDL_clamp(theme->button_border.rgba.r + 126, 0, 255);
    theme->bevel_l2.rgba.g = SDL_clamp(theme->button_border.rgba.g + 126, 0, 255);
    theme->bevel_l2.rgba.b = SDL_clamp(theme->button_border.rgba.b + 126, 0, 255);

    theme->bevel_d.rgba.r = SDL_clamp(theme->button_border.rgba.r - 87, 0, 255);
    theme->bevel_d.rgba.g = SDL_clamp(theme->button_border.rgba.g - 87, 0, 255);
    theme->bevel_d.rgba.b = SDL_clamp(theme->button_border.rgba.b - 87, 0, 255);

    theme->pressed.rgba.r = SDL_clamp(theme->button_bg.rgba.r - 48, 0, 255);
    theme->pressed.rgba.g = SDL_clamp(theme->button_bg.rgba.g - 48, 0, 255);
    theme->pressed.rgba.b = SDL_clamp(theme->button_bg.rgba.b - 48, 0, 255);

    theme->disabled_text.rgba.r = SDL_clamp(theme->text.rgba.r + 75, 0, 255);
    theme->disabled_text.rgba.g = SDL_clamp(theme->text.rgba.g + 75, 0, 255);
    theme->disabled_text.rgba.b = SDL_clamp(theme->text.rgba.b + 75, 0, 255);
}

static bool IsLocaleRtl(void)
{
    SDL_Locale **current_locales;
    static const SDL_Locale rtl_locales[] = {
        {
            "ar",
            NULL,
        },
        {
            "fa",
            "AF",
        },
        {
            "fa",
            "IR",
        },
        {
            "he",
            NULL,
        },
        {
            "iw",
            NULL,
        },
        {
            "yi",
            NULL,
        },
        {
            "ur",
            NULL,
        },
        {
            "ug",
            NULL,
        },
        {
            "kd",
            NULL,
        },
        {
            "pk",
            "PK",
        },
        {
            "ps",
            NULL,
        }
    };
    int current_locales_sz;
    int i;

    current_locales = SDL_GetPreferredLocales(&current_locales_sz);
    if (current_locales_sz <= 0) {
        return false;
    }
    for (i = 0; i < SDL_arraysize(rtl_locales); ++i) {
        if (SDL_startswith(current_locales[0]->language, rtl_locales[i].language)) {
            if (!rtl_locales[i].country) {
                return true;
            } else {
                return SDL_startswith(current_locales[0]->country, rtl_locales[i].country);
            }
        }
    }

    return false;
}

static void ScaleChangeListener(SDL_ToolkitVideoDriver *drv, float scale, void *udata)
{
    SDL_ToolkitWindow *wnd;

    wnd = udata;

    if (SDL_ToolkitVideoWindow_UpdateScale(wnd->vid_wnd, scale)) {
        wnd->int_scale = (unsigned int)SDL_ceilf(wnd->vid_wnd->scale);
        SDL_ToolkitTextFont_Destroy(wnd->fnt);
        wnd->fnt = SDL_ToolkitTextDriver_CreateFont(wnd->set->text, 14 * wnd->int_scale, false);
        if (wnd->scale_change_cb) {
            wnd->scale_change_cb(wnd, wnd->scale_change_cbdata);
        }
    }
}

SDL_ToolkitWindow *SDL_ToolkitWindow_Create(SDL_ToolkitDriverSet *set, SDL_ToolkitColorScheme *scheme, SDL_ToolkitVideoWindowParent *parent, SDL_ToolkitVideoWindowType type, unsigned int w, unsigned int h, const char *title)
{
    SDL_ToolkitWindow *wnd;
    SDL_Rect rct;

    wnd = SDL_malloc(sizeof(SDL_ToolkitWindow));
    if (!wnd) {
        return NULL;
    }

    wnd->flags = SDL_TOOLKIT_WINDOW_FLAGS_ACTIVE;
    if (IsLocaleRtl()) {
        wnd->flags |= SDL_TOOLKIT_WINDOW_FLAGS_RTL;
    }

    rct.x = rct.y = 0;
    rct.w = w;
    rct.h = h;
    wnd->set = set;
    wnd->colors = *scheme;

    wnd->vid_wnd = SDL_ToolkitVideoDriver_CreateWindow(set->video, type, parent, &rct, title);
    wnd->vid_wnd->udata = wnd;

    wnd->int_scale = (unsigned int)SDL_ceilf(wnd->vid_wnd->scale);
    wnd->scale_change_cb = NULL;
    wnd->scale_change_cbdata = NULL;
    wnd->drv_scale_listener.listener = ScaleChangeListener;
    wnd->drv_scale_listener.data = wnd;
    SDL_ToolkitVideoDriver_AddScaleChangeListener(wnd->set->video, &wnd->drv_scale_listener);

    wnd->fnt = SDL_ToolkitTextDriver_CreateFont(wnd->set->text, 14 * wnd->int_scale, false);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.bg);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.text);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.button_border);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.button_bg);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.button_selected);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.bevel_l1);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.bevel_l2);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.bevel_d);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.disabled_text);
    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &wnd->colors.pressed);

    wnd->controls = NULL;
    wnd->interactive_controls = NULL;
    wnd->interacted_control = NULL;
    wnd->selected_control = NULL;
    wnd->selected_control_before_focus_changed = NULL;

    set->window_count++;
    return wnd;
}

void SDL_ToolkitWindow_Realize(SDL_ToolkitWindow *wnd)
{
    if (SDL_ToolkitVideoWindow_IsScaleFractional(wnd->vid_wnd)) {
        SDL_ToolkitVideoWindow_CreateBuffer(wnd->vid_wnd, SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING, SDL_ToolkitWindow_ScaleValue(wnd, wnd->vid_wnd->rct.w), SDL_ToolkitWindow_ScaleValue(wnd, wnd->vid_wnd->rct.h));
    }

    wnd->ctx = SDL_ToolkitRenderDriver_CreateContext(wnd->set->render, wnd->vid_wnd);
}

void SDL_ToolkitWindow_Destroy(SDL_ToolkitWindow *wnd)
{
    SDL_ToolkitVideoDriver_RemoveScaleChangeListener(wnd->set->video, &wnd->drv_scale_listener);

    SDL_ListClear(&wnd->controls);
    SDL_ListClear(&wnd->interactive_controls);

    SDL_ToolkitTextFont_Destroy(wnd->fnt);
    SDL_ToolkitRenderContext_Destroy(wnd->ctx);
    SDL_ToolkitVideoWindow_Destroy(wnd->vid_wnd);
}

static SDL_ToolkitControl *GetControlUnderCursor(SDL_ToolkitWindow *wnd, SDL_Point *pos)
{
    SDL_ListNode *cursor;

    pos->x = SDL_ToolkitWindow_ScaleValue(wnd, pos->x);
    pos->y = SDL_ToolkitWindow_ScaleValue(wnd, pos->y);

    cursor = wnd->interactive_controls;
    while (cursor) {
        SDL_ToolkitControl *control;

        control = cursor->entry;
        if (SDL_PointInRect(pos, &control->rct)) {
            return control;
        }
        cursor = cursor->next;
    }

    return NULL;
}

static void ProcessWindowEvent(SDL_ToolkitWindow *wnd, SDL_ToolkitVideoEvent *e)
{
    SDL_ListNode *cursor;
    SDL_ToolkitControl *prev_control;
    bool draw;

    draw = false;
    switch (e->type) {
    case SDL_TOOLKIT_VIDEO_EVENT_CLOSE_REQUESTED:
        wnd->flags &= ~SDL_TOOLKIT_WINDOW_FLAGS_ACTIVE;
        SDL_ToolkitVideoWindow_Hide(wnd->vid_wnd);
        wnd->set->window_count--;
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_EXPOSE:
        draw = true;
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_FOCUS_IN:
        wnd->flags |= SDL_TOOLKIT_WINDOW_FLAGS_FOCUSED;
        wnd->selected_control = wnd->selected_control_before_focus_changed;
        draw = true;
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_FOCUS_OUT:
        wnd->flags &= ~SDL_TOOLKIT_WINDOW_FLAGS_FOCUSED;
        wnd->selected_control_before_focus_changed = wnd->selected_control;
        wnd->selected_control = NULL;
        cursor = wnd->controls;
        while (cursor) {
            SDL_ToolkitControl *control;

            control = cursor->entry;
            control->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
            cursor = cursor->next;
        }
        draw = true;
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_MOTION:
        if (wnd->flags & SDL_TOOLKIT_WINDOW_FLAGS_FOCUSED) {
            prev_control = wnd->interacted_control;

            wnd->interacted_control = GetControlUnderCursor(wnd, &e->data.motion);
            if (prev_control) {
                prev_control->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
                SDL_ToolkitControl_StateUpdated(prev_control);
                draw = true;
            }

            if (wnd->interacted_control) {
                if (wnd->interacted_control->flags & SDL_TOOLKIT_CONTROL_FLAGS_DYNAMIC) {
                    wnd->interacted_control->state = SDL_TOOLKIT_CONTROL_STATE_HOVER;
                    SDL_ToolkitControl_StateUpdated(wnd->interacted_control);
                    draw = true;
                } else {
                    wnd->interacted_control = NULL;
                }
            }
        }
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_BUTTON_PRESS:
        prev_control = wnd->interacted_control;
        if (prev_control) {
            prev_control->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
            SDL_ToolkitControl_StateUpdated(prev_control);
            draw = true;
        }

        if (e->data.button.button == SDL_TOOLKIT_VIDEO_BUTTON_ONE) {
            wnd->interacted_control = GetControlUnderCursor(wnd, &e->data.button.coords);
            if (wnd->interacted_control) {
                wnd->interacted_control->state = SDL_TOOLKIT_CONTROL_STATE_PRESSED_HELD;
                SDL_ToolkitControl_StateUpdated(wnd->interacted_control);
                draw = true;
            }
        }
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_BUTTON_RELEASE:
        if (e->data.button.button == SDL_TOOLKIT_VIDEO_BUTTON_ONE && wnd->interacted_control) {
            SDL_ToolkitControl *control;

            control = GetControlUnderCursor(wnd, &e->data.button.coords);
            if (wnd->interacted_control == control) {
                wnd->interacted_control->state = SDL_TOOLKIT_CONTROL_STATE_PRESSED;
                SDL_ToolkitControl_StateUpdated(wnd->interacted_control);
                wnd->interacted_control->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
                draw = true;
            }
        }
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_KEY_PRESS:
        wnd->last_key = e->data.key;

        if (wnd->last_key == SDL_TOOLKIT_VIDEO_KEY_ESCAPE) {
            cursor = wnd->interactive_controls;
            while (cursor) {
                SDL_ToolkitControl *control;

                control = cursor->entry;
                if (control->flags & SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ESCAPE) {
                    control->state = SDL_TOOLKIT_CONTROL_STATE_PRESSED_HELD;
                    SDL_ToolkitControl_StateUpdated(control);
                    draw = true;
                }
                cursor = cursor->next;
            }
        } else if ((wnd->last_key == SDL_TOOLKIT_VIDEO_KEY_RETURN) || (wnd->last_key == SDL_TOOLKIT_VIDEO_KEY_NUMERIC_ENTER)) {
            if (wnd->selected_control) {
                wnd->selected_control->state = SDL_TOOLKIT_CONTROL_STATE_PRESSED_HELD;
                SDL_ToolkitControl_StateUpdated(wnd->selected_control);
                draw = true;
            }
        }
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_KEY_RELEASE:
        if (e->data.key != wnd->last_key) {
            break;
        }

        if (e->data.key == SDL_TOOLKIT_VIDEO_KEY_ESCAPE) {
            cursor = wnd->interactive_controls;
            while (cursor) {
                SDL_ToolkitControl *control;

                control = cursor->entry;
                if (control->flags & SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ESCAPE) {
                    control->state = SDL_TOOLKIT_CONTROL_STATE_PRESSED;
                    SDL_ToolkitControl_StateUpdated(control);
                    control->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
                    SDL_ToolkitControl_StateUpdated(control);
                    draw = true;
                }
                cursor = cursor->next;
            }
        } else if (e->data.key == SDL_TOOLKIT_VIDEO_KEY_RETURN || e->data.key == SDL_TOOLKIT_VIDEO_KEY_NUMERIC_ENTER) {
            if (wnd->selected_control) {
                wnd->selected_control->state = SDL_TOOLKIT_CONTROL_STATE_PRESSED;
                SDL_ToolkitControl_StateUpdated(wnd->selected_control);
                wnd->selected_control->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
                SDL_ToolkitControl_StateUpdated(wnd->selected_control);
                draw = true;
            }
        } else if (e->data.key == SDL_TOOLKIT_VIDEO_KEY_TAB || e->data.key == SDL_TOOLKIT_VIDEO_KEY_LEFT || e->data.key == SDL_TOOLKIT_VIDEO_KEY_RIGHT) {
            SDL_ToolkitControl *next;
            SDL_ToolkitControl *prev;

            draw = true;

            if (wnd->selected_control) {
                next = prev = wnd->selected_control;
                cursor = wnd->interactive_controls;
                while (cursor) {
                    if (cursor->entry == wnd->selected_control) {
                        if (cursor->next) {
                            next = cursor->next->entry;
                        }
                        break;
                    }

                    prev = cursor->entry;
                    cursor = cursor->next;
                }
            } else {
                next = prev = NULL;
                if (wnd->controls) {
                    next = prev = wnd->controls->entry;
                }
            }

            if (e->data.key == SDL_TOOLKIT_VIDEO_KEY_LEFT) {
                wnd->selected_control = prev;
            } else {
                wnd->selected_control = next;
            }
        }
        break;
    default:
        break;
    }

    if (draw) {
        SDL_Rect bg_rct;

        SDL_ToolkitVideoWindow_BeginDraw(wnd->vid_wnd);

        bg_rct.x = bg_rct.y = 0;
        bg_rct.w = wnd->vid_wnd->buffer_w;
        bg_rct.h = wnd->vid_wnd->buffer_h;
        SDL_ToolkitRenderContext_SetColor(wnd->ctx, &wnd->colors.bg);
        SDL_ToolkitRenderContext_FillRect(wnd->ctx, &bg_rct);

        cursor = wnd->controls;
        while (cursor) {
            SDL_ToolkitControl *control;

            control = cursor->entry;
            SDL_ToolkitControl_Draw(control);
            cursor = cursor->next;
        }

        SDL_ToolkitVideoWindow_EndDraw(wnd->vid_wnd);
    }
}

void SDL_ToolkitDriverSet_EventLoop(SDL_ToolkitDriverSet *set)
{
    SDL_ToolkitVideoEvent e;

    for (;;) {
        SDL_ToolkitVideoDriver_NextEvent(set->video, &e);

        if (e.type != SDL_TOOLKIT_VIDEO_EVENT_NONE && e.wnd) {
            ProcessWindowEvent(e.wnd->udata, &e);
        }

        if (!set->window_count) {
            break;
        }
    }
}

static void Control_NoOp(SDL_ToolkitControl *base)
{
}

static void IconControl_InitResources(SDL_ToolkitControl *base)
{
    SDL_ToolkitIconControl *control;
    char chr;

    control = (SDL_ToolkitIconControl *)base;

    if (control->font) {
        SDL_ToolkitTextFont_Destroy(control->font);
    }

    if (control->object) {
        SDL_ToolkitTextObject_Destroy(control->object);
    }

    control->font = SDL_ToolkitTextDriver_CreateFont(base->wnd->set->text, 20 * base->wnd->int_scale, true);

    switch (control->icon) {
    case SDL_TOOLKIT_ICON_WARNING:
        chr = '!';
        break;
    case SDL_TOOLKIT_ICON_INFORMATION:
        chr = 'i';
        break;
    default:
        chr = 'X';
        break;
    }
    control->object = SDL_ToolkitTextDriver_CreateObject(base->wnd->set->text, control->font, &chr, 1);
}

static void IconControl_Destroy(SDL_ToolkitControl *base)
{
    SDL_ToolkitIconControl *control;

    control = (SDL_ToolkitIconControl *)base;

    if (control->font) {
        SDL_ToolkitTextFont_Destroy(control->font);
    }

    if (control->object) {
        SDL_ToolkitTextObject_Destroy(control->object);
    }

    SDL_free(control);
}

static void IconControl_Draw(SDL_ToolkitControl *base)
{
    SDL_ToolkitIconControl *control;
    SDL_Rect rct;

    control = (SDL_ToolkitIconControl *)base;

    rct.w = control->disc_w;
    rct.h = control->disc_h;
    rct.x = base->rct.x + control->shadow_pos.x;
    rct.y = base->rct.y + control->shadow_pos.y;
    SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->shadow);
    SDL_ToolkitRenderContext_FillCircle(base->wnd->ctx, &rct);

    rct.w = control->disc_w;
    rct.h = control->disc_h;
    rct.x = base->rct.x;
    rct.y = base->rct.y;
    switch (control->icon) {
    case SDL_TOOLKIT_ICON_WARNING:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->black);
        break;
    case SDL_TOOLKIT_ICON_INFORMATION:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->white);
        break;
    default:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->dark_red);
        break;
    }
    SDL_ToolkitRenderContext_FillCircle(base->wnd->ctx, &rct);

    rct.w = control->disc_w - 2 * base->wnd->int_scale;
    rct.h = control->disc_h - 2 * base->wnd->int_scale;
    rct.x = base->rct.x + 1 * base->wnd->int_scale;
    rct.y = base->rct.y + 1 * base->wnd->int_scale;
    switch (control->icon) {
    case SDL_TOOLKIT_ICON_WARNING:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->yellow);
        break;
    case SDL_TOOLKIT_ICON_INFORMATION:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->blue);
        break;
    default:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->red);
        break;
    }
    SDL_ToolkitRenderContext_FillCircle(base->wnd->ctx, &rct)

    switch (control->icon) {
    case SDL_TOOLKIT_ICON_WARNING:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->black);
        break;
    default:
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &control->white);
        break;
    }
    SDL_ToolkitTextObject_Draw(control->object, base->wnd->ctx, base->rct.x + control->object_pos.x, base->rct.y + control->object_pos.y);
}

static void IconControl_Calculate(SDL_ToolkitControl *base)
{
    SDL_ToolkitIconControl *control;

    control = (SDL_ToolkitIconControl *)base;

    if (base->flags & SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE) {
        control->disc_w = control->disc_h = SDL_max(control->object->w, control->object->h) + SDL_TOOLKIT_PADDING_6 * 2 * base->wnd->int_scale;
        control->object_pos.x = (control->disc_w - control->object->w) / 2;
        control->object_pos.y = (control->disc_h - control->object->h) / 2;
        control->shadow_pos.x = 2 * base->wnd->int_scale;
        control->shadow_pos.y = 2 * base->wnd->int_scale;
        base->rct.w = control->disc_w + control->shadow_pos.x;
        base->rct.h = control->disc_h + control->shadow_pos.y;
        base->rct.x = base->rct.y = 0;
    }
}

SDL_ToolkitControl *SDL_ToolkitIconControl_Create(SDL_ToolkitWindow *wnd, SDL_ToolkitIcon icon)
{
    SDL_ToolkitIconControl *control;
    SDL_ToolkitControl *base;

    control = (SDL_ToolkitIconControl *)SDL_malloc(sizeof(SDL_ToolkitIconControl));
    base = (SDL_ToolkitControl *)control;
    if (!control) {
        return NULL;
    }

    base->wnd = wnd;
    base->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
    base->flags = SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE;
    base->Draw = IconControl_Draw;
    base->Calculate = IconControl_Calculate;
    base->InitResources = IconControl_InitResources;
    base->StateUpdated = Control_NoOp;
    base->FlagsUpdated = Control_NoOp;
    base->Destroy = IconControl_Destroy;
    base->udata = NULL;
    base->udata_free = NULL;

    control->icon = icon;
    control->font = NULL;
    control->object = NULL;
    switch (icon) {
    case SDL_TOOLKIT_ICON_WARNING:
        control->black.rgba.r = 0;
        control->black.rgba.g = 0;
        control->black.rgba.b = 0;

        control->yellow.rgba.r = 255;
        control->yellow.rgba.g = 255;
        control->yellow.rgba.b = 0;

        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->black);
        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->yellow);
        break;
    case SDL_TOOLKIT_ICON_INFORMATION:
        control->white.rgba.r = 255;
        control->white.rgba.g = 255;
        control->white.rgba.b = 255;

        control->blue.rgba.r = 0;
        control->blue.rgba.g = 0;
        control->blue.rgba.b = 255;

        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->white);
        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->blue);
        break;
    default:
        control->white.rgba.r = 255;
        control->white.rgba.g = 255;
        control->white.rgba.b = 255;

        control->red.rgba.r = 255;
        control->red.rgba.g = 0;
        control->red.rgba.b = 0;

        control->dark_red.rgba.r = 158;
        control->dark_red.rgba.g = 0;
        control->dark_red.rgba.b = 0;

        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->white);
        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->red);
        SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->dark_red);
        break;
    }

    if (wnd->colors.bg.rgba.r > 128) {
        control->shadow.rgba.r = SDL_clamp(wnd->colors.bg.rgba.r - 52, 0, 255);
    } else if (!wnd->colors.bg.rgba.r) {
        control->shadow.rgba.r = SDL_clamp(wnd->colors.bg.rgba.r + 35, 0, 255);
    } else {
        control->shadow.rgba.r = SDL_clamp(wnd->colors.bg.rgba.r - 12, 0, 255);
    }

    if (wnd->colors.bg.rgba.g > 128) {
        control->shadow.rgba.g = SDL_clamp(wnd->colors.bg.rgba.g - 52, 0, 255);
    } else if (!wnd->colors.bg.rgba.g) {
        control->shadow.rgba.g = SDL_clamp(wnd->colors.bg.rgba.g + 35, 0, 255);
    } else {
        control->shadow.rgba.g = SDL_clamp(wnd->colors.bg.rgba.g - 12, 0, 255);
    }

    if (wnd->colors.bg.rgba.b > 128) {
        control->shadow.rgba.b = SDL_clamp(wnd->colors.bg.rgba.b - 52, 0, 255);
    } else if (!wnd->colors.bg.rgba.b) {
        control->shadow.rgba.b = SDL_clamp(wnd->colors.bg.rgba.b + 35, 0, 255);
    } else {
        control->shadow.rgba.b = SDL_clamp(wnd->colors.bg.rgba.b - 12, 0, 255);
    }

    SDL_ToolkitVideoWindow_MapColor(wnd->vid_wnd, &control->shadow);

    IconControl_InitResources(base);
    IconControl_Calculate(base);

    SDL_ListAdd(&wnd->controls, base);
    return base;
}

static size_t CountLinesOfText(const char *text)
{
    size_t result;

    result = 0;
    while (text && *text) {
        const char *lf;

        lf = SDL_strchr(text, '\n');
        result++; // even without an endline, this counts as a line.
        text = lf ? lf + 1 : NULL;
    }
    return result;
}

static void LabelControl_InitResources(SDL_ToolkitControl *base)
{
    SDL_ToolkitLabelControl *control;
    const char *utf8;
    size_t i;

    control = (SDL_ToolkitLabelControl *)base;

    if (control->lines) {
        for (i = 0; i < control->lines_sz; i++) {
            SDL_ToolkitTextObject_Destroy(control->lines[i].object);
        }

        SDL_free(control->lines);
    }

    utf8 = control->utf8;
    control->lines_sz = CountLinesOfText(utf8);
    control->lines = SDL_calloc(control->lines_sz, sizeof(SDL_ToolkitLabelControlLine));
    for (i = 0; i < control->lines_sz; i++) {
        const char *lf;
        size_t length;
        size_t sz;

        lf = SDL_strchr(utf8, '\n');
        length = lf ? (lf - utf8) : SDL_strlen(utf8);

        sz = length;
        if (lf && (lf > utf8) && (lf[-1] == '\r')) {
            sz--;
        }

        control->lines[i].object = SDL_ToolkitTextDriver_CreateObject(base->wnd->set->text, base->wnd->fnt, (char *)utf8, sz);

        utf8 += length + 1;
        if (!lf) {
            break;
        }
    }
}

static void LabelControl_Calculate(SDL_ToolkitControl *base)
{
    SDL_ToolkitLabelControl *control;
    SDL_ToolkitTextDirection first_ndn_dir;
    ssize_t last_ndn;
    size_t i;

    control = (SDL_ToolkitLabelControl *)base;

    base->rct.w = base->rct.h = 0;
    base->rct.x = base->rct.y = 0;

    for (i = 0; i < control->lines_sz; i++) {
        base->rct.w = SDL_max(base->rct.w, control->lines[i].object->w);

        if (!i) {
            control->lines[i].pos.y = 0;
        } else {
            control->lines[i].pos.y = control->lines[i].object->fnt->height + control->lines[i - 1].pos.y;
        }
    }

    first_ndn_dir = SDL_TOOLKIT_TEXT_DIRECTION_LTR;
    for (i = 0; i < control->lines_sz; i++) {
        if (control->lines[i].object->dir != SDL_TOOLKIT_TEXT_DIRECTION_NEUTRAL) {
            first_ndn_dir = control->lines[i].object->dir;
        }
    }

    last_ndn = -1;
    for (i = 0; i < control->lines_sz; i++) {
        switch (control->lines[i].object->dir) {
        case SDL_TOOLKIT_TEXT_DIRECTION_LTR:
            control->lines[i].pos.x = 0;
            last_ndn = i;
            break;
        case SDL_TOOLKIT_TEXT_DIRECTION_RTL:
            control->lines[i].pos.x = base->rct.w - control->lines[i].object->w;
            last_ndn = i;
            break;
        default:
            if (last_ndn != -1) {
                if (control->lines[last_ndn].object->dir == SDL_TOOLKIT_TEXT_DIRECTION_RTL) {
                    control->lines[i].pos.x = base->rct.w - control->lines[i].object->w;
                } else {
                    control->lines[i].pos.x = 0;
                }
            } else {
                if (first_ndn_dir == SDL_TOOLKIT_TEXT_DIRECTION_RTL) {
                    control->lines[i].pos.x = base->rct.w - control->lines[i].object->w;
                } else {
                    control->lines[i].pos.x = 0;
                }
            }
            break;
        }
    }

    if (control->lines_sz) {
        base->rct.h = control->lines[control->lines_sz - 1].pos.y + control->lines[control->lines_sz - 1].object->h;
    }
}

static void LabelControl_Draw(SDL_ToolkitControl *base)
{
    SDL_ToolkitLabelControl *control;
    size_t i;

    control = (SDL_ToolkitLabelControl *)base;

    for (i = 0; i < control->lines_sz; i++) {
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.text);
        SDL_ToolkitTextObject_Draw(control->lines[i].object, base->wnd->ctx, base->rct.x + control->lines[i].pos.x, base->rct.y + control->lines[i].pos.y);
    }
}

static void LabelControl_Destroy(SDL_ToolkitControl *base)
{
    SDL_ToolkitLabelControl *control;
    size_t i;

    control = (SDL_ToolkitLabelControl *)base;

    if (control->lines) {
        for (i = 0; i < control->lines_sz; i++) {
            SDL_ToolkitTextObject_Destroy(control->lines[i].object);
        }

        SDL_free(control->lines);
    }

    SDL_free(control);
}

SDL_ToolkitControl *SDL_ToolkitLabelControl_Create(SDL_ToolkitWindow *wnd, const char *utf8)
{
    SDL_ToolkitLabelControl *control;
    SDL_ToolkitControl *base;

    control = (SDL_ToolkitLabelControl *)SDL_malloc(sizeof(SDL_ToolkitLabelControl));
    base = (SDL_ToolkitControl *)control;
    if (!control) {
        return NULL;
    }

    base->wnd = wnd;
    base->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
    base->flags = SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE;
    base->Draw = LabelControl_Draw;
    base->Calculate = LabelControl_Calculate;
    base->InitResources = LabelControl_InitResources;
    base->StateUpdated = Control_NoOp;
    base->FlagsUpdated = Control_NoOp;
    base->Destroy = LabelControl_Destroy;
    base->udata = NULL;
    base->udata_free = NULL;

    control->utf8 = utf8;
    control->lines = NULL;
    control->lines_sz = 0;

    LabelControl_InitResources(base);
    LabelControl_Calculate(base);

    SDL_ListAdd(&wnd->controls, base);
    return base;
}

static void ButtonControl_InitResources(SDL_ToolkitControl *base)
{
    SDL_ToolkitButtonControl *control;

    control = (SDL_ToolkitButtonControl *)base;

    if (control->obj) {
        SDL_ToolkitTextObject_Destroy(control->obj);
    }

    control->obj = SDL_ToolkitTextDriver_CreateObject(base->wnd->set->text, base->wnd->fnt, (char *)control->text, SDL_strlen(control->text));
}

static void ButtonControl_Destroy(SDL_ToolkitControl *base)
{
    SDL_ToolkitButtonControl *control;

    control = (SDL_ToolkitButtonControl *)base;

    if (control->obj) {
        SDL_ToolkitTextObject_Destroy(control->obj);
    }

    SDL_free(control);
}

static void ButtonControl_Calculate(SDL_ToolkitControl *base)
{
    SDL_ToolkitButtonControl *control;

    control = (SDL_ToolkitButtonControl *)base;

    if (base->flags & SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE) {
        base->rct.x = base->rct.y = 0;
        base->rct.w = SDL_TOOLKIT_PADDING_3 * 2 + control->obj->w;
        base->rct.h = SDL_TOOLKIT_PADDING_3 * 2 + control->obj->h;
    }
    control->obj_pos.x = (base->rct.w - control->obj->w) / 2;
    control->obj_pos.y = (base->rct.h - control->obj->h) / 2;
}

static void ButtonControl_Draw(SDL_ToolkitControl *base)
{
    SDL_ToolkitButtonControl *control;
    SDL_Rect rct;

    control = (SDL_ToolkitButtonControl *)base;

    if (base->state == SDL_TOOLKIT_CONTROL_STATE_PRESSED || base->state == SDL_TOOLKIT_CONTROL_STATE_PRESSED_HELD) {
        rct.w = base->rct.w;
        rct.h = base->rct.h;
        rct.x = base->rct.x;
        rct.y = base->rct.y;
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_d);
        SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

        rct.w = base->rct.w - 1 * base->wnd->int_scale;
        rct.h = base->rct.h - 1 * base->wnd->int_scale;
        rct.x = base->rct.x;
        rct.y = base->rct.y;
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_l2);
        SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

        rct.w = base->rct.w - 3 * base->wnd->int_scale;
        rct.h = base->rct.h - 2 * base->wnd->int_scale;
        rct.x = base->rct.x + 1 * base->wnd->int_scale;
        rct.y = base->rct.y + 1 * base->wnd->int_scale;
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_l1);
        SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

        rct.w = base->rct.w - 3 * base->wnd->int_scale;
        rct.h = base->rct.h - 3 * base->wnd->int_scale;
        rct.x = base->rct.x + 1 * base->wnd->int_scale;
        rct.y = base->rct.y + 1 * base->wnd->int_scale;
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_border);
        SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

        rct.w = base->rct.w - 4 * base->wnd->int_scale;
        rct.h = base->rct.h - 4 * base->wnd->int_scale;
        rct.x = base->rct.x + 2 * base->wnd->int_scale;
        rct.y = base->rct.y + 2 * base->wnd->int_scale;
        SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.pressed);
        SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);
    } else {
        if (base->wnd->selected_control == base) {
            rct.w = base->rct.w;
            rct.h = base->rct.h;
            rct.x = base->rct.x;
            rct.y = base->rct.y;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_d);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 3 * base->wnd->int_scale;
            rct.h = base->rct.h - 3 * base->wnd->int_scale;
            rct.x = base->rct.x + 1 * base->wnd->int_scale;
            rct.y = base->rct.y + 1 * base->wnd->int_scale;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_l2);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 4 * base->wnd->int_scale;
            rct.h = base->rct.h - 4 * base->wnd->int_scale;
            rct.x = base->rct.x + 2 * base->wnd->int_scale;
            rct.y = base->rct.y + 2 * base->wnd->int_scale;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_border);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 5 * base->wnd->int_scale;
            rct.h = base->rct.h - 5 * base->wnd->int_scale;
            rct.x = base->rct.x + 2 * base->wnd->int_scale;
            rct.y = base->rct.y + 2 * base->wnd->int_scale;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_l1);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 5 * base->wnd->int_scale;
            rct.h = base->rct.h - 5 * base->wnd->int_scale;
            rct.x = base->rct.x + 3 * base->wnd->int_scale;
            rct.y = base->rct.y + 3 * base->wnd->int_scale;
            if (base->state == SDL_TOOLKIT_CONTROL_STATE_HOVER) {
                SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_selected);
            } else {
                SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_bg);
            }
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);
        } else {
            rct.w = base->rct.w;
            rct.h = base->rct.h;
            rct.x = base->rct.x;
            rct.y = base->rct.y;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_d);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 1 * base->wnd->int_scale;
            rct.h = base->rct.h - 1 * base->wnd->int_scale;
            rct.x = base->rct.x;
            rct.y = base->rct.y;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_l2);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 2 * base->wnd->int_scale;
            rct.h = base->rct.h - 2 * base->wnd->int_scale;
            rct.x = base->rct.x + 1 * base->wnd->int_scale;
            rct.y = base->rct.y + 1 * base->wnd->int_scale;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_border);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 3 * base->wnd->int_scale;
            rct.h = base->rct.h - 3 * base->wnd->int_scale;
            rct.x = base->rct.x + 1 * base->wnd->int_scale;
            rct.y = base->rct.y + 1 * base->wnd->int_scale;
            SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.bevel_l1);
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);

            rct.w = base->rct.w - 4 * base->wnd->int_scale;
            rct.h = base->rct.h - 4 * base->wnd->int_scale;
            rct.x = base->rct.x + 2 * base->wnd->int_scale;
            rct.y = base->rct.y + 2 * base->wnd->int_scale;
            if (base->state == SDL_TOOLKIT_CONTROL_STATE_HOVER) {
                SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_selected);
            } else {
                SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.button_bg);
            }
            SDL_ToolkitRenderContext_FillRect(base->wnd->ctx, &rct);
        }
    }

    SDL_ToolkitRenderContext_SetColor(base->wnd->ctx, &base->wnd->colors.text);
    SDL_ToolkitTextObject_Draw(control->obj, base->wnd->ctx, base->rct.x + control->obj_pos.x, base->rct.y + control->obj_pos.y);
}

static void ButtonControl_StateUpdated(SDL_ToolkitControl *base)
{
    SDL_ToolkitButtonControl *control;

    control = (SDL_ToolkitButtonControl *)base;

    if (base->state == SDL_TOOLKIT_CONTROL_STATE_PRESSED && control->cb) {
        control->cb(base, control->cbdata);
    }
}

static void ButtonControl_FlagsUpdated(SDL_ToolkitControl *base)
{
    if (base->flags & SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ENTER) {
        base->wnd->selected_control = base->wnd->selected_control_before_focus_changed = base;
    }
}

SDL_ToolkitControl *SDL_ToolkitButtonControl_Create(SDL_ToolkitWindow *wnd, const char *text, void (*cb)(struct SDL_ToolkitControl *, void *), void *cbdata)
{
    SDL_ToolkitButtonControl *control;
    SDL_ToolkitControl *base;

    control = (SDL_ToolkitButtonControl *)SDL_malloc(sizeof(SDL_ToolkitButtonControl));
    base = (SDL_ToolkitControl *)control;
    if (!control) {
        return NULL;
    }

    base->wnd = wnd;
    base->state = SDL_TOOLKIT_CONTROL_STATE_NORMAL;
    base->flags = SDL_TOOLKIT_CONTROL_FLAGS_DYNAMIC | SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE | SDL_TOOLKIT_CONTROL_FLAGS_RESIZABLE;
    base->Draw = ButtonControl_Draw;
    base->Calculate = ButtonControl_Calculate;
    base->InitResources = ButtonControl_InitResources;
    base->StateUpdated = ButtonControl_StateUpdated;
    base->FlagsUpdated = ButtonControl_FlagsUpdated;
    base->Destroy = ButtonControl_Destroy;
    base->udata = NULL;
    base->udata_free = NULL;

    control->obj = NULL;
    control->cb = cb;
    control->cbdata = cbdata;
    control->text = text;

    ButtonControl_InitResources(base);
    ButtonControl_Calculate(base);

    base->flags &= ~SDL_TOOLKIT_CONTROL_FLAGS_CALCULATE_SIZE;
    SDL_ListAdd(&wnd->controls, base);
    SDL_ListAdd(&wnd->interactive_controls, base);
    return base;
}

void SDL_ToolkitWindow_Close(SDL_ToolkitWindow *wnd)
{
    wnd->flags &= ~SDL_TOOLKIT_WINDOW_FLAGS_ACTIVE;
    SDL_ToolkitVideoWindow_Hide(wnd->vid_wnd);
    wnd->set->window_count--;
}

unsigned int SDL_ToolkitLabelControl_GetHeightOfFirstLine(SDL_ToolkitControl *base)
{
    SDL_ToolkitLabelControl *control;

    control = (SDL_ToolkitLabelControl *)base;

    if (control->lines_sz) {
        return control->lines[0].object->h;
    } else {
        return 0;
    }
}

void SDL_ToolkitControl_Destroy(SDL_ToolkitControl *base)
{
    if (!base) {
        return;
    }

    if (base->udata_free) {
        base->udata_free(base);
    }

    base->Destroy(base);
}
