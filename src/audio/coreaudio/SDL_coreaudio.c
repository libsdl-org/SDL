/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_AUDIO_DRIVER_COREAUDIO

/* !!! FIXME: clean out some of the macro salsa in here. */

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_coreaudio.h"
#include "SDL_assert.h"

#define DEBUG_COREAUDIO 0

#define CHECK_RESULT(msg) \
    if (result != noErr) { \
        SDL_SetError("CoreAudio error (%s): %d", msg, (int) result); \
        return 0; \
    }

#if MACOSX_COREAUDIO
static const AudioObjectPropertyAddress devlist_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

typedef void (*addDevFn)(const char *name, const int iscapture, AudioDeviceID devId, void *data);

typedef struct AudioDeviceList
{
    AudioDeviceID devid;
    SDL_bool alive;
    struct AudioDeviceList *next;
} AudioDeviceList;

static AudioDeviceList *output_devs = NULL;
static AudioDeviceList *capture_devs = NULL;

static SDL_bool
add_to_internal_dev_list(const int iscapture, AudioDeviceID devId)
{
    AudioDeviceList *item = (AudioDeviceList *) SDL_malloc(sizeof (AudioDeviceList));
    if (item == NULL) {
        return SDL_FALSE;
    }
    item->devid = devId;
    item->alive = SDL_TRUE;
    item->next = iscapture ? capture_devs : output_devs;
    if (iscapture) {
        capture_devs = item;
    } else {
        output_devs = item;
    }

    return SDL_TRUE;
}

static void
addToDevList(const char *name, const int iscapture, AudioDeviceID devId, void *data)
{
    if (add_to_internal_dev_list(iscapture, devId)) {
        SDL_AddAudioDevice(iscapture, name, (void *) ((size_t) devId));
    }
}

static void
build_device_list(int iscapture, addDevFn addfn, void *addfndata)
{
    OSStatus result = noErr;
    UInt32 size = 0;
    AudioDeviceID *devs = NULL;
    UInt32 i = 0;
    UInt32 max = 0;

    result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                            &devlist_address, 0, NULL, &size);
    if (result != kAudioHardwareNoError)
        return;

    devs = (AudioDeviceID *) alloca(size);
    if (devs == NULL)
        return;

    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &devlist_address, 0, NULL, &size, devs);
    if (result != kAudioHardwareNoError)
        return;

    max = size / sizeof (AudioDeviceID);
    for (i = 0; i < max; i++) {
        CFStringRef cfstr = NULL;
        char *ptr = NULL;
        AudioDeviceID dev = devs[i];
        AudioBufferList *buflist = NULL;
        int usable = 0;
        CFIndex len = 0;
        const AudioObjectPropertyAddress addr = {
            kAudioDevicePropertyStreamConfiguration,
            iscapture ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        const AudioObjectPropertyAddress nameaddr = {
            kAudioObjectPropertyName,
            iscapture ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        result = AudioObjectGetPropertyDataSize(dev, &addr, 0, NULL, &size);
        if (result != noErr)
            continue;

        buflist = (AudioBufferList *) SDL_malloc(size);
        if (buflist == NULL)
            continue;

        result = AudioObjectGetPropertyData(dev, &addr, 0, NULL,
                                            &size, buflist);

        if (result == noErr) {
            UInt32 j;
            for (j = 0; j < buflist->mNumberBuffers; j++) {
                if (buflist->mBuffers[j].mNumberChannels > 0) {
                    usable = 1;
                    break;
                }
            }
        }

        SDL_free(buflist);

        if (!usable)
            continue;


        size = sizeof (CFStringRef);
        result = AudioObjectGetPropertyData(dev, &nameaddr, 0, NULL, &size, &cfstr);
        if (result != kAudioHardwareNoError)
            continue;

        len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
                                                kCFStringEncodingUTF8);

        ptr = (char *) SDL_malloc(len + 1);
        usable = ((ptr != NULL) &&
                  (CFStringGetCString
                   (cfstr, ptr, len + 1, kCFStringEncodingUTF8)));

        CFRelease(cfstr);

        if (usable) {
            len = strlen(ptr);
            /* Some devices have whitespace at the end...trim it. */
            while ((len > 0) && (ptr[len - 1] == ' ')) {
                len--;
            }
            usable = (len > 0);
        }

        if (usable) {
            ptr[len] = '\0';

#if DEBUG_COREAUDIO
            printf("COREAUDIO: Found %s device #%d: '%s' (devid %d)\n",
                   ((iscapture) ? "capture" : "output"),
                   (int) i, ptr, (int) dev);
#endif
            addfn(ptr, iscapture, dev, addfndata);
        }
        SDL_free(ptr);  /* addfn() would have copied the string. */
    }
}

