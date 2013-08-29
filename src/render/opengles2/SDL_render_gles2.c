/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#if SDL_VIDEO_RENDER_OGL_ES2 && !SDL_RENDER_DISABLED

#include "SDL_hints.h"
#include "SDL_opengles2.h"
#include "../SDL_sysrender.h"
#include "SDL_shaders_gles2.h"

/* Used to re-create the window with OpenGL ES capability */
extern int SDL_RecreateWindow(SDL_Window * window, Uint32 flags);

/*************************************************************************************************
 * Bootstrap data                                                                                *
 *************************************************************************************************/

static SDL_Renderer *GLES2_CreateRenderer(SDL_Window *window, Uint32 flags);

SDL_RenderDriver GLES2_RenderDriver = {
    GLES2_CreateRenderer,
    {
        "opengles2",
        (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE),
        4,
        {SDL_PIXELFORMAT_ABGR8888,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_PIXELFORMAT_RGB888,
        SDL_PIXELFORMAT_BGR888},
        0,
        0
    }
};

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
    GLenum texture;
    GLenum texture_type;
    GLenum pixel_format;
    GLenum pixel_type;
    void *pixel_data;
    size_t pitch;
    GLES2_FBOList *fbo;
} GLES2_TextureData;

typedef struct GLES2_ShaderCacheEntry
{
    GLuint id;
    GLES2_ShaderType type;
    const GLES2_ShaderInstance *instance;
    int references;
    struct GLES2_ShaderCacheEntry *prev;
    struct GLES2_ShaderCacheEntry *next;
} GLES2_ShaderCacheEntry;

typedef struct GLES2_ShaderCache
{
    int count;
    GLES2_ShaderCacheEntry *head;
} GLES2_ShaderCache;

typedef struct GLES2_ProgramCacheEntry
{
    GLuint id;
    SDL_BlendMode blend_mode;
    GLES2_ShaderCacheEntry *vertex_shader;
    GLES2_ShaderCacheEntry *fragment_shader;
    GLuint uniform_locations[16];
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
    GLES2_ATTRIBUTE_POSITION = 0,
    GLES2_ATTRIBUTE_TEXCOORD = 1,
    GLES2_ATTRIBUTE_ANGLE = 2,
    GLES2_ATTRIBUTE_CENTER = 3,
} GLES2_Attribute;

typedef enum
{
    GLES2_UNIFORM_PROJECTION,
    GLES2_UNIFORM_TEXTURE,
    GLES2_UNIFORM_MODULATION,
    GLES2_UNIFORM_COLOR,
    GLES2_UNIFORM_COLORTABLE
} GLES2_Uniform;

typedef enum
{
    GLES2_IMAGESOURCE_SOLID,
    GLES2_IMAGESOURCE_TEXTURE_ABGR,
    GLES2_IMAGESOURCE_TEXTURE_ARGB,
    GLES2_IMAGESOURCE_TEXTURE_RGB,
    GLES2_IMAGESOURCE_TEXTURE_BGR
} GLES2_ImageSource;

typedef struct GLES2_DriverContext
{
    SDL_GLContext *context;
    struct {
        int blendMode;
        SDL_bool tex_coords;
    } current;

#define SDL_PROC(ret,func,params) ret (APIENTRY *func) params;
#include "SDL_gles2funcs.h"
#undef SDL_PROC
    GLES2_FBOList *framebuffers;
    GLuint window_framebuffer;

    int shader_format_count;
    GLenum *shader_formats;
    GLES2_ShaderCache shader_cache;
    GLES2_ProgramCache program_cache;
    GLES2_ProgramCacheEntry *current_program;
} GLES2_DriverContext;

#define GLES2_MAX_CACHED_PROGRAMS 8

/*************************************************************************************************
 * Renderer state APIs                                                                           *
 *************************************************************************************************/

static int GLES2_ActivateRenderer(SDL_Renderer *renderer);
static void GLES2_WindowEvent(SDL_Renderer * renderer,
                              const SDL_WindowEvent *event);
static int GLES2_UpdateViewport(SDL_Renderer * renderer);
static void GLES2_DestroyRenderer(SDL_Renderer *renderer);
static int GLES2_SetOrthographicProjection(SDL_Renderer *renderer);


static SDL_GLContext SDL_CurrentContext = NULL;

