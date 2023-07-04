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

#include "SDL_clipboard_c.h"
#include "SDL_sysvideo.h"
#include "../events/SDL_clipboardevents_c.h"


void SDL_CancelClipboardData(Uint32 sequence)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    size_t i;

    if (sequence != _this->clipboard_sequence) {
        /* This clipboard data was already canceled */
        return;
    }

    SDL_SendClipboardCancelled(_this->clipboard_userdata);

    if (_this->clipboard_cleanup) {
        _this->clipboard_cleanup(_this->clipboard_userdata);
    }

    if (_this->clipboard_mime_types) {
        for (i = 0; i < _this->num_clipboard_mime_types; ++i) {
            SDL_free(_this->clipboard_mime_types[i]);
        }
        SDL_free(_this->clipboard_mime_types);
        _this->clipboard_mime_types = NULL;
        _this->num_clipboard_mime_types = 0;
    }

    _this->clipboard_callback = NULL;
    _this->clipboard_cleanup = NULL;
    _this->clipboard_userdata = NULL;
}

int SDL_SetClipboardData(SDL_ClipboardDataCallback callback, SDL_ClipboardCleanupCallback cleanup, void *userdata, const char **mime_types, size_t num_mime_types)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    size_t i;

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set clipboard text");
    }

    /* Parameter validation */
    if (!((callback && mime_types && num_mime_types > 0) ||
          (!callback && !mime_types && num_mime_types == 0))) {
        return SDL_SetError("Invalid parameters");
    }

    if (!callback && !_this->clipboard_callback) {
        /* Nothing to do, don't modify the system clipboard */
        return 0;
    }

    SDL_CancelClipboardData(_this->clipboard_sequence);

    ++_this->clipboard_sequence;
    if (!_this->clipboard_sequence) {
        _this->clipboard_sequence = 1;
    }
    _this->clipboard_callback = callback;
    _this->clipboard_cleanup = cleanup;
    _this->clipboard_userdata = userdata;

    if (mime_types && num_mime_types > 0) {
        size_t num_allocated = 0;

        _this->clipboard_mime_types = (char **)SDL_malloc(num_mime_types * sizeof(char *));
        if (_this->clipboard_mime_types) {
            for (i = 0; i < num_mime_types; ++i) {
                _this->clipboard_mime_types[i] = SDL_strdup(mime_types[i]);
                if (_this->clipboard_mime_types[i]) {
                    ++num_allocated;
                }
            }
        }
        if (num_allocated < num_mime_types) {
            SDL_ClearClipboardData();
            return SDL_OutOfMemory();
        }
        _this->num_clipboard_mime_types = num_mime_types;
    }

    if (_this->SetClipboardData) {
        if (_this->SetClipboardData(_this) < 0) {
            return -1;
        }
    }

    SDL_SendClipboardUpdate();
    return 0;
}

int SDL_ClearClipboardData(void)
{
    return SDL_SetClipboardData(NULL, NULL, NULL, NULL, 0);
}

int SDL_SetClipboardText(const char *text)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set clipboard text");
    }

    if (text == NULL) {
        text = "";
    }
    if (_this->SetClipboardText) {
        if (_this->SetClipboardText(_this, text) < 0) {
            return -1;
        }
    } else {
        SDL_free(_this->clipboard_text);
        _this->clipboard_text = SDL_strdup(text);
    }

    SDL_SendClipboardUpdate();
    return 0;
}

int SDL_SetPrimarySelectionText(const char *text)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set primary selection text");
    }

    if (text == NULL) {
        text = "";
    }
    if (_this->SetPrimarySelectionText) {
        if (_this->SetPrimarySelectionText(_this, text) < 0) {
            return -1;
        }
    } else {
        SDL_free(_this->primary_selection_text);
        _this->primary_selection_text = SDL_strdup(text);
    }

    SDL_SendClipboardUpdate();
    return 0;
}

void *SDL_GetClipboardData(const char *mime_type, size_t *size)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    void *data = NULL;

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to get clipboard data");
        return NULL;
    }

    if (!mime_type) {
        SDL_InvalidParamError("mime_type");
        return NULL;
    }
    if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    if (_this->GetClipboardData) {
        data = _this->GetClipboardData(_this, mime_type, size);
    } else if (_this->clipboard_callback) {
        const void *provided_data = _this->clipboard_callback(_this->clipboard_userdata, mime_type, size);
        if (provided_data) {
            /* Make a copy of it for the caller */
            data = SDL_malloc(*size);
            if (data) {
                SDL_memcpy(data, provided_data, *size);
            } else {
                SDL_OutOfMemory();
            }
        }
    }
    if (!data) {
        *size = 0;
    }
    return data;
}

char *SDL_GetClipboardText(void)
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

char *SDL_GetPrimarySelectionText(void)
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

SDL_bool SDL_HasClipboardData(const char *mime_type)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    size_t i;

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to check clipboard data");
        return SDL_FALSE;
    }

    if (!mime_type) {
        SDL_InvalidParamError("mime_type");
        return SDL_FALSE;
    }

    if (_this->HasClipboardData) {
        return _this->HasClipboardData(_this, mime_type);
    } else {
        for (i = 0; i < _this->num_clipboard_mime_types; ++i) {
            if (SDL_strcmp(mime_type, _this->clipboard_mime_types[i]) == 0) {
                return SDL_TRUE;
            }
        }
        return SDL_FALSE;
    }
}

SDL_bool SDL_HasClipboardText(void)
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

SDL_bool SDL_HasPrimarySelectionText(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to check primary selection text");
        return SDL_FALSE;
    }

    if (_this->HasPrimarySelectionText) {
        return _this->HasPrimarySelectionText(_this);
    } else {
        if (_this->primary_selection_text && _this->primary_selection_text[0] != '\0') {
            return SDL_TRUE;
        } else {
            return SDL_FALSE;
        }
    }
}
