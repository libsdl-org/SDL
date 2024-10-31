/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Display a video with a sprite bouncing around over it
 *
 * For a more complete video example, see ffplay.c in the ffmpeg sources.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#ifdef HAVE_EGL
#include <SDL3/SDL_egl.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengles2.h>

#include <libavutil/hwcontext_drm.h>

#ifndef fourcc_code
#define fourcc_code(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#endif
#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8 fourcc_code('R', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_GR88
#define DRM_FORMAT_GR88 fourcc_code('G', 'R', '8', '8')
#endif
#endif

#define DRM_FORMAT_MOD_VENDOR_NONE 0
#define DRM_FORMAT_RESERVED        ((1ULL << 56) - 1)

#define fourcc_mod_get_vendor(modifier) \
    (((modifier) >> 56) & 0xff)

#define fourcc_mod_is_vendor(modifier, vendor) \
    (fourcc_mod_get_vendor(modifier) == DRM_FORMAT_MOD_VENDOR_##vendor)

#define fourcc_mod_code(vendor, val) \
    ((((Uint64)DRM_FORMAT_MOD_VENDOR_##vendor) << 56) | ((val) & 0x00ffffffffffffffULL))

#define DRM_FORMAT_MOD_INVALID fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)
#define DRM_FORMAT_MOD_LINEAR  fourcc_mod_code(NONE, 0)

#ifdef SDL_PLATFORM_APPLE
#include <CoreVideo/CoreVideo.h>
#endif

#ifdef SDL_PLATFORM_WIN32
#define COBJMACROS
#include <libavutil/hwcontext_d3d11va.h>
#endif /* SDL_PLATFORM_WIN32 */

#include "testffmpeg_vulkan.h"

#include "icon.h"

static SDL_Texture *sprite;
static SDL_FRect *positions;
static SDL_FRect *velocities;
static int sprite_w, sprite_h;
static int num_sprites = 0;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_AudioStream *audio;
static SDL_Texture *video_texture;
static Uint64 video_start;
static bool software_only;
static bool has_eglCreateImage;
#ifdef HAVE_EGL
static bool has_EGL_EXT_image_dma_buf_import;
static bool has_EGL_EXT_image_dma_buf_import_modifiers;
static PFNGLACTIVETEXTUREARBPROC glActiveTextureARBFunc;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOESFunc;
#endif
#ifdef SDL_PLATFORM_WIN32
static ID3D11Device *d3d11_device;
static ID3D11DeviceContext *d3d11_context;
static const GUID SDL_IID_ID3D11Resource = { 0xdc8e63f3, 0xd12b, 0x4952, { 0xb4, 0x7b, 0x5e, 0x45, 0x02, 0x6a, 0x86, 0x2d } };
#endif
static VulkanVideoContext *vulkan_context;
struct SwsContextContainer
{
    struct SwsContext *context;
};
static const char *SWS_CONTEXT_CONTAINER_PROPERTY = "SWS_CONTEXT_CONTAINER";
static bool verbose;

static bool CreateWindowAndRenderer(SDL_WindowFlags window_flags, const char *driver)
{
    SDL_PropertiesID props;
    bool useOpenGL = (driver && (SDL_strcmp(driver, "opengl") == 0 || SDL_strcmp(driver, "opengles2") == 0));
    bool useEGL = (driver && SDL_strcmp(driver, "opengles2") == 0);
    bool useVulkan = (driver && SDL_strcmp(driver, "vulkan") == 0);
    Uint32 flags = SDL_WINDOW_HIDDEN;

    if (useOpenGL) {
        if (useEGL) {
            SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        } else {
            SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "0");
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        }
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);

        flags |= SDL_WINDOW_OPENGL;
    }
    if (useVulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }

    /* The window will be resized to the video size when it's loaded, in OpenVideoStream() */
    window = SDL_CreateWindow("testffmpeg", 1920, 1080, flags);
    if (!window) {
        return false;
    }

    if (useVulkan) {
        vulkan_context = CreateVulkanVideoContext(window);
        if (!vulkan_context) {
            SDL_DestroyWindow(window);
            window = NULL;
            return false;
        }
    }

    props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, driver);
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    if (useVulkan) {
        SetupVulkanRenderProperties(vulkan_context, props);
    }
    if (SDL_GetBooleanProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN, false)) {
        /* Try to create an HDR capable renderer */
        SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB_LINEAR);
        renderer = SDL_CreateRendererWithProperties(props);
    }
    if (!renderer) {
        /* Try again with the sRGB colorspace */
        SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);
        renderer = SDL_CreateRendererWithProperties(props);
    }
    SDL_DestroyProperties(props);
    if (!renderer) {
        SDL_DestroyWindow(window);
        window = NULL;
        return false;
    }

    SDL_Log("Created renderer %s\n", SDL_GetRendererName(renderer));

#ifdef HAVE_EGL
    if (useEGL) {
        const char *egl_extensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
        if (!egl_extensions) {
            return false;
        }

        char *extensions = SDL_strdup(egl_extensions);
        if (!extensions) {
            return false;
        }

        char *saveptr, *token;
        token = SDL_strtok_r(extensions, " ", &saveptr);
        if (!token) {
            SDL_free(extensions);
            return false;
        }
        do {
            if (SDL_strcmp(token, "EGL_EXT_image_dma_buf_import") == 0) {
                has_EGL_EXT_image_dma_buf_import = true;
            } else if (SDL_strcmp(token, "EGL_EXT_image_dma_buf_import_modifiers") == 0) {
                has_EGL_EXT_image_dma_buf_import_modifiers = true;
            }
        } while ((token = SDL_strtok_r(NULL, " ", &saveptr)) != NULL);

        SDL_free(extensions);

        if (SDL_GL_ExtensionSupported("GL_OES_EGL_image")) {
            glEGLImageTargetTexture2DOESFunc = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        }

        glActiveTextureARBFunc = (PFNGLACTIVETEXTUREARBPROC)SDL_GL_GetProcAddress("glActiveTextureARB");

        if (has_EGL_EXT_image_dma_buf_import &&
            glEGLImageTargetTexture2DOESFunc &&
            glActiveTextureARBFunc) {
            has_eglCreateImage = true;
        }
    }
#endif /* HAVE_EGL */

#ifdef SDL_PLATFORM_WIN32
    d3d11_device = (ID3D11Device *)SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, NULL);
    if (d3d11_device) {
        ID3D11Device_AddRef(d3d11_device);
        ID3D11Device_GetImmediateContext(d3d11_device, &d3d11_context);
    }
