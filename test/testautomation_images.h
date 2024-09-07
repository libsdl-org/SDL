/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
extern SDL_Surface *SDLTest_ImageBlit(void);
extern SDL_Surface *SDLTest_ImageBlitTiled(void);
extern SDL_Surface *SDLTest_ImageBlitColor(void);
extern SDL_Surface *SDLTest_ImageFace(void);
extern SDL_Surface *SDLTest_ImagePrimitives(void);
extern SDL_Surface *SDLTest_ImageBlendingBackground(void);
extern SDL_Surface *SDLTest_ImageBlendingSprite(void);
extern SDL_Surface *SDLTest_ImageWrappingSprite(void);
