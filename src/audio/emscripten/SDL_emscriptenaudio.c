/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_AUDIO_DRIVER_EMSCRIPTEN

#include "../SDL_sysaudio.h"
#include "SDL_emscriptenaudio.h"

#include <emscripten/emscripten.h>

// just turn off clang-format for this whole file, this INDENT_OFF stuff on
//  each EM_ASM section is ugly.
/* *INDENT-OFF* */ /* clang-format off */

static Uint8 *EMSCRIPTENAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return device->hidden->mixbuf;
}

static int EMSCRIPTENAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size)
{
    const int framelen = SDL_AUDIO_FRAMESIZE(device->spec);
    MAIN_THREAD_EM_ASM({
        var SDL3 = Module['SDL3'];
        var numChannels = SDL3.audio.currentOutputBuffer['numberOfChannels'];
        for (var c = 0; c < numChannels; ++c) {
            var channelData = SDL3.audio.currentOutputBuffer['getChannelData'](c);
            if (channelData.length != $1) {
                throw 'Web Audio output buffer length mismatch! Destination size: ' + channelData.length + ' samples vs expected ' + $1 + ' samples!';
            }

            for (var j = 0; j < $1; ++j) {
                channelData[j] = HEAPF32[$0 + ((j*numChannels + c) << 2) >> 2];  // !!! FIXME: why are these shifts here?
            }
        }
    }, buffer, buffer_size / framelen);
    return 0;
}


static void EMSCRIPTENAUDIO_FlushCapture(SDL_AudioDevice *device)
{
    // Do nothing, the new data will just be dropped.
}

static int EMSCRIPTENAUDIO_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    MAIN_THREAD_EM_ASM({
        var SDL3 = Module['SDL3'];
        var numChannels = SDL3.capture.currentCaptureBuffer.numberOfChannels;
        for (var c = 0; c < numChannels; ++c) {
            var channelData = SDL3.capture.currentCaptureBuffer.getChannelData(c);
            if (channelData.length != $1) {
                throw 'Web Audio capture buffer length mismatch! Destination size: ' + channelData.length + ' samples vs expected ' + $1 + ' samples!';
            }

            if (numChannels == 1) {  // fastpath this a little for the common (mono) case.
                for (var j = 0; j < $1; ++j) {
                    setValue($0 + (j * 4), channelData[j], 'float');
                }
            } else {
                for (var j = 0; j < $1; ++j) {
                    setValue($0 + (((j * numChannels) + c) * 4), channelData[j], 'float');
                }
            }
        }
    }, buffer, (buflen / sizeof(float)) / device->spec.channels);

    return buflen;
}

static void EMSCRIPTENAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (!device->hidden) {
        return;
    }

    MAIN_THREAD_EM_ASM({
        var SDL3 = Module['SDL3'];
        if ($0) {
            if (SDL3.capture.silenceTimer !== undefined) {
                clearInterval(SDL3.capture.silenceTimer);
            }
            if (SDL3.capture.stream !== undefined) {
                var tracks = SDL3.capture.stream.getAudioTracks();
                for (var i = 0; i < tracks.length; i++) {
                    SDL3.capture.stream.removeTrack(tracks[i]);
                }
            }
            if (SDL3.capture.scriptProcessorNode !== undefined) {
                SDL3.capture.scriptProcessorNode.onaudioprocess = function(audioProcessingEvent) {};
                SDL3.capture.scriptProcessorNode.disconnect();
            }
            if (SDL3.capture.mediaStreamNode !== undefined) {
                SDL3.capture.mediaStreamNode.disconnect();
            }
            SDL3.capture = undefined;
        } else {
            if (SDL3.audio.scriptProcessorNode != undefined) {
                SDL3.audio.scriptProcessorNode.disconnect();
            }
            if (SDL3.audio.silenceTimer !== undefined) {
                clearInterval(SDL3.audio.silenceTimer);
            }
            SDL3.audio = undefined;
        }
        if ((SDL3.audioContext !== undefined) && (SDL3.audio === undefined) && (SDL3.capture === undefined)) {
            SDL3.audioContext.close();
            SDL3.audioContext = undefined;
        }
    }, device->iscapture);

    SDL_free(device->hidden->mixbuf);
    SDL_free(device->hidden);
    device->hidden = NULL;

    SDL_AudioThreadFinalize(device);
}

EM_JS_DEPS(sdlaudio, "$autoResumeAudioContext,$dynCall");

