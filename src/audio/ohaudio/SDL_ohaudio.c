/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

// OpenHarmony OHAudio driver.
// This code is based on the Android AAudio backend, since the two APIs are almost identical.

#ifdef SDL_AUDIO_DRIVER_OHAUDIO

#include "../SDL_sysaudio.h"
#include "SDL_ohaudio.h"

#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>

struct SDL_PrivateAudioData
{
    union {
        OH_AudioRenderer *renderer;
        OH_AudioCapturer *capturer;
    } stream;

    int num_buffers;
    Uint8 *mixbuf;          // Raw mixing buffer
    size_t mixbuf_bytes;    // num_buffers * device->buffer_size
    size_t callback_bytes;
    size_t processed_bytes;
    SDL_Semaphore *semaphore;
    SDL_AtomicInt error_callback_triggered;
};

// !!! FIXME: Remove this LOGI macro.
// Debug
#if 0
#define LOGI(...) SDL_Log(__VA_ARGS__);
#else
#define LOGI(...)
#endif

#define LIB_OHAUDIO_SO "libohaudio.so"

SDL_ELF_NOTE_DLOPEN(
    "audio-ohaudio",
    "Support for audio through OHAudio",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    LIB_OHAUDIO_SO
)

typedef struct OHAUDIO_Data
{
    SDL_SharedObject *handle;
#define SDL_PROC(ret, func, params) ret (*func) params;
#include "SDL_ohaudiofuncs.h"
} OHAUDIO_Data;
static OHAUDIO_Data ctx;

static bool OHAUDIO_LoadFunctions(OHAUDIO_Data *data)
{
#define SDL_PROC(ret, func, params)                                                             \
    do {                                                                                        \
        data->func = (ret (*) params)SDL_LoadFunction(data->handle, #func);                     \
        if (!data->func) {                                                                      \
            return SDL_SetError("Couldn't load OHAudio function %s: %s", #func, SDL_GetError()); \
        }                                                                                       \
    } while (0);

#define SDL_PROC_OPTIONAL(ret, func, params)                                                          \
    do {                                                                                              \
        data->func = (ret (*) params)SDL_LoadFunction(data->handle, #func);  /* if it fails, okay. */ \
    } while (0);
#include "SDL_ohaudiofuncs.h"
    return true;
}


static const char *AudioStreamResultToString(OH_AudioStream_Result res)
{
    switch (res) {
        #define ERRCASE(nam) case AUDIOSTREAM_##nam: return #nam
        ERRCASE(SUCCESS);
        ERRCASE(ERROR_INVALID_PARAM);
        ERRCASE(ERROR_ILLEGAL_STATE);
        ERRCASE(ERROR_SYSTEM);
        ERRCASE(ERROR_UNSUPPORTED_FORMAT);
        #undef ERRCASE
        default: break;
    }
    return "[unknown]";
}

static void ErrorCallback(void *userData, OH_AudioStream_Result error)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *) userData;
    LOGI("SDL ErrorCallback%s: %d - %s", device->recording ? "Capturer" : "Renderer", error, AudioStreamResultToString(error));
    // You MUST NOT close the audio stream from this callback, so we cannot call SDL_AudioDeviceDisconnected here.
    // Just flag the device so we can kill it in PlayDevice instead.
    SDL_SetAtomicInt(&device->hidden->error_callback_triggered, (int) error);  // AUDIOSTREAM_SUCCESS is zero, so !triggered means no error.
    SDL_SignalSemaphore(device->hidden->semaphore);  // in case we're blocking in WaitDevice.
}

static void ErrorCallbackCapturer(OH_AudioCapturer *capturer, void *userData, OH_AudioStream_Result error)
{
    ErrorCallback(userData, error);
}

static void ErrorCallbackRenderer(OH_AudioRenderer *renderer, void *userData, OH_AudioStream_Result error)
{
    ErrorCallback(userData, error);
}

static void ReadDataCallback(OH_AudioCapturer *capturer, void *userData, void *audioData, int32_t numBytes)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *) userData;
    struct SDL_PrivateAudioData *hidden = device->hidden;
    const size_t framesize = SDL_AUDIO_FRAMESIZE(device->spec);
    const size_t callback_bytes = (size_t) numBytes;
    size_t old_buffer_index = hidden->callback_bytes / device->buffer_size;
    const Uint8 *input = (const Uint8 *)audioData;
    const size_t available_bytes = hidden->mixbuf_bytes - (hidden->callback_bytes - hidden->processed_bytes);
    const size_t size = SDL_min(available_bytes, callback_bytes);
    const size_t offset = hidden->callback_bytes % hidden->mixbuf_bytes;
    const size_t end = (offset + size) % hidden->mixbuf_bytes;

    SDL_assert(size <= hidden->mixbuf_bytes);
    (void) framesize;  // only used in macros that bubble out of release code, etc.

    //LOGI("Recorded %zu frames, %zu available, %zu max (%zu written, %zu read)", callback_bytes / framesize, available_bytes / framesize, hidden->mixbuf_bytes / framesize, hidden->callback_bytes / framesize, hidden->processed_bytes / framesize);

    if (offset <= end) {
        SDL_memcpy(&hidden->mixbuf[offset], input, size);
    } else {
        size_t partial = (hidden->mixbuf_bytes - offset);
        SDL_memcpy(&hidden->mixbuf[offset], &input[0], partial);
        SDL_memcpy(&hidden->mixbuf[0], &input[partial], end);
    }

    SDL_MemoryBarrierRelease();  // the other half of this is in OHAUDIO_RecordDevice().
    hidden->callback_bytes += size;

    if (size < callback_bytes) {
        LOGI("Audio recording overflow, dropped %zu frames", (callback_bytes - size) / framesize);
    }

    // tell the audio thread to pull this data in.
    const size_t new_buffer_index = hidden->callback_bytes / device->buffer_size;
    while (old_buffer_index < new_buffer_index) {
        // Trigger audio processing
        SDL_SignalSemaphore(hidden->semaphore);
        ++old_buffer_index;
    }
}

