/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

#if (SDL_VIDEO_RENDER_D3D || SDL_VIDEO_RENDER_D3D11) && !SDL_RENDER_DISABLED
#include "SDL_stdinc.h"

#include "SDL_d3dmath.h"

/* Direct3D matrix math functions */

Float4X4 MatrixIdentity()
{
    Float4X4 m;
    SDL_zero(m);
    m._11 = 1.0f;
    m._22 = 1.0f;
    m._33 = 1.0f;
    m._44 = 1.0f;
    return m;
}

Float4X4 MatrixMultiply(Float4X4 M1, Float4X4 M2)
{
    Float4X4 m;
    m._11 = M1._11 * M2._11 + M1._12 * M2._21 + M1._13 * M2._31 + M1._14 * M2._41;
    m._12 = M1._11 * M2._12 + M1._12 * M2._22 + M1._13 * M2._32 + M1._14 * M2._42;
    m._13 = M1._11 * M2._13 + M1._12 * M2._23 + M1._13 * M2._33 + M1._14 * M2._43;
    m._14 = M1._11 * M2._14 + M1._12 * M2._24 + M1._13 * M2._34 + M1._14 * M2._44;
    m._21 = M1._21 * M2._11 + M1._22 * M2._21 + M1._23 * M2._31 + M1._24 * M2._41;
    m._22 = M1._21 * M2._12 + M1._22 * M2._22 + M1._23 * M2._32 + M1._24 * M2._42;
    m._23 = M1._21 * M2._13 + M1._22 * M2._23 + M1._23 * M2._33 + M1._24 * M2._43;
    m._24 = M1._21 * M2._14 + M1._22 * M2._24 + M1._23 * M2._34 + M1._24 * M2._44;
    m._31 = M1._31 * M2._11 + M1._32 * M2._21 + M1._33 * M2._31 + M1._34 * M2._41;
    m._32 = M1._31 * M2._12 + M1._32 * M2._22 + M1._33 * M2._32 + M1._34 * M2._42;
    m._33 = M1._31 * M2._13 + M1._32 * M2._23 + M1._33 * M2._33 + M1._34 * M2._43;
    m._34 = M1._31 * M2._14 + M1._32 * M2._24 + M1._33 * M2._34 + M1._34 * M2._44;
    m._41 = M1._41 * M2._11 + M1._42 * M2._21 + M1._43 * M2._31 + M1._44 * M2._41;
    m._42 = M1._41 * M2._12 + M1._42 * M2._22 + M1._43 * M2._32 + M1._44 * M2._42;
    m._43 = M1._41 * M2._13 + M1._42 * M2._23 + M1._43 * M2._33 + M1._44 * M2._43;
    m._44 = M1._41 * M2._14 + M1._42 * M2._24 + M1._43 * M2._34 + M1._44 * M2._44;
    return m;
}

Float4X4 MatrixScaling(float x, float y, float z)
{
    Float4X4 m;
    SDL_zero(m);
    m._11 = x;
    m._22 = y;
    m._33 = z;
    m._44 = 1.0f;
    return m;
}

Float4X4 MatrixTranslation(float x, float y, float z)
{
    Float4X4 m;
    SDL_zero(m);
    m._11 = 1.0f;
    m._22 = 1.0f;
    m._33 = 1.0f;
    m._44 = 1.0f;
    m._41 = x;
    m._42 = y;
    m._43 = z;
    return m;
}

Float4X4 MatrixRotationX(float r)
{
    float sinR = SDL_sinf(r);
    float cosR = SDL_cosf(r);
    Float4X4 m;
    SDL_zero(m);
    m._11 = 1.0f;
    m._22 = cosR;
    m._23 = sinR;
    m._32 = -sinR;
    m._33 = cosR;
    m._44 = 1.0f;
    return m;
}

Float4X4 MatrixRotationY(float r)
{
    float sinR = SDL_sinf(r);
    float cosR = SDL_cosf(r);
    Float4X4 m;
    SDL_zero(m);
    m._11 = cosR;
    m._13 = -sinR;
    m._22 = 1.0f;
    m._31 = sinR;
    m._33 = cosR;
    m._44 = 1.0f;
    return m;
}

Float4X4 MatrixRotationZ(float r)
{
    float sinR = SDL_sinf(r);
    float cosR = SDL_cosf(r);
    Float4X4 m;
    SDL_zero(m);
    m._11 = cosR;
    m._12 = sinR;
    m._21 = -sinR;
    m._22 = cosR;
    m._33 = 1.0f;
    m._44 = 1.0f;
    return m;

}

#endif /* (SDL_VIDEO_RENDER_D3D || SDL_VIDEO_RENDER_D3D11) && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
