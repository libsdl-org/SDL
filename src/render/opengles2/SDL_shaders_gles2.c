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
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_OGL_ES2 && !SDL_RENDER_DISABLED

#include "SDL_video.h"
#include "SDL_opengles2.h"
#include "SDL_shaders_gles2.h"
#include "SDL_stdinc.h"

/*************************************************************************************************
 * Vertex/fragment shader source                                                                 *
 *************************************************************************************************/

static const Uint8 GLES2_VertexSrc_Default_[] = " \
    uniform mat4 u_projection; \
    attribute vec2 a_position; \
    attribute vec2 a_texCoord; \
    attribute float a_angle; \
    attribute vec2 a_center; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        float angle = radians(a_angle); \
        float c = cos(angle); \
        float s = sin(angle); \
        mat2 rotationMatrix = mat2(c, -s, s, c); \
        vec2 position = rotationMatrix * (a_position - a_center) + a_center; \
        v_texCoord = a_texCoord; \
        gl_Position = u_projection * vec4(position, 0.0, 1.0);\
        gl_PointSize = 1.0; \
    } \
";

static const Uint8 GLES2_FragmentSrc_SolidSrc_[] = " \
    precision mediump float; \
    uniform vec4 u_color; \
    \
    void main() \
    { \
        gl_FragColor = u_color; \
    } \
";

static const Uint8 GLES2_FragmentSrc_TextureABGRSrc_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        gl_FragColor = texture2D(u_texture, v_texCoord); \
        gl_FragColor *= u_modulation; \
    } \
";

/* ARGB to ABGR conversion */
static const Uint8 GLES2_FragmentSrc_TextureARGBSrc_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        vec4 abgr = texture2D(u_texture, v_texCoord); \
        gl_FragColor = abgr; \
        gl_FragColor.r = abgr.b; \
        gl_FragColor.b = abgr.r; \
        gl_FragColor *= u_modulation; \
    } \
";

/* RGB to ABGR conversion */
static const Uint8 GLES2_FragmentSrc_TextureRGBSrc_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        vec4 abgr = texture2D(u_texture, v_texCoord); \
        gl_FragColor = abgr; \
        gl_FragColor.r = abgr.b; \
        gl_FragColor.b = abgr.r; \
        gl_FragColor.a = 1.0; \
        gl_FragColor *= u_modulation; \
    } \
";

/* BGR to ABGR conversion */
static const Uint8 GLES2_FragmentSrc_TextureBGRSrc_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        vec4 abgr = texture2D(u_texture, v_texCoord); \
        gl_FragColor = abgr; \
        gl_FragColor.a = 1.0; \
        gl_FragColor *= u_modulation; \
    } \
";

/* YUV to ABGR conversion */
static const Uint8 GLES2_FragmentSrc_TextureYUVSrc_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform sampler2D u_texture_u; \
    uniform sampler2D u_texture_v; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        mediump vec3 yuv; \
        lowp vec3 rgb; \
        yuv.x = texture2D(u_texture,   v_texCoord).r; \
        yuv.y = texture2D(u_texture_u, v_texCoord).r - 0.5; \
        yuv.z = texture2D(u_texture_v, v_texCoord).r - 0.5; \
        rgb = mat3( 1,        1,       1, \
                    0,       -0.39465, 2.03211, \
                    1.13983, -0.58060, 0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
        gl_FragColor *= u_modulation; \
    } \
";

/* NV12 to ABGR conversion */
static const Uint8 GLES2_FragmentSrc_TextureNV12Src_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform sampler2D u_texture_u; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        mediump vec3 yuv; \
        lowp vec3 rgb; \
        yuv.x = texture2D(u_texture,   v_texCoord).r; \
        yuv.yz = texture2D(u_texture_u, v_texCoord).ra - 0.5; \
        rgb = mat3( 1,        1,       1, \
                    0,       -0.39465, 2.03211, \
                    1.13983, -0.58060, 0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
        gl_FragColor *= u_modulation; \
    } \
";

/* NV21 to ABGR conversion */
static const Uint8 GLES2_FragmentSrc_TextureNV21Src_[] = " \
    precision mediump float; \
    uniform sampler2D u_texture; \
    uniform sampler2D u_texture_u; \
    uniform vec4 u_modulation; \
    varying vec2 v_texCoord; \
    \
    void main() \
    { \
        mediump vec3 yuv; \
        lowp vec3 rgb; \
        yuv.x = texture2D(u_texture,   v_texCoord).r; \
        yuv.yz = texture2D(u_texture_u, v_texCoord).ar - 0.5; \
        rgb = mat3( 1,        1,       1, \
                    0,       -0.39465, 2.03211, \
                    1.13983, -0.58060, 0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
        gl_FragColor *= u_modulation; \
    } \
