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

#if SDL_VIDEO_RENDER_VITA_GLES2 && !SDL_RENDER_DISABLED

#include "SDL_assert.h"
#include "SDL_hints.h"
#include "SDL_opengles2.h"
#include "../SDL_sysrender.h"
#include "../../video/SDL_blit.h"
#include "SDL_shaders_gles2vita.h"

//#include <psp2/gxm.h>
//#include <psp2/types.h>
//#include <psp2/kernel/sysmem.h>

/* To prevent unnecessary window recreation,
 * these should match the defaults selected in SDL_GL_ResetAttributes
 */
#define RENDERER_CONTEXT_MAJOR 2
#define RENDERER_CONTEXT_MINOR 0

/* Used to re-create the window with OpenGL ES capability */
extern int SDL_RecreateWindow(SDL_Window * window, Uint32 flags);

/*************************************************************************************************
 * Context structures                                                                            *
 *************************************************************************************************/

typedef struct VITA_GLES2_FBOList VITA_GLES2_FBOList;

struct VITA_GLES2_FBOList
{
   Uint32 w, h;
   GLuint FBO;
   VITA_GLES2_FBOList *next;
};

typedef struct VITA_GLES2_TextureData
{
    GLenum texture;
    GLenum texture_type;
    GLenum pixel_format;
    GLenum pixel_type;
    void *pixel_data;
    int pitch;
    /* YUV texture support */
    SDL_bool yuv;
    SDL_bool nv12;
    GLenum texture_v;
    GLenum texture_u;
    VITA_GLES2_FBOList *fbo;
} VITA_GLES2_TextureData;

typedef struct VITA_GLES2_ShaderCacheEntry
{
    GLuint id;
    VITA_GLES2_ShaderType type;
    const VITA_GLES2_ShaderInstance *instance;
    int references;
    struct VITA_GLES2_ShaderCacheEntry *prev;
    struct VITA_GLES2_ShaderCacheEntry *next;
} VITA_GLES2_ShaderCacheEntry;

typedef struct VITA_GLES2_ShaderCache
{
    int count;
    VITA_GLES2_ShaderCacheEntry *head;
} VITA_GLES2_ShaderCache;

typedef struct VITA_GLES2_ProgramCacheEntry
{
    GLuint id;
    VITA_GLES2_ShaderCacheEntry *vertex_shader;
    VITA_GLES2_ShaderCacheEntry *fragment_shader;
    GLuint uniform_locations[16];
    Uint32 color;
    GLfloat projection[4][4];
    struct VITA_GLES2_ProgramCacheEntry *prev;
    struct VITA_GLES2_ProgramCacheEntry *next;
} VITA_GLES2_ProgramCacheEntry;

typedef struct VITA_GLES2_ProgramCache
{
    int count;
    VITA_GLES2_ProgramCacheEntry *head;
    VITA_GLES2_ProgramCacheEntry *tail;
} VITA_GLES2_ProgramCache;

typedef enum
{
    VITA_GLES2_ATTRIBUTE_POSITION = 0,
    VITA_GLES2_ATTRIBUTE_TEXCOORD = 1,
    VITA_GLES2_ATTRIBUTE_ANGLE = 2,
    VITA_GLES2_ATTRIBUTE_CENTER = 3,
} VITA_GLES2_Attribute;

typedef enum
{
    VITA_GLES2_UNIFORM_PROJECTION,
    VITA_GLES2_UNIFORM_TEXTURE,
    VITA_GLES2_UNIFORM_COLOR,
    VITA_GLES2_UNIFORM_TEXTURE_U,
    VITA_GLES2_UNIFORM_TEXTURE_V
} VITA_GLES2_Uniform;

typedef enum
{
    VITA_GLES2_IMAGESOURCE_INVALID,
    VITA_GLES2_IMAGESOURCE_SOLID,
    VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR,
    VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB,
    VITA_GLES2_IMAGESOURCE_TEXTURE_RGB,
    VITA_GLES2_IMAGESOURCE_TEXTURE_BGR,
    VITA_GLES2_IMAGESOURCE_TEXTURE_YUV,
    VITA_GLES2_IMAGESOURCE_TEXTURE_NV12,
    VITA_GLES2_IMAGESOURCE_TEXTURE_NV21,
    VITA_GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES
} VITA_GLES2_ImageSource;

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
    SDL_bool is_copy_ex;
    Uint32 color;
    Uint32 clear_color;
    int drawablew;
    int drawableh;
    VITA_GLES2_ProgramCacheEntry *program;
    GLfloat projection[4][4];
} VITA_GLES2_DrawStateCache;

typedef struct VITA_GLES2_RenderData
{
    SDL_GLContext *context;

    SDL_bool debug_enabled;

#define SDL_PROC(ret,func,params) ret (APIENTRY *func) params;
#include "SDL_gles2funcs.h"
#undef SDL_PROC

    VITA_GLES2_FBOList *framebuffers;
    GLuint window_framebuffer;

    int shader_format_count;
    GLenum *shader_formats;
    VITA_GLES2_ShaderCache shader_cache;
    VITA_GLES2_ProgramCache program_cache;
    Uint8 clear_r, clear_g, clear_b, clear_a;

    GLuint vertex_buffers[8];
    size_t vertex_buffer_size[8];
    int current_vertex_buffer;
    VITA_GLES2_DrawStateCache drawstate;
} VITA_GLES2_RenderData;

#define VITA_GLES2_MAX_CACHED_PROGRAMS 8

static const float inv255f = 1.0f / 255.0f;


SDL_FORCE_INLINE const char*
GL_TranslateError (GLenum error)
{
#define GL_ERROR_TRANSLATE(e) case e: return #e;
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

SDL_FORCE_INLINE void
GL_ClearErrors(SDL_Renderer *renderer)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *) renderer->driverdata;

    if (!data->debug_enabled) {
        return;
    }
    while (data->glGetError() != GL_NO_ERROR) {
        /* continue; */
    }
}

SDL_FORCE_INLINE int
GL_CheckAllErrors (const char *prefix, SDL_Renderer *renderer, const char *file, int line, const char *function)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *) renderer->driverdata;
    int ret = 0;

    if (!data->debug_enabled) {
        return 0;
    }
    /* check gl errors (can return multiple errors) */
    for (;;) {
        GLenum error = data->glGetError();
        if (error != GL_NO_ERROR) {
            if (prefix == NULL || prefix[0] == '\0') {
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

static int VITA_GLES2_LoadFunctions(VITA_GLES2_RenderData * data)
{
#define SDL_PROC(ret,func,params) data->func=func;
#include "SDL_gles2funcs.h"
#undef SDL_PROC
    return 0;
}

static VITA_GLES2_FBOList *
VITA_GLES2_GetFBO(VITA_GLES2_RenderData *data, Uint32 w, Uint32 h)
{
   VITA_GLES2_FBOList *result = data->framebuffers;
   while ((result) && ((result->w != w) || (result->h != h)) ) {
       result = result->next;
   }
   if (result == NULL) {
       result = SDL_malloc(sizeof(VITA_GLES2_FBOList));
       result->w = w;
       result->h = h;
       data->glGenFramebuffers(1, &result->FBO);
       result->next = data->framebuffers;
       data->framebuffers = result;
   }
   return result;
}

static int
VITA_GLES2_ActivateRenderer(SDL_Renderer * renderer)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;

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

static void
VITA_GLES2_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;

    if (event->event == SDL_WINDOWEVENT_MINIMIZED) {
        /* According to Apple documentation, we need to finish drawing NOW! */
        data->glFinish();
    }
}

static int
VITA_GLES2_GetOutputSize(SDL_Renderer * renderer, int *w, int *h)
{
    SDL_GL_GetDrawableSize(renderer->window, w, h);
    return 0;
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
    default:
        return GL_INVALID_ENUM;
    }
}

static SDL_bool
VITA_GLES2_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode)
{
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
    return SDL_TRUE;
}


