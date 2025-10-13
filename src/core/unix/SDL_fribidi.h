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

#ifndef SDL_fribidi_h_
#define SDL_fribidi_h_

#ifdef HAVE_FRIBIDI_H
#include <sys/types.h> // for ssize_t
#include <fribidi.h>

typedef FriBidiStrIndex (*SDL_FriBidiUnicodeToCharset)(FriBidiCharSet, const FriBidiChar *, FriBidiStrIndex, char *);
typedef FriBidiStrIndex (*SDL_FriBidiCharsetToUnicode)(FriBidiCharSet, const char *, FriBidiStrIndex, FriBidiChar *);
typedef void (*SDL_FriBidiGetBidiTypes)(const FriBidiChar *, const FriBidiStrIndex, FriBidiCharType *);
typedef FriBidiParType (*SDL_FriBidiGetParDirection)(const FriBidiCharType *, const FriBidiStrIndex);
typedef FriBidiLevel (*SDL_FriBidiGetParEmbeddingLevels)(const FriBidiCharType *, const FriBidiStrIndex, FriBidiParType *, FriBidiLevel *);
typedef void (*SDL_FriBidiGetJoiningTypes)(const FriBidiChar *, const FriBidiStrIndex, FriBidiJoiningType *);
typedef void (*SDL_FriBidiJoinArabic)(const FriBidiCharType *, const FriBidiStrIndex, const FriBidiLevel *, FriBidiArabicProp *);
typedef void (*SDL_FriBidiShape)(FriBidiFlags flags, const FriBidiLevel *, const FriBidiStrIndex, FriBidiArabicProp *, FriBidiChar *str);
typedef FriBidiLevel (*SDL_FriBidiReorderLine)(FriBidiFlags flags, const FriBidiCharType *, const FriBidiStrIndex, const FriBidiStrIndex, const FriBidiParType, FriBidiLevel *, FriBidiChar *, FriBidiStrIndex *);

typedef struct SDL_FriBidi {
    SDL_SharedObject *lib;
    SDL_FriBidiUnicodeToCharset unicode_to_charset;
    SDL_FriBidiCharsetToUnicode charset_to_unicode;
    SDL_FriBidiGetBidiTypes get_bidi_types;
    SDL_FriBidiGetParDirection get_par_direction;
    SDL_FriBidiGetParEmbeddingLevels get_par_embedding_levels;
    SDL_FriBidiGetJoiningTypes get_joining_types;
    SDL_FriBidiJoinArabic join_arabic;
    SDL_FriBidiShape shape;
    SDL_FriBidiReorderLine reorder_line;
} SDL_FriBidi;

extern SDL_FriBidi *SDL_FriBidi_Create(void);
extern char *SDL_FriBidi_Process(SDL_FriBidi *fribidi, char *utf8, ssize_t utf8_len, bool shaping, FriBidiParType *out_par_type);
extern void SDL_FriBidi_Destroy(SDL_FriBidi *fribidi);

#endif // HAVE_FRIBIDI_H

#endif // SDL_fribidi_h_