static int EMSCRIPTENAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    // based on parts of library_sdl.js

    // create context
    const int result = MAIN_THREAD_EM_ASM_INT({
        if (typeof(Module['SDL3']) === 'undefined') {
            Module['SDL3'] = {};
        }
        var SDL3 = Module['SDL3'];
        if (!$0) {
            SDL3.audio = {};
        } else {
            SDL3.capture = {};
        }

        if (!SDL3.audioContext) {
            if (typeof(AudioContext) !== 'undefined') {
                SDL3.audioContext = new AudioContext();
            } else if (typeof(webkitAudioContext) !== 'undefined') {
                SDL3.audioContext = new webkitAudioContext();
            }
            if (SDL3.audioContext) {
                if ((typeof navigator.userActivation) === 'undefined') {  // Firefox doesn't have this (as of August 2023), use autoResumeAudioContext instead.
                    autoResumeAudioContext(SDL3.audioContext);
                }
            }
        }
        return SDL3.audioContext === undefined ? -1 : 0;
    }, device->iscapture);

    if (result < 0) {
        return SDL_SetError("Web Audio API is not available!");
    }

    device->spec.format = SDL_AUDIO_F32;  // web audio only supports floats

    // Initialize all variables that we clean on shutdown
    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return -1;
    }

    // limit to native freq
    device->spec.freq = EM_ASM_INT({ return Module['SDL3'].audioContext.sampleRate; });

    SDL_UpdatedAudioDeviceFormat(device);

    if (!device->iscapture) {
        device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->buffer_size);
        if (!device->hidden->mixbuf) {
            return -1;
        }
        SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);
    }

    if (device->iscapture) {
        /* The idea is to take the capture media stream, hook it up to an
           audio graph where we can pass it through a ScriptProcessorNode
           to access the raw PCM samples and push them to the SDL app's
           callback. From there, we "process" the audio data into silence
           and forget about it.

           This should, strictly speaking, use MediaRecorder for capture, but
           this API is cleaner to use and better supported, and fires a
           callback whenever there's enough data to fire down into the app.
           The downside is that we are spending CPU time silencing a buffer
           that the audiocontext uselessly mixes into any output. On the
           upside, both of those things are not only run in native code in
           the browser, they're probably SIMD code, too. MediaRecorder
           feels like it's a pretty inefficient tapdance in similar ways,
           to be honest. */

        MAIN_THREAD_EM_ASM({
            var SDL3 = Module['SDL3'];
            var have_microphone = function(stream) {
                //console.log('SDL audio capture: we have a microphone! Replacing silence callback.');
                if (SDL3.capture.silenceTimer !== undefined) {
                    clearInterval(SDL3.capture.silenceTimer);
                    SDL3.capture.silenceTimer = undefined;
                    SDL3.capture.silenceBuffer = undefined
                }
                SDL3.capture.mediaStreamNode = SDL3.audioContext.createMediaStreamSource(stream);
                SDL3.capture.scriptProcessorNode = SDL3.audioContext.createScriptProcessor($1, $0, 1);
                SDL3.capture.scriptProcessorNode.onaudioprocess = function(audioProcessingEvent) {
                    if ((SDL3 === undefined) || (SDL3.capture === undefined)) { return; }
                    audioProcessingEvent.outputBuffer.getChannelData(0).fill(0.0);
                    SDL3.capture.currentCaptureBuffer = audioProcessingEvent.inputBuffer;
                    dynCall('vi', $2, [$3]);
                };
                SDL3.capture.mediaStreamNode.connect(SDL3.capture.scriptProcessorNode);
                SDL3.capture.scriptProcessorNode.connect(SDL3.audioContext.destination);
                SDL3.capture.stream = stream;
            };

            var no_microphone = function(error) {
                //console.log('SDL audio capture: we DO NOT have a microphone! (' + error.name + ')...leaving silence callback running.');
            };

            // we write silence to the audio callback until the microphone is available (user approves use, etc).
            SDL3.capture.silenceBuffer = SDL3.audioContext.createBuffer($0, $1, SDL3.audioContext.sampleRate);
            SDL3.capture.silenceBuffer.getChannelData(0).fill(0.0);
            var silence_callback = function() {
                SDL3.capture.currentCaptureBuffer = SDL3.capture.silenceBuffer;
                dynCall('vi', $2, [$3]);
            };

            SDL3.capture.silenceTimer = setInterval(silence_callback, ($1 / SDL3.audioContext.sampleRate) * 1000);

            if ((navigator.mediaDevices !== undefined) && (navigator.mediaDevices.getUserMedia !== undefined)) {
                navigator.mediaDevices.getUserMedia({ audio: true, video: false }).then(have_microphone).catch(no_microphone);
            } else if (navigator.webkitGetUserMedia !== undefined) {
                navigator.webkitGetUserMedia({ audio: true, video: false }, have_microphone, no_microphone);
            }
        }, device->spec.channels, device->sample_frames, SDL_CaptureAudioThreadIterate, device);
    } else {
        // setup a ScriptProcessorNode
        MAIN_THREAD_EM_ASM({
            var SDL3 = Module['SDL3'];
            SDL3.audio.scriptProcessorNode = SDL3.audioContext['createScriptProcessor']($1, 0, $0);
            SDL3.audio.scriptProcessorNode['onaudioprocess'] = function (e) {
                if ((SDL3 === undefined) || (SDL3.audio === undefined)) { return; }
                // if we're actually running the node, we don't need the fake callback anymore, so kill it.
                if (SDL3.audio.silenceTimer !== undefined) {
                    clearInterval(SDL3.audio.silenceTimer);
                    SDL3.audio.silenceTimer = undefined;
                    SDL3.audio.silenceBuffer = undefined;
                }
                SDL3.audio.currentOutputBuffer = e['outputBuffer'];
                dynCall('vi', $2, [$3]);
            };

            SDL3.audio.scriptProcessorNode['connect'](SDL3.audioContext['destination']);

            if (SDL3.audioContext.state === 'suspended') {  // uhoh, autoplay is blocked.
                SDL3.audio.silenceBuffer = SDL3.audioContext.createBuffer($0, $1, SDL3.audioContext.sampleRate);
                SDL3.audio.silenceBuffer.getChannelData(0).fill(0.0);
                var silence_callback = function() {
                    if ((typeof navigator.userActivation) !== 'undefined') {  // Almost everything modern except Firefox (as of August 2023)
                        if (navigator.userActivation.hasBeenActive) {
                            SDL3.audioContext.resume();
                        }
                    }

                    // the buffer that gets filled here just gets ignored, so the app can make progress
                    //  and/or avoid flooding audio queues until we can actually play audio.
                    SDL3.audio.currentOutputBuffer = SDL3.audio.silenceBuffer;
                    dynCall('vi', $2, [$3]);
                    SDL3.audio.currentOutputBuffer = undefined;
                };

                SDL3.audio.silenceTimer = setInterval(silence_callback, ($1 / SDL3.audioContext.sampleRate) * 1000);
            }
        }, device->spec.channels, device->sample_frames, SDL_OutputAudioThreadIterate, device);
    }

    return 0;
}

