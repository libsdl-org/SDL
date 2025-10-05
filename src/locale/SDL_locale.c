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
#include "SDL_syslocale.h"

static SDL_Locale **build_locales_from_csv_string(char *csv, int *count)
{
    int i, num_locales;
    size_t slen;
    size_t alloclen;
    char *ptr;
    SDL_Locale *loc;
    SDL_Locale **result;

    if (count) {
        *count = 0;
    }

    while (csv && *csv && SDL_isspace(*csv)) {
        ++csv;
    }
    if (!csv || !*csv) {
        return NULL; // nothing to report
    }

    num_locales = 1; // at least one
    for (ptr = csv; *ptr; ptr++) {
        if (*ptr == ',') {
            num_locales++;
        }
    }

    slen = ((size_t)(ptr - csv)) + 1; // SDL_strlen(csv) + 1
    alloclen = ((num_locales + 1) * sizeof(SDL_Locale *)) + (num_locales * sizeof(SDL_Locale)) + slen;

    result = (SDL_Locale **)SDL_calloc(1, alloclen);
    if (!result) {
        return NULL; // oh well
    }
    loc = (SDL_Locale *)(result + (num_locales + 1));
    ptr = (char *)(loc + num_locales);
    SDL_memcpy(ptr, csv, slen);

    i = 0;
    result[i++] = loc;
    while (true) { // parse out the string
        while (SDL_isspace(*ptr)) {
            ptr++; // skip whitespace.
        }

        if (*ptr == '\0') {
            break;
        }
        loc->language = ptr++;
        while (true) {
            const char ch = *ptr;
            if (ch == '_') {
                *(ptr++) = '\0';
                loc->country = ptr;
            } else if (SDL_isspace(ch)) {
                *(ptr++) = '\0'; // trim ending whitespace and keep going.
            } else if (ch == ',') {
                *(ptr++) = '\0';
                loc++;
                result[i++] = loc;
                break;
            } else if (ch == '\0') {
                break;
            } else {
                ptr++; // just keep going, still a valid string
            }
        }
    }

    if (count) {
        *count = num_locales;
    }

    return result;
}

SDL_Locale **SDL_GetPreferredLocales(int *count)
{
    char locbuf[128]; // enough for 21 "xx_YY," language strings.
    const char *hint = SDL_GetHint(SDL_HINT_PREFERRED_LOCALES);
    if (hint) {
        SDL_strlcpy(locbuf, hint, sizeof(locbuf));
    } else {
        SDL_zeroa(locbuf);
        SDL_SYS_GetPreferredLocales(locbuf, sizeof(locbuf));
    }
    return build_locales_from_csv_string(locbuf, count);
}

SDL_LocaleDirection SDL_GetLocaleDirection(SDL_Locale *locale) 
{
    SDL_LocaleDirection dir;
    
    if (SDL_SYS_GetLocaleDirection(locale, &dir)) {
        return dir;
    }
    
    /* fallback implementation */    
    dir = SDL_LOCALE_DIRECTION_HORIZONTAL_LEFT_TO_RIGHT;

    if (!SDL_strcmp(locale->language, "ar")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
    
    if (!SDL_strcmp(locale->language, "fa") && locale->country) {
        if (!SDL_strcmp(locale->country, "AF")) {
            dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
        }
    }

    if (!SDL_strcmp(locale->language, "fa") && locale->country) {
        if (!SDL_strcmp(locale->country, "IR")) {
            dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
        }
    }
    
    if (!SDL_strcmp(locale->language, "he")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
    
    if (!SDL_strcmp(locale->language, "iw")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
    
    if (!SDL_strcmp(locale->language, "kd")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
    
    if (!SDL_strcmp(locale->language, "PK") && locale->country) {
        if (!SDL_strcmp(locale->country, "PK")) {
            dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
        }
    }
    
    if (!SDL_strcmp(locale->language, "ps")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
    
    if (!SDL_strcmp(locale->language, "ug")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
    
    if (!SDL_strcmp(locale->language, "ur")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }
        
    if (!SDL_strcmp(locale->language, "yi")) {
        dir = SDL_LOCALE_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    }

    if (!SDL_strcmp(locale->language, "mn") && locale->country) {
        if (!SDL_strcmp(locale->country, "MN")) {
            dir = SDL_LOCALE_DIRECTION_VERTICAL_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
        }
    }
        
    return dir;
}