static void
free_audio_device_list(AudioDeviceList **list)
{
    AudioDeviceList *item = *list;
    while (item) {
        AudioDeviceList *next = item->next;
        SDL_free(item);
        item = next;
    }
    *list = NULL;
}

static void
COREAUDIO_DetectDevices(void)
{
    build_device_list(SDL_TRUE, addToDevList, NULL);
    build_device_list(SDL_FALSE, addToDevList, NULL);
}

static void
build_device_change_list(const char *name, const int iscapture, AudioDeviceID devId, void *data)
{
    AudioDeviceList **list = (AudioDeviceList **) data;
    AudioDeviceList *item;
    for (item = *list; item != NULL; item = item->next) {
        if (item->devid == devId) {
            item->alive = SDL_TRUE;
            return;
        }
    }

    add_to_internal_dev_list(iscapture, devId);  /* new device, add it. */
    SDL_AddAudioDevice(iscapture, name, (void *) ((size_t) devId));
}

static void
reprocess_device_list(const int iscapture, AudioDeviceList **list)
{
    AudioDeviceList *item;
    AudioDeviceList *prev = NULL;
    for (item = *list; item != NULL; item = item->next) {
        item->alive = SDL_FALSE;
    }

    build_device_list(iscapture, build_device_change_list, list);

    /* free items in the list that aren't still alive. */
    item = *list;
    while (item != NULL) {
        AudioDeviceList *next = item->next;
        if (item->alive) {
            prev = item;
        } else {
            SDL_RemoveAudioDevice(iscapture, (void *) ((size_t) item->devid));
            if (prev) {
                prev->next = item->next;
            } else {
                *list = item->next;
            }
            SDL_free(item);
        }
        item = next;
    }
}

/* this is called when the system's list of available audio devices changes. */
static OSStatus
device_list_changed(AudioObjectID systemObj, UInt32 num_addr, const AudioObjectPropertyAddress *addrs, void *data)
{
    reprocess_device_list(SDL_TRUE, &capture_devs);
    reprocess_device_list(SDL_FALSE, &output_devs);
    return 0;
}
#endif


static int open_playback_devices = 0;
static int open_capture_devices = 0;

static void update_audio_session()
{
#if !MACOSX_COREAUDIO
    /* !!! FIXME: move this to AVAudioSession. This is deprecated, and the new version is available as of (ancient!) iOS 3.0 */
    UInt32 category;
    if (open_playback_devices && open_capture_devices) {
        category = kAudioSessionCategory_PlayAndRecord;
    } else if (open_capture_devices) {
        category = kAudioSessionCategory_RecordAudio;
    } else {  /* nothing open, or just playing audio. */
        category = kAudioSessionCategory_AmbientSound;
    }
    AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof (UInt32), &category);
#endif
}


