/**
 * Original code: automated SDL audio test written by Edgar Simo "bobbens"
 * New/updated tests: aschiffler at ferzkopp dot net
 */

/* quiet windows compiler warnings */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <math.h>
#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Fixture */

static void audioSetUp(void *arg)
{
    /* Start SDL audio subsystem */
    int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_AUDIO)");
    SDLTest_AssertCheck(ret == 0, "Check result from SDL_InitSubSystem(SDL_INIT_AUDIO)");
    if (ret != 0) {
        SDLTest_LogError("%s", SDL_GetError());
    }
}

static void audioTearDown(void *arg)
{
    /* Remove a possibly created file from SDL disk writer audio driver; ignore errors */
    (void)remove("sdlaudio.raw");

    SDLTest_AssertPass("Cleanup of test files completed");
}

#if 0  /* !!! FIXME: maybe update this? */
/* Global counter for callback invocation */
static int g_audio_testCallbackCounter;

/* Global accumulator for total callback length */
static int g_audio_testCallbackLength;

/* Test callback function */
static void SDLCALL audio_testCallback(void *userdata, Uint8 *stream, int len)
{
    /* track that callback was called */
    g_audio_testCallbackCounter++;
    g_audio_testCallbackLength += len;
}
#endif

static SDL_AudioDeviceID g_audio_id = -1;

/* Test case functions */

/**
 * Stop and restart audio subsystem
 *
 * \sa SDL_QuitSubSystem
 * \sa SDL_InitSubSystem
 */
static int audio_quitInitAudioSubSystem(void *arg)
{
    /* Stop SDL audio subsystem */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

    /* Restart audio again */
    audioSetUp(NULL);

    return TEST_COMPLETED;
}

/**
 * Start and stop audio directly
 *
 * \sa SDL_InitAudio
 * \sa SDL_QuitAudio
 */
static int audio_initQuitAudio(void *arg)
{
    int result;
    int i, iMax;
    const char *audioDriver;

    /* Stop SDL audio subsystem */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

    /* Loop over all available audio drivers */
    iMax = SDL_GetNumAudioDrivers();
    SDLTest_AssertPass("Call to SDL_GetNumAudioDrivers()");
    SDLTest_AssertCheck(iMax > 0, "Validate number of audio drivers; expected: >0 got: %d", iMax);
    for (i = 0; i < iMax; i++) {
        audioDriver = SDL_GetAudioDriver(i);
        SDLTest_AssertPass("Call to SDL_GetAudioDriver(%d)", i);
        SDLTest_Assert(audioDriver != NULL, "Audio driver name is not NULL");
        SDLTest_AssertCheck(audioDriver[0] != '\0', "Audio driver name is not empty; got: %s", audioDriver); /* NOLINT(clang-analyzer-core.NullDereference): Checked for NULL above */

        /* Call Init */
        SDL_SetHint("SDL_AUDIO_DRIVER", audioDriver);
        result = SDL_InitSubSystem(SDL_INIT_AUDIO);
        SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_AUDIO) with driver='%s'", audioDriver);
        SDLTest_AssertCheck(result == 0, "Validate result value; expected: 0 got: %d", result);

        /* Call Quit */
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");
    }

    /* NULL driver specification */
    audioDriver = NULL;

    /* Call Init */
    SDL_SetHint("SDL_AUDIO_DRIVER", audioDriver);
    result = SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_AudioInit(NULL)");
    SDLTest_AssertCheck(result == 0, "Validate result value; expected: 0 got: %d", result);

    /* Call Quit */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

    /* Restart audio again */
    audioSetUp(NULL);

    return TEST_COMPLETED;
}

/**
 * Start, open, close and stop audio
 *
 * \sa SDL_InitAudio
 * \sa SDL_OpenAudioDevice
 * \sa SDL_CloseAudioDevice
 * \sa SDL_QuitAudio
 */
static int audio_initOpenCloseQuitAudio(void *arg)
{
    int result;
    int i, iMax, j, k;
    const char *audioDriver;
    SDL_AudioSpec desired;

    /* Stop SDL audio subsystem */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

    /* Loop over all available audio drivers */
    iMax = SDL_GetNumAudioDrivers();
    SDLTest_AssertPass("Call to SDL_GetNumAudioDrivers()");
    SDLTest_AssertCheck(iMax > 0, "Validate number of audio drivers; expected: >0 got: %d", iMax);
    for (i = 0; i < iMax; i++) {
        audioDriver = SDL_GetAudioDriver(i);
        SDLTest_AssertPass("Call to SDL_GetAudioDriver(%d)", i);
        SDLTest_Assert(audioDriver != NULL, "Audio driver name is not NULL");
        SDLTest_AssertCheck(audioDriver[0] != '\0', "Audio driver name is not empty; got: %s", audioDriver); /* NOLINT(clang-analyzer-core.NullDereference): Checked for NULL above */

        /* Change specs */
        for (j = 0; j < 2; j++) {

            /* Call Init */
            SDL_SetHint("SDL_AUDIO_DRIVER", audioDriver);
            result = SDL_InitSubSystem(SDL_INIT_AUDIO);
            SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_AUDIO) with driver='%s'", audioDriver);
            SDLTest_AssertCheck(result == 0, "Validate result value; expected: 0 got: %d", result);

            /* Set spec */
            SDL_memset(&desired, 0, sizeof(desired));
            switch (j) {
            case 0:
                /* Set standard desired spec */
                desired.freq = 22050;
                desired.format = SDL_AUDIO_S16;
                desired.channels = 2;

            case 1:
                /* Set custom desired spec */
                desired.freq = 48000;
                desired.format = SDL_AUDIO_F32;
                desired.channels = 2;
                break;
            }

            /* Call Open (maybe multiple times) */
            for (k = 0; k <= j; k++) {
                result = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &desired);
                if (k == 0) {
                    g_audio_id = result;
                }
                SDLTest_AssertPass("Call to SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, desired_spec_%d), call %d", j, k + 1);
                SDLTest_AssertCheck(result > 0, "Verify return value; expected: > 0, got: %d", result);
            }

            /* Call Close (maybe multiple times) */
            for (k = 0; k <= j; k++) {
                SDL_CloseAudioDevice(g_audio_id);
                SDLTest_AssertPass("Call to SDL_CloseAudioDevice(), call %d", k + 1);
            }

            /* Call Quit (maybe multiple times) */
            for (k = 0; k <= j; k++) {
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO), call %d", k + 1);
            }

        } /* spec loop */
    }     /* driver loop */

    /* Restart audio again */
    audioSetUp(NULL);

    return TEST_COMPLETED;
}

