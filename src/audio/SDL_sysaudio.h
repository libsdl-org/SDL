/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

#ifndef _SDL_sysaudio_h
#define _SDL_sysaudio_h

#include "SDL_mutex.h"
#include "SDL_thread.h"

/* The SDL audio driver */
typedef struct SDL_AudioDevice SDL_AudioDevice;
#define _THIS   SDL_AudioDevice *_this

/* Used by audio targets during DetectDevices() */
typedef void (*SDL_AddAudioDevice)(const char *name);

typedef struct SDL_AudioDriverImpl
{
    void (*DetectDevices) (int iscapture, SDL_AddAudioDevice addfn);
    int (*OpenDevice) (_THIS, const char *devname, int iscapture);
    void (*ThreadInit) (_THIS); /* Called by audio thread at start */
    void (*WaitDevice) (_THIS);
    void (*PlayDevice) (_THIS);
    Uint8 *(*GetDeviceBuf) (_THIS);
    void (*WaitDone) (_THIS);
    void (*CloseDevice) (_THIS);
    void (*LockDevice) (_THIS);
    void (*UnlockDevice) (_THIS);
    void (*Deinitialize) (void);

    /* !!! FIXME: add pause(), so we can optimize instead of mixing silence. */

    /* Some flags to push duplicate code into the core and reduce #ifdefs. */
    int ProvidesOwnCallbackThread;
    int SkipMixerLock;  /* !!! FIXME: do we need this anymore? */
    int HasCaptureSupport;
    int OnlyHasDefaultOutputDevice;
    int OnlyHasDefaultInputDevice;
} SDL_AudioDriverImpl;


typedef struct SDL_AudioDriver
{
    /* * * */
    /* The name of this audio driver */
    const char *name;

    /* * * */
    /* The description of this audio driver */
    const char *desc;

    SDL_AudioDriverImpl impl;

    char **outputDevices;
    int outputDeviceCount;

    char **inputDevices;
    int inputDeviceCount;
} SDL_AudioDriver;


/* Streamer */
typedef struct
{
    Uint8 *buffer;
    int max_len;                /* the maximum length in bytes */
    int read_pos, write_pos;    /* the position of the write and read heads in bytes */
} SDL_AudioStreamer;


/* Define the SDL audio driver structure */
struct SDL_AudioDevice
{
    /* * * */
    /* Data common to all devices */

    /* The current audio specification (shared with audio thread) */
    SDL_AudioSpec spec;

    /* An audio conversion block for audio format emulation */
    SDL_AudioCVT convert;

    /* The streamer, if sample rate conversion necessitates it */
    int use_streamer;
    SDL_AudioStreamer streamer;

    /* Current state flags */
    int iscapture;
    int enabled;
    int paused;
    int opened;

    /* Fake audio buffer for when the audio hardware is busy */
    Uint8 *fake_stream;

    /* A semaphore for locking the mixing buffers */
    SDL_mutex *mixer_lock;

    /* A thread to feed the audio device */
    SDL_Thread *thread;
    SDL_threadID threadid;

    /* * * */
    /* Data private to this driver */
    struct SDL_PrivateAudioData *hidden;
};
#undef _THIS

typedef struct AudioBootStrap
{
    const char *name;
    const char *desc;
    int (*init) (SDL_AudioDriverImpl * impl);
    int demand_only;  /* 1==request explicitly, or it won't be available. */
} AudioBootStrap;

#endif /* _SDL_sysaudio_h */

/* vi: set ts=4 sw=4 expandtab: */