static OH_AudioData_Callback_Result WriteDataCallback(OH_AudioRenderer *renderer, void *userData, void *audioData, int32_t numBytes)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *) userData;
    struct SDL_PrivateAudioData *hidden = device->hidden;
    const size_t framesize = SDL_AUDIO_FRAMESIZE(device->spec);
    const size_t callback_bytes = (size_t) numBytes;
    size_t old_buffer_index = hidden->callback_bytes / device->buffer_size;
    Uint8 *output = (Uint8 *)audioData;
    const size_t available_bytes = (hidden->processed_bytes - hidden->callback_bytes);
    const size_t size = SDL_min(available_bytes, callback_bytes);
    const size_t offset = hidden->callback_bytes % hidden->mixbuf_bytes;
    const size_t end = (offset + size) % hidden->mixbuf_bytes;

    SDL_assert(size <= hidden->mixbuf_bytes);
    (void) framesize;  // only used in macros that bubble out of release code, etc.

    //LOGI("Playing %zu frames, %zu available, %zu max (%zu written, %zu read)", callback_bytes / framesize, available_bytes / framesize, hidden->mixbuf_bytes / framesize, hidden->processed_bytes / framesize, hidden->callback_bytes / framesize);

    SDL_MemoryBarrierAcquire();  // the other half of this is in OHAUDIO_PlayDevice().
    if (offset <= end) {
        SDL_memcpy(output, &hidden->mixbuf[offset], size);
    } else {
        size_t partial = (hidden->mixbuf_bytes - offset);
        SDL_memcpy(&output[0], &hidden->mixbuf[offset], partial);
        SDL_memcpy(&output[partial], &hidden->mixbuf[0], end);
    }
    hidden->callback_bytes += size;

    if (size < callback_bytes) {
        LOGI("Audio playback underflow, missed %zu frames", (callback_bytes - size) / framesize);
        SDL_memset(&output[size], device->silence_value, (callback_bytes - size));
    }

    // tell the audio thread to generate more data, so we're ready to memcpy it when this callback runs again.
    const size_t new_buffer_index = hidden->callback_bytes / device->buffer_size;
    while (old_buffer_index < new_buffer_index) {
        // Trigger audio processing
        SDL_SignalSemaphore(hidden->semaphore);
        ++old_buffer_index;
    }

    return AUDIO_DATA_CALLBACK_RESULT_VALID;
}

