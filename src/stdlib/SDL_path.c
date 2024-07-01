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
#include "SDL_internal.h"

/* This file contains portable path manipulation functions for SDL */

char *SDL_dirname(char *path) {
    size_t len_prefix = 0;
    size_t len_path;
    Sint64 pos_start_last_slash_group = -1;
    Sint64 pos_end_last_slash_group = -1;
    Sint64 pos_start_2nd_last_slash_group = -1;
    Sint64 pos_end_2nd_last_slash_group = -1;
#ifdef SDL_PLATFORM_WINDOWS
    char path_separator = '\0';
#else
    const char path_separator = '/';
#endif

    if (!path || path[0] == '\0') {
        return ".";
    }

#ifdef SDL_PLATFORM_WINDOWS
    if (SDL_isalpha(path[0]) && path[1] == ':') {
        len_prefix = 2;
    }
#endif

    len_path = len_prefix;

    /* Calculate string length and find slashes in one loop */
    for (;; len_path++) {
        char c = path[len_path];
        if (c == '\0') {
            break;
        }
#ifdef SDL_PLATFORM_WINDOWS
        if (c == '/' || c == '\\') {
#else
        if (c == '/') {
#endif
            if (pos_end_last_slash_group == -1) {
                pos_start_last_slash_group = len_path;
                pos_end_last_slash_group = len_path + 1;
#ifdef SDL_PLATFORM_WINDOWS
                if (path_separator == '\0') {
                    path_separator = c;
                }
#endif
            } else {
                pos_end_last_slash_group = len_path + 1;
            }
        } else if (pos_end_last_slash_group != -1) {
            pos_start_2nd_last_slash_group = pos_start_last_slash_group;
            pos_end_2nd_last_slash_group = pos_end_last_slash_group;
            pos_end_last_slash_group = -1;
        }
    }

    /* 1. If string is "//", skip steps 2 to 5. */
    if (!(len_path == len_prefix + 2 && pos_start_last_slash_group == len_prefix && pos_end_last_slash_group == len_prefix + 2)) {

        /* 2. If string consists entirely of slash characters, string shall be set to a single slash character. In this case, skip steps 3 to 8. */
        if (pos_start_last_slash_group == len_prefix && pos_end_last_slash_group == len_path) {
            SDL_assert(path[len_prefix] == '/' || path[len_prefix] == '\\');
            path[len_prefix + 1] =  '\0';
            return path;
        }

        /* 3. If there are any trailing slash characters in string, they shall be removed. */
        if (pos_end_last_slash_group == len_path) {
            len_path = pos_start_last_slash_group;
            pos_start_last_slash_group = pos_start_2nd_last_slash_group;
            pos_end_last_slash_group = pos_end_2nd_last_slash_group;
        }

        /* 4. If there are no slash characters remaining in string, string shall be set to a single period character. In this case, skip steps 5 to 8. */
        if (pos_start_last_slash_group == -1) {
            if (len_prefix != 0) {
                return path;
            }
            return ".";
        }

        /* 5. If there are any trailing non-slash characters in string, they shall be removed. */
        len_path = pos_end_last_slash_group;
    }

    /* 6. If the remaining string is //, it is implementation-defined whether steps 7 and 8 are skipped or processed. */
    if (len_prefix == 0 && len_path == len_prefix + 2 && pos_start_last_slash_group == len_prefix && pos_end_last_slash_group == len_prefix + 2) {
        return path;
    }

    /* 7. If there are any trailing slash characters in string, they shall be removed. */
    if (pos_start_last_slash_group != -1) {
        len_path = pos_start_last_slash_group;
    }

    /* 8. If the remaining string is empty, string shall be set to a single slash character. */
    if (len_path == len_prefix) {
        path[len_prefix] = path_separator;
        path[len_prefix + 1] = '\0';
        return path;
    }

    path[len_path] = '\0';
    return path;
}

char *SDL_basename(char *path) {
    size_t start_pos;
    size_t end_pos;
    size_t len_prefix;

    /* 1. If string is a null string, it is unspecified whether the resulting string is '.' or a null string.
     *    In either case, skip steps 2 through 6. */
    if (path == NULL || path[0] == '\0') {
        return ".";
    }

    len_prefix = 0;
#ifdef SDL_PLATFORM_WINDOWS
    if (SDL_isalpha(path[0]) && path[1] == ':') {
        len_prefix = 2;
        if (path[2] == '\0') {
            return ".";
        }
    }
#endif

    /* 2. If string is "//", it is implementation-defined whether steps 3 to 6 are skipped or processed. */

    /* 3. If string consists entirely of <slash> characters, string shall be set to a single <slash> character.
     *    In this case, skip steps 4 to 6. */
    end_pos = SDL_strlen(path);

    if (!end_pos) {
        return path;
    }

    for (; end_pos != len_prefix; --end_pos) {
        char c = path[end_pos - 1];
#ifdef SDL_PLATFORM_WINDOWS
        if (c != '/' && c != '\\') {
#else
        if (c != '/') {
#endif
            break;
        }
    }
    if (end_pos == len_prefix) {
        return "/";
    }

    /* 4. If there are any trailing <slash> characters in string, they shall be removed. */
    path[end_pos] = '\0';

    /* 5. If there are any <slash> characters remaining in string,
     *    the prefix of string up to and including the last <slash> character in string shall be removed. */
    for (start_pos = end_pos; start_pos != 0; --start_pos) {
        if (path[start_pos - 1] == '/' || path[start_pos - 1] == '\\') {
            break;
        }
    }

    /* 6. (does not apply) */

    return path + start_pos;
}
