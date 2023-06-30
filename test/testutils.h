/*
Copyright 1997-2023 Sam Lantinga
Copyright 2022 Collabora Ltd.
SPDX-License-Identifier: Zlib
*/

#ifndef TESTUTILS_H
#define TESTUTILS_H

#include "SDL.h"

SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char *file, SDL_bool transparent,
                         int *width_out, int *height_out);
char *GetNearbyFilename(const char *file);
char *GetResourceFilename(const char *user_specified, const char *def);

#endif
