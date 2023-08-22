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

// SDL audio driver using an AudioWorklet. This requires pthreads. A separate
// audio driver for Emscripten that is still single-threaded-friendly is in
// the "emscripten" directory.

#if SDL_AUDIO_DRIVER_AUDIOWORKLET

#include <emscripten/emscripten.h>
#include <emscripten/webaudio.h>

#include "../SDL_audio_c.h"
#include "SDL_audioworklet.h"


// just turn off clang-format for this whole file, this INDENT_OFF stuff on
//  each EM_ASM section is ugly.
/* *INDENT-OFF* */ /* clang-format off */


// we keep one global AudioContext that is shared by output and capture.
static EMSCRIPTEN_WEBAUDIO_T sdl_audio_context = 0;

// we keep on global block of memory for the the AudioWorklet's stack. There's only ever one AudioWorklet worker thread in the lifetime of a page, so this is global and never free'd.
static void *audio_thread_stack = NULL;

EM_JS_DEPS(sdlaudioworklet, "$autoResumeAudioContext,$dynCall,emscripten_create_audio_context");

static EM_BOOL AudioWorkletProcess(int numInputs, const AudioSampleFrame *inputs, int numOutputs, AudioSampleFrame *outputs,
                      int numParams, const AudioParamFrame *params, void *userData)
{
//printf("AudioWorklet: process start!\n");

    SDL_AudioDevice *device = (SDL_AudioDevice *)userData;
    SDL_assert(numOutputs == 1);
    SDL_assert(device->hidden->outputs == NULL);
    device->hidden->outputs = outputs;

    if (!SDL_OutputAudioThreadIterate(device)) {
        // Something went wrong (or device is shutting down), just provide silence.
//        SDL_memset(outputs->data, '\0', outputs->numberOfChannels * sizeof (float) * 128);
    } else {
        // zero out any unexpected channels, just in case.
//        if (device->spec.channels < outputs->numberOfChannels) {
//            float *ptr = outputs->data + (device->spec.channels * 128);
//            SDL_memset(ptr, '\0', (outputs->numberOfChannels - device->spec.channels) * sizeof (float) * 128);
//        }
    }

    device->hidden->outputs = NULL;

//printf("AudioWorklet: process end!\n");
//fflush(stdout);

    return EM_TRUE;  // keep the graph output going
}

static Uint8 *AUDIOWORKLET_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return (Uint8 *) device->hidden->mixbuf;
}

static void AUDIOWORKLET_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size)
{
return;
    AudioSampleFrame *outputs = device->hidden->outputs;
    const int chans = device->spec.channels;
    const float *basesrc = (const float *) buffer;
    float *dst = outputs->data;

    SDL_assert(outputs != NULL);
    SDL_assert(buffer_size == (sizeof (float) * chans * 128));

    for (int i = 0; i < chans; i++) {
        const float *src = basesrc++;
        for (int j = 0; j < 128; j++) {
            *(dst++) = *src;
            src += chans;
        }
    }
}

static int CreateAudioWorkletNode(SDL_AudioDevice *device)
{
    int outputChannelCounts[1] = { device->spec.channels };
    EmscriptenAudioWorkletNodeCreateOptions opts;
    SDL_zero(opts);
    opts.numberOfInputs = 0;
    opts.numberOfOutputs = 1;
    opts.outputChannelCounts = outputChannelCounts;

    device->hidden->worklet_node = emscripten_create_wasm_audio_worklet_node(sdl_audio_context, "SDL3", &opts, AudioWorkletProcess, device);
    if (!device->hidden->worklet_node) {
        return -1;
    }

SDL_Log("Worklet node == %d", device->hidden->worklet_node);

    // Connect it to audio context destination
    MAIN_THREAD_EM_ASM({emscriptenGetAudioObject($0).connect(emscriptenGetAudioObject($1).destination);}, device->hidden->worklet_node, sdl_audio_context);

// !!! FIXME: deal with silence callback, etc.
    emscripten_resume_audio_context_sync(sdl_audio_context);

    //MAIN_THREAD_EM_ASM({console.log(emscriptenGetAudioObject($0).state);}, context);
    return 0;
}