static void
VITA_GLES2_EvictShader(VITA_GLES2_RenderData *data, VITA_GLES2_ShaderCacheEntry *entry)
{
    /* Unlink the shader from the cache */
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (data->shader_cache.head == entry) {
        data->shader_cache.head = entry->next;
    }
    --data->shader_cache.count;

    /* Deallocate the shader */
    data->glDeleteShader(entry->id);
    SDL_free(entry);
}

static VITA_GLES2_ProgramCacheEntry *
VITA_GLES2_CacheProgram(VITA_GLES2_RenderData *data, VITA_GLES2_ShaderCacheEntry *vertex,
                   VITA_GLES2_ShaderCacheEntry *fragment)
{
    VITA_GLES2_ProgramCacheEntry *entry;
    VITA_GLES2_ShaderCacheEntry *shaderEntry;
    GLint linkSuccessful;

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
    entry = (VITA_GLES2_ProgramCacheEntry *)SDL_calloc(1, sizeof(VITA_GLES2_ProgramCacheEntry));
    if (!entry) {
        SDL_OutOfMemory();
        return NULL;
    }
    entry->vertex_shader = vertex;
    entry->fragment_shader = fragment;

    /* Create the program and link it */
    entry->id = data->glCreateProgram();
    data->glAttachShader(entry->id, vertex->id);
    data->glAttachShader(entry->id, fragment->id);
    data->glBindAttribLocation(entry->id, VITA_GLES2_ATTRIBUTE_POSITION, "a_position");
    data->glBindAttribLocation(entry->id, VITA_GLES2_ATTRIBUTE_TEXCOORD, "a_texCoord");
    data->glBindAttribLocation(entry->id, VITA_GLES2_ATTRIBUTE_ANGLE, "a_angle");
    data->glBindAttribLocation(entry->id, VITA_GLES2_ATTRIBUTE_CENTER, "a_center");
    data->glLinkProgram(entry->id);
    data->glGetProgramiv(entry->id, GL_LINK_STATUS, &linkSuccessful);
    if (!linkSuccessful) {
        data->glDeleteProgram(entry->id);
        SDL_free(entry);
        SDL_SetError("Failed to link shader program");
        return NULL;
    }

    /* Predetermine locations of uniform variables */
    entry->uniform_locations[VITA_GLES2_UNIFORM_PROJECTION] =
        data->glGetUniformLocation(entry->id, "u_projection");
    entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE_V] =
        data->glGetUniformLocation(entry->id, "u_texture_v");
    entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE_U] =
        data->glGetUniformLocation(entry->id, "u_texture_u");
    entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE] =
        data->glGetUniformLocation(entry->id, "u_texture");
    entry->uniform_locations[VITA_GLES2_UNIFORM_COLOR] =
        data->glGetUniformLocation(entry->id, "u_color");

    entry->color = 0;

    data->glUseProgram(entry->id);
    if (entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE_V] != -1) {
        data->glUniform1i(entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE_V], 2);  /* always texture unit 2. */
    }
    if (entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE_U] != -1) {
        data->glUniform1i(entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE_U], 1);  /* always texture unit 1. */
    }
    if (entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE] != -1) {
        data->glUniform1i(entry->uniform_locations[VITA_GLES2_UNIFORM_TEXTURE], 0);  /* always texture unit 0. */
    }
    if (entry->uniform_locations[VITA_GLES2_UNIFORM_PROJECTION] != -1) {
        data->glUniformMatrix4fv(entry->uniform_locations[VITA_GLES2_UNIFORM_PROJECTION], 1, GL_FALSE, (GLfloat *)entry->projection);
    }
    if (entry->uniform_locations[VITA_GLES2_UNIFORM_COLOR] != -1) {
        data->glUniform4f(entry->uniform_locations[VITA_GLES2_UNIFORM_COLOR], 0.0f, 0.0f, 0.0f, 0.0f);
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

    /* Increment the refcount of the shaders we're using */
    ++vertex->references;
    ++fragment->references;

    /* Evict the last entry from the cache if we exceed the limit */
    if (data->program_cache.count > VITA_GLES2_MAX_CACHED_PROGRAMS) {
        shaderEntry = data->program_cache.tail->vertex_shader;
        if (--shaderEntry->references <= 0) {
            VITA_GLES2_EvictShader(data, shaderEntry);
        }
        shaderEntry = data->program_cache.tail->fragment_shader;
        if (--shaderEntry->references <= 0) {
            VITA_GLES2_EvictShader(data, shaderEntry);
        }
        data->glDeleteProgram(data->program_cache.tail->id);
        data->program_cache.tail = data->program_cache.tail->prev;
        if (data->program_cache.tail != NULL) {
            SDL_free(data->program_cache.tail->next);
            data->program_cache.tail->next = NULL;
        }
        --data->program_cache.count;
    }
    return entry;
}

static VITA_GLES2_ShaderCacheEntry *
VITA_GLES2_CacheShader(VITA_GLES2_RenderData *data, VITA_GLES2_ShaderType type)
{
    const VITA_GLES2_Shader *shader;
    const VITA_GLES2_ShaderInstance *instance = NULL;
    VITA_GLES2_ShaderCacheEntry *entry = NULL;
    GLint compileSuccessful = GL_FALSE;
    int i, j;

    /* Find the corresponding shader */
    shader = VITA_GLES2_GetShader(type);
    if (!shader) {
        SDL_SetError("No shader matching the requested characteristics was found");
        return NULL;
    }

    /* Find a matching shader instance that's supported on this hardware */
    for (i = 0; i < shader->instance_count && !instance; ++i) {
        for (j = 0; j < data->shader_format_count && !instance; ++j) {
            if (!shader->instances[i]) {
                continue;
            }
            if (shader->instances[i]->format != data->shader_formats[j]) {
                continue;
            }
            instance = shader->instances[i];
        }
    }
    if (!instance) {
        SDL_SetError("The specified shader cannot be loaded on the current platform");
        return NULL;
    }

    /* Check if we've already cached this shader */
    entry = data->shader_cache.head;
    while (entry) {
        if (entry->instance == instance) {
            break;
        }
        entry = entry->next;
    }
    if (entry) {
        return entry;
    }

    /* Create a shader cache entry */
    entry = (VITA_GLES2_ShaderCacheEntry *)SDL_calloc(1, sizeof(VITA_GLES2_ShaderCacheEntry));
    if (!entry) {
        SDL_OutOfMemory();
        return NULL;
    }
    entry->type = type;
    entry->instance = instance;

    /* Compile or load the selected shader instance */
    entry->id = data->glCreateShader(instance->type);
    if (instance->format == (GLenum)-1) {
        data->glShaderSource(entry->id, 1, (const char **)(char *)&instance->data, NULL);
        data->glCompileShader(entry->id);
        data->glGetShaderiv(entry->id, GL_COMPILE_STATUS, &compileSuccessful);
    } else {
        data->glShaderBinary(1, &entry->id, instance->format, instance->data, instance->length);
        compileSuccessful = GL_TRUE;
    }
    if (!compileSuccessful) {
        SDL_bool isstack = SDL_FALSE;
        char *info = NULL;
        int length = 0;

        data->glGetShaderiv(entry->id, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            info = SDL_small_alloc(char, length, &isstack);
            if (info) {
                data->glGetShaderInfoLog(entry->id, length, &length, info);
            }
        }
        if (info) {
            SDL_SetError("Failed to load the shader: %s", info);
            SDL_small_free(info, isstack);
        } else {
            SDL_SetError("Failed to load the shader");
        }
        data->glDeleteShader(entry->id);
        SDL_free(entry);
        return NULL;
    }

    /* Link the shader entry in at the front of the cache */
    if (data->shader_cache.head) {
        entry->next = data->shader_cache.head;
        data->shader_cache.head->prev = entry;
    }
    data->shader_cache.head = entry;
    ++data->shader_cache.count;
    return entry;
}

