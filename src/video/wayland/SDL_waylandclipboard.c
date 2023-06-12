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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "../../events/SDL_events_c.h"
#include "SDL_waylanddatamanager.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandclipboard.h"

int Wayland_SetClipboardData(SDL_VideoDevice *_this, SDL_ClipboardDataCallback callback, size_t mime_count, const char **mime_types,
                             void *userdata)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandDataDevice *data_device = NULL;

    int status = 0;

    video_data = _this->driverdata;
    if (video_data->input != NULL && video_data->input->data_device != NULL) {
        data_device = video_data->input->data_device;

        if (callback && mime_types) {
            SDL_WaylandDataSource *source = Wayland_data_source_create(_this);
            Wayland_data_source_set_callback(source, callback, userdata, SDL_FALSE);

            status = Wayland_data_device_set_selection(data_device, source, mime_count, mime_types);
            if (status != 0) {
                Wayland_data_source_destroy(source);
            }
        } else {
            status = Wayland_data_device_clear_selection(data_device);
        }
    }

    return status;
}

#define TEXT_MIME_TYPES_LEN 5
static const char *text_mime_types[TEXT_MIME_TYPES_LEN] = {
    TEXT_MIME,
    "text/plain",
    "TEXT",
    "UTF8_STRING",
    "STRING",
};

static void *Wayland_ClipboardTextCallback(size_t *length, const char *mime_type, void *userdata)
{
    void *data = NULL;
    SDL_bool valid_mime_type = SDL_FALSE;
    *length = 0;

    if (userdata == NULL) {
        return data;
    }

    for (size_t i = 0; i < TEXT_MIME_TYPES_LEN; ++i) {
        if (SDL_strcmp(mime_type, text_mime_types[i]) == 0) {
            valid_mime_type = SDL_TRUE;
            break;
        }
    }

    if (valid_mime_type) {
        char *text = userdata;
        *length = SDL_strlen(text);
        data = userdata;
    }

    return data;
}

int Wayland_SetClipboardText(SDL_VideoDevice *_this, const char *text)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandDataDevice *data_device = NULL;

    int status = 0;

    if (_this == NULL || _this->driverdata == NULL) {
        status = SDL_SetError("Video driver uninitialized");
    } else {
        video_data = _this->driverdata;
        if (video_data->input != NULL && video_data->input->data_device != NULL) {
            data_device = video_data->input->data_device;

            if (text[0] != '\0') {
                SDL_WaylandDataSource *source = Wayland_data_source_create(_this);
                Wayland_data_source_set_callback(source, Wayland_ClipboardTextCallback, SDL_strdup(text), SDL_TRUE);

                status = Wayland_data_device_set_selection(data_device, source, TEXT_MIME_TYPES_LEN, text_mime_types);
                if (status != 0) {
                    Wayland_data_source_destroy(source);
                }
            } else {
                status = Wayland_data_device_clear_selection(data_device);
            }
        }
    }

    return status;
}

int Wayland_SetPrimarySelectionText(SDL_VideoDevice *_this, const char *text)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandPrimarySelectionDevice *primary_selection_device = NULL;

    int status = 0;

    if (_this == NULL || _this->driverdata == NULL) {
        status = SDL_SetError("Video driver uninitialized");
    } else {
        video_data = _this->driverdata;
        if (video_data->input != NULL && video_data->input->primary_selection_device != NULL) {
            primary_selection_device = video_data->input->primary_selection_device;
            if (text[0] != '\0') {
                SDL_WaylandPrimarySelectionSource *source = Wayland_primary_selection_source_create(_this);
                Wayland_primary_selection_source_set_callback(source, Wayland_ClipboardTextCallback, SDL_strdup(text));

                status = Wayland_primary_selection_device_set_selection(primary_selection_device,
                                                                        source,
                                                                        TEXT_MIME_TYPES_LEN,
                                                                        text_mime_types);
                if (status != 0) {
                    Wayland_primary_selection_source_destroy(source);
                }
            } else {
                status = Wayland_primary_selection_device_clear_selection(primary_selection_device);
            }
        }
    }

    return status;
}

void *Wayland_GetClipboardData(SDL_VideoDevice *_this, size_t *length, const char *mime_type)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandDataDevice *data_device = NULL;

    void *buffer = NULL;

    video_data = _this->driverdata;
    if (video_data->input != NULL && video_data->input->data_device != NULL) {
        data_device = video_data->input->data_device;
        if (data_device->selection_source != NULL) {
            buffer = Wayland_data_source_get_data(data_device->selection_source,
                    length, mime_type, SDL_FALSE);
        } else if (Wayland_data_offer_has_mime(
                    data_device->selection_offer, mime_type)) {
            buffer = Wayland_data_offer_receive(data_device->selection_offer,
                    length, mime_type, SDL_FALSE);
        }
    }

    return buffer;
}

