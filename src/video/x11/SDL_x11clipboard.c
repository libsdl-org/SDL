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

#ifdef SDL_VIDEO_DRIVER_X11

#include <limits.h> /* For INT_MAX */

#include "SDL_x11video.h"
#include "SDL_x11clipboard.h"
#include "../SDL_clipboard_c.h"
#include "../../events/SDL_events_c.h"

static const char *text_mime_types[] = {
    "text/plain;charset=utf-8",
    "text/plain",
    "TEXT",
    "UTF8_STRING",
    "STRING"
};

/* Get any application owned window handle for clipboard association */
static Window GetWindow(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;

    /* We create an unmapped window that exists just to manage the clipboard,
       since X11 selection data is tied to a specific window and dies with it.
       We create the window on demand, so apps that don't use the clipboard
       don't have to keep an unnecessary resource around. */
    if (data->clipboard_window == None) {
        Display *dpy = data->display;
        Window parent = RootWindow(dpy, DefaultScreen(dpy));
        XSetWindowAttributes xattr;
        data->clipboard_window = X11_XCreateWindow(dpy, parent, -10, -10, 1, 1, 0,
                                                   CopyFromParent, InputOnly,
                                                   CopyFromParent, 0, &xattr);
        X11_XFlush(data->display);
    }

    return data->clipboard_window;
}

static int SetSelectionData(SDL_VideoDevice *_this, Atom selection, SDL_ClipboardDataCallback callback,
                            void *userdata, const char **mime_types, size_t mime_count, Uint32 sequence)
{
    SDL_VideoData *videodata = _this->driverdata;
    Display *display = videodata->display;
    Window window;
    SDLX11_ClipboardData *clipboard;
    SDL_bool clipboard_owner = SDL_FALSE;

    window = GetWindow(_this);
    if (window == None) {
        return SDL_SetError("Couldn't find a window to own the selection");
    }

    if (selection == XA_PRIMARY) {
        clipboard = &videodata->primary_selection;
    } else {
        clipboard = &videodata->clipboard;
    }

    clipboard_owner = X11_XGetSelectionOwner(display, selection) == window;

    /* If we are cancelling our own data we need to clean it up */
    if (clipboard_owner && clipboard->sequence == 0) {
        SDL_free(clipboard->userdata);
    }

    clipboard->callback = callback;
    clipboard->userdata = userdata;
    clipboard->mime_types = mime_types;
    clipboard->mime_count = mime_count;
    clipboard->sequence = sequence;

    X11_XSetSelectionOwner(display, selection, window, CurrentTime);
    return 0;
}

static void *CloneDataBuffer(const void *buffer, size_t *len)
{
    void *clone = NULL;
    if (*len > 0 && buffer) {
        clone = SDL_malloc((*len)+sizeof(Uint32));
        if (clone) {
            SDL_memcpy(clone, buffer, *len);
            SDL_memset((Uint8 *)clone + *len, 0, sizeof(Uint32));
        }
    }
    return clone;
}

