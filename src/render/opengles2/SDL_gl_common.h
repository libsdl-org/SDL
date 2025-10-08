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

typedef struct
{
    SDL_Rect viewport;
    bool viewport_dirty;
    SDL_Texture *texture;
    SDL_Texture *target;
    SDL_BlendMode blend;
    bool cliprect_enabled_dirty;
    bool cliprect_enabled;
    bool cliprect_dirty;
    SDL_Rect cliprect;
    bool texturing;
    bool texturing_dirty;
    SDL_FColor clear_color;
    bool clear_color_dirty;
    int drawablew;
    int drawableh;

} GL_COMMON_DrawStateCache;

typedef struct GL_COMMON_FBOList GL_COMMON_FBOList;

struct GL_COMMON_FBOList
{
    Uint32 w, h;
    GLuint FBO;
    GL_COMMON_FBOList *next;
};

typedef struct
{
    GLuint texture;
} GL_COMMON_PaletteData;