static int
VITA_GLES2_SelectProgram(VITA_GLES2_RenderData *data, VITA_GLES2_ImageSource source, int w, int h)
{
    VITA_GLES2_ShaderCacheEntry *vertex = NULL;
    VITA_GLES2_ShaderCacheEntry *fragment = NULL;
    VITA_GLES2_ShaderType vtype, ftype;
    VITA_GLES2_ProgramCacheEntry *program;

    /* Select an appropriate shader pair for the specified modes */
    vtype = VITA_GLES2_SHADER_VERTEX_DEFAULT;
    switch (source) {
    case VITA_GLES2_IMAGESOURCE_SOLID:
        ftype = VITA_GLES2_SHADER_FRAGMENT_SOLID_SRC;
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR:
        ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_ABGR_SRC;
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB:
        ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_ARGB_SRC;
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_RGB:
        ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_RGB_SRC;
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_BGR:
        ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_BGR_SRC;
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_YUV:
        switch (SDL_GetYUVConversionModeForResolution(w, h)) {
        case SDL_YUV_CONVERSION_JPEG:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_YUV_JPEG_SRC;
            break;
        case SDL_YUV_CONVERSION_BT601:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT601_SRC;
            break;
        case SDL_YUV_CONVERSION_BT709:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT709_SRC;
            break;
        default:
            SDL_SetError("Unsupported YUV conversion mode: %d\n", SDL_GetYUVConversionModeForResolution(w, h));
            goto fault;
        }
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_NV12:
        switch (SDL_GetYUVConversionModeForResolution(w, h)) {
        case SDL_YUV_CONVERSION_JPEG:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV12_JPEG_SRC;
            break;
        case SDL_YUV_CONVERSION_BT601:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV12_BT601_SRC;
            break;
        case SDL_YUV_CONVERSION_BT709:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV12_BT709_SRC;
            break;
        default:
            SDL_SetError("Unsupported YUV conversion mode: %d\n", SDL_GetYUVConversionModeForResolution(w, h));
            goto fault;
        }
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_NV21:
        switch (SDL_GetYUVConversionModeForResolution(w, h)) {
        case SDL_YUV_CONVERSION_JPEG:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV21_JPEG_SRC;
            break;
        case SDL_YUV_CONVERSION_BT601:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT601_SRC;
            break;
        case SDL_YUV_CONVERSION_BT709:
            ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT709_SRC;
            break;
        default:
            SDL_SetError("Unsupported YUV conversion mode: %d\n", SDL_GetYUVConversionModeForResolution(w, h));
            goto fault;
        }
        break;
    case VITA_GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES:
        ftype = VITA_GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES_SRC;
        break;
    default:
        goto fault;
    }

    /* Load the requested shaders */
    vertex = VITA_GLES2_CacheShader(data, vtype);
    if (!vertex) {
        goto fault;
    }
    fragment = VITA_GLES2_CacheShader(data, ftype);
    if (!fragment) {
        goto fault;
    }

    /* Check if we need to change programs at all */
    if (data->drawstate.program &&
        data->drawstate.program->vertex_shader == vertex &&
        data->drawstate.program->fragment_shader == fragment) {
        return 0;
    }

    /* Generate a matching program */
    program = VITA_GLES2_CacheProgram(data, vertex, fragment);
    if (!program) {
        goto fault;
    }

    /* Select that program in OpenGL */
    data->glUseProgram(program->id);

    /* Set the current program */
    data->drawstate.program = program;

    /* Clean up and return */
    return 0;
fault:
    if (vertex && vertex->references <= 0) {
        VITA_GLES2_EvictShader(data, vertex);
    }
    if (fragment && fragment->references <= 0) {
        VITA_GLES2_EvictShader(data, fragment);
    }
    data->drawstate.program = NULL;
    return -1;
}

static int
VITA_GLES2_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;  /* nothing to do in this backend. */
}

static int
VITA_GLES2_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{

    GLfloat *verts = (GLfloat *) SDL_AllocateRenderVertices(renderer, count * 2 * sizeof (GLfloat), 4, &cmd->data.draw.first);

    int i;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;
    for (i = 0; i < count; i++) {
        *(verts++) = 0.5f + points[i].x;
        *(verts++) = 0.5f + points[i].y;
    }

    return 0;
}

static int
VITA_GLES2_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    GLfloat *verts = (GLfloat *) SDL_AllocateRenderVertices(renderer, count * 8 * sizeof (GLfloat), 4, &cmd->data.draw.first);

    int i;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++) {
        const SDL_FRect *rect = &rects[i];
        const GLfloat minx = rect->x;
        const GLfloat maxx = rect->x + rect->w;
        const GLfloat miny = rect->y;
        const GLfloat maxy = rect->y + rect->h;
        *(verts++) = minx;
        *(verts++) = miny;
        *(verts++) = maxx;
        *(verts++) = miny;
        *(verts++) = minx;
        *(verts++) = maxy;
        *(verts++) = maxx;
        *(verts++) = maxy;
    }

    return 0;
}

static int
VITA_GLES2_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                          const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    GLfloat minx, miny, maxx, maxy;
    GLfloat minu, maxu, minv, maxv;
    GLfloat *verts = (GLfloat *) SDL_AllocateRenderVertices(renderer, 16 * sizeof (GLfloat), 4, &cmd->data.draw.first);

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = 1;

    minx = dstrect->x;
    miny = dstrect->y;
    maxx = dstrect->x + dstrect->w;
    maxy = dstrect->y + dstrect->h;

    minu = (GLfloat) srcrect->x / texture->w;
    maxu = (GLfloat) (srcrect->x + srcrect->w) / texture->w;
    minv = (GLfloat) srcrect->y / texture->h;
    maxv = (GLfloat) (srcrect->y + srcrect->h) / texture->h;

    *(verts++) = minx;
    *(verts++) = miny;
    *(verts++) = maxx;
    *(verts++) = miny;
    *(verts++) = minx;
    *(verts++) = maxy;
    *(verts++) = maxx;
    *(verts++) = maxy;

    *(verts++) = minu;
    *(verts++) = minv;
    *(verts++) = maxu;
    *(verts++) = minv;
    *(verts++) = minu;
    *(verts++) = maxv;
    *(verts++) = maxu;
    *(verts++) = maxv;

    return 0;
}

