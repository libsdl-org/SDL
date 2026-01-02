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

#ifdef HAVE_FRIBIDI_H

#include "SDL_fribidi.h"

#ifdef SDL_FRIBIDI_DYNAMIC
SDL_ELF_NOTE_DLOPEN(
    "fribidi",
    "Bidirectional text support",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    SDL_FRIBIDI_DYNAMIC
)
#endif

SDL_FriBidi *SDL_FriBidi_Create(void)
{
    SDL_FriBidi *fribidi;

    fribidi = (SDL_FriBidi *)SDL_malloc(sizeof(SDL_FriBidi));
    if (!fribidi) {
        return NULL;
    }

#ifdef SDL_FRIBIDI_DYNAMIC
    #define SDL_FRIBIDI_LOAD_SYM(x, n, t) x = ((t)SDL_LoadFunction(fribidi->lib, n)); if (!x) { SDL_UnloadObject(fribidi->lib); SDL_free(fribidi); return NULL; }

    fribidi->lib = SDL_LoadObject(SDL_FRIBIDI_DYNAMIC);
    if (!fribidi->lib) {
        SDL_free(fribidi);
        return NULL;
    }

    SDL_FRIBIDI_LOAD_SYM(fribidi->unicode_to_charset, "fribidi_unicode_to_charset", SDL_FriBidiUnicodeToCharset);
    SDL_FRIBIDI_LOAD_SYM(fribidi->charset_to_unicode, "fribidi_charset_to_unicode", SDL_FriBidiCharsetToUnicode);
    SDL_FRIBIDI_LOAD_SYM(fribidi->get_bidi_types, "fribidi_get_bidi_types", SDL_FriBidiGetBidiTypes);
    SDL_FRIBIDI_LOAD_SYM(fribidi->get_par_direction, "fribidi_get_par_direction", SDL_FriBidiGetParDirection);
    SDL_FRIBIDI_LOAD_SYM(fribidi->get_par_embedding_levels, "fribidi_get_par_embedding_levels", SDL_FriBidiGetParEmbeddingLevels);
    SDL_FRIBIDI_LOAD_SYM(fribidi->get_joining_types, "fribidi_get_joining_types", SDL_FriBidiGetJoiningTypes);
    SDL_FRIBIDI_LOAD_SYM(fribidi->join_arabic, "fribidi_join_arabic", SDL_FriBidiJoinArabic);
    SDL_FRIBIDI_LOAD_SYM(fribidi->shape, "fribidi_shape", SDL_FriBidiShape);
    SDL_FRIBIDI_LOAD_SYM(fribidi->reorder_line, "fribidi_reorder_line", SDL_FriBidiReorderLine);
#else
    fribidi->unicode_to_charset = fribidi_unicode_to_charset;
    fribidi->charset_to_unicode = fribidi_charset_to_unicode;
    fribidi->get_bidi_types = fribidi_get_bidi_types;
    fribidi->get_par_direction = fribidi_get_par_direction;
    fribidi->get_par_embedding_levels = fribidi_get_par_embedding_levels;
    fribidi->get_joining_types = fribidi_get_joining_types;
    fribidi->join_arabic = fribidi_join_arabic;
    fribidi->shape = fribidi_shape;
    fribidi->reorder_line = fribidi_reorder_line;
#endif

    return fribidi;
}

char *SDL_FriBidi_Process(SDL_FriBidi *fribidi, char *utf8, ssize_t utf8_len, bool shaping, FriBidiParType *out_par_type)
{
    FriBidiCharType *types;
    FriBidiLevel *levels;
    FriBidiArabicProp *props;
    FriBidiChar *str;
    char *result;
    FriBidiStrIndex len;
    FriBidiLevel max_level;
    FriBidiLevel start;
    FriBidiLevel end;
    FriBidiParType direction;
    FriBidiParType str_direction;
    unsigned int i;
    unsigned int c;

    if (!fribidi || !utf8) {
        return NULL;
    }

    /* Convert to UTF32 */
    if (utf8_len < 0) {
        utf8_len = SDL_strlen(utf8);
    }
    str = SDL_calloc(SDL_utf8strnlen(utf8, utf8_len), sizeof(FriBidiChar));
    len = fribidi->charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, utf8, utf8_len, str);

    /* Setup various BIDI structures */
    direction = FRIBIDI_PAR_LTR;
    types = NULL;
    levels = NULL;
    props = SDL_calloc(len + 1, sizeof(FriBidiArabicProp));
    levels = SDL_calloc(len + 1, sizeof(FriBidiLevel));
    types = SDL_calloc(len + 1, sizeof(FriBidiCharType));

    /* Shape */
    fribidi->get_bidi_types(str, len, types);
    str_direction = fribidi->get_par_direction(types, len);
    max_level = fribidi->get_par_embedding_levels(types, len, &direction, levels);
    if (shaping) {
        fribidi->get_joining_types(str, len, props);
        fribidi->join_arabic(types, len, levels, props);
        fribidi->shape(FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAGS_ARABIC, levels, len, props, str);
    }

    /* BIDI */
    for (end = 0, start = 0; end < len; end++) {
        if (str[end] == '\n' || str[end] == '\r' || str[end] == '\f' || str[end] == '\v' || end == len - 1) {
            max_level = fribidi->reorder_line(FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAGS_ARABIC, types, end - start + 1, start, direction, levels, str, NULL);
            start = end + 1;
        }
    }

    /* Silence warning */
    (void)max_level;

    /* Remove fillers */
    for (i = 0, c = 0; i < len; i++) {
        if (str[i] != FRIBIDI_CHAR_FILL) {
            str[c++] = str[i];
        }
    }
    len = c;

    /* Convert back to UTF8 */
    result = SDL_malloc(len * 4 + 1);
    fribidi->unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, str, len, result);

    /* Cleanup */
    SDL_free(levels);
    SDL_free(props);
    SDL_free(types);

    /* Return */
    if (out_par_type) {
        *out_par_type = str_direction;
    }
    return result;
}

void SDL_FriBidi_Destroy(SDL_FriBidi *fribidi)
{
    if (!fribidi) {
        return;
    }

#ifdef SDL_FRIBIDI_DYNAMIC
    SDL_UnloadObject(fribidi->lib);
#endif

    SDL_free(fribidi);
}

#endif