static void AudioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *) userData;
SDL_Log("ProcessorCreated!");

    SDL_assert(context == sdl_audio_context);

    if (!success) {
        // !!! FIXME: disconnect the device.
SDL_Log("UGH 2");
        return;
    }

    if (CreateAudioWorkletNode(device) < 0) {
        // !!! FIXME: disconnect the device (or move it to silence?)
    }
}

static void AudioThreadInitialized(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *) userData;

    SDL_assert(context == sdl_audio_context);

    if (!success) {
        // !!! FIXME: disconnect the device, maybe free the thread stack?
SDL_Log("UGH 1");
        return;
    }

    WebAudioWorkletProcessorCreateOptions opts;
    SDL_zero(opts);
    opts.name = "SDL3";

SDL_Log("emscripten_create_wasm_audio_worklet_processor_async");
    emscripten_create_wasm_audio_worklet_processor_async(context, &opts, AudioWorkletProcessorCreated, device);
}

static void AUDIOWORKLET_FlushCapture(SDL_AudioDevice *device)
{
    // Do nothing, the new data will just be dropped.
}

static int AUDIOWORKLET_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
// SDL_Log("Got %d more sample frames from microphone!", (int) ((buflen / sizeof(float)) / device->spec.channels));

    MAIN_THREAD_EM_ASM({
        var ctx = emscriptenGetAudioObject($0);
        var SDL3_capture = ctx.SDL3_capture;
        var numChannels = SDL3_capture.currentCaptureBuffer.numberOfChannels;
        for (var c = 0; c < numChannels; ++c) {
            var channelData = SDL3_capture.currentCaptureBuffer.getChannelData(c);
            if (channelData.length != $2) {
                throw 'Web Audio capture buffer length mismatch! Destination size: ' + channelData.length + ' samples vs expected ' + $2 + ' samples!';
            }

            if (numChannels == 1) {  // fastpath this a little for the common (mono) case.
                for (var j = 0; j < $2; ++j) {
                    setValue($1 + (j * 4), channelData[j], 'float');
                }
            } else {
                for (var j = 0; j < $2; ++j) {
                    setValue($1 + (((j * numChannels) + c) * 4), channelData[j], 'float');
                }
            }
        }
    }, sdl_audio_context, buffer, (buflen / sizeof(float)) / device->spec.channels);

    return buflen;
}