static int GLES2_LoadFunctions(GLES2_DriverContext * data)
{
#if SDL_VIDEO_DRIVER_UIKIT
#define __SDL_NOGETPROCADDR__
#elif SDL_VIDEO_DRIVER_ANDROID
#define __SDL_NOGETPROCADDR__
#elif SDL_VIDEO_DRIVER_PANDORA
#define __SDL_NOGETPROCADDR__
#endif

#if defined __SDL_NOGETPROCADDR__
#define SDL_PROC(ret,func,params) data->func=func;
#else
#define SDL_PROC(ret,func,params) \
    do { \
        data->func = SDL_GL_GetProcAddress(#func); \
        if ( ! data->func ) { \
            return SDL_SetError("Couldn't load GLES2 function %s: %s\n", #func, SDL_GetError()); \
        } \
    } while ( 0 );
#endif /* _SDL_NOGETPROCADDR_ */

#include "SDL_gles2funcs.h"
#undef SDL_PROC
    return 0;
}

GLES2_FBOList *
GLES2_GetFBO(GLES2_DriverContext *data, Uint32 w, Uint32 h)
{
   GLES2_FBOList *result = data->framebuffers;
   while ((result) && ((result->w != w) || (result->h != h)) )
   {
       result = result->next;
   }
   if (result == NULL)
   {
       result = SDL_malloc(sizeof(GLES2_FBOList));
       result->w = w;
       result->h = h;
       data->glGenFramebuffers(1, &result->FBO);
       result->next = data->framebuffers;
       data->framebuffers = result;
   }
   return result;
}

static int
GLES2_ActivateRenderer(SDL_Renderer * renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;

    if (SDL_CurrentContext != rdata->context) {
        /* Null out the current program to ensure we set it again */
        rdata->current_program = NULL;

        if (SDL_GL_MakeCurrent(renderer->window, rdata->context) < 0) {
            return -1;
        }
        SDL_CurrentContext = rdata->context;

        GLES2_UpdateViewport(renderer);
    }
    return 0;
}

static void
GLES2_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;

    if (event->event == SDL_WINDOWEVENT_SIZE_CHANGED ||
        event->event == SDL_WINDOWEVENT_SHOWN ||
        event->event == SDL_WINDOWEVENT_HIDDEN) {
        /* Rebind the context to the window area */
        SDL_CurrentContext = NULL;
    }

    if (event->event == SDL_WINDOWEVENT_MINIMIZED) {
        /* According to Apple documentation, we need to finish drawing NOW! */
        rdata->glFinish();
    }
}

static int
GLES2_UpdateViewport(SDL_Renderer * renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;

    if (SDL_CurrentContext != rdata->context) {
        /* We'll update the viewport after we rebind the context */
        return 0;
    }

    rdata->glViewport(renderer->viewport.x, renderer->viewport.y,
               renderer->viewport.w, renderer->viewport.h);

    if (rdata->current_program) {
        GLES2_SetOrthographicProjection(renderer);
    }
    return 0;
}

static int
GLES2_UpdateClipRect(SDL_Renderer * renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    const SDL_Rect *rect = &renderer->clip_rect;

    if (SDL_CurrentContext != rdata->context) {
        /* We'll update the clip rect after we rebind the context */
        return 0;
    }

    if (!SDL_RectEmpty(rect)) {
        rdata->glEnable(GL_SCISSOR_TEST);
        rdata->glScissor(rect->x, renderer->viewport.h - rect->y - rect->h, rect->w, rect->h);
    } else {
        rdata->glDisable(GL_SCISSOR_TEST);
    }
    return 0;
}

static void
GLES2_DestroyRenderer(SDL_Renderer *renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;

    /* Deallocate everything */
    if (rdata) {
        GLES2_ActivateRenderer(renderer);

        {
            GLES2_ShaderCacheEntry *entry;
            GLES2_ShaderCacheEntry *next;
            entry = rdata->shader_cache.head;
            while (entry)
            {
                rdata->glDeleteShader(entry->id);
                next = entry->next;
                SDL_free(entry);
                entry = next;
            }
        }
        {
            GLES2_ProgramCacheEntry *entry;
            GLES2_ProgramCacheEntry *next;
            entry = rdata->program_cache.head;
            while (entry) {
                rdata->glDeleteProgram(entry->id);
                next = entry->next;
                SDL_free(entry);
                entry = next;
            }
        }
        if (rdata->context) {
            while (rdata->framebuffers) {
                GLES2_FBOList *nextnode = rdata->framebuffers->next;
                rdata->glDeleteFramebuffers(1, &rdata->framebuffers->FBO);
                SDL_free(rdata->framebuffers);
                rdata->framebuffers = nextnode;
            }
            SDL_GL_DeleteContext(rdata->context);
        }
        SDL_free(rdata->shader_formats);
        SDL_free(rdata);
    }
    SDL_free(renderer);
}

/*************************************************************************************************
 * Texture APIs                                                                                  *
 *************************************************************************************************/

static int GLES2_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void GLES2_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static int GLES2_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                             void **pixels, int *pitch);
static void GLES2_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static int GLES2_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                               const void *pixels, int pitch);
static int GLES2_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture);

static GLenum
GetScaleQuality(void)
{
    const char *hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);

    if (!hint || *hint == '0' || SDL_strcasecmp(hint, "nearest") == 0) {
        return GL_NEAREST;
    } else {
        return GL_LINEAR;
    }
}

static int
GLES2_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *tdata;
    GLenum format;
    GLenum type;
    GLenum scaleMode;

    GLES2_ActivateRenderer(renderer);

    /* Determine the corresponding GLES texture format params */
    switch (texture->format)
    {
    case SDL_PIXELFORMAT_ABGR8888:
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_BGR888:
    case SDL_PIXELFORMAT_RGB888:
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        break;
    default:
        return SDL_SetError("Texture format not supported");
    }

    /* Allocate a texture struct */
    tdata = (GLES2_TextureData *)SDL_calloc(1, sizeof(GLES2_TextureData));
    if (!tdata) {
        return SDL_OutOfMemory();
    }
    tdata->texture = 0;
    tdata->texture_type = GL_TEXTURE_2D;
    tdata->pixel_format = format;
    tdata->pixel_type = type;
    scaleMode = GetScaleQuality();

    /* Allocate a blob for image data */
    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        tdata->pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
        tdata->pixel_data = SDL_calloc(1, tdata->pitch * texture->h);
        if (!tdata->pixel_data) {
            SDL_free(tdata);
            return SDL_OutOfMemory();
        }
    }

    /* Allocate the texture */
    rdata->glGetError();
    rdata->glGenTextures(1, &tdata->texture);
    if (rdata->glGetError() != GL_NO_ERROR) {
        SDL_free(tdata);
        return SDL_SetError("Texture creation failed in glGenTextures()");
    }
    rdata->glActiveTexture(GL_TEXTURE0);
    rdata->glBindTexture(tdata->texture_type, tdata->texture);
    rdata->glTexParameteri(tdata->texture_type, GL_TEXTURE_MIN_FILTER, scaleMode);
    rdata->glTexParameteri(tdata->texture_type, GL_TEXTURE_MAG_FILTER, scaleMode);
    rdata->glTexParameteri(tdata->texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    rdata->glTexParameteri(tdata->texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    rdata->glTexImage2D(tdata->texture_type, 0, format, texture->w, texture->h, 0, format, type, NULL);
    if (rdata->glGetError() != GL_NO_ERROR) {
        rdata->glDeleteTextures(1, &tdata->texture);
        SDL_free(tdata);
        return SDL_SetError("Texture creation failed");
    }
    texture->driverdata = tdata;

    if (texture->access == SDL_TEXTUREACCESS_TARGET) {
       tdata->fbo = GLES2_GetFBO(renderer->driverdata, texture->w, texture->h);
    } else {
       tdata->fbo = NULL;
    }

    return 0;
}

static void
GLES2_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;

    GLES2_ActivateRenderer(renderer);

    /* Destroy the texture */
    if (tdata)
    {
        rdata->glDeleteTextures(1, &tdata->texture);
        SDL_free(tdata->pixel_data);
        SDL_free(tdata);
        texture->driverdata = NULL;
    }
}

static int
GLES2_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
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

static void
GLES2_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
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