static SDL_bool EMSCRIPTENAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    SDL_bool available, capture_available;

    impl->OpenDevice = EMSCRIPTENAUDIO_OpenDevice;
    impl->CloseDevice = EMSCRIPTENAUDIO_CloseDevice;
    impl->GetDeviceBuf = EMSCRIPTENAUDIO_GetDeviceBuf;
    impl->PlayDevice = EMSCRIPTENAUDIO_PlayDevice;
    impl->FlushCapture = EMSCRIPTENAUDIO_FlushCapture;
    impl->CaptureFromDevice = EMSCRIPTENAUDIO_CaptureFromDevice;

    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;

    // technically, this is just runs in idle time in the main thread, but it's close enough to a "thread" for our purposes.
    impl->ProvidesOwnCallbackThread = SDL_TRUE;

    // check availability
    available = MAIN_THREAD_EM_ASM_INT({
        if (typeof(AudioContext) !== 'undefined') {
            return true;
        } else if (typeof(webkitAudioContext) !== 'undefined') {
            return true;
        }
        return false;
    });

    if (!available) {
        SDL_SetError("No audio context available");
    }

    capture_available = available && MAIN_THREAD_EM_ASM_INT({
        if ((typeof(navigator.mediaDevices) !== 'undefined') && (typeof(navigator.mediaDevices.getUserMedia) !== 'undefined')) {
            return true;
        } else if (typeof(navigator.webkitGetUserMedia) !== 'undefined') {
            return true;
        }
        return false;
    });

    impl->HasCaptureSupport = capture_available;
    impl->OnlyHasDefaultCaptureDevice = capture_available;

    return available;
}

AudioBootStrap EMSCRIPTENAUDIO_bootstrap = {
    "emscripten", "SDL emscripten audio driver", EMSCRIPTENAUDIO_Init, SDL_FALSE
};

/* *INDENT-ON* */ /* clang-format on */

#endif // SDL_AUDIO_DRIVER_EMSCRIPTEN
