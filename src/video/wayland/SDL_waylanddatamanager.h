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

#ifndef SDL_waylanddatamanager_h_
#define SDL_waylanddatamanager_h_

typedef struct Wayland_DataDevice Wayland_DataDevice;
typedef struct Wayland_PrimarySelectionDevice Wayland_PrimarySelectionDevice;

#include "SDL_waylandevents_c.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"

#define TEXT_MIME            "text/plain;charset=utf-8"
#define FILE_MIME            "text/uri-list"
#define FILE_PORTAL_MIME     "application/vnd.portal.filetransfer"
#define SDL_DATA_ORIGIN_MIME "application/x-sdl3-source-id"

// Data device (clipboard and drag & drop)
extern bool Wayland_CreateDataDevice(Wayland_Seat *seat);
extern void Wayland_DestroyDataDevice(Wayland_DataDevice *data_device);
extern bool Wayland_HasSelectionSource(Wayland_DataDevice *data_device);
extern bool Wayland_SetSelectionSource(Wayland_DataDevice *data_device, const char *const *mime_types, size_t mime_count,
                                       SDL_ClipboardDataCallback data_callback, SDL_ClipboardCleanupCallback cleanup_callback, void *userdata, Uint32 sequence);
extern bool Wayland_HasSelectionOfferMIME(Wayland_DataDevice *data_device, const char *mime_type);
extern void *Wayland_ReceiveSelectionOffer(Wayland_DataDevice *data_device, const char *mime_type, size_t *length);
extern void Wayland_RemoveSelectionOffer(Wayland_DataDevice *data_device, bool clear);
extern void Wayland_RemoveWindowReferencesFromDataDevice(Wayland_DataDevice *data_device, SDL_Window *window);

// Primary selection
extern bool Wayland_CreatePrimarySelectionDevice(Wayland_Seat *seat);
extern void Wayland_DestroyPrimarySelectionDevice(Wayland_PrimarySelectionDevice *primary_selection_device);
extern bool Wayland_HasPrimarySelectionDeviceText(Wayland_PrimarySelectionDevice *primary_selection_device);
extern char *Wayland_GetPrimarySelectionDeviceText(Wayland_PrimarySelectionDevice *primary_selection_device, size_t *length);
extern bool Wayland_SetPrimarySelectionDeviceText(Wayland_PrimarySelectionDevice *primary_selection_device, const char *text);
extern void Wayland_RemovePrimarySelectionDeviceOffer(Wayland_PrimarySelectionDevice *primary_selection_device);

#endif // SDL_waylanddatamanager_h_
