/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>
  Copyright 2022 Collabora Ltd.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "testutils.h"

#ifdef SDL_PLATFORM_DOS
static const struct
{
    const char *longname;
    const char *shortname;
} names83_map[] = {
    { "unifont-15.1.05.hex", "UNIFONT.HEX" },
    { "unifont-15.1.05-license.txt", "UNIFONTL.TXT" },
    { "physaudiodev.png", "PHYSADEV.PNG" },
    { "logaudiodev.png", "LOGADEV.PNG" },
    { "audiofile.png", "AUDIOFIL.PNG" },
    { "soundboard.png", "SNDBRD.PNG" },
    { "soundboard_levels.png", "SNDLVL.PNG" },
    { "trashcan.png", "TRASHCAN.PNG" },
    { "msdf_font.png", "MSDFFONT.PNG" },
    { "msdf_font.csv", "MSDFFONT.CSV" },
    { "gamepad_front.png", "GP_FRONT.PNG" },
    { "gamepad_back.png", "GP_BACK.PNG" },
    { "gamepad_face_abxy.png", "GP_FABXY.PNG" },
    { "gamepad_face_axby.png", "GP_FAXBY.PNG" },
    { "gamepad_face_bayx.png", "GP_FBAYX.PNG" },
    { "gamepad_face_sony.png", "GP_FSONY.PNG" },
    { "gamepad_battery.png", "GP_BATT.PNG" },
    { "gamepad_battery_unknown.png", "GP_BATTX.PNG" },
    { "gamepad_battery_wired.png", "GP_BATTW.PNG" },
    { "gamepad_touchpad.png", "GP_TOUCH.PNG" },
    { "gamepad_button.png", "GP_BTN.PNG" },
    { "gamepad_button_small.png", "GP_BTNSM.PNG" },
    { "gamepad_button_background.png", "GP_BTNBG.PNG" },
    { "gamepad_axis.png", "GP_AXIS.PNG" },
    { "gamepad_axis_arrow.png", "GP_AXARW.PNG" },
    { "gamepad_wired.png", "GP_WIRED.PNG" },
    { "gamepad_wireless.png", "GP_WLESS.PNG" },
    { "sdl-test_round.png", "SDLROUND.PNG" },
    { NULL, NULL }
};

static const char *Map83Filename(const char *file)
{
    int i;
    for (i = 0; names83_map[i].longname; i++) {
        if (SDL_strcasecmp(file, names83_map[i].longname) == 0) {
            return names83_map[i].shortname;
        }
    }
    return file;
}
#endif

/**
 * Return the absolute path to def in the SDL_GetBasePath() if possible, or
 * the relative path to def on platforms that don't have a working
 * SDL_GetBasePath(). Free the result with SDL_free.
 *
 * Fails and returns NULL if out of memory.
 */
char *GetNearbyFilename(const char *file)
{
    const char *base = SDL_GetBasePath();
#ifdef SDL_PLATFORM_DOS
    file = Map83Filename(file);
#endif
    char *path;

    if (base) {
        SDL_IOStream *rw;

        if (SDL_asprintf(&path, "%s%s", base, file) < 0) {
            return NULL;
        }

        rw = SDL_IOFromFile(path, "rb");
        if (rw) {
            SDL_CloseIO(rw);
            return path;
        }

        /* Couldn't find the file in the base path */
        SDL_free(path);
    }

    return SDL_strdup(file);
}

/**
 * If user_specified is non-NULL, return a copy of it. Free with SDL_free.
 *
 * Otherwise, return the absolute path to def in the SDL_GetBasePath() if
 * possible, or the relative path to def on platforms that don't have a
 * working SDL_GetBasePath(). Free the result with SDL_free.
 *
 * Fails and returns NULL if out of memory.
 */
char *GetResourceFilename(const char *user_specified, const char *def)
{
    if (user_specified) {
        return SDL_strdup(user_specified);
    }
#ifdef SDL_PLATFORM_DOS
    def = Map83Filename(def);
#endif
    return GetNearbyFilename(def);
}

/**
 * Load the .png file whose name is file, from the SDL_GetBasePath() if
 * possible or the current working directory if not.
 *
 * If transparent is true, set the transparent colour from the top left pixel.
 *
 * If width_out is non-NULL, set it to the texture width.
 *
 * If height_out is non-NULL, set it to the texture height.
 */
SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char *file, bool transparent)
{
    SDL_Surface *temp = NULL;
    SDL_Texture *texture = NULL;
    char *path;

    path = GetNearbyFilename(file);

    if (path) {
        file = path;
    }

    temp = SDL_LoadSurface(file);
    if (!temp) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", file, SDL_GetError());
    } else {
        /* Set transparent pixel as the pixel at (0,0) */
        if (transparent) {
            if (SDL_GetSurfacePalette(temp)) {
                const Uint8 bpp = SDL_BITSPERPIXEL(temp->format);
                const Uint8 mask = (1 << bpp) - 1;
                if (SDL_PIXELORDER(temp->format) == SDL_BITMAPORDER_4321)
                    SDL_SetSurfaceColorKey(temp, true, (*(Uint8 *)temp->pixels) & mask);
                else
                    SDL_SetSurfaceColorKey(temp, true, ((*(Uint8 *)temp->pixels) >> (8 - bpp)) & mask);
            } else {
                switch (SDL_BITSPERPIXEL(temp->format)) {
                case 15:
                    SDL_SetSurfaceColorKey(temp, true,
                                    (*(Uint16 *)temp->pixels) & 0x00007FFF);
                    break;
                case 16:
                    SDL_SetSurfaceColorKey(temp, true, *(Uint16 *)temp->pixels);
                    break;
                case 24:
                    SDL_SetSurfaceColorKey(temp, true,
                                    (*(Uint32 *)temp->pixels) & 0x00FFFFFF);
                    break;
                case 32:
                    SDL_SetSurfaceColorKey(temp, true, *(Uint32 *)temp->pixels);
                    break;
                }
            }
        }

        texture = SDL_CreateTextureFromSurface(renderer, temp);
        if (!texture) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s", SDL_GetError());
        }
    }
    SDL_DestroySurface(temp);
    SDL_free(path);
    return texture;
}
