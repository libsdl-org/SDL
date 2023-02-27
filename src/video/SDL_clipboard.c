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

#include "SDL_sysvideo.h"

int SDL_SetClipboardText(const char *text)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_bool is_string = SDL_TRUE;
    size_t text_len = 0;
    int retval = 0;

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set clipboard text");
    }

    if (text == NULL) {
        text = "";
    } else {
        text_len = SDL_strlen(text);
    }

    if (_this->SetClipboardText) {
        retval = _this->SetClipboardText(_this, text, text_len, is_string);
    } else {
        SDL_free(_this->clipboard_text);
        _this->clipboard_text = SDL_strdup(text);
        if (!_this->clipboard_text) {
            return SDL_Error(SDL_ENOMEM);
        }
    }
    return retval;
}

int SDL_SetClipboardTextBuffer(const char *text, size_t len)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_bool is_string;
    size_t text_len;
    int retval = 0;

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set clipboard text");
    }

    if (text == NULL) {
        text = "";
        text_len = 0;
        is_string = SDL_TRUE;
    } else {
        /* This ensures no embedded nuls */
        text_len = SDL_strnlen(text, len);
        is_string = (text_len < len);
    }

    /* Backends can rely on text_len being correct, and is_string being TRUE iff text is a string (zero-terminated) */
    if (_this->SetClipboardText) {
        if (_this->clipboard_accepts_buffer) {
             retval = _this->SetClipboardText(_this, text, text_len, is_string);
        } else {
            char *duped_text = SDL_strndup(text, text_len);
            if (duped_text) {
                retval = _this->SetClipboardText(_this, duped_text, text_len, SDL_TRUE);
                SDL_free(duped_text);
            } else {
                return SDL_Error(SDL_ENOMEM);
            }
        }
    } else {
        SDL_free(_this->clipboard_text);
        _this->clipboard_text = SDL_strndup(text, text_len);
        if (!_this->clipboard_text) {
            return SDL_Error(SDL_ENOMEM);
        }
    }
    return retval;
}

int SDL_SetPrimarySelectionText(const char *text)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    int retval = 0;

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set primary selection text");
    }

    if (text == NULL) {
        text = "";
    }
    if (_this->SetPrimarySelectionText) {
        retval = _this->SetPrimarySelectionText(_this, text);
    } else {
        SDL_free(_this->primary_selection_text);
        _this->primary_selection_text = SDL_strdup(text);
        if (!_this->primary_selection_text) {
            return SDL_Error(SDL_ENOMEM);
        }
    }
    return retval;
}

char *
SDL_GetClipboardText(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to get clipboard text");
        return SDL_strdup("");
    }

    if (_this->GetClipboardText) {
        return _this->GetClipboardText(_this);
    } else {
        const char *text = _this->clipboard_text;
        if (text == NULL) {
            text = "";
        }
        return SDL_strdup(text);
    }
}

char *
SDL_GetPrimarySelectionText(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to get primary selection text");
        return SDL_strdup("");
    }

    if (_this->GetPrimarySelectionText) {
        return _this->GetPrimarySelectionText(_this);
    } else {
        const char *text = _this->primary_selection_text;
        if (text == NULL) {
            text = "";
        }
        return SDL_strdup(text);
    }
}

SDL_bool
SDL_HasClipboardText(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to check clipboard text");
        return SDL_FALSE;
    }

    if (_this->HasClipboardText) {
        return _this->HasClipboardText(_this);
    } else {
        if (_this->clipboard_text && _this->clipboard_text[0] != '\0') {
            return SDL_TRUE;
        } else {
            return SDL_FALSE;
        }
    }
}

SDL_bool
SDL_HasPrimarySelectionText(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to check primary selection text");
        return SDL_FALSE;
    }

    if (_this->HasPrimarySelectionText) {
        return _this->HasPrimarySelectionText(_this);
    }

    if (_this->primary_selection_text && _this->primary_selection_text[0] != '\0') {
        return SDL_TRUE;
    }

    return SDL_FALSE;
}
