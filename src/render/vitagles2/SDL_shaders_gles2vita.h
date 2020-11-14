/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#ifndef SDL_shaders_gles2vita_h_
#define SDL_shaders_gles2vita_h_

#if SDL_VIDEO_RENDER_VITA_GLES2

typedef struct VITA_GLES2_ShaderInstance
{
    GLenum type;
    GLenum format;
    int length;
    const void *data;
} VITA_GLES2_ShaderInstance;

typedef struct VITA_GLES2_Shader
{
    int instance_count;
    const VITA_GLES2_ShaderInstance *instances[4];
} VITA_GLES2_Shader;

typedef enum
{
    VITA_GLES2_SHADER_VERTEX_DEFAULT,
    VITA_GLES2_SHADER_FRAGMENT_SOLID_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_ABGR_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_ARGB_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_BGR_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_RGB_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_YUV_JPEG_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT601_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT709_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV12_JPEG_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV12_BT601_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV12_BT709_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV21_JPEG_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT601_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT709_SRC,
    VITA_GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES_SRC
} VITA_GLES2_ShaderType;

#define VITA_GLES2_SOURCE_SHADER (GLenum)-1

const VITA_GLES2_Shader *VITA_GLES2_GetShader(VITA_GLES2_ShaderType type);

#endif /* SDL_VIDEO_RENDER_VITA_GLES2 */

#endif /* SDL_shaders_gles2vita_h_ */

/* vi: set ts=4 sw=4 expandtab: */
