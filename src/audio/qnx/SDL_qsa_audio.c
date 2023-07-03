/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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

/*
 * !!! FIXME: streamline this a little by removing all the
 * !!! FIXME:  if (capture) {} else {} sections that are identical
 * !!! FIXME:  except for one flag.
 */

/* !!! FIXME: can this target support hotplugging? */

#include "../../SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_QNX

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/select.h>
#include <sys/neutrino.h>
#include <sys/asoundlib.h>

#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_audio.h"
#include "../../core/unix/SDL_poll.h"
#include "../SDL_audio_c.h"
#include "SDL_qsa_audio.h"

/* default channel communication parameters */
#define DEFAULT_CPARAMS_RATE   44100
#define DEFAULT_CPARAMS_VOICES 1

#define DEFAULT_CPARAMS_FRAG_SIZE 4096
#define DEFAULT_CPARAMS_FRAGS_MIN 1
#define DEFAULT_CPARAMS_FRAGS_MAX 1

/* List of found devices */
#define QSA_MAX_DEVICES       32
#define QSA_MAX_NAME_LENGTH   81+16     /* Hardcoded in QSA, can't be changed */

typedef struct _QSA_Device
{
    char name[QSA_MAX_NAME_LENGTH];     /* Long audio device name for SDL  */
    int cardno;
    int deviceno;
} QSA_Device;

QSA_Device qsa_playback_device[QSA_MAX_DEVICES];
uint32_t qsa_playback_devices;

QSA_Device qsa_capture_device[QSA_MAX_DEVICES];
uint32_t qsa_capture_devices;

static int QSA_SetError(const char *fn, int status)
{
    return SDL_SetError("QSA: %s() failed: %s", fn, snd_strerror(status));
}

/* !!! FIXME: does this need to be here? Does the SDL version not work? */
static void QSA_ThreadInit(SDL_AudioDevice *_this)
{
    /* Increase default 10 priority to 25 to avoid jerky sound */
    struct sched_param param;
    if (SchedGet(0, 0, &param) != -1) {
        param.sched_priority = param.sched_curpriority + 15;
        SchedSet(0, 0, SCHED_NOCHANGE, &param);
    }
}

/* PCM channel parameters initialize function */
static void QSA_InitAudioParams(snd_pcm_channel_params_t * cpars)
{
    SDL_zerop(cpars);
    cpars->channel = SND_PCM_CHANNEL_PLAYBACK;
    cpars->mode = SND_PCM_MODE_BLOCK;
    cpars->start_mode = SND_PCM_START_DATA;
    cpars->stop_mode = SND_PCM_STOP_STOP;
    cpars->format.format = SND_PCM_SFMT_S16_LE;
    cpars->format.interleave = 1;
    cpars->format.rate = DEFAULT_CPARAMS_RATE;
    cpars->format.voices = DEFAULT_CPARAMS_VOICES;
    cpars->buf.block.frag_size = DEFAULT_CPARAMS_FRAG_SIZE;
    cpars->buf.block.frags_min = DEFAULT_CPARAMS_FRAGS_MIN;
    cpars->buf.block.frags_max = DEFAULT_CPARAMS_FRAGS_MAX;
}

/* This function waits until it is possible to write a full sound buffer */
static void QSA_WaitDevice(SDL_AudioDevice *_this)
{
    int result;

    /* Setup timeout for playing one fragment equal to 2 seconds          */
    /* If timeout occurred than something wrong with hardware or driver   */
    /* For example, Vortex 8820 audio driver stucks on second DAC because */
    /* it doesn't exist !                                                 */
    result = SDL_IOReady(_this->hidden->audio_fd,
                         _this->hidden->iscapture ? SDL_IOR_READ : SDL_IOR_WRITE,
                         2 * 1000);
    switch (result) {
    case -1:
        SDL_SetError("QSA: SDL_IOReady() failed: %s", strerror(errno));
        break;
    case 0:
        SDL_SetError("QSA: timeout on buffer waiting occurred");
        _this->hidden->timeout_on_wait = 1;
        break;
    default:
        _this->hidden->timeout_on_wait = 0;
        break;
    }
}

