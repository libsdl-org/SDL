/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_oldnames.h
 *
 *  Definitions to ease transition from SDL2 code
 */

#ifndef SDL_oldnames_h_
#define SDL_oldnames_h_

#include <SDL3/SDL_platform.h>

/* The new function names are recommended, but if you want to have the
 * old names available while you are in the process of migrating code
 * to SDL3, you can define `SDL_ENABLE_OLD_NAMES` in your project.
 *
 * You can use https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_symbols.py to mass rename the symbols defined here in your codebase:
 *  rename_symbols.py --all-symbols source_code_path
 */
#ifdef SDL_ENABLE_OLD_NAMES

/* ##SDL_keycode.h */
#define KMOD_ALT SDL_KMOD_ALT
#define KMOD_CAPS SDL_KMOD_CAPS
#define KMOD_CTRL SDL_KMOD_CTRL
#define KMOD_GUI SDL_KMOD_GUI
#define KMOD_LALT SDL_KMOD_LALT
#define KMOD_LCTRL SDL_KMOD_LCTRL
#define KMOD_LGUI SDL_KMOD_LGUI
#define KMOD_LSHIFT SDL_KMOD_LSHIFT
#define KMOD_MODE SDL_KMOD_MODE
#define KMOD_NONE SDL_KMOD_NONE
#define KMOD_NUM SDL_KMOD_NUM
#define KMOD_RALT SDL_KMOD_RALT
#define KMOD_RCTRL SDL_KMOD_RCTRL
#define KMOD_RESERVED SDL_KMOD_RESERVED
#define KMOD_RGUI SDL_KMOD_RGUI
#define KMOD_RSHIFT SDL_KMOD_RSHIFT
#define KMOD_SCROLL SDL_KMOD_SCROLL
#define KMOD_SHIFT SDL_KMOD_SHIFT

/* ##SDL_platform.h */
#ifdef __IOS__
#define __IPHONEOS__ __IOS__
#endif
#ifdef __MACOS__
#define __MACOSX__ __MACOS__
#endif

/* ##SDL_rwops.h */
#define RW_SEEK_CUR SDL_RW_SEEK_CUR
#define RW_SEEK_END SDL_RW_SEEK_END
#define RW_SEEK_SET SDL_RW_SEEK_SET

#else /* !SDL_ENABLE_OLD_NAMES */

/* ##SDL_keycode.h */
#define KMOD_ALT KMOD_ALT_renamed_SDL_KMOD_ALT
#define KMOD_CAPS KMOD_CAPS_renamed_SDL_KMOD_CAPS
#define KMOD_CTRL KMOD_CTRL_renamed_SDL_KMOD_CTRL
#define KMOD_GUI KMOD_GUI_renamed_SDL_KMOD_GUI
#define KMOD_LALT KMOD_LALT_renamed_SDL_KMOD_LALT
#define KMOD_LCTRL KMOD_LCTRL_renamed_SDL_KMOD_LCTRL
#define KMOD_LGUI KMOD_LGUI_renamed_SDL_KMOD_LGUI
#define KMOD_LSHIFT KMOD_LSHIFT_renamed_SDL_KMOD_LSHIFT
#define KMOD_MODE KMOD_MODE_renamed_SDL_KMOD_MODE
#define KMOD_NONE KMOD_NONE_renamed_SDL_KMOD_NONE
#define KMOD_NUM KMOD_NUM_renamed_SDL_KMOD_NUM
#define KMOD_RALT KMOD_RALT_renamed_SDL_KMOD_RALT
#define KMOD_RCTRL KMOD_RCTRL_renamed_SDL_KMOD_RCTRL
#define KMOD_RESERVED KMOD_RESERVED_renamed_SDL_KMOD_RESERVED
#define KMOD_RGUI KMOD_RGUI_renamed_SDL_KMOD_RGUI
#define KMOD_RSHIFT KMOD_RSHIFT_renamed_SDL_KMOD_RSHIFT
#define KMOD_SCROLL KMOD_SCROLL_renamed_SDL_KMOD_SCROLL
#define KMOD_SHIFT KMOD_SHIFT_renamed_SDL_KMOD_SHIFT

/* ##SDL_platform.h */
#ifdef __IOS__
#define __IPHONEOS__ __IPHONEOS___renamed___IOS__
#endif
#ifdef __MACOS__
#define __MACOSX__ __MACOSX___renamed___MACOS__
#endif

/* ##SDL_rwops.h */
#define RW_SEEK_CUR RW_SEEK_CUR_renamed_SDL_RW_SEEK_CUR
#define RW_SEEK_END RW_SEEK_END_renamed_SDL_RW_SEEK_END
#define RW_SEEK_SET RW_SEEK_SET_renamed_SDL_RW_SEEK_SET

#endif /* SDL_ENABLE_OLD_NAMES */

#endif /* SDL_oldnames_h_ */
