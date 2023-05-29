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

#ifndef SDL_sysaudio_h_
#define SDL_sysaudio_h_

#include "../SDL_dataqueue.h"

#define DEBUG_AUDIOSTREAM 0
#define DEBUG_AUDIO_CONVERT 0

#if DEBUG_AUDIO_CONVERT
#define LOG_DEBUG_AUDIO_CONVERT(from, to) SDL_Log("SDL_AUDIO_CONVERT: Converting %s to %s.\n", from, to);
#else
#define LOG_DEBUG_AUDIO_CONVERT(from, to)
#endif

/* These pointers get set during SDL_ChooseAudioConverters() to various SIMD implementations. */
extern void (*SDL_Convert_S8_to_F32)(float *dst, const Sint8 *src, int num_samples);
extern void (*SDL_Convert_U8_to_F32)(float *dst, const Uint8 *src, int num_samples);
extern void (*SDL_Convert_S16_to_F32)(float *dst, const Sint16 *src, int num_samples);
extern void (*SDL_Convert_S32_to_F32)(float *dst, const Sint32 *src, int num_samples);
extern void (*SDL_Convert_F32_to_S8)(Sint8 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_U8)(Uint8 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_S16)(Sint16 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_S32)(Sint32 *dst, const float *src, int num_samples);


/* !!! FIXME: These are wordy and unlocalized... */
#define DEFAULT_OUTPUT_DEVNAME "System audio output device"
#define DEFAULT_INPUT_DEVNAME  "System audio capture device"

/* these are used when no better specifics are known. We default to CD audio quality. */
#define DEFAULT_AUDIO_FORMAT SDL_AUDIO_S16
#define DEFAULT_AUDIO_CHANNELS 2
#define DEFAULT_AUDIO_FREQUENCY 44100


/* The SDL audio driver */
typedef struct SDL_AudioDevice SDL_AudioDevice;

/* Used by src/SDL.c to initialize a particular audio driver. */
extern int SDL_InitAudio(const char *driver_name);

/* Used by src/SDL.c to shut down previously-initialized audio. */
extern void SDL_QuitAudio(void);

/* Function to get a list of audio formats, ordered most similar to `format` to least, 0-terminated. Don't free results. */
const SDL_AudioFormat *SDL_ClosestAudioFormats(SDL_AudioFormat format);

/* Must be called at least once before using converters (SDL_CreateAudioStream will call it !!! FIXME but probably shouldn't). */
extern void SDL_ChooseAudioConverters(void);

/* Audio targets should call this as devices are added to the system (such as
   a USB headset being plugged in), and should also be called for
   for every device found during DetectDevices(). */
extern SDL_AudioDevice *SDL_AddAudioDevice(const SDL_bool iscapture, const char *name, SDL_AudioFormat fmt, int channels, int freq, void *handle);

/* Audio targets should call this if an opened audio device is lost.
   This can happen due to i/o errors, or a device being unplugged, etc. */
extern void SDL_AudioDeviceDisconnected(SDL_AudioDevice *device);

/* Find the SDL_AudioDevice associated with the handle supplied to SDL_AddAudioDevice. NULL if not found. Locks the device! You must unlock!! */
extern SDL_AudioDevice *SDL_ObtainAudioDeviceByHandle(void *handle);

/* Backends should call this if they change the device format, channels, freq, or sample_frames to keep other state correct. */
extern void SDL_UpdatedAudioDeviceFormat(SDL_AudioDevice *device);


/* These functions are the heart of the audio threads. Backends can call them directly if they aren't using the SDL-provided thread. */
extern void SDL_OutputAudioThreadSetup(SDL_AudioDevice *device);
extern SDL_bool SDL_OutputAudioThreadIterate(SDL_AudioDevice *device);
extern void SDL_OutputAudioThreadShutdown(SDL_AudioDevice *device);
extern void SDL_CaptureAudioThreadSetup(SDL_AudioDevice *device);
extern SDL_bool SDL_CaptureAudioThreadIterate(SDL_AudioDevice *device);
extern void SDL_CaptureAudioThreadShutdown(SDL_AudioDevice *device);