static void QSA_PlayDevice(SDL_AudioDevice *_this)
{
    snd_pcm_channel_status_t cstatus;
    int written;
    int status;
    int towrite;
    void *pcmbuffer;

    if (!SDL_AtomicGet(&_this->enabled) || !_this->hidden) {
        return;
    }

    towrite = _this->spec.size;
    pcmbuffer = _this->hidden->pcm_buf;

    /* Write the audio data, checking for EAGAIN (buffer full) and underrun */
    do {
        written =
            snd_pcm_plugin_write(_this->hidden->audio_handle, pcmbuffer,
                                 towrite);
        if (written != towrite) {
            /* Check if samples playback got stuck somewhere in hardware or in */
            /* the audio device driver */
            if ((errno == EAGAIN) && (written == 0)) {
                if (_this->hidden->timeout_on_wait != 0) {
                    SDL_SetError("QSA: buffer playback timeout");
                    return;
                }
            }

            /* Check for errors or conditions */
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                /* Let a little CPU time go by and try to write again */
                SDL_Delay(1);

                /* if we wrote some data */
                towrite -= written;
                pcmbuffer += written * _this->spec.channels;
                continue;
            } else {
                if ((errno == EINVAL) || (errno == EIO)) {
                    SDL_zero(cstatus);
                    if (!_this->hidden->iscapture) {
                        cstatus.channel = SND_PCM_CHANNEL_PLAYBACK;
                    } else {
                        cstatus.channel = SND_PCM_CHANNEL_CAPTURE;
                    }

                    status =
                        snd_pcm_plugin_status(_this->hidden->audio_handle,
                                              &cstatus);
                    if (status < 0) {
                        QSA_SetError("snd_pcm_plugin_status", status);
                        return;
                    }

                    if ((cstatus.status == SND_PCM_STATUS_UNDERRUN) ||
                        (cstatus.status == SND_PCM_STATUS_READY)) {
                        if (!_this->hidden->iscapture) {
                            status =
                                snd_pcm_plugin_prepare(_this->hidden->
                                                       audio_handle,
                                                       SND_PCM_CHANNEL_PLAYBACK);
                        } else {
                            status =
                                snd_pcm_plugin_prepare(_this->hidden->
                                                       audio_handle,
                                                       SND_PCM_CHANNEL_CAPTURE);
                        }
                        if (status < 0) {
                            QSA_SetError("snd_pcm_plugin_prepare", status);
                            return;
                        }
                    }
                    continue;
                } else {
                    return;
                }
            }
        } else {
            /* we wrote all remaining data */
            towrite -= written;
            pcmbuffer += written * _this->spec.channels;
        }
    } while ((towrite > 0) && SDL_AtomicGet(&_this->enabled));

    /* If we couldn't write, assume fatal error for now */
    if (towrite != 0) {
        SDL_OpenedAudioDeviceDisconnected(_this);
    }
}

static Uint8 *QSA_GetDeviceBuf(SDL_AudioDevice *_this)
{
    return _this->hidden->pcm_buf;
}

static void QSA_CloseDevice(SDL_AudioDevice *_this)
{
    if (_this->hidden->audio_handle != NULL) {
#if _NTO_VERSION < 710
        if (!_this->hidden->iscapture) {
            /* Finish playing available samples */
            snd_pcm_plugin_flush(_this->hidden->audio_handle,
                                 SND_PCM_CHANNEL_PLAYBACK);
        } else {
            /* Cancel unread samples during capture */
            snd_pcm_plugin_flush(_this->hidden->audio_handle,
                                 SND_PCM_CHANNEL_CAPTURE);
        }
#endif
        snd_pcm_close(_this->hidden->audio_handle);
    }

    SDL_free(_this->hidden->pcm_buf);
    SDL_free(_this->hidden);
}

