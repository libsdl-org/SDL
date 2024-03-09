/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_RENDER_OGL_ES2

#include "../../video/SDL_sysvideo.h" /* For SDL_RecreateWindow */
#include <SDL3/SDL_opengles2.h>
#include "../SDL_sysrender.h"
#include "../../video/SDL_blit.h"
#include "../../video/SDL_pixels_c.h"
#include "SDL_shaders_gles2.h"

/* WebGL doesn't offer client-side arrays, so use Vertex Buffer Objects
   on Emscripten, which converts GLES2 into WebGL calls.
   In all other cases, attempt to use client-side arrays, as they tend to
   be dramatically faster when not batching, and about the same when
   we are. */
#ifdef SDL_PLATFORM_EMSCRIPTEN
#define USE_VERTEX_BUFFER_OBJECTS 1
#else
#define USE_VERTEX_BUFFER_OBJECTS 0
#endif

/* To prevent unnecessary window recreation,
 * these should match the defaults selected in SDL_GL_ResetAttributes
 */
#define RENDERER_CONTEXT_MAJOR 2
#define RENDERER_CONTEXT_MINOR 0

/*************************************************************************************************
 * Context structures                                                                            *
 *************************************************************************************************/

typedef struct GLES2_FBOList GLES2_FBOList;

struct GLES2_FBOList
{
    Uint32 w, h;
    GLuint FBO;
    GLES2_FBOList *next;
};

typedef struct GLES2_TextureData
{
    GLuint texture;
    SDL_bool texture_external;
    GLenum texture_type;
    GLenum pixel_format;
    GLenum pixel_type;
    void *pixel_data;
    int pitch;
#if SDL_HAVE_YUV
    /* YUV texture support */
    SDL_bool yuv;
    SDL_bool nv12;
    GLuint texture_v;
    GLuint texture_v_external;
    GLuint texture_u;
    GLuint texture_u_external;
#endif
    GLES2_FBOList *fbo;
} GLES2_TextureData;

typedef enum
{
    GLES2_ATTRIBUTE_POSITION = 0,
    GLES2_ATTRIBUTE_COLOR = 1,
    GLES2_ATTRIBUTE_TEXCOORD = 2,
} GLES2_Attribute;

typedef enum
{
    GLES2_UNIFORM_PROJECTION,
    GLES2_UNIFORM_TEXTURE,
    GLES2_UNIFORM_TEXTURE_U,
    GLES2_UNIFORM_TEXTURE_V,
    GLES2_UNIFORM_OFFSET,
    GLES2_UNIFORM_MATRIX,
    NUM_GLES2_UNIFORMS
} GLES2_Uniform;

static const char *GLES2_UniformNames[] = {
    "u_projection",
    "u_texture",
    "u_texture_u",
    "u_texture_v",
    "u_offset",
    "u_matrix"
};
SDL_COMPILE_TIME_ASSERT(GLES2_UniformNames, SDL_arraysize(GLES2_UniformNames) == NUM_GLES2_UNIFORMS);

typedef struct GLES2_ProgramCacheEntry
{
    GLuint id;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint uniform_locations[NUM_GLES2_UNIFORMS];
    GLfloat projection[4][4];
    const float *shader_params;
    struct GLES2_ProgramCacheEntry *prev;
    struct GLES2_ProgramCacheEntry *next;
} GLES2_ProgramCacheEntry;

typedef struct GLES2_ProgramCache
{
    int count;
    GLES2_ProgramCacheEntry *head;
    GLES2_ProgramCacheEntry *tail;
} GLES2_ProgramCache;

typedef enum
{
    GLES2_IMAGESOURCE_INVALID,
    GLES2_IMAGESOURCE_SOLID,
    GLES2_IMAGESOURCE_TEXTURE_ABGR,
    GLES2_IMAGESOURCE_TEXTURE_ARGB,
    GLES2_IMAGESOURCE_TEXTURE_RGB,
    GLES2_IMAGESOURCE_TEXTURE_BGR,
    GLES2_IMAGESOURCE_TEXTURE_YUV,
    GLES2_IMAGESOURCE_TEXTURE_NV12,
    GLES2_IMAGESOURCE_TEXTURE_NV21,
    GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES
} GLES2_ImageSource;

typedef struct
{
    SDL_Rect viewport;
    SDL_bool viewport_dirty;
    SDL_Texture *texture;
    SDL_Texture *target;
    SDL_BlendMode blend;
    SDL_bool cliprect_enabled_dirty;
    SDL_bool cliprect_enabled;
    SDL_bool cliprect_dirty;
    SDL_Rect cliprect;
    SDL_bool texturing;
    SDL_bool texturing_dirty;
    SDL_FColor clear_color;
    SDL_bool clear_color_dirty;
    int drawablew;
    int drawableh;
    GLES2_ProgramCacheEntry *program;
    const float *shader_params;
    GLfloat projection[4][4];
} GLES2_DrawStateCache;

typedef struct GLES2_RenderData
{
    SDL_GLContext context;

    SDL_bool debug_enabled;

    SDL_bool GL_EXT_blend_minmax_supported;

#define SDL_PROC(ret, func, params) ret (APIENTRY *func) params;
#include "SDL_gles2funcs.h"
#undef SDL_PROC
    GLES2_FBOList *framebuffers;
    GLuint window_framebuffer;

    GLuint shader_id_cache[GLES2_SHADER_COUNT];

    GLES2_ProgramCache program_cache;
    Uint8 clear_r, clear_g, clear_b, clear_a;

#if USE_VERTEX_BUFFER_OBJECTS
    GLuint vertex_buffers[8];
    size_t vertex_buffer_size[8];
    int current_vertex_buffer;
#endif

    GLES2_DrawStateCache drawstate;
    GLES2_ShaderIncludeType texcoord_precision_hint;
} GLES2_RenderData;

#define GLES2_MAX_CACHED_PROGRAMS 8

static const char *GL_TranslateError(GLenum error)
{
#define GL_ERROR_TRANSLATE(e) \
    case e:                   \
        return #e;
    switch (error) {
        GL_ERROR_TRANSLATE(GL_INVALID_ENUM)
        GL_ERROR_TRANSLATE(GL_INVALID_VALUE)
        GL_ERROR_TRANSLATE(GL_INVALID_OPERATION)
        GL_ERROR_TRANSLATE(GL_OUT_OF_MEMORY)
        GL_ERROR_TRANSLATE(GL_NO_ERROR)
    default:
        return "UNKNOWN";
    }
#undef GL_ERROR_TRANSLATE
}

static void GL_ClearErrors(SDL_Renderer *renderer)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;

    if (!data->debug_enabled) {
        return;
    }
    while (data->glGetError() != GL_NO_ERROR) {
        /* continue; */
    }
}

static int GL_CheckAllErrors(const char *prefix, SDL_Renderer *renderer, const char *file, int line, const char *function)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    int ret = 0;

    if (!data->debug_enabled) {
        return 0;
    }
    /* check gl errors (can return multiple errors) */
    for (;;) {
        GLenum error = data->glGetError();
        if (error != GL_NO_ERROR) {
            if (!prefix || prefix[0] == '\0') {
                prefix = "generic";
            }
            SDL_SetError("%s: %s (%d): %s %s (0x%X)", prefix, file, line, function, GL_TranslateError(error), error);
            ret = -1;
        } else {
            break;
        }
    }
    return ret;
}

#if 0
#define GL_CheckError(prefix, renderer)
#else
#define GL_CheckError(prefix, renderer) GL_CheckAllErrors(prefix, renderer, SDL_FILE, SDL_LINE, SDL_FUNCTION)
#endif

/*************************************************************************************************
 * Renderer state APIs                                                                           *
 *************************************************************************************************/

static int GLES2_LoadFunctions(GLES2_RenderData *data)
{
#ifdef SDL_VIDEO_DRIVER_UIKIT
#define __SDL_NOGETPROCADDR__
#elif defined(SDL_VIDEO_DRIVER_ANDROID)
#define __SDL_NOGETPROCADDR__
#endif

#if defined __SDL_NOGETPROCADDR__
#define SDL_PROC(ret, func, params) data->func = func;
#else
#define SDL_PROC(ret, func, params)                                                            \
    do {                                                                                       \
        data->func = (ret (APIENTRY *) params)SDL_GL_GetProcAddress(#func);                                             \
        if (!data->func) {                                                                     \
            return SDL_SetError("Couldn't load GLES2 function %s: %s", #func, SDL_GetError()); \
        }                                                                                      \
    } while (0);
#endif /* __SDL_NOGETPROCADDR__ */

#include "SDL_gles2funcs.h"
#undef SDL_PROC
    return 0;
}

static GLES2_FBOList *GLES2_GetFBO(GLES2_RenderData *data, Uint32 w, Uint32 h)
{
    GLES2_FBOList *result = data->framebuffers;
    while ((result) && ((result->w != w) || (result->h != h))) {
        result = result->next;
    }
    if (!result) {
        result = (GLES2_FBOList *)SDL_malloc(sizeof(GLES2_FBOList));
        result->w = w;
        result->h = h;
        data->glGenFramebuffers(1, &result->FBO);
        result->next = data->framebuffers;
        data->framebuffers = result;
    }
    return result;
}

static int GLES2_ActivateRenderer(SDL_Renderer *renderer)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;

    if (SDL_GL_GetCurrentContext() != data->context) {
        /* Null out the current program to ensure we set it again */
        data->drawstate.program = NULL;

        if (SDL_GL_MakeCurrent(renderer->window, data->context) < 0) {
            return -1;
        }
    }

    GL_ClearErrors(renderer);

    return 0;
}

static void GLES2_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;

    if (event->type == SDL_EVENT_WINDOW_MINIMIZED) {
        /* According to Apple documentation, we need to finish drawing NOW! */
        data->glFinish();
    }
}

