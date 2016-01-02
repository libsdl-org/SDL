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

#if SDL_AUDIO_DRIVER_EMSCRIPTEN

#include "SDL_audio.h"
#include "SDL_log.h"
#include "../SDL_audio_c.h"
#include "SDL_emscriptenaudio.h"

#include <emscripten/emscripten.h>

static int
copyData(_THIS)
{
    int byte_len;

    if (this->hidden->write_off + this->convert.len_cvt > this->hidden->mixlen) {
        if (this->hidden->write_off > this->hidden->read_off) {
            SDL_memmove(this->hidden->mixbuf,
                        this->hidden->mixbuf + this->hidden->read_off,
                        this->hidden->mixlen - this->hidden->read_off);
            this->hidden->write_off = this->hidden->write_off - this->hidden->read_off;
        } else {
            this->hidden->write_off = 0;
        }
        this->hidden->read_off = 0;
    }

    SDL_memcpy(this->hidden->mixbuf + this->hidden->write_off,
               this->convert.buf,
               this->convert.len_cvt);
    this->hidden->write_off += this->convert.len_cvt;
    byte_len = this->hidden->write_off - this->hidden->read_off;

    return byte_len;
}

static void
HandleAudioProcess(_THIS)
{
    Uint8 *buf = NULL;
    int byte_len = 0;
    int bytes = SDL_AUDIO_BITSIZE(this->spec.format) / 8;
    int bytes_in = SDL_AUDIO_BITSIZE(this->convert.src_format) / 8;

    /* Only do soemthing if audio is enabled */
    if (!this->enabled)
        return;

    if (this->paused)
        return;

    if (this->convert.needed) {
        if (this->hidden->conv_in_len != 0) {
            this->convert.len = this->hidden->conv_in_len * bytes_in * this->spec.channels;
        }

        (*this->spec.callback) (this->spec.userdata,
                                 this->convert.buf,
                                 this->convert.len);
        SDL_ConvertAudio(&this->convert);
        buf = this->convert.buf;
        byte_len = this->convert.len_cvt;

        /* size mismatch*/
        if (byte_len != this->spec.size) {
            if (!this->hidden->mixbuf) {
                this->hidden->mixlen = this->spec.size > byte_len ? this->spec.size * 2 : byte_len * 2;
                this->hidden->mixbuf = SDL_malloc(this->hidden->mixlen);
            }

            /* copy existing data */
            byte_len = copyData(this);

            /* read more data*/
            while (byte_len < this->spec.size) {
                (*this->spec.callback) (this->spec.userdata,
                                         this->convert.buf,
                                         this->convert.len);
                SDL_ConvertAudio(&this->convert);
                byte_len = copyData(this);
            }

            byte_len = this->spec.size;
            buf = this->hidden->mixbuf + this->hidden->read_off;
            this->hidden->read_off += byte_len;
        }

    } else {
        if (!this->hidden->mixbuf) {
            this->hidden->mixlen = this->spec.size;
            this->hidden->mixbuf = SDL_malloc(this->hidden->mixlen);
        }
        (*this->spec.callback) (this->spec.userdata,
                                 this->hidden->mixbuf,
                                 this->hidden->mixlen);
        buf = this->hidden->mixbuf;
        byte_len = this->hidden->mixlen;
    }

    if (buf) {
        EM_ASM_ARGS({
            var numChannels = SDL2.audio.currentOutputBuffer['numberOfChannels'];
            for (var c = 0; c < numChannels; ++c) {
                var channelData = SDL2.audio.currentOutputBuffer['getChannelData'](c);
                if (channelData.length != $1) {
                    throw 'Web Audio output buffer length mismatch! Destination size: ' + channelData.length + ' samples vs expected ' + $1 + ' samples!';
                }

                for (var j = 0; j < $1; ++j) {
                    channelData[j] = getValue($0 + (j*numChannels + c)*4, 'float');
                }
            }
        }, buf, byte_len / bytes / this->spec.channels);
    }
}

static void
Emscripten_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        if (this->hidden->mixbuf != NULL) {
            /* Clean up the audio buffer */
            SDL_free(this->hidden->mixbuf);
            this->hidden->mixbuf = NULL;
        }

        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}

static int
Emscripten_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    SDL_bool valid_format = SDL_FALSE;
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    int i;
    float f;
    int result;

    while ((!valid_format) && (test_format)) {
        switch (test_format) {
        case AUDIO_F32: /* web audio only supports floats */
            this->spec.format = test_format;

            valid_format = SDL_TRUE;
            break;
        }
        test_format = SDL_NextAudioFormat();
    }

    if (!valid_format) {
        /* Didn't find a compatible format :( */
        return SDL_SetError("No compatible audio format!");
    }

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));

    /* based on parts of library_sdl.js */

    /* create context (TODO: this puts stuff in the global namespace...)*/
    result = EM_ASM_INT_V({
        if(typeof(SDL2) === 'undefined')
            SDL2 = {};

        if(typeof(SDL2.audio) === 'undefined')
            SDL2.audio = {};

        if (!SDL2.audioContext) {
            if (typeof(AudioContext) !== 'undefined') {
                SDL2.audioContext = new AudioContext();
            } else if (typeof(webkitAudioContext) !== 'undefined') {
                SDL2.audioContext = new webkitAudioContext();
            } else {
                return -1;
            }
        }
        return 0;
    });
    if (result < 0) {
        return SDL_SetError("Web Audio API is not available!");
    }

    /* limit to native freq */
    int sampleRate = EM_ASM_INT_V({
        return SDL2.audioContext['sampleRate'];
    });

    if(this->spec.freq != sampleRate) {
        for (i = this->spec.samples; i > 0; i--) {
            f = (float)i / (float)sampleRate * (float)this->spec.freq;
            if (SDL_floor(f) == f) {
                this->hidden->conv_in_len = SDL_floor(f);
                break;
            }
        }

        this->spec.freq = sampleRate;
    }

    SDL_CalculateAudioSpec(&this->spec);

    /* setup a ScriptProcessorNode */
    EM_ASM_ARGS({
        SDL2.audio.scriptProcessorNode = SDL2.audioContext['createScriptProcessor']($1, 0, $0);
        SDL2.audio.scriptProcessorNode['onaudioprocess'] = function (e) {
            SDL2.audio.currentOutputBuffer = e['outputBuffer'];
            Runtime.dynCall('vi', $2, [$3]);
        };
        SDL2.audio.scriptProcessorNode['connect'](SDL2.audioContext['destination']);
    }, this->spec.channels, this->spec.samples, HandleAudioProcess, this);
    return 0;
}

static int
Emscripten_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = Emscripten_OpenDevice;
    impl->CloseDevice = Emscripten_CloseDevice;

    /* only one output */
    impl->OnlyHasDefaultOutputDevice = 1;

    /* no threads here */
    impl->SkipMixerLock = 1;
    impl->ProvidesOwnCallbackThread = 1;

    /* check availability */
    int available = EM_ASM_INT_V({
        if (typeof(AudioContext) !== 'undefined') {
            return 1;
        } else if (typeof(webkitAudioContext) !== 'undefined') {
            return 1;
        }
        return 0;
    });

    if (!available) {
        SDL_SetError("No audio context available");
    }

    return available;
}

AudioBootStrap EmscriptenAudio_bootstrap = {
    "emscripten", "SDL emscripten audio driver", Emscripten_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_EMSCRIPTEN */

/* vi: set ts=4 sw=4 expandtab: */