static int
VITA_GLES2_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                        const SDL_Rect * srcquad, const SDL_FRect * dstrect,
                        const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    /* render expects cos value - 1 (see VITA_GLES2_VertexSrc_Default_) */
    const float radian_angle = (float)(M_PI * (360.0 - angle) / 180.0);
    const GLfloat s = (GLfloat) SDL_sin(radian_angle);
    const GLfloat c = (GLfloat) SDL_cos(radian_angle) - 1.0f;
    const GLfloat centerx = center->x + dstrect->x;
    const GLfloat centery = center->y + dstrect->y;
    GLfloat minx, miny, maxx, maxy;
    GLfloat minu, maxu, minv, maxv;
    GLfloat *verts = (GLfloat *) SDL_AllocateRenderVertices(renderer, 32 * sizeof (GLfloat), 4, &cmd->data.draw.first);

    if (!verts) {
        return -1;
    }

    if (flip & SDL_FLIP_HORIZONTAL) {
        minx = dstrect->x + dstrect->w;
        maxx = dstrect->x;
    } else {
        minx = dstrect->x;
        maxx = dstrect->x + dstrect->w;
    }

    if (flip & SDL_FLIP_VERTICAL) {
        miny = dstrect->y + dstrect->h;
        maxy = dstrect->y;
    } else {
        miny = dstrect->y;
        maxy = dstrect->y + dstrect->h;
    }

    minu = ((GLfloat) srcquad->x) / ((GLfloat) texture->w);
    maxu = ((GLfloat) (srcquad->x + srcquad->w)) / ((GLfloat) texture->w);
    minv = ((GLfloat) srcquad->y) / ((GLfloat) texture->h);
    maxv = ((GLfloat) (srcquad->y + srcquad->h)) / ((GLfloat) texture->h);


    cmd->data.draw.count = 1;

    *(verts++) = minx;
    *(verts++) = miny;
    *(verts++) = maxx;
    *(verts++) = miny;
    *(verts++) = minx;
    *(verts++) = maxy;
    *(verts++) = maxx;
    *(verts++) = maxy;

    *(verts++) = minu;
    *(verts++) = minv;
    *(verts++) = maxu;
    *(verts++) = minv;
    *(verts++) = minu;
    *(verts++) = maxv;
    *(verts++) = maxu;
    *(verts++) = maxv;

    *(verts++) = s;
    *(verts++) = c;
    *(verts++) = s;
    *(verts++) = c;
    *(verts++) = s;
    *(verts++) = c;
    *(verts++) = s;
    *(verts++) = c;

    *(verts++) = centerx;
    *(verts++) = centery;
    *(verts++) = centerx;
    *(verts++) = centery;
    *(verts++) = centerx;
    *(verts++) = centery;
    *(verts++) = centerx;
    *(verts++) = centery;

    return 0;
}

static int
SetDrawState(VITA_GLES2_RenderData *data, const SDL_RenderCommand *cmd, const VITA_GLES2_ImageSource imgsrc)
{
    const SDL_bool was_copy_ex = data->drawstate.is_copy_ex;
    const SDL_bool is_copy_ex = (cmd->command == SDL_RENDERCMD_COPY_EX);
    SDL_Texture *texture = cmd->data.draw.texture;
    const SDL_BlendMode blend = cmd->data.draw.blend;
    VITA_GLES2_ProgramCacheEntry *program;

    SDL_assert((texture != NULL) == (imgsrc != VITA_GLES2_IMAGESOURCE_SOLID));

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

    if (texture != data->drawstate.texture) {
        if ((texture != NULL) != data->drawstate.texturing) {
            if (texture == NULL) {
                data->glDisableVertexAttribArray((GLenum) VITA_GLES2_ATTRIBUTE_TEXCOORD);
                data->drawstate.texturing = SDL_FALSE;
            } else {
                data->glEnableVertexAttribArray((GLenum) VITA_GLES2_ATTRIBUTE_TEXCOORD);
                data->drawstate.texturing = SDL_TRUE;
            }
        }

        if (texture) {
            VITA_GLES2_TextureData *tdata = (VITA_GLES2_TextureData *) texture->driverdata;
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
            data->glBindTexture(tdata->texture_type, tdata->texture);
        }

        data->drawstate.texture = texture;
    }

    if (texture) {
        data->glVertexAttribPointer(VITA_GLES2_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *) (cmd->data.draw.first + (sizeof (GLfloat) * 8)));
    }

    if (VITA_GLES2_SelectProgram(data, imgsrc, texture ? texture->w : 0, texture ? texture->h : 0) < 0) {
        return -1;
    }

    program = data->drawstate.program;

    if (program->uniform_locations[VITA_GLES2_UNIFORM_PROJECTION] != -1) {
        if (SDL_memcmp(program->projection, data->drawstate.projection, sizeof (data->drawstate.projection)) != 0) {
            data->glUniformMatrix4fv(program->uniform_locations[VITA_GLES2_UNIFORM_PROJECTION], 1, GL_FALSE, (GLfloat *)data->drawstate.projection);
            SDL_memcpy(program->projection, data->drawstate.projection, sizeof (data->drawstate.projection));
        }
    }

    if (program->uniform_locations[VITA_GLES2_UNIFORM_COLOR] != -1) {
        if (data->drawstate.color != program->color) {
            const Uint8 r = (data->drawstate.color >> 16) & 0xFF;
            const Uint8 g = (data->drawstate.color >> 8) & 0xFF;
            const Uint8 b = (data->drawstate.color >> 0) & 0xFF;
            const Uint8 a = (data->drawstate.color >> 24) & 0xFF;
            data->glUniform4f(program->uniform_locations[VITA_GLES2_UNIFORM_COLOR], r * inv255f, g * inv255f, b * inv255f, a * inv255f);
            program->color = data->drawstate.color;
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
    data->glVertexAttribPointer(VITA_GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *) cmd->data.draw.first);

    if (is_copy_ex != was_copy_ex) {
        if (is_copy_ex) {
            data->glEnableVertexAttribArray((GLenum) VITA_GLES2_ATTRIBUTE_ANGLE);
            data->glEnableVertexAttribArray((GLenum) VITA_GLES2_ATTRIBUTE_CENTER);
        } else {
            data->glDisableVertexAttribArray((GLenum) VITA_GLES2_ATTRIBUTE_ANGLE);
            data->glDisableVertexAttribArray((GLenum) VITA_GLES2_ATTRIBUTE_CENTER);
        }
        data->drawstate.is_copy_ex = is_copy_ex;
    }

    if (is_copy_ex) {
        data->glVertexAttribPointer(VITA_GLES2_ATTRIBUTE_ANGLE, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *) (cmd->data.draw.first + (sizeof (GLfloat) * 16)));
        data->glVertexAttribPointer(VITA_GLES2_ATTRIBUTE_CENTER, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *) (cmd->data.draw.first + (sizeof (GLfloat) * 24)));
    }

    return 0;
}