/**
 * Pause and unpause audio
 *
 * \sa SDL_PauseAudioDevice
 * \sa SDL_PlayAudioDevice
 */
static int audio_pauseUnpauseAudio(void *arg)
{
    int iMax;
    int i, j /*, k, l*/;
    int result;
    const char *audioDriver;
    SDL_AudioSpec desired;

    /* Stop SDL audio subsystem */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

    /* Loop over all available audio drivers */
    iMax = SDL_GetNumAudioDrivers();
    SDLTest_AssertPass("Call to SDL_GetNumAudioDrivers()");
    SDLTest_AssertCheck(iMax > 0, "Validate number of audio drivers; expected: >0 got: %d", iMax);
    for (i = 0; i < iMax; i++) {
        audioDriver = SDL_GetAudioDriver(i);
        SDLTest_AssertPass("Call to SDL_GetAudioDriver(%d)", i);
        SDLTest_Assert(audioDriver != NULL, "Audio driver name is not NULL");
        SDLTest_AssertCheck(audioDriver[0] != '\0', "Audio driver name is not empty; got: %s", audioDriver); /* NOLINT(clang-analyzer-core.NullDereference): Checked for NULL above */

        /* Change specs */
        for (j = 0; j < 2; j++) {

            /* Call Init */
            SDL_SetHint("SDL_AUDIO_DRIVER", audioDriver);
            result = SDL_InitSubSystem(SDL_INIT_AUDIO);
            SDLTest_AssertPass("Call to SDL_InitSubSystem(SDL_INIT_AUDIO) with driver='%s'", audioDriver);
            SDLTest_AssertCheck(result == 0, "Validate result value; expected: 0 got: %d", result);

            /* Set spec */
            SDL_memset(&desired, 0, sizeof(desired));
            switch (j) {
            case 0:
                /* Set standard desired spec */
                desired.freq = 22050;
                desired.format = SDL_AUDIO_S16;
                desired.channels = 2;
                break;

            case 1:
                /* Set custom desired spec */
                desired.freq = 48000;
                desired.format = SDL_AUDIO_F32;
                desired.channels = 2;
                break;
            }

            /* Call Open */
            g_audio_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &desired);
            result = g_audio_id;
            SDLTest_AssertPass("Call to SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, desired_spec_%d)", j);
            SDLTest_AssertCheck(result > 0, "Verify return value; expected > 0 got: %d", result);

#if 0  /* !!! FIXME: maybe update this? */
            /* Start and stop audio multiple times */
            for (l = 0; l < 3; l++) {
                SDLTest_Log("Pause/Unpause iteration: %d", l + 1);

                /* Reset callback counters */
                g_audio_testCallbackCounter = 0;
                g_audio_testCallbackLength = 0;

                /* Un-pause audio to start playing (maybe multiple times) */
                for (k = 0; k <= j; k++) {
                    SDL_PlayAudioDevice(g_audio_id);
                    SDLTest_AssertPass("Call to SDL_PlayAudioDevice(g_audio_id), call %d", k + 1);
                }

                /* Wait for callback */
                int totalDelay = 0;
                do {
                    SDL_Delay(10);
                    totalDelay += 10;
                } while (g_audio_testCallbackCounter == 0 && totalDelay < 1000);
                SDLTest_AssertCheck(g_audio_testCallbackCounter > 0, "Verify callback counter; expected: >0 got: %d", g_audio_testCallbackCounter);
                SDLTest_AssertCheck(g_audio_testCallbackLength > 0, "Verify callback length; expected: >0 got: %d", g_audio_testCallbackLength);

                /* Pause audio to stop playing (maybe multiple times) */
                for (k = 0; k <= j; k++) {
                    const int pause_on = (k == 0) ? 1 : SDLTest_RandomIntegerInRange(99, 9999);
                    if (pause_on) {
                        SDL_PauseAudioDevice(g_audio_id);
                        SDLTest_AssertPass("Call to SDL_PauseAudioDevice(g_audio_id), call %d", k + 1);
                    } else {
                        SDL_PlayAudioDevice(g_audio_id);
                        SDLTest_AssertPass("Call to SDL_PlayAudioDevice(g_audio_id), call %d", k + 1);
                    }
                }

                /* Ensure callback is not called again */
                const int originalCounter = g_audio_testCallbackCounter;
                SDL_Delay(totalDelay + 10);
                SDLTest_AssertCheck(originalCounter == g_audio_testCallbackCounter, "Verify callback counter; expected: %d, got: %d", originalCounter, g_audio_testCallbackCounter);
            }
#endif

            /* Call Close */
            SDL_CloseAudioDevice(g_audio_id);
            SDLTest_AssertPass("Call to SDL_CloseAudioDevice()");

            /* Call Quit */
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

        } /* spec loop */
    }     /* driver loop */

    /* Restart audio again */
    audioSetUp(NULL);

    return TEST_COMPLETED;
}

/**
 * Enumerate and name available audio devices (output and capture).
 *
 * \sa SDL_GetNumAudioDevices
 * \sa SDL_GetAudioDeviceName
 */
static int audio_enumerateAndNameAudioDevices(void *arg)
{
    int t;
    int i, n;
    char *name;
    SDL_AudioDeviceID *devices = NULL;

    /* Iterate over types: t=0 output device, t=1 input/capture device */
    for (t = 0; t < 2; t++) {
        /* Get number of devices. */
        devices = (t) ? SDL_GetAudioCaptureDevices(&n) : SDL_GetAudioOutputDevices(&n);
        SDLTest_AssertPass("Call to SDL_GetAudio%sDevices(%i)", (t) ? "Capture" : "Output", t);
        SDLTest_Log("Number of %s devices < 0, reported as %i", (t) ? "capture" : "output", n);
        SDLTest_AssertCheck(n >= 0, "Validate result is >= 0, got: %i", n);

        /* List devices. */
        if (n > 0) {
            SDLTest_AssertCheck(devices != NULL, "Validate devices is not NULL if n > 0");
            for (i = 0; i < n; i++) {
                name = SDL_GetAudioDeviceName(devices[i]);
                SDLTest_AssertPass("Call to SDL_GetAudioDeviceName(%i)", i);
                SDLTest_AssertCheck(name != NULL, "Verify result from SDL_GetAudioDeviceName(%i) is not NULL", i);
                if (name != NULL) {
                    SDLTest_AssertCheck(name[0] != '\0', "verify result from SDL_GetAudioDeviceName(%i) is not empty, got: '%s'", i, name);
                    SDL_free(name);
                }
            }
        }

        SDL_free(devices);
    }

    return TEST_COMPLETED;
}