typedef struct SDL_AudioDriverImpl
{
    void (*DetectDevices)(void);
    int (*OpenDevice)(SDL_AudioDevice *device);
    void (*ThreadInit)(SDL_AudioDevice *device);   /* Called by audio thread at start */
    void (*ThreadDeinit)(SDL_AudioDevice *device); /* Called by audio thread at end */
    void (*WaitDevice)(SDL_AudioDevice *device);
    void (*PlayDevice)(SDL_AudioDevice *device, int buffer_size);
    Uint8 *(*GetDeviceBuf)(SDL_AudioDevice *device, int *buffer_size);
    int (*CaptureFromDevice)(SDL_AudioDevice *device, void *buffer, int buflen);
    void (*FlushCapture)(SDL_AudioDevice *device);
    void (*CloseDevice)(SDL_AudioDevice *device);
    void (*FreeDeviceHandle)(void *handle); /**< SDL is done with handle from SDL_AddAudioDevice() */
    void (*Deinitialize)(void);

    /* Some flags to push duplicate code into the core and reduce #ifdefs. */
    SDL_bool ProvidesOwnCallbackThread;
    SDL_bool HasCaptureSupport;
    SDL_bool OnlyHasDefaultOutputDevice;
    SDL_bool OnlyHasDefaultCaptureDevice;
    SDL_bool AllowsArbitraryDeviceNames;
    SDL_bool SupportsNonPow2Samples;
} SDL_AudioDriverImpl;

typedef struct SDL_AudioDriver
{
    const char *name;  /* The name of this audio driver */
    const char *desc;  /* The description of this audio driver */
    SDL_AudioDriverImpl impl; /* the backend's interface */
    SDL_RWLock *device_list_lock;  /* A mutex for device detection */
    SDL_AudioDevice *output_devices;  /* the list of currently-available audio output devices. */
    SDL_AudioDevice *capture_devices;  /* the list of currently-available audio capture devices. */
    SDL_AtomicInt output_device_count;
    SDL_AtomicInt capture_device_count;
    SDL_AtomicInt last_device_instance_id;  /* increments on each device add to provide unique instance IDs */
    SDL_AtomicInt shutting_down;  /* non-zero during SDL_Quit, so we known not to accept any last-minute device hotplugs. */
} SDL_AudioDriver;

struct SDL_AudioStream
{
    SDL_DataQueue *queue;
    SDL_Mutex *lock;  /* this is just a copy of `queue`'s mutex. We share a lock. */

    SDL_AudioStreamRequestCallback get_callback;
    void *get_callback_userdata;
    SDL_AudioStreamRequestCallback put_callback;
    void *put_callback_userdata;

    Uint8 *work_buffer;    /* used for scratch space during data conversion/resampling. */
    Uint8 *history_buffer;  /* history for left padding and future sample rate changes. */
    Uint8 *future_buffer;  /* stuff that left the queue for the right padding and will be next read's data. */
    float *left_padding;  /* left padding for resampling. */
    float *right_padding;  /* right padding for resampling. */

    SDL_bool flushed;

    size_t work_buffer_allocation;
    size_t history_buffer_allocation;
    size_t future_buffer_allocation;
    size_t resampler_padding_allocation;

    int resampler_padding_frames;
    int history_buffer_frames;
    int future_buffer_filled_frames;

    int max_sample_frame_size;

    int src_sample_frame_size;
    SDL_AudioFormat src_format;
    int src_channels;
    int src_rate;

    int dst_sample_frame_size;
    SDL_AudioFormat dst_format;
    int dst_channels;
    int dst_rate;

    int pre_resample_channels;
    int packetlen;

