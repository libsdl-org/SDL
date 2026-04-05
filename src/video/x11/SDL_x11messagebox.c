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

#include "../../toolkit/SDL_toolkit.h"
#include "../../toolkit/SDL_toolkitx11.h"
#include "../../dialog/unix/SDL_zenitymessagebox.h"

typedef struct SDL_MessageBoxDataX11
{
    const SDL_MessageBoxData *messageboxdata;

    SDL_ToolkitDriverSet *set;
    SDL_ToolkitWindow *wnd;
    SDL_ToolkitVideoWindowParent parent;
    SDL_ToolkitColorScheme scheme;

    SDL_ToolkitControl *icon;
    SDL_ToolkitControl *label;
    SDL_ToolkitControl **buttons;

    int ret_id;
} SDL_MessageBoxDataX11;

static void FreeUserData(SDL_ToolkitControl *base)
{
    SDL_free(base->udata);
}

static void ButtonCallback(SDL_ToolkitControl *base, void *cbdata)
{
    SDL_MessageBoxDataX11 *data;

    data = cbdata;
    data->ret_id = *(int *)base->udata;
    SDL_ToolkitWindow_Close(base->wnd);
}

static void PositionControls(SDL_MessageBoxDataX11 *data)
{
    SDL_Rect rct;
    int first_line_width;
    int first_line_height;
    int second_line_width;
    int second_line_height;
    int max_button_width;
    int max_button_height;
    int iw;
    int ih;
    int i;
    bool rtl;

    if (data->messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
        rtl = true;
    } else if (data->messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT) {
        rtl = false;
    } else {
        rtl = data->wnd->flags & SDL_TOOLKIT_WINDOW_FLAGS_RTL;
    }

    first_line_width = first_line_height = 0;
    if (data->icon && data->label) {
        data->icon->rct.y = 0;

        first_line_width = data->icon->rct.w + SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale + data->label->rct.w;

        if (data->wnd->flags & SDL_TOOLKIT_WINDOW_FLAGS_RTL) {
            data->label->rct.x = 0;
            data->icon->rct.x = data->label->rct.w + SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
        } else {
            data->icon->rct.x = 0;
            data->label->rct.x = data->icon->rct.w + SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
        }

        if (data->label->rct.h > data->icon->rct.h) {
            data->label->rct.y = (data->icon->rct.h - SDL_ToolkitLabelControl_GetHeightOfFirstLine(data->label)) / 2;
            first_line_height = data->label->rct.y + data->label->rct.h;
        } else {
            data->label->rct.y = (data->icon->rct.h - data->label->rct.h) / 2;
            first_line_height = data->icon->rct.h;
        }
    } else if (!data->icon && data->label) {
        first_line_width = data->label->rct.w;
        first_line_height = data->label->rct.h;
        data->label->rct.x = 0;
        data->label->rct.y = 0;
    } else if (data->icon && !data->label) {
        first_line_width = data->icon->rct.w;
        first_line_height = data->icon->rct.h;
        data->icon->rct.x = 0;
        data->icon->rct.y = 0;
    }

    max_button_width = 50 * data->wnd->int_scale;
    max_button_height = 32 * data->wnd->int_scale;
    second_line_width = second_line_height = 0;

    for (i = 0; i < data->messageboxdata->numbuttons; i++) {
        max_button_width = SDL_max(max_button_width, data->buttons[i]->rct.w);
        max_button_height = SDL_max(max_button_height, data->buttons[i]->rct.h);
        data->buttons[i]->rct.x = 0;
        data->buttons[i]->rct.y = 0;
    }

    if (rtl) {
        for (i = (data->messageboxdata->numbuttons - 1); i != -1; i--) {
            data->buttons[i]->rct.w = max_button_width;
            data->buttons[i]->rct.h = max_button_height;
            SDL_ToolkitControl_Calculate(data->buttons[i]);

            if (first_line_height) {
                data->buttons[i]->rct.y = first_line_height + SDL_TOOLKIT_PADDING_4 * data->wnd->int_scale;
                second_line_height = max_button_height + SDL_TOOLKIT_PADDING_4 * data->wnd->int_scale;
            } else {
                second_line_height = max_button_height;
            }

            if ((i + 1) < data->messageboxdata->numbuttons) {
                data->buttons[i]->rct.x = data->buttons[i + 1]->rct.x + data->buttons[i + 1]->rct.w + (SDL_TOOLKIT_PADDING_3 * data->wnd->int_scale);
            }
        }
    } else {
        for (i = 0; i < data->messageboxdata->numbuttons; i++) {
            data->buttons[i]->rct.w = max_button_width;
            data->buttons[i]->rct.h = max_button_height;
            SDL_ToolkitControl_Calculate(data->buttons[i]);

            if (first_line_height) {
                data->buttons[i]->rct.y = first_line_height + SDL_TOOLKIT_PADDING_4 * data->wnd->int_scale;
                second_line_height = max_button_height + SDL_TOOLKIT_PADDING_4 * data->wnd->int_scale;
            } else {
                second_line_height = max_button_height;
            }

            if (i) {
                data->buttons[i]->rct.x = data->buttons[i - 1]->rct.x + data->buttons[i - 1]->rct.w + (SDL_TOOLKIT_PADDING_3 * data->wnd->int_scale);
            }
        }
    }

    if (data->messageboxdata->numbuttons) {
        if (rtl) {
            second_line_width = data->buttons[0]->rct.x + data->buttons[0]->rct.w;
        } else {
            second_line_width = data->buttons[data->messageboxdata->numbuttons - 1]->rct.x + data->buttons[data->messageboxdata->numbuttons - 1]->rct.w;
        }
    }

    if (second_line_width > first_line_width) {
        int pad;

        pad = (second_line_width - first_line_width) / 2;
        if (data->label) {
            data->label->rct.x += pad;
        }
        if (data->icon) {
            data->icon->rct.x += pad;
        }
    } else {
        int pad;

        pad = (first_line_width - second_line_width) / 2;
        for (i = 0; i < data->messageboxdata->numbuttons; i++) {
            data->buttons[i]->rct.x += pad;
        }
    }

    iw = rct.w = SDL_max(first_line_width, second_line_width) + SDL_TOOLKIT_PADDING_2 * 2 * data->wnd->int_scale;
    ih = rct.h = first_line_height + second_line_height + SDL_TOOLKIT_PADDING_2 * 2 * data->wnd->int_scale;
    rct.w = SDL_ToolkitWindow_ReverseScaleValue(data->wnd, rct.w);
    rct.h = SDL_ToolkitWindow_ReverseScaleValue(data->wnd, rct.h);
    SDL_ToolkitWindow_UpdateRect(data->wnd, &rct, iw, ih);

    if (data->label) {
        data->label->rct.x += SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
        data->label->rct.y += SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
    }

    if (data->icon) {
        data->icon->rct.x += SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
        data->icon->rct.y += SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
    }

    for (i = 0; i < data->messageboxdata->numbuttons; i++) {
        data->buttons[i]->rct.x += SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
        data->buttons[i]->rct.y += SDL_TOOLKIT_PADDING_2 * data->wnd->int_scale;
    }
}

