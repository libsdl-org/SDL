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

SDL_ClipboardData *
SDL_CreateClipboardData(void *data, size_t len, const char *mime_type)
{
    SDL_ClipboardData *cb = SDL_calloc(1, sizeof(SDL_ClipboardData));
    if (cb == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    cb->data = SDL_calloc(1, len);
    if (cb->data == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_memcpy(cb->data, data, len);

    cb->mime_type = SDL_strdup(mime_type);
    if (cb->mime_type == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    cb->size = len;

    return cb;
}

int
SDL_AddClipboardData(SDL_ClipboardData *cbdata, void *data, size_t len, const char *mime_type)
{
    SDL_ClipboardData *tail;

    if (cbdata == NULL) {
        return SDL_InvalidParamError("cbdata");
    }

    while (cbdata->next != NULL) {
        cbdata = cbdata->next;
    }

    tail = SDL_CreateClipboardData(data, len, mime_type);
    if (tail == NULL) {
        /* Error is set in the create function */
        return -1;
    }

    cbdata->next = tail;
    return 0;
}

void
SDL_DestroyClipboardData(SDL_ClipboardData *cbdata)
{
    SDL_ClipboardData *next;

    while (cbdata != NULL) {
        if (cbdata->data != NULL) {
            SDL_free(cbdata->data);
        }
        if (cbdata->mime_type != NULL) {
            SDL_free(cbdata->mime_type);
        }
        next = cbdata->next;
        SDL_free(cbdata);
        cbdata = next;
    }
}

int
SDL_SetClipboardData(SDL_ClipboardData *cbdata, SDL_bool free_resource)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    int status = 0;

    if (_this == NULL) {
        return SDL_SetError("Video subsystem must be initialized to set clipboard text");
    }

    if (_this->SetClipboardData) {
        status = _this->SetClipboardData(_this, cbdata);
    }

    if (free_resource == SDL_TRUE) {
        SDL_DestroyClipboardData(cbdata);
    }

    return status;
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
        return _this->SetClipboardText(_this, text);
    } else {
        SDL_free(_this->clipboard_text);
        _this->clipboard_text = SDL_strdup(text);
        return 0;
    }
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
        return _this->SetPrimarySelectionText(_this, text);
    } else {
        SDL_free(_this->primary_selection_text);
        _this->primary_selection_text = SDL_strdup(text);
        return 0;
    }
}

void *
SDL_GetClipboardData(size_t *length, const char *mime_type)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to get clipboard data");
        return NULL;
    }

    if (_this->GetClipboardData) {
        return _this->GetClipboardData(_this, length, mime_type);
    } else {
        return NULL;
    }
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
SDL_HasClipboardData(const char *mime_type)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this == NULL) {
        SDL_SetError("Video subsystem must be initialized to check clipboard data");
        return SDL_FALSE;
    }

    if (_this->HasClipboardData) {
        return _this->HasClipboardData(_this, mime_type);
    }
    return SDL_FALSE;
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