/* The CoreAudio callback */
static OSStatus
outputCallback(void *inRefCon,
               AudioUnitRenderActionFlags * ioActionFlags,
               const AudioTimeStamp * inTimeStamp,
               UInt32 inBusNumber, UInt32 inNumberFrames,
               AudioBufferList * ioData)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *) inRefCon;
    AudioBuffer *abuf;
    UInt32 remaining, len;
    void *ptr;
    UInt32 i;

    /* Only do anything if audio is enabled and not paused */
    if (!SDL_AtomicGet(&this->enabled) || SDL_AtomicGet(&this->paused)) {
        for (i = 0; i < ioData->mNumberBuffers; i++) {
            abuf = &ioData->mBuffers[i];
            SDL_memset(abuf->mData, this->spec.silence, abuf->mDataByteSize);
        }
        return 0;
    }

    /* No SDL conversion should be needed here, ever, since we accept
       any input format in OpenAudio, and leave the conversion to CoreAudio.
     */
    /*
       SDL_assert(!this->convert.needed);
       SDL_assert(this->spec.channels == ioData->mNumberChannels);
     */

    for (i = 0; i < ioData->mNumberBuffers; i++) {
        abuf = &ioData->mBuffers[i];
        remaining = abuf->mDataByteSize;
        ptr = abuf->mData;
        while (remaining > 0) {
            if (this->hidden->bufferOffset >= this->hidden->bufferSize) {
                /* Generate the data */
                SDL_LockMutex(this->mixer_lock);
                (*this->spec.callback)(this->spec.userdata,
                            this->hidden->buffer, this->hidden->bufferSize);
                SDL_UnlockMutex(this->mixer_lock);
                this->hidden->bufferOffset = 0;
            }

            len = this->hidden->bufferSize - this->hidden->bufferOffset;
            if (len > remaining)
                len = remaining;
            SDL_memcpy(ptr, (char *)this->hidden->buffer +
                       this->hidden->bufferOffset, len);
            ptr = (char *)ptr + len;
            remaining -= len;
            this->hidden->bufferOffset += len;
        }
    }

    return noErr;
}

static OSStatus
inputCallback(void *inRefCon,
              AudioUnitRenderActionFlags *ioActionFlags,
              const AudioTimeStamp *inTimeStamp,
              UInt32 inBusNumber, UInt32 inNumberFrames,
              AudioBufferList *ioData)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *) inRefCon;
    if (!SDL_AtomicGet(&this->enabled) || SDL_AtomicGet(&this->paused)) {
        return noErr;  /* just drop this if we're not accepting input. */
    }

    const OSStatus err = AudioUnitRender(this->hidden->audioUnit, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, &this->hidden->captureBufferList);
    SDL_assert(this->hidden->captureBufferList.mNumberBuffers == 1);

    if (err == noErr) {
        const AudioBuffer *abuf = &this->hidden->captureBufferList.mBuffers[0];
        UInt32 remaining = abuf->mDataByteSize;
        const Uint8 *ptr = (const Uint8 *) abuf->mData;

        /* No SDL conversion should be needed here, ever, since we accept
           any input format in OpenAudio, and leave the conversion to CoreAudio.
         */
        while (remaining > 0) {
            UInt32 len = this->hidden->bufferSize - this->hidden->bufferOffset;
            if (len > remaining)
                len = remaining;

            /* !!! FIXME: why are we copying here? just pass the buffer to the callback? */
            SDL_memcpy((char *)this->hidden->buffer + this->hidden->bufferOffset, ptr, len);
            ptr += len;
            remaining -= len;
            this->hidden->bufferOffset += len;

            if (this->hidden->bufferOffset >= this->hidden->bufferSize) {
                SDL_LockMutex(this->mixer_lock);
                (*this->spec.callback)(this->spec.userdata,
                            this->hidden->buffer, this->hidden->bufferSize);
                SDL_UnlockMutex(this->mixer_lock);
                this->hidden->bufferOffset = 0;
            }
        }
    }

    return noErr;
}


#if MACOSX_COREAUDIO
static const AudioObjectPropertyAddress alive_address =
{
    kAudioDevicePropertyDeviceIsAlive,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static OSStatus
device_unplugged(AudioObjectID devid, UInt32 num_addr, const AudioObjectPropertyAddress *addrs, void *data)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *) data;
    SDL_bool dead = SDL_FALSE;
    UInt32 isAlive = 1;
    UInt32 size = sizeof (isAlive);
    OSStatus error;

    if (!SDL_AtomicGet(&this->enabled)) {
        return 0;  /* already known to be dead. */
    }

    error = AudioObjectGetPropertyData(this->hidden->deviceID, &alive_address,
                                       0, NULL, &size, &isAlive);

    if (error == kAudioHardwareBadDeviceError) {
        dead = SDL_TRUE;  /* device was unplugged. */
    } else if ((error == kAudioHardwareNoError) && (!isAlive)) {
        dead = SDL_TRUE;  /* device died in some other way. */
    }

    if (dead) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }

    return 0;
}
#endif