static void CreateControls(SDL_MessageBoxDataX11 *data)
{
    int i;

    switch (data->messageboxdata->flags & (SDL_MESSAGEBOX_ERROR | SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_INFORMATION)) {
    case SDL_MESSAGEBOX_ERROR:
        data->icon = SDL_ToolkitIconControl_Create(data->wnd, SDL_TOOLKIT_ICON_ERROR);
        break;
    case SDL_MESSAGEBOX_WARNING:
        data->icon = SDL_ToolkitIconControl_Create(data->wnd, SDL_TOOLKIT_ICON_WARNING);
        break;
    case SDL_MESSAGEBOX_INFORMATION:
        data->icon = SDL_ToolkitIconControl_Create(data->wnd, SDL_TOOLKIT_ICON_INFORMATION);
        break;
    default:
        data->icon = NULL;
    }

    if (data->messageboxdata->message) {
        if (SDL_strcmp(data->messageboxdata->message, "")) {
            data->label = SDL_ToolkitLabelControl_Create(data->wnd, data->messageboxdata->message);
        } else {
            data->label = NULL;
        }
    } else {
        data->label = NULL;
    }

    data->buttons = SDL_calloc(data->messageboxdata->numbuttons, sizeof(SDL_ToolkitControl *));
    for (i = 0; i < data->messageboxdata->numbuttons; i++) {
        data->buttons[i] = SDL_ToolkitButtonControl_Create(data->wnd, data->messageboxdata->buttons[i].text, ButtonCallback, data);

        data->buttons[i]->udata = SDL_malloc(sizeof(int));
        *(int *)data->buttons[i]->udata = data->messageboxdata->buttons[i].buttonID;
        data->buttons[i]->udata_free = FreeUserData;

        if (data->messageboxdata->buttons[i].flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) {
            data->buttons[i]->flags |= SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ENTER;
        }

        if (data->messageboxdata->buttons[i].flags & SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT) {
            data->buttons[i]->flags |= SDL_TOOLKIT_CONTROL_FLAGS_ACTIVATE_ON_ESCAPE;
        }

        SDL_ToolkitControl_FlagsUpdated(data->buttons[i]);
    }
}

