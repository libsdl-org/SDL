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

#ifndef SDL_genericmessagebox_c_h_
#define SDL_genericmessagebox_c_h_

#include "SDL_internal.h"

#define MAX_BUTTONS 8 /* Maximum number of buttons supported */

#ifdef HAVE_LIBDONNELL_H

#include <donnell.h>

#define MIN_MESSAGE_BOX_SIZE_W 235 
#define MIN_MESSAGE_BOX_SIZE_H 99
#define MESSAGE_BOX_ICON_SIZE DONNELL_ICON_SIZE_32 
#define MESSAGE_BOX_TEXT_SIZE 14 
#define MESSAGE_BOX_TEXT_FONT DONNELL_FONT_OPTIONS_SANS_SERIF 
#define MESSAGE_BOX_ICON_PADDING_AMOUNT 13 
#define MESSAGE_BOX_TEXT_PADDING_AMOUNT_X MESSAGE_BOX_ICON_PADDING_AMOUNT 
#define MESSAGE_BOX_TEXT_PADDING_AMOUNT_Y 23
#define MESSAGE_BOX_BUTTON_PADDING_AMOUNT_X MESSAGE_BOX_TEXT_PADDING_AMOUNT_X 
#define MESSAGE_BOX_BUTTON_PADDING_AMOUNT_Y MESSAGE_BOX_TEXT_PADDING_AMOUNT_Y
#define MESSAGE_BOX_BUTTON_PADDING_AMOUNT_YM MESSAGE_BOX_ICON_PADDING_AMOUNT
#define MESSAGE_BOX_BUTTON_TEXT_PADDING_AMOUNT MESSAGE_BOX_TEXT_PADDING_AMOUNT_X 
#define MESSAGE_BOX_BUTTON_SIZE_H 27
#define MESSAGE_BOX_BUTTON_TEXT_SIZE 12 
#define MIN_MESSAGE_BOX_BUTTON_SIZE_W 62
#define MESSAGE_BOX_TEXT_COLOR 0
#define MESSAGE_BOX_BG_COLOR 199

typedef struct SDL_MessageBoxButtonDataGeneric
{
	DonnellRect button_rect;
	DonnellButtonState button_state;
} SDL_MessageBoxButtonDataGeneric;

typedef struct SDL_MessageBoxDataGeneric
{
	DonnellImageBuffer* buffer;
	DonnellPixel* text_color;
	DonnellPixel* bg_color;
	DonnellIcon* icon;
	int icon_index;
    SDL_MessageBoxButtonDataGeneric buttons[MAX_BUTTONS];
    const SDL_MessageBoxData *messageboxdata;
} SDL_MessageBoxDataGeneric;

SDL_MessageBoxDataGeneric* SDL_CreateGenericMessageBoxData(const SDL_MessageBoxData *messageboxdata, unsigned int scale);
void SDL_RenderGenericMessageBox(SDL_MessageBoxDataGeneric* data, SDL_bool only_buttons);
void SDL_DestroyGenericMessageBoxData(SDL_MessageBoxDataGeneric* data);
#else
/**
 * This function shows a message box using Zenity if libdonnell is not available.
*/
extern int SDL_ShowGenericMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid);
#endif 

#endif