/**
 * Negative tests around enumeration and naming of audio devices.
 *
 * \sa SDL_GetNumAudioDevices
 * \sa SDL_GetAudioDeviceName
 */
static int audio_enumerateAndNameAudioDevicesNegativeTests(void *arg)
{
    return TEST_COMPLETED;  /* nothing in here atm since these interfaces changed in SDL3. */
}

/**
 * Checks available audio driver names.
 *
 * \sa SDL_GetNumAudioDrivers
 * \sa SDL_GetAudioDriver
 */
static int audio_printAudioDrivers(void *arg)
{
    int i, n;
    const char *name;

    /* Get number of drivers */
    n = SDL_GetNumAudioDrivers();
    SDLTest_AssertPass("Call to SDL_GetNumAudioDrivers()");
    SDLTest_AssertCheck(n >= 0, "Verify number of audio drivers >= 0, got: %i", n);

    /* List drivers. */
    if (n > 0) {
        for (i = 0; i < n; i++) {
            name = SDL_GetAudioDriver(i);
            SDLTest_AssertPass("Call to SDL_GetAudioDriver(%i)", i);
            SDLTest_AssertCheck(name != NULL, "Verify returned name is not NULL");
            if (name != NULL) {
                SDLTest_AssertCheck(name[0] != '\0', "Verify returned name is not empty, got: '%s'", name);
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Checks current audio driver name with initialized audio.
 *
 * \sa SDL_GetCurrentAudioDriver
 */
static int audio_printCurrentAudioDriver(void *arg)
{
    /* Check current audio driver */
    const char *name = SDL_GetCurrentAudioDriver();
    SDLTest_AssertPass("Call to SDL_GetCurrentAudioDriver()");
    SDLTest_AssertCheck(name != NULL, "Verify returned name is not NULL");
    if (name != NULL) {
        SDLTest_AssertCheck(name[0] != '\0', "Verify returned name is not empty, got: '%s'", name);
    }

    return TEST_COMPLETED;
}

/* Definition of all formats, channels, and frequencies used to test audio conversions */
static SDL_AudioFormat g_audioFormats[] = {
    SDL_AUDIO_S8, SDL_AUDIO_U8,
    SDL_AUDIO_S16LE, SDL_AUDIO_S16BE,
    SDL_AUDIO_S32LE, SDL_AUDIO_S32BE,
    SDL_AUDIO_F32LE, SDL_AUDIO_F32BE
};
static const char *g_audioFormatsVerbose[] = {
    "SDL_AUDIO_S8", "SDL_AUDIO_U8",
    "SDL_AUDIO_S16LE", "SDL_AUDIO_S16BE",
    "SDL_AUDIO_S32LE", "SDL_AUDIO_S32BE",
    "SDL_AUDIO_F32LE", "SDL_AUDIO_F32BE"
};
static const int g_numAudioFormats = SDL_arraysize(g_audioFormats);
static Uint8 g_audioChannels[] = { 1, 2, 4, 6 };
static const int g_numAudioChannels = SDL_arraysize(g_audioChannels);
static int g_audioFrequencies[] = { 11025, 22050, 44100, 48000 };
static const int g_numAudioFrequencies = SDL_arraysize(g_audioFrequencies);

/* Verify the audio formats are laid out as expected */
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_U8_FORMAT, SDL_AUDIO_U8 == SDL_AUDIO_BITSIZE(8));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_S8_FORMAT, SDL_AUDIO_S8 == (SDL_AUDIO_BITSIZE(8) | SDL_AUDIO_MASK_SIGNED));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_S16LE_FORMAT, SDL_AUDIO_S16LE == (SDL_AUDIO_BITSIZE(16) | SDL_AUDIO_MASK_SIGNED));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_S16BE_FORMAT, SDL_AUDIO_S16BE == (SDL_AUDIO_S16LE | SDL_AUDIO_MASK_BIG_ENDIAN));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_S32LE_FORMAT, SDL_AUDIO_S32LE == (SDL_AUDIO_BITSIZE(32) | SDL_AUDIO_MASK_SIGNED));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_S32BE_FORMAT, SDL_AUDIO_S32BE == (SDL_AUDIO_S32LE | SDL_AUDIO_MASK_BIG_ENDIAN));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_F32LE_FORMAT, SDL_AUDIO_F32LE == (SDL_AUDIO_BITSIZE(32) | SDL_AUDIO_MASK_FLOAT | SDL_AUDIO_MASK_SIGNED));
SDL_COMPILE_TIME_ASSERT(SDL_AUDIO_F32BE_FORMAT, SDL_AUDIO_F32BE == (SDL_AUDIO_F32LE | SDL_AUDIO_MASK_BIG_ENDIAN));

/**
 * Builds various audio conversion structures
 *
 * \sa SDL_CreateAudioStream
 */
static int audio_buildAudioStream(void *arg)
{
    SDL_AudioStream *stream;
    SDL_AudioSpec spec1;
    SDL_AudioSpec spec2;
    int i, ii, j, jj, k, kk;

    /* Call Quit */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDLTest_AssertPass("Call to SDL_QuitSubSystem(SDL_INIT_AUDIO)");

    /* No conversion needed */
    spec1.format = SDL_AUDIO_S16LE;
    spec1.channels = 2;
    spec1.freq = 22050;
    stream = SDL_CreateAudioStream(&spec1, &spec1);
    SDLTest_AssertPass("Call to SDL_CreateAudioStream(spec1 ==> spec1)");
    SDLTest_AssertCheck(stream != NULL, "Verify stream value; expected: != NULL, got: %p", (void *)stream);
    SDL_DestroyAudioStream(stream);

    /* Typical conversion */
    spec1.format = SDL_AUDIO_S8;
    spec1.channels = 1;
    spec1.freq = 22050;
    spec2.format = SDL_AUDIO_S16LE;
    spec2.channels = 2;
    spec2.freq = 44100;
    stream = SDL_CreateAudioStream(&spec1, &spec2);
    SDLTest_AssertPass("Call to SDL_CreateAudioStream(spec1 ==> spec2)");
    SDLTest_AssertCheck(stream != NULL, "Verify stream value; expected: != NULL, got: %p", (void *)stream);
    SDL_DestroyAudioStream(stream);

    /* All source conversions with random conversion targets, allow 'null' conversions */
    for (i = 0; i < g_numAudioFormats; i++) {
        for (j = 0; j < g_numAudioChannels; j++) {
            for (k = 0; k < g_numAudioFrequencies; k++) {
                spec1.format = g_audioFormats[i];
                spec1.channels = g_audioChannels[j];
                spec1.freq = g_audioFrequencies[k];
                ii = SDLTest_RandomIntegerInRange(0, g_numAudioFormats - 1);
                jj = SDLTest_RandomIntegerInRange(0, g_numAudioChannels - 1);
                kk = SDLTest_RandomIntegerInRange(0, g_numAudioFrequencies - 1);
                spec2.format = g_audioFormats[ii];
                spec2.channels = g_audioChannels[jj];
                spec2.freq = g_audioFrequencies[kk];
                stream = SDL_CreateAudioStream(&spec1, &spec2);

                SDLTest_AssertPass("Call to SDL_CreateAudioStream(format[%i]=%s(%i),channels[%i]=%i,freq[%i]=%i ==> format[%i]=%s(%i),channels[%i]=%i,freq[%i]=%i)",
                                   i, g_audioFormatsVerbose[i], spec1.format, j, spec1.channels, k, spec1.freq, ii, g_audioFormatsVerbose[ii], spec2.format, jj, spec2.channels, kk, spec2.freq);
                SDLTest_AssertCheck(stream != NULL, "Verify stream value; expected: != NULL, got: %p", (void *)stream);
                if (stream == NULL) {
                    SDLTest_LogError("%s", SDL_GetError());
                }
                SDL_DestroyAudioStream(stream);
            }
        }
    }

    /* Restart audio again */
    audioSetUp(NULL);

    return TEST_COMPLETED;
}

