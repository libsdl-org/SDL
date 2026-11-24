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

#ifndef SDL_menu_h_
#define SDL_menu_h_

#include "./SDL_list.h"

typedef enum SDL_MenuItemType
{
    SDL_MENU_ITEM_TYPE_NORMAL,
    SDL_MENU_ITEM_TYPE_SEPERATOR,
    SDL_MENU_ITEM_TYPE_CHECKBOX
} SDL_MenuItemType;

typedef enum SDL_MenuItemFlags
{
    SDL_MENU_ITEM_FLAGS_NONE = 0,
    SDL_MENU_ITEM_FLAGS_DISABLED = 1 << 0,
    SDL_MENU_ITEM_FLAGS_CHECKED = 1 << 1,
    SDL_MENU_ITEM_FLAGS_BAR_ITEM = 1 << 2
} SDL_MenuItemFlags;

/* Do not create this struct directly, users of this structure like the DBUSMENU layer use extended versions of it which need to be allocated by specfic functions. */
/* This struct is meant to be in an SDL_List just like sub_menu */
typedef struct SDL_MenuItem
{
    /* Basic properties */
    const char *utf8;
    SDL_MenuItemType type;
    SDL_MenuItemFlags flags;

    /* Callback */
    void *cb_data;
    void (*cb)(struct SDL_MenuItem *, void *);

    /* Submenu, set to NULL if none */
    SDL_ListNode *sub_menu;

    /* User data slots */
    void *udata;
    void *udata2;
    void *udata3;
} SDL_MenuItem;

#endif // SDL_menu_h_