static void *GetSelectionData(SDL_VideoDevice *_this, Atom selection_type,
                              const char *mime_type, size_t *length)
{
    SDL_VideoData *videodata = _this->driverdata;
    Display *display = videodata->display;
    Window window;
    Window owner;
    Atom selection;
    Atom seln_type;
    int seln_format;
    unsigned long count;
    unsigned long overflow;
    Uint64 waitStart;
    Uint64 waitElapsed;

    SDLX11_ClipboardData *clipboard;
    void *data = NULL;
    unsigned char *src = NULL;
    Atom XA_MIME = X11_XInternAtom(display, mime_type, False);
    Atom XA_INCR = X11_XInternAtom(display, "INCR", False);

    *length = 0;

    /* Get the window that holds the selection */
    window = GetWindow(_this);
    owner = X11_XGetSelectionOwner(display, selection_type);
    if (owner == None) {
        /* This requires a fallback to ancient X10 cut-buffers. We will just skip those for now */
        data = NULL;
    } else if (owner == window) {
        owner = DefaultRootWindow(display);
        if (selection_type == XA_PRIMARY) {
            clipboard = &videodata->primary_selection;
        } else {
            clipboard = &videodata->clipboard;
        }

        if (clipboard->callback) {
            const void *clipboard_data = clipboard->callback(clipboard->userdata, mime_type, length);
            data = CloneDataBuffer(clipboard_data, length);
        }
    } else {
        /* Request that the selection owner copy the data to our window */
        owner = window;
        selection = X11_XInternAtom(display, "SDL_SELECTION", False);
        X11_XConvertSelection(display, selection_type, XA_MIME, selection, owner,
                              CurrentTime);

        /* When using synergy on Linux and when data has been put in the clipboard
           on the remote (Windows anyway) machine then selection_waiting may never
           be set to False. Time out after a while. */
        waitStart = SDL_GetTicks();
        videodata->selection_waiting = SDL_TRUE;
        while (videodata->selection_waiting) {
            SDL_PumpEvents();
            waitElapsed = SDL_GetTicks() - waitStart;
            /* Wait one second for a selection response. */
            if (waitElapsed > 1000) {
                videodata->selection_waiting = SDL_FALSE;
                SDL_SetError("Selection timeout");
                /* We need to set the selection text so that next time we won't
                   timeout, otherwise we will hang on every call to this function. */
                SetSelectionData(_this, selection_type, SDL_ClipboardTextCallback, NULL,
                                 text_mime_types, SDL_arraysize(text_mime_types), 0);
                data = NULL;
                *length = 0;
            }
        }

        if (X11_XGetWindowProperty(display, owner, selection, 0, INT_MAX / 4, False,
                                   XA_MIME, &seln_type, &seln_format, &count, &overflow, &src) == Success) {
            if (seln_type == XA_MIME) {
                *length = (size_t)count;
                data = CloneDataBuffer(src, length);
            } else if (seln_type == XA_INCR) {
                /* FIXME: Need to implement the X11 INCR protocol */
                /*SDL_Log("Need to implement the X11 INCR protocol");*/
            }
            X11_XFree(src);
        }
    }
    return data;
}

const char **X11_GetTextMimeTypes(SDL_VideoDevice *_this, size_t *num_mime_types)
{
    *num_mime_types = SDL_arraysize(text_mime_types);
    return text_mime_types;
}

int X11_SetClipboardData(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = _this->driverdata;
    Atom XA_CLIPBOARD = X11_XInternAtom(videodata->display, "CLIPBOARD", 0);
    if (XA_CLIPBOARD == None) {
        return SDL_SetError("Couldn't access X clipboard");
    }
    return SetSelectionData(_this, XA_CLIPBOARD, _this->clipboard_callback, _this->clipboard_userdata, (const char **)_this->clipboard_mime_types, _this->num_clipboard_mime_types, _this->clipboard_sequence);
}

void *X11_GetClipboardData(SDL_VideoDevice *_this, const char *mime_type, size_t *length)
{
    SDL_VideoData *videodata = _this->driverdata;
    Atom XA_CLIPBOARD = X11_XInternAtom(videodata->display, "CLIPBOARD", 0);
    if (XA_CLIPBOARD == None) {
        SDL_SetError("Couldn't access X clipboard");
        *length = 0;
        return NULL;
    }
    return GetSelectionData(_this, XA_CLIPBOARD, mime_type, length);
}

SDL_bool X11_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{
    size_t length;
    void *data;
    data = X11_GetClipboardData(_this, mime_type, &length);
    if (data) {
        SDL_free(data);
    }
    return length > 0;
}

int X11_SetPrimarySelectionText(SDL_VideoDevice *_this, const char *text)
{
    return SetSelectionData(_this, XA_PRIMARY, SDL_ClipboardTextCallback, SDL_strdup(text), text_mime_types, SDL_arraysize(text_mime_types), 0);
}

char *X11_GetPrimarySelectionText(SDL_VideoDevice *_this)
{
    size_t length;
    char *text = GetSelectionData(_this, XA_PRIMARY, text_mime_types[0], &length);
    if (!text) {
        text = SDL_strdup("");
    }
    return text;
}

SDL_bool X11_HasPrimarySelectionText(SDL_VideoDevice *_this)
{
    SDL_bool result = SDL_FALSE;
    char *text = X11_GetPrimarySelectionText(_this);
    if (text) {
        if (text[0] != '\0') {
            result = SDL_TRUE;
        }
        SDL_free(text);
    }
    return result;
}

void X11_QuitClipboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;
    if (data->primary_selection.sequence == 0) {
        SDL_free(data->primary_selection.userdata);
    }
    if (data->clipboard.sequence == 0) {
        SDL_free(data->clipboard.userdata);
    }
}

#endif /* SDL_VIDEO_DRIVER_X11 */