#endif

    return true;
}

static SDL_Texture *CreateTexture(SDL_Renderer *r, unsigned char *data, unsigned int len, int *w, int *h)
{
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_IOStream *src = SDL_IOFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_IO(src, true);
        if (surface) {
            /* Treat white as transparent */
            SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, 255, 255, 255));

            texture = SDL_CreateTextureFromSurface(r, surface);
            *w = surface->w;
            *h = surface->h;
            SDL_DestroySurface(surface);
        }
    }
    return texture;
}

static void MoveSprite(void)
{
    SDL_Rect viewport;
    SDL_FRect *position, *velocity;
    int i;

    SDL_GetRenderViewport(renderer, &viewport);

    for (i = 0; i < num_sprites; ++i) {
        position = &positions[i];
        velocity = &velocities[i];
        position->x += velocity->x;
        if ((position->x < 0) || (position->x >= (viewport.w - sprite_w))) {
            velocity->x = -velocity->x;
            position->x += velocity->x;
        }
        position->y += velocity->y;
        if ((position->y < 0) || (position->y >= (viewport.h - sprite_h))) {
            velocity->y = -velocity->y;
            position->y += velocity->y;
        }
    }

    /* Blit the sprite onto the screen */
    for (i = 0; i < num_sprites; ++i) {
        position = &positions[i];

        /* Blit the sprite onto the screen */
        SDL_RenderTexture(renderer, sprite, NULL, position);
    }
}

static SDL_PixelFormat GetTextureFormat(enum AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_RGB8:
        return SDL_PIXELFORMAT_RGB332;
    case AV_PIX_FMT_RGB444:
        return SDL_PIXELFORMAT_XRGB4444;
    case AV_PIX_FMT_RGB555:
        return SDL_PIXELFORMAT_XRGB1555;
    case AV_PIX_FMT_BGR555:
        return SDL_PIXELFORMAT_XBGR1555;
    case AV_PIX_FMT_RGB565:
        return SDL_PIXELFORMAT_RGB565;
    case AV_PIX_FMT_BGR565:
        return SDL_PIXELFORMAT_BGR565;
    case AV_PIX_FMT_RGB24:
        return SDL_PIXELFORMAT_RGB24;
    case AV_PIX_FMT_BGR24:
        return SDL_PIXELFORMAT_BGR24;
    case AV_PIX_FMT_0RGB32:
        return SDL_PIXELFORMAT_XRGB8888;
    case AV_PIX_FMT_0BGR32:
        return SDL_PIXELFORMAT_XBGR8888;
    case AV_PIX_FMT_NE(RGB0, 0BGR):
        return SDL_PIXELFORMAT_RGBX8888;
    case AV_PIX_FMT_NE(BGR0, 0RGB):
        return SDL_PIXELFORMAT_BGRX8888;
    case AV_PIX_FMT_RGB32:
        return SDL_PIXELFORMAT_ARGB8888;
    case AV_PIX_FMT_RGB32_1:
        return SDL_PIXELFORMAT_RGBA8888;
    case AV_PIX_FMT_BGR32:
        return SDL_PIXELFORMAT_ABGR8888;
    case AV_PIX_FMT_BGR32_1:
        return SDL_PIXELFORMAT_BGRA8888;
    case AV_PIX_FMT_YUV420P:
        return SDL_PIXELFORMAT_IYUV;
    case AV_PIX_FMT_YUYV422:
        return SDL_PIXELFORMAT_YUY2;
    case AV_PIX_FMT_UYVY422:
        return SDL_PIXELFORMAT_UYVY;
    case AV_PIX_FMT_NV12:
        return SDL_PIXELFORMAT_NV12;
    case AV_PIX_FMT_NV21:
        return SDL_PIXELFORMAT_NV21;
    case AV_PIX_FMT_P010:
        return SDL_PIXELFORMAT_P010;
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static bool SupportedPixelFormat(enum AVPixelFormat format)
{
    if (!software_only) {
        if (has_eglCreateImage &&
            (format == AV_PIX_FMT_VAAPI || format == AV_PIX_FMT_DRM_PRIME)) {
            return true;
        }
#ifdef SDL_PLATFORM_APPLE
        if (format == AV_PIX_FMT_VIDEOTOOLBOX) {
            return true;
        }
#endif
#ifdef SDL_PLATFORM_WIN32
        if (d3d11_device && format == AV_PIX_FMT_D3D11) {
            return true;
        }
#endif
        if (vulkan_context && format == AV_PIX_FMT_VULKAN) {
            return true;
        }
    }

    if (GetTextureFormat(format) != SDL_PIXELFORMAT_UNKNOWN) {
        return true;
    }
    return false;
}

static enum AVPixelFormat GetSupportedPixelFormat(AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);

        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            /* We support all memory formats using swscale */
            break;
        }

        if (SupportedPixelFormat(*p)) {
            /* We support this format */
            break;
        }
    }

    if (*p == AV_PIX_FMT_NONE) {
        SDL_Log("Couldn't find a supported pixel format:\n");
        for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            SDL_Log("    %s\n", av_get_pix_fmt_name(*p));
        }
    }

    return *p;
}

static AVCodecContext *OpenVideoStream(AVFormatContext *ic, int stream, const AVCodec *codec)
{
    AVStream *st = ic->streams[stream];
    AVCodecParameters *codecpar = st->codecpar;
    AVCodecContext *context;
    const AVCodecHWConfig *config;
    int i;
    int result;

    SDL_Log("Video stream: %s %dx%d\n", avcodec_get_name(codec->id), codecpar->width, codecpar->height);

    context = avcodec_alloc_context3(NULL);
    if (!context) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_alloc_context3 failed");
        return NULL;
    }

    result = avcodec_parameters_to_context(context, ic->streams[stream]->codecpar);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_parameters_to_context failed: %s\n", av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }
    context->pkt_timebase = ic->streams[stream]->time_base;

    /* Look for supported hardware accelerated configurations */
    i = 0;
    while (!context->hw_device_ctx &&
           (config = avcodec_get_hw_config(codec, i++)) != NULL) {
#if 0
        SDL_Log("Found %s hardware acceleration with pixel format %s\n", av_hwdevice_get_type_name(config->device_type), av_get_pix_fmt_name(config->pix_fmt));
#endif

        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) ||
            !SupportedPixelFormat(config->pix_fmt)) {
            continue;
        }