/**
 * Checks calls with invalid input to SDL_CreateAudioStream
 *
 * \sa SDL_CreateAudioStream
 */
static int audio_buildAudioStreamNegative(void *arg)
{
    const char *error;
    SDL_AudioStream *stream;
    SDL_AudioSpec spec1;
    SDL_AudioSpec spec2;
    int i;
    char message[256];

    /* Valid format */
    spec1.format = SDL_AUDIO_S8;
    spec1.channels = 1;
    spec1.freq = 22050;
    spec2.format = SDL_AUDIO_S16LE;
    spec2.channels = 2;
    spec2.freq = 44100;

    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    /* Invalid conversions */
    for (i = 1; i < 64; i++) {
        /* Valid format to start with */
        spec1.format = SDL_AUDIO_S8;
        spec1.channels = 1;
        spec1.freq = 22050;
        spec2.format = SDL_AUDIO_S16LE;
        spec2.channels = 2;
        spec2.freq = 44100;

        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");

        /* Set various invalid format inputs */
        SDL_strlcpy(message, "Invalid: ", 256);
        if (i & 1) {
            SDL_strlcat(message, " spec1.format", 256);
            spec1.format = 0;
        }
        if (i & 2) {
            SDL_strlcat(message, " spec1.channels", 256);
            spec1.channels = 0;
        }
        if (i & 4) {
            SDL_strlcat(message, " spec1.freq", 256);
            spec1.freq = 0;
        }
        if (i & 8) {
            SDL_strlcat(message, " spec2.format", 256);
            spec2.format = 0;
        }
        if (i & 16) {
            SDL_strlcat(message, " spec2.channels", 256);
            spec2.channels = 0;
        }
        if (i & 32) {
            SDL_strlcat(message, " spec2.freq", 256);
            spec2.freq = 0;
        }
        SDLTest_Log("%s", message);
        stream = SDL_CreateAudioStream(&spec1, &spec2);
        SDLTest_AssertPass("Call to SDL_CreateAudioStream(spec1 ==> spec2)");
        SDLTest_AssertCheck(stream == NULL, "Verify stream value; expected: NULL, got: %p", (void *)stream);
        error = SDL_GetError();
        SDLTest_AssertPass("Call to SDL_GetError()");
        SDLTest_AssertCheck(error != NULL && error[0] != '\0', "Validate that error message was not NULL or empty");
        SDL_DestroyAudioStream(stream);
    }

    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");

    return TEST_COMPLETED;
}

/**
 * Checks current audio status.
 *
 * \sa SDL_GetAudioDeviceStatus
 */
static int audio_getAudioStatus(void *arg)
{
    return TEST_COMPLETED;  /* no longer a thing in SDL3. */
}

/**
 * Opens, checks current audio status, and closes a device.
 *
 * \sa SDL_GetAudioStatus
 */
static int audio_openCloseAndGetAudioStatus(void *arg)
{
    return TEST_COMPLETED;  /* not a thing in SDL3. */
}

/**
 * Locks and unlocks open audio device.
 *
 * \sa SDL_LockAudioDevice
 * \sa SDL_UnlockAudioDevice
 */
static int audio_lockUnlockOpenAudioDevice(void *arg)
{
    return TEST_COMPLETED;  /* not a thing in SDL3 */
}

/**
 * Convert audio using various conversion structures
 *
 * \sa SDL_CreateAudioStream
 */