static GLenum GetBlendFunc(SDL_BlendFactor factor)
{
    switch (factor) {
    case SDL_BLENDFACTOR_ZERO:
        return GL_ZERO;
    case SDL_BLENDFACTOR_ONE:
        return GL_ONE;
    case SDL_BLENDFACTOR_SRC_COLOR:
        return GL_SRC_COLOR;
    case SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR:
        return GL_ONE_MINUS_SRC_COLOR;
    case SDL_BLENDFACTOR_SRC_ALPHA:
        return GL_SRC_ALPHA;
    case SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA:
        return GL_ONE_MINUS_SRC_ALPHA;
    case SDL_BLENDFACTOR_DST_COLOR:
        return GL_DST_COLOR;
    case SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR:
        return GL_ONE_MINUS_DST_COLOR;
    case SDL_BLENDFACTOR_DST_ALPHA:
        return GL_DST_ALPHA;
    case SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA:
        return GL_ONE_MINUS_DST_ALPHA;
    default:
        return GL_INVALID_ENUM;
    }
}

static GLenum GetBlendEquation(SDL_BlendOperation operation)
{
    switch (operation) {
    case SDL_BLENDOPERATION_ADD:
        return GL_FUNC_ADD;
    case SDL_BLENDOPERATION_SUBTRACT:
        return GL_FUNC_SUBTRACT;
    case SDL_BLENDOPERATION_REV_SUBTRACT:
        return GL_FUNC_REVERSE_SUBTRACT;
    case SDL_BLENDOPERATION_MINIMUM:
        return GL_MIN_EXT;
    case SDL_BLENDOPERATION_MAXIMUM:
        return GL_MAX_EXT;
    default:
        return GL_INVALID_ENUM;
    }
}

static SDL_bool GLES2_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;

    SDL_BlendFactor srcColorFactor = SDL_GetBlendModeSrcColorFactor(blendMode);
    SDL_BlendFactor srcAlphaFactor = SDL_GetBlendModeSrcAlphaFactor(blendMode);
    SDL_BlendOperation colorOperation = SDL_GetBlendModeColorOperation(blendMode);
    SDL_BlendFactor dstColorFactor = SDL_GetBlendModeDstColorFactor(blendMode);
    SDL_BlendFactor dstAlphaFactor = SDL_GetBlendModeDstAlphaFactor(blendMode);
    SDL_BlendOperation alphaOperation = SDL_GetBlendModeAlphaOperation(blendMode);

    if (GetBlendFunc(srcColorFactor) == GL_INVALID_ENUM ||
        GetBlendFunc(srcAlphaFactor) == GL_INVALID_ENUM ||
        GetBlendEquation(colorOperation) == GL_INVALID_ENUM ||
        GetBlendFunc(dstColorFactor) == GL_INVALID_ENUM ||
        GetBlendFunc(dstAlphaFactor) == GL_INVALID_ENUM ||
        GetBlendEquation(alphaOperation) == GL_INVALID_ENUM) {
        return SDL_FALSE;
    }

    if (colorOperation == SDL_BLENDOPERATION_MINIMUM && !data->GL_EXT_blend_minmax_supported) {
        return SDL_FALSE;
    }
    if (colorOperation == SDL_BLENDOPERATION_MAXIMUM && !data->GL_EXT_blend_minmax_supported) {
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

static GLES2_ProgramCacheEntry *GLES2_CacheProgram(GLES2_RenderData *data, GLuint vertex, GLuint fragment)
{
    GLES2_ProgramCacheEntry *entry;
    GLint linkSuccessful;
    int i;

    /* Check if we've already cached this program */
    entry = data->program_cache.head;
    while (entry) {
        if (entry->vertex_shader == vertex && entry->fragment_shader == fragment) {
            break;
        }
        entry = entry->next;
    }
    if (entry) {
        if (data->program_cache.head != entry) {
            if (entry->next) {
                entry->next->prev = entry->prev;
            }
            if (entry->prev) {
                entry->prev->next = entry->next;
            }
            entry->prev = NULL;
            entry->next = data->program_cache.head;
            data->program_cache.head->prev = entry;
            data->program_cache.head = entry;
        }
        return entry;
    }

    /* Create a program cache entry */
    entry = (GLES2_ProgramCacheEntry *)SDL_calloc(1, sizeof(GLES2_ProgramCacheEntry));
    if (!entry) {
        return NULL;
    }
    entry->vertex_shader = vertex;
    entry->fragment_shader = fragment;

    /* Create the program and link it */
    entry->id = data->glCreateProgram();
    data->glAttachShader(entry->id, vertex);
    data->glAttachShader(entry->id, fragment);
    data->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_POSITION, "a_position");
    data->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_COLOR, "a_color");
    data->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_TEXCOORD, "a_texCoord");
    data->glLinkProgram(entry->id);
    data->glGetProgramiv(entry->id, GL_LINK_STATUS, &linkSuccessful);
    if (!linkSuccessful) {
        data->glDeleteProgram(entry->id);
        SDL_free(entry);
        SDL_SetError("Failed to link shader program");
        return NULL;
    }

    /* Predetermine locations of uniform variables */
    for (i = 0; i < NUM_GLES2_UNIFORMS; ++i) {
        entry->uniform_locations[i] = data->glGetUniformLocation(entry->id, GLES2_UniformNames[i]);
    }

    data->glUseProgram(entry->id);
    if (entry->uniform_locations[GLES2_UNIFORM_TEXTURE_V] != -1) {
        data->glUniform1i(entry->uniform_locations[GLES2_UNIFORM_TEXTURE_V], 2); /* always texture unit 2. */
    }
    if (entry->uniform_locations[GLES2_UNIFORM_TEXTURE_U] != -1) {
        data->glUniform1i(entry->uniform_locations[GLES2_UNIFORM_TEXTURE_U], 1); /* always texture unit 1. */
    }
    if (entry->uniform_locations[GLES2_UNIFORM_TEXTURE] != -1) {
        data->glUniform1i(entry->uniform_locations[GLES2_UNIFORM_TEXTURE], 0); /* always texture unit 0. */
    }
    if (entry->uniform_locations[GLES2_UNIFORM_PROJECTION] != -1) {
        data->glUniformMatrix4fv(entry->uniform_locations[GLES2_UNIFORM_PROJECTION], 1, GL_FALSE, (GLfloat *)entry->projection);
    }

    /* Cache the linked program */
    if (data->program_cache.head) {
        entry->next = data->program_cache.head;
        data->program_cache.head->prev = entry;
    } else {
        data->program_cache.tail = entry;
    }
    data->program_cache.head = entry;
    ++data->program_cache.count;

    /* Evict the last entry from the cache if we exceed the limit */
    if (data->program_cache.count > GLES2_MAX_CACHED_PROGRAMS) {
        data->glDeleteProgram(data->program_cache.tail->id);
        data->program_cache.tail = data->program_cache.tail->prev;
        if (data->program_cache.tail) {
            SDL_free(data->program_cache.tail->next);
            data->program_cache.tail->next = NULL;
        }
        --data->program_cache.count;
    }
    return entry;
}

static GLuint GLES2_CacheShader(GLES2_RenderData *data, GLES2_ShaderType type, GLenum shader_type)
{
    GLuint id = 0;
    GLint compileSuccessful = GL_FALSE;
    int attempt, num_src;
    const GLchar *shader_src_list[3];
    const GLchar *shader_body = GLES2_GetShader(type);

    if (!shader_body) {
        SDL_SetError("No shader body src");
        return 0;
    }

    for (attempt = 0; attempt < 2 && !compileSuccessful; ++attempt) {
        num_src = 0;

        shader_src_list[num_src++] = GLES2_GetShaderPrologue(type);

        if (shader_type == GL_FRAGMENT_SHADER) {
            if (attempt == 0) {
                shader_src_list[num_src++] = GLES2_GetShaderInclude(data->texcoord_precision_hint);
            } else {
                shader_src_list[num_src++] = GLES2_GetShaderInclude(GLES2_SHADER_FRAGMENT_INCLUDE_UNDEF_PRECISION);
            }
        }

        shader_src_list[num_src++] = shader_body;

        SDL_assert(num_src <= SDL_arraysize(shader_src_list));

#ifdef DEBUG_PRINT_SHADERS
        {
            int i;
            char *message = NULL;

            SDL_asprintf(&message, "Compiling shader:\n");
            for (i = 0; i < num_src; ++i) {
                char *last_message = message;
                SDL_asprintf(&message, "%s%s", last_message, shader_src_list[i]);
                SDL_free(last_message);
            }
            SDL_Log("%s\n", message);
            SDL_free(message);
        }
#endif

        /* Compile */
        id = data->glCreateShader(shader_type);
        data->glShaderSource(id, num_src, shader_src_list, NULL);
        data->glCompileShader(id);
        data->glGetShaderiv(id, GL_COMPILE_STATUS, &compileSuccessful);
    }

    if (!compileSuccessful) {
        char *info = NULL;
        int length = 0;

        data->glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            info = (char *)SDL_malloc(length);
            if (info) {
                data->glGetShaderInfoLog(id, length, &length, info);
            }
        }
        if (info) {
            SDL_SetError("Failed to load the shader %d: %s", type, info);
            SDL_free(info);
        } else {
            SDL_SetError("Failed to load the shader %d", type);
        }
        data->glDeleteShader(id);
        return 0;
    }

    /* Cache */
    data->shader_id_cache[(Uint32)type] = id;

    return id;
}