char *Wayland_GetClipboardText(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandDataDevice *data_device = NULL;

    char *text = NULL;
    size_t length = 0;

    if (_this == NULL || _this->driverdata == NULL) {
        SDL_SetError("Video driver uninitialized");
    } else {
        video_data = _this->driverdata;
        if (video_data->input != NULL && video_data->input->data_device != NULL) {
            data_device = video_data->input->data_device;
            if (data_device->selection_source != NULL) {
                text = Wayland_data_source_get_data(data_device->selection_source, &length, TEXT_MIME, SDL_TRUE);
            } else if (Wayland_data_offer_has_mime(
                        data_device->selection_offer, TEXT_MIME)) {
                text = Wayland_data_offer_receive(data_device->selection_offer,
                                                  &length, TEXT_MIME, SDL_TRUE);
            }
        }
    }

    if (text == NULL) {
        text = SDL_strdup("");
    }

    return text;
}

char *Wayland_GetPrimarySelectionText(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandPrimarySelectionDevice *primary_selection_device = NULL;

    char *text = NULL;
    size_t length = 0;

    if (_this == NULL || _this->driverdata == NULL) {
        SDL_SetError("Video driver uninitialized");
    } else {
        video_data = _this->driverdata;
        if (video_data->input != NULL && video_data->input->primary_selection_device != NULL) {
            primary_selection_device = video_data->input->primary_selection_device;
            if (primary_selection_device->selection_source != NULL) {
                text = Wayland_primary_selection_source_get_data(primary_selection_device->selection_source, &length, TEXT_MIME, SDL_TRUE);
            } else if (Wayland_primary_selection_offer_has_mime(
                        primary_selection_device->selection_offer, TEXT_MIME)) {
                text = Wayland_primary_selection_offer_receive(primary_selection_device->selection_offer,
                                                               &length, TEXT_MIME, SDL_TRUE);
            }
        }
    }

    if (text == NULL) {
        text = SDL_strdup("");
    }

    return text;
}

static SDL_bool HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandDataDevice *data_device = NULL;

    SDL_bool result = SDL_FALSE;
    video_data = _this->driverdata;
    if (video_data->input != NULL && video_data->input->data_device != NULL) {
        data_device = video_data->input->data_device;
        if (data_device->selection_source != NULL) {
            size_t length = 0;
            char *buffer = Wayland_data_source_get_data(data_device->selection_source, &length, mime_type, SDL_TRUE);
            result = buffer != NULL;
            SDL_free(buffer);
        } else {
            result = Wayland_data_offer_has_mime(data_device->selection_offer, mime_type);
        }
    }
    return result;
}

SDL_bool Wayland_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{
    return HasClipboardData(_this, mime_type);
}

SDL_bool Wayland_HasClipboardText(SDL_VideoDevice *_this)
{
    return HasClipboardData(_this, TEXT_MIME);
}

SDL_bool Wayland_HasPrimarySelectionText(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandPrimarySelectionDevice *primary_selection_device = NULL;

    SDL_bool result = SDL_FALSE;
    if (_this == NULL || _this->driverdata == NULL) {
        SDL_SetError("Video driver uninitialized");
    } else {
        video_data = _this->driverdata;
        if (video_data->input != NULL && video_data->input->primary_selection_device != NULL) {
            primary_selection_device = video_data->input->primary_selection_device;
            if (primary_selection_device->selection_source != NULL) {
                size_t length = 0;
                char *buffer = Wayland_primary_selection_source_get_data(primary_selection_device->selection_source,
                                                                         &length, TEXT_MIME, SDL_TRUE);
                result = buffer != NULL;
                SDL_free(buffer);
            } else {
                result = Wayland_primary_selection_offer_has_mime(
                    primary_selection_device->selection_offer, TEXT_MIME);
            }
        }
    }
    return result;
}

void *Wayland_GetClipboardUserdata(SDL_VideoDevice *_this)
{
    SDL_VideoData *video_data = NULL;
    SDL_WaylandDataDevice *data_device = NULL;
    void *data = NULL;

    video_data = _this->driverdata;
    if (video_data->input != NULL && video_data->input->data_device != NULL) {
        data_device = video_data->input->data_device;
        if (data_device->selection_source != NULL
                && data_device->selection_source->userdata.internal == SDL_FALSE) {
            data = data_device->selection_source->userdata.data;
        }
    }

    return data;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
