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

#ifndef SDL_dynapi_dlopennote_h
#define SDL_dynapi_dlopennote_h

#define SDL_ELF_NOTE_DLOPEN_PRIORITY_REQUIRED    "required"
#define SDL_ELF_NOTE_DLOPEN_PRIORITY_RECOMMENDED "recommended"
#define SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED   "suggested"

#if defined(__ELF__) && defined(HAVE_DLOPEN_NOTES)

#define SDL_ELF_NOTE_DLOPEN_VENDOR "FDO"
#define SDL_ELF_NOTE_DLOPEN_TYPE 0x407c0c0aU

#define SDL_ELF_NOTE_INTERNAL2(json, variable_name)                 \
    __attribute__((aligned(4), used, section(".note.dlopen")))      \
    static const struct {                                           \
        struct {                                                    \
            Uint32 n_namesz;                                        \
            Uint32 n_descsz;                                        \
            Uint32 n_type;                                          \
        } nhdr;                                                     \
        char name[4];                                               \
        __attribute__((aligned(4))) char dlopen_json[sizeof(json)]; \
    } variable_name = {                                             \
        {                                                           \
             sizeof(SDL_ELF_NOTE_DLOPEN_VENDOR),                    \
             sizeof(json),                                          \
             SDL_ELF_NOTE_DLOPEN_TYPE                               \
        },                                                          \
        SDL_ELF_NOTE_DLOPEN_VENDOR,                                 \
        json                                                        \
    }

#define SDL_ELF_NOTE_INTERNAL(json, variable_name) \
    SDL_ELF_NOTE_INTERNAL2(json, variable_name)

#define SDL_SONAME_ARRAY1(N1) "[\"" N1 "\"]"
#define SDL_SONAME_ARRAY2(N1,N2) "[\"" N1 "\",\"" N2 "\"]"
#define SDL_SONAME_ARRAY3(N1,N2,N3) "[\"" N1 "\",\"" N2 "\",\"" N3 "\"]"
#define SDL_SONAME_ARRAY4(N1,N2,N3,N4) "[\"" N1 "\",\"" N2 "\",\"" N3 "\",\"" N4 "\"]"
#define SDL_SONAME_ARRAY5(N1,N2,N3,N4,N5) "[\"" N1 "\",\"" N2 "\",\"" N3 "\",\"" N4 "\",\"" N5 "\"]"
#define SDL_SONAME_ARRAY6(N1,N2,N3,N4,N5,N6) "[\"" N1 "\",\"" N2 "\",\"" N3 "\",\"" N4 "\",\"" N5 "\",\"" N6 "\"]"
#define SDL_SONAME_ARRAY7(N1,N2,N3,N4,N5,N6,N7) "[\"" N1 "\",\"" N2 "\",\"" N3 "\",\"" N4 "\",\"" N5 "\",\"" N6 "\",\"" N7 "\"]"
#define SDL_SONAME_ARRAY8(N1,N2,N3,N4,N5,N6,N7,N8) "[\"" N1 "\",\"" N2 "\",\"" N3 "\",\"" N4 "\",\"" N5 "\",\"" N6 "\",\"" N7 "\",\"" N8 "\"]"
#define SDL_SONAME_ARRAY_GET(N1,N2,N3,N4,N5,N6,N7,N8,NAME,...) NAME
#define SDL_SONAME_ARRAY(...) \
    SDL_SONAME_ARRAY_GET(__VA_ARGS__, \
         SDL_SONAME_ARRAY8, \
         SDL_SONAME_ARRAY7, \
         SDL_SONAME_ARRAY6, \
         SDL_SONAME_ARRAY5, \
         SDL_SONAME_ARRAY4, \
         SDL_SONAME_ARRAY3, \
         SDL_SONAME_ARRAY2, \
         SDL_SONAME_ARRAY1 \
    )(__VA_ARGS__)

// Create "unique" variable name using __LINE__,
// so creating elf notes on the same line is not supported
#define SDL_ELF_NOTE_JOIN2(A,B) A##B
#define SDL_ELF_NOTE_JOIN(A,B) SDL_ELF_NOTE_JOIN2(A,B)
#define SDL_ELF_NOTE_UNIQUE_NAME SDL_ELF_NOTE_JOIN(s_dlopen_note_, __LINE__)

#define SDL_ELF_NOTE_DLOPEN(feature, description, priority, ...) \
    SDL_ELF_NOTE_INTERNAL(                                       \
        "[{\"feature\":\"" feature                               \
        "\",\"description\":\"" description                      \
        "\",\"priority\":\"" priority                            \
        "\",\"soname\":" SDL_SONAME_ARRAY(__VA_ARGS__) "}]",     \
        SDL_ELF_NOTE_UNIQUE_NAME)

#else

#define SDL_ELF_NOTE_DLOPEN(...)

#endif

#endif
