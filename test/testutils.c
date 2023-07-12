/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>
  Copyright 2022 Collabora Ltd.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "testutils.h"

/*
 * Return the absolute path to def in the SDL_GetBasePath() if possible, or
 * the relative path to def on platforms that don't have a working
 * SDL_GetBasePath(). Free the result with SDL_free.
 *
 * Fails and returns NULL if out of memory.
 */
char *
GetNearbyFilename(const char *file)
{
    char *base;
    char *path;

    base = SDL_GetBasePath();

    if (base != NULL) {
        SDL_RWops *rw;
        size_t len = SDL_strlen(base) + SDL_strlen(file) + 1;

        path = SDL_malloc(len);

        if (path == NULL) {
            SDL_free(base);
            SDL_OutOfMemory();
            return NULL;
        }

        (void)SDL_snprintf(path, len, "%s%s", base, file);
        SDL_free(base);

        rw = SDL_RWFromFile(path, "rb");
        if (rw) {
            SDL_RWclose(rw);
            return path;
        }

        /* Couldn't find the file in the base path */
        SDL_free(path);
    }

    path = SDL_strdup(file);
    if (path == NULL) {
        SDL_OutOfMemory();
    }
    return path;
}

/*
 * If user_specified is non-NULL, return a copy of it. Free with SDL_free.
 *
 * Otherwise, return the absolute path to def in the SDL_GetBasePath() if
 * possible, or the relative path to def on platforms that don't have a
 * working SDL_GetBasePath(). Free the result with SDL_free.
 *
 * Fails and returns NULL if out of memory.
 */
char *
GetResourceFilename(const char *user_specified, const char *def)
{
    if (user_specified != NULL) {
        char *ret = SDL_strdup(user_specified);

        if (ret == NULL) {
            SDL_OutOfMemory();
        }

        return ret;
    } else {
        return GetNearbyFilename(def);
    }
}

/*
 * Load the .bmp file whose name is file, from the SDL_GetBasePath() if
 * possible or the current working directory if not.
 *
 * If transparent is true, set the transparent colour from the top left pixel.
 *
 * If width_out is non-NULL, set it to the texture width.
 *
 * If height_out is non-NULL, set it to the texture height.
 */
SDL_Texture *
LoadTexture(SDL_Renderer *renderer, const char *file, SDL_bool transparent,
            int *width_out, int *height_out)
{
    SDL_Surface *temp = NULL;
    SDL_Texture *texture = NULL;
    char *path;

    path = GetNearbyFilename(file);

    if (path != NULL) {
        file = path;
    }

    temp = SDL_LoadBMP(file);
    if (temp == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", file, SDL_GetError());
    } else {
        /* Set transparent pixel as the pixel at (0,0) */
        if (transparent) {
            if (temp->format->palette) {
                SDL_SetColorKey(temp, SDL_TRUE, *(Uint8 *)temp->pixels);
            } else {
                switch (temp->format->BitsPerPixel) {
                case 15:
                    SDL_SetColorKey(temp, SDL_TRUE,
                                    (*(Uint16 *)temp->pixels) & 0x00007FFF);
                    break;
                case 16:
                    SDL_SetColorKey(temp, SDL_TRUE, *(Uint16 *)temp->pixels);
                    break;
                case 24:
                    SDL_SetColorKey(temp, SDL_TRUE,
                                    (*(Uint32 *)temp->pixels) & 0x00FFFFFF);
                    break;
                case 32:
                    SDL_SetColorKey(temp, SDL_TRUE, *(Uint32 *)temp->pixels);
                    break;
                }
            }
        }

        if (width_out != NULL) {
            *width_out = temp->w;
        }

        if (height_out != NULL) {
            *height_out = temp->h;
        }

        texture = SDL_CreateTextureFromSurface(renderer, temp);
        if (texture == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s\n", SDL_GetError());
        }
    }
    SDL_FreeSurface(temp);
    if (path) {
        SDL_free(path);
    }
    return texture;
}