static int
GLES2_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect,
                    const void *pixels, int pitch)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;
    Uint8 *blob = NULL;
    Uint8 *src;
    int srcPitch;
    int y;

    GLES2_ActivateRenderer(renderer);

    /* Bail out if we're supposed to update an empty rectangle */
    if (rect->w <= 0 || rect->h <= 0)
        return 0;

    /* Reformat the texture data into a tightly packed array */
    srcPitch = rect->w * SDL_BYTESPERPIXEL(texture->format);
    src = (Uint8 *)pixels;
    if (pitch != srcPitch) {
        blob = (Uint8 *)SDL_malloc(srcPitch * rect->h);
        if (!blob) {
            return SDL_OutOfMemory();
        }
        src = blob;
        for (y = 0; y < rect->h; ++y)
        {
            SDL_memcpy(src, pixels, srcPitch);
            src += srcPitch;
            pixels = (Uint8 *)pixels + pitch;
        }
        src = blob;
    }

    /* Create a texture subimage with the supplied data */
    rdata->glGetError();
    rdata->glActiveTexture(GL_TEXTURE0);
    rdata->glBindTexture(tdata->texture_type, tdata->texture);
    rdata->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    rdata->glTexSubImage2D(tdata->texture_type,
                    0,
                    rect->x,
                    rect->y,
                    rect->w,
                    rect->h,
                    tdata->pixel_format,
                    tdata->pixel_type,
                    src);
    SDL_free(blob);

    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to update texture");
    }
    return 0;
}

static int
GLES2_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GLES2_DriverContext *data = (GLES2_DriverContext *) renderer->driverdata;
    GLES2_TextureData *texturedata = NULL;
    GLenum status;

    if (texture == NULL) {
        data->glBindFramebuffer(GL_FRAMEBUFFER, data->window_framebuffer);
    } else {
        texturedata = (GLES2_TextureData *) texture->driverdata;
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

/*************************************************************************************************
 * Shader management functions                                                                   *
 *************************************************************************************************/

static GLES2_ShaderCacheEntry *GLES2_CacheShader(SDL_Renderer *renderer, GLES2_ShaderType type,
                                                 SDL_BlendMode blendMode);
static void GLES2_EvictShader(SDL_Renderer *renderer, GLES2_ShaderCacheEntry *entry);
static GLES2_ProgramCacheEntry *GLES2_CacheProgram(SDL_Renderer *renderer,
                                                   GLES2_ShaderCacheEntry *vertex,
                                                   GLES2_ShaderCacheEntry *fragment,
                                                   SDL_BlendMode blendMode);
static int GLES2_SelectProgram(SDL_Renderer *renderer, GLES2_ImageSource source,
                               SDL_BlendMode blendMode);

static GLES2_ProgramCacheEntry *
GLES2_CacheProgram(SDL_Renderer *renderer, GLES2_ShaderCacheEntry *vertex,
                   GLES2_ShaderCacheEntry *fragment, SDL_BlendMode blendMode)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_ProgramCacheEntry *entry;
    GLES2_ShaderCacheEntry *shaderEntry;
    GLint linkSuccessful;

    /* Check if we've already cached this program */
    entry = rdata->program_cache.head;
    while (entry)
    {
        if (entry->vertex_shader == vertex && entry->fragment_shader == fragment)
            break;
        entry = entry->next;
    }
    if (entry)
    {
        if (rdata->program_cache.head != entry)
        {
            if (entry->next)
                entry->next->prev = entry->prev;
            if (entry->prev)
                entry->prev->next = entry->next;
            entry->prev = NULL;
            entry->next = rdata->program_cache.head;
            rdata->program_cache.head->prev = entry;
            rdata->program_cache.head = entry;
        }
        return entry;
    }

    /* Create a program cache entry */
    entry = (GLES2_ProgramCacheEntry *)SDL_calloc(1, sizeof(GLES2_ProgramCacheEntry));
    if (!entry)
    {
        SDL_OutOfMemory();
        return NULL;
    }
    entry->vertex_shader = vertex;
    entry->fragment_shader = fragment;
    entry->blend_mode = blendMode;

    /* Create the program and link it */
    rdata->glGetError();
    entry->id = rdata->glCreateProgram();
    rdata->glAttachShader(entry->id, vertex->id);
    rdata->glAttachShader(entry->id, fragment->id);
    rdata->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_POSITION, "a_position");
    rdata->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_TEXCOORD, "a_texCoord");
    rdata->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_ANGLE, "a_angle");
    rdata->glBindAttribLocation(entry->id, GLES2_ATTRIBUTE_CENTER, "a_center");
    rdata->glLinkProgram(entry->id);
    rdata->glGetProgramiv(entry->id, GL_LINK_STATUS, &linkSuccessful);
    if (rdata->glGetError() != GL_NO_ERROR || !linkSuccessful)
    {
        rdata->glDeleteProgram(entry->id);
        SDL_free(entry);
        SDL_SetError("Failed to link shader program");
        return NULL;
    }

    /* Predetermine locations of uniform variables */
    entry->uniform_locations[GLES2_UNIFORM_PROJECTION] =
        rdata->glGetUniformLocation(entry->id, "u_projection");
    entry->uniform_locations[GLES2_UNIFORM_TEXTURE] =
        rdata->glGetUniformLocation(entry->id, "u_texture");
    entry->uniform_locations[GLES2_UNIFORM_MODULATION] =
        rdata->glGetUniformLocation(entry->id, "u_modulation");
    entry->uniform_locations[GLES2_UNIFORM_COLOR] =
        rdata->glGetUniformLocation(entry->id, "u_color");
    entry->uniform_locations[GLES2_UNIFORM_COLORTABLE] =
        rdata->glGetUniformLocation(entry->id, "u_colorTable");

    /* Cache the linked program */
    if (rdata->program_cache.head)
    {
        entry->next = rdata->program_cache.head;
        rdata->program_cache.head->prev = entry;
    }
    else
    {
        rdata->program_cache.tail = entry;
    }
    rdata->program_cache.head = entry;
    ++rdata->program_cache.count;

    /* Increment the refcount of the shaders we're using */
    ++vertex->references;
    ++fragment->references;

    /* Evict the last entry from the cache if we exceed the limit */
    if (rdata->program_cache.count > GLES2_MAX_CACHED_PROGRAMS)
    {
        shaderEntry = rdata->program_cache.tail->vertex_shader;
        if (--shaderEntry->references <= 0)
            GLES2_EvictShader(renderer, shaderEntry);
        shaderEntry = rdata->program_cache.tail->fragment_shader;
        if (--shaderEntry->references <= 0)
            GLES2_EvictShader(renderer, shaderEntry);
        rdata->glDeleteProgram(rdata->program_cache.tail->id);
        rdata->program_cache.tail = rdata->program_cache.tail->prev;
        SDL_free(rdata->program_cache.tail->next);
        rdata->program_cache.tail->next = NULL;
        --rdata->program_cache.count;
    }
    return entry;
}

