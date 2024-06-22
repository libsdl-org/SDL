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
#include "SDL_internal.h"

#if SDL_VIDEO_DRIVER_WAYLAND

#include "../SDL_sysvideo.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "text-input-unstable-v3-client-protocol.h"

int Wayland_InitKeyboard(SDL_VideoDevice *_this)
{
#ifdef SDL_USE_IME
    SDL_VideoData *driverdata = _this->driverdata;
    if (!driverdata->text_input_manager) {
        SDL_IME_Init();
    }
#endif
    SDL_SetScancodeName(SDL_SCANCODE_APPLICATION, "Menu");

    return 0;
}

void Wayland_QuitKeyboard(SDL_VideoDevice *_this)
{
#ifdef SDL_USE_IME
    SDL_VideoData *driverdata = _this->driverdata;
    if (!driverdata->text_input_manager) {
        SDL_IME_Quit();
    }
#endif
}

int Wayland_StartTextInput(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *driverdata = _this->driverdata;
    struct SDL_WaylandInput *input = driverdata->input;

    if (driverdata->text_input_manager) {
        if (input && input->text_input) {
            const SDL_Rect *rect = &input->text_input->cursor_rect;

            zwp_text_input_v3_enable(input->text_input->text_input);

            /* Now that it's enabled, set the input properties */
            zwp_text_input_v3_set_content_type(input->text_input->text_input,
                                               ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                                               ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
            if (!SDL_RectEmpty(rect)) {
                /* This gets reset on enable so we have to cache it */
                zwp_text_input_v3_set_cursor_rectangle(input->text_input->text_input,
                                                       rect->x,
                                                       rect->y,
                                                       rect->w,
                                                       rect->h);
            }
            zwp_text_input_v3_commit(input->text_input->text_input);
        }
    }

    if (input && input->xkb.compose_state) {
        /* Reset compose state so composite and dead keys don't carry over */
        WAYLAND_xkb_compose_state_reset(input->xkb.compose_state);
    }

    return Wayland_UpdateTextInputRect(_this, window);
}

int Wayland_StopTextInput(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *driverdata = _this->driverdata;
    struct SDL_WaylandInput *input = driverdata->input;

    if (driverdata->text_input_manager) {
        if (input && input->text_input) {
            zwp_text_input_v3_disable(input->text_input->text_input);
            zwp_text_input_v3_commit(input->text_input->text_input);
        }
    }
#ifdef SDL_USE_IME
    else {
        SDL_IME_Reset();
    }
#endif

    if (input && input->xkb.compose_state) {
        /* Reset compose state so composite and dead keys don't carry over */
        WAYLAND_xkb_compose_state_reset(input->xkb.compose_state);
    }
    return 0;
}

int Wayland_UpdateTextInputRect(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *driverdata = _this->driverdata;
    if (driverdata->text_input_manager) {
        struct SDL_WaylandInput *input = driverdata->input;
        if (input && input->text_input) {
            if (!SDL_RectsEqual(&window->text_input_rect, &input->text_input->cursor_rect)) {
                SDL_copyp(&input->text_input->cursor_rect, &window->text_input_rect);
                zwp_text_input_v3_set_cursor_rectangle(input->text_input->text_input,
                                                       window->text_input_rect.x,
                                                       window->text_input_rect.y,
                                                       window->text_input_rect.w,
                                                       window->text_input_rect.h);
                zwp_text_input_v3_commit(input->text_input->text_input);
            }
        }
    }

#ifdef SDL_USE_IME
    else {
        SDL_IME_UpdateTextRect(window);
    }
#endif
    return 0;
}

SDL_bool Wayland_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    /* In reality we just want to return true when the screen keyboard is the
     * _only_ way to get text input. So, in addition to checking for the text
     * input protocol, make sure we don't have any physical keyboards either.
     */
    SDL_VideoData *driverdata = _this->driverdata;
    SDL_bool haskeyboard = (driverdata->input != NULL) && (driverdata->input->keyboard != NULL);
    SDL_bool hastextmanager = (driverdata->text_input_manager != NULL);
    return !haskeyboard && hastextmanager;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