static Uint8 *OHAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *bufsize)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    size_t offset = (hidden->processed_bytes % hidden->mixbuf_bytes);
    return &hidden->mixbuf[offset];
}

static bool OHAUDIO_WaitDevice(SDL_AudioDevice *device)
{
    while (!SDL_GetAtomicInt(&device->shutdown)) {
        // this semaphore won't fire when the app is in the background (OHAUDIO_PauseDevices was called).
        if (SDL_WaitSemaphoreTimeout(device->hidden->semaphore, 100)) {
            return true;  // semaphore was signaled, let's go!
        }
        // Still waiting on the semaphore (or the system), check other things then wait again.
    }
    return true;
}

static bool BuildOHAudioStream(SDL_AudioDevice *device);

static bool RecoverOHAudioDevice(SDL_AudioDevice *device)
{
    SDL_assert(!device->recording);  // !!! FIXME: we (currently) only do this for playback devices. This is true in AAudio, too.

    // attempt to build a new stream, in case there's a new default device.
    struct SDL_PrivateAudioData *hidden = device->hidden;
    ctx.OH_AudioRenderer_Stop(hidden->stream.renderer);
    ctx.OH_AudioRenderer_Release(hidden->stream.renderer);
    hidden->stream.renderer = NULL;

    SDL_aligned_free(hidden->mixbuf);
    hidden->mixbuf = NULL;

    SDL_DestroySemaphore(hidden->semaphore);
    hidden->semaphore = NULL;

    const int prev_sample_frames = device->sample_frames;
    SDL_AudioSpec prevspec;
    SDL_copyp(&prevspec, &device->spec);

    if (!BuildOHAudioStream(device)) {
        return false;  // oh well, we tried.
    }

    // we don't know the new device spec until we open the new device, so we saved off the old one and force it back
    // so SDL_AudioDeviceFormatChanged can set up all the important state if necessary and then set it back to the new spec.
    const int new_sample_frames = device->sample_frames;
    SDL_AudioSpec newspec;
    SDL_copyp(&newspec, &device->spec);

    device->sample_frames = prev_sample_frames;
    SDL_copyp(&device->spec, &prevspec);
    if (!SDL_AudioDeviceFormatChangedAlreadyLocked(device, &newspec, new_sample_frames)) {
        return false;  // ugh
    }
    return true;
}

static bool OHAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;

    // WriteDataCallback picks up our work and unblocks OHAUDIO_WaitDevice. But make sure we didn't fail here.
    const OH_AudioStream_Result err = (OH_AudioStream_Result) SDL_GetAtomicInt(&hidden->error_callback_triggered);
    if (err) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ohaudio: Audio device triggered error %d (%s)", (int) err, AudioStreamResultToString(err));
        if (!RecoverOHAudioDevice(device)) {
            return false;  // oh well, we went down hard.
        }
    } else {
        SDL_MemoryBarrierRelease();
        hidden->processed_bytes += buflen;
    }
    return true;
}

static int OHAUDIO_RecordDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;

    // ReadDataCallback picks up our work and unblocks OHAUDIO_WaitDevice. But make sure we didn't fail here.
    if (SDL_GetAtomicInt(&hidden->error_callback_triggered)) {
        SDL_SetAtomicInt(&hidden->error_callback_triggered, 0);
        return -1;
    }

    SDL_assert(buflen == device->buffer_size);  // If this isn't true, we need to change semaphore trigger logic and account for wrapping copies here
    size_t offset = (hidden->processed_bytes % hidden->mixbuf_bytes);
    SDL_MemoryBarrierAcquire();
    SDL_memcpy(buffer, &hidden->mixbuf[offset], buflen);
    hidden->processed_bytes += buflen;
    return buflen;
}

static void OHAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;

    if (hidden) {
        if ((device->recording) && (hidden->stream.capturer)) {
            ctx.OH_AudioCapturer_Stop(hidden->stream.capturer);
            ctx.OH_AudioCapturer_Release(hidden->stream.capturer);
        } else if ((!device->recording) && (hidden->stream.renderer)) {
            ctx.OH_AudioRenderer_Stop(hidden->stream.renderer);
            // !!! FIXME: do we have to wait for the state to change to make sure all buffered audio has played, or will close do this (or will the system do this after the close)?
            // !!! FIXME: also, will this definitely wait for a running data callback to finish, and then stop the callback from firing again?
            ctx.OH_AudioRenderer_Release(hidden->stream.renderer);
        }

        if (hidden->semaphore) {
            SDL_DestroySemaphore(hidden->semaphore);
        }

        SDL_aligned_free(hidden->mixbuf);
        SDL_free(hidden);
        device->hidden = NULL;
    }
}