static GLES2_ShaderCacheEntry *
GLES2_CacheShader(SDL_Renderer *renderer, GLES2_ShaderType type, SDL_BlendMode blendMode)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    const GLES2_Shader *shader;
    const GLES2_ShaderInstance *instance = NULL;
    GLES2_ShaderCacheEntry *entry = NULL;
    GLint compileSuccessful = GL_FALSE;
    int i, j;

    /* Find the corresponding shader */
    shader = GLES2_GetShader(type, blendMode);
    if (!shader)
    {
        SDL_SetError("No shader matching the requested characteristics was found");
        return NULL;
    }

    /* Find a matching shader instance that's supported on this hardware */
    for (i = 0; i < shader->instance_count && !instance; ++i)
    {
        for (j = 0; j < rdata->shader_format_count && !instance; ++j)
        {
            if (!shader->instances)
                continue;
            if (!shader->instances[i])
                continue;
            if (shader->instances[i]->format != rdata->shader_formats[j])
                continue;
            instance = shader->instances[i];
        }
    }
    if (!instance)
    {
        SDL_SetError("The specified shader cannot be loaded on the current platform");
        return NULL;
    }

    /* Check if we've already cached this shader */
    entry = rdata->shader_cache.head;
    while (entry)
    {
        if (entry->instance == instance)
            break;
        entry = entry->next;
    }
    if (entry)
        return entry;

    /* Create a shader cache entry */
    entry = (GLES2_ShaderCacheEntry *)SDL_calloc(1, sizeof(GLES2_ShaderCacheEntry));
    if (!entry)
    {
        SDL_OutOfMemory();
        return NULL;
    }
    entry->type = type;
    entry->instance = instance;

    /* Compile or load the selected shader instance */
    rdata->glGetError();
    entry->id = rdata->glCreateShader(instance->type);
    if (instance->format == (GLenum)-1)
    {
        rdata->glShaderSource(entry->id, 1, (const char **)&instance->data, NULL);
        rdata->glCompileShader(entry->id);
        rdata->glGetShaderiv(entry->id, GL_COMPILE_STATUS, &compileSuccessful);
    }
    else
    {
        rdata->glShaderBinary(1, &entry->id, instance->format, instance->data, instance->length);
        compileSuccessful = GL_TRUE;
    }
    if (rdata->glGetError() != GL_NO_ERROR || !compileSuccessful)
    {
        char *info = NULL;
        int length = 0;

        rdata->glGetShaderiv(entry->id, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            info = SDL_stack_alloc(char, length);
            if (info) {
                rdata->glGetShaderInfoLog(entry->id, length, &length, info);
            }
        }
        if (info) {
            SDL_SetError("Failed to load the shader: %s", info);
            SDL_stack_free(info);
        } else {
            SDL_SetError("Failed to load the shader");
        }
        rdata->glDeleteShader(entry->id);
        SDL_free(entry);
        return NULL;
    }

    /* Link the shader entry in at the front of the cache */
    if (rdata->shader_cache.head)
    {
        entry->next = rdata->shader_cache.head;
        rdata->shader_cache.head->prev = entry;
    }
    rdata->shader_cache.head = entry;
    ++rdata->shader_cache.count;
    return entry;
}

static void
GLES2_EvictShader(SDL_Renderer *renderer, GLES2_ShaderCacheEntry *entry)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;

    /* Unlink the shader from the cache */
    if (entry->next)
        entry->next->prev = entry->prev;
    if (entry->prev)
        entry->prev->next = entry->next;
    if (rdata->shader_cache.head == entry)
        rdata->shader_cache.head = entry->next;
    --rdata->shader_cache.count;

    /* Deallocate the shader */
    rdata->glDeleteShader(entry->id);
    SDL_free(entry);
}

static int
GLES2_SelectProgram(SDL_Renderer *renderer, GLES2_ImageSource source, SDL_BlendMode blendMode)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_ShaderCacheEntry *vertex = NULL;
    GLES2_ShaderCacheEntry *fragment = NULL;
    GLES2_ShaderType vtype, ftype;
    GLES2_ProgramCacheEntry *program;

    /* Select an appropriate shader pair for the specified modes */
    vtype = GLES2_SHADER_VERTEX_DEFAULT;
    switch (source)
    {
    case GLES2_IMAGESOURCE_SOLID:
        ftype = GLES2_SHADER_FRAGMENT_SOLID_SRC;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_ABGR:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_ABGR_SRC;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_ARGB:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_ARGB_SRC;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_RGB:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_RGB_SRC;
        break;
    case GLES2_IMAGESOURCE_TEXTURE_BGR:
        ftype = GLES2_SHADER_FRAGMENT_TEXTURE_BGR_SRC;
        break;
    default:
        goto fault;
    }

    /* Load the requested shaders */
    vertex = GLES2_CacheShader(renderer, vtype, blendMode);
    if (!vertex)
        goto fault;
    fragment = GLES2_CacheShader(renderer, ftype, blendMode);
    if (!fragment)
        goto fault;

    /* Check if we need to change programs at all */
    if (rdata->current_program &&
        rdata->current_program->vertex_shader == vertex &&
        rdata->current_program->fragment_shader == fragment)
        return 0;

    /* Generate a matching program */
    program = GLES2_CacheProgram(renderer, vertex, fragment, blendMode);
    if (!program)
        goto fault;

    /* Select that program in OpenGL */
    rdata->glGetError();
    rdata->glUseProgram(program->id);
    if (rdata->glGetError() != GL_NO_ERROR)
    {
        SDL_SetError("Failed to select program");
        goto fault;
    }

    /* Set the current program */
    rdata->current_program = program;

    /* Activate an orthographic projection */
    if (GLES2_SetOrthographicProjection(renderer) < 0)
        goto fault;

    /* Clean up and return */
    return 0;
fault:
    if (vertex && vertex->references <= 0)
        GLES2_EvictShader(renderer, vertex);
    if (fragment && fragment->references <= 0)
        GLES2_EvictShader(renderer, fragment);
    rdata->current_program = NULL;
    return -1;
}