static int GLES2_CacheShaders(GLES2_RenderData *data)
{
    int shader;

    data->texcoord_precision_hint = GLES2_GetTexCoordPrecisionEnumFromHint();

    for (shader = 0; shader < GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES; ++shader) {
        GLenum shader_type;

        if (shader == GLES2_SHADER_VERTEX_DEFAULT) {
            shader_type = GL_VERTEX_SHADER;
        } else {
            shader_type = GL_FRAGMENT_SHADER;
        }
        if (!GLES2_CacheShader(data, (GLES2_ShaderType)shader, shader_type)) {
            return -1;
        }
    }
    return 0;
}

static int GLES2_SelectProgram(GLES2_RenderData *data, GLES2_ImageSource source, SDL_Colorspace colorspace)
{
    GLuint vertex;
    GLuint fragment;
    GLES2_ShaderType vtype, ftype;
    GLES2_ProgramCacheEntry *program;
    const float *shader_params = NULL;

    /* Select an appropriate shader pair for the specified modes */
    vtype = GLES2_SHADER_VERTEX_DEFAULT;
    switch (source) {
    case GLES2_IMAGESOURCE_SOLID:
        ftype = GLES2_SHADER_FRAGMENT_SOLID;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_ABGR:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_ABGR;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_ARGB:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_ARGB;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_RGB:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_RGB;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_BGR:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_BGR;
        break;
#if SDL_HAVE_YUV
    case GLES2_IMAGESOURCE_TEXTURE_YUV:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_YUV;
        shader_params = SDL_GetYCbCRtoRGBConversionMatrix(colorspace, 0, 0, 8);
        if (!shader_params) {
            SDL_SetError("Unsupported YUV colorspace");
            goto fault;
        }
        break;
    case GLES2_IMAGESOURCE_TEXTURE_NV12:
        if (SDL_GetHintBoolean("SDL_RENDER_OPENGL_NV12_RG_SHADER", SDL_FALSE)) {
            ftype = GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RG;
        } else {
            ftype = GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RA;
        }
        shader_params = SDL_GetYCbCRtoRGBConversionMatrix(colorspace, 0, 0, 8);
        if (!shader_params) {
            SDL_SetError("Unsupported YUV colorspace");
            goto fault;
        }
        break;
    case GLES2_IMAGESOURCE_TEXTURE_NV21:
        if (SDL_GetHintBoolean("SDL_RENDER_OPENGL_NV12_RG_SHADER", SDL_FALSE)) {
            ftype = GLES2_SHADER_FRAGMENT_TEXTURE_NV21_RG;
        } else {
            ftype = GLES2_SHADER_FRAGMENT_TEXTURE_NV21_RA;
        }
        shader_params = SDL_GetYCbCRtoRGBConversionMatrix(colorspace, 0, 0, 8);
        if (!shader_params) {
            SDL_SetError("Unsupported YUV colorspace");
            goto fault;
        }
        break;
#endif /* SDL_HAVE_YUV */
    case GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES;
        break;
    default:
        goto fault;
    }

    /* Load the requested shaders */
    vertex = data->shader_id_cache[(Uint32)vtype];
    if (!vertex) {
        vertex = GLES2_CacheShader(data, vtype, GL_VERTEX_SHADER);
        if (!vertex) {
            goto fault;
        }
    }

    fragment = data->shader_id_cache[(Uint32)ftype];
    if (!fragment) {
        fragment = GLES2_CacheShader(data, ftype, GL_FRAGMENT_SHADER);
        if (!fragment) {
            goto fault;
        }
    }

    /* Check if we need to change programs at all */
    if (data->drawstate.program &&
        data->drawstate.program->vertex_shader == vertex &&
        data->drawstate.program->fragment_shader == fragment &&
        data->drawstate.program->shader_params == shader_params) {
        return 0;
    }

    /* Generate a matching program */
    program = GLES2_CacheProgram(data, vertex, fragment);
    if (!program) {
        goto fault;
    }

    /* Select that program in OpenGL */
    data->glUseProgram(program->id);

    if (shader_params && shader_params != program->shader_params) {
        /* YUV shader params are Yoffset, 0, Rcoeff, 0, Gcoeff, 0, Bcoeff, 0 */
        if (program->uniform_locations[GLES2_UNIFORM_OFFSET] != -1) {
            data->glUniform3f(program->uniform_locations[GLES2_UNIFORM_OFFSET], shader_params[0], shader_params[1], shader_params[2]);
        }
        if (program->uniform_locations[GLES2_UNIFORM_MATRIX] != -1) {
            GLfloat matrix[3 * 3];

            matrix[0 * 3 + 0] = shader_params[4];
            matrix[0 * 3 + 1] = shader_params[5];
            matrix[0 * 3 + 2] = shader_params[6];
            matrix[1 * 3 + 0] = shader_params[8];
            matrix[1 * 3 + 1] = shader_params[9];
            matrix[1 * 3 + 2] = shader_params[10];
            matrix[2 * 3 + 0] = shader_params[12];
            matrix[2 * 3 + 1] = shader_params[13];
            matrix[2 * 3 + 2] = shader_params[14];
            data->glUniformMatrix3fv(program->uniform_locations[GLES2_UNIFORM_MATRIX], 1, GL_FALSE, matrix);
        }
        program->shader_params = shader_params;
    }

    /* Set the current program */
    data->drawstate.program = program;

    /* Clean up and return */
    return 0;
fault:
    data->drawstate.program = NULL;
    return -1;
}

static int GLES2_QueueNoOp(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return 0; /* nothing to do in this backend. */
}

static int GLES2_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    const SDL_bool colorswap = (renderer->target && (renderer->target->format == SDL_PIXELFORMAT_BGRA32 || renderer->target->format == SDL_PIXELFORMAT_BGRX32));
    SDL_VertexSolid *verts = (SDL_VertexSolid *)SDL_AllocateRenderVertices(renderer, count * sizeof(*verts), 0, &cmd->data.draw.first);
    int i;
    SDL_FColor color = cmd->data.draw.color;
    const float color_scale = cmd->data.draw.color_scale;

    if (!verts) {
        return -1;
    }

    color.r *= color_scale;
    color.g *= color_scale;
    color.b *= color_scale;

    if (colorswap) {
        float r = color.r;
        color.r = color.b;
        color.b = r;
    }

    cmd->data.draw.count = count;
    for (i = 0; i < count; i++) {
        verts->position.x = 0.5f + points[i].x;
        verts->position.y = 0.5f + points[i].y;
        verts->color = color;
        verts++;
    }

    return 0;
}

static int GLES2_QueueDrawLines(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    const SDL_bool colorswap = (renderer->target && (renderer->target->format == SDL_PIXELFORMAT_BGRA32 || renderer->target->format == SDL_PIXELFORMAT_BGRX32));
    int i;
    GLfloat prevx, prevy;
    SDL_VertexSolid *verts = (SDL_VertexSolid *)SDL_AllocateRenderVertices(renderer, count * sizeof(*verts), 0, &cmd->data.draw.first);
    SDL_FColor color = cmd->data.draw.color;
    const float color_scale = cmd->data.draw.color_scale;

    if (!verts) {
        return -1;
    }

    color.r *= color_scale;
    color.g *= color_scale;
    color.b *= color_scale;

    if (colorswap) {
        float r = color.r;
        color.r = color.b;
        color.b = r;
    }

    cmd->data.draw.count = count;

    /* 0.5f offset to hit the center of the pixel. */
    prevx = 0.5f + points->x;
    prevy = 0.5f + points->y;
    verts->position.x = prevx;
    verts->position.y = prevy;
    verts->color = color;
    verts++;

    /* bump the end of each line segment out a quarter of a pixel, to provoke
       the diamond-exit rule. Without this, you won't just drop the last
       pixel of the last line segment, but you might also drop pixels at the
       edge of any given line segment along the way too. */
    for (i = 1; i < count; i++) {
        const GLfloat xstart = prevx;
        const GLfloat ystart = prevy;
        const GLfloat xend = points[i].x + 0.5f; /* 0.5f to hit pixel center. */
        const GLfloat yend = points[i].y + 0.5f;
        /* bump a little in the direction we are moving in. */
        const GLfloat deltax = xend - xstart;
        const GLfloat deltay = yend - ystart;
        const GLfloat angle = SDL_atan2f(deltay, deltax);
        prevx = xend + (SDL_cosf(angle) * 0.25f);
        prevy = yend + (SDL_sinf(angle) * 0.25f);
        verts->position.x = prevx;
        verts->position.y = prevy;
        verts->color = color;
        verts++;
    }

    return 0;
}