static int
SetCopyState(SDL_Renderer *renderer, const SDL_RenderCommand *cmd)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *) renderer->driverdata;
    VITA_GLES2_ImageSource sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR;
    SDL_Texture *texture = cmd->data.draw.texture;

    /* Pick an appropriate shader */
    if (renderer->target) {
        /* Check if we need to do color mapping between the source and render target textures */
        if (renderer->target->format != texture->format) {
            switch (texture->format) {
            case SDL_PIXELFORMAT_ARGB8888:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_ABGR8888:
                case SDL_PIXELFORMAT_BGR888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                case SDL_PIXELFORMAT_RGB888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR;
                    break;
                }
                break;
            case SDL_PIXELFORMAT_ABGR8888:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_ARGB8888:
                case SDL_PIXELFORMAT_RGB888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                case SDL_PIXELFORMAT_BGR888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR;
                    break;
                }
                break;
            case SDL_PIXELFORMAT_RGB888:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_ABGR8888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                case SDL_PIXELFORMAT_ARGB8888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_BGR;
                    break;
                case SDL_PIXELFORMAT_BGR888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                }
                break;
            case SDL_PIXELFORMAT_BGR888:
                switch (renderer->target->format) {
                case SDL_PIXELFORMAT_ABGR8888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_BGR;
                    break;
                case SDL_PIXELFORMAT_ARGB8888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_RGB;
                    break;
                case SDL_PIXELFORMAT_RGB888:
                    sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB;
                    break;
                }
                break;
            case SDL_PIXELFORMAT_IYUV:
            case SDL_PIXELFORMAT_YV12:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_YUV;
                break;
            case SDL_PIXELFORMAT_NV12:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_NV12;
                break;
            case SDL_PIXELFORMAT_NV21:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_NV21;
                break;
            case SDL_PIXELFORMAT_EXTERNAL_OES:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES;
                break;
            default:
                return SDL_SetError("Unsupported texture format");
            }
        } else {
            sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR;   /* Texture formats match, use the non color mapping shader (even if the formats are not ABGR) */
        }
    } else {
        switch (texture->format) {
            case SDL_PIXELFORMAT_ARGB8888:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ARGB;
                break;
            case SDL_PIXELFORMAT_ABGR8888:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_ABGR;
                break;
            case SDL_PIXELFORMAT_RGB888:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_RGB;
                break;
            case SDL_PIXELFORMAT_BGR888:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_BGR;
                break;
            case SDL_PIXELFORMAT_IYUV:
            case SDL_PIXELFORMAT_YV12:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_YUV;
                break;
            case SDL_PIXELFORMAT_NV12:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_NV12;
                break;
            case SDL_PIXELFORMAT_NV21:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_NV21;
                break;
            case SDL_PIXELFORMAT_EXTERNAL_OES:
                sourceType = VITA_GLES2_IMAGESOURCE_TEXTURE_EXTERNAL_OES;
                break;
            default:
                return SDL_SetError("Unsupported texture format");
        }
    }

    return SetDrawState(data, cmd, sourceType);
}

static int
VITA_GLES2_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *) renderer->driverdata;
    const SDL_bool colorswap = (renderer->target && (renderer->target->format == SDL_PIXELFORMAT_ARGB8888 || renderer->target->format == SDL_PIXELFORMAT_RGB888));
    const int vboidx = data->current_vertex_buffer;
    const GLuint vbo = data->vertex_buffers[vboidx];
    size_t i;

    if (VITA_GLES2_ActivateRenderer(renderer) < 0) {
        return -1;
    }

    data->drawstate.target = renderer->target;
    if (!data->drawstate.target) {
        SDL_GL_GetDrawableSize(renderer->window, &data->drawstate.drawablew, &data->drawstate.drawableh);
    }

    /* upload the new VBO data for this set of commands. */
    /* NULL/zero vertices could happen SetRenderTarget is the first render command */
    if (vertsize > 0 && vertices != NULL)
    {
        data->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        if (data->vertex_buffer_size[vboidx] < vertsize) {
            data->glBufferData(GL_ARRAY_BUFFER, vertsize, vertices, GL_DYNAMIC_DRAW);
            data->vertex_buffer_size[vboidx] = vertsize;
        } else {
            data->glBufferSubData(GL_ARRAY_BUFFER, 0, vertsize, vertices);
        }
    }

    /* cycle through a few VBOs so the GL has some time with the data before we replace it. */
    data->current_vertex_buffer++;
    if (data->current_vertex_buffer >= SDL_arraysize(data->vertex_buffers)) {
        data->current_vertex_buffer = 0;
    }

    data->glDisable(GL_DEPTH_TEST);
    data->glDisable(GL_CULL_FACE);

    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETDRAWCOLOR: {
                const Uint8 r = colorswap ? cmd->data.color.b : cmd->data.color.r;
                const Uint8 g = cmd->data.color.g;
                const Uint8 b = colorswap ? cmd->data.color.r : cmd->data.color.b;
                const Uint8 a = cmd->data.color.a;
                data->drawstate.color = ((a << 24) | (r << 16) | (g << 8) | b);
                break;
            }

            case SDL_RENDERCMD_SETVIEWPORT: {
                SDL_Rect *viewport = &data->drawstate.viewport;
                if (SDL_memcmp(viewport, &cmd->data.viewport.rect, sizeof (SDL_Rect)) != 0) {
                    SDL_memcpy(viewport, &cmd->data.viewport.rect, sizeof (SDL_Rect));
                    data->drawstate.viewport_dirty = SDL_TRUE;
                }
                break;
            }

            case SDL_RENDERCMD_SETCLIPRECT: {
                const SDL_Rect *rect = &cmd->data.cliprect.rect;
                if (data->drawstate.cliprect_enabled != cmd->data.cliprect.enabled) {
                    data->drawstate.cliprect_enabled = cmd->data.cliprect.enabled;
                    data->drawstate.cliprect_enabled_dirty = SDL_TRUE;
                }

                if (SDL_memcmp(&data->drawstate.cliprect, rect, sizeof (SDL_Rect)) != 0) {
                    SDL_memcpy(&data->drawstate.cliprect, rect, sizeof (SDL_Rect));
                    data->drawstate.cliprect_dirty = SDL_TRUE;
                }
                break;
            }

            case SDL_RENDERCMD_CLEAR: {
                const Uint8 r = colorswap ? cmd->data.color.b : cmd->data.color.r;
                const Uint8 g = cmd->data.color.g;
                const Uint8 b = colorswap ? cmd->data.color.r : cmd->data.color.b;
                const Uint8 a = cmd->data.color.a;
                const Uint32 color = ((a << 24) | (r << 16) | (g << 8) | b);
                if (color != data->drawstate.clear_color) {
                    const GLfloat fr = ((GLfloat) r) * inv255f;
                    const GLfloat fg = ((GLfloat) g) * inv255f;
                    const GLfloat fb = ((GLfloat) b) * inv255f;
                    const GLfloat fa = ((GLfloat) a) * inv255f;
                    data->glClearColor(fr, fg, fb, fa);
                    data->drawstate.clear_color = color;
                }

                if (data->drawstate.cliprect_enabled || data->drawstate.cliprect_enabled_dirty) {
                    data->glDisable(GL_SCISSOR_TEST);
                    data->drawstate.cliprect_enabled_dirty = data->drawstate.cliprect_enabled;
                }

                data->glClear(GL_COLOR_BUFFER_BIT);
                break;
            }

            case SDL_RENDERCMD_DRAW_POINTS: {
                if (SetDrawState(data, cmd, VITA_GLES2_IMAGESOURCE_SOLID) == 0) {
                    data->glDrawArrays(GL_POINTS, 0, (GLsizei) cmd->data.draw.count);
                }
                break;
            }

            case SDL_RENDERCMD_DRAW_LINES: {
                const GLfloat *verts = (GLfloat *) (((Uint8 *) vertices) + cmd->data.draw.first);
                const size_t count = cmd->data.draw.count;
                if (SetDrawState(data, cmd, VITA_GLES2_IMAGESOURCE_SOLID) == 0) {
                    if (count > 2 && (verts[0] == verts[(count-1)*2]) && (verts[1] == verts[(count*2)-1])) {
                        /* GL_LINE_LOOP takes care of the final segment */
                        data->glDrawArrays(GL_LINE_LOOP, 0, (GLsizei) (count - 1));
                    } else {
                        data->glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) count);
                        /* We need to close the endpoint of the line */
                        data->glDrawArrays(GL_POINTS, (GLsizei) (count - 1), 1);
                    }
                }
                break;
            }

            case SDL_RENDERCMD_FILL_RECTS: {
                const size_t count = cmd->data.draw.count;
                size_t offset = 0;
                if (SetDrawState(data, cmd, VITA_GLES2_IMAGESOURCE_SOLID) == 0) {
                    for (i = 0; i < count; ++i, offset += 4) {
                        data->glDrawArrays(GL_TRIANGLE_STRIP, (GLsizei) offset, 4);
                    }
                }
                break;
            }

            case SDL_RENDERCMD_COPY:
            case SDL_RENDERCMD_COPY_EX: {
                if (SetCopyState(renderer, cmd) == 0) {
                    data->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
                break;
            }

            case SDL_RENDERCMD_NO_OP:
                break;
        }

        cmd = cmd->next;
    }

    return GL_CheckError("", renderer);
}