#ifdef SDL_PLATFORM_WIN32
        if (d3d11_device && config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
            AVD3D11VADeviceContext *device_context;

            context->hw_device_ctx = av_hwdevice_ctx_alloc(config->device_type);

            device_context = (AVD3D11VADeviceContext *)((AVHWDeviceContext *)context->hw_device_ctx->data)->hwctx;
            device_context->device = d3d11_device;
            ID3D11Device_AddRef(device_context->device);
            device_context->device_context = d3d11_context;
            ID3D11DeviceContext_AddRef(device_context->device_context);

            result = av_hwdevice_ctx_init(context->hw_device_ctx);
            if (result < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create %s hardware device context: %s", av_hwdevice_get_type_name(config->device_type), av_err2str(result));
            } else {
                SDL_Log("Using %s hardware acceleration with pixel format %s\n", av_hwdevice_get_type_name(config->device_type), av_get_pix_fmt_name(config->pix_fmt));
            }
        } else
#endif
        if (vulkan_context && config->device_type == AV_HWDEVICE_TYPE_VULKAN) {
            AVVulkanDeviceContext *device_context;

            context->hw_device_ctx = av_hwdevice_ctx_alloc(config->device_type);

            device_context = (AVVulkanDeviceContext *)((AVHWDeviceContext *)context->hw_device_ctx->data)->hwctx;
            SetupVulkanDeviceContextData(vulkan_context, device_context);

            result = av_hwdevice_ctx_init(context->hw_device_ctx);
            if (result < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create %s hardware device context: %s", av_hwdevice_get_type_name(config->device_type), av_err2str(result));
            } else {
                SDL_Log("Using %s hardware acceleration with pixel format %s\n", av_hwdevice_get_type_name(config->device_type), av_get_pix_fmt_name(config->pix_fmt));
            }
        } else {
            result = av_hwdevice_ctx_create(&context->hw_device_ctx, config->device_type, NULL, NULL, 0);
            if (result < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create %s hardware device context: %s", av_hwdevice_get_type_name(config->device_type), av_err2str(result));
            } else {
                SDL_Log("Using %s hardware acceleration with pixel format %s\n", av_hwdevice_get_type_name(config->device_type), av_get_pix_fmt_name(config->pix_fmt));
            }
        }
    }

    /* Allow supported hardware accelerated pixel formats */
    context->get_format = GetSupportedPixelFormat;

    if (codecpar->codec_id == AV_CODEC_ID_VVC) {
        context->strict_std_compliance = -2;

        /* Enable threaded decoding, VVC decode is slow */
        context->thread_count = SDL_GetNumLogicalCPUCores();
        context->thread_type = (FF_THREAD_FRAME | FF_THREAD_SLICE);
    }

    result = avcodec_open2(context, codec, NULL);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open codec %s: %s", avcodec_get_name(context->codec_id), av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }

    SDL_SetWindowSize(window, codecpar->width, codecpar->height);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    return context;
}

static SDL_Colorspace GetFrameColorspace(AVFrame *frame)
{
    SDL_Colorspace colorspace = SDL_COLORSPACE_SRGB;

    if (frame && frame->colorspace != AVCOL_SPC_RGB) {
#ifdef DEBUG_COLORSPACE
        SDL_Log("Frame colorspace: range: %d, primaries: %d, trc: %d, colorspace: %d, chroma_location: %d\n", frame->color_range, frame->color_primaries, frame->color_trc, frame->colorspace, frame->chroma_location);
#endif
        colorspace = SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                           frame->color_range,
                                           frame->color_primaries,
                                           frame->color_trc,
                                           frame->colorspace,
                                           frame->chroma_location);
    }
    return colorspace;
}

static SDL_PropertiesID CreateVideoTextureProperties(AVFrame *frame, SDL_PixelFormat format, int access)
{
    AVFrameSideData *pSideData;
    SDL_PropertiesID props;
    int width = frame->width;
    int height = frame->height;
    SDL_Colorspace colorspace = GetFrameColorspace(frame);

    /* ITU-R BT.2408-6 recommends using an SDR white point of 203 nits, which is more likely for game content */
    static const float k_flSDRWhitePoint = 203.0f;
    float flMaxLuminance = k_flSDRWhitePoint;

    if (frame->hw_frames_ctx) {
        AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx->data);

        width = frames->width;
        height = frames->height;
        if (format == SDL_PIXELFORMAT_UNKNOWN) {
            format = GetTextureFormat(frames->sw_format);
        }
    } else {
        if (format == SDL_PIXELFORMAT_UNKNOWN) {
            format = GetTextureFormat(frame->format);
        }
    }

    props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, colorspace);
    pSideData = av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    if (pSideData) {
        AVMasteringDisplayMetadata *pMasteringDisplayMetadata = (AVMasteringDisplayMetadata *)pSideData->data;
        flMaxLuminance = (float)pMasteringDisplayMetadata->max_luminance.num / pMasteringDisplayMetadata->max_luminance.den;
    } else if (SDL_COLORSPACETRANSFER(colorspace) == SDL_TRANSFER_CHARACTERISTICS_PQ) {
        /* The official definition is 10000, but PQ game content is often mastered for 400 or 1000 nits */
        flMaxLuminance = 1000.0f;
    }
    if (flMaxLuminance > k_flSDRWhitePoint) {
        SDL_SetFloatProperty(props, SDL_PROP_TEXTURE_CREATE_SDR_WHITE_POINT_FLOAT, k_flSDRWhitePoint);
        SDL_SetFloatProperty(props, SDL_PROP_TEXTURE_CREATE_HDR_HEADROOM_FLOAT, flMaxLuminance / k_flSDRWhitePoint);
    }
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, format);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, access);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, height);

    return props;
}

static void SDLCALL FreeSwsContextContainer(void *userdata, void *value)
{
    struct SwsContextContainer *sws_container = (struct SwsContextContainer *)value;
    if (sws_container->context) {
        sws_freeContext(sws_container->context);
    }
    SDL_free(sws_container);
}