static void
COREAUDIO_CloseDevice(_THIS)
{
    const int iscapture = this->iscapture;
    if (this->hidden->audioUnitOpened) {
        #if MACOSX_COREAUDIO
        /* Unregister our disconnect callback. */
        AudioObjectRemovePropertyListener(this->hidden->deviceID, &alive_address, device_unplugged, this);
        #endif

        AURenderCallbackStruct callback;
        const AudioUnitElement output_bus = 0;
        const AudioUnitElement input_bus = 1;
        const AudioUnitElement bus = ((iscapture) ? input_bus : output_bus);

        /* stop processing the audio unit */
        AudioOutputUnitStop(this->hidden->audioUnit);

        /* Remove the input callback */
        SDL_zero(callback);
        AudioUnitSetProperty(this->hidden->audioUnit,
                             iscapture ? kAudioOutputUnitProperty_SetInputCallback : kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Global, bus, &callback, sizeof(callback));
        AudioComponentInstanceDispose(this->hidden->audioUnit);
    }

    SDL_free(this->hidden->captureBufferList.mBuffers[0].mData);
    SDL_free(this->hidden->buffer);
    SDL_free(this->hidden);

    if (iscapture) {
        open_capture_devices--;
    } else {
        open_playback_devices--;
    }
    update_audio_session();
}

#if MACOSX_COREAUDIO
static int
prepare_device(_THIS, void *handle, int iscapture)
{
    AudioDeviceID devid = (AudioDeviceID) ((size_t) handle);
    OSStatus result = noErr;
    UInt32 size = 0;
    UInt32 alive = 0;
    pid_t pid = 0;

    AudioObjectPropertyAddress addr = {
        0,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    if (handle == NULL) {
        size = sizeof (AudioDeviceID);
        addr.mSelector =
            ((iscapture) ? kAudioHardwarePropertyDefaultInputDevice :
            kAudioHardwarePropertyDefaultOutputDevice);
        result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                            0, NULL, &size, &devid);
        CHECK_RESULT("AudioHardwareGetProperty (default device)");
    }

    addr.mSelector = kAudioDevicePropertyDeviceIsAlive;
    addr.mScope = iscapture ? kAudioDevicePropertyScopeInput :
                    kAudioDevicePropertyScopeOutput;

    size = sizeof (alive);
    result = AudioObjectGetPropertyData(devid, &addr, 0, NULL, &size, &alive);
    CHECK_RESULT
        ("AudioDeviceGetProperty (kAudioDevicePropertyDeviceIsAlive)");

    if (!alive) {
        SDL_SetError("CoreAudio: requested device exists, but isn't alive.");
        return 0;
    }

    addr.mSelector = kAudioDevicePropertyHogMode;
    size = sizeof (pid);
    result = AudioObjectGetPropertyData(devid, &addr, 0, NULL, &size, &pid);

    /* some devices don't support this property, so errors are fine here. */
    if ((result == noErr) && (pid != -1)) {
        SDL_SetError("CoreAudio: requested device is being hogged.");
        return 0;
    }

    this->hidden->deviceID = devid;
    return 1;
}
#endif

