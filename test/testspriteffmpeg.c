/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>

#include "icon.h"

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

static SDL_Texture *sprite;
static SDL_FRect position;
static SDL_FRect velocity;
static int sprite_w, sprite_h;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_AudioStream *audio;
static SDL_Texture *video_texture;
static SDL_PixelFormatEnum video_format;
static int video_width;
static int video_height;
static Uint64 video_start;
static struct SwsContext *video_conversion_context;
static int done;

static SDL_Texture *CreateTexture(SDL_Renderer *r, unsigned char *data, unsigned int len, int *w, int *h) {
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_RWops *src = SDL_RWFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_RW(src, SDL_TRUE);
        if (surface) {
            /* Treat white as transparent */
            SDL_SetSurfaceColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 255, 255, 255));

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
    int max_w, max_h;

    SDL_GetCurrentRenderOutputSize(renderer, &max_w, &max_h);

    /* Move the sprite, bounce at the wall, and draw */
    position.x += velocity.x;
    if ((position.x < 0) || (position.x >= (max_w - sprite_w))) {
        velocity.x = -velocity.x;
        position.x += velocity.x;
    }
    position.y += velocity.y;
    if ((position.y < 0) || (position.y >= (max_h - sprite_h))) {
        velocity.y = -velocity.y;
        position.y += velocity.y;
    }

    /* Blit the sprite onto the screen */
    SDL_RenderTexture(renderer, sprite, NULL, &position);
}

static AVCodecContext *OpenStream(AVFormatContext *ic, int stream)
{
    AVCodecContext *context;
    const AVCodec *codec;
    int result;

    context = avcodec_alloc_context3(NULL);
    if (!context) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_alloc_context3 failed");
        return NULL;
    }

    if (avcodec_parameters_to_context(context, ic->streams[stream]->codecpar) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "avcodec_parameters_to_context failed");
        avcodec_free_context(&context);
        return NULL;
    }
    context->pkt_timebase = ic->streams[stream]->time_base;

    codec = avcodec_find_decoder(context->codec_id);
    if (!codec) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find codec %s", avcodec_get_name(context->codec_id));
        avcodec_free_context(&context);
        return NULL;
    }

    context->codec_id = codec->id;
    result = avcodec_open2(context, codec, NULL);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open codec %s: %s", avcodec_get_name(context->codec_id), av_err2str(result));
        avcodec_free_context(&context);
        return NULL;
    }

    return context;
}

static AVCodecContext *OpenAudioStream(AVFormatContext *ic, int stream)
{
    AVStream *st = ic->streams[stream];
    AVCodecParameters *codecpar = st->codecpar;
    AVCodecContext *context = OpenStream(ic, stream);

    if (context) {
        SDL_Log("Audio stream: %s %d channels, %d Hz\n", avcodec_get_name(context->codec_id), codecpar->ch_layout.nb_channels, codecpar->sample_rate);

        SDL_AudioSpec spec = { SDL_AUDIO_F32, codecpar->ch_layout.nb_channels, codecpar->sample_rate };
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &spec, NULL, NULL);
        if (audio) {
            SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio));
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s", SDL_GetError());
        }
    }
    return context;
}

static SDL_AudioFormat GetAudioFormat(enum AVSampleFormat format)
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
        return 0;
    }
}

static SDL_bool IsPlanarAudioFormat(enum AVSampleFormat format)
{
    switch (format) {
    case AV_SAMPLE_FMT_U8P:
    case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64P:
        return SDL_TRUE;
    default:
        return SDL_FALSE;
    }
}

