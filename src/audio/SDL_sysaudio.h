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

#include "../SDL_hashtable.h"

#define DEBUG_AUDIOSTREAM 0
#define DEBUG_AUDIO_CONVERT 0

#if DEBUG_AUDIO_CONVERT
#define LOG_DEBUG_AUDIO_CONVERT(from, to) SDL_Log("SDL_AUDIO_CONVERT: Converting %s to %s.\n", from, to);
#else
#define LOG_DEBUG_AUDIO_CONVERT(from, to)
#endif

// These pointers get set during SDL_ChooseAudioConverters() to various SIMD implementations.
extern void (*SDL_Convert_S8_to_F32)(float *dst, const Sint8 *src, int num_samples);
extern void (*SDL_Convert_U8_to_F32)(float *dst, const Uint8 *src, int num_samples);
extern void (*SDL_Convert_S16_to_F32)(float *dst, const Sint16 *src, int num_samples);
extern void (*SDL_Convert_S32_to_F32)(float *dst, const Sint32 *src, int num_samples);
extern void (*SDL_Convert_F32_to_S8)(Sint8 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_U8)(Uint8 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_S16)(Sint16 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_S32)(Sint32 *dst, const float *src, int num_samples);

// !!! FIXME: These are wordy and unlocalized...
#define DEFAULT_OUTPUT_DEVNAME "System audio output device"
#define DEFAULT_INPUT_DEVNAME  "System audio capture device"

// these are used when no better specifics are known. We default to CD audio quality.
#define DEFAULT_AUDIO_OUTPUT_FORMAT SDL_AUDIO_S16
#define DEFAULT_AUDIO_OUTPUT_CHANNELS 2
#define DEFAULT_AUDIO_OUTPUT_FREQUENCY 44100

#define DEFAULT_AUDIO_CAPTURE_FORMAT SDL_AUDIO_S16
#define DEFAULT_AUDIO_CAPTURE_CHANNELS 1
#define DEFAULT_AUDIO_CAPTURE_FREQUENCY 44100

#define AUDIO_SPECS_EQUAL(x, y) (((x).format == (y).format) && ((x).channels == (y).channels) && ((x).freq == (y).freq))

typedef struct SDL_AudioDevice SDL_AudioDevice;
typedef struct SDL_LogicalAudioDevice SDL_LogicalAudioDevice;

// Used by src/SDL.c to initialize a particular audio driver.
extern int SDL_InitAudio(const char *driver_name);

// Used by src/SDL.c to shut down previously-initialized audio.
extern void SDL_QuitAudio(void);

// Function to get a list of audio formats, ordered most similar to `format` to least, 0-terminated. Don't free results.
const SDL_AudioFormat *SDL_ClosestAudioFormats(SDL_AudioFormat format);

// Must be called at least once before using converters.
extern void SDL_ChooseAudioConverters(void);
extern void SDL_SetupAudioResampler(void);

/* Backends should call this as devices are added to the system (such as
   a USB headset being plugged in), and should also be called for
   for every device found during DetectDevices(). */
extern SDL_AudioDevice *SDL_AddAudioDevice(const SDL_bool iscapture, const char *name, const SDL_AudioSpec *spec, void *handle);

/* Backends should call this if an opened audio device is lost.
   This can happen due to i/o errors, or a device being unplugged, etc. */
extern void SDL_AudioDeviceDisconnected(SDL_AudioDevice *device);

// Backends should call this if the system default device changes.
extern void SDL_DefaultAudioDeviceChanged(SDL_AudioDevice *new_default_device);

// Backends should call this if a device's format is changing (opened or not); SDL will update state and carry on with the new format.
extern int SDL_AudioDeviceFormatChanged(SDL_AudioDevice *device, const SDL_AudioSpec *newspec, int new_sample_frames);

// Same as above, but assume the device is already locked.
extern int SDL_AudioDeviceFormatChangedAlreadyLocked(SDL_AudioDevice *device, const SDL_AudioSpec *newspec, int new_sample_frames);