static int
prepare_audiounit(_THIS, void *handle, int iscapture,
                  const AudioStreamBasicDescription * strdesc)
{
    OSStatus result = noErr;
    AURenderCallbackStruct callback;
    AudioComponentDescription desc;
    AudioComponent comp = NULL;
    const AudioUnitElement output_bus = 0;
    const AudioUnitElement input_bus = 1;

#if MACOSX_COREAUDIO
    if (!prepare_device(this, handle, iscapture)) {
        return 0;
    }
#endif

    SDL_zero(desc);
    desc.componentType = kAudioUnitType_Output;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

#if MACOSX_COREAUDIO
    desc.componentSubType = iscapture ? kAudioUnitSubType_HALOutput : kAudioUnitSubType_DefaultOutput;
#else
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
    comp = AudioComponentFindNext(NULL, &desc);

    if (comp == NULL) {
        SDL_SetError("Couldn't find requested CoreAudio component");
        return 0;
    }

    /* Open & initialize the audio unit */
    result = AudioComponentInstanceNew(comp, &this->hidden->audioUnit);
    CHECK_RESULT("AudioComponentInstanceNew");

    this->hidden->audioUnitOpened = 1;

    if (iscapture) {  /* have to do EnableIO only for capture devices. */
        UInt32 enable = 1;
        result = AudioUnitSetProperty(this->hidden->audioUnit,
                                      kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Input, input_bus,
                                      &enable, sizeof (enable));
        CHECK_RESULT
            ("AudioUnitSetProperty (kAudioOutputUnitProperty_EnableIO input bus)");

        enable = 0;
        result = AudioUnitSetProperty(this->hidden->audioUnit,
                                      kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Output, output_bus,
                                      &enable, sizeof (enable));
        CHECK_RESULT
            ("AudioUnitSetProperty (kAudioOutputUnitProperty_EnableIO output bus)");
    }

#if MACOSX_COREAUDIO
    /* this is always on the output_bus, even for capture devices. */
    result = AudioUnitSetProperty(this->hidden->audioUnit,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global, output_bus,
                                  &this->hidden->deviceID,
                                  sizeof(AudioDeviceID));
    CHECK_RESULT
        ("AudioUnitSetProperty (kAudioOutputUnitProperty_CurrentDevice)");
#endif

    /* Set the data format of the audio unit. */
    result = AudioUnitSetProperty(this->hidden->audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  iscapture ? kAudioUnitScope_Output : kAudioUnitScope_Input,
                                  iscapture ? input_bus : output_bus,
                                  strdesc, sizeof (*strdesc));
    CHECK_RESULT("AudioUnitSetProperty (kAudioUnitProperty_StreamFormat)");

    if (iscapture) {  /* only need to do this for capture devices. */
        void *ptr;
        UInt32 framesize = 0;
        UInt32 propsize = sizeof (UInt32);

        result = AudioUnitGetProperty(this->hidden->audioUnit,
                                      kAudioUnitProperty_MaximumFramesPerSlice,
                                      kAudioUnitScope_Global, output_bus,
                                      &framesize, &propsize);
        CHECK_RESULT
            ("AudioUnitGetProperty (kAudioDevicePropertyBufferFrameSize)");

        framesize *= SDL_AUDIO_BITSIZE(this->spec.format) / 8;
        ptr = SDL_calloc(1, framesize);
        if (ptr == NULL) {
            SDL_OutOfMemory();
            return 0;
        }
        this->hidden->captureBufferList.mNumberBuffers = 1;
        this->hidden->captureBufferList.mBuffers[0].mNumberChannels = this->spec.channels;
        this->hidden->captureBufferList.mBuffers[0].mDataByteSize = framesize;
        this->hidden->captureBufferList.mBuffers[0].mData = ptr;
    }

    /* Set the audio callback */
    SDL_zero(callback);
    callback.inputProc = ((iscapture) ? inputCallback : outputCallback);
    callback.inputProcRefCon = this;

    result = AudioUnitSetProperty(this->hidden->audioUnit,
                                  iscapture ? kAudioOutputUnitProperty_SetInputCallback : kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  iscapture ? input_bus : output_bus,
                                  &callback, sizeof (callback));
    CHECK_RESULT
        ("AudioUnitSetProperty (kAudioUnitProperty_SetRenderCallback)");

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate a sample buffer */
    this->hidden->bufferSize = this->spec.size;
    this->hidden->bufferOffset = iscapture ? 0 : this->hidden->bufferSize;

    this->hidden->buffer = SDL_malloc(this->hidden->bufferSize);
    if (this->hidden->buffer == NULL) {
        SDL_OutOfMemory();
        return 0;
    }

    result = AudioUnitInitialize(this->hidden->audioUnit);
    CHECK_RESULT("AudioUnitInitialize");

    /* Finally, start processing of the audio unit */
    result = AudioOutputUnitStart(this->hidden->audioUnit);
    CHECK_RESULT("AudioOutputUnitStart");

/* !!! FIXME: what does iOS do when a bluetooth audio device vanishes? Headphones unplugged? */
/* !!! FIXME: (we only do a "default" device on iOS right now...can we do more?) */
#if MACOSX_COREAUDIO
    /* Fire a callback if the device stops being "alive" (disconnected, etc). */
    AudioObjectAddPropertyListener(this->hidden->deviceID, &alive_address, device_unplugged, this);
#endif

    /* We're running! */
    return 1;
}