static void SetOptionalStreamUsage(OH_AudioStreamBuilder *builder)
{
    const char *hint = SDL_GetHint(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE);
    if (hint) {
        OH_AudioStream_Usage usage = AUDIOSTREAM_USAGE_MOVIE;  // !!! FIXME: I don't know if this is a good default.
        if ((SDL_strcasecmp(hint, "Communications") == 0) || (SDL_strcasecmp(hint, "GameChat") == 0)) {
            usage = AUDIOSTREAM_USAGE_VOICE_COMMUNICATION;
        } else if (SDL_strcasecmp(hint, "Game") == 0) {
            usage = AUDIOSTREAM_USAGE_GAME;
        }
        ctx.OH_AudioStreamBuilder_SetRendererInfo(builder, usage);
    }
}

static bool BuildOHAudioStream(SDL_AudioDevice *device)
{
    const char *streamtypestr = device->recording ? "Capturer" : "Renderer";

    SDL_assert(SDL_GetOpenHarmonySDKVersion() >= 12);  // if not, we need to use a different, deprecated way to assign callbacks. It's doable, but messy if not required.

    // !!! FIXME: make sure these all actually settle out into the say channel order SDL expects.
    // !!! FIXME: 4POINT1 doesn't match SDL's layout in any case. If you can specify bits instead of prechosen enums, we want CH_LAYOUT_QUAD | CH_SET_LOW_FREQUENCY
    static const OH_AudioChannelLayout sdl_layout_map[] = { CH_LAYOUT_MONO, CH_LAYOUT_STEREO, CH_LAYOUT_2POINT1, CH_LAYOUT_QUAD, CH_LAYOUT_4POINT1, CH_LAYOUT_5POINT1_BACK, CH_LAYOUT_6POINT1, CH_LAYOUT_7POINT1 };
    SDL_assert(device->spec.channels <= SDL_arraysize(sdl_layout_map));  // update this if we add support for more channels later.
    device->spec.channels = SDL_clamp(device->spec.channels, 1, SDL_arraysize(sdl_layout_map));

    struct SDL_PrivateAudioData *hidden = device->hidden;
    const bool recording = device->recording;
    OH_AudioStream_Result res;

    SDL_SetAtomicInt(&hidden->error_callback_triggered, 0);

    OH_AudioStreamBuilder *builder = NULL;
    res = ctx.OH_AudioStreamBuilder_Create(&builder, recording ? AUDIOSTREAM_TYPE_CAPTURER : AUDIOSTREAM_TYPE_RENDERER);
    if (res != AUDIOSTREAM_SUCCESS) {
        LOGI("SDL Failed OH_AudioStreamBuilder_Create %d", (int) res);
        return SDL_SetError("SDL Failed OH_AudioStreamBuilder_Create %d", (int) res);
    } else if (!builder) {
        LOGI("SDL Failed OH_AudioStreamBuilder_Create - builder NULL");
        return SDL_SetError("SDL Failed OH_AudioStreamBuilder_Create - builder NULL");
    }

    OH_AudioStream_SampleFormat format;
    if ((device->spec.format == SDL_AUDIO_F32LE) && (SDL_GetOpenHarmonySDKVersion() >= 17)) {
        format = AUDIOSTREAM_SAMPLE_F32LE;
    } else if (device->spec.format == SDL_AUDIO_S32LE) {
        format = AUDIOSTREAM_SAMPLE_S32LE;
    } else if (device->spec.format == SDL_AUDIO_S16LE) {
        format = AUDIOSTREAM_SAMPLE_S16LE;
    } else if (device->spec.format == SDL_AUDIO_U8) {
        format = AUDIOSTREAM_SAMPLE_U8;
    } else {
        format = AUDIOSTREAM_SAMPLE_S16LE;  // sint16 is a safe bet for everything else.
    }
    ctx.OH_AudioStreamBuilder_SetSampleFormat(builder, format);
    ctx.OH_AudioStreamBuilder_SetSamplingRate(builder, device->spec.freq);
    ctx.OH_AudioStreamBuilder_SetChannelCount(builder, device->spec.channels);

#if 0  // !!! FIXME: maybe we don't mess with this. (this is from the AAudio backend.)
    int32_t sample_frames;
    if (SDL_GetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES)) {
        sample_frames = device->sample_frames;
    } else {
        // Use 20 ms for the default audio buffer size
        sample_frames = (device->spec.freq / 50);
    }
    ctx.AAudioStreamBuilder_setBufferCapacityInFrames(builder, 2 * sample_frames); // AAudio requires that the buffer capacity is at least
    ctx.AAudioStreamBuilder_setFramesPerDataCallback(builder, sample_frames);      // twice the size of the data callback buffer size