static int
GLES2_SetOrthographicProjection(SDL_Renderer *renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLfloat projection[4][4];
    GLuint locProjection;

    if (!renderer->viewport.w || !renderer->viewport.h) {
        return 0;
    }

    /* Prepare an orthographic projection */
    projection[0][0] = 2.0f / renderer->viewport.w;
    projection[0][1] = 0.0f;
    projection[0][2] = 0.0f;
    projection[0][3] = 0.0f;
    projection[1][0] = 0.0f;
    if (renderer->target) {
        projection[1][1] = 2.0f / renderer->viewport.h;
    } else {
        projection[1][1] = -2.0f / renderer->viewport.h;
    }
    projection[1][2] = 0.0f;
    projection[1][3] = 0.0f;
    projection[2][0] = 0.0f;
    projection[2][1] = 0.0f;
    projection[2][2] = 0.0f;
    projection[2][3] = 0.0f;
    projection[3][0] = -1.0f;
    if (renderer->target) {
        projection[3][1] = -1.0f;
    } else {
        projection[3][1] = 1.0f;
    }
    projection[3][2] = 0.0f;
    projection[3][3] = 1.0f;

    /* Set the projection matrix */
    locProjection = rdata->current_program->uniform_locations[GLES2_UNIFORM_PROJECTION];
    rdata->glGetError();
    rdata->glUniformMatrix4fv(locProjection, 1, GL_FALSE, (GLfloat *)projection);
    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to set orthographic projection");
    }
    return 0;
}

/*************************************************************************************************
 * Rendering functions                                                                           *
 *************************************************************************************************/

static const float inv255f = 1.0f / 255.0f;

static int GLES2_RenderClear(SDL_Renderer *renderer);
static int GLES2_RenderDrawPoints(SDL_Renderer *renderer, const SDL_FPoint *points, int count);
static int GLES2_RenderDrawLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count);
static int GLES2_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);
static int GLES2_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect,
                            const SDL_FRect *dstrect);
static int GLES2_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                         const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);
static int GLES2_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch);
static void GLES2_RenderPresent(SDL_Renderer *renderer);


static int
GLES2_RenderClear(SDL_Renderer * renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;

    GLES2_ActivateRenderer(renderer);

    rdata->glClearColor((GLfloat) renderer->r * inv255f,
                 (GLfloat) renderer->g * inv255f,
                 (GLfloat) renderer->b * inv255f,
                 (GLfloat) renderer->a * inv255f);

    rdata->glClear(GL_COLOR_BUFFER_BIT);

    return 0;
}

static void
GLES2_SetBlendMode(GLES2_DriverContext *rdata, int blendMode)
{
    if (blendMode != rdata->current.blendMode) {
        switch (blendMode) {
        default:
        case SDL_BLENDMODE_NONE:
            rdata->glDisable(GL_BLEND);
            break;
        case SDL_BLENDMODE_BLEND:
            rdata->glEnable(GL_BLEND);
            rdata->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case SDL_BLENDMODE_ADD:
            rdata->glEnable(GL_BLEND);
            rdata->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
            break;
        case SDL_BLENDMODE_MOD:
            rdata->glEnable(GL_BLEND);
            rdata->glBlendFuncSeparate(GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_ONE);
            break;
        }
        rdata->current.blendMode = blendMode;
    }
}

static void
GLES2_SetTexCoords(GLES2_DriverContext * rdata, SDL_bool enabled)
{
    if (enabled != rdata->current.tex_coords) {
        if (enabled) {
            rdata->glEnableVertexAttribArray(GLES2_ATTRIBUTE_TEXCOORD);
        } else {
            rdata->glDisableVertexAttribArray(GLES2_ATTRIBUTE_TEXCOORD);
        }
        rdata->current.tex_coords = enabled;
    }
}

static int
GLES2_SetDrawingState(SDL_Renderer * renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    int blendMode = renderer->blendMode;
    GLuint locColor;

    rdata->glGetError();

    GLES2_ActivateRenderer(renderer);

    GLES2_SetBlendMode(rdata, blendMode);

    GLES2_SetTexCoords(rdata, SDL_FALSE);

    /* Activate an appropriate shader and set the projection matrix */
    if (GLES2_SelectProgram(renderer, GLES2_IMAGESOURCE_SOLID, blendMode) < 0)
        return -1;

    /* Select the color to draw with */
    locColor = rdata->current_program->uniform_locations[GLES2_UNIFORM_COLOR];
    if (renderer->target &&
        (renderer->target->format == SDL_PIXELFORMAT_ARGB8888 ||
         renderer->target->format == SDL_PIXELFORMAT_RGB888)) {
        rdata->glUniform4f(locColor,
                    renderer->b * inv255f,
                    renderer->g * inv255f,
                    renderer->r * inv255f,
                    renderer->a * inv255f);
    } else {
        rdata->glUniform4f(locColor,
                    renderer->r * inv255f,
                    renderer->g * inv255f,
                    renderer->b * inv255f,
                    renderer->a * inv255f);
    }
    return 0;
}

static int
GLES2_RenderDrawPoints(SDL_Renderer *renderer, const SDL_FPoint *points, int count)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLfloat *vertices;
    int idx;

    if (GLES2_SetDrawingState(renderer) < 0) {
        return -1;
    }

    /* Emit the specified vertices as points */
    vertices = SDL_stack_alloc(GLfloat, count * 2);
    for (idx = 0; idx < count; ++idx) {
        GLfloat x = points[idx].x + 0.5f;
        GLfloat y = points[idx].y + 0.5f;

        vertices[idx * 2] = x;
        vertices[(idx * 2) + 1] = y;
    }
    rdata->glGetError();
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    rdata->glDrawArrays(GL_POINTS, 0, count);
    SDL_stack_free(vertices);
    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to render points");
    }
    return 0;
}

static int
GLES2_RenderDrawLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLfloat *vertices;
    int idx;

    if (GLES2_SetDrawingState(renderer) < 0) {
        return -1;
    }

    /* Emit a line strip including the specified vertices */
    vertices = SDL_stack_alloc(GLfloat, count * 2);
    for (idx = 0; idx < count; ++idx) {
        GLfloat x = points[idx].x + 0.5f;
        GLfloat y = points[idx].y + 0.5f;

        vertices[idx * 2] = x;
        vertices[(idx * 2) + 1] = y;
    }
    rdata->glGetError();
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    rdata->glDrawArrays(GL_LINE_STRIP, 0, count);

    /* We need to close the endpoint of the line */
    if (count == 2 ||
        points[0].x != points[count-1].x || points[0].y != points[count-1].y) {
        rdata->glDrawArrays(GL_POINTS, count-1, 1);
    }
    SDL_stack_free(vertices);
    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to render lines");
    }
    return 0;
}

static int
GLES2_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLfloat vertices[8];
    int idx;

    if (GLES2_SetDrawingState(renderer) < 0) {
        return -1;
    }

    /* Emit a line loop for each rectangle */
    rdata->glGetError();
    for (idx = 0; idx < count; ++idx) {
        const SDL_FRect *rect = &rects[idx];

        GLfloat xMin = rect->x;
        GLfloat xMax = (rect->x + rect->w);
        GLfloat yMin = rect->y;
        GLfloat yMax = (rect->y + rect->h);

        vertices[0] = xMin;
        vertices[1] = yMin;
        vertices[2] = xMax;
        vertices[3] = yMin;
        vertices[4] = xMin;
        vertices[5] = yMax;
        vertices[6] = xMax;
        vertices[7] = yMax;
        rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        rdata->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to render filled rects");
    }
    return 0;
}

