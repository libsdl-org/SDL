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

#include <database/pasteboard/oh_pasteboard.h>
#include <database/pasteboard/oh_pasteboard_err_code.h>
#include <database/udmf/udmf.h>
#include <database/udmf/uds.h>

#include "SDL_openharmonyvideo.h"
#include "SDL_openharmonyclipboard.h"
#include "../../core/openharmony/SDL_openharmony.h"
#include "../../events/SDL_events_c.h"

static void PasteboardNotifyCallback(void *userdata, Pasteboard_NotifyType type)
{
    SDL_assert(userdata != NULL);
    SDL_VideoDevice *_this = (SDL_VideoDevice *) userdata;
    SDL_VideoData *data = _this->internal;

    if (data->clipboard_set) {
        data->clipboard_set = false;  // this was us, ignore it.
        return;
    }

    OH_Pasteboard *pasteboard = (OH_Pasteboard *) data->oh_pasteboard;
    SDL_assert(pasteboard != NULL);

    const char *mimetype = "text/plain";
    if (OH_Pasteboard_HasType(pasteboard, mimetype)) {
        const size_t slen = SDL_strlen(mimetype);
        const size_t allocationsize = (slen + 1) + (sizeof (char *) * 2);
        char **new_mime_types = SDL_AllocateTemporaryMemory(allocationsize);
        if (new_mime_types) {
            char *ptr = (char *)(new_mime_types + 2);
            SDL_strlcpy(ptr, mimetype, slen + 1);
            new_mime_types[0] = ptr;
            new_mime_types[1] = NULL;
            SDL_SendClipboardUpdate(false, new_mime_types, 1);
        }
    }
}

static void PasteboardFinalizeCallback(void *userdata)
{
    // do nothing.
}

void OPENHARMONY_InitClipboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    data->clipboard_set = false;

    OH_Pasteboard *pasteboard = OH_Pasteboard_Create();
    data->oh_pasteboard = pasteboard;

    OH_PasteboardObserver *observer = OH_PasteboardObserver_Create();
    data->oh_pasteboard_observer = observer;
    if (observer) {
        OH_PasteboardObserver_SetData(observer, _this, PasteboardNotifyCallback, PasteboardFinalizeCallback);
    }

    if (pasteboard && observer) {
        OH_Pasteboard_Subscribe(pasteboard, NOTIFY_LOCAL_DATA_CHANGE, observer);
    }
}

void OPENHARMONY_QuitClipboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    OH_PasteboardObserver *observer = (OH_PasteboardObserver *) data->oh_pasteboard_observer;
    OH_Pasteboard *pasteboard = (OH_Pasteboard *) data->oh_pasteboard;

    if (observer && pasteboard) {
        OH_Pasteboard_Unsubscribe(pasteboard, NOTIFY_LOCAL_DATA_CHANGE, observer);
    }

    if (observer) {
        OH_PasteboardObserver_Destroy(observer);
        data->oh_pasteboard_observer = NULL;
    }

    if (pasteboard) {
        OH_Pasteboard_Destroy(pasteboard);
        data->oh_pasteboard = NULL;
    }
}

const char *const *OPENHARMONY_GetTextMimeTypes(SDL_VideoDevice *_this, size_t *num_mime_types)
{
    static const char *const text_mime_types[] = { "text/plain" };
    *num_mime_types = SDL_arraysize(text_mime_types);
    return text_mime_types;
}

bool OPENHARMONY_SetClipboardText(SDL_VideoDevice *_this, const char *text)
{
    SDL_VideoData *data = _this->internal;
    OH_Pasteboard *pasteboard = (OH_Pasteboard *) data->oh_pasteboard;
    if (!pasteboard) {
        return SDL_SetError("Pasteboard not initialized");
    }

    OH_UdsPlainText *udspt = OH_UdsPlainText_Create();
    if (!udspt) {
        return SDL_SetError("Could not create OH_UdsPlainText instance");
    }
    OH_UdsPlainText_SetContent(udspt, text);

    OH_UdmfRecord *udmfrec = OH_UdmfRecord_Create();
    if (!udmfrec) {
        OH_UdsPlainText_Destroy(udspt);
        return SDL_SetError("Could not create OH_UdmfRecord instance");
    }
    OH_UdmfRecord_AddPlainText(udmfrec, udspt);

    OH_UdmfData *udmfdata = OH_UdmfData_Create();
    if (!udmfdata) {
        OH_UdmfRecord_Destroy(udmfrec);
        OH_UdsPlainText_Destroy(udspt);
        return SDL_SetError("Could not create OH_UdmfData instance");
    }
    OH_UdmfData_AddRecord(udmfdata, udmfrec);

    data->clipboard_set = true;
    const PASTEBOARD_ErrCode rc = OH_Pasteboard_SetData(pasteboard, udmfdata);

    OH_UdmfData_Destroy(udmfdata);
    OH_UdmfRecord_Destroy(udmfrec);
    OH_UdsPlainText_Destroy(udspt);

    if (rc != ERR_OK) {
        data->clipboard_set = true;
        return SDL_SetError("OH_Pasteboard_SetData failed: %d", (int) rc);
    }
    return true;
}

char *OPENHARMONY_GetClipboardText(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    OH_Pasteboard *pasteboard = (OH_Pasteboard *) data->oh_pasteboard;
    if (!pasteboard) {
        SDL_SetError("Pasteboard not initialized");
        return NULL;
    }

    char *retval = NULL;
    if (OH_Pasteboard_HasType(pasteboard, "text/plain")) {
        int rc = (int) ERR_OK;
        OH_UdmfData *udmfdata = OH_Pasteboard_GetData(pasteboard, &rc);
        if (!udmfdata) {
            SDL_SetError("Could not create OH_UdmfData instance %d", rc);
            return NULL;
        }

        OH_UdmfRecord *udmfrec = OH_UdmfData_GetRecord(udmfdata, 0); // 0 == first record in udmfdata
        if (!udmfrec) {
            OH_UdmfData_Destroy(udmfdata);
            SDL_SetError("Could not create OH_UdmfRecord instance");
            return NULL;
        }

        OH_UdsPlainText *udspt = OH_UdsPlainText_Create();
        if (!udspt) {
            OH_UdmfRecord_Destroy(udmfrec);
            OH_UdmfData_Destroy(udmfdata);
            SDL_SetError("Could not create OH_UdsPlainText instance");
            return NULL;
        }

        OH_UdmfRecord_GetPlainText(udmfrec, udspt);
        const char *utf8 = OH_UdsPlainText_GetContent(udspt);
        if (!utf8) {
            SDL_SetError("Could not get plaintext content");
        } else {
            retval = SDL_strdup(utf8);
        }

        OH_UdsPlainText_Destroy(udspt);
        OH_UdmfRecord_Destroy(udmfrec);
        OH_UdmfData_Destroy(udmfdata);
    }

    return retval;
}

bool OPENHARMONY_HasClipboardText(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    OH_Pasteboard *pasteboard = (OH_Pasteboard *) data->oh_pasteboard;
    if (!pasteboard) {
        return SDL_SetError("Pasteboard not initialized");
    }
    return OH_Pasteboard_HasType(pasteboard, "text/plain");
}

#endif // SDL_VIDEO_DRIVER_OPENHARMONY