static int QSA_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
#if 0
    /* !!! FIXME: SDL2 used to pass this handle. What's the alternative? */
    const QSA_Device *device = (const QSA_Device *) handle;
#else
    const QSA_Device *device = NULL;
#endif
    int status = 0;
    int format = 0;
    SDL_AudioFormat test_format = 0;
    const SDL_AudioFormat *closefmts;
    snd_pcm_channel_setup_t csetup;
    snd_pcm_channel_params_t cparams;
    SDL_bool iscapture = _this->iscapture;

    /* Initialize all variables that we clean on shutdown */
    _this->hidden =
        (struct SDL_PrivateAudioData *) SDL_calloc(1,
                                                   (sizeof
                                                    (struct
                                                     SDL_PrivateAudioData)));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    /* Initialize channel transfer parameters to default */
    QSA_InitAudioParams(&cparams);

    /* Initialize channel direction: capture or playback */
    _this->hidden->iscapture = iscapture ? SDL_TRUE : SDL_FALSE;

    if (device != NULL) {
        /* Open requested audio device */
        _this->hidden->deviceno = device->deviceno;
        _this->hidden->cardno = device->cardno;
        status = snd_pcm_open(&_this->hidden->audio_handle,
                              device->cardno, device->deviceno,
                              iscapture ? SND_PCM_OPEN_CAPTURE : SND_PCM_OPEN_PLAYBACK);
    } else {
        /* Open system default audio device */
        status = snd_pcm_open_preferred(&_this->hidden->audio_handle,
                                        &_this->hidden->cardno,
                                        &_this->hidden->deviceno,
                                        iscapture ? SND_PCM_OPEN_CAPTURE : SND_PCM_OPEN_PLAYBACK);
    }

    /* Check if requested device is opened */
    if (status < 0) {
        _this->hidden->audio_handle = NULL;
        return QSA_SetError("snd_pcm_open", status);
    }

    /* Try for a closest match on audio format */
    closefmts = SDL_ClosestAudioFormats(_this->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        /* if match found set format to equivalent QSA format */
        switch (test_format) {
        #define CHECKFMT(sdlfmt, qsafmt) case SDL_AUDIO_##sdlfmt: format = SND_PCM_SFMT_##qsafmt; break
        CHECKFMT(U8, U8);
        CHECKFMT(S8, S8);
        CHECKFMT(S16LSB, S16_LE);
        CHECKFMT(S16MSB, S16_BE);
        CHECKFMT(S32LSB, S32_LE);
        CHECKFMT(S32MSB, S32_BE);
        CHECKFMT(F32LSB, FLOAT_LE);
        CHECKFMT(F32MSB, FLOAT_BE);
        #undef CHECKFMT
        default: continue;
        }
        break;
    }

    /* assumes test_format not 0 on success */
    if (test_format == 0) {
        return SDL_SetError("QSA: Couldn't find any hardware audio formats");
    }

    _this->spec.format = test_format;

    /* Set the audio format */
    cparams.format.format = format;

    /* Set mono/stereo/4ch/6ch/8ch audio */
    cparams.format.voices = _this->spec.channels;

    /* Set rate */
    cparams.format.rate = _this->spec.freq;

    /* Setup the transfer parameters according to cparams */
    status = snd_pcm_plugin_params(_this->hidden->audio_handle, &cparams);
    if (status < 0) {
        return QSA_SetError("snd_pcm_plugin_params", status);
    }

    /* Make sure channel is setup right one last time */
    SDL_zero(csetup);
    if (!_this->hidden->iscapture) {
        csetup.channel = SND_PCM_CHANNEL_PLAYBACK;
    } else {
        csetup.channel = SND_PCM_CHANNEL_CAPTURE;
    }

    /* Setup an audio channel */
    if (snd_pcm_plugin_setup(_this->hidden->audio_handle, &csetup) < 0) {
        return SDL_SetError("QSA: Unable to setup channel");
    }

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&_this->spec);

    _this->hidden->pcm_len = _this->spec.size;

    if (_this->hidden->pcm_len == 0) {
        _this->hidden->pcm_len =
            csetup.buf.block.frag_size * _this->spec.channels *
            (snd_pcm_format_width(format) / 8);
    }

    /*
     * Allocate memory to the audio buffer and initialize with silence
     *  (Note that buffer size must be a multiple of fragment size, so find
     *  closest multiple)
     */
    _this->hidden->pcm_buf =
        (Uint8 *) SDL_malloc(_this->hidden->pcm_len);
    if (_this->hidden->pcm_buf == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(_this->hidden->pcm_buf, _this->spec.silence,
               _this->hidden->pcm_len);

    /* get the file descriptor */
    if (!_this->hidden->iscapture) {
        _this->hidden->audio_fd =
            snd_pcm_file_descriptor(_this->hidden->audio_handle,
                                    SND_PCM_CHANNEL_PLAYBACK);
    } else {
        _this->hidden->audio_fd =
            snd_pcm_file_descriptor(_this->hidden->audio_handle,
                                    SND_PCM_CHANNEL_CAPTURE);
    }

    if (_this->hidden->audio_fd < 0) {
        return QSA_SetError("snd_pcm_file_descriptor", status);
    }

    /* Prepare an audio channel */
    if (!_this->hidden->iscapture) {
        /* Prepare audio playback */
        status =
            snd_pcm_plugin_prepare(_this->hidden->audio_handle,
                                   SND_PCM_CHANNEL_PLAYBACK);
    } else {
        /* Prepare audio capture */
        status =
            snd_pcm_plugin_prepare(_this->hidden->audio_handle,
                                   SND_PCM_CHANNEL_CAPTURE);
    }

    if (status < 0) {
        return QSA_SetError("snd_pcm_plugin_prepare", status);
    }

    /* We're really ready to rock and roll. :-) */
    return 0;
}