static int
GLES2_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect,
                 const SDL_FRect *dstrect)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;
    GLES2_ImageSource sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
    SDL_BlendMode blendMode;
    GLfloat vertices[8];
    GLfloat texCoords[8];
    GLuint locTexture;
    GLuint locModulation;

    GLES2_ActivateRenderer(renderer);

    /* Activate an appropriate shader and set the projection matrix */
    blendMode = texture->blendMode;
    if (renderer->target) {
        /* Check if we need to do color mapping between the source and render target textures */
        if (renderer->target->format != texture->format) {
            switch (texture->format)
            {
            case SDL_PIXELFORMAT_ABGR8888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ARGB8888:
                    case SDL_PIXELFORMAT_RGB888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                    case SDL_PIXELFORMAT_BGR888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                        break;
                }
                break;
            case SDL_PIXELFORMAT_ARGB8888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ABGR8888:
                    case SDL_PIXELFORMAT_BGR888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                    case SDL_PIXELFORMAT_RGB888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                        break;
                }
                break;
            case SDL_PIXELFORMAT_BGR888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ABGR8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                        break;
                    case SDL_PIXELFORMAT_ARGB8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_RGB;
                        break;
                    case SDL_PIXELFORMAT_RGB888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                }
                break;
            case SDL_PIXELFORMAT_RGB888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ABGR8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                    case SDL_PIXELFORMAT_ARGB8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                        break;
                    case SDL_PIXELFORMAT_BGR888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                }
                break;
            }
        }
        else sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;   /* Texture formats match, use the non color mapping shader (even if the formats are not ABGR) */
    }
    else {
        switch (texture->format)
        {
            case SDL_PIXELFORMAT_ABGR8888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                break;
            case SDL_PIXELFORMAT_ARGB8888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                break;
            case SDL_PIXELFORMAT_BGR888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                break;
            case SDL_PIXELFORMAT_RGB888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_RGB;
                break;
            default:
                return -1;
        }
    }
    if (GLES2_SelectProgram(renderer, sourceType, blendMode) < 0)
        return -1;

    /* Select the target texture */
    locTexture = rdata->current_program->uniform_locations[GLES2_UNIFORM_TEXTURE];
    rdata->glGetError();
    rdata->glActiveTexture(GL_TEXTURE0);
    rdata->glBindTexture(tdata->texture_type, tdata->texture);
    rdata->glUniform1i(locTexture, 0);

    /* Configure color modulation */
    locModulation = rdata->current_program->uniform_locations[GLES2_UNIFORM_MODULATION];
    if (renderer->target &&
        (renderer->target->format == SDL_PIXELFORMAT_ARGB8888 ||
         renderer->target->format == SDL_PIXELFORMAT_RGB888)) {
        rdata->glUniform4f(locModulation,
                    texture->b * inv255f,
                    texture->g * inv255f,
                    texture->r * inv255f,
                    texture->a * inv255f);
    } else {
        rdata->glUniform4f(locModulation,
                    texture->r * inv255f,
                    texture->g * inv255f,
                    texture->b * inv255f,
                    texture->a * inv255f);
    }

    /* Configure texture blending */
    GLES2_SetBlendMode(rdata, blendMode);

    GLES2_SetTexCoords(rdata, SDL_TRUE);

    /* Emit the textured quad */
    vertices[0] = dstrect->x;
    vertices[1] = dstrect->y;
    vertices[2] = (dstrect->x + dstrect->w);
    vertices[3] = dstrect->y;
    vertices[4] = dstrect->x;
    vertices[5] = (dstrect->y + dstrect->h);
    vertices[6] = (dstrect->x + dstrect->w);
    vertices[7] = (dstrect->y + dstrect->h);
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    texCoords[0] = srcrect->x / (GLfloat)texture->w;
    texCoords[1] = srcrect->y / (GLfloat)texture->h;
    texCoords[2] = (srcrect->x + srcrect->w) / (GLfloat)texture->w;
    texCoords[3] = srcrect->y / (GLfloat)texture->h;
    texCoords[4] = srcrect->x / (GLfloat)texture->w;
    texCoords[5] = (srcrect->y + srcrect->h) / (GLfloat)texture->h;
    texCoords[6] = (srcrect->x + srcrect->w) / (GLfloat)texture->w;
    texCoords[7] = (srcrect->y + srcrect->h) / (GLfloat)texture->h;
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    rdata->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to render texture");
    }
    return 0;
}