#endif

    ctx.OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_FAST);
    ctx.OH_AudioStreamBuilder_SetChannelLayout(builder, sdl_layout_map[device->spec.channels-1]);

    LOGI("OHAudio Try to generate %s stream %d hz %s %u channels samples %u",
         device->recording ? "recording" : "playback", device->spec.freq,
         SDL_GetAudioFormatName(device->spec.format), device->spec.channels,
         device->sample_frames);

    if (device->recording) {
        // try to use a microphone that is for recording external audio, otherwise you might end up with one used for telephone calls right when holding the device near your face.
        ctx.OH_AudioStreamBuilder_SetCapturerInfo(builder, AUDIOSTREAM_SOURCE_TYPE_CAMCORDER);
        ctx.OH_AudioStreamBuilder_SetCapturerReadDataCallback(builder, ReadDataCallback, device);
        // !!! FIXME: we can't seem to bind a stream to a specific device, so I don't see a lot of value in knowing when routing changes.
        //OH_AudioStream_Result OH_AudioStreamBuilder_SetCapturerDeviceChangeCallback(OH_AudioStreamBuilder* builder, OH_AudioCapturer_OnDeviceChangeCallback  callback, void* userData);
        ctx.OH_AudioStreamBuilder_SetCapturerErrorCallback(builder, ErrorCallbackCapturer, device);
        res = ctx.OH_AudioStreamBuilder_GenerateCapturer(builder, &hidden->stream.capturer);
    } else {
        SetOptionalStreamUsage(builder);
        ctx.OH_AudioStreamBuilder_SetRendererErrorCallback(builder, ErrorCallbackRenderer, device);
        ctx.OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, WriteDataCallback, device);
        // !!! FIXME: we can't seem to bind a stream to a specific device, so I don't see a lot of value in knowing when routing changes.
        //OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererOutputDeviceChangeCallback(OH_AudioStreamBuilder* builder, OH_AudioRenderer_OutputDeviceChangeCallback callback, void* userData);
        res = ctx.OH_AudioStreamBuilder_GenerateRenderer(builder, &hidden->stream.renderer);
    }

    ctx.OH_AudioStreamBuilder_Destroy(builder);

    if (res != AUDIOSTREAM_SUCCESS) {
        LOGI("SDL Failed OH_AudioStreamBuilder_Generate%s %d %s", streamtypestr, res, AudioStreamResultToString(res));
        return SDL_SetError("%s : %s", SDL_FUNCTION, AudioStreamResultToString(res));
    }

    // !!! FIXME: it's not clear to me if OHAudio streams will promise you the format you requested, converting if necessary, if Generate(Renderer|Capturer) succeeded.
    // !!! FIXME:  AAudio goes and queries to see what it _actually_ got from the stream, so we'll do that too.

    // I assume all of these succeed...why would it fail to tell you the sampling rate of the stream, etc?
    int32_t sample_frames = 0, freq = 0, channels = 0;
    if (device->recording) {
        ctx.OH_AudioCapturer_GetFrameSizeInCallback(hidden->stream.capturer, &sample_frames);
        ctx.OH_AudioCapturer_GetSamplingRate(hidden->stream.capturer, &freq);
        ctx.OH_AudioCapturer_GetChannelCount(hidden->stream.capturer, &channels);
        ctx.OH_AudioCapturer_GetSampleFormat(hidden->stream.capturer, &format);
    } else {
        ctx.OH_AudioRenderer_GetFrameSizeInCallback(hidden->stream.renderer, &sample_frames);
        ctx.OH_AudioRenderer_GetSamplingRate(hidden->stream.renderer, &freq);
        ctx.OH_AudioRenderer_GetChannelCount(hidden->stream.renderer, &channels);
        ctx.OH_AudioRenderer_GetSampleFormat(hidden->stream.renderer, &format);
    }

    device->sample_frames = (int) sample_frames;  // OHAudio docs claim this is always consistent, never returns something like AAUDIO_UNSPECIFIED like AAudio.
    device->spec.freq = (int) freq;
    device->spec.channels = (int) channels;

    if (format == AUDIOSTREAM_SAMPLE_U8) {
        device->spec.format = SDL_AUDIO_U8;
    } else if (format == AUDIOSTREAM_SAMPLE_S16LE) {
        device->spec.format = SDL_AUDIO_S16LE;
    } else if (format == AUDIOSTREAM_SAMPLE_S32LE) {
        device->spec.format = SDL_AUDIO_S32LE;
    } else if (format == AUDIOSTREAM_SAMPLE_F32LE) {
        device->spec.format = SDL_AUDIO_F32LE;
    } else {
        return SDL_SetError("Got unexpected audio format %d from OH_Audio%s_GetSampleFormat", (int) format, streamtypestr);
    }

    SDL_UpdatedAudioDeviceFormat(device);

    // Allocate a triple buffered mixing buffer
    // Two buffers can be in the process of being filled while the third is being read
    hidden->num_buffers = 3;
    hidden->mixbuf_bytes = (hidden->num_buffers * device->buffer_size);
    hidden->mixbuf = (Uint8 *)SDL_aligned_alloc(SDL_GetSIMDAlignment(), hidden->mixbuf_bytes);
    if (!hidden->mixbuf) {
        return false;
    }
    hidden->processed_bytes = 0;
    hidden->callback_bytes = 0;

    hidden->semaphore = SDL_CreateSemaphore(recording ? 0 : hidden->num_buffers - 1);
    if (!hidden->semaphore) {
        LOGI("SDL Failed SDL_CreateSemaphore %s recording:%d", SDL_GetError(), recording);
        return false;
    }

    LOGI("OHAudio Actually generate %s stream %d hz %s %u channels samples %u, buffers %d",
         device->recording ? "recording" : "playback",
         device->spec.freq, SDL_GetAudioFormatName(device->spec.format),
         device->spec.channels, device->sample_frames, hidden->num_buffers);

    return true;
}