// Find the SDL_AudioDevice associated with the handle supplied to SDL_AddAudioDevice. NULL if not found. DOES NOT LOCK THE DEVICE.
extern SDL_AudioDevice *SDL_FindPhysicalAudioDeviceByHandle(void *handle);

// Find an SDL_AudioDevice, selected by a callback. NULL if not found. DOES NOT LOCK THE DEVICE.
extern SDL_AudioDevice *SDL_FindPhysicalAudioDeviceByCallback(SDL_bool (*callback)(SDL_AudioDevice *device, void *userdata), void *userdata);

// Backends should call this if they change the device format, channels, freq, or sample_frames to keep other state correct.
extern void SDL_UpdatedAudioDeviceFormat(SDL_AudioDevice *device);

// Backends can call this to get a standardized name for a thread to power a specific audio device.
extern char *SDL_GetAudioThreadName(SDL_AudioDevice *device, char *buf, size_t buflen);

// Backends can call these to change a device's refcount.
extern void RefPhysicalAudioDevice(SDL_AudioDevice *device);
extern void UnrefPhysicalAudioDevice(SDL_AudioDevice *device);

// These functions are the heart of the audio threads. Backends can call them directly if they aren't using the SDL-provided thread.
extern void SDL_OutputAudioThreadSetup(SDL_AudioDevice *device);
extern SDL_bool SDL_OutputAudioThreadIterate(SDL_AudioDevice *device);
extern void SDL_OutputAudioThreadShutdown(SDL_AudioDevice *device);
extern void SDL_CaptureAudioThreadSetup(SDL_AudioDevice *device);
extern SDL_bool SDL_CaptureAudioThreadIterate(SDL_AudioDevice *device);
extern void SDL_CaptureAudioThreadShutdown(SDL_AudioDevice *device);
extern void SDL_AudioThreadFinalize(SDL_AudioDevice *device);

// this gets used from the audio device threads. It has rules, don't use this if you don't know how to use it!
extern void ConvertAudio(int num_frames, const void *src, SDL_AudioFormat src_format, int src_channels,
                         void *dst, SDL_AudioFormat dst_format, int dst_channels, void* scratch);

// Special case to let something in SDL_audiocvt.c access something in SDL_audio.c. Don't use this.
extern void OnAudioStreamCreated(SDL_AudioStream *stream);
extern void OnAudioStreamDestroy(SDL_AudioStream *stream);

typedef struct SDL_AudioDriverImpl
{
    void (*DetectDevices)(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture);
    int (*OpenDevice)(SDL_AudioDevice *device);
    void (*ThreadInit)(SDL_AudioDevice *device);   // Called by audio thread at start
    void (*ThreadDeinit)(SDL_AudioDevice *device); // Called by audio thread at end
    int (*WaitDevice)(SDL_AudioDevice *device);
    int (*PlayDevice)(SDL_AudioDevice *device, const Uint8 *buffer, int buflen);  // buffer and buflen are always from GetDeviceBuf, passed here for convenience.
    Uint8 *(*GetDeviceBuf)(SDL_AudioDevice *device, int *buffer_size);
    int (*WaitCaptureDevice)(SDL_AudioDevice *device);
    int (*CaptureFromDevice)(SDL_AudioDevice *device, void *buffer, int buflen);
    void (*FlushCapture)(SDL_AudioDevice *device);
    void (*CloseDevice)(SDL_AudioDevice *device);
    void (*FreeDeviceHandle)(SDL_AudioDevice *device); // SDL is done with this device; free the handle from SDL_AddAudioDevice()
    void (*DeinitializeStart)(void); // SDL calls this, then starts destroying objects, then calls Deinitialize. This is a good place to stop hotplug detection.
    void (*Deinitialize)(void);

    // Some flags to push duplicate code into the core and reduce #ifdefs.
    SDL_bool ProvidesOwnCallbackThread;  // !!! FIXME: rename this, it's not a callback thread anymore.
    SDL_bool HasCaptureSupport;
    SDL_bool OnlyHasDefaultOutputDevice;
    SDL_bool OnlyHasDefaultCaptureDevice;   // !!! FIXME: is there ever a time where you'd have a default output and not a default capture (or vice versa)?
} SDL_AudioDriverImpl;