static void QSA_DetectDevices(void)
{
    uint32_t it;
    uint32_t cards;
    uint32_t devices;
    int32_t status;

    /* Detect amount of available devices       */
    /* this value can be changed in the runtime */
    cards = snd_cards();

    /* If io-audio manager is not running we will get 0 as number */
    /* of available audio devices                                 */
    if (cards == 0) {
        /* We have no any available audio devices */
        return;
    }

    /* !!! FIXME: code duplication */
    /* Find requested devices by type */
    {  /* output devices */
        /* Playback devices enumeration requested */
        for (it = 0; it < cards; it++) {
            devices = 0;
            do {
                status =
                    snd_card_get_longname(it,
                                          qsa_playback_device
                                          [qsa_playback_devices].name,
                                          QSA_MAX_NAME_LENGTH);
                if (status == EOK) {
                    snd_pcm_t *handle;

                    /* Add device number to device name */
                    sprintf(qsa_playback_device[qsa_playback_devices].name +
                            SDL_strlen(qsa_playback_device
                                       [qsa_playback_devices].name), " d%d",
                            devices);

                    /* Store associated card number id */
                    qsa_playback_device[qsa_playback_devices].cardno = it;

                    /* Check if this device id could play anything */
                    status =
                        snd_pcm_open(&handle, it, devices,
                                     SND_PCM_OPEN_PLAYBACK);
                    if (status == EOK) {
                        qsa_playback_device[qsa_playback_devices].deviceno =
                            devices;
                        status = snd_pcm_close(handle);
                        if (status == EOK) {
                            /* Note that spec is NULL, because we are required to open the device before
                             * acquiring the mix format, making this information inaccessible at
                             * enumeration time
                             */
                            SDL_AddAudioDevice(SDL_FALSE, qsa_playback_device[qsa_playback_devices].name, NULL, &qsa_playback_device[qsa_playback_devices]);
                            qsa_playback_devices++;
                        }
                    } else {
                        /* Check if we got end of devices list */
                        if (status == -ENOENT) {
                            break;
                        }
                    }
                } else {
                    break;
                }

                /* Check if we reached maximum devices count */
                if (qsa_playback_devices >= QSA_MAX_DEVICES) {
                    break;
                }
                devices++;
            } while (1);

            /* Check if we reached maximum devices count */
            if (qsa_playback_devices >= QSA_MAX_DEVICES) {
                break;
            }
        }
    }

    {  /* capture devices */
        /* Capture devices enumeration requested */
        for (it = 0; it < cards; it++) {
            devices = 0;
            do {
                status =
                    snd_card_get_longname(it,
                                          qsa_capture_device
                                          [qsa_capture_devices].name,
                                          QSA_MAX_NAME_LENGTH);
                if (status == EOK) {
                    snd_pcm_t *handle;

                    /* Add device number to device name */
                    sprintf(qsa_capture_device[qsa_capture_devices].name +
                            SDL_strlen(qsa_capture_device
                                       [qsa_capture_devices].name), " d%d",
                            devices);

                    /* Store associated card number id */
                    qsa_capture_device[qsa_capture_devices].cardno = it;

                    /* Check if this device id could play anything */
                    status =
                        snd_pcm_open(&handle, it, devices,
                                     SND_PCM_OPEN_CAPTURE);
                    if (status == EOK) {
                        qsa_capture_device[qsa_capture_devices].deviceno =
                            devices;
                        status = snd_pcm_close(handle);
                        if (status == EOK) {
                            /* Note that spec is NULL, because we are required to open the device before
                             * acquiring the mix format, making this information inaccessible at
                             * enumeration time
                             */
                            SDL_AddAudioDevice(SDL_TRUE, qsa_capture_device[qsa_capture_devices].name, NULL, &qsa_capture_device[qsa_capture_devices]);
                            qsa_capture_devices++;
                        }
                    } else {
                        /* Check if we got end of devices list */
                        if (status == -ENOENT) {
                            break;
                        }
                    }

                    /* Check if we reached maximum devices count */
                    if (qsa_capture_devices >= QSA_MAX_DEVICES) {
                        break;
                    }
                } else {
                    break;
                }
                devices++;
            } while (1);

            /* Check if we reached maximum devices count */
            if (qsa_capture_devices >= QSA_MAX_DEVICES) {
                break;
            }
        }
    }
}