// !!! FIXME: make this non-blocking!
#if 0
static void SDLCALL RequestAndroidPermissionBlockingCallback(void *userdata, const char *permission, bool granted)
{
    SDL_SetAtomicInt((SDL_AtomicInt *) userdata, granted ? 1 : -1);
}
#endif

static bool OHAUDIO_OpenDevice(SDL_AudioDevice *device)
{
#if 0  // !!! FIXME: look up how to do this in HarmonyOS.
    if (device->recording) {
        // !!! FIXME: make this non-blocking!
        SDL_AtomicInt permission_response;
        SDL_SetAtomicInt(&permission_response, 0);
        if (!SDL_RequestAndroidPermission("android.permission.RECORD_AUDIO", RequestAndroidPermissionBlockingCallback, &permission_response)) {
            return false;
        }

        while (SDL_GetAtomicInt(&permission_response) == 0) {
            SDL_Delay(10);
        }

        if (SDL_GetAtomicInt(&permission_response) < 0) {
            LOGI("This app doesn't have RECORD_AUDIO permission");
            return SDL_SetError("This app doesn't have RECORD_AUDIO permission");
        }
    }
#endif

    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return false;
    }

    return BuildOHAudioStream(device);
}

static bool PauseOneDevice(SDL_AudioDevice *device, void *userdata)
{
    struct SDL_PrivateAudioData *hidden = (struct SDL_PrivateAudioData *)device->hidden;
    if (hidden) {
        if (hidden->stream.renderer) {  // this is a union, so a valid capture stream will still have a non-NULL `renderer`.
            OH_AudioStream_Result res;
            if (device->recording) {
                res = ctx.OH_AudioCapturer_Pause(hidden->stream.capturer);
            } else {
                res = ctx.OH_AudioRenderer_Pause(hidden->stream.renderer);
            }

            if (res != AUDIOSTREAM_SUCCESS) {
                LOGI("SDL Failed OH_Audio%s_Pause %d", device->recording ? "Capturer" : "Renderer", res);
                SDL_SetError("%s : %s", SDL_FUNCTION, AudioStreamResultToString(res));
            }
        }
    }
    return false;  // keep enumerating.
}