static int
COREAUDIO_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    AudioStreamBasicDescription strdesc;
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    int valid_datatype = 0;

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(this->hidden);

    if (iscapture) {
        open_capture_devices++;
    } else {
        open_playback_devices++;
    }
    update_audio_session();

    /* Setup a AudioStreamBasicDescription with the requested format */
    SDL_zero(strdesc);
    strdesc.mFormatID = kAudioFormatLinearPCM;
    strdesc.mFormatFlags = kLinearPCMFormatFlagIsPacked;
    strdesc.mChannelsPerFrame = this->spec.channels;
    strdesc.mSampleRate = this->spec.freq;
    strdesc.mFramesPerPacket = 1;

    while ((!valid_datatype) && (test_format)) {
        this->spec.format = test_format;
        /* Just a list of valid SDL formats, so people don't pass junk here. */
        switch (test_format) {
        case AUDIO_U8:
        case AUDIO_S8:
        case AUDIO_U16LSB:
        case AUDIO_S16LSB:
        case AUDIO_U16MSB:
        case AUDIO_S16MSB:
        case AUDIO_S32LSB:
        case AUDIO_S32MSB:
        case AUDIO_F32LSB:
        case AUDIO_F32MSB:
            valid_datatype = 1;
            strdesc.mBitsPerChannel = SDL_AUDIO_BITSIZE(this->spec.format);
            if (SDL_AUDIO_ISBIGENDIAN(this->spec.format))
                strdesc.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;

            if (SDL_AUDIO_ISFLOAT(this->spec.format))
                strdesc.mFormatFlags |= kLinearPCMFormatFlagIsFloat;
            else if (SDL_AUDIO_ISSIGNED(this->spec.format))
                strdesc.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
            break;
        }
    }

    if (!valid_datatype) {      /* shouldn't happen, but just in case... */
        return SDL_SetError("Unsupported audio format");
    }

    strdesc.mBytesPerFrame =
        strdesc.mBitsPerChannel * strdesc.mChannelsPerFrame / 8;
    strdesc.mBytesPerPacket =
        strdesc.mBytesPerFrame * strdesc.mFramesPerPacket;

    if (!prepare_audiounit(this, handle, iscapture, &strdesc)) {
        return -1;      /* prepare_audiounit() will call SDL_SetError()... */
    }

    return 0;   /* good to go. */
}

static void
COREAUDIO_Deinitialize(void)
{
#if MACOSX_COREAUDIO
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &devlist_address, device_list_changed, NULL);
    free_audio_device_list(&capture_devs);
    free_audio_device_list(&output_devs);
#endif
}

static int
COREAUDIO_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = COREAUDIO_OpenDevice;
    impl->CloseDevice = COREAUDIO_CloseDevice;
    impl->Deinitialize = COREAUDIO_Deinitialize;

#if MACOSX_COREAUDIO
    impl->DetectDevices = COREAUDIO_DetectDevices;
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &devlist_address, device_list_changed, NULL);
#else
    impl->OnlyHasDefaultOutputDevice = 1;
    impl->OnlyHasDefaultCaptureDevice = 1;

    /* Set category to ambient sound so that other music continues playing.
       You can change this at runtime in your own code if you need different
       behavior.  If this is common, we can add an SDL hint for this.
    */
    /* !!! FIXME: move this to AVAudioSession. This is deprecated, and the new version is available as of (ancient!) iOS 3.0 */
    AudioSessionInitialize(NULL, NULL, NULL, nil);
    UInt32 category = kAudioSessionCategory_AmbientSound;
    AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(UInt32), &category);
#endif

    impl->ProvidesOwnCallbackThread = 1;
    impl->HasCaptureSupport = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap COREAUDIO_bootstrap = {
    "coreaudio", "CoreAudio", COREAUDIO_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_COREAUDIO */

/* vi: set ts=4 sw=4 expandtab: */
