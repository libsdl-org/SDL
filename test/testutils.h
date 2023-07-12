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

#ifndef TESTUTILS_H
#define TESTUTILS_H

#include "SDL.h"

SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char *file, SDL_bool transparent,
                         int *width_out, int *height_out);
char *GetNearbyFilename(const char *file);
char *GetResourceFilename(const char *user_specified, const char *def);

#endif