static void AUDIOWORKLET_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (sdl_audio_context) {
            if (device->iscapture) {
                MAIN_THREAD_EM_ASM({
                    var ctx = emscriptenGetAudioObject($0);
                    var SDL3_capture = ctx.SDL3_capture;

                    if (SDL3_capture.silenceTimer !== undefined) {
                        clearTimeout(SDL3_capture.silenceTimer);
                    }
                    if (SDL3_capture.stream !== undefined) {
                        var tracks = SDL3_capture.stream.getAudioTracks();
                        for (var i = 0; i < tracks.length; i++) {
                            SDL3_capture.stream.removeTrack(tracks[i]);
                        }
                        SDL3_capture.stream = undefined;
                    }
                    if (SDL3_capture.scriptProcessorNode !== undefined) {
                        SDL3_capture.scriptProcessorNode.onaudioprocess = function(audioProcessingEvent) {};
                        SDL3_capture.scriptProcessorNode.disconnect();
                        SDL3_capture.scriptProcessorNode = undefined;
                    }
                    if (SDL3_capture.mediaStreamNode !== undefined) {
                        SDL3_capture.mediaStreamNode.disconnect();
                        SDL3_capture.mediaStreamNode = undefined;
                    }
                    if (SDL3_capture.silenceBuffer !== undefined) {
                        SDL3_capture.silenceBuffer = undefined
                    }
                    ctx.SDL3_capture = undefined;
                }, sdl_audio_context);
            }
        }

        if (device->hidden->worklet_node) {
            SDL_assert(!device->iscapture);
            emscripten_destroy_web_audio_node(device->hidden->worklet_node);
        }

        SDL_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static int AUDIOWORKLET_OpenDevice(SDL_AudioDevice *device)
{
    const SDL_bool iscapture = device->iscapture;

    // Initialize all variables that we clean on shutdown
    device->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, (sizeof *device->hidden));
    if (device->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    // Don't create the context until the first device open, so there's slightly more chance the user interacted with the page between
    // SDL_Init and device open. But generally, it is what it is.
    if (!sdl_audio_context) {
        sdl_audio_context = MAIN_THREAD_EM_ASM_INT({
            return _emscripten_create_audio_context(0);  // this is a C-callable function, but we need to wrap it in MAIN_THREAD_EM_ASM to proxy it to the main thread.
        });
        if (!sdl_audio_context) {
            return SDL_SetError("emscripten_create_audio_context failed");
        }
    }

    device->spec.format = SDL_AUDIO_F32;  // web audio only supports floats
	device->spec.freq = MAIN_THREAD_EM_ASM_INT({ return emscriptenGetAudioObject($0).sampleRate; }, sdl_audio_context);  // limit to native freq

    if (!iscapture) {
        device->sample_frames = 128;  // AudioWorklet buffers are always 128 sample frames!
    }

    SDL_UpdatedAudioDeviceFormat(device);

    if (iscapture) {
        /* The idea is to take the capture media stream, hook it up to an
           audio graph where we can pass it through a ScriptProcessorNode
           to access the raw PCM samples and push them to the SDL app's
           callback. From there, we "process" the audio data into silence
           and forget about it.

           This should, strictly speaking, use MediaRecorder for capture, but
           device API is cleaner to use and better supported, and fires a
           callback whenever there's enough data to fire down into the app.
           The downside is that we are spending CPU time silencing a buffer
           that the audiocontext uselessly mixes into any output. On the
           upside, both of those things are not only run in native code in
           the browser, they're probably SIMD code, too. MediaRecorder
           feels like it's a pretty inefficient tapdance in similar ways,
           to be honest. */

        MAIN_THREAD_EM_ASM({
            var ctx = emscriptenGetAudioObject($4);

            ctx.SDL3_capture = {};
            var SDL3_capture = ctx.SDL3_capture;

            var have_microphone = function(stream) {
                //console.log('SDL audio capture: we have a microphone! Replacing silence callback.');
                if (SDL3_capture.silenceTimer !== undefined) {
                    clearTimeout(SDL3_capture.silenceTimer);
                    SDL3_capture.silenceTimer = undefined;
                }
                SDL3_capture.mediaStreamNode = ctx.createMediaStreamSource(stream);
                SDL3_capture.scriptProcessorNode = ctx.createScriptProcessor($1, $0, 1);
                SDL3_capture.scriptProcessorNode.onaudioprocess = function(audioProcessingEvent) {
                    audioProcessingEvent.outputBuffer.getChannelData(0).fill(0.0);
                    SDL3_capture.currentCaptureBuffer = audioProcessingEvent.inputBuffer;
                    dynCall('vi', $2, [$3]);
                };
                SDL3_capture.mediaStreamNode.connect(SDL3_capture.scriptProcessorNode);
                SDL3_capture.scriptProcessorNode.connect(ctx.destination);
                SDL3_capture.stream = stream;
            };

            var no_microphone = function(error) {
                //console.log('SDL audio capture: we DO NOT have a microphone! (' + error.name + ')...leaving silence callback running.');
            };

            // we write silence to the audio callback until the microphone is available (user approves use, etc).
            SDL3_capture.silenceBuffer = ctx.createBuffer($0, $1, ctx.sampleRate);
            SDL3_capture.silenceBuffer.getChannelData(0).fill(0.0);
            var silence_callback = function() {
                SDL3_capture.currentCaptureBuffer = SDL3_capture.silenceBuffer;
                dynCall('vi', $2, [$3]);
            };

            SDL3_capture.silenceTimer = setTimeout(silence_callback, ($1 / ctx.sampleRate) * 1000);

            if ((navigator.mediaDevices !== undefined) && (navigator.mediaDevices.getUserMedia !== undefined)) {
                navigator.mediaDevices.getUserMedia({ audio: true, video: false }).then(have_microphone).catch(no_microphone);
            } else if (navigator.webkitGetUserMedia !== undefined) {
                navigator.webkitGetUserMedia({ audio: true, video: false }, have_microphone, no_microphone);
            }
        }, device->spec.channels, device->sample_frames, SDL_CaptureAudioThreadIterate, device, sdl_audio_context);
    } else {
        device->hidden->mixbuf = (float *) SDL_calloc(1, device->buffer_size * 128);
        if (device->hidden->mixbuf == NULL) {
            return SDL_OutOfMemory();
        }

        if (!audio_thread_stack) {
            const uint32_t audio_thread_stack_size = 1024 * 512;   // !!! FIXME: just winging this. Is 512k too much or too little?!
            SDL_assert((audio_thread_stack_size % 16) == 0);  // stack size must be a multiple of 16 bytes, according to Emscripten docs...
            audio_thread_stack = SDL_aligned_alloc(16, audio_thread_stack_size);  // ...and aligned to 16 bytes.
            if (audio_thread_stack == NULL) {
                return SDL_OutOfMemory();
            }
            // Fire off the worklet thread. This only happens once, and lives for the rest of the page's life.
            emscripten_start_wasm_audio_worklet_thread_async(sdl_audio_context, audio_thread_stack, audio_thread_stack_size, AudioThreadInitialized, device);
        } else {
            // we already set up an AudioWorklet thread before, just make a worklet node for it.
            if (CreateAudioWorkletNode(device) == -1) {
                return SDL_SetError("Failed to create AudioWorklet node!");
            }
        }
    }

    return 0;
}