static int GLES2_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                               const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride,
                               int num_vertices, const void *indices, int num_indices, int size_indices,
                               float scale_x, float scale_y)
{
    int i;
    const SDL_bool colorswap = (renderer->target && (renderer->target->format == SDL_PIXELFORMAT_BGRA32 || renderer->target->format == SDL_PIXELFORMAT_BGRX32));
    int count = indices ? num_indices : num_vertices;
    const float color_scale = cmd->data.draw.color_scale;

    cmd->data.draw.count = count;
    size_indices = indices ? size_indices : 0;

    if (texture) {
        SDL_Vertex *verts = (SDL_Vertex *)SDL_AllocateRenderVertices(renderer, count * sizeof(*verts), 0, &cmd->data.draw.first);
        if (!verts) {
            return -1;
        }

        for (i = 0; i < count; i++) {
            int j;
            float *xy_;
            SDL_FColor col_;
            float *uv_;
            if (size_indices == 4) {
                j = ((const Uint32 *)indices)[i];
            } else if (size_indices == 2) {
                j = ((const Uint16 *)indices)[i];
            } else if (size_indices == 1) {
                j = ((const Uint8 *)indices)[i];
            } else {
                j = i;
            }

            xy_ = (float *)((char *)xy + j * xy_stride);
            col_ = *(SDL_FColor *)((char *)color + j * color_stride);
            uv_ = (float *)((char *)uv + j * uv_stride);

            verts->position.x = xy_[0] * scale_x;
            verts->position.y = xy_[1] * scale_y;

            col_.r *= color_scale;
            col_.g *= color_scale;
            col_.b *= color_scale;

            if (colorswap) {
                float r = col_.r;
                col_.r = col_.b;
                col_.b = r;
            }

            verts->color = col_;
            verts->tex_coord.x = uv_[0];
            verts->tex_coord.y = uv_[1];
            verts++;
        }

    } else {
        SDL_VertexSolid *verts = (SDL_VertexSolid *)SDL_AllocateRenderVertices(renderer, count * sizeof(*verts), 0, &cmd->data.draw.first);
        if (!verts) {
            return -1;
        }

        for (i = 0; i < count; i++) {
            int j;
            float *xy_;
            SDL_FColor col_;

            if (size_indices == 4) {
                j = ((const Uint32 *)indices)[i];
            } else if (size_indices == 2) {
                j = ((const Uint16 *)indices)[i];
            } else if (size_indices == 1) {
                j = ((const Uint8 *)indices)[i];
            } else {
                j = i;
            }

            xy_ = (float *)((char *)xy + j * xy_stride);
            col_ = *(SDL_FColor *)((char *)color + j * color_stride);

            verts->position.x = xy_[0] * scale_x;
            verts->position.y = xy_[1] * scale_y;

            col_.r *= color_scale;
            col_.g *= color_scale;
            col_.b *= color_scale;

            if (colorswap) {
                float r = col_.r;
                col_.r = col_.b;
                col_.b = r;
            }

            verts->color = col_;
            verts++;
        }
    }

    return 0;
}

static int SetDrawState(GLES2_RenderData *data, const SDL_RenderCommand *cmd, const GLES2_ImageSource imgsrc, void *vertices)
{
    SDL_Texture *texture = cmd->data.draw.texture;
    const SDL_BlendMode blend = cmd->data.draw.blend;
    GLES2_ProgramCacheEntry *program;
    int stride;

    SDL_assert((texture != NULL) == (imgsrc != GLES2_IMAGESOURCE_SOLID));

    if (data->drawstate.viewport_dirty) {
        const SDL_Rect *viewport = &data->drawstate.viewport;
        data->glViewport(viewport->x,
                         data->drawstate.target ? viewport->y : (data->drawstate.drawableh - viewport->y - viewport->h),
                         viewport->w, viewport->h);
        if (viewport->w && viewport->h) {
            data->drawstate.projection[0][0] = 2.0f / viewport->w;
            data->drawstate.projection[1][1] = (data->drawstate.target ? 2.0f : -2.0f) / viewport->h;
            data->drawstate.projection[3][1] = data->drawstate.target ? -1.0f : 1.0f;
        }
        data->drawstate.viewport_dirty = SDL_FALSE;
    }

    if (data->drawstate.cliprect_enabled_dirty) {
        if (!data->drawstate.cliprect_enabled) {
            data->glDisable(GL_SCISSOR_TEST);
        } else {
            data->glEnable(GL_SCISSOR_TEST);
        }
        data->drawstate.cliprect_enabled_dirty = SDL_FALSE;
    }

    if (data->drawstate.cliprect_enabled && data->drawstate.cliprect_dirty) {
        const SDL_Rect *viewport = &data->drawstate.viewport;
        const SDL_Rect *rect = &data->drawstate.cliprect;
        data->glScissor(viewport->x + rect->x,
                        data->drawstate.target ? viewport->y + rect->y : data->drawstate.drawableh - viewport->y - rect->y - rect->h,
                        rect->w, rect->h);
        data->drawstate.cliprect_dirty = SDL_FALSE;
    }

    if (data->drawstate.texturing_dirty || ((texture != NULL) != data->drawstate.texturing)) {
        if (!texture) {
            data->glDisableVertexAttribArray((GLenum)GLES2_ATTRIBUTE_TEXCOORD);
            data->drawstate.texturing = SDL_FALSE;
        } else {
            data->glEnableVertexAttribArray((GLenum)GLES2_ATTRIBUTE_TEXCOORD);
            data->drawstate.texturing = SDL_TRUE;
        }
        data->drawstate.texturing_dirty = SDL_FALSE;
    }

    if (texture) {
        stride = sizeof(SDL_Vertex);
    } else {
        stride = sizeof(SDL_VertexSolid);
    }

    if (texture) {
        SDL_Vertex *verts = (SDL_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
        data->glVertexAttribPointer(GLES2_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)&verts->tex_coord);
    }

    if (GLES2_SelectProgram(data, imgsrc, texture ? texture->colorspace : SDL_COLORSPACE_SRGB) < 0) {
        return -1;
    }

    program = data->drawstate.program;

    if (program->uniform_locations[GLES2_UNIFORM_PROJECTION] != -1) {
        if (SDL_memcmp(program->projection, data->drawstate.projection, sizeof(data->drawstate.projection)) != 0) {
            data->glUniformMatrix4fv(program->uniform_locations[GLES2_UNIFORM_PROJECTION], 1, GL_FALSE, (GLfloat *)data->drawstate.projection);
            SDL_memcpy(program->projection, data->drawstate.projection, sizeof(data->drawstate.projection));
        }
    }

    if (blend != data->drawstate.blend) {
        if (blend == SDL_BLENDMODE_NONE) {
            data->glDisable(GL_BLEND);
        } else {
            data->glEnable(GL_BLEND);
            data->glBlendFuncSeparate(GetBlendFunc(SDL_GetBlendModeSrcColorFactor(blend)),
                                      GetBlendFunc(SDL_GetBlendModeDstColorFactor(blend)),
                                      GetBlendFunc(SDL_GetBlendModeSrcAlphaFactor(blend)),
                                      GetBlendFunc(SDL_GetBlendModeDstAlphaFactor(blend)));
            data->glBlendEquationSeparate(GetBlendEquation(SDL_GetBlendModeColorOperation(blend)),
                                          GetBlendEquation(SDL_GetBlendModeAlphaOperation(blend)));
        }
        data->drawstate.blend = blend;
    }

    /* all drawing commands use this */
    {
        SDL_VertexSolid *verts = (SDL_VertexSolid *)(((Uint8 *)vertices) + cmd->data.draw.first);
        data->glVertexAttribPointer(GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)&verts->position);
        data->glVertexAttribPointer(GLES2_ATTRIBUTE_COLOR, 4, GL_FLOAT, GL_TRUE /* Normalized */, stride, (const GLvoid *)&verts->color);
    }

    return 0;
}

static int SetCopyState(SDL_Renderer *renderer, const SDL_RenderCommand *cmd, void *vertices)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    GLES2_ImageSource sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
    SDL_Texture *texture = cmd->data.draw.texture;
    int ret;

    /* Pick an appropriate shader */
    if (renderer->target) {
        /* Check if we need to do color mapping between the source and render target textures */
        if (renderer->target->format != texture->format) {
            switch (texture->format) {
            case SDL_PIXELFORMAT_BGRA32:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_RGBA32:
                case SDL_PIXELFORMAT_RGBX32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                case SDL_PIXELFORMAT_BGRX32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                    break;
                default:
                    break;
                }
                break;
            case SDL_PIXELFORMAT_RGBA32:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_BGRA32:
                case SDL_PIXELFORMAT_BGRX32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                case SDL_PIXELFORMAT_RGBX32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                    break;
                default:
                    break;
                }
                break;
            case SDL_PIXELFORMAT_BGRX32:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_RGBA32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                case SDL_PIXELFORMAT_BGRA32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                    break;
                case SDL_PIXELFORMAT_RGBX32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                default:
                    break;
                }
                break;
            case SDL_PIXELFORMAT_RGBX32:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_RGBA32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                    break;
                case SDL_PIXELFORMAT_BGRA32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_RGB;
                    break;
                case SDL_PIXELFORMAT_BGRX32:
                    sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                default:
                    break;
                }
                break;
#if SDL_HAVE_YUV
            case SDL_PIXELFORMAT_IYUV:
            case SDL_PIXELFORMAT_YV12:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_YUV;
                break;
            case SDL_PIXELFORMAT_NV12:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_NV12;
                break;
            case SDL_PIXELFORMAT_NV21:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_NV21;
                break;
#endif
            case SDL_PIXELFORMAT_EXTERNAL_OES:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES;
                break;
            default:
                return SDL_SetError("Unsupported texture format");
            }
        } else {
            sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR; /* Texture formats match, use the non color mapping shader (even if the formats are not ABGR) */
        }
    } else {
        switch (texture->format) {
        case SDL_PIXELFORMAT_BGRA32:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
            break;
        case SDL_PIXELFORMAT_RGBA32:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
            break;
        case SDL_PIXELFORMAT_BGRX32:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_RGB;
            break;
        case SDL_PIXELFORMAT_RGBX32:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
            break;
#if SDL_HAVE_YUV
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_YV12:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_YUV;
            break;
        case SDL_PIXELFORMAT_NV12:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_NV12;
            break;
        case SDL_PIXELFORMAT_NV21:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_NV21;
            break;
#endif
        case SDL_PIXELFORMAT_EXTERNAL_OES:
            sourceType = GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES;
            break;
        default:
            return SDL_SetError("Unsupported texture format");
        }
    }

    ret = SetDrawState(data, cmd, sourceType, vertices);

    if (texture != data->drawstate.texture) {
        GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;
#if SDL_HAVE_YUV
        if (tdata->yuv) {
            data->glActiveTexture(GL_TEXTURE2);
            data->glBindTexture(tdata->texture_type, tdata->texture_v);

            data->glActiveTexture(GL_TEXTURE1);
            data->glBindTexture(tdata->texture_type, tdata->texture_u);

            data->glActiveTexture(GL_TEXTURE0);
        } else if (tdata->nv12) {
            data->glActiveTexture(GL_TEXTURE1);
            data->glBindTexture(tdata->texture_type, tdata->texture_u);

            data->glActiveTexture(GL_TEXTURE0);
        }
#endif
        data->glBindTexture(tdata->texture_type, tdata->texture);
        data->drawstate.texture = texture;
    }

    return ret;
}

