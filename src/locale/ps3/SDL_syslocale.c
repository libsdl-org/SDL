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

#include "../SDL_syslocale.h"
#include <SDL3/SDL_assert.h>

#include <sysutil/sysutil.h>

bool SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    int current_locale_int = 0;

    SDL_assert(buflen > 0);

    if (sysUtilGetSystemParamInt(SYSUTIL_SYSTEMPARAM_ID_LANG, &current_locale_int) < 0) {
        // Default to English if call failed
        SDL_strlcpy(buf, "en_US", buflen);
        return true;
    }
    switch(current_locale_int) {
        case SYSUTIL_LANG_JAPANESE:
            SDL_strlcpy(buf, "ja_JP", buflen);
            break;
        case SYSUTIL_LANG_ENGLISH_US:
            SDL_strlcpy(buf, "en_US", buflen);
            break;
        case SYSUTIL_LANG_FRENCH:
            SDL_strlcpy(buf, "fr_FR", buflen);
            break;
        case SYSUTIL_LANG_SPANISH:
            SDL_strlcpy(buf, "es_ES", buflen);
            break;
        case SYSUTIL_LANG_GERMAN:
            SDL_strlcpy(buf, "de_DE", buflen);
            break;
        case SYSUTIL_LANG_ITALIAN:
            SDL_strlcpy(buf, "it_IT", buflen);
            break;
        case SYSUTIL_LANG_DUTCH:
            SDL_strlcpy(buf, "nl_NL", buflen);
            break;
        case SYSUTIL_LANG_PORTUGUESE_PT:
            SDL_strlcpy(buf, "pt_PT", buflen);
            break;
        case SYSUTIL_LANG_RUSSIAN:
            SDL_strlcpy(buf, "ru_RU", buflen);
            break;
        case SYSUTIL_LANG_KOREAN:
            SDL_strlcpy(buf, "ko_KR", buflen);
            break;
        case SYSUTIL_LANG_CHINESE_T:
            SDL_strlcpy(buf, "zh_TW", buflen);
            break;
        case SYSUTIL_LANG_CHINESE_S:
            SDL_strlcpy(buf, "zh_CN", buflen);
            break;
        case SYSUTIL_LANG_FINNISH:
            SDL_strlcpy(buf, "fi_FI", buflen);
            break;
        case SYSUTIL_LANG_SWEDISH:
            SDL_strlcpy(buf, "sv_SE", buflen);
            break;
        case SYSUTIL_LANG_DANISH:
            SDL_strlcpy(buf, "da_DK", buflen);
            break;
        case SYSUTIL_LANG_NORWEGIAN:
            SDL_strlcpy(buf, "no_NO", buflen);
            break;
        case SYSUTIL_LANG_POLISH:
            SDL_strlcpy(buf, "pl_PL", buflen);
            break;
        case SYSUTIL_LANG_PORTUGUESE_BR:
            SDL_strlcpy(buf, "pt_BR", buflen);
            break;
        case SYSUTIL_LANG_ENGLISH_GB:
            SDL_strlcpy(buf, "en_GB", buflen);
            break;
        case SYSUTIL_LANG_TURKISH:
            SDL_strlcpy(buf, "tr_TR", buflen);
            break;
        default:
            SDL_strlcpy(buf, "en_US", buflen);
            break;
    }

    return true;
}

/* vi: set ts=4 sw=4 expandtab: */