static bool GetTextureForMemoryFrame(AVFrame *frame, SDL_Texture **texture)
{
    int texture_width = 0, texture_height = 0;
    SDL_PixelFormat texture_format = SDL_PIXELFORMAT_UNKNOWN;
    SDL_PixelFormat frame_format = GetTextureFormat(frame->format);

    if (*texture) {
        SDL_PropertiesID props = SDL_GetTextureProperties(*texture);
        texture_format = (SDL_PixelFormat)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, SDL_PIXELFORMAT_UNKNOWN);
        texture_width = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
        texture_height = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);
    }
    if (!*texture || texture_width != frame->width || texture_height != frame->height ||
        (frame_format != SDL_PIXELFORMAT_UNKNOWN && texture_format != frame_format) ||
        (frame_format == SDL_PIXELFORMAT_UNKNOWN && texture_format != SDL_PIXELFORMAT_ARGB8888)) {
        if (*texture) {
            SDL_DestroyTexture(*texture);
        }

        SDL_PropertiesID props;
        if (frame_format == SDL_PIXELFORMAT_UNKNOWN) {
            props = CreateVideoTextureProperties(frame, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING);
        } else {
            props = CreateVideoTextureProperties(frame, frame_format, SDL_TEXTUREACCESS_STREAMING);
        }
        *texture = SDL_CreateTextureWithProperties(renderer, props);
        SDL_DestroyProperties(props);
        if (!*texture) {
            return false;
        }

        if (frame_format == SDL_PIXELFORMAT_UNKNOWN || SDL_ISPIXELFORMAT_ALPHA(frame_format)) {
            SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);
        } else {
            SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_NONE);
        }
        SDL_SetTextureScaleMode(*texture, SDL_SCALEMODE_LINEAR);
    }

    switch (frame_format) {
    case SDL_PIXELFORMAT_UNKNOWN:
    {
        SDL_PropertiesID props = SDL_GetTextureProperties(*texture);
        struct SwsContextContainer *sws_container = (struct SwsContextContainer *)SDL_GetPointerProperty(props, SWS_CONTEXT_CONTAINER_PROPERTY, NULL);
        if (!sws_container) {
            sws_container = (struct SwsContextContainer *)SDL_calloc(1, sizeof(*sws_container));
            if (!sws_container) {
                return false;
            }
            SDL_SetPointerPropertyWithCleanup(props, SWS_CONTEXT_CONTAINER_PROPERTY, sws_container, FreeSwsContextContainer, NULL);
        }
        sws_container->context = sws_getCachedContext(sws_container->context, frame->width, frame->height, frame->format, frame->width, frame->height, AV_PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
        if (sws_container->context) {
            uint8_t *pixels[4];
            int pitch[4];
            if (SDL_LockTexture(*texture, NULL, (void **)&pixels[0], &pitch[0])) {
                sws_scale(sws_container->context, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, pixels, pitch);
                SDL_UnlockTexture(*texture);
            }
        } else {
            SDL_SetError("Can't initialize the conversion context");
            return false;
        }
        break;
    }
    case SDL_PIXELFORMAT_IYUV:
        if (frame->linesize[0] > 0 && frame->linesize[1] > 0 && frame->linesize[2] > 0) {
            SDL_UpdateYUVTexture(*texture, NULL, frame->data[0], frame->linesize[0],
                                 frame->data[1], frame->linesize[1],
                                 frame->data[2], frame->linesize[2]);
        } else if (frame->linesize[0] < 0 && frame->linesize[1] < 0 && frame->linesize[2] < 0) {
            SDL_UpdateYUVTexture(*texture, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0],
                                 frame->data[1] + frame->linesize[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[1],
                                 frame->data[2] + frame->linesize[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[2]);
        }
        break;
    default:
        if (frame->linesize[0] < 0) {
            SDL_UpdateTexture(*texture, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
        } else {
            SDL_UpdateTexture(*texture, NULL, frame->data[0], frame->linesize[0]);
        }
        break;
    }
    return true;
}

#ifdef HAVE_EGL

static bool GetNV12TextureForDRMFrame(AVFrame *frame, SDL_Texture **texture)
{
    AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx ? frame->hw_frames_ctx->data : NULL);
    const AVDRMFrameDescriptor *desc = (const AVDRMFrameDescriptor *)frame->data[0];
    int i, j, image_index;
    EGLDisplay display = eglGetCurrentDisplay();
    SDL_PropertiesID props;
    GLuint textures[2];

    if (*texture) {
        /* Free the previous texture now that we're about to render a new one */
        SDL_DestroyTexture(*texture);
    } else {
        /* First time set up for NV12 textures */
        SDL_SetHint("SDL_RENDER_OPENGL_NV12_RG_SHADER", "1");
    }

    props = CreateVideoTextureProperties(frame, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STATIC);
    *texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (!*texture) {
        return false;
    }
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(*texture, SDL_SCALEMODE_LINEAR);

    props = SDL_GetTextureProperties(*texture);
    textures[0] = (GLuint)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, 0);
    textures[1] = (GLuint)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_UV_NUMBER, 0);
    if (!textures[0] || !textures[1]) {
        SDL_SetError("Couldn't get NV12 OpenGL textures");
        return false;
    }

    /* import the frame into OpenGL */
    image_index = 0;
    for (i = 0; i < desc->nb_layers; ++i) {
        const AVDRMLayerDescriptor *layer = &desc->layers[i];
        for (j = 0; j < layer->nb_planes; ++j) {
            const AVDRMPlaneDescriptor *plane = &layer->planes[j];
            const AVDRMObjectDescriptor *object = &desc->objects[plane->object_index];

            EGLAttrib attr[32];
            size_t k = 0;

            attr[k++] = EGL_LINUX_DRM_FOURCC_EXT;
            attr[k++] = layer->format;

            attr[k++] = EGL_WIDTH;
            attr[k++] = (frames ? frames->width : frame->width) / (image_index + 1); /* half size for chroma */

            attr[k++] = EGL_HEIGHT;
            attr[k++] = (frames ? frames->height : frame->height) / (image_index + 1);

            attr[k++] = EGL_DMA_BUF_PLANE0_FD_EXT;
            attr[k++] = object->fd;

            attr[k++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
            attr[k++] = plane->offset;

            attr[k++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
            attr[k++] = plane->pitch;

            if (has_EGL_EXT_image_dma_buf_import_modifiers) {
                attr[k++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
                attr[k++] = (object->format_modifier >> 0) & 0xFFFFFFFF;

                attr[k++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
                attr[k++] = (object->format_modifier >> 32) & 0xFFFFFFFF;
            }

            attr[k++] = EGL_NONE;

            EGLImage image = eglCreateImage(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attr);
            if (image == EGL_NO_IMAGE) {
                SDL_Log("Couldn't create image: %d\n", glGetError());
                return false;
            }

            glActiveTextureARBFunc(GL_TEXTURE0_ARB + image_index);
            glBindTexture(GL_TEXTURE_2D, textures[image_index]);
            glEGLImageTargetTexture2DOESFunc(GL_TEXTURE_2D, image);
            ++image_index;
        }
    }

    return true;
}

static bool GetOESTextureForDRMFrame(AVFrame *frame, SDL_Texture **texture)
{
    AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx ? frame->hw_frames_ctx->data : NULL);
    const AVDRMFrameDescriptor *desc = (const AVDRMFrameDescriptor *)frame->data[0];
    int i, j, k, image_index;
    EGLDisplay display = eglGetCurrentDisplay();
    SDL_PropertiesID props;
    GLuint textureID;
    EGLAttrib attr[64];
    SDL_Colorspace colorspace;

    if (*texture) {
        /* Free the previous texture now that we're about to render a new one */
        SDL_DestroyTexture(*texture);
    }

    props = CreateVideoTextureProperties(frame, SDL_PIXELFORMAT_EXTERNAL_OES, SDL_TEXTUREACCESS_STATIC);
    *texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (!*texture) {
        return false;
    }
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(*texture, SDL_SCALEMODE_LINEAR);

    props = SDL_GetTextureProperties(*texture);
    textureID = (GLuint)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, 0);
    if (!textureID) {
        SDL_SetError("Couldn't get OpenGL texture");
        return false;
    }
    colorspace = (SDL_Colorspace)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_COLORSPACE_NUMBER, SDL_COLORSPACE_UNKNOWN);

    /* import the frame into OpenGL */
    k = 0;
    attr[k++] = EGL_LINUX_DRM_FOURCC_EXT;
    attr[k++] = desc->layers[0].format;
    attr[k++] = EGL_WIDTH;
    attr[k++] = frames ? frames->width : frame->width;
    attr[k++] = EGL_HEIGHT;
    attr[k++] = frames ? frames->height : frame->height;
    image_index = 0;
    for (i = 0; i < desc->nb_layers; ++i) {
        const AVDRMLayerDescriptor *layer = &desc->layers[i];
        for (j = 0; j < layer->nb_planes; ++j) {
            const AVDRMPlaneDescriptor *plane = &layer->planes[j];
            const AVDRMObjectDescriptor *object = &desc->objects[plane->object_index];

            switch (image_index) {
            case 0:
                attr[k++] = EGL_DMA_BUF_PLANE0_FD_EXT;
                attr[k++] = object->fd;
                attr[k++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
                attr[k++] = plane->offset;
                attr[k++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
                attr[k++] = plane->pitch;
                if (has_EGL_EXT_image_dma_buf_import_modifiers && object->format_modifier != DRM_FORMAT_MOD_INVALID) {
                    attr[k++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
                    attr[k++] = (object->format_modifier & 0xFFFFFFFF);
                    attr[k++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
                    attr[k++] = (object->format_modifier >> 32);
                }
                break;
            case 1:
                attr[k++] = EGL_DMA_BUF_PLANE1_FD_EXT;
                attr[k++] = object->fd;
                attr[k++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
                attr[k++] = plane->offset;
                attr[k++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
                attr[k++] = plane->pitch;
                if (has_EGL_EXT_image_dma_buf_import_modifiers && object->format_modifier != DRM_FORMAT_MOD_INVALID) {
                    attr[k++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
                    attr[k++] = (object->format_modifier & 0xFFFFFFFF);
                    attr[k++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
                    attr[k++] = (object->format_modifier >> 32);
                }
                break;
            case 2:
                attr[k++] = EGL_DMA_BUF_PLANE2_FD_EXT;
                attr[k++] = object->fd;
                attr[k++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
                attr[k++] = plane->offset;
                attr[k++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
                attr[k++] = plane->pitch;
                if (has_EGL_EXT_image_dma_buf_import_modifiers && object->format_modifier != DRM_FORMAT_MOD_INVALID) {
                    attr[k++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
                    attr[k++] = (object->format_modifier & 0xFFFFFFFF);
                    attr[k++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
                    attr[k++] = (object->format_modifier >> 32);
                }
                break;
            case 3:
                attr[k++] = EGL_DMA_BUF_PLANE3_FD_EXT;
                attr[k++] = object->fd;
                attr[k++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
                attr[k++] = plane->offset;
                attr[k++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
                attr[k++] = plane->pitch;
                if (has_EGL_EXT_image_dma_buf_import_modifiers && object->format_modifier != DRM_FORMAT_MOD_INVALID) {
                    attr[k++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
                    attr[k++] = (object->format_modifier & 0xFFFFFFFF);
                    attr[k++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
                    attr[k++] = (object->format_modifier >> 32);
                }
                break;

            default:
                break;
            }
            ++image_index;
        }
    }

    switch (SDL_COLORSPACEPRIMARIES(colorspace)) {
    case SDL_COLOR_PRIMARIES_BT601:
    case SDL_COLOR_PRIMARIES_SMPTE240:
        attr[k++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attr[k++] = EGL_ITU_REC601_EXT;
        break;
    case SDL_COLOR_PRIMARIES_BT709:
        attr[k++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attr[k++] = EGL_ITU_REC709_EXT;
        break;
    case SDL_COLOR_PRIMARIES_BT2020:
        attr[k++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
        attr[k++] = EGL_ITU_REC2020_EXT;
        break;
    default:
        break;
    }

    switch (SDL_COLORSPACERANGE(colorspace)) {
    case SDL_COLOR_RANGE_FULL:
        attr[k++] = EGL_SAMPLE_RANGE_HINT_EXT;
        attr[k++] = EGL_YUV_FULL_RANGE_EXT;
        break;
    case SDL_COLOR_RANGE_LIMITED:
    default:
        attr[k++] = EGL_SAMPLE_RANGE_HINT_EXT;
        attr[k++] = EGL_YUV_NARROW_RANGE_EXT;
        break;
    }

    switch (SDL_COLORSPACECHROMA(colorspace)) {
    case SDL_CHROMA_LOCATION_LEFT:
        attr[k++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
        attr[k++] = EGL_YUV_CHROMA_SITING_0_EXT;
        attr[k++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
        attr[k++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
        break;
    case SDL_CHROMA_LOCATION_CENTER:
        attr[k++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
        attr[k++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
        attr[k++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
        attr[k++] = EGL_YUV_CHROMA_SITING_0_5_EXT;
        break;
    case SDL_CHROMA_LOCATION_TOPLEFT:
        attr[k++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
        attr[k++] = EGL_YUV_CHROMA_SITING_0_EXT;
        attr[k++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
        attr[k++] = EGL_YUV_CHROMA_SITING_0_EXT;
        break;
    default:
        break;
    }

    SDL_assert(k < SDL_arraysize(attr));
    attr[k++] = EGL_NONE;

    EGLImage image = eglCreateImage(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attr);
    if (image == EGL_NO_IMAGE) {
        SDL_Log("Couldn't create image: %d\n", glGetError());
        return false;
    }

    glActiveTextureARBFunc(GL_TEXTURE0_ARB);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureID);
    glEGLImageTargetTexture2DOESFunc(GL_TEXTURE_EXTERNAL_OES, image);
    return true;
}
#endif // HAVE_EGL

static bool GetTextureForDRMFrame(AVFrame *frame, SDL_Texture **texture)
{
#ifdef HAVE_EGL
    const AVDRMFrameDescriptor *desc = (const AVDRMFrameDescriptor *)frame->data[0];

    if (desc->nb_layers == 2 &&
        desc->layers[0].format == DRM_FORMAT_R8 &&
        desc->layers[1].format == DRM_FORMAT_GR88) {
        return GetNV12TextureForDRMFrame(frame, texture);
    } else {
        return GetOESTextureForDRMFrame(frame, texture);
    }
#else
    return false;
#endif
}

static bool GetTextureForVAAPIFrame(AVFrame *frame, SDL_Texture **texture)
{
    AVFrame *drm_frame;
    bool result = false;

    drm_frame = av_frame_alloc();
    if (drm_frame) {
        drm_frame->format = AV_PIX_FMT_DRM_PRIME;
        if (av_hwframe_map(drm_frame, frame, 0) == 0) {
            result = GetTextureForDRMFrame(drm_frame, texture);
        } else {
            SDL_SetError("Couldn't map hardware frame");
        }
        av_frame_free(&drm_frame);
    }
    return result;
}

static bool GetTextureForD3D11Frame(AVFrame *frame, SDL_Texture **texture)
{
#ifdef SDL_PLATFORM_WIN32
    AVHWFramesContext *frames = (AVHWFramesContext *)(frame->hw_frames_ctx->data);
    int texture_width = 0, texture_height = 0;
    ID3D11Texture2D *pTexture = (ID3D11Texture2D *)frame->data[0];
    UINT iSliceIndex = (UINT)(uintptr_t)frame->data[1];

    if (*texture) {
        SDL_PropertiesID props = SDL_GetTextureProperties(*texture);
        texture_width = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
        texture_height = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);
    }
    if (!*texture || texture_width != frames->width || texture_height != frames->height) {
        if (*texture) {
            SDL_DestroyTexture(*texture);
        }

        SDL_PropertiesID props = CreateVideoTextureProperties(frame, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STATIC);
        *texture = SDL_CreateTextureWithProperties(renderer, props);
        SDL_DestroyProperties(props);
        if (!*texture) {
            return false;
        }
    }

    ID3D11Resource *dx11_resource = SDL_GetPointerProperty(SDL_GetTextureProperties(*texture), SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, NULL);
    if (!dx11_resource) {
        SDL_SetError("Couldn't get texture ID3D11Resource interface");
        return false;
    }
    ID3D11DeviceContext_CopySubresourceRegion(d3d11_context, dx11_resource, 0, 0, 0, 0, (ID3D11Resource *)pTexture, iSliceIndex, NULL);

    return true;
#else
    return false;
#endif
}

static bool GetTextureForVideoToolboxFrame(AVFrame *frame, SDL_Texture **texture)
{
#ifdef SDL_PLATFORM_APPLE
    CVPixelBufferRef pPixelBuffer = (CVPixelBufferRef)frame->data[3];
    SDL_PropertiesID props;

    if (*texture) {
        /* Free the previous texture now that we're about to render a new one */
        /* FIXME: We can actually keep a cache of textures that map to pixel buffers */
        SDL_DestroyTexture(*texture);
    }

    props = CreateVideoTextureProperties(frame, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STATIC);
    SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER, pPixelBuffer);
    *texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (!*texture) {
        return false;
    }

    return true;
#else
    return false;
#endif
}

static bool GetTextureForVulkanFrame(AVFrame *frame, SDL_Texture **texture)
{
    SDL_PropertiesID props;

    if (*texture) {
        SDL_DestroyTexture(*texture);
    }

    props = CreateVideoTextureProperties(frame, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STATIC);
    *texture = CreateVulkanVideoTexture(vulkan_context, frame, renderer, props);
    SDL_DestroyProperties(props);
    if (!*texture) {
        return false;
    }
    return true;
}

static bool GetTextureForFrame(AVFrame *frame, SDL_Texture **texture)
{
    switch (frame->format) {
    case AV_PIX_FMT_VAAPI:
        return GetTextureForVAAPIFrame(frame, texture);
    case AV_PIX_FMT_DRM_PRIME:
        return GetTextureForDRMFrame(frame, texture);
    case AV_PIX_FMT_D3D11:
        return GetTextureForD3D11Frame(frame, texture);
    case AV_PIX_FMT_VIDEOTOOLBOX:
        return GetTextureForVideoToolboxFrame(frame, texture);
    case AV_PIX_FMT_VULKAN:
        return GetTextureForVulkanFrame(frame, texture);
    default:
        return GetTextureForMemoryFrame(frame, texture);
    }
}

static int BeginFrameRendering(AVFrame *frame)
{
    if (frame->format == AV_PIX_FMT_VULKAN) {
        return BeginVulkanFrameRendering(vulkan_context, frame, renderer);
    }
    return 0;
}

static int FinishFrameRendering(AVFrame *frame)
{
    if (frame->format == AV_PIX_FMT_VULKAN) {
        return FinishVulkanFrameRendering(vulkan_context, frame, renderer);
    }
    return 0;
}

static void DisplayVideoTexture(AVFrame *frame)
{
    /* Update the video texture */
    if (!GetTextureForFrame(frame, &video_texture)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get texture for frame: %s\n", SDL_GetError());
        return;
    }

    SDL_FRect src;
    src.x = 0.0f;
    src.y = 0.0f;
    src.w = (float)frame->width;
    src.h = (float)frame->height;
    if (frame->linesize[0] < 0) {
        SDL_RenderTextureRotated(renderer, video_texture, &src, NULL, 0.0, NULL, SDL_FLIP_VERTICAL);
    } else {
        SDL_RenderTexture(renderer, video_texture, &src, NULL);
    }
}

static void DisplayVideoFrame(AVFrame *frame)
{
    DisplayVideoTexture(frame);
}

static void HandleVideoFrame(AVFrame *frame, double pts)
{
    /* Quick and dirty PTS handling */
    if (!video_start) {
        video_start = SDL_GetTicks();
    }
    double now = (double)(SDL_GetTicks() - video_start) / 1000.0;
    if (now < pts) {
        SDL_DelayPrecise((Uint64)((pts - now) * SDL_NS_PER_SECOND));
    }

    if (BeginFrameRendering(frame) < 0) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    DisplayVideoFrame(frame);

    /* Render any bouncing balls */
    MoveSprite();

    SDL_RenderPresent(renderer);

    FinishFrameRendering(frame);
}

static AVCodecContext *OpenAudioStream(AVFormatContext *ic, int stream, const AVCodec *codec)
{
    AVStream *st = ic->streams[stream];
    AVCodecParameters *codecpar = st->codecpar;
    AVCodecContext *context;
    int result;

    SDL_Log("Audio stream: %s %d channels, %d Hz\n", avcodec_get_name(codec->id), codecpar->ch_layout.nb_channels, codecpar->sample_rate);

    context = avcodec_alloc_context3(NULL);
    if (!context) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_alloc_context3 failed\n");
        return NULL;
    }

    result = avcodec_parameters_to_context(context, ic->streams[stream]->codecpar);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_parameters_to_context failed: %s\n", av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }
    context->pkt_timebase = ic->streams[stream]->time_base;

    result = avcodec_open2(context, codec, NULL);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open codec %s: %s", avcodec_get_name(context->codec_id), av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }

    SDL_AudioSpec spec = { SDL_AUDIO_F32, codecpar->ch_layout.nb_channels, codecpar->sample_rate };
    audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (audio) {
        SDL_ResumeAudioStreamDevice(audio);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s", SDL_GetError());
    }
    return context;
}

static SDL_AudioFormat GetAudioFormat(int format)
{
    switch (format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        return SDL_AUDIO_U8;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        return SDL_AUDIO_S16;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
        return SDL_AUDIO_S32;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        return SDL_AUDIO_F32;
    default:
        /* Unsupported */
        return SDL_AUDIO_UNKNOWN;
    }
}

static bool IsPlanarAudioFormat(int format)
{
    switch (format) {
    case AV_SAMPLE_FMT_U8P:
    case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64P:
        return true;
    default:
        return false;
    }
}

static void InterleaveAudio(AVFrame *frame, const SDL_AudioSpec *spec)
{
    int c, n;
    int samplesize = SDL_AUDIO_BYTESIZE(spec->format);
    int framesize = SDL_AUDIO_FRAMESIZE(*spec);
    Uint8 *data = (Uint8 *)SDL_malloc(frame->nb_samples * framesize);
    if (!data) {
        return;
    }

    /* This could be optimized with SIMD and not allocating memory each time */
    for (c = 0; c < spec->channels; ++c) {
        const Uint8 *src = frame->data[c];
        Uint8 *dst = data + c * samplesize;
        for (n = frame->nb_samples; n--;) {
            SDL_memcpy(dst, src, samplesize);
            src += samplesize;
            dst += framesize;
        }
    }
    SDL_PutAudioStreamData(audio, data, frame->nb_samples * framesize);
    SDL_free(data);
}

static void HandleAudioFrame(AVFrame *frame)
{
    if (audio) {
        SDL_AudioSpec spec = { GetAudioFormat(frame->format), frame->ch_layout.nb_channels, frame->sample_rate };
        SDL_SetAudioStreamFormat(audio, &spec, NULL);

        if (frame->ch_layout.nb_channels > 1 && IsPlanarAudioFormat(frame->format)) {
            InterleaveAudio(frame, &spec);
        } else {
            SDL_PutAudioStreamData(audio, frame->data[0], frame->nb_samples * SDL_AUDIO_FRAMESIZE(spec));
        }
    }
}

static void av_log_callback(void *avcl, int level, const char *fmt, va_list vl)
{
    const char *pszCategory = NULL;
    char *message;

    switch (level) {
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
        pszCategory = "fatal error";
        break;
    case AV_LOG_ERROR:
        pszCategory = "error";
        break;
    case AV_LOG_WARNING:
        pszCategory = "warning";
        break;
    case AV_LOG_INFO:
        pszCategory = "info";
        break;
    case AV_LOG_VERBOSE:
        pszCategory = "verbose";
        break;
    case AV_LOG_DEBUG:
        if (verbose) {
            pszCategory = "debug";
        }
        break;
    }

    if (!pszCategory) {
        // We don't care about this message
        return;
    }

    SDL_vasprintf(&message, fmt, vl);
    SDL_Log("ffmpeg %s: %s", pszCategory, message);
    SDL_free(message);
}

static void print_usage(SDLTest_CommonState *state, const char *argv0)
{
    static const char *options[] = { "[--verbose]", "[--sprites N]", "[--audio-codec codec]", "[--video-codec codec]", "[--software]", "video_file", NULL };
    SDLTest_CommonLogUsage(state, argv0, options);
}

int main(int argc, char *argv[])
{
    const char *file = NULL;
    AVFormatContext *ic = NULL;
    int audio_stream = -1;
    int video_stream = -1;
    const char *audio_codec_name = NULL;
    const char *video_codec_name = NULL;
    const AVCodec *audio_codec = NULL;
    const AVCodec *video_codec = NULL;
    AVCodecContext *audio_context = NULL;
    AVCodecContext *video_context = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    double first_pts = -1.0;
    int i;
    int result;
    int return_code = -1;
    SDL_WindowFlags window_flags;
    bool flushing = false;
    bool decoded = false;
    bool done = false;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Log ffmpeg messages */
    av_log_set_callback(av_log_callback);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--verbose") == 0) {
                verbose = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--sprites") == 0 && argv[i + 1]) {
                num_sprites = SDL_atoi(argv[i + 1]);
                consumed = 2;
            } else if (SDL_strcmp(argv[i], "--audio-codec") == 0 && argv[i + 1]) {
                audio_codec_name = argv[i + 1];
                consumed = 2;
            } else if (SDL_strcmp(argv[i], "--video-codec") == 0 && argv[i + 1]) {
                video_codec_name = argv[i + 1];
                consumed = 2;
            } else if (SDL_strcmp(argv[i], "--software") == 0) {
                software_only = true;
                consumed = 1;
            } else if (!file) {
                /* We'll try to open this as a media file */
                file = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            print_usage(state, argv[0]);
            return_code = 1;
            goto quit;
        }

        i += consumed;
    }

    if (!file) {
        print_usage(state, argv[0]);
        return_code = 1;
        goto quit;
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        return_code = 2;
        goto quit;
    }

    window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef SDL_PLATFORM_APPLE
    window_flags |= SDL_WINDOW_METAL;
#elif !defined(SDL_PLATFORM_WIN32)
    window_flags |= SDL_WINDOW_OPENGL;
#endif
    if (SDL_GetHint(SDL_HINT_RENDER_DRIVER) != NULL) {
        CreateWindowAndRenderer(window_flags, SDL_GetHint(SDL_HINT_RENDER_DRIVER));
    }
#ifdef HAVE_EGL
    /* Try to create an EGL compatible window for DRM hardware frame support */
    if (!window) {
        CreateWindowAndRenderer(window_flags, "opengles2");
    }
#endif
#ifdef SDL_PLATFORM_APPLE
    if (!window) {
        CreateWindowAndRenderer(window_flags, "metal");
    }
#endif
#ifdef SDL_PLATFORM_WIN32
    if (!window) {
        CreateWindowAndRenderer(window_flags, "direct3d11");
    }
#endif
    if (!window) {
        if (!CreateWindowAndRenderer(window_flags, NULL)) {
            return_code = 2;
            goto quit;
        }
    }

    if (!SDL_SetWindowTitle(window, file)) {
        SDL_Log("SDL_SetWindowTitle: %s", SDL_GetError());
    }

    /* Open the media file */
    result = avformat_open_input(&ic, file, NULL, NULL);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open %s: %d", argv[1], result);
        return_code = 4;
        goto quit;
    }
    video_stream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (video_stream >= 0) {
        if (video_codec_name) {
            video_codec = avcodec_find_decoder_by_name(video_codec_name);
            if (!video_codec) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find codec '%s'", video_codec_name);
                return_code = 4;
                goto quit;
            }
        }
        video_context = OpenVideoStream(ic, video_stream, video_codec);
        if (!video_context) {
            return_code = 4;
            goto quit;
        }
    }
    audio_stream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, video_stream, &audio_codec, 0);
    if (audio_stream >= 0) {
        if (audio_codec_name) {
            audio_codec = avcodec_find_decoder_by_name(audio_codec_name);
            if (!audio_codec) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find codec '%s'", audio_codec_name);
                return_code = 4;
                goto quit;
            }
        }
        audio_context = OpenAudioStream(ic, audio_stream, audio_codec);
        if (!audio_context) {
            return_code = 4;
            goto quit;
        }
    }
    pkt = av_packet_alloc();
    if (!pkt) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "av_packet_alloc failed");
        return_code = 4;
        goto quit;
    }
    frame = av_frame_alloc();
    if (!frame) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "av_frame_alloc failed");
        return_code = 4;
        goto quit;
    }

    /* Create the sprite */
    sprite = CreateTexture(renderer, icon_bmp, icon_bmp_len, &sprite_w, &sprite_h);

    if (!sprite) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture (%s)", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    /* Allocate memory for the sprite info */
    positions = (SDL_FRect *)SDL_malloc(num_sprites * sizeof(*positions));
    velocities = (SDL_FRect *)SDL_malloc(num_sprites * sizeof(*velocities));
    if (!positions || !velocities) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!\n");
        return_code = 3;
        goto quit;
    }

    /* Position sprites and set their velocities */
    SDL_Rect viewport;
    SDL_GetRenderViewport(renderer, &viewport);
    for (i = 0; i < num_sprites; ++i) {
        positions[i].x = (float)SDL_rand(viewport.w - sprite_w);
        positions[i].y = (float)SDL_rand(viewport.h - sprite_h);
        positions[i].w = (float)sprite_w;
        positions[i].h = (float)sprite_h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (velocities[i].x == 0.f || velocities[i].y == 0.f) {
            velocities[i].x = (float)(SDL_rand(2 + 1) - 1);
            velocities[i].y = (float)(SDL_rand(2 + 1) - 1);
        }
    }

    /* We're ready to go! */
    SDL_ShowWindow(window);

    /* Main render loop */
    while (!done) {
        SDL_Event event;

        /* Check for events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                done = true;
            }
        }

        if (!flushing) {
            result = av_read_frame(ic, pkt);
            if (result < 0) {
                SDL_Log("End of stream, finishing decode\n");
                if (audio_context) {
                    avcodec_flush_buffers(audio_context);
                }
                if (video_context) {
                    avcodec_flush_buffers(video_context);
                }
                flushing = true;
            } else {
                if (pkt->stream_index == audio_stream) {
                    result = avcodec_send_packet(audio_context, pkt);
                    if (result < 0) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_send_packet(audio_context) failed: %s", av_err2str(result));
                    }
                } else if (pkt->stream_index == video_stream) {
                    result = avcodec_send_packet(video_context, pkt);
                    if (result < 0) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_send_packet(video_context) failed: %s", av_err2str(result));
                    }
                }
                av_packet_unref(pkt);
            }
        }

        decoded = false;
        if (audio_context) {
            while (avcodec_receive_frame(audio_context, frame) >= 0) {
                HandleAudioFrame(frame);
                decoded = true;
            }
            if (flushing) {
                /* Let SDL know we're done sending audio */
                SDL_FlushAudioStream(audio);
            }
        }
        if (video_context) {
            while (avcodec_receive_frame(video_context, frame) >= 0) {
                double pts = ((double)frame->pts * video_context->pkt_timebase.num) / video_context->pkt_timebase.den;
                if (first_pts < 0.0) {
                    first_pts = pts;
                }
                pts -= first_pts;

                HandleVideoFrame(frame, pts);
                decoded = true;
            }
        } else {
            /* Update video rendering */
            SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
            SDL_RenderClear(renderer);
            MoveSprite();
            SDL_RenderPresent(renderer);
        }

        if (flushing && !decoded) {
            if (SDL_GetAudioStreamQueued(audio) > 0) {
                /* Wait a little bit for the audio to finish */
                SDL_Delay(10);
            } else {
                done = true;
            }
        }
    }
    return_code = 0;
quit:
#ifdef SDL_PLATFORM_WIN32
    if (d3d11_context) {
        ID3D11DeviceContext_Release(d3d11_context);
        d3d11_context = NULL;
    }
    if (d3d11_device) {
        ID3D11Device_Release(d3d11_device);
        d3d11_device = NULL;
    }
#endif
    SDL_free(positions);
    SDL_free(velocities);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&audio_context);
    avcodec_free_context(&video_context);
    avformat_close_input(&ic);
    SDL_DestroyRenderer(renderer);
    if (vulkan_context) {
        DestroyVulkanVideoContext(vulkan_context);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return return_code;
}