typedef struct SDL_PendingAudioDeviceEvent
{
    Uint32 type;
    SDL_AudioDeviceID devid;
    struct SDL_PendingAudioDeviceEvent *next;
} SDL_PendingAudioDeviceEvent;

typedef struct SDL_AudioDriver
{
    const char *name;  // The name of this audio driver
    const char *desc;  // The description of this audio driver
    SDL_AudioDriverImpl impl; // the backend's interface
    SDL_RWLock *device_hash_lock;  // A rwlock that protects `device_hash`
    SDL_HashTable *device_hash;  // the collection of currently-available audio devices (capture, playback, logical and physical!)
    SDL_AudioStream *existing_streams;  // a list of all existing SDL_AudioStreams.
    SDL_AudioDeviceID default_output_device_id;
    SDL_AudioDeviceID default_capture_device_id;
    SDL_PendingAudioDeviceEvent pending_events;
    SDL_PendingAudioDeviceEvent *pending_events_tail;

    // !!! FIXME: most (all?) of these don't have to be atomic.
    SDL_AtomicInt output_device_count;
    SDL_AtomicInt capture_device_count;
    SDL_AtomicInt shutting_down;  // non-zero during SDL_Quit, so we known not to accept any last-minute device hotplugs.
} SDL_AudioDriver;

struct SDL_AudioQueue; // forward decl.

struct SDL_AudioStream
{
    SDL_Mutex* lock;

    SDL_PropertiesID props;

    SDL_AudioStreamCallback get_callback;
    void *get_callback_userdata;
    SDL_AudioStreamCallback put_callback;
    void *put_callback_userdata;

    SDL_AudioSpec src_spec;
    SDL_AudioSpec dst_spec;
    float freq_ratio;

    struct SDL_AudioQueue* queue;
    Uint64 total_bytes_queued;

    SDL_AudioSpec input_spec; // The spec of input data currently being processed
    Sint64 resample_offset;

    Uint8 *work_buffer;    // used for scratch space during data conversion/resampling.
    size_t work_buffer_allocation;

    Uint8 *history_buffer;  // history for left padding and future sample rate changes.
    size_t history_buffer_allocation;

    SDL_bool simplified;  // SDL_TRUE if created via SDL_OpenAudioDeviceStream

    SDL_LogicalAudioDevice *bound_device;
    SDL_AudioStream *next_binding;
    SDL_AudioStream *prev_binding;

    SDL_AudioStream *prev;  // linked list of all existing streams (so we can free them on shutdown).
    SDL_AudioStream *next;  // linked list of all existing streams (so we can free them on shutdown).
};

/* Logical devices are an abstraction in SDL3; you can open the same physical
   device multiple times, and each will result in an object with its own set
   of bound audio streams, etc, even though internally these are all processed
   as a group when mixing the final output for the physical device. */
struct SDL_LogicalAudioDevice
{
    // the unique instance ID of this device.
    SDL_AudioDeviceID instance_id;

    // The physical device associated with this opened device.
    SDL_AudioDevice *physical_device;

    // If whole logical device is paused (process no streams bound to this device).
    SDL_AtomicInt paused;

    // double-linked list of all audio streams currently bound to this opened device.
    SDL_AudioStream *bound_streams;

    // SDL_TRUE if this was opened as a default device.
    SDL_bool opened_as_default;

    // SDL_TRUE if device was opened with SDL_OpenAudioDeviceStream (so it forbids binding changes, etc).
    SDL_bool simplified;

