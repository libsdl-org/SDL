/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_EMSCRIPTEN

#include <emscripten/html5.h>

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_touch_c.h"

#include "SDL_emscriptenevents.h"
#include "SDL_emscriptenvideo.h"

#include "SDL_hints.h"

#define FULLSCREEN_MASK ( SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_FULLSCREEN )

/*
.keyCode to scancode
https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent
https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/keyCode
*/
static const SDL_Scancode emscripten_scancode_table[] = {
    /*  0 */    SDL_SCANCODE_UNKNOWN,
    /*  1 */    SDL_SCANCODE_UNKNOWN,
    /*  2 */    SDL_SCANCODE_UNKNOWN,
    /*  3 */    SDL_SCANCODE_CANCEL,
    /*  4 */    SDL_SCANCODE_UNKNOWN,
    /*  5 */    SDL_SCANCODE_UNKNOWN,
    /*  6 */    SDL_SCANCODE_HELP,
    /*  7 */    SDL_SCANCODE_UNKNOWN,
    /*  8 */    SDL_SCANCODE_BACKSPACE,
    /*  9 */    SDL_SCANCODE_TAB,
    /*  10 */   SDL_SCANCODE_UNKNOWN,
    /*  11 */   SDL_SCANCODE_UNKNOWN,
    /*  12 */   SDL_SCANCODE_UNKNOWN,
    /*  13 */   SDL_SCANCODE_RETURN,
    /*  14 */   SDL_SCANCODE_UNKNOWN,
    /*  15 */   SDL_SCANCODE_UNKNOWN,
    /*  16 */   SDL_SCANCODE_LSHIFT,
    /*  17 */   SDL_SCANCODE_LCTRL,
    /*  18 */   SDL_SCANCODE_LALT,
    /*  19 */   SDL_SCANCODE_PAUSE,
    /*  20 */   SDL_SCANCODE_CAPSLOCK,
    /*  21 */   SDL_SCANCODE_UNKNOWN,
    /*  22 */   SDL_SCANCODE_UNKNOWN,
    /*  23 */   SDL_SCANCODE_UNKNOWN,
    /*  24 */   SDL_SCANCODE_UNKNOWN,
    /*  25 */   SDL_SCANCODE_UNKNOWN,
    /*  26 */   SDL_SCANCODE_UNKNOWN,
    /*  27 */   SDL_SCANCODE_ESCAPE,
    /*  28 */   SDL_SCANCODE_UNKNOWN,
    /*  29 */   SDL_SCANCODE_UNKNOWN,
    /*  30 */   SDL_SCANCODE_UNKNOWN,
    /*  31 */   SDL_SCANCODE_UNKNOWN,
    /*  32 */   SDL_SCANCODE_SPACE,
    /*  33 */   SDL_SCANCODE_PAGEUP,
    /*  34 */   SDL_SCANCODE_PAGEDOWN,
    /*  35 */   SDL_SCANCODE_END,
    /*  36 */   SDL_SCANCODE_HOME,
    /*  37 */   SDL_SCANCODE_LEFT,
    /*  38 */   SDL_SCANCODE_UP,
    /*  39 */   SDL_SCANCODE_RIGHT,
    /*  40 */   SDL_SCANCODE_DOWN,
    /*  41 */   SDL_SCANCODE_UNKNOWN,
    /*  42 */   SDL_SCANCODE_UNKNOWN,
    /*  43 */   SDL_SCANCODE_UNKNOWN,
    /*  44 */   SDL_SCANCODE_UNKNOWN,
    /*  45 */   SDL_SCANCODE_INSERT,
    /*  46 */   SDL_SCANCODE_DELETE,
    /*  47 */   SDL_SCANCODE_UNKNOWN,
    /*  48 */   SDL_SCANCODE_0,
    /*  49 */   SDL_SCANCODE_1,
    /*  50 */   SDL_SCANCODE_2,
    /*  51 */   SDL_SCANCODE_3,
    /*  52 */   SDL_SCANCODE_4,
    /*  53 */   SDL_SCANCODE_5,
    /*  54 */   SDL_SCANCODE_6,
    /*  55 */   SDL_SCANCODE_7,
    /*  56 */   SDL_SCANCODE_8,
    /*  57 */   SDL_SCANCODE_9,
    /*  58 */   SDL_SCANCODE_UNKNOWN,
    /*  59 */   SDL_SCANCODE_SEMICOLON,
    /*  60 */   SDL_SCANCODE_UNKNOWN,
    /*  61 */   SDL_SCANCODE_EQUALS,
    /*  62 */   SDL_SCANCODE_UNKNOWN,
    /*  63 */   SDL_SCANCODE_UNKNOWN,
    /*  64 */   SDL_SCANCODE_UNKNOWN,
    /*  65 */   SDL_SCANCODE_A,
    /*  66 */   SDL_SCANCODE_B,
    /*  67 */   SDL_SCANCODE_C,
    /*  68 */   SDL_SCANCODE_D,
    /*  69 */   SDL_SCANCODE_E,
    /*  70 */   SDL_SCANCODE_F,
    /*  71 */   SDL_SCANCODE_G,
    /*  72 */   SDL_SCANCODE_H,
    /*  73 */   SDL_SCANCODE_I,
    /*  74 */   SDL_SCANCODE_J,
    /*  75 */   SDL_SCANCODE_K,
    /*  76 */   SDL_SCANCODE_L,
    /*  77 */   SDL_SCANCODE_M,
    /*  78 */   SDL_SCANCODE_N,
    /*  79 */   SDL_SCANCODE_O,
    /*  80 */   SDL_SCANCODE_P,
    /*  81 */   SDL_SCANCODE_Q,
    /*  82 */   SDL_SCANCODE_R,
    /*  83 */   SDL_SCANCODE_S,
    /*  84 */   SDL_SCANCODE_T,
    /*  85 */   SDL_SCANCODE_U,
    /*  86 */   SDL_SCANCODE_V,
    /*  87 */   SDL_SCANCODE_W,
    /*  88 */   SDL_SCANCODE_X,
    /*  89 */   SDL_SCANCODE_Y,
    /*  90 */   SDL_SCANCODE_Z,
    /*  91 */   SDL_SCANCODE_LGUI,
    /*  92 */   SDL_SCANCODE_UNKNOWN,
    /*  93 */   SDL_SCANCODE_APPLICATION,
    /*  94 */   SDL_SCANCODE_UNKNOWN,
    /*  95 */   SDL_SCANCODE_UNKNOWN,
    /*  96 */   SDL_SCANCODE_KP_0,
    /*  97 */   SDL_SCANCODE_KP_1,
    /*  98 */   SDL_SCANCODE_KP_2,
    /*  99 */   SDL_SCANCODE_KP_3,
    /* 100 */   SDL_SCANCODE_KP_4,
    /* 101 */   SDL_SCANCODE_KP_5,
    /* 102 */   SDL_SCANCODE_KP_6,
    /* 103 */   SDL_SCANCODE_KP_7,
    /* 104 */   SDL_SCANCODE_KP_8,
    /* 105 */   SDL_SCANCODE_KP_9,
    /* 106 */   SDL_SCANCODE_KP_MULTIPLY,
    /* 107 */   SDL_SCANCODE_KP_PLUS,
    /* 108 */   SDL_SCANCODE_UNKNOWN,
    /* 109 */   SDL_SCANCODE_KP_MINUS,
    /* 110 */   SDL_SCANCODE_KP_PERIOD,
    /* 111 */   SDL_SCANCODE_KP_DIVIDE,
    /* 112 */   SDL_SCANCODE_F1,
    /* 113 */   SDL_SCANCODE_F2,
    /* 114 */   SDL_SCANCODE_F3,
    /* 115 */   SDL_SCANCODE_F4,
    /* 116 */   SDL_SCANCODE_F5,
    /* 117 */   SDL_SCANCODE_F6,
    /* 118 */   SDL_SCANCODE_F7,
    /* 119 */   SDL_SCANCODE_F8,
    /* 120 */   SDL_SCANCODE_F9,
    /* 121 */   SDL_SCANCODE_F10,
    /* 122 */   SDL_SCANCODE_F11,
    /* 123 */   SDL_SCANCODE_F12,
    /* 124 */   SDL_SCANCODE_F13,
    /* 125 */   SDL_SCANCODE_F14,
    /* 126 */   SDL_SCANCODE_F15,
    /* 127 */   SDL_SCANCODE_F16,
    /* 128 */   SDL_SCANCODE_F17,
    /* 129 */   SDL_SCANCODE_F18,
    /* 130 */   SDL_SCANCODE_F19,
    /* 131 */   SDL_SCANCODE_F20,
    /* 132 */   SDL_SCANCODE_F21,
    /* 133 */   SDL_SCANCODE_F22,
    /* 134 */   SDL_SCANCODE_F23,
    /* 135 */   SDL_SCANCODE_F24,
    /* 136 */   SDL_SCANCODE_UNKNOWN,
    /* 137 */   SDL_SCANCODE_UNKNOWN,
    /* 138 */   SDL_SCANCODE_UNKNOWN,
    /* 139 */   SDL_SCANCODE_UNKNOWN,
    /* 140 */   SDL_SCANCODE_UNKNOWN,
    /* 141 */   SDL_SCANCODE_UNKNOWN,
    /* 142 */   SDL_SCANCODE_UNKNOWN,
    /* 143 */   SDL_SCANCODE_UNKNOWN,
    /* 144 */   SDL_SCANCODE_NUMLOCKCLEAR,
    /* 145 */   SDL_SCANCODE_SCROLLLOCK,
    /* 146 */   SDL_SCANCODE_UNKNOWN,
    /* 147 */   SDL_SCANCODE_UNKNOWN,
    /* 148 */   SDL_SCANCODE_UNKNOWN,
    /* 149 */   SDL_SCANCODE_UNKNOWN,
    /* 150 */   SDL_SCANCODE_UNKNOWN,
    /* 151 */   SDL_SCANCODE_UNKNOWN,
    /* 152 */   SDL_SCANCODE_UNKNOWN,
    /* 153 */   SDL_SCANCODE_UNKNOWN,
    /* 154 */   SDL_SCANCODE_UNKNOWN,
    /* 155 */   SDL_SCANCODE_UNKNOWN,
    /* 156 */   SDL_SCANCODE_UNKNOWN,
    /* 157 */   SDL_SCANCODE_UNKNOWN,
    /* 158 */   SDL_SCANCODE_UNKNOWN,
    /* 159 */   SDL_SCANCODE_UNKNOWN,
    /* 160 */   SDL_SCANCODE_UNKNOWN,
    /* 161 */   SDL_SCANCODE_UNKNOWN,
    /* 162 */   SDL_SCANCODE_UNKNOWN,
    /* 163 */   SDL_SCANCODE_UNKNOWN,
    /* 164 */   SDL_SCANCODE_UNKNOWN,
    /* 165 */   SDL_SCANCODE_UNKNOWN,
    /* 166 */   SDL_SCANCODE_UNKNOWN,
    /* 167 */   SDL_SCANCODE_UNKNOWN,
    /* 168 */   SDL_SCANCODE_UNKNOWN,
    /* 169 */   SDL_SCANCODE_UNKNOWN,
    /* 170 */   SDL_SCANCODE_UNKNOWN,
    /* 171 */   SDL_SCANCODE_UNKNOWN,
    /* 172 */   SDL_SCANCODE_UNKNOWN,
    /* 173 */   SDL_SCANCODE_MINUS, /*FX*/
    /* 174 */   SDL_SCANCODE_UNKNOWN,
    /* 175 */   SDL_SCANCODE_UNKNOWN,
    /* 176 */   SDL_SCANCODE_UNKNOWN,
    /* 177 */   SDL_SCANCODE_UNKNOWN,
    /* 178 */   SDL_SCANCODE_UNKNOWN,
    /* 179 */   SDL_SCANCODE_UNKNOWN,
    /* 180 */   SDL_SCANCODE_UNKNOWN,
    /* 181 */   SDL_SCANCODE_UNKNOWN,
    /* 182 */   SDL_SCANCODE_UNKNOWN,
    /* 183 */   SDL_SCANCODE_UNKNOWN,
    /* 184 */   SDL_SCANCODE_UNKNOWN,
    /* 185 */   SDL_SCANCODE_UNKNOWN,
    /* 186 */   SDL_SCANCODE_SEMICOLON, /*IE, Chrome, D3E legacy*/
    /* 187 */   SDL_SCANCODE_EQUALS, /*IE, Chrome, D3E legacy*/
    /* 188 */   SDL_SCANCODE_COMMA,
    /* 189 */   SDL_SCANCODE_MINUS, /*IE, Chrome, D3E legacy*/
    /* 190 */   SDL_SCANCODE_PERIOD,
    /* 191 */   SDL_SCANCODE_SLASH,
    /* 192 */   SDL_SCANCODE_GRAVE, /*FX, D3E legacy (SDL_SCANCODE_APOSTROPHE in IE/Chrome)*/
    /* 193 */   SDL_SCANCODE_UNKNOWN,
    /* 194 */   SDL_SCANCODE_UNKNOWN,
    /* 195 */   SDL_SCANCODE_UNKNOWN,
    /* 196 */   SDL_SCANCODE_UNKNOWN,
    /* 197 */   SDL_SCANCODE_UNKNOWN,
    /* 198 */   SDL_SCANCODE_UNKNOWN,
    /* 199 */   SDL_SCANCODE_UNKNOWN,
    /* 200 */   SDL_SCANCODE_UNKNOWN,
    /* 201 */   SDL_SCANCODE_UNKNOWN,
    /* 202 */   SDL_SCANCODE_UNKNOWN,
    /* 203 */   SDL_SCANCODE_UNKNOWN,
    /* 204 */   SDL_SCANCODE_UNKNOWN,
    /* 205 */   SDL_SCANCODE_UNKNOWN,
    /* 206 */   SDL_SCANCODE_UNKNOWN,
    /* 207 */   SDL_SCANCODE_UNKNOWN,
    /* 208 */   SDL_SCANCODE_UNKNOWN,
    /* 209 */   SDL_SCANCODE_UNKNOWN,
    /* 210 */   SDL_SCANCODE_UNKNOWN,
    /* 211 */   SDL_SCANCODE_UNKNOWN,
    /* 212 */   SDL_SCANCODE_UNKNOWN,
    /* 213 */   SDL_SCANCODE_UNKNOWN,
    /* 214 */   SDL_SCANCODE_UNKNOWN,
    /* 215 */   SDL_SCANCODE_UNKNOWN,
    /* 216 */   SDL_SCANCODE_UNKNOWN,
    /* 217 */   SDL_SCANCODE_UNKNOWN,
    /* 218 */   SDL_SCANCODE_UNKNOWN,
    /* 219 */   SDL_SCANCODE_LEFTBRACKET,
    /* 220 */   SDL_SCANCODE_BACKSLASH,
    /* 221 */   SDL_SCANCODE_RIGHTBRACKET,
    /* 222 */   SDL_SCANCODE_APOSTROPHE, /*FX, D3E legacy*/
};