static void DestroyControls(SDL_MessageBoxDataX11 *data)
{
    int i;

    for (i = 0; i < data->messageboxdata->numbuttons; i++) {
        SDL_ToolkitControl_Destroy(data->buttons[i]);
    }

    if (data->messageboxdata->numbuttons) {
        SDL_free(data->buttons);
    }

    if (data->label) {
        SDL_ToolkitControl_Destroy(data->label);
    }

    if (data->icon) {
        SDL_ToolkitControl_Destroy(data->icon);
    }
}

static void OnScaleChange(SDL_ToolkitWindow *window, void *udata)
{
    SDL_MessageBoxDataX11 *data;

    data = udata;

    DestroyControls(data);
    CreateControls(data);
    PositionControls(data);
    SDL_ToolkitWindow_CenterInParent(data->wnd);
}

bool X11_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    SDL_MessageBoxDataX11 data;

	if (SDL_Zenity_ShowMessageBox(messageboxdata, buttonID)) {
        return true;
    }
    
    data.ret_id = -1;
    data.messageboxdata = messageboxdata;
    data.set = SDL_ToolkitDriverSet_Create(SDL_GetVideoDevice());
    if (!SDL_ToolkitDriverSet_IsValid(data.set)) {
		SDL_ToolkitDriverSet_Destroy(data.set);
		return false;
	}
	
    if (messageboxdata->colorScheme) {
        data.scheme.bg.rgba.r = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BACKGROUND].r;
        data.scheme.bg.rgba.g = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BACKGROUND].g;
        data.scheme.bg.rgba.b = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BACKGROUND].b;

        data.scheme.text.rgba.r = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_TEXT].r;
        data.scheme.text.rgba.g = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_TEXT].g;
        data.scheme.text.rgba.b = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_TEXT].b;

        data.scheme.button_border.rgba.r = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].r;
        data.scheme.button_border.rgba.g = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].g;
        data.scheme.button_border.rgba.b = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].b;

        data.scheme.button_bg.rgba.r = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].r;
        data.scheme.button_bg.rgba.g = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].g;
        data.scheme.button_bg.rgba.b = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].b;

        data.scheme.button_selected.rgba.r = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].r;
        data.scheme.button_selected.rgba.g = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].g;
        data.scheme.button_selected.rgba.b = messageboxdata->colorScheme->colors[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].b;
    } else {
        SDL_ToolkitColorScheme_SetSystemColors(&data.scheme);
    }
    SDL_ToolkitColorScheme_GenerateShades(&data.scheme);

    data.parent.type = SDL_TOOLKIT_VIDEO_WINDOW_PARENT_SDL;
    data.parent.parent.sdl = messageboxdata->window;
    if (messageboxdata->window) {
        data.wnd = SDL_ToolkitWindow_Create(data.set, &data.scheme, &data.parent, SDL_TOOLKIT_VIDEO_WINDOW_TYPE_DIALOG, 300, 300, messageboxdata->title);
    } else {
        data.wnd = SDL_ToolkitWindow_Create(data.set, &data.scheme, NULL, SDL_TOOLKIT_VIDEO_WINDOW_TYPE_DIALOG, 300, 300, messageboxdata->title);
    }
    if (!data.wnd) {
		SDL_ToolkitDriverSet_Destroy(data.set);
		return false;
	}
    data.wnd->scale_change_cb = OnScaleChange;
    data.wnd->scale_change_cbdata = &data;

    CreateControls(&data);
    PositionControls(&data);

    SDL_ToolkitWindow_Realize(data.wnd);
    SDL_ToolkitWindow_CenterInParent(data.wnd);
    SDL_ToolkitWindow_Show(data.wnd);
    SDL_ToolkitDriverSet_EventLoop(data.set);

    DestroyControls(&data);
    SDL_ToolkitWindow_Destroy(data.wnd);
    SDL_ToolkitDriverSet_Destroy(data.set);

    if (data.ret_id != -1) {
        *buttonID = data.ret_id;
    }
    return true;
}

#endif // SDL_VIDEO_DRIVER_X11
