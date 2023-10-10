/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

extern SDL_bool SetupVideoToolboxOutput(SDL_Renderer *renderer);
extern SDL_bool DisplayVideoToolboxFrame(SDL_Renderer *renderer, void *buffer, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH );
extern void CleanupVideoToolboxOutput();