/* "borrowed" from SDL_windowsevents.c */
int
Emscripten_ConvertUTF32toUTF8(Uint32 codepoint, char * text)
{
    if (codepoint <= 0x7F) {
        text[0] = (char) codepoint;
        text[1] = '\0';
    } else if (codepoint <= 0x7FF) {
        text[0] = 0xC0 | (char) ((codepoint >> 6) & 0x1F);
        text[1] = 0x80 | (char) (codepoint & 0x3F);
        text[2] = '\0';
    } else if (codepoint <= 0xFFFF) {
        text[0] = 0xE0 | (char) ((codepoint >> 12) & 0x0F);
        text[1] = 0x80 | (char) ((codepoint >> 6) & 0x3F);
        text[2] = 0x80 | (char) (codepoint & 0x3F);
        text[3] = '\0';
    } else if (codepoint <= 0x10FFFF) {
        text[0] = 0xF0 | (char) ((codepoint >> 18) & 0x0F);
        text[1] = 0x80 | (char) ((codepoint >> 12) & 0x3F);
        text[2] = 0x80 | (char) ((codepoint >> 6) & 0x3F);
        text[3] = 0x80 | (char) (codepoint & 0x3F);
        text[4] = '\0';
    } else {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

EM_BOOL
Emscripten_HandleMouseMove(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    int mx = mouseEvent->canvasX, my = mouseEvent->canvasY;
    EmscriptenPointerlockChangeEvent pointerlock_status;

    /* check for pointer lock */
    emscripten_get_pointerlock_status(&pointerlock_status);

    if (pointerlock_status.isActive) {
        mx = mouseEvent->movementX;
        my = mouseEvent->movementY;
    }

    /* rescale (in case canvas is being scaled)*/
    double client_w, client_h;
    emscripten_get_element_css_size(NULL, &client_w, &client_h);

    mx = mx * (window_data->window->w / (client_w * window_data->pixel_ratio));
    my = my * (window_data->window->h / (client_h * window_data->pixel_ratio));

    SDL_SendMouseMotion(window_data->window, 0, pointerlock_status.isActive, mx, my);
    return 0;
}

EM_BOOL
Emscripten_HandleMouseButton(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    uint32_t sdl_button;
    switch (mouseEvent->button) {
        case 0:
            sdl_button = SDL_BUTTON_LEFT;
            break;
        case 1:
            sdl_button = SDL_BUTTON_MIDDLE;
            break;
        case 2:
            sdl_button = SDL_BUTTON_RIGHT;
            break;
        default:
            return 0;
    }
    SDL_SendMouseButton(window_data->window, 0, eventType == EMSCRIPTEN_EVENT_MOUSEDOWN ? SDL_PRESSED : SDL_RELEASED, sdl_button);
    return 1;
}

EM_BOOL
Emscripten_HandleMouseFocus(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    SDL_SendWindowEvent(window_data->window, eventType == EMSCRIPTEN_EVENT_MOUSEENTER ? SDL_WINDOWEVENT_ENTER : SDL_WINDOWEVENT_LEAVE, 0, 0);
    return 1;
}

EM_BOOL
Emscripten_HandleWheel(int eventType, const EmscriptenWheelEvent *wheelEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    SDL_SendMouseWheel(window_data->window, 0, wheelEvent->deltaX, -wheelEvent->deltaY, SDL_MOUSEWHEEL_NORMAL);
    return 1;
}

EM_BOOL
Emscripten_HandleFocus(int eventType, const EmscriptenFocusEvent *wheelEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    SDL_SendWindowEvent(window_data->window, eventType == EMSCRIPTEN_EVENT_FOCUS ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
    return 1;
}

EM_BOOL
Emscripten_HandleTouch(int eventType, const EmscriptenTouchEvent *touchEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    int i;

    SDL_TouchID deviceId = 1;
    if (SDL_AddTouch(deviceId, "") < 0) {
         return 0;
    }

    for (i = 0; i < touchEvent->numTouches; i++) {
        SDL_FingerID id;
        float x, y;

        if (!touchEvent->touches[i].isChanged)
            continue;

        id = touchEvent->touches[i].identifier;
        x = touchEvent->touches[i].canvasX / (float)window_data->windowed_width;
        y = touchEvent->touches[i].canvasY / (float)window_data->windowed_height;

        if (eventType == EMSCRIPTEN_EVENT_TOUCHMOVE) {
            SDL_SendTouchMotion(deviceId, id, x, y, 1.0f);
        } else if (eventType == EMSCRIPTEN_EVENT_TOUCHSTART) {
            SDL_SendTouch(deviceId, id, SDL_TRUE, x, y, 1.0f);
        } else {
            SDL_SendTouch(deviceId, id, SDL_FALSE, x, y, 1.0f);
        }
    }


    return 1;
}

EM_BOOL
Emscripten_HandleKey(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
    Uint32 scancode;

    /* .keyCode is deprecated, but still the most reliable way to get keys */
    if (keyEvent->keyCode < SDL_arraysize(emscripten_scancode_table)) {
        scancode = emscripten_scancode_table[keyEvent->keyCode];

        if (scancode != SDL_SCANCODE_UNKNOWN) {

            if (keyEvent->location == DOM_KEY_LOCATION_RIGHT) {
                switch (scancode) {
                    case SDL_SCANCODE_LSHIFT:
                        scancode = SDL_SCANCODE_RSHIFT;
                        break;
                    case SDL_SCANCODE_LCTRL:
                        scancode = SDL_SCANCODE_RCTRL;
                        break;
                    case SDL_SCANCODE_LALT:
                        scancode = SDL_SCANCODE_RALT;
                        break;
                    case SDL_SCANCODE_LGUI:
                        scancode = SDL_SCANCODE_RGUI;
                        break;
                }
            }
            SDL_SendKeyboardKey(eventType == EMSCRIPTEN_EVENT_KEYDOWN ?
                                SDL_PRESSED : SDL_RELEASED, scancode);
        }
    }

    /* if we prevent keydown, we won't get keypress
     * also we need to ALWAYS prevent backspace and tab otherwise chrome takes action and does bad navigation UX
     */
    return SDL_GetEventState(SDL_TEXTINPUT) != SDL_ENABLE || eventType != EMSCRIPTEN_EVENT_KEYDOWN
            || keyEvent->keyCode == 8 /* backspace */ || keyEvent->keyCode == 9 /* tab */;
}

EM_BOOL
Emscripten_HandleKeyPress(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
    char text[5];
    if (Emscripten_ConvertUTF32toUTF8(keyEvent->charCode, text)) {
        SDL_SendKeyboardText(text);
    }
    return 1;
}

EM_BOOL
Emscripten_HandleFullscreenChange(int eventType, const EmscriptenFullscreenChangeEvent *fullscreenChangeEvent, void *userData)
{
    /*make sure this is actually our element going fullscreen*/
    if(SDL_strcmp(fullscreenChangeEvent->id, "SDLFullscreenElement") != 0)
        return 0;

    SDL_WindowData *window_data = userData;
    if(fullscreenChangeEvent->isFullscreen)
    {
        SDL_bool is_desktop_fullscreen;
        window_data->window->flags |= window_data->requested_fullscreen_mode;

        if(!window_data->requested_fullscreen_mode)
            window_data->window->flags |= SDL_WINDOW_FULLSCREEN_DESKTOP; /*we didn't reqest fullscreen*/

        window_data->requested_fullscreen_mode = 0;

        is_desktop_fullscreen = (window_data->window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;

        /*update size*/
        if(window_data->window->flags & SDL_WINDOW_RESIZABLE || is_desktop_fullscreen)
        {
            emscripten_set_canvas_size(fullscreenChangeEvent->screenWidth, fullscreenChangeEvent->screenHeight);
            SDL_SendWindowEvent(window_data->window, SDL_WINDOWEVENT_RESIZED, fullscreenChangeEvent->screenWidth, fullscreenChangeEvent->screenHeight);
        }
        else
        {
            /*preserve ratio*/
            double w = window_data->window->w;
            double h = window_data->window->h;
            double factor = SDL_min(fullscreenChangeEvent->screenWidth / w, fullscreenChangeEvent->screenHeight / h);
            emscripten_set_element_css_size(NULL, w * factor, h * factor);
        }
    }
    else
    {
        EM_ASM({
            //un-reparent canvas (similar to Module.requestFullscreen)
            var canvas = Module['canvas'];
            if(canvas.parentNode.id == "SDLFullscreenElement") {
                var canvasContainer = canvas.parentNode;
                canvasContainer.parentNode.insertBefore(canvas, canvasContainer);
                canvasContainer.parentNode.removeChild(canvasContainer);
            }
        });
        double unscaled_w = window_data->windowed_width / window_data->pixel_ratio;
        double unscaled_h = window_data->windowed_height / window_data->pixel_ratio;
        emscripten_set_canvas_size(window_data->windowed_width, window_data->windowed_height);

        if (!window_data->external_size && window_data->pixel_ratio != 1.0f) {
            emscripten_set_element_css_size(NULL, unscaled_w, unscaled_h);
        }

        SDL_SendWindowEvent(window_data->window, SDL_WINDOWEVENT_RESIZED, unscaled_w, unscaled_h);

        window_data->window->flags &= ~FULLSCREEN_MASK;
    }

    return 0;
}

EM_BOOL
Emscripten_HandleResize(int eventType, const EmscriptenUiEvent *uiEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    if(window_data->window->flags & FULLSCREEN_MASK)
    {
        SDL_bool is_desktop_fullscreen = (window_data->window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;

        if(window_data->window->flags & SDL_WINDOW_RESIZABLE || is_desktop_fullscreen)
        {
            emscripten_set_canvas_size(uiEvent->windowInnerWidth * window_data->pixel_ratio, uiEvent->windowInnerHeight * window_data->pixel_ratio);
            SDL_SendWindowEvent(window_data->window, SDL_WINDOWEVENT_RESIZED, uiEvent->windowInnerWidth, uiEvent->windowInnerHeight);
        }
    }
    else
    {
        /* this will only work if the canvas size is set through css */
        if(window_data->window->flags & SDL_WINDOW_RESIZABLE)
        {
            double w = window_data->window->w;
            double h = window_data->window->h;

            if(window_data->external_size) {
                emscripten_get_element_css_size(NULL, &w, &h);
            }

            emscripten_set_canvas_size(w * window_data->pixel_ratio, h * window_data->pixel_ratio);

            /* set_canvas_size unsets this */
            if (!window_data->external_size && window_data->pixel_ratio != 1.0f) {
                emscripten_set_element_css_size(NULL, w, h);
            }

            SDL_SendWindowEvent(window_data->window, SDL_WINDOWEVENT_RESIZED, w, h);
        }
    }

    return 0;
}

EM_BOOL
Emscripten_HandleVisibilityChange(int eventType, const EmscriptenVisibilityChangeEvent *visEvent, void *userData)
{
    SDL_WindowData *window_data = userData;
    SDL_SendWindowEvent(window_data->window, visEvent->hidden ? SDL_WINDOWEVENT_HIDDEN : SDL_WINDOWEVENT_SHOWN, 0, 0);
    return 0;
}

void
Emscripten_RegisterEventHandlers(SDL_WindowData *data)
{
    /* There is only one window and that window is the canvas */
    emscripten_set_mousemove_callback("#canvas", data, 0, Emscripten_HandleMouseMove);

    emscripten_set_mousedown_callback("#canvas", data, 0, Emscripten_HandleMouseButton);
    emscripten_set_mouseup_callback("#canvas", data, 0, Emscripten_HandleMouseButton);

    emscripten_set_mouseenter_callback("#canvas", data, 0, Emscripten_HandleMouseFocus);
    emscripten_set_mouseleave_callback("#canvas", data, 0, Emscripten_HandleMouseFocus);

    emscripten_set_wheel_callback("#canvas", data, 0, Emscripten_HandleWheel);

    emscripten_set_focus_callback("#canvas", data, 0, Emscripten_HandleFocus);
    emscripten_set_blur_callback("#canvas", data, 0, Emscripten_HandleFocus);

    emscripten_set_touchstart_callback("#canvas", data, 0, Emscripten_HandleTouch);
    emscripten_set_touchend_callback("#canvas", data, 0, Emscripten_HandleTouch);
    emscripten_set_touchmove_callback("#canvas", data, 0, Emscripten_HandleTouch);
    emscripten_set_touchcancel_callback("#canvas", data, 0, Emscripten_HandleTouch);

    /* Keyboard events are awkward */
    const char *keyElement = SDL_GetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT);
    if (!keyElement) keyElement = "#window";

    emscripten_set_keydown_callback(keyElement, data, 0, Emscripten_HandleKey);
    emscripten_set_keyup_callback(keyElement, data, 0, Emscripten_HandleKey);
    emscripten_set_keypress_callback(keyElement, data, 0, Emscripten_HandleKeyPress);

    emscripten_set_fullscreenchange_callback("#document", data, 0, Emscripten_HandleFullscreenChange);

    emscripten_set_resize_callback("#window", data, 0, Emscripten_HandleResize);

    emscripten_set_visibilitychange_callback(data, 0, Emscripten_HandleVisibilityChange);
}

void
Emscripten_UnregisterEventHandlers(SDL_WindowData *data)
{
    /* only works due to having one window */
    emscripten_set_mousemove_callback("#canvas", NULL, 0, NULL);

    emscripten_set_mousedown_callback("#canvas", NULL, 0, NULL);
    emscripten_set_mouseup_callback("#canvas", NULL, 0, NULL);

    emscripten_set_mouseenter_callback("#canvas", NULL, 0, NULL);
    emscripten_set_mouseleave_callback("#canvas", NULL, 0, NULL);

    emscripten_set_wheel_callback("#canvas", NULL, 0, NULL);

    emscripten_set_focus_callback("#canvas", NULL, 0, NULL);
    emscripten_set_blur_callback("#canvas", NULL, 0, NULL);

    emscripten_set_touchstart_callback("#canvas", NULL, 0, NULL);
    emscripten_set_touchend_callback("#canvas", NULL, 0, NULL);
    emscripten_set_touchmove_callback("#canvas", NULL, 0, NULL);
    emscripten_set_touchcancel_callback("#canvas", NULL, 0, NULL);

    const char *target = SDL_GetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT);
    if (!target) {
        target = "#window";
    }

    emscripten_set_keydown_callback(target, NULL, 0, NULL);
    emscripten_set_keyup_callback(target, NULL, 0, NULL);

    emscripten_set_keypress_callback(target, NULL, 0, NULL);

    emscripten_set_fullscreenchange_callback("#document", NULL, 0, NULL);

    emscripten_set_resize_callback("#window", NULL, 0, NULL);

    emscripten_set_visibilitychange_callback(NULL, 0, NULL);
}

#endif /* SDL_VIDEO_DRIVER_EMSCRIPTEN */

/* vi: set ts=4 sw=4 expandtab: */