static void InterleaveAudio(AVFrame *frame, const SDL_AudioSpec *spec)
{
    int samplesize = SDL_AUDIO_BYTESIZE(spec->format);
    int framesize = SDL_AUDIO_FRAMESIZE(*spec);
    Uint8 *data = (Uint8 *)SDL_malloc(frame->nb_samples * framesize);
    if (!data) {
        return;
    }

    /* This could be optimized with SIMD and not allocating memory each time */
    for (int c = 0; c < spec->channels; ++c) {
        const Uint8 *src = frame->data[c];
        Uint8 *dst = data + c * samplesize;
        for (int n = frame->nb_samples; n--; ) {
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

static AVCodecContext *OpenVideoStream(AVFormatContext *ic, int stream)
{
    AVStream *st = ic->streams[stream];
    AVCodecParameters *codecpar = st->codecpar;
    AVCodecContext *context = OpenStream(ic, stream);

    if (context) {
        SDL_Log("Video stream: %s %dx%d\n", avcodec_get_name(context->codec_id), codecpar->width, codecpar->height);
        SDL_SetWindowSize(window, codecpar->width, codecpar->height);
    }
    return context;
}

static SDL_PixelFormatEnum GetVideoFormat(enum AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_RGB8:
        return SDL_PIXELFORMAT_RGB332;
    case AV_PIX_FMT_RGB444:
        return SDL_PIXELFORMAT_RGB444;
    case AV_PIX_FMT_RGB555:
        return SDL_PIXELFORMAT_RGB555;
    case AV_PIX_FMT_BGR555:
        return SDL_PIXELFORMAT_BGR555;
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
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static void SetYUVConversionMode(AVFrame *frame)
{
    SDL_YUV_CONVERSION_MODE mode = SDL_YUV_CONVERSION_AUTOMATIC;
    if (frame && (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUYV422 || frame->format == AV_PIX_FMT_UYVY422)) {
        if (frame->color_range == AVCOL_RANGE_JPEG)
            mode = SDL_YUV_CONVERSION_JPEG;
        else if (frame->colorspace == AVCOL_SPC_BT709)
            mode = SDL_YUV_CONVERSION_BT709;
        else if (frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M)
            mode = SDL_YUV_CONVERSION_BT601;
    }
    SDL_SetYUVConversionMode(mode); /* FIXME: no support for linear transfer */
}

static void HandleVideoFrame(AVFrame *frame, double pts)
{
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    /* Update the video texture */
    SDL_PixelFormatEnum format = GetVideoFormat(frame->format);
    if (!video_texture || format != video_format || frame->width != video_width || frame->height != video_height) {
        if (video_texture) {
            SDL_DestroyTexture(video_texture);
        }

        if (format == SDL_PIXELFORMAT_UNKNOWN) {
            video_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
        } else {
            video_texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
        }
        video_format = format;
        video_width = frame->width;
        video_height = frame->height;
    }

    switch (format) {
    case SDL_PIXELFORMAT_UNKNOWN:
        video_conversion_context = sws_getCachedContext(video_conversion_context,
            frame->width, frame->height, frame->format, frame->width, frame->height,
            AV_PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
        if (video_conversion_context != NULL) {
            uint8_t *pixels[4];
            int pitch[4];
            if (SDL_LockTexture(video_texture, NULL, (void **)pixels, pitch) == 0) {
                sws_scale(video_conversion_context, (const uint8_t * const *)frame->data, frame->linesize,
                          0, frame->height, pixels, pitch);
                SDL_UnlockTexture(video_texture);
            }
        }
        break;
    case SDL_PIXELFORMAT_IYUV:
        if (frame->linesize[0] > 0 && frame->linesize[1] > 0 && frame->linesize[2] > 0) {
            SDL_UpdateYUVTexture(video_texture, NULL, frame->data[0], frame->linesize[0],
                                                   frame->data[1], frame->linesize[1],
                                                   frame->data[2], frame->linesize[2]);
        } else if (frame->linesize[0] < 0 && frame->linesize[1] < 0 && frame->linesize[2] < 0) {
            SDL_UpdateYUVTexture(video_texture, NULL, frame->data[0] + frame->linesize[0] * (frame->height                    - 1), -frame->linesize[0],
                                                   frame->data[1] + frame->linesize[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[1],
                                                   frame->data[2] + frame->linesize[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[2]);
            flip = SDL_FLIP_VERTICAL;
        }
        SetYUVConversionMode(frame);
        break;
    default:
        if (frame->linesize[0] < 0) {
            SDL_UpdateTexture(video_texture, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
            flip = SDL_FLIP_VERTICAL;
        } else {
            SDL_UpdateTexture(video_texture, NULL, frame->data[0], frame->linesize[0]);
        }
        break;
    }

    /* Quick and dirty PTS handling */
    if (!video_start) {
        video_start = SDL_GetTicks();
    }
    double now = (double)(SDL_GetTicks() - video_start) / 1000.0;
    while (now < pts - 0.001) {
        SDL_Delay(1);
        now = (double)(SDL_GetTicks() - video_start) / 1000.0;
    }

    SDL_RenderTextureRotated(renderer, video_texture, NULL, NULL, 0.0, NULL, flip);
    MoveSprite();
    SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[])
{
    AVFormatContext *ic = NULL;
    int audio_stream = -1;
    int video_stream = -1;
    AVCodecContext *audio_context = NULL;
    AVCodecContext *video_context = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    double first_pts = -1.0;
    int result;
    int return_code = -1;
    SDL_bool flushing = SDL_FALSE;
    SDL_bool decoded = SDL_FALSE;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc != 2) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s video_file\n", argv[0]);
        return_code = 1;
        goto quit;
    }

    if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0) {
        return_code = 2;
        goto quit;
    }

    if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer) < 0) {
        return_code = 2;
        goto quit;
    }

    if (SDL_SetWindowTitle(window, argv[1]) < 0) {
        SDL_Log("SDL_SetWindowTitle: %s", SDL_GetError());
    }

    /* Open the media file */
    result = avformat_open_input(&ic, argv[1], NULL, NULL);
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open %s: %d", argv[1], result);
        return_code = 4;
        goto quit;
    }
    video_stream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream >= 0) {
        video_context = OpenVideoStream(ic, video_stream);
        if (!video_context) {
            return_code = 4;
            goto quit;
        }
    }
    audio_stream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, video_stream, NULL, 0);
    if (audio_stream >= 0) {
        audio_context = OpenAudioStream(ic, audio_stream);
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

    if (sprite == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture (%s)", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    /* Initialize the sprite position */
    int max_w, max_h;
    SDL_GetCurrentRenderOutputSize(renderer, &max_w, &max_h);
    srand((unsigned int)time(NULL));
    position.x = (float)(rand() % (max_w - sprite_w));
    position.y = (float)(rand() % (max_h - sprite_h));
    position.w = (float)sprite_w;
    position.h = (float)sprite_h;
    velocity.x = 0.0f;
    velocity.y = 0.0f;
    while (!velocity.x || !velocity.y) {
        velocity.x = (float)((rand() % (2 + 1)) - 1);
        velocity.y = (float)((rand() % (2 + 1)) - 1);
    }

    /* Main render loop */
    done = 0;

    while (!done) {
        SDL_Event event;

        /* Check for events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_KEY_DOWN) {
                done = 1;
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
                flushing = SDL_TRUE;
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

        decoded = SDL_FALSE;
        if (audio_context) {
            while (avcodec_receive_frame(audio_context, frame) >= 0) {
                HandleAudioFrame(frame);
                decoded = SDL_TRUE;
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
                decoded = SDL_TRUE;
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
                done = 1;
            }
        }
    }
    return_code = 0;
quit:
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&audio_context);
    avcodec_free_context(&video_context);
    avformat_close_input(&ic);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return return_code;
}