static void GLES2_InvalidateCachedState(SDL_Renderer *renderer)
{
    GLES2_DrawStateCache *cache = &((GLES2_RenderData *)renderer->driverdata)->drawstate;
    cache->viewport_dirty = SDL_TRUE;
    cache->texture = NULL;
    cache->blend = SDL_BLENDMODE_INVALID;
    cache->cliprect_enabled_dirty = SDL_TRUE;
    cache->cliprect_dirty = SDL_TRUE;
    cache->texturing_dirty = SDL_TRUE;
    cache->clear_color_dirty = SDL_TRUE;
    cache->drawablew = 0;
    cache->drawableh = 0;
    cache->program = NULL;
}

static int GLES2_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    const SDL_bool colorswap = (renderer->target && (renderer->target->format == SDL_PIXELFORMAT_BGRA32 || renderer->target->format == SDL_PIXELFORMAT_BGRX32));

#if USE_VERTEX_BUFFER_OBJECTS
    const int vboidx = data->current_vertex_buffer;
    const GLuint vbo = data->vertex_buffers[vboidx];
#endif

    if (GLES2_ActivateRenderer(renderer) < 0) {
        return -1;
    }

    data->drawstate.target = renderer->target;
    if (!data->drawstate.target) {
        int w, h;
        SDL_GetWindowSizeInPixels(renderer->window, &w, &h);
        if ((w != data->drawstate.drawablew) || (h != data->drawstate.drawableh)) {
            data->drawstate.viewport_dirty = SDL_TRUE; // if the window dimensions changed, invalidate the current viewport, etc.
            data->drawstate.cliprect_dirty = SDL_TRUE;
            data->drawstate.drawablew = w;
            data->drawstate.drawableh = h;
        }
    }

#if USE_VERTEX_BUFFER_OBJECTS
    /* upload the new VBO data for this set of commands. */
    data->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (data->vertex_buffer_size[vboidx] < vertsize) {
        data->glBufferData(GL_ARRAY_BUFFER, vertsize, vertices, GL_STREAM_DRAW);
        data->vertex_buffer_size[vboidx] = vertsize;
    } else {
        data->glBufferSubData(GL_ARRAY_BUFFER, 0, vertsize, vertices);
    }

    /* cycle through a few VBOs so the GL has some time with the data before we replace it. */
    data->current_vertex_buffer++;
    if (data->current_vertex_buffer >= SDL_arraysize(data->vertex_buffers)) {
        data->current_vertex_buffer = 0;
    }
    vertices = NULL; /* attrib pointers will be offsets into the VBO. */
#endif

    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            break;
        }

        case SDL_RENDERCMD_SETVIEWPORT:
        {
            SDL_Rect *viewport = &data->drawstate.viewport;
            if (SDL_memcmp(viewport, &cmd->data.viewport.rect, sizeof(cmd->data.viewport.rect)) != 0) {
                SDL_copyp(viewport, &cmd->data.viewport.rect);
                data->drawstate.viewport_dirty = SDL_TRUE;
                data->drawstate.cliprect_dirty = SDL_TRUE;
            }
            break;
        }

        case SDL_RENDERCMD_SETCLIPRECT:
        {
            const SDL_Rect *rect = &cmd->data.cliprect.rect;
            if (data->drawstate.cliprect_enabled != cmd->data.cliprect.enabled) {
                data->drawstate.cliprect_enabled = cmd->data.cliprect.enabled;
                data->drawstate.cliprect_enabled_dirty = SDL_TRUE;
            }

            if (SDL_memcmp(&data->drawstate.cliprect, rect, sizeof(*rect)) != 0) {
                SDL_copyp(&data->drawstate.cliprect, rect);
                data->drawstate.cliprect_dirty = SDL_TRUE;
            }
            break;
        }

        case SDL_RENDERCMD_CLEAR:
        {
            const float r = (colorswap ? cmd->data.color.color.b : cmd->data.color.color.r) * cmd->data.color.color_scale;
            const float g = cmd->data.color.color.g * cmd->data.color.color_scale;
            const float b = (colorswap ? cmd->data.color.color.r : cmd->data.color.color.b) * cmd->data.color.color_scale;
            const float a = cmd->data.color.color.a;
            if (data->drawstate.clear_color_dirty ||
                (r != data->drawstate.clear_color.r) ||
                (g != data->drawstate.clear_color.g) ||
                (b != data->drawstate.clear_color.b) ||
                (a != data->drawstate.clear_color.a)) {
                data->glClearColor(r, g, b, a);
                data->drawstate.clear_color.r = r;
                data->drawstate.clear_color.g = g;
                data->drawstate.clear_color.b = b;
                data->drawstate.clear_color.a = a;
                data->drawstate.clear_color_dirty = SDL_FALSE;
            }

            if (data->drawstate.cliprect_enabled || data->drawstate.cliprect_enabled_dirty) {
                data->glDisable(GL_SCISSOR_TEST);
                data->drawstate.cliprect_enabled_dirty = data->drawstate.cliprect_enabled;
            }

            data->glClear(GL_COLOR_BUFFER_BIT);
            break;
        }

        case SDL_RENDERCMD_FILL_RECTS: /* unused */
            break;

        case SDL_RENDERCMD_COPY: /* unused */
            break;

        case SDL_RENDERCMD_COPY_EX: /* unused */
            break;

        case SDL_RENDERCMD_DRAW_LINES:
        {
            if (SetDrawState(data, cmd, GLES2_IMAGESOURCE_SOLID, vertices) == 0) {
                size_t count = cmd->data.draw.count;
                if (count > 2) {
                    /* joined lines cannot be grouped */
                    data->glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)count);
                } else {
                    /* let's group non joined lines */
                    SDL_RenderCommand *finalcmd = cmd;
                    SDL_RenderCommand *nextcmd = cmd->next;
                    SDL_BlendMode thisblend = cmd->data.draw.blend;

                    while (nextcmd) {
                        const SDL_RenderCommandType nextcmdtype = nextcmd->command;
                        if (nextcmdtype != SDL_RENDERCMD_DRAW_LINES) {
                            break; /* can't go any further on this draw call, different render command up next. */
                        } else if (nextcmd->data.draw.count != 2) {
                            break; /* can't go any further on this draw call, those are joined lines */
                        } else if (nextcmd->data.draw.blend != thisblend) {
                            break; /* can't go any further on this draw call, different blendmode copy up next. */
                        } else {
                            finalcmd = nextcmd; /* we can combine copy operations here. Mark this one as the furthest okay command. */
                            count += nextcmd->data.draw.count;
                        }
                        nextcmd = nextcmd->next;
                    }

                    data->glDrawArrays(GL_LINES, 0, (GLsizei)count);
                    cmd = finalcmd; /* skip any copy commands we just combined in here. */
                }
            }
            break;
        }

        case SDL_RENDERCMD_DRAW_POINTS:
        case SDL_RENDERCMD_GEOMETRY:
        {
            /* as long as we have the same copy command in a row, with the
               same texture, we can combine them all into a single draw call. */
            SDL_Texture *thistexture = cmd->data.draw.texture;
            SDL_BlendMode thisblend = cmd->data.draw.blend;
            const SDL_RenderCommandType thiscmdtype = cmd->command;
            SDL_RenderCommand *finalcmd = cmd;
            SDL_RenderCommand *nextcmd = cmd->next;
            size_t count = cmd->data.draw.count;
            int ret;
            while (nextcmd) {
                const SDL_RenderCommandType nextcmdtype = nextcmd->command;
                if (nextcmdtype != thiscmdtype) {
                    break; /* can't go any further on this draw call, different render command up next. */
                } else if (nextcmd->data.draw.texture != thistexture || nextcmd->data.draw.blend != thisblend) {
                    break; /* can't go any further on this draw call, different texture/blendmode copy up next. */
                } else {
                    finalcmd = nextcmd; /* we can combine copy operations here. Mark this one as the furthest okay command. */
                    count += nextcmd->data.draw.count;
                }
                nextcmd = nextcmd->next;
            }

            if (thistexture) {
                ret = SetCopyState(renderer, cmd, vertices);
            } else {
                ret = SetDrawState(data, cmd, GLES2_IMAGESOURCE_SOLID, vertices);
            }

            if (ret == 0) {
                int op = GL_TRIANGLES; /* SDL_RENDERCMD_GEOMETRY */
                if (thiscmdtype == SDL_RENDERCMD_DRAW_POINTS) {
                    op = GL_POINTS;
                }
                data->glDrawArrays(op, 0, (GLsizei)count);
            }

            cmd = finalcmd; /* skip any copy commands we just combined in here. */
            break;
        }

        case SDL_RENDERCMD_NO_OP:
            break;
        }

        cmd = cmd->next;
    }

    return GL_CheckError("", renderer);
}