";

static const GLES2_ShaderInstance GLES2_VertexSrc_Default = {
    GL_VERTEX_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_VertexSrc_Default_),
    GLES2_VertexSrc_Default_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_SolidSrc = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_SolidSrc_),
    GLES2_FragmentSrc_SolidSrc_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureABGRSrc = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureABGRSrc_),
    GLES2_FragmentSrc_TextureABGRSrc_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureARGBSrc = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureARGBSrc_),
    GLES2_FragmentSrc_TextureARGBSrc_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureRGBSrc = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureRGBSrc_),
    GLES2_FragmentSrc_TextureRGBSrc_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureBGRSrc = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureBGRSrc_),
    GLES2_FragmentSrc_TextureBGRSrc_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureYUVSrc = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureYUVSrc_),
    GLES2_FragmentSrc_TextureYUVSrc_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureNV12Src = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureNV12Src_),
    GLES2_FragmentSrc_TextureNV12Src_
};

static const GLES2_ShaderInstance GLES2_FragmentSrc_TextureNV21Src = {
    GL_FRAGMENT_SHADER,
    GLES2_SOURCE_SHADER,
    sizeof(GLES2_FragmentSrc_TextureNV21Src_),
    GLES2_FragmentSrc_TextureNV21Src_
};


/*************************************************************************************************
 * Vertex/fragment shader definitions                                                            *
 *************************************************************************************************/

static GLES2_Shader GLES2_VertexShader_Default = {
    1,
    {
        &GLES2_VertexSrc_Default
    }
};

static GLES2_Shader GLES2_FragmentShader_SolidSrc = {
    1,
    {
        &GLES2_FragmentSrc_SolidSrc
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureABGRSrc = {
    1,
    {
        &GLES2_FragmentSrc_TextureABGRSrc
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureARGBSrc = {
    1,
    {
        &GLES2_FragmentSrc_TextureARGBSrc
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureRGBSrc = {
    1,
    {
        &GLES2_FragmentSrc_TextureRGBSrc
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureBGRSrc = {
    1,
    {
        &GLES2_FragmentSrc_TextureBGRSrc
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureYUVSrc = {
    1,
    {
        &GLES2_FragmentSrc_TextureYUVSrc
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureNV12Src = {
    1,
    {
        &GLES2_FragmentSrc_TextureNV12Src
    }
};

static GLES2_Shader GLES2_FragmentShader_TextureNV21Src = {
    1,
    {
        &GLES2_FragmentSrc_TextureNV21Src
    }
};


/*************************************************************************************************
 * Shader selector                                                                               *
 *************************************************************************************************/

const GLES2_Shader *GLES2_GetShader(GLES2_ShaderType type)
{
    switch (type) {
    case GLES2_SHADER_VERTEX_DEFAULT:
        return &GLES2_VertexShader_Default;
    case GLES2_SHADER_FRAGMENT_SOLID_SRC:
        return &GLES2_FragmentShader_SolidSrc;
    case GLES2_SHADER_FRAGMENT_TEXTURE_ABGR_SRC:
        return &GLES2_FragmentShader_TextureABGRSrc;
    case GLES2_SHADER_FRAGMENT_TEXTURE_ARGB_SRC:
        return &GLES2_FragmentShader_TextureARGBSrc;
    case GLES2_SHADER_FRAGMENT_TEXTURE_RGB_SRC:
        return &GLES2_FragmentShader_TextureRGBSrc;
    case GLES2_SHADER_FRAGMENT_TEXTURE_BGR_SRC:
        return &GLES2_FragmentShader_TextureBGRSrc;
    case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_SRC:
        return &GLES2_FragmentShader_TextureYUVSrc;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_SRC:
        return &GLES2_FragmentShader_TextureNV12Src;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_SRC:
        return &GLES2_FragmentShader_TextureNV21Src;
    default:
        return NULL;
    }
}

#endif /* SDL_VIDEO_RENDER_OGL_ES2 && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
