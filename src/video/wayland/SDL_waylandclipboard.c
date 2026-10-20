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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_waylanddatamanager.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandclipboard.h"
#include "../SDL_clipboard_c.h"
#include "../../events/SDL_events_c.h"

static const char *const text_mime_types[] = {
    TEXT_MIME,
    "text/plain",
    "TEXT",
    "UTF8_STRING",
    "STRING"
};

bool Wayland_SetClipboardData(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = _this->internal;
    SDL_WaylandSeat *seat = video_data->last_implicit_grab_seat;
    bool result = false;

    // If no implicit grab is available yet, just attach it to the first available seat.
    if (!seat && !WAYLAND_wl_list_empty(&video_data->seat_list)) {
        seat = wl_container_of(video_data->seat_list.next, seat, link);
    }

    video_data->current_data_offer_seat = seat;

    if (seat && seat->data_device) {
        // Clear references to the clipboard held by other seats, as they are about to become invalid.
        SDL_WaylandSeat *s;
        wl_list_for_each (s, &video_data->seat_list, link) {
            if (s->data_device) {
                if (s != seat) {
                    Wayland_DataDeviceSetSelectionSource(s->data_device, NULL, 0, NULL, NULL, NULL, 0);
                }

                Wayland_DataDeviceDestroySelectionOffer(s->data_device, false);
            }
        }

        Wayland_DataDevice *data_device = seat->data_device;
        result = Wayland_DataDeviceSetSelectionSource(data_device, (const char *const *)_this->clipboard_mime_types, _this->num_clipboard_mime_types,
                                                      _this->clipboard_callback, _this->clipboard_cleanup, _this->clipboard_userdata, _this->clipboard_sequence);
    }

    return result;
}

void *Wayland_GetClipboardData(SDL_VideoDevice *_this, const char *mime_type, size_t *length)
{
    SDL_VideoData *video_data = _this->internal;
    SDL_WaylandSeat *seat = video_data->current_data_offer_seat;
    void *buffer = NULL;

    if (seat && seat->data_device) {
        Wayland_DataDevice *data_device = seat->data_device;
        if (Wayland_DataDeviceHasSelectionSource(data_device)) {
            buffer = SDL_GetInternalClipboardData(_this, mime_type, length);
        } else if (Wayland_DataDeviceSelectionOfferHasMIME(data_device, mime_type)) {
            buffer = Wayland_DataDeviceReceiveSelectionOffer(data_device, mime_type, length);
        }
    }

    return buffer;
}

bool Wayland_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{
    SDL_VideoData *video_data = _this->internal;
    SDL_WaylandSeat *seat = video_data->current_data_offer_seat;
    bool result = false;

    if (seat && seat->data_device) {
        Wayland_DataDevice *data_device = seat->data_device;
        if (Wayland_DataDeviceHasSelectionSource(data_device)) {
            result = SDL_HasInternalClipboardData(_this, mime_type);
        } else {
            result = Wayland_DataDeviceSelectionOfferHasMIME(data_device, mime_type);
        }
    }
    return result;
}

const char *const *Wayland_GetTextMimeTypes(SDL_VideoDevice *_this, size_t *num_mime_types)
{
    *num_mime_types = SDL_arraysize(text_mime_types);
    return text_mime_types;
}

bool Wayland_SetPrimarySelectionText(SDL_VideoDevice *_this, const char *text)
{
    SDL_VideoData *video_data = _this->internal;
    SDL_WaylandSeat *seat = video_data->last_implicit_grab_seat;
    bool result;

    // If no implicit grab is available yet, just attach it to the first available seat.
    if (!seat && !WAYLAND_wl_list_empty(&video_data->seat_list)) {
        seat = wl_container_of(video_data->seat_list.next, seat, link);
    }

    video_data->current_primary_selection_seat = seat;

    if (seat && seat->primary_selection_device) {
        // Clear references to the primary selection held by other seats, as they are about to become invalid.
        SDL_WaylandSeat *s;
        wl_list_for_each (s, &video_data->seat_list, link) {
            if (s->data_device) {
                if (s != seat) {
                    Wayland_PrimarySelectionDeviceSetText(s->primary_selection_device, NULL);
                }

                Wayland_PrimarySelectionDeviceDestroyOffer(s->primary_selection_device);
            }
        }
        result = Wayland_PrimarySelectionDeviceSetText(seat->primary_selection_device, text);
    } else {
        result = SDL_SetError("Primary selection not supported");
    }
    return result;
}

char *Wayland_GetPrimarySelectionText(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = _this->internal;
    SDL_WaylandSeat *seat = video_data->current_primary_selection_seat;
    char *text = NULL;
    size_t length = 0;

    if (seat && seat->primary_selection_device) {
        text = Wayland_PrimarySelectionDeviceGetText(seat->primary_selection_device, &length);
    }

    if (!text) {
        text = SDL_strdup("");
    }

    return text;
}

bool Wayland_HasPrimarySelectionText(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = _this->internal;
    SDL_WaylandSeat *seat = video_data->current_primary_selection_seat;

    if (seat && seat->primary_selection_device) {
        return Wayland_PrimarySelectionDeviceHasText(seat->primary_selection_device);
    }
    return false;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