static void QSA_Deinitialize(void)
{
    /* Clear devices array on shutdown */
    /* !!! FIXME: we zero these on init...any reason to do it here? */
    SDL_zeroa(qsa_playback_device);
    SDL_zeroa(qsa_capture_device);
    qsa_playback_devices = 0;
    qsa_capture_devices = 0;
}

static SDL_bool QSA_Init(SDL_AudioDriverImpl * impl)
{
    /* Clear devices array */
    SDL_zeroa(qsa_playback_device);
    SDL_zeroa(qsa_capture_device);
    qsa_playback_devices = 0;
    qsa_capture_devices = 0;

    /* Set function pointers                                     */
    /* DeviceLock and DeviceUnlock functions are used default,   */
    /* provided by SDL, which uses pthread_mutex for lock/unlock */
    impl->DetectDevices = QSA_DetectDevices;
    impl->OpenDevice = QSA_OpenDevice;
    impl->ThreadInit = QSA_ThreadInit;
    impl->WaitDevice = QSA_WaitDevice;
    impl->PlayDevice = QSA_PlayDevice;
    impl->GetDeviceBuf = QSA_GetDeviceBuf;
    impl->CloseDevice = QSA_CloseDevice;
    impl->Deinitialize = QSA_Deinitialize;
    impl->LockDevice = NULL;
    impl->UnlockDevice = NULL;

    impl->ProvidesOwnCallbackThread = 0;
    impl->HasCaptureSupport = 1;
    impl->OnlyHasDefaultOutputDevice = 0;
    impl->OnlyHasDefaultCaptureDevice = 0;

    return SDL_TRUE;   /* this audio target is available. */
}

AudioBootStrap QSAAUDIO_bootstrap = {
    "qsa", "QNX QSA Audio", QSA_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_QNX */

/* vi: set ts=4 sw=4 expandtab: */
