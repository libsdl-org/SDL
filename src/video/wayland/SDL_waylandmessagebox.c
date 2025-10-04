/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_waylandmessagebox.h"
#include "SDL_waylandtoolkit.h"
#include "../../dialog/unix/SDL_zenitymessagebox.h"

typedef struct SDL_MessageBoxDataToolkit
{
	SDL_WaylandTextRenderer *text_renderer;

    const SDL_MessageBoxData *messageboxdata;
} SDL_MessageBoxDataToolkit;

bool Wayland_ShowToolkitMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID) {
	SDL_MessageBoxDataToolkit data;
	SDL_Color color;
	
	data.messageboxdata = messageboxdata;
	color.r = 255;
	color.g = 0;
	color.b = 255;
	color.a = 255;
	
	data.text_renderer = WaylandToolkit_CreateTextRenderer();
	WaylandToolkit_RenderText(data.text_renderer, (char *)messageboxdata->message, &color);
	WaylandToolkit_FreeTextRenderer(data.text_renderer);
	
	return true;
}

bool Wayland_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    // Are we trying to connect to or are currently in a Wayland session?
    if (!SDL_getenv("WAYLAND_DISPLAY")) {
        const char *session = SDL_getenv("XDG_SESSION_TYPE");
        if (session && SDL_strcasecmp(session, "wayland") != 0) {
            return SDL_SetError("Not on a wayland display");
        }
    }
    
	if (SDL_GetHintBoolean(SDL_HINT_VIDEO_WAYLAND_PREFER_TOOLKIT, false)) {
		return Wayland_ShowToolkitMessageBox(messageboxdata, buttonID);
    } else {
		bool ret;
		
		ret = SDL_Zenity_ShowMessageBox(messageboxdata, buttonID);
		if (!ret) {
			return Wayland_ShowToolkitMessageBox(messageboxdata, buttonID);		
		} else {
			return ret;
		}
	}     
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