static void GLES2_DestroyRenderer(SDL_Renderer *renderer)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;

    /* Deallocate everything */
    if (data) {
        GLES2_ActivateRenderer(renderer);

        {
            int i;
            for (i = 0; i < GLES2_SHADER_COUNT; i++) {
                GLuint id = data->shader_id_cache[i];
                if (id) {
                    data->glDeleteShader(id);
                }
            }
        }
        {
            GLES2_ProgramCacheEntry *entry;
            GLES2_ProgramCacheEntry *next;
            entry = data->program_cache.head;
            while (entry) {
                data->glDeleteProgram(entry->id);
                next = entry->next;
                SDL_free(entry);
                entry = next;
            }
        }

        if (data->context) {
            while (data->framebuffers) {
                GLES2_FBOList *nextnode = data->framebuffers->next;
                data->glDeleteFramebuffers(1, &data->framebuffers->FBO);
                GL_CheckError("", renderer);
                SDL_free(data->framebuffers);
                data->framebuffers = nextnode;
            }

#if USE_VERTEX_BUFFER_OBJECTS
            data->glDeleteBuffers(SDL_arraysize(data->vertex_buffers), data->vertex_buffers);
            GL_CheckError("", renderer);
#endif

            SDL_GL_DeleteContext(data->context);
        }

        SDL_free(data);
    }
    SDL_free(renderer);
}

static int GLES2_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_PropertiesID create_props)
{
    GLES2_RenderData *renderdata = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *data;
    GLenum format;
    GLenum type;
    GLenum scaleMode;

    GLES2_ActivateRenderer(renderer);

    renderdata->drawstate.texture = NULL; /* we trash this state. */

    /* Determine the corresponding GLES texture format params */
    switch (texture->format) {
    case SDL_PIXELFORMAT_BGRA32:
    case SDL_PIXELFORMAT_RGBA32:
    case SDL_PIXELFORMAT_BGRX32:
    case SDL_PIXELFORMAT_RGBX32:
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        break;
#if SDL_HAVE_YUV
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_BYTE;
        break;
#endif
#ifdef GL_TEXTURE_EXTERNAL_OES
    case SDL_PIXELFORMAT_EXTERNAL_OES:
        format = GL_NONE;
        type = GL_NONE;
        break;
#endif
    default:
        return SDL_SetError("Texture format not supported");
    }

    if (texture->format == SDL_PIXELFORMAT_EXTERNAL_OES &&
        texture->access != SDL_TEXTUREACCESS_STATIC) {
        return SDL_SetError("Unsupported texture access for SDL_PIXELFORMAT_EXTERNAL_OES");
    }

    /* Allocate a texture struct */
    data = (GLES2_TextureData *)SDL_calloc(1, sizeof(GLES2_TextureData));
    if (!data) {
        return -1;
    }
    data->texture = 0;
#ifdef GL_TEXTURE_EXTERNAL_OES
    data->texture_type = (texture->format == SDL_PIXELFORMAT_EXTERNAL_OES) ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
#else
    data->texture_type = GL_TEXTURE_2D;
#endif
    data->pixel_format = format;
    data->pixel_type = type;
#if SDL_HAVE_YUV
    data->yuv = ((texture->format == SDL_PIXELFORMAT_IYUV) || (texture->format == SDL_PIXELFORMAT_YV12));
    data->nv12 = ((texture->format == SDL_PIXELFORMAT_NV12) || (texture->format == SDL_PIXELFORMAT_NV21));
    data->texture_u = 0;
    data->texture_v = 0;
#endif
    scaleMode = (texture->scaleMode == SDL_SCALEMODE_NEAREST) ? GL_NEAREST : GL_LINEAR;

    /* Allocate a blob for image renderdata */
    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        size_t size;
        data->pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
        size = (size_t)texture->h * data->pitch;
#if SDL_HAVE_YUV
        if (data->yuv) {
            /* Need to add size for the U and V planes */
            size += 2 * ((texture->h + 1) / 2) * ((data->pitch + 1) / 2);
        } else if (data->nv12) {
            /* Need to add size for the U/V plane */
            size += 2 * ((texture->h + 1) / 2) * ((data->pitch + 1) / 2);
        }
#endif
        data->pixel_data = SDL_calloc(1, size);
        if (!data->pixel_data) {
            SDL_free(data);
            return -1;
        }
    }

    /* Allocate the texture */
    GL_CheckError("", renderer);

#if SDL_HAVE_YUV
    if (data->yuv) {
        data->texture_v = (GLuint)SDL_GetNumberProperty(create_props, SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_V_NUMBER, 0);
        if (data->texture_v) {
            data->texture_v_external = SDL_TRUE;
        } else {
            renderdata->glGenTextures(1, &data->texture_v);
            if (GL_CheckError("glGenTexures()", renderer) < 0) {
                return -1;
            }
        }
        renderdata->glActiveTexture(GL_TEXTURE2);
        renderdata->glBindTexture(data->texture_type, data->texture_v);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        renderdata->glTexImage2D(data->texture_type, 0, format, (texture->w + 1) / 2, (texture->h + 1) / 2, 0, format, type, NULL);
        SDL_SetNumberProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_V_NUMBER, data->texture_v);

        data->texture_u = (GLuint)SDL_GetNumberProperty(create_props, SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_U_NUMBER, 0);
        if (data->texture_u) {
            data->texture_u_external = SDL_TRUE;
        } else {
            renderdata->glGenTextures(1, &data->texture_u);
            if (GL_CheckError("glGenTexures()", renderer) < 0) {
                return -1;
            }
        }
        renderdata->glActiveTexture(GL_TEXTURE1);
        renderdata->glBindTexture(data->texture_type, data->texture_u);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        renderdata->glTexImage2D(data->texture_type, 0, format, (texture->w + 1) / 2, (texture->h + 1) / 2, 0, format, type, NULL);
        if (GL_CheckError("glTexImage2D()", renderer) < 0) {
            return -1;
        }
        SDL_SetNumberProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_U_NUMBER, data->texture_u);

        if (!SDL_GetYCbCRtoRGBConversionMatrix(texture->colorspace, texture->w, texture->h, 8)) {
            return SDL_SetError("Unsupported YUV colorspace");
        }
    } else if (data->nv12) {
        data->texture_u = (GLuint)SDL_GetNumberProperty(create_props, SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_UV_NUMBER, 0);
        if (data->texture_u) {
            data->texture_u_external = SDL_TRUE;
        } else {
            renderdata->glGenTextures(1, &data->texture_u);
            if (GL_CheckError("glGenTexures()", renderer) < 0) {
                return -1;
            }
        }
        renderdata->glActiveTexture(GL_TEXTURE1);
        renderdata->glBindTexture(data->texture_type, data->texture_u);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        renderdata->glTexImage2D(data->texture_type, 0, GL_LUMINANCE_ALPHA, (texture->w + 1) / 2, (texture->h + 1) / 2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, NULL);
        if (GL_CheckError("glTexImage2D()", renderer) < 0) {
            return -1;
        }
        SDL_SetNumberProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_UV_NUMBER, data->texture_u);

        if (!SDL_GetYCbCRtoRGBConversionMatrix(texture->colorspace, texture->w, texture->h, 8)) {
            return SDL_SetError("Unsupported YUV colorspace");
        }
    }
#endif

    data->texture = (GLuint)SDL_GetNumberProperty(create_props, SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_NUMBER, 0);
    if (data->texture) {
        data->texture_external = SDL_TRUE;
    } else {
        renderdata->glGenTextures(1, &data->texture);
        if (GL_CheckError("glGenTexures()", renderer) < 0) {
            return -1;
        }
    }
    texture->driverdata = data;
    renderdata->glActiveTexture(GL_TEXTURE0);
    renderdata->glBindTexture(data->texture_type, data->texture);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, scaleMode);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, scaleMode);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (texture->format != SDL_PIXELFORMAT_EXTERNAL_OES) {
        renderdata->glTexImage2D(data->texture_type, 0, format, texture->w, texture->h, 0, format, type, NULL);
        if (GL_CheckError("glTexImage2D()", renderer) < 0) {
            return -1;
        }
    }
    SDL_SetNumberProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, data->texture);
    SDL_SetNumberProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_TARGET_NUMBER, data->texture_type);

    if (texture->access == SDL_TEXTUREACCESS_TARGET) {
        data->fbo = GLES2_GetFBO((GLES2_RenderData *)renderer->driverdata, texture->w, texture->h);
    } else {
        data->fbo = NULL;
    }

    return GL_CheckError("", renderer);
}

static int GLES2_TexSubImage2D(GLES2_RenderData *data, GLenum target, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels, GLint pitch, GLint bpp)
{
    Uint8 *blob = NULL;
    Uint8 *src;
    size_t src_pitch;
    int y;

    if ((width == 0) || (height == 0) || (bpp == 0)) {
        return 0; /* nothing to do */
    }

    /* Reformat the texture data into a tightly packed array */
    src_pitch = (size_t)width * bpp;
    src = (Uint8 *)pixels;
    if ((size_t)pitch != src_pitch) {
        blob = (Uint8 *)SDL_malloc(src_pitch * height);
        if (!blob) {
            return -1;
        }
        src = blob;
        for (y = 0; y < height; ++y) {
            SDL_memcpy(src, pixels, src_pitch);
            src += src_pitch;
            pixels = (Uint8 *)pixels + pitch;
        }
        src = blob;
    }

    data->glTexSubImage2D(target, 0, xoffset, yoffset, width, height, format, type, src);
    if (blob) {
        SDL_free(blob);
    }
    return 0;
}

