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

#ifdef SDL_VIDEO_DRIVER_OPENHARMONY

#include "../../events/SDL_events_c.h"

#include "SDL_openharmonykeyboard.h"

#include "../../core/openharmony/SDL_openharmony.h"

bool OPENHARMONY_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return true;
}

void OPENHARMONY_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    int input_type = 0;
    int cap_type = 0;
    if (SDL_HasProperty(props, SDL_PROP_TEXTINPUT_OPENHARMONY_INPUTTYPE_NUMBER)) {
        input_type = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTINPUT_OPENHARMONY_INPUTTYPE_NUMBER, 0);
    } else {
        switch (SDL_GetTextInputType(props)) {
            #define INPUTTYPE(sdl,oh) case SDL_TEXTINPUT_TYPE_##sdl: input_type = oh; break
            INPUTTYPE(TEXT, 0);
            INPUTTYPE(TEXT_NAME, 0);
            INPUTTYPE(TEXT_EMAIL, 5);
            INPUTTYPE(TEXT_USERNAME, 10);
            INPUTTYPE(TEXT_PASSWORD_HIDDEN, 7);  // !!! FIXME: this is _also_ PASSWORD_VISIBLE, but I think this doesn't dictate rendering...?
            INPUTTYPE(TEXT_PASSWORD_VISIBLE, 7);
            INPUTTYPE(NUMBER_PASSWORD_HIDDEN, 8); // !!! FIXME: this is _also_ NUMBER_PASSWORD_VISIBLE, but I think this doesn't dictate rendering...?
            INPUTTYPE(NUMBER_PASSWORD_VISIBLE, 8);
            #undef INPUTTYPE
            default: break;
        }

        switch (SDL_GetTextInputCapitalization(props)) {
            #define CAPTYPE(sdl,oh) case SDL_CAPITALIZE_##sdl: cap_type = oh; break
            CAPTYPE(NONE, 0);
            CAPTYPE(LETTERS, 3);
            CAPTYPE(WORDS, 2);
            CAPTYPE(SENTENCES, 1);
            #undef CAPTYPE
            default: break;
        }

        #if 0  // !!! FIXME
        if (SDL_GetTextInputAutocorrect(props)) {
            input_type |= (TYPE_TEXT_FLAG_AUTO_CORRECT | TYPE_TEXT_FLAG_AUTO_COMPLETE);
        }
        #endif

        if (SDL_GetTextInputMultiline(props)) {
            input_type = 1;  // sorry, this only works with basic TEXT type.
        }
    }
    SDL_OpenHarmonyShowScreenKeyboard(input_type, cap_type, &window->text_input_rect);
}

void OPENHARMONY_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_OpenHarmonyHideScreenKeyboard();
}

void OPENHARMONY_RestoreScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (_this->screen_keyboard_shown) {
        OPENHARMONY_ShowScreenKeyboard(_this, window, window->text_input_props);
    }
}

#endif // SDL_VIDEO_DRIVER_OPENHARMONY