static int
GLES2_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect,
                 const SDL_FRect *dstrect, const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *tdata = (GLES2_TextureData *)texture->driverdata;
    GLES2_ImageSource sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
    SDL_BlendMode blendMode;
    GLfloat vertices[8];
    GLfloat texCoords[8];
    GLuint locTexture;
    GLuint locModulation;
    GLfloat translate[8];
    GLfloat fAngle[4];
    GLfloat tmp;

    GLES2_ActivateRenderer(renderer);

    rdata->glEnableVertexAttribArray(GLES2_ATTRIBUTE_CENTER);
    rdata->glEnableVertexAttribArray(GLES2_ATTRIBUTE_ANGLE);
    fAngle[0] = fAngle[1] = fAngle[2] = fAngle[3] = (GLfloat)(360.0f - angle);
    /* Calculate the center of rotation */
    translate[0] = translate[2] = translate[4] = translate[6] = (center->x + dstrect->x);
    translate[1] = translate[3] = translate[5] = translate[7] = (center->y + dstrect->y);

    /* Activate an appropriate shader and set the projection matrix */
    blendMode = texture->blendMode;
    if (renderer->target) {
        /* Check if we need to do color mapping between the source and render target textures */
        if (renderer->target->format != texture->format) {
            switch (texture->format)
            {
            case SDL_PIXELFORMAT_ABGR8888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ARGB8888:
                    case SDL_PIXELFORMAT_RGB888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                    case SDL_PIXELFORMAT_BGR888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                        break;
                }
                break;
            case SDL_PIXELFORMAT_ARGB8888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ABGR8888:
                    case SDL_PIXELFORMAT_BGR888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                    case SDL_PIXELFORMAT_RGB888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                        break;
                }
                break;
            case SDL_PIXELFORMAT_BGR888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ABGR8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                        break;
                    case SDL_PIXELFORMAT_ARGB8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_RGB;
                        break;
                    case SDL_PIXELFORMAT_RGB888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                }
                break;
            case SDL_PIXELFORMAT_RGB888:
                switch (renderer->target->format)
                {
                    case SDL_PIXELFORMAT_ABGR8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                    case SDL_PIXELFORMAT_ARGB8888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                        break;
                    case SDL_PIXELFORMAT_BGR888:
                        sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                        break;
                }
                break;
            }
        }
        else sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;   /* Texture formats match, use the non color mapping shader (even if the formats are not ABGR) */
    }
    else {
        switch (texture->format)
        {
            case SDL_PIXELFORMAT_ABGR8888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_ABGR;
                break;
            case SDL_PIXELFORMAT_ARGB8888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_ARGB;
                break;
            case SDL_PIXELFORMAT_BGR888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_BGR;
                break;
            case SDL_PIXELFORMAT_RGB888:
                sourceType = GLES2_IMAGESOURCE_TEXTURE_RGB;
                break;
            default:
                return -1;
        }
    }
    if (GLES2_SelectProgram(renderer, sourceType, blendMode) < 0)
        return -1;

    /* Select the target texture */
    locTexture = rdata->current_program->uniform_locations[GLES2_UNIFORM_TEXTURE];
    rdata->glGetError();
    rdata->glActiveTexture(GL_TEXTURE0);
    rdata->glBindTexture(tdata->texture_type, tdata->texture);
    rdata->glUniform1i(locTexture, 0);

    /* Configure color modulation */
    locModulation = rdata->current_program->uniform_locations[GLES2_UNIFORM_MODULATION];
    if (renderer->target &&
        (renderer->target->format == SDL_PIXELFORMAT_ARGB8888 ||
         renderer->target->format == SDL_PIXELFORMAT_RGB888)) {
        rdata->glUniform4f(locModulation,
                    texture->b * inv255f,
                    texture->g * inv255f,
                    texture->r * inv255f,
                    texture->a * inv255f);
    } else {
        rdata->glUniform4f(locModulation,
                    texture->r * inv255f,
                    texture->g * inv255f,
                    texture->b * inv255f,
                    texture->a * inv255f);
    }

    /* Configure texture blending */
    GLES2_SetBlendMode(rdata, blendMode);

    GLES2_SetTexCoords(rdata, SDL_TRUE);

    /* Emit the textured quad */
    vertices[0] = dstrect->x;
    vertices[1] = dstrect->y;
    vertices[2] = (dstrect->x + dstrect->w);
    vertices[3] = dstrect->y;
    vertices[4] = dstrect->x;
    vertices[5] = (dstrect->y + dstrect->h);
    vertices[6] = (dstrect->x + dstrect->w);
    vertices[7] = (dstrect->y + dstrect->h);
    if (flip & SDL_FLIP_HORIZONTAL) {
        tmp = vertices[0];
        vertices[0] = vertices[4] = vertices[2];
        vertices[2] = vertices[6] = tmp;
    }
    if (flip & SDL_FLIP_VERTICAL) {
        tmp = vertices[1];
        vertices[1] = vertices[3] = vertices[5];
        vertices[5] = vertices[7] = tmp;
    }

    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_ANGLE, 1, GL_FLOAT, GL_FALSE, 0, &fAngle);
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_CENTER, 2, GL_FLOAT, GL_FALSE, 0, translate);
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);

    texCoords[0] = srcrect->x / (GLfloat)texture->w;
    texCoords[1] = srcrect->y / (GLfloat)texture->h;
    texCoords[2] = (srcrect->x + srcrect->w) / (GLfloat)texture->w;
    texCoords[3] = srcrect->y / (GLfloat)texture->h;
    texCoords[4] = srcrect->x / (GLfloat)texture->w;
    texCoords[5] = (srcrect->y + srcrect->h) / (GLfloat)texture->h;
    texCoords[6] = (srcrect->x + srcrect->w) / (GLfloat)texture->w;
    texCoords[7] = (srcrect->y + srcrect->h) / (GLfloat)texture->h;
    rdata->glVertexAttribPointer(GLES2_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    rdata->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    rdata->glDisableVertexAttribArray(GLES2_ATTRIBUTE_CENTER);
    rdata->glDisableVertexAttribArray(GLES2_ATTRIBUTE_ANGLE);
    if (rdata->glGetError() != GL_NO_ERROR) {
        return SDL_SetError("Failed to render texture");
    }
    return 0;
}

static int
GLES2_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *)renderer->driverdata;
    Uint32 temp_format = SDL_PIXELFORMAT_ABGR8888;
    void *temp_pixels;
    int temp_pitch;
    Uint8 *src, *dst, *tmp;
    int w, h, length, rows;
    int status;

    GLES2_ActivateRenderer(renderer);

    temp_pitch = rect->w * SDL_BYTESPERPIXEL(temp_format);
    temp_pixels = SDL_malloc(rect->h * temp_pitch);
    if (!temp_pixels) {
        return SDL_OutOfMemory();
    }

    SDL_GetRendererOutputSize(renderer, &w, &h);

    rdata->glPixelStorei(GL_PACK_ALIGNMENT, 1);

    rdata->glReadPixels(rect->x, (h-rect->y)-rect->h, rect->w, rect->h,
                       GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels);

    /* Flip the rows to be top-down */
    length = rect->w * SDL_BYTESPERPIXEL(temp_format);
    src = (Uint8*)temp_pixels + (rect->h-1)*temp_pitch;
    dst = (Uint8*)temp_pixels;
    tmp = SDL_stack_alloc(Uint8, length);
    rows = rect->h / 2;
    while (rows--) {
        SDL_memcpy(tmp, dst, length);
        SDL_memcpy(dst, src, length);
        SDL_memcpy(src, tmp, length);
        dst += temp_pitch;
        src -= temp_pitch;
    }
    SDL_stack_free(tmp);

    status = SDL_ConvertPixels(rect->w, rect->h,
                               temp_format, temp_pixels, temp_pitch,
                               pixel_format, pixels, pitch);
    SDL_free(temp_pixels);

    return status;
}

static void
GLES2_RenderPresent(SDL_Renderer *renderer)
{
    GLES2_ActivateRenderer(renderer);

    /* Tell the video driver to swap buffers */
    SDL_GL_SwapWindow(renderer->window);
}


/*************************************************************************************************
 * Bind/unbinding of textures
 *************************************************************************************************/
static int GLES2_BindTexture (SDL_Renderer * renderer, SDL_Texture *texture, float *texw, float *texh);
static int GLES2_UnbindTexture (SDL_Renderer * renderer, SDL_Texture *texture);