static void AUDIOWORKLET_Deinitialize(void)
{
    if (sdl_audio_context) {
        emscripten_destroy_audio_context(sdl_audio_context);
        sdl_audio_context = 0;
    }
}

static SDL_bool AUDIOWORKLET_Init(SDL_AudioDriverImpl *impl)
{
    // check availability
    const SDL_bool available = MAIN_THREAD_EM_ASM_INT({
        if (typeof(AudioWorkletNode) === 'undefined') {
            return false;
        } else if (typeof(SharedArrayBuffer) === 'undefined') {
            return false;  // obviously we need threads for this.
        }
        return true;
    });

    if (!available) {
        SDL_SetError("AudioWorklet or SharedArrayBuffer is not available");
    }

    const SDL_bool capture_available = MAIN_THREAD_EM_ASM_INT({
        if ((typeof(navigator.mediaDevices) !== 'undefined') && (typeof(navigator.mediaDevices.getUserMedia) !== 'undefined')) {
            return true;
        } else if (typeof(navigator.webkitGetUserMedia) !== 'undefined') {
            return true;
        }
        return false;
    });

    // Set the function pointers
    impl->OpenDevice = AUDIOWORKLET_OpenDevice;
    impl->CloseDevice = AUDIOWORKLET_CloseDevice;
    impl->GetDeviceBuf = AUDIOWORKLET_GetDeviceBuf;
    impl->PlayDevice = AUDIOWORKLET_PlayDevice;
    impl->FlushCapture = AUDIOWORKLET_FlushCapture;
    impl->CaptureFromDevice = AUDIOWORKLET_CaptureFromDevice;
    impl->Deinitialize = AUDIOWORKLET_Deinitialize;

    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->ProvidesOwnCallbackThread = SDL_TRUE;
    impl->HasCaptureSupport = capture_available ? SDL_TRUE : SDL_FALSE;
    impl->OnlyHasDefaultCaptureDevice = capture_available ? SDL_TRUE : SDL_FALSE;

    return available;
}

AudioBootStrap AUDIOWORKLET_bootstrap = {
    "audioworklet", "Emscripten AudioWorklet", AUDIOWORKLET_Init, SDL_FALSE
};

/* *INDENT-ON* */ /* clang-format on */

#endif // SDL_AUDIO_DRIVER_AUDIOWORKLET
