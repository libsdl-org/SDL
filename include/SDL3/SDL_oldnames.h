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

/* The new function names are recommended, but if you want to have the
 * old names available while you are in the process of migrating code
 * to SDL3, you can define `SDL_ENABLE_OLD_NAMES` in your project.
 *
 * You can use https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_symbols.py to mass rename the symbols defined here in your codebase:
 *  rename_symbols.py --all-symbols source_code_path
 */
#ifdef SDL_ENABLE_OLD_NAMES

/* ##SDL_platform.h */
#define __IPHONEOS__ __IOS__
#define __MACOSX__ __MACOS__

/* ##SDL_rwops.h */
#define RW_SEEK_CUR SDL_RW_SEEK_CUR
#define RW_SEEK_END SDL_RW_SEEK_END
#define RW_SEEK_SET SDL_RW_SEEK_SET

#else /* !SDL_ENABLE_OLD_NAMES */

/* ##SDL_platform.h */
#define __IPHONEOS__ __IPHONEOS___renamed___IOS__
#define __MACOSX__ __MACOSX___renamed___MACOS__

/* ##SDL_rwops.h */
#define RW_SEEK_CUR RW_SEEK_CUR_renamed_SDL_RW_SEEK_CUR
#define RW_SEEK_END RW_SEEK_END_renamed_SDL_RW_SEEK_END
#define RW_SEEK_SET RW_SEEK_SET_renamed_SDL_RW_SEEK_SET

#endif /* SDL_ENABLE_OLD_NAMES */

#endif /* SDL_oldnames_h_ */

/* vi: set ts=4 sw=4 expandtab: */