static void
VITA_GLES2_DestroyRenderer(SDL_Renderer *renderer)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;

    /* Deallocate everything */
    if (data) {
        VITA_GLES2_ActivateRenderer(renderer);

        {
            VITA_GLES2_ShaderCacheEntry *entry;
            VITA_GLES2_ShaderCacheEntry *next;
            entry = data->shader_cache.head;
            while (entry) {
                data->glDeleteShader(entry->id);
                next = entry->next;
                SDL_free(entry);
                entry = next;
            }
        }
        {
            VITA_GLES2_ProgramCacheEntry *entry;
            VITA_GLES2_ProgramCacheEntry *next;
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
                VITA_GLES2_FBOList *nextnode = data->framebuffers->next;
                data->glDeleteFramebuffers(1, &data->framebuffers->FBO);
                GL_CheckError("", renderer);
                SDL_free(data->framebuffers);
                data->framebuffers = nextnode;
            }

            data->glDeleteBuffers(SDL_arraysize(data->vertex_buffers), data->vertex_buffers);
            GL_CheckError("", renderer);

            SDL_GL_DeleteContext(data->context);
        }

        SDL_free(data->shader_formats);
        SDL_free(data);
    }
    SDL_free(renderer);
}

static int
VITA_GLES2_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_GLES2_RenderData *renderdata = (VITA_GLES2_RenderData *)renderer->driverdata;
    VITA_GLES2_TextureData *data;
    GLenum format;
    GLenum type;
    GLenum scaleMode;

    VITA_GLES2_ActivateRenderer(renderer);

    renderdata->drawstate.texture = NULL;  /* we trash this state. */

    /* Determine the corresponding GLES texture format params */
    switch (texture->format)
    {
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_ABGR8888:
    case SDL_PIXELFORMAT_RGB888:
    case SDL_PIXELFORMAT_BGR888:
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        break;
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_BYTE;
        break;
    default:
        return SDL_SetError("Texture format not supported");
    }

    if (texture->format == SDL_PIXELFORMAT_EXTERNAL_OES &&
        texture->access != SDL_TEXTUREACCESS_STATIC) {
        return SDL_SetError("Unsupported texture access for SDL_PIXELFORMAT_EXTERNAL_OES");
    }

    /* Allocate a texture struct */
    data = (VITA_GLES2_TextureData *)SDL_calloc(1, sizeof(VITA_GLES2_TextureData));
    if (!data) {
        return SDL_OutOfMemory();
    }
    data->texture = 0;
    data->texture_type = GL_TEXTURE_2D;
    data->pixel_format = format;
    data->pixel_type = type;
    data->yuv = ((texture->format == SDL_PIXELFORMAT_IYUV) || (texture->format == SDL_PIXELFORMAT_YV12));
    data->nv12 = ((texture->format == SDL_PIXELFORMAT_NV12) || (texture->format == SDL_PIXELFORMAT_NV21));
    data->texture_u = 0;
    data->texture_v = 0;
    scaleMode = (texture->scaleMode == SDL_ScaleModeNearest) ? GL_NEAREST : GL_LINEAR;

    /* Allocate a blob for image renderdata */
    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        size_t size;
        data->pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
        size = texture->h * data->pitch;
        if (data->yuv) {
            /* Need to add size for the U and V planes */
            size += 2 * ((texture->h + 1) / 2) * ((data->pitch + 1) / 2);
        } else if (data->nv12) {
            /* Need to add size for the U/V plane */
            size += 2 * ((texture->h + 1) / 2) * ((data->pitch + 1) / 2);
        }
        data->pixel_data = SDL_calloc(1, size);
        if (!data->pixel_data) {
            SDL_free(data);
            return SDL_OutOfMemory();
        }
    }

    /* Allocate the texture */
    GL_CheckError("", renderer);

    if (data->yuv) {
        renderdata->glGenTextures(1, &data->texture_v);
        if (GL_CheckError("glGenTexures()", renderer) < 0) {
            return -1;
        }
        renderdata->glActiveTexture(GL_TEXTURE2);
        renderdata->glBindTexture(data->texture_type, data->texture_v);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, scaleMode);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        renderdata->glTexImage2D(data->texture_type, 0, format, (texture->w + 1) / 2, (texture->h + 1) / 2, 0, format, type, NULL);

        renderdata->glGenTextures(1, &data->texture_u);
        if (GL_CheckError("glGenTexures()", renderer) < 0) {
            return -1;
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
    } else if (data->nv12) {
        renderdata->glGenTextures(1, &data->texture_u);
        if (GL_CheckError("glGenTexures()", renderer) < 0) {
            return -1;
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
    }

    renderdata->glGenTextures(1, &data->texture);
    if (GL_CheckError("glGenTexures()", renderer) < 0) {
        return -1;
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

    if (texture->access == SDL_TEXTUREACCESS_TARGET) {
       data->fbo = VITA_GLES2_GetFBO(renderer->driverdata, texture->w, texture->h);
    } else {
       data->fbo = NULL;
    }

    return GL_CheckError("", renderer);
}

static int
VITA_GLES2_TexSubImage2D(VITA_GLES2_RenderData *data, GLenum target, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels, GLint pitch, GLint bpp)
{
    Uint8 *blob = NULL;
    Uint8 *src;
    int src_pitch;
    int y;

    if ((width == 0) || (height == 0) || (bpp == 0)) {
        return 0;  /* nothing to do */
    }

    /* Reformat the texture data into a tightly packed array */
    src_pitch = width * bpp;
    src = (Uint8 *)pixels;
    if (pitch != src_pitch) {
        blob = (Uint8 *)SDL_malloc(src_pitch * height);
        if (!blob) {
            return SDL_OutOfMemory();
        }
        src = blob;
        for (y = 0; y < height; ++y)
        {
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

static int
VITA_GLES2_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                    const void *pixels, int pitch)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;
    VITA_GLES2_TextureData *tdata = (VITA_GLES2_TextureData *)texture->driverdata;

    VITA_GLES2_ActivateRenderer(renderer);

    /* Bail out if we're supposed to update an empty rectangle */
    if (rect->w <= 0 || rect->h <= 0) {
        return 0;
    }

    data->drawstate.texture = NULL;  /* we trash this state. */

    /* Create a texture subimage with the supplied data */
    data->glBindTexture(tdata->texture_type, tdata->texture);
    VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                    rect->x,
                    rect->y,
                    rect->w,
                    rect->h,
                    tdata->pixel_format,
                    tdata->pixel_type,
                    pixels, pitch, SDL_BYTESPERPIXEL(texture->format));

    if (tdata->yuv) {
        /* Skip to the correct offset into the next texture */
        pixels = (const void*)((const Uint8*)pixels + rect->h * pitch);
        if (texture->format == SDL_PIXELFORMAT_YV12) {
            data->glBindTexture(tdata->texture_type, tdata->texture_v);
        } else {
            data->glBindTexture(tdata->texture_type, tdata->texture_u);
        }
        VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                rect->x / 2,
                rect->y / 2,
                (rect->w + 1) / 2,
                (rect->h + 1) / 2,
                tdata->pixel_format,
                tdata->pixel_type,
                pixels, (pitch + 1) / 2, 1);


        /* Skip to the correct offset into the next texture */
        pixels = (const void*)((const Uint8*)pixels + ((rect->h + 1) / 2) * ((pitch + 1)/2));
        if (texture->format == SDL_PIXELFORMAT_YV12) {
            data->glBindTexture(tdata->texture_type, tdata->texture_u);
        } else {
            data->glBindTexture(tdata->texture_type, tdata->texture_v);
        }
        VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                rect->x / 2,
                rect->y / 2,
                (rect->w + 1) / 2,
                (rect->h + 1) / 2,
                tdata->pixel_format,
                tdata->pixel_type,
                pixels, (pitch + 1) / 2, 1);
    } else if (tdata->nv12) {
        /* Skip to the correct offset into the next texture */
        pixels = (const void*)((const Uint8*)pixels + rect->h * pitch);
        data->glBindTexture(tdata->texture_type, tdata->texture_u);
        VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                rect->x / 2,
                rect->y / 2,
                (rect->w + 1) / 2,
                (rect->h + 1) / 2,
                GL_LUMINANCE_ALPHA,
                GL_UNSIGNED_BYTE,
                pixels, 2 * ((pitch + 1) / 2), 2);
    }

    return GL_CheckError("glTexSubImage2D()", renderer);
}