    SDL_AudioDevice *bound_device;
    SDL_AudioStream *next_binding;
    SDL_AudioStream *prev_binding;
};

struct SDL_AudioDevice
{
    /* A mutex for locking access to this struct */
    SDL_Mutex *lock;

    /* human-readable name of the device. ("SoundBlaster Pro 16") */
    char *name;

    /* the unique instance ID of this device. */
    SDL_AudioDeviceID instance_id;

    /* a way for the backend to identify this device _when not opened_ */
    void *handle;

    /* The device's current audio specification */
    SDL_AudioFormat format;
    int freq;
    int channels;
    Uint32 buffer_size;

    /* The device's default audio specification */
    SDL_AudioFormat default_format;
    int default_freq;
    int default_channels;

    /* Number of sample frames the devices wants per-buffer. */
    int sample_frames;

    /* Value to use for SDL_memset to silence a buffer in this device's format */
    int silence_value;

    /* non-zero if we are signaling the audio thread to end. */
    SDL_AtomicInt shutdown;

    /* non-zero if we want the device to be destroyed (so audio thread knows to do it on termination). */
    SDL_AtomicInt condemned;

    /* SDL_TRUE if this is a capture device instead of an output device */
    SDL_bool iscapture;

    /* Scratch buffer used for mixing. */
    Uint8 *work_buffer;

    /* A thread to feed the audio device */
    SDL_Thread *thread;

    /* Data private to this driver */
    struct SDL_PrivateAudioData *hidden;

    /* Each device open increases the refcount. We actually close the system device when this hits zero again. */
    SDL_AtomicInt refcount;

    /* double-linked list of all audio streams currently bound to this device. */
    SDL_AudioStream *bound_streams;

    /* double-linked list of all devices. */
    struct SDL_AudioDevice *prev;
    struct SDL_AudioDevice *next;
};

typedef struct AudioBootStrap
{
    const char *name;
    const char *desc;
    SDL_bool (*init)(SDL_AudioDriverImpl *impl);
    SDL_bool demand_only; /* if SDL_TRUE: request explicitly, or it won't be available. */
} AudioBootStrap;

/* Not all of these are available in a given build. Use #ifdefs, etc. */
extern AudioBootStrap PIPEWIRE_bootstrap;
extern AudioBootStrap PULSEAUDIO_bootstrap;
extern AudioBootStrap ALSA_bootstrap;
extern AudioBootStrap JACK_bootstrap;
extern AudioBootStrap SNDIO_bootstrap;
extern AudioBootStrap NETBSDAUDIO_bootstrap;
extern AudioBootStrap DSP_bootstrap;
extern AudioBootStrap WASAPI_bootstrap;
extern AudioBootStrap DSOUND_bootstrap;
extern AudioBootStrap WINMM_bootstrap;
extern AudioBootStrap HAIKUAUDIO_bootstrap;
extern AudioBootStrap COREAUDIO_bootstrap;
extern AudioBootStrap DISKAUDIO_bootstrap;
extern AudioBootStrap DUMMYAUDIO_bootstrap;
extern AudioBootStrap aaudio_bootstrap;   /* !!! FIXME: capitalize this to match the others */
extern AudioBootStrap openslES_bootstrap;  /* !!! FIXME: capitalize this to match the others */
extern AudioBootStrap ANDROIDAUDIO_bootstrap;
extern AudioBootStrap PS2AUDIO_bootstrap;
extern AudioBootStrap PSPAUDIO_bootstrap;
extern AudioBootStrap VITAAUD_bootstrap;
extern AudioBootStrap N3DSAUDIO_bootstrap;
extern AudioBootStrap EMSCRIPTENAUDIO_bootstrap;
extern AudioBootStrap QSAAUDIO_bootstrap;

extern SDL_AudioDevice *get_audio_dev(SDL_AudioDeviceID id);
extern int get_max_num_audio_dev(void);

#endif /* SDL_sysaudio_h_ */
