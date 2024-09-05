/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* These headers are generated with ../build-scripts/gen_checkstruct.py */
#include "checkstruct_align1.h"
#include "checkstruct_align4.h"
#include "checkstruct_align8.h"
#include "checkstruct_validate.h"

int main(int argc, char *argv[])
{
    return ValidatePadding() ? 0 : 1;
}