static int audio_convertAudio(void *arg)
{
    SDL_AudioStream *stream;
    SDL_AudioSpec spec1;
    SDL_AudioSpec spec2;
    int c;
    char message[128];
    int i, ii, j, jj, k, kk;

    /* Iterate over bitmask that determines which parameters are modified in the conversion */
    for (c = 1; c < 8; c++) {
        SDL_strlcpy(message, "Changing:", 128);
        if (c & 1) {
            SDL_strlcat(message, " Format", 128);
        }
        if (c & 2) {
            SDL_strlcat(message, " Channels", 128);
        }
        if (c & 4) {
            SDL_strlcat(message, " Frequencies", 128);
        }
        SDLTest_Log("%s", message);
        /* All source conversions with random conversion targets */
        for (i = 0; i < g_numAudioFormats; i++) {
            for (j = 0; j < g_numAudioChannels; j++) {
                for (k = 0; k < g_numAudioFrequencies; k++) {
                    spec1.format = g_audioFormats[i];
                    spec1.channels = g_audioChannels[j];
                    spec1.freq = g_audioFrequencies[k];

                    /* Ensure we have a different target format */
                    do {
                        if (c & 1) {
                            ii = SDLTest_RandomIntegerInRange(0, g_numAudioFormats - 1);
                        } else {
                            ii = 1;
                        }
                        if (c & 2) {
                            jj = SDLTest_RandomIntegerInRange(0, g_numAudioChannels - 1);
                        } else {
                            jj = j;
                        }
                        if (c & 4) {
                            kk = SDLTest_RandomIntegerInRange(0, g_numAudioFrequencies - 1);
                        } else {
                            kk = k;
                        }
                    } while ((i == ii) && (j == jj) && (k == kk));
                    spec2.format = g_audioFormats[ii];
                    spec2.channels = g_audioChannels[jj];
                    spec2.freq = g_audioFrequencies[kk];

                    stream = SDL_CreateAudioStream(&spec1, &spec2);
                    SDLTest_AssertPass("Call to SDL_CreateAudioStream(format[%i]=%s(%i),channels[%i]=%i,freq[%i]=%i ==> format[%i]=%s(%i),channels[%i]=%i,freq[%i]=%i)",
                                       i, g_audioFormatsVerbose[i], spec1.format, j, spec1.channels, k, spec1.freq, ii, g_audioFormatsVerbose[ii], spec2.format, jj, spec2.channels, kk, spec2.freq);
                    SDLTest_AssertCheck(stream != NULL, "Verify stream value; expected: != NULL, got: %p", (void *)stream);
                    if (stream == NULL) {
                        SDLTest_LogError("%s", SDL_GetError());
                    } else {
                        Uint8 *dst_buf = NULL, *src_buf = NULL;
                        int dst_len = 0, src_len = 0, real_dst_len = 0;
                        int l = 64, m;
                        int src_framesize, dst_framesize;
                        int src_silence, dst_silence;

                        src_framesize = SDL_AUDIO_FRAMESIZE(spec1);
                        dst_framesize = SDL_AUDIO_FRAMESIZE(spec2);

                        src_len = l * src_framesize;
                        SDLTest_Log("Creating dummy sample buffer of %i length (%i bytes)", l, src_len);
                        src_buf = (Uint8 *)SDL_malloc(src_len);
                        SDLTest_AssertCheck(src_buf != NULL, "Check src data buffer to convert is not NULL");
                        if (src_buf == NULL) {
                            return TEST_ABORTED;
                        }

                        src_silence = SDL_GetSilenceValueForFormat(spec1.format);
                        SDL_memset(src_buf, src_silence, src_len);

                        dst_len = ((int)((((Sint64)l * spec2.freq) - 1) / spec1.freq) + 1) * dst_framesize;
                        dst_buf = (Uint8 *)SDL_malloc(dst_len);
                        SDLTest_AssertCheck(dst_buf != NULL, "Check dst data buffer to convert is not NULL");
                        if (dst_buf == NULL) {
                            return TEST_ABORTED;
                        }

                        real_dst_len = SDL_GetAudioStreamAvailable(stream);
                        SDLTest_AssertCheck(0 == real_dst_len, "Verify available (pre-put); expected: %i; got: %i", 0, real_dst_len);

                        /* Run the audio converter */
                        if (SDL_PutAudioStreamData(stream, src_buf, src_len) < 0 ||
                                SDL_FlushAudioStream(stream) < 0) {
                            return TEST_ABORTED;
                        }

                        real_dst_len = SDL_GetAudioStreamAvailable(stream);
                        SDLTest_AssertCheck(dst_len == real_dst_len, "Verify available (post-put); expected: %i; got: %i", dst_len, real_dst_len);

                        real_dst_len = SDL_GetAudioStreamData(stream, dst_buf, dst_len);
                        SDLTest_AssertCheck(dst_len == real_dst_len, "Verify result value; expected: %i; got: %i", dst_len, real_dst_len);
                        if (dst_len != real_dst_len) {
                            return TEST_ABORTED;
                        }

                        real_dst_len = SDL_GetAudioStreamAvailable(stream);
                        SDLTest_AssertCheck(0 == real_dst_len, "Verify available (post-get); expected: %i; got: %i", 0, real_dst_len);

                        dst_silence = SDL_GetSilenceValueForFormat(spec2.format);

                        for (m = 0; m < dst_len; ++m) {
                            if (dst_buf[m] != dst_silence) {
                                SDLTest_LogError("Output buffer is not silent");
                                return TEST_ABORTED;
                            }
                        }

                        SDL_DestroyAudioStream(stream);
                        /* Free converted buffer */
                        SDL_free(src_buf);
                        SDL_free(dst_buf);
                    }
                }
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Opens, checks current connected status, and closes a device.
 *
 * \sa SDL_AudioDeviceConnected
 */
static int audio_openCloseAudioDeviceConnected(void *arg)
{
    return TEST_COMPLETED;  /* not a thing in SDL3. */
}

static double sine_wave_sample(const Sint64 idx, const Sint64 rate, const Sint64 freq, const double phase)
{
  /* Using integer modulo to avoid precision loss caused by large floating
   * point numbers. Sint64 is needed for the large integer multiplication.
   * The integers are assumed to be non-negative so that modulo is always
   * non-negative.
   *   sin(i / rate * freq * 2 * PI + phase)
   * = sin(mod(i / rate * freq, 1) * 2 * PI + phase)
   * = sin(mod(i * freq, rate) / rate * 2 * PI + phase) */
  return SDL_sin(((double)(idx * freq % rate)) / ((double)rate) * (SDL_PI_D * 2) + phase);
}

/**
 * Check signal-to-noise ratio and maximum error of audio resampling.
 *
 * \sa https://wiki.libsdl.org/SDL_CreateAudioStream
 * \sa https://wiki.libsdl.org/SDL_DestroyAudioStream
 * \sa https://wiki.libsdl.org/SDL_PutAudioStreamData
 * \sa https://wiki.libsdl.org/SDL_FlushAudioStream
 * \sa https://wiki.libsdl.org/SDL_GetAudioStreamData
 */
static int audio_resampleLoss(void *arg)
{
  /* Note: always test long input time (>= 5s from experience) in some test
   * cases because an improper implementation may suffer from low resampling
   * precision with long input due to e.g. doing subtraction with large floats. */
  struct test_spec_t {
    int time;
    int freq;
    double phase;
    int rate_in;
    int rate_out;
    double signal_to_noise;
    double max_error;
  } test_specs[] = {
    { 50, 440, 0, 44100, 48000, 80, 0.0009 },
    { 50, 5000, SDL_PI_D / 2, 20000, 10000, 999, 0.0001 },
    { 50, 440, 0, 22050, 96000, 79, 0.0120 },
    { 50, 440, 0, 96000, 22050, 80, 0.0002 },
    { 0 }
  };

  int spec_idx = 0;
  int min_channels = 1;
  int max_channels = 1 /*8*/;
  int num_channels = min_channels;

  for (spec_idx = 0; test_specs[spec_idx].time > 0;) {
    const struct test_spec_t *spec = &test_specs[spec_idx];
    const int frames_in = spec->time * spec->rate_in;
    const int frames_target = spec->time * spec->rate_out;
    const int len_in = (frames_in * num_channels) * (int)sizeof(float);
    const int len_target = (frames_target * num_channels) * (int)sizeof(float);

    SDL_AudioSpec tmpspec1, tmpspec2;
    Uint64 tick_beg = 0;
    Uint64 tick_end = 0;
    int i = 0;
    int j = 0;
    int ret = 0;
    SDL_AudioStream *stream = NULL;
    float *buf_in = NULL;
    float *buf_out = NULL;
    int len_out = 0;
    double max_error = 0;
    double sum_squared_error = 0;
    double sum_squared_value = 0;
    double signal_to_noise = 0;

    SDLTest_AssertPass("Test resampling of %i s %i Hz %f phase sine wave from sampling rate of %i Hz to %i Hz",
                       spec->time, spec->freq, spec->phase, spec->rate_in, spec->rate_out);

    tmpspec1.format = SDL_AUDIO_F32;
    tmpspec1.channels = num_channels;
    tmpspec1.freq = spec->rate_in;
    tmpspec2.format = SDL_AUDIO_F32;
    tmpspec2.channels = num_channels;
    tmpspec2.freq = spec->rate_out;
    stream = SDL_CreateAudioStream(&tmpspec1, &tmpspec2);
    SDLTest_AssertPass("Call to SDL_CreateAudioStream(SDL_AUDIO_F32, 1, %i, SDL_AUDIO_F32, 1, %i)", spec->rate_in, spec->rate_out);
    SDLTest_AssertCheck(stream != NULL, "Expected SDL_CreateAudioStream to succeed.");
    if (stream == NULL) {
      return TEST_ABORTED;
    }

    buf_in = (float *)SDL_malloc(len_in);
    SDLTest_AssertCheck(buf_in != NULL, "Expected input buffer to be created.");
    if (buf_in == NULL) {
      SDL_DestroyAudioStream(stream);
      return TEST_ABORTED;
    }

    for (i = 0; i < frames_in; ++i) {
      float f = (float)sine_wave_sample(i, spec->rate_in, spec->freq, spec->phase);
      for (j = 0; j < num_channels; ++j) {
        *(buf_in + (i * num_channels) + j) = f;
      }
    }

    tick_beg = SDL_GetPerformanceCounter();

    ret = SDL_PutAudioStreamData(stream, buf_in, len_in);
    SDLTest_AssertPass("Call to SDL_PutAudioStreamData(stream, buf_in, %i)", len_in);
    SDLTest_AssertCheck(ret == 0, "Expected SDL_PutAudioStreamData to succeed.");
    SDL_free(buf_in);
    if (ret != 0) {
      SDL_DestroyAudioStream(stream);
      return TEST_ABORTED;
    }

    ret = SDL_FlushAudioStream(stream);
    SDLTest_AssertPass("Call to SDL_FlushAudioStream(stream)");
    SDLTest_AssertCheck(ret == 0, "Expected SDL_FlushAudioStream to succeed");
    if (ret != 0) {
      SDL_DestroyAudioStream(stream);
      return TEST_ABORTED;
    }

    buf_out = (float *)SDL_malloc(len_target);
    SDLTest_AssertCheck(buf_out != NULL, "Expected output buffer to be created.");
    if (buf_out == NULL) {
      SDL_DestroyAudioStream(stream);
      return TEST_ABORTED;
    }

    len_out = SDL_GetAudioStreamData(stream, buf_out, len_target);
    SDLTest_AssertPass("Call to SDL_GetAudioStreamData(stream, buf_out, %i)", len_target);
    SDLTest_AssertCheck(len_out == len_target, "Expected output length to be no larger than %i, got %i.",
                        len_target, len_out);
    SDL_DestroyAudioStream(stream);
    if (len_out > len_target) {
      SDL_free(buf_out);
      return TEST_ABORTED;
    }

    tick_end = SDL_GetPerformanceCounter();
    SDLTest_Log("Resampling used %f seconds.", ((double)(tick_end - tick_beg)) / SDL_GetPerformanceFrequency());

    for (i = 0; i < frames_target; ++i) {
        const double target = sine_wave_sample(i, spec->rate_out, spec->freq, spec->phase);
        for (j = 0; j < num_channels; ++j) {
            const float output = *(buf_out + (i * num_channels) + j);
            const double error = SDL_fabs(target - output);
            max_error = SDL_max(max_error, error);
            sum_squared_error += error * error;
            sum_squared_value += target * target;
        }
    }
    SDL_free(buf_out);
    signal_to_noise = 10 * SDL_log10(sum_squared_value / sum_squared_error); /* decibel */
    SDLTest_AssertCheck(isfinite(sum_squared_value), "Sum of squared target should be finite.");
    SDLTest_AssertCheck(isfinite(sum_squared_error), "Sum of squared error should be finite.");
    /* Infinity is theoretically possible when there is very little to no noise */
    SDLTest_AssertCheck(!isnan(signal_to_noise), "Signal-to-noise ratio should not be NaN.");
    SDLTest_AssertCheck(isfinite(max_error), "Maximum conversion error should be finite.");
    SDLTest_AssertCheck(signal_to_noise >= spec->signal_to_noise, "Conversion signal-to-noise ratio %f dB should be no less than %f dB.",
                        signal_to_noise, spec->signal_to_noise);
    SDLTest_AssertCheck(max_error <= spec->max_error, "Maximum conversion error %f should be no more than %f.",
                        max_error, spec->max_error);

    if (++num_channels > max_channels) {
        num_channels = min_channels;
        ++spec_idx;
    }
  }

  return TEST_COMPLETED;
}

/**
 * Check accuracy converting between audio formats.
 *
 * \sa SDL_ConvertAudioSamples
 */
static int audio_convertAccuracy(void *arg)
{
    static SDL_AudioFormat formats[] = { SDL_AUDIO_S8, SDL_AUDIO_U8, SDL_AUDIO_S16, SDL_AUDIO_S32 };
    static const char* format_names[] = { "S8", "U8", "S16", "S32" };

    int src_num = 65537 + 2048 + 48 + 256 + 100000;
    int src_len = src_num * sizeof(float);
    float* src_data = SDL_malloc(src_len);
    int i, j;

    SDLTest_AssertCheck(src_data != NULL, "Expected source buffer to be created.");
    if (src_data == NULL) {
        return TEST_ABORTED;
    }

    j = 0;

    /* Generate a uniform range of floats between [-1.0, 1.0] */
    for (i = 0; i < 65537; ++i) {
        src_data[j++] = ((float)i - 32768.0f) / 32768.0f;
    }

    /* Generate floats close to 1.0 */
    const float max_val = 16777216.0f;

    for (i = 0; i < 1024; ++i) {
        float f = (max_val + (float)(512 - i)) / max_val;
        src_data[j++] =  f;
        src_data[j++] = -f;
    }

    for (i = 0; i < 24; ++i) {
        float f = (max_val + (float)(3u << i)) / max_val;
        src_data[j++] =  f;
        src_data[j++] = -f;
    }

    /* Generate floats far outside the [-1.0, 1.0] range */
    for (i = 0; i < 128; ++i) {
        float f = 2.0f + (float) i;
        src_data[j++] =  f;
        src_data[j++] = -f;
    }

    /* Fill the rest with random floats between [-1.0, 1.0] */
    for (i = 0; i < 100000; ++i) {
        src_data[j++] = SDLTest_RandomSint32() / 2147483648.0f;
    }

    /* Shuffle the data for good measure */
    for (i = src_num - 1; i > 0; --i) {
        float f = src_data[i];
        j = SDLTest_RandomIntegerInRange(0, i);
        src_data[i] = src_data[j];
        src_data[j] = f;
    }

    for (i = 0; i < SDL_arraysize(formats); ++i) {
        SDL_AudioSpec src_spec, tmp_spec;
        Uint64 convert_begin, convert_end;
        Uint8 *tmp_data, *dst_data;
        int tmp_len, dst_len;
        int ret;

        SDL_AudioFormat format = formats[i];
        const char* format_name = format_names[i];

        /* Formats with > 23 bits can represent every value exactly */
        float min_delta =  1.0f;
        float max_delta = -1.0f;

        /* Subtract 1 bit to account for sign */
        int bits = SDL_AUDIO_BITSIZE(format) - 1;
        float target_max_delta = (bits > 23) ? 0.0f : (1.0f / (float)(1 << bits));
        float target_min_delta = -target_max_delta;

        src_spec.format = SDL_AUDIO_F32;
        src_spec.channels = 1;
        src_spec.freq = 44100;

        tmp_spec.format = format;
        tmp_spec.channels = 1;
        tmp_spec.freq = 44100;

        convert_begin = SDL_GetPerformanceCounter();

        tmp_data = NULL;
        tmp_len = 0;
        ret = SDL_ConvertAudioSamples(&src_spec, (const Uint8*) src_data, src_len, &tmp_spec, &tmp_data, &tmp_len);
        SDLTest_AssertCheck(ret == 0, "Expected SDL_ConvertAudioSamples(F32->%s) to succeed", format_name);
        if (ret != 0) {
            SDL_free(src_data);
            return TEST_ABORTED;
        }

        dst_data = NULL;
        dst_len = 0;
        ret = SDL_ConvertAudioSamples(&tmp_spec, tmp_data, tmp_len, &src_spec, &dst_data, &dst_len);
        SDLTest_AssertCheck(ret == 0, "Expected SDL_ConvertAudioSamples(%s->F32) to succeed", format_name);
        if (ret != 0) {
            SDL_free(tmp_data);
            SDL_free(src_data);
            return TEST_ABORTED;
        }

        convert_end = SDL_GetPerformanceCounter();
        SDLTest_Log("Conversion via %s took %f seconds.", format_name, ((double)(convert_end - convert_begin)) / SDL_GetPerformanceFrequency());

        SDL_free(tmp_data);

        for (j = 0; j < src_num; ++j) {
            float x = src_data[j];
            float y = ((float*)dst_data)[j];
            float d = SDL_clamp(x, -1.0f, 1.0f) - y;

            min_delta = SDL_min(min_delta, d);
            max_delta = SDL_max(max_delta, d);
        }

        SDLTest_AssertCheck(min_delta >= target_min_delta, "%s has min delta of %+f, should be >= %+f", format_name, min_delta, target_min_delta);
        SDLTest_AssertCheck(max_delta <= target_max_delta, "%s has max delta of %+f, should be <= %+f", format_name, max_delta, target_max_delta);

        SDL_free(dst_data);
    }

    SDL_free(src_data);

    return TEST_COMPLETED;
}

/**
 * Check accuracy when switching between formats
 *
 * \sa SDL_SetAudioStreamFormat
 */
static int audio_formatChange(void *arg)
{
    int i;
    SDL_AudioSpec spec1, spec2, spec3;
    int frames_1, frames_2, frames_3;
    int length_1, length_2, length_3;
    int retval = 0;
    int status = TEST_ABORTED;
    float* buffer_1 = NULL;
    float* buffer_2 = NULL;
    float* buffer_3 = NULL;
    SDL_AudioStream* stream = NULL;
    double max_error = 0;
    double sum_squared_error = 0;
    double sum_squared_value = 0;
    double signal_to_noise = 0;
    double target_max_error = 0.02;
    double target_signal_to_noise = 75.0;
    int sine_freq = 500;

    spec1.format = SDL_AUDIO_F32;
    spec1.channels = 1;
    spec1.freq = 20000;

    spec2.format = SDL_AUDIO_F32;
    spec2.channels = 1;
    spec2.freq = 40000;

    spec3.format = SDL_AUDIO_F32;
    spec3.channels = 1;
    spec3.freq = 80000;

    frames_1 = spec1.freq;
    frames_2 = spec2.freq;
    frames_3 = spec3.freq * 2;

    length_1 = (int)(frames_1 * sizeof(*buffer_1));
    buffer_1 = (float*) SDL_malloc(length_1);
    if (!SDLTest_AssertCheck(buffer_1 != NULL, "Expected buffer_1 to be created.")) {
        goto cleanup;
    }

    length_2 = (int)(frames_2 * sizeof(*buffer_2));
    buffer_2 = (float*) SDL_malloc(length_2);
    if (!SDLTest_AssertCheck(buffer_2 != NULL, "Expected buffer_2 to be created.")) {
        goto cleanup;
    }

    length_3 = (int)(frames_3 * sizeof(*buffer_3));
    buffer_3 = (float*) SDL_malloc(length_3);
    if (!SDLTest_AssertCheck(buffer_3 != NULL, "Expected buffer_3 to be created.")) {
        goto cleanup;
    }

    for (i = 0; i < frames_1; ++i) {
        buffer_1[i] = (float) sine_wave_sample(i, spec1.freq, sine_freq, 0.0f);
    }

    for (i = 0; i < frames_2; ++i) {
        buffer_2[i] = (float) sine_wave_sample(i, spec2.freq, sine_freq, 0.0f);
    }

    stream = SDL_CreateAudioStream(NULL, NULL);
    if (!SDLTest_AssertCheck(stream != NULL, "Expected SDL_CreateAudioStream to succeed")) {
        goto cleanup;
    }

    retval = SDL_SetAudioStreamFormat(stream, &spec1, &spec3);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_SetAudioStreamFormat(spec1, spec3) to succeed")) {
        goto cleanup;
    }

    retval = SDL_GetAudioStreamAvailable(stream);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_GetAudioStreamAvailable return 0")) {
        goto cleanup;
    }

    retval = SDL_PutAudioStreamData(stream, buffer_1, length_1);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_PutAudioStreamData(buffer_1) to succeed")) {
        goto cleanup;
    }

    retval = SDL_FlushAudioStream(stream);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_FlushAudioStream to succeed")) {
        goto cleanup;
    }

    retval = SDL_SetAudioStreamFormat(stream, &spec2, &spec3);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_SetAudioStreamFormat(spec2, spec3) to succeed")) {
        goto cleanup;
    }

    retval = SDL_PutAudioStreamData(stream, buffer_2, length_2);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_PutAudioStreamData(buffer_1) to succeed")) {
        goto cleanup;
    }

    retval = SDL_FlushAudioStream(stream);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_FlushAudioStream to succeed")) {
        goto cleanup;
    }

    retval = SDL_GetAudioStreamAvailable(stream);
    if (!SDLTest_AssertCheck(retval == length_3, "Expected SDL_GetAudioStreamAvailable to return %i, got %i", length_3, retval)) {
        goto cleanup;
    }

    retval = SDL_GetAudioStreamData(stream, buffer_3, length_3);
    if (!SDLTest_AssertCheck(retval == length_3, "Expected SDL_GetAudioStreamData to return %i, got %i", length_3, retval)) {
        goto cleanup;
    }

    retval = SDL_GetAudioStreamAvailable(stream);
    if (!SDLTest_AssertCheck(retval == 0, "Expected SDL_GetAudioStreamAvailable to return 0")) {
        goto cleanup;
    }

    for (i = 0; i < frames_3; ++i) {
        const float output = buffer_3[i];
        const float target = (float) sine_wave_sample(i, spec3.freq, sine_freq, 0.0f);
        const double error = SDL_fabs(target - output);
        max_error = SDL_max(max_error, error);
        sum_squared_error += error * error;
        sum_squared_value += target * target;
    }

    signal_to_noise = 10 * SDL_log10(sum_squared_value / sum_squared_error); /* decibel */
    SDLTest_AssertCheck(isfinite(sum_squared_value), "Sum of squared target should be finite.");
    SDLTest_AssertCheck(isfinite(sum_squared_error), "Sum of squared error should be finite.");
    /* Infinity is theoretically possible when there is very little to no noise */
    SDLTest_AssertCheck(!isnan(signal_to_noise), "Signal-to-noise ratio should not be NaN.");
    SDLTest_AssertCheck(isfinite(max_error), "Maximum conversion error should be finite.");
    SDLTest_AssertCheck(signal_to_noise >= target_signal_to_noise, "Conversion signal-to-noise ratio %f dB should be no less than %f dB.",
                        signal_to_noise, target_signal_to_noise);
    SDLTest_AssertCheck(max_error <= target_max_error, "Maximum conversion error %f should be no more than %f.",
                        max_error, target_max_error);

    status = TEST_COMPLETED;