    // If non-NULL, callback into the app that lets them access the final postmix buffer.
    SDL_AudioPostmixCallback postmix;

    // App-supplied pointer for postmix callback.
    void *postmix_userdata;

    // double-linked list of opened devices on the same physical device.
    SDL_LogicalAudioDevice *next;
    SDL_LogicalAudioDevice *prev;
};

struct SDL_AudioDevice
{
    // A mutex for locking access to this struct
    SDL_Mutex *lock;

    // A condition variable to protect device close, where we can't hold the device lock forever.
    SDL_Condition *close_cond;

    // Reference count of the device; logical devices, device threads, etc, add to this.
    SDL_AtomicInt refcount;

    // These are, initially, set from current_audio, but we might swap them out with Zombie versions on disconnect/failure.
    int (*WaitDevice)(SDL_AudioDevice *device);
    int (*PlayDevice)(SDL_AudioDevice *device, const Uint8 *buffer, int buflen);
    Uint8 *(*GetDeviceBuf)(SDL_AudioDevice *device, int *buffer_size);
    int (*WaitCaptureDevice)(SDL_AudioDevice *device);
    int (*CaptureFromDevice)(SDL_AudioDevice *device, void *buffer, int buflen);
    void (*FlushCapture)(SDL_AudioDevice *device);

    // human-readable name of the device. ("SoundBlaster Pro 16")
    char *name;

    // the unique instance ID of this device.
    SDL_AudioDeviceID instance_id;

    // a way for the backend to identify this device _when not opened_
    void *handle;

    // The device's current audio specification
    SDL_AudioSpec spec;
    int buffer_size;

    // The device's default audio specification
    SDL_AudioSpec default_spec;

    // Number of sample frames the devices wants per-buffer.
    int sample_frames;

    // Value to use for SDL_memset to silence a buffer in this device's format
    int silence_value;

    // non-zero if we are signaling the audio thread to end.
    SDL_AtomicInt shutdown;

    // non-zero if this was a disconnected device and we're waiting for it to be decommissioned.
    SDL_AtomicInt zombie;

    // SDL_TRUE if this is a capture device instead of an output device
    SDL_bool iscapture;

    // SDL_TRUE if audio thread can skip silence/mix/convert stages and just do a basic memcpy.
    SDL_bool simple_copy;

    // Scratch buffers used for mixing.
    Uint8 *work_buffer;
    Uint8 *mix_buffer;
    float *postmix_buffer;

    // Size of work_buffer (and mix_buffer) in bytes.
    int work_buffer_size;

    // A thread to feed the audio device
    SDL_Thread *thread;

    // SDL_TRUE if this physical device is currently opened by the backend.
    SDL_bool currently_opened;

    // Data private to this driver
    struct SDL_PrivateAudioData *hidden;

    // All logical devices associated with this physical device.
    SDL_LogicalAudioDevice *logical_devices;
};

typedef struct AudioBootStrap
{
    const char *name;
    const char *desc;
    SDL_bool (*init)(SDL_AudioDriverImpl *impl);
    SDL_bool demand_only; // if SDL_TRUE: request explicitly, or it won't be available.
} AudioBootStrap;

// Not all of these are available in a given build. Use #ifdefs, etc.
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
extern AudioBootStrap AAUDIO_bootstrap;
extern AudioBootStrap OPENSLES_bootstrap;
extern AudioBootStrap ANDROIDAUDIO_bootstrap;
extern AudioBootStrap PS2AUDIO_bootstrap;
extern AudioBootStrap PSPAUDIO_bootstrap;
extern AudioBootStrap VITAAUD_bootstrap;
extern AudioBootStrap N3DSAUDIO_bootstrap;
extern AudioBootStrap EMSCRIPTENAUDIO_bootstrap;
extern AudioBootStrap QSAAUDIO_bootstrap;

#endif // SDL_sysaudio_h_