static int
VITA_GLES2_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                    const SDL_Rect * rect,
                    const Uint8 *Yplane, int Ypitch,
                    const Uint8 *Uplane, int Upitch,
                    const Uint8 *Vplane, int Vpitch)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;
    VITA_GLES2_TextureData *tdata = (VITA_GLES2_TextureData *)texture->driverdata;

    VITA_GLES2_ActivateRenderer(renderer);

    /* Bail out if we're supposed to update an empty rectangle */
    if (rect->w <= 0 || rect->h <= 0) {
        return 0;
    }

    data->drawstate.texture = NULL;  /* we trash this state. */

    data->glBindTexture(tdata->texture_type, tdata->texture_v);
    VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                    rect->x / 2,
                    rect->y / 2,
                    (rect->w + 1) / 2,
                    (rect->h + 1) / 2,
                    tdata->pixel_format,
                    tdata->pixel_type,
                    Vplane, Vpitch, 1);

    data->glBindTexture(tdata->texture_type, tdata->texture_u);
    VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                    rect->x / 2,
                    rect->y / 2,
                    (rect->w + 1) / 2,
                    (rect->h + 1) / 2,
                    tdata->pixel_format,
                    tdata->pixel_type,
                    Uplane, Upitch, 1);

    data->glBindTexture(tdata->texture_type, tdata->texture);
    VITA_GLES2_TexSubImage2D(data, tdata->texture_type,
                    rect->x,
                    rect->y,
                    rect->w,
                    rect->h,
                    tdata->pixel_format,
                    tdata->pixel_type,
                    Yplane, Ypitch, 1);

    return GL_CheckError("glTexSubImage2D()", renderer);
}

static int
VITA_GLES2_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                  void **pixels, int *pitch)
{
    VITA_GLES2_TextureData *tdata = (VITA_GLES2_TextureData *)texture->driverdata;

    /* Retrieve the buffer/pitch for the specified region */
    *pixels = (Uint8 *)tdata->pixel_data +
              (tdata->pitch * rect->y) +
              (rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = tdata->pitch;

    return 0;
}

static void
VITA_GLES2_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_GLES2_TextureData *tdata = (VITA_GLES2_TextureData *)texture->driverdata;
    SDL_Rect rect;

    /* We do whole texture updates, at least for now */
    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    VITA_GLES2_UpdateTexture(renderer, texture, &rect, tdata->pixel_data, tdata->pitch);
}

static void
VITA_GLES2_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode)
{
    VITA_GLES2_RenderData *renderdata = (VITA_GLES2_RenderData *) renderer->driverdata;
    VITA_GLES2_TextureData *data = (VITA_GLES2_TextureData *) texture->driverdata;
    GLenum glScaleMode = (scaleMode == SDL_ScaleModeNearest) ? GL_NEAREST : GL_LINEAR;

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

    renderdata->glActiveTexture(GL_TEXTURE0);
    renderdata->glBindTexture(data->texture_type, data->texture);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MIN_FILTER, glScaleMode);
    renderdata->glTexParameteri(data->texture_type, GL_TEXTURE_MAG_FILTER, glScaleMode);
}

static int
VITA_GLES2_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *) renderer->driverdata;
    VITA_GLES2_TextureData *texturedata = NULL;
    GLenum status;

    data->drawstate.viewport_dirty = SDL_TRUE;

    if (texture == NULL) {
        data->glBindFramebuffer(GL_FRAMEBUFFER, data->window_framebuffer);
    } else {
        texturedata = (VITA_GLES2_TextureData *) texture->driverdata;
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

static void
VITA_GLES2_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;
    VITA_GLES2_TextureData *tdata = (VITA_GLES2_TextureData *)texture->driverdata;

    VITA_GLES2_ActivateRenderer(renderer);

    if (data->drawstate.texture == texture) {
        data->drawstate.texture = NULL;
    }
    if (data->drawstate.target == texture) {
        data->drawstate.target = NULL;
    }

    /* Destroy the texture */
    if (tdata) {
        data->glDeleteTextures(1, &tdata->texture);
        if (tdata->texture_v) {
            data->glDeleteTextures(1, &tdata->texture_v);
        }
        if (tdata->texture_u) {
            data->glDeleteTextures(1, &tdata->texture_u);
        }
        SDL_free(tdata->pixel_data);
        SDL_free(tdata);
        texture->driverdata = NULL;
    }
}

static int
VITA_GLES2_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;
    Uint32 temp_format = renderer->target ? renderer->target->format : SDL_PIXELFORMAT_ABGR8888;
    size_t buflen;
    void *temp_pixels;
    int temp_pitch;
    Uint8 *src, *dst, *tmp;
    int w, h, length, rows;
    int status;

    temp_pitch = rect->w * SDL_BYTESPERPIXEL(temp_format);
    buflen = rect->h * temp_pitch;
    if (buflen == 0) {
        return 0;  /* nothing to do. */
    }

    temp_pixels = SDL_malloc(buflen);
    if (!temp_pixels) {
        return SDL_OutOfMemory();
    }

    SDL_GetRendererOutputSize(renderer, &w, &h);

    data->glReadPixels(rect->x, renderer->target ? rect->y : (h-rect->y)-rect->h,
                       rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels);
    if (GL_CheckError("glReadPixels()", renderer) < 0) {
        return -1;
    }

    /* Flip the rows to be top-down if necessary */
    if (!renderer->target) {
        SDL_bool isstack;
        length = rect->w * SDL_BYTESPERPIXEL(temp_format);
        src = (Uint8*)temp_pixels + (rect->h-1)*temp_pitch;
        dst = (Uint8*)temp_pixels;
        tmp = SDL_small_alloc(Uint8, length, &isstack);
        rows = rect->h / 2;
        while (rows--) {
            SDL_memcpy(tmp, dst, length);
            SDL_memcpy(dst, src, length);
            SDL_memcpy(src, tmp, length);
            dst += temp_pitch;
            src -= temp_pitch;
        }
        SDL_small_free(tmp, isstack);
    }

    status = SDL_ConvertPixels(rect->w, rect->h,
                               temp_format, temp_pixels, temp_pitch,
                               pixel_format, pixels, pitch);
    SDL_free(temp_pixels);

    return status;
}