static int GLES2_BindTexture (SDL_Renderer * renderer, SDL_Texture *texture, float *texw, float *texh) {
    GLES2_DriverContext *data = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *texturedata = (GLES2_TextureData *)texture->driverdata;
    GLES2_ActivateRenderer(renderer);

    data->glBindTexture(texturedata->texture_type, texturedata->texture);

    if(texw) *texw = 1.0;
    if(texh) *texh = 1.0;

    return 0;
}

static int GLES2_UnbindTexture (SDL_Renderer * renderer, SDL_Texture *texture) {
    GLES2_DriverContext *data = (GLES2_DriverContext *)renderer->driverdata;
    GLES2_TextureData *texturedata = (GLES2_TextureData *)texture->driverdata;
    GLES2_ActivateRenderer(renderer);

    data->glBindTexture(texturedata->texture_type, 0);

    return 0;
}


/*************************************************************************************************
 * Renderer instantiation                                                                        *
 *************************************************************************************************/

#define GL_NVIDIA_PLATFORM_BINARY_NV 0x890B

static void
GLES2_ResetState(SDL_Renderer *renderer)
{
    GLES2_DriverContext *rdata = (GLES2_DriverContext *) renderer->driverdata;

    if (SDL_CurrentContext == rdata->context) {
        GLES2_UpdateViewport(renderer);
    } else {
        GLES2_ActivateRenderer(renderer);
    }

    rdata->current.blendMode = -1;
    rdata->current.tex_coords = SDL_FALSE;

    rdata->glEnableVertexAttribArray(GLES2_ATTRIBUTE_POSITION);
    rdata->glDisableVertexAttribArray(GLES2_ATTRIBUTE_TEXCOORD);
}

static SDL_Renderer *
GLES2_CreateRenderer(SDL_Window *window, Uint32 flags)
{
    SDL_Renderer *renderer;
    GLES2_DriverContext *rdata;
    GLint nFormats;
#ifndef ZUNE_HD
    GLboolean hasCompiler;
#endif
    Uint32 windowFlags;
    GLint window_framebuffer;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    windowFlags = SDL_GetWindowFlags(window);
    if (!(windowFlags & SDL_WINDOW_OPENGL)) {
        if (SDL_RecreateWindow(window, windowFlags | SDL_WINDOW_OPENGL) < 0) {
            /* Uh oh, better try to put it back... */
            SDL_RecreateWindow(window, windowFlags);
            return NULL;
        }
    }

    /* Create the renderer struct */
    renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(SDL_Renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    rdata = (GLES2_DriverContext *)SDL_calloc(1, sizeof(GLES2_DriverContext));
    if (!rdata) {
        GLES2_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    renderer->info = GLES2_RenderDriver.info;
    renderer->info.flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
    renderer->driverdata = rdata;
    renderer->window = window;

    /* Create an OpenGL ES 2.0 context */
    rdata->context = SDL_GL_CreateContext(window);
    if (!rdata->context)
    {
        GLES2_DestroyRenderer(renderer);
        return NULL;
    }
    if (SDL_GL_MakeCurrent(window, rdata->context) < 0) {
        GLES2_DestroyRenderer(renderer);
        return NULL;
    }

    if (GLES2_LoadFunctions(rdata) < 0) {
        GLES2_DestroyRenderer(renderer);
        return NULL;
    }

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }
    if (SDL_GL_GetSwapInterval() > 0) {
        renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    /* Determine supported shader formats */
    /* HACK: glGetInteger is broken on the Zune HD's compositor, so we just hardcode this */
    rdata->glGetError();
#ifdef ZUNE_HD
    nFormats = 1;
#else /* !ZUNE_HD */
    rdata->glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &nFormats);
    rdata->glGetBooleanv(GL_SHADER_COMPILER, &hasCompiler);
    if (hasCompiler)
        ++nFormats;
#endif /* ZUNE_HD */
    rdata->shader_formats = (GLenum *)SDL_calloc(nFormats, sizeof(GLenum));
    if (!rdata->shader_formats)
    {
        GLES2_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    rdata->shader_format_count = nFormats;
#ifdef ZUNE_HD
    rdata->shader_formats[0] = GL_NVIDIA_PLATFORM_BINARY_NV;
#else /* !ZUNE_HD */
    rdata->glGetIntegerv(GL_SHADER_BINARY_FORMATS, (GLint *)rdata->shader_formats);
    if (rdata->glGetError() != GL_NO_ERROR)
    {
        GLES2_DestroyRenderer(renderer);
        SDL_SetError("Failed to query supported shader formats");
        return NULL;
    }
    if (hasCompiler)
        rdata->shader_formats[nFormats - 1] = (GLenum)-1;
#endif /* ZUNE_HD */

    rdata->framebuffers = NULL;
    rdata->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &window_framebuffer);
    rdata->window_framebuffer = (GLuint)window_framebuffer;

    /* Populate the function pointers for the module */
    renderer->WindowEvent         = &GLES2_WindowEvent;
    renderer->CreateTexture       = &GLES2_CreateTexture;
    renderer->UpdateTexture       = &GLES2_UpdateTexture;
    renderer->LockTexture         = &GLES2_LockTexture;
    renderer->UnlockTexture       = &GLES2_UnlockTexture;
    renderer->SetRenderTarget     = &GLES2_SetRenderTarget;
    renderer->UpdateViewport      = &GLES2_UpdateViewport;
    renderer->UpdateClipRect      = &GLES2_UpdateClipRect;
    renderer->RenderClear         = &GLES2_RenderClear;
    renderer->RenderDrawPoints    = &GLES2_RenderDrawPoints;
    renderer->RenderDrawLines     = &GLES2_RenderDrawLines;
    renderer->RenderFillRects     = &GLES2_RenderFillRects;
    renderer->RenderCopy          = &GLES2_RenderCopy;
    renderer->RenderCopyEx        = &GLES2_RenderCopyEx;
    renderer->RenderReadPixels    = &GLES2_RenderReadPixels;
    renderer->RenderPresent       = &GLES2_RenderPresent;
    renderer->DestroyTexture      = &GLES2_DestroyTexture;
    renderer->DestroyRenderer     = &GLES2_DestroyRenderer;
    renderer->GL_BindTexture      = &GLES2_BindTexture;
    renderer->GL_UnbindTexture    = &GLES2_UnbindTexture;

    GLES2_ResetState(renderer);

    return renderer;
}

#endif /* SDL_VIDEO_RENDER_OGL_ES2 && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