static int GLES2_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                               const void *pixels, int pitch)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;

    GLES2_ActivateRenderer(renderer);

    /* Bail out if we're supposed to update an empty rectangle */
    if (rect->w <= 0 || rect->h <= 0) {
        return 0;
    }

    data->drawstate.texture = NULL; /* we trash this state. */

    /* Create a texture subimage with the supplied data */
    data->glBindTexture(tdata->texture_type, tdata->texture);
    GLES2_TexSubImage2D(data, tdata->texture_type,
                        rect->x,
                        rect->y,
                        rect->w,
                        rect->h,
                        tdata->pixel_format,
                        tdata->pixel_type,
                        pixels, pitch, SDL_BYTESPERPIXEL(texture->format));

#if SDL_HAVE_YUV
    if (tdata->yuv) {
        /* Skip to the correct offset into the next texture */
        pixels = (const void *)((const Uint8 *)pixels + rect->h * pitch);
        if (texture->format == SDL_PIXELFORMAT_YV12) {
            data->glBindTexture(tdata->texture_type, tdata->texture_v);
        } else {
            data->glBindTexture(tdata->texture_type, tdata->texture_u);
        }
        GLES2_TexSubImage2D(data, tdata->texture_type,
                            rect->x / 2,
                            rect->y / 2,
                            (rect->w + 1) / 2,
                            (rect->h + 1) / 2,
                            tdata->pixel_format,
                            tdata->pixel_type,
                            pixels, (pitch + 1) / 2, 1);

        /* Skip to the correct offset into the next texture */
        pixels = (const void *)((const Uint8 *)pixels + ((rect->h + 1) / 2) * ((pitch + 1) / 2));
        if (texture->format == SDL_PIXELFORMAT_YV12) {
            data->glBindTexture(tdata->texture_type, tdata->texture_u);
        } else {
            data->glBindTexture(tdata->texture_type, tdata->texture_v);
        }
        GLES2_TexSubImage2D(data, tdata->texture_type,
                            rect->x / 2,
                            rect->y / 2,
                            (rect->w + 1) / 2,
                            (rect->h + 1) / 2,
                            tdata->pixel_format,
                            tdata->pixel_type,
                            pixels, (pitch + 1) / 2, 1);
    } else if (tdata->nv12) {
        /* Skip to the correct offset into the next texture */
        pixels = (const void *)((const Uint8 *)pixels + rect->h * pitch);
        data->glBindTexture(tdata->texture_type, tdata->texture_u);
        GLES2_TexSubImage2D(data, tdata->texture_type,
                            rect->x / 2,
                            rect->y / 2,
                            (rect->w + 1) / 2,
                            (rect->h + 1) / 2,
                            GL_LUMINANCE_ALPHA,
                            GL_UNSIGNED_BYTE,
                            pixels, 2 * ((pitch + 1) / 2), 2);
    }
#endif

    return GL_CheckError("glTexSubImage2D()", renderer);
}

#if SDL_HAVE_YUV
static int GLES2_UpdateTextureYUV(SDL_Renderer *renderer, SDL_Texture *texture,
                                  const SDL_Rect *rect,
                                  const Uint8 *Yplane, int Ypitch,
                                  const Uint8 *Uplane, int Upitch,
                                  const Uint8 *Vplane, int Vpitch)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;

    GLES2_ActivateRenderer(renderer);

    /* Bail out if we're supposed to update an empty rectangle */
    if (rect->w <= 0 || rect->h <= 0) {
        return 0;
    }

    data->drawstate.texture = NULL; /* we trash this state. */

    data->glBindTexture(tdata->texture_type, tdata->texture_v);
    GLES2_TexSubImage2D(data, tdata->texture_type,
                        rect->x / 2,
                        rect->y / 2,
                        (rect->w + 1) / 2,
                        (rect->h + 1) / 2,
                        tdata->pixel_format,
                        tdata->pixel_type,
                        Vplane, Vpitch, 1);

    data->glBindTexture(tdata->texture_type, tdata->texture_u);
    GLES2_TexSubImage2D(data, tdata->texture_type,
                        rect->x / 2,
                        rect->y / 2,
                        (rect->w + 1) / 2,
                        (rect->h + 1) / 2,
                        tdata->pixel_format,
                        tdata->pixel_type,
                        Uplane, Upitch, 1);

    data->glBindTexture(tdata->texture_type, tdata->texture);
    GLES2_TexSubImage2D(data, tdata->texture_type,
                        rect->x,
                        rect->y,
                        rect->w,
                        rect->h,
                        tdata->pixel_format,
                        tdata->pixel_type,
                        Yplane, Ypitch, 1);

    return GL_CheckError("glTexSubImage2D()", renderer);
}

static int GLES2_UpdateTextureNV(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect,
                                 const Uint8 *Yplane, int Ypitch,
                                 const Uint8 *UVplane, int UVpitch)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;

    GLES2_ActivateRenderer(renderer);

    /* Bail out if we're supposed to update an empty rectangle */
    if (rect->w <= 0 || rect->h <= 0) {
        return 0;
    }

    data->drawstate.texture = NULL; /* we trash this state. */

    data->glBindTexture(tdata->texture_type, tdata->texture_u);
    GLES2_TexSubImage2D(data, tdata->texture_type,
                        rect->x / 2,
                        rect->y / 2,
                        (rect->w + 1) / 2,
                        (rect->h + 1) / 2,
                        GL_LUMINANCE_ALPHA,
                        GL_UNSIGNED_BYTE,
                        UVplane, UVpitch, 2);

    data->glBindTexture(tdata->texture_type, tdata->texture);
    GLES2_TexSubImage2D(data, tdata->texture_type,
                        rect->x,
                        rect->y,
                        rect->w,
                        rect->h,
                        tdata->pixel_format,
                        tdata->pixel_type,
                        Yplane, Ypitch, 1);

    return GL_CheckError("glTexSubImage2D()", renderer);
}
#endif

static int GLES2_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                             void **pixels, int *pitch)
{
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;

    /* Retrieve the buffer/pitch for the specified region */
    *pixels = (Uint8 *)tdata->pixel_data +
              (tdata->pitch * rect->y) +
              (rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = tdata->pitch;

    return 0;
}

static void GLES2_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;
    SDL_Rect rect;

    /* We do whole texture updates, at least for now */
    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    GLES2_UpdateTexture(renderer, texture, &rect, tdata->pixel_data, tdata->pitch);
}

static void GLES2_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
    GLES2_RenderData *renderdata = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *data = (GLES2_TextureData *)texture->driverdata;
    GLenum glScaleMode = (scaleMode == SDL_SCALEMODE_NEAREST) ? GL_NEAREST : GL_LINEAR;

#if SDL_HAVE_YUV
    if (data->yuv) {
        renderdata->glActiveTexture(GL_TEXTURE2);
        renderdata->glBindTexture(data->texture_type, data->texture_v);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, glScaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, glScaleMode);

        renderdata->glActiveTexture(GL_TEXTURE1);
        renderdata->glBindTexture(data->texture_type, data->texture_u);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, glScaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, glScaleMode);
    } else if (data->nv12) {
        renderdata->glActiveTexture(GL_TEXTURE1);
        renderdata->glBindTexture(data->texture_type, data->texture_u);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, glScaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, glScaleMode);
    }
#endif

    renderdata->glActiveTexture(GL_TEXTURE0);
    renderdata->glBindTexture(data->texture_type, data->texture);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, glScaleMode);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, glScaleMode);
}

static int GLES2_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *texturedata = NULL;
    GLenum status;

    data->drawstate.viewport_dirty = SDL_TRUE;

    if (!texture) {
        data->glBindFramebuffer(GL_FRAMEBUFFER, data->window_framebuffer);
    } else {
        texturedata = (GLES2_TextureData *)texture->driverdata;
        data->glBindFramebuffer(GL_FRAMEBUFFER, texturedata->fbo->FBO);
        /* TODO: check if texture pixel format allows this operation */
        data->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texturedata->texture_type, texturedata->texture, 0);
        /* Check FBO status */
        status = data->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            return SDL_SetError("glFramebufferTexture2D() failed");
        }
    }
    return 0;
}

static void GLES2_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;

    GLES2_ActivateRenderer(renderer);

    if (data->drawstate.texture == texture) {
        data->drawstate.texture = NULL;
    }
    if (data->drawstate.target == texture) {
        data->drawstate.target = NULL;
    }

    /* Destroy the texture */
    if (tdata) {
        if (tdata->texture && !tdata->texture_external) {
            data->glDeleteTextures(1, &tdata->texture);
        }
#if SDL_HAVE_YUV
        if (tdata->texture_v && !tdata->texture_v_external) {
            data->glDeleteTextures(1, &tdata->texture_v);
        }
        if (tdata->texture_u && !tdata->texture_u_external) {
            data->glDeleteTextures(1, &tdata->texture_u);
        }
#endif
        SDL_free(tdata->pixel_data);
        SDL_free(tdata);
        texture->driverdata = NULL;
    }
}

static SDL_Surface *GLES2_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    GLES2_RenderData *data = (GLES2_RenderData *)renderer->driverdata;
    SDL_PixelFormatEnum format = renderer->target ? renderer->target->format : SDL_PIXELFORMAT_RGBA32;
    int w, h;
    SDL_Surface *surface;

    surface = SDL_CreateSurface(rect->w, rect->h, format);
    if (!surface) {
        return NULL;
    }

    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

    data->glReadPixels(rect->x, renderer->target ? rect->y : (h - rect->y) - rect->h,
                       rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    if (GL_CheckError("glReadPixels()", renderer) < 0) {
        SDL_DestroySurface(surface);
        return NULL;
    }

    /* Flip the rows to be top-down if necessary */
    if (!renderer->target) {
        SDL_bool isstack;
        int length = rect->w * SDL_BYTESPERPIXEL(format);
        Uint8 *src = (Uint8 *)surface->pixels + (rect->h - 1) * surface->pitch;
        Uint8 *dst = (Uint8 *)surface->pixels;
        Uint8 *tmp = SDL_small_alloc(Uint8, length, &isstack);
        int rows = rect->h / 2;
        while (rows--) {
            SDL_memcpy(tmp, dst, length);
            SDL_memcpy(dst, src, length);
            SDL_memcpy(src, tmp, length);
            dst += surface->pitch;
            src -= surface->pitch;
        }
        SDL_small_free(tmp, isstack);
    }
    return surface;
}