static void
VITA_GLES2_RenderPresent(SDL_Renderer *renderer)
{
    /* Tell the video driver to swap buffers */
    SDL_GL_SwapWindow(renderer->window);
}


/*************************************************************************************************
 * Bind/unbinding of textures
 *************************************************************************************************/
static int VITA_GLES2_BindTexture (SDL_Renderer * renderer, SDL_Texture *texture, float *texw, float *texh);
static int VITA_GLES2_UnbindTexture (SDL_Renderer * renderer, SDL_Texture *texture);

static int VITA_GLES2_BindTexture (SDL_Renderer * renderer, SDL_Texture *texture, float *texw, float *texh)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;
    VITA_GLES2_TextureData *texturedata = (VITA_GLES2_TextureData *)texture->driverdata;
    VITA_GLES2_ActivateRenderer(renderer);

    data->glBindTexture(texturedata->texture_type, texturedata->texture);
    data->drawstate.texture = texture;

    if (texw) {
        *texw = 1.0;
    }
    if (texh) {
        *texh = 1.0;
    }

    return 0;
}

static int VITA_GLES2_UnbindTexture (SDL_Renderer * renderer, SDL_Texture *texture)
{
    VITA_GLES2_RenderData *data = (VITA_GLES2_RenderData *)renderer->driverdata;
    VITA_GLES2_TextureData *texturedata = (VITA_GLES2_TextureData *)texture->driverdata;
    VITA_GLES2_ActivateRenderer(renderer);

    data->glBindTexture(texturedata->texture_type, 0);
    data->drawstate.texture = NULL;

    return 0;
}


/*************************************************************************************************
 * Renderer instantiation                                                                        *
 *************************************************************************************************/

static SDL_Renderer *
VITA_GLES2_CreateRenderer(SDL_Window *window, Uint32 flags)
{
    SDL_Renderer *renderer;
    VITA_GLES2_RenderData *data;
    GLint nFormats;
    GLboolean hasCompiler;
    GLint window_framebuffer;
    GLint value;
    Uint32 window_flags = 0;

    window_flags = SDL_GetWindowFlags(window);

    if (!(window_flags & SDL_WINDOW_OPENGL)) {
        if (SDL_RecreateWindow(window, window_flags | SDL_WINDOW_OPENGL) < 0) {
            goto error;
        }
    }

    /* Create the renderer struct */
    renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(SDL_Renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        goto error;
    }

    data = (VITA_GLES2_RenderData *)SDL_calloc(1, sizeof(VITA_GLES2_RenderData));
    if (!data) {
        SDL_free(renderer);
        SDL_OutOfMemory();
        goto error;
    }

    renderer->info = VITA_GLES2_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    renderer->driverdata = data;
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

    if (VITA_GLES2_LoadFunctions(data) < 0) {
        SDL_GL_DeleteContext(data->context);
        SDL_free(renderer);
        SDL_free(data);
        goto error;
    }

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }

    if (SDL_GL_GetSwapInterval() > 0) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
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

    data->glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &nFormats);
    hasCompiler = GL_TRUE;
    if (hasCompiler) {
        ++nFormats;
    }

    data->shader_formats = (GLenum *)SDL_calloc(nFormats, sizeof(GLenum));
    if (!data->shader_formats) {
        VITA_GLES2_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        goto error;
    }
    data->shader_format_count = nFormats;
    data->glGetIntegerv(GL_SHADER_BINARY_FORMATS, (GLint *)data->shader_formats);
    if (hasCompiler) {
        data->shader_formats[nFormats - 1] = (GLenum)-1;
    }

    /* we keep a few of these and cycle through them, so data can live for a few frames. */
    data->glGenBuffers(SDL_arraysize(data->vertex_buffers), data->vertex_buffers);

    data->framebuffers = NULL;
    data->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &window_framebuffer);
    data->window_framebuffer = (GLuint)window_framebuffer;

    /* Populate the function pointers for the module */
    renderer->WindowEvent         = VITA_GLES2_WindowEvent;
    renderer->GetOutputSize       = VITA_GLES2_GetOutputSize;
    renderer->SupportsBlendMode   = VITA_GLES2_SupportsBlendMode;
    renderer->CreateTexture       = VITA_GLES2_CreateTexture;
    renderer->UpdateTexture       = VITA_GLES2_UpdateTexture;
    renderer->UpdateTextureYUV    = VITA_GLES2_UpdateTextureYUV;
    renderer->LockTexture         = VITA_GLES2_LockTexture;
    renderer->UnlockTexture       = VITA_GLES2_UnlockTexture;
    renderer->SetTextureScaleMode = VITA_GLES2_SetTextureScaleMode;
    renderer->SetRenderTarget     = VITA_GLES2_SetRenderTarget;

    renderer->QueueSetViewport    = VITA_GLES2_QueueSetViewport;
    renderer->QueueSetDrawColor   = VITA_GLES2_QueueSetViewport;  /* SetViewport and SetDrawColor are (currently) no-ops. */
    renderer->QueueDrawPoints     = VITA_GLES2_QueueDrawPoints;
    renderer->QueueDrawLines      = VITA_GLES2_QueueDrawPoints;  /* lines and points queue vertices the same way. */
    renderer->QueueFillRects      = VITA_GLES2_QueueFillRects;
    renderer->QueueCopy           = VITA_GLES2_QueueCopy;
    renderer->QueueCopyEx         = VITA_GLES2_QueueCopyEx;
    renderer->RunCommandQueue     = VITA_GLES2_RunCommandQueue;
    renderer->RenderReadPixels    = VITA_GLES2_RenderReadPixels;
    renderer->RenderPresent       = VITA_GLES2_RenderPresent;
    renderer->DestroyTexture      = VITA_GLES2_DestroyTexture;
    renderer->DestroyRenderer     = VITA_GLES2_DestroyRenderer;
    renderer->GL_BindTexture      = VITA_GLES2_BindTexture;
    renderer->GL_UnbindTexture    = VITA_GLES2_UnbindTexture;

// TODO. See shaders
//    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_YV12;
//    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_IYUV;
//    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_NV12;
//    renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_NV21;

    /* Set up parameters for rendering */
    data->glActiveTexture(GL_TEXTURE0);
    data->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    data->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    data->glEnableVertexAttribArray(VITA_GLES2_ATTRIBUTE_POSITION);
    data->glDisableVertexAttribArray(VITA_GLES2_ATTRIBUTE_TEXCOORD);

    data->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    data->drawstate.blend = SDL_BLENDMODE_INVALID;
    data->drawstate.color = 0xFFFFFFFF;
    data->drawstate.clear_color = 0xFFFFFFFF;
    data->drawstate.projection[3][0] = -1.0f;
    data->drawstate.projection[3][3] = 1.0f;

    GL_CheckError("", renderer);

    return renderer;

error:
    return NULL;
}

SDL_RenderDriver VITA_GLES2_RenderDriver = {
    VITA_GLES2_CreateRenderer,
    {
        "VITA gles2",
        (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE),
        4,
        {
        SDL_PIXELFORMAT_ARGB8888,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_PIXELFORMAT_RGB888,
        SDL_PIXELFORMAT_BGR888
        },
        0,
        0
    }
};

#endif /* SDL_VIDEO_RENDER_VITA_GLES2 && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