// Pause (block) all non already paused audio devices by taking their mixer lock
void OHAUDIO_PauseDevices(void)
{
    if (ctx.handle) {  // OHAudio driver is used?
        (void) SDL_FindPhysicalAudioDeviceByCallback(PauseOneDevice, NULL);
    }
}

// Resume (unblock) all non already paused audio devices by releasing their mixer lock
static bool ResumeOneDevice(SDL_AudioDevice *device, void *userdata)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    if (hidden) {
        OH_AudioStream_Result res = AUDIOSTREAM_SUCCESS;
        if (device->recording && hidden->stream.capturer) {
            res = ctx.OH_AudioCapturer_Start(hidden->stream.capturer);
        } else if (!device->recording && hidden->stream.renderer) {
            res = ctx.OH_AudioRenderer_Start(hidden->stream.renderer);
        }

        if (res != AUDIOSTREAM_SUCCESS) {
            LOGI("SDL Failed OH_Audio%s_Start %d", device->recording ? "Capturer" : "Renderer", res);
            SDL_SetError("%s : %s", SDL_FUNCTION, AudioStreamResultToString(res));
        }
    }
    return false;  // keep enumerating.
}

void OHAUDIO_ResumeDevices(void)
{
    if (ctx.handle) {  // AAUDIO driver is used?
        (void) SDL_FindPhysicalAudioDeviceByCallback(ResumeOneDevice, NULL);
    }
}

static void OHAUDIO_Deinitialize(void)
{
    if (ctx.handle) {
        SDL_UnloadObject(ctx.handle);
    }
    SDL_zero(ctx);
}


static void OHAUDIO_ThreadInit(SDL_AudioDevice *device)
{
    SDL_SetCurrentThreadPriority(device->recording ? SDL_THREAD_PRIORITY_HIGH : SDL_THREAD_PRIORITY_TIME_CRITICAL);
    // Start the audio stream here; OHAudio's docs say that starting and stopping might take significant time and shouldn't be done from the main thread.
    if (device->recording) {
        ctx.OH_AudioCapturer_Start(device->hidden->stream.capturer);
    } else {
        ctx.OH_AudioRenderer_Start(device->hidden->stream.renderer);
    }
}

static bool OHAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    SDL_zero(ctx);

    ctx.handle = SDL_LoadObject(LIB_OHAUDIO_SO);
    if (!ctx.handle) {
        LOGI("SDL couldn't find " LIB_OHAUDIO_SO);
        return false;
    }

    if (!OHAUDIO_LoadFunctions(&ctx)) {
        SDL_UnloadObject(ctx.handle);
        SDL_zero(ctx);
        return false;
    }

    impl->ThreadInit = OHAUDIO_ThreadInit;
    impl->Deinitialize = OHAUDIO_Deinitialize;
    impl->OpenDevice = OHAUDIO_OpenDevice;
    impl->CloseDevice = OHAUDIO_CloseDevice;
    impl->WaitDevice = OHAUDIO_WaitDevice;
    impl->PlayDevice = OHAUDIO_PlayDevice;
    impl->GetDeviceBuf = OHAUDIO_GetDeviceBuf;
    impl->WaitRecordingDevice = OHAUDIO_WaitDevice;
    impl->RecordDevice = OHAUDIO_RecordDevice;
    impl->HasRecordingSupport = true;
    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->OnlyHasDefaultRecordingDevice = true;

    LOGI("SDL OHAUDIO_Init OK");
    return true;
}

AudioBootStrap OHAUDIO_bootstrap = {
    "OHAudio", "OpenHarmony OHAudio audio driver", OHAUDIO_Init, false, false
};

#endif // SDL_AUDIO_DRIVER_OHAUDIO
