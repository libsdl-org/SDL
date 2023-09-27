/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#if !(defined(__WIN32__) || defined(__WINGDK__))

DECLSPEC void *SDLCALL SDL_GetRenderD3D9Device(SDL_Renderer *renderer); /* returns IDirect3DDevice9 * */
void *SDL_GetRenderD3D9Device(SDL_Renderer *renderer)
{
    (void)renderer;
    SDL_Unsupported();
    return NULL;
}

DECLSPEC void *SDLCALL SDL_GetRenderD3D11Device(SDL_Renderer *renderer); /* returns ID3D11Device * */
void *SDL_GetRenderD3D11Device(SDL_Renderer *renderer)
{
    (void)renderer;
    SDL_Unsupported();
    return NULL;
}

DECLSPEC void *SDLCALL SDL_RenderGetD3D12Device(SDL_Renderer *renderer); /* return ID3D12Device * */
void *SDL_RenderGetD3D12Device(SDL_Renderer *renderer)
{
    (void)renderer;
    SDL_Unsupported();
    return NULL;
}

#endif