static int GLES2_RenderPresent(SDL_Renderer *renderer)
{
    /* Tell the video driver to swap buffers */
    return SDL_GL_SwapWindow(renderer->window);
}

static int GLES2_SetVSync(SDL_Renderer *renderer, const int vsync)
{
    int retval;
    int interval = 0;
    if (vsync) {
        retval = SDL_GL_SetSwapInterval(1);
    } else {
        retval = SDL_GL_SetSwapInterval(0);
    }
    if (retval != 0) {
        return retval;
    }

    retval = SDL_GL_GetSwapInterval(&interval);
    if (retval < 0) {
        return retval;
    }

    if (interval != 0) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    } else {
        renderer->info.flags &= ~SDL_RENDERER_PRESENTVSYNC;
    }
    return retval;
}

/*************************************************************************************************
 * Renderer instantiation                                                                        *
 *************************************************************************************************/

static SDL_Renderer *GLES2_CreateRenderer(SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_Renderer *renderer;
    GLES2_RenderData *data;
    SDL_WindowFlags window_flags = 0; /* -Wconditional-uninitialized */
    GLint window_framebuffer;
    GLint value;
    int profile_mask = 0, major = 0, minor = 0;
    SDL_bool changed_window = SDL_FALSE;

    if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile_mask) < 0) {
        goto error;
    }
    if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major) < 0) {
        goto error;
    }
    if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor) < 0) {
        goto error;
    }

    SDL_SyncWindow(window);
    window_flags = SDL_GetWindowFlags(window);

    /* OpenGL ES 3.0 is a superset of OpenGL ES 2.0 */
    if (!(window_flags & SDL_WINDOW_OPENGL) ||
        profile_mask != SDL_GL_CONTEXT_PROFILE_ES || major < RENDERER_CONTEXT_MAJOR) {

        changed_window = SDL_TRUE;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, RENDERER_CONTEXT_MAJOR);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, RENDERER_CONTEXT_MINOR);

        if (SDL_RecreateWindow(window, (window_flags & ~(SDL_WINDOW_VULKAN | SDL_WINDOW_METAL)) | SDL_WINDOW_OPENGL) < 0) {
            goto error;
        }
    }

    /* Create the renderer struct */
    renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(SDL_Renderer));
    if (!renderer) {
        goto error;
    }
    renderer->magic = &SDL_renderer_magic;

    SDL_SetupRendererColorspace(renderer, create_props);

    if (renderer->output_colorspace != SDL_COLORSPACE_SRGB) {
        SDL_SetError("Unsupported output colorspace");
        SDL_free(renderer);
        goto error;
    }

    data = (GLES2_RenderData *)SDL_calloc(1, sizeof(GLES2_RenderData));
    if (!data) {
        SDL_free(renderer);
        goto error;
    }
    renderer->info = GLES2_RenderDriver.info;
    renderer->info.flags = SDL_RENDERER_ACCELERATED;
    renderer->driverdata = data;
    GLES2_InvalidateCachedState(renderer);
    renderer->window = window;

    /* Create an OpenGL ES 2.0 context */
    data->context = SDL_GL_CreateContext(window);
    if (!data->context) {
        SDL_free(renderer);
        SDL_free(data);
        goto error;
    }
    if (SDL_GL_MakeCurrent(window, data->context) < 0) {
        SDL_GL_DeleteContext(data->context);
        SDL_free(renderer);
        SDL_free(data);
        goto error;
    }

    if (GLES2_LoadFunctions(data) < 0) {
        SDL_GL_DeleteContext(data->context);
        SDL_free(renderer);
        SDL_free(data);
        goto error;
    }

    if (GLES2_CacheShaders(data) < 0) {
        SDL_GL_DeleteContext(data->context);
        SDL_free(renderer);
        SDL_free(data);
        goto error;
    }

#ifdef SDL_PLATFORM_WINRT
    /* DLudwig, 2013-11-29: ANGLE for WinRT doesn't seem to work unless VSync
     * is turned on.  Not doing so will freeze the screen's contents to that
     * of the first drawn frame.
     */
    SDL_SetBooleanProperty(create_props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_BOOLEAN, SDL_TRUE);
#endif

    if (SDL_GetBooleanProperty(create_props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_BOOLEAN, SDL_FALSE)) {
        SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }

    {
        int interval = 0;
        if (SDL_GL_GetSwapInterval(&interval) < 0) {
            /* Error */
        } else if (interval != 0) {
            renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
        }
    }

    /* Check for debug output support */
    if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &value) == 0 &&
        (value & SDL_GL_CONTEXT_DEBUG_FLAG)) {
        data->debug_enabled = SDL_TRUE;
    }

    value = 0;
    data->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    renderer->info.max_texture_width = value;
    value = 0;
    data->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    renderer->info.max_texture_height = value;

#if USE_VERTEX_BUFFER_OBJECTS
    /* we keep a few of these and cycle through them, so data can live for a few frames. */
    data->glGenBuffers(SDL_arraysize(data->vertex_buffers), data->vertex_buffers);
#endif

    data->framebuffers = NULL;
    data->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &window_framebuffer);
    data->window_framebuffer = (GLuint)window_framebuffer;

    /* Populate the function pointers for the module */
    renderer->WindowEvent = GLES2_WindowEvent;
    renderer->SupportsBlendMode = GLES2_SupportsBlendMode;
    renderer->CreateTexture = GLES2_CreateTexture;
    renderer->UpdateTexture = GLES2_UpdateTexture;
#if SDL_HAVE_YUV
    renderer->UpdateTextureYUV = GLES2_UpdateTextureYUV;
    renderer->UpdateTextureNV = GLES2_UpdateTextureNV;
#endif
    renderer->LockTexture = GLES2_LockTexture;
    renderer->UnlockTexture = GLES2_UnlockTexture;
    renderer->SetTextureScaleMode = GLES2_SetTextureScaleMode;
    renderer->SetRenderTarget = GLES2_SetRenderTarget;
    renderer->QueueSetViewport = GLES2_QueueNoOp;
    renderer->QueueSetDrawColor = GLES2_QueueNoOp;
    renderer->QueueDrawPoints = GLES2_QueueDrawPoints;
    renderer->QueueDrawLines = GLES2_QueueDrawLines;
    renderer->QueueGeometry = GLES2_QueueGeometry;
    renderer->InvalidateCachedState = GLES2_InvalidateCachedState;
    renderer->RunCommandQueue = GLES2_RunCommandQueue;
    renderer->RenderReadPixels = GLES2_RenderReadPixels;
    renderer->RenderPresent = GLES2_RenderPresent;
    renderer->DestroyTexture = GLES2_DestroyTexture;
    renderer->DestroyRenderer = GLES2_DestroyRenderer;
    renderer->SetVSync = GLES2_SetVSync;
#if SDL_HAVE_YUV
    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_YV12;
    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_IYUV;
    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_NV12;
    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_NV21;
#endif
#ifdef GL_TEXTURE_EXTERNAL_OES
    if (GLES2_CacheShader(data, GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES, GL_FRAGMENT_SHADER)) {
        renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_EXTERNAL_OES;
    }
#endif

    renderer->rect_index_order[0] = 0;
    renderer->rect_index_order[1] = 1;
    renderer->rect_index_order[2] = 3;
    renderer->rect_index_order[3] = 1;
    renderer->rect_index_order[4] = 3;
    renderer->rect_index_order[5] = 2;

    if (SDL_GL_ExtensionSupported("GL_EXT_blend_minmax")) {
        data->GL_EXT_blend_minmax_supported = SDL_TRUE;
    }

    /* Set up parameters for rendering */
    data->glActiveTexture(GL_TEXTURE0);
    data->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    data->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    data->glEnableVertexAttribArray(GLES2_ATTRIBUTE_POSITION);
    data->glEnableVertexAttribArray(GLES2_ATTRIBUTE_COLOR);
    data->glDisableVertexAttribArray(GLES2_ATTRIBUTE_TEXCOORD);

    data->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    data->drawstate.clear_color.r = 1.0f;
    data->drawstate.clear_color.g = 1.0f;
    data->drawstate.clear_color.b = 1.0f;
    data->drawstate.clear_color.a = 1.0f;
    data->drawstate.projection[3][0] = -1.0f;
    data->drawstate.projection[3][3] = 1.0f;

    GL_CheckError("", renderer);

    return renderer;

error:
    if (changed_window) {
        /* Uh oh, better try to put it back... */
        char *error = SDL_strdup(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile_mask);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
        SDL_RecreateWindow(window, window_flags);
        SDL_SetError("%s", error);
        SDL_free(error);
    }
    return NULL;
}

SDL_RenderDriver GLES2_RenderDriver = {
    GLES2_CreateRenderer,
    { "opengles2",
      (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
      4,
      { SDL_PIXELFORMAT_RGBA32,
        SDL_PIXELFORMAT_BGRA32,
        SDL_PIXELFORMAT_BGRX32,
        SDL_PIXELFORMAT_RGBX32 },
      0,
      0 }
};

#endif /* SDL_VIDEO_RENDER_OGL_ES2 */
