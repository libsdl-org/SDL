/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Defines some images for tests */

/**
 * Type for test images.
 */
typedef struct SDLTest_SurfaceImage_s {
  int width;
  int height;
  unsigned int bytes_per_pixel; /* 3:RGB, 4:RGBA */
  char *pixel_data;
} SDLTest_SurfaceImage_t;

/* Test images */
SDL_Surface *SDLTest_ImageBlit(void);
SDL_Surface *SDLTest_ImageBlitColor(void);
SDL_Surface *SDLTest_ImageBlitAlpha(void);
SDL_Surface *SDLTest_ImageBlitBlendAdd(void);
SDL_Surface *SDLTest_ImageBlitBlend(void);
SDL_Surface *SDLTest_ImageBlitBlendMod(void);
SDL_Surface *SDLTest_ImageBlitBlendNone(void);
SDL_Surface *SDLTest_ImageBlitBlendAll(void);
SDL_Surface *SDLTest_ImageFace(void);
SDL_Surface *SDLTest_ImagePrimitives(void);
SDL_Surface *SDLTest_ImagePrimitivesBlend(void);