cleanup:
    SDL_free(buffer_1);
    SDL_free(buffer_2);
    SDL_free(buffer_3);
    SDL_DestroyAudioStream(stream);

    return status;
}
/* ================= Test Case References ================== */

/* Audio test cases */
static const SDLTest_TestCaseReference audioTest1 = {
    audio_enumerateAndNameAudioDevices, "audio_enumerateAndNameAudioDevices", "Enumerate and name available audio devices (output and capture)", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest2 = {
    audio_enumerateAndNameAudioDevicesNegativeTests, "audio_enumerateAndNameAudioDevicesNegativeTests", "Negative tests around enumeration and naming of audio devices.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest3 = {
    audio_printAudioDrivers, "audio_printAudioDrivers", "Checks available audio driver names.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest4 = {
    audio_printCurrentAudioDriver, "audio_printCurrentAudioDriver", "Checks current audio driver name with initialized audio.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest5 = {
    audio_buildAudioStream, "audio_buildAudioStream", "Builds various audio conversion structures.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest6 = {
    audio_buildAudioStreamNegative, "audio_buildAudioStreamNegative", "Checks calls with invalid input to SDL_CreateAudioStream", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest7 = {
    audio_getAudioStatus, "audio_getAudioStatus", "Checks current audio status.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest8 = {
    audio_openCloseAndGetAudioStatus, "audio_openCloseAndGetAudioStatus", "Opens and closes audio device and get audio status.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest9 = {
    audio_lockUnlockOpenAudioDevice, "audio_lockUnlockOpenAudioDevice", "Locks and unlocks an open audio device.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest10 = {
    audio_convertAudio, "audio_convertAudio", "Convert audio using available formats.", TEST_ENABLED
};

/* TODO: enable test when SDL_AudioDeviceConnected has been implemented.           */

static const SDLTest_TestCaseReference audioTest11 = {
    audio_openCloseAudioDeviceConnected, "audio_openCloseAudioDeviceConnected", "Opens and closes audio device and get connected status.", TEST_DISABLED
};

static const SDLTest_TestCaseReference audioTest12 = {
    audio_quitInitAudioSubSystem, "audio_quitInitAudioSubSystem", "Quit and re-init audio subsystem.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest13 = {
    audio_initQuitAudio, "audio_initQuitAudio", "Init and quit audio drivers directly.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest14 = {
    audio_initOpenCloseQuitAudio, "audio_initOpenCloseQuitAudio", "Cycle through init, open, close and quit with various audio specs.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest15 = {
    audio_pauseUnpauseAudio, "audio_pauseUnpauseAudio", "Pause and Unpause audio for various audio specs while testing callback.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest16 = {
    audio_resampleLoss, "audio_resampleLoss", "Check signal-to-noise ratio and maximum error of audio resampling.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest17 = {
    audio_convertAccuracy, "audio_convertAccuracy", "Check accuracy converting between audio formats.", TEST_ENABLED
};

static const SDLTest_TestCaseReference audioTest18 = {
    audio_formatChange, "audio_formatChange", "Check handling of format changes.", TEST_ENABLED
};

/* Sequence of Audio test cases */
static const SDLTest_TestCaseReference *audioTests[] = {
    &audioTest1, &audioTest2, &audioTest3, &audioTest4, &audioTest5, &audioTest6,
    &audioTest7, &audioTest8, &audioTest9, &audioTest10, &audioTest11,
    &audioTest12, &audioTest13, &audioTest14, &audioTest15, &audioTest16,
    &audioTest17, &audioTest18, NULL
};

/* Audio test suite (global) */
SDLTest_TestSuiteReference audioTestSuite = {
    "Audio",
    audioSetUp,
    audioTests,
    audioTearDown
};
