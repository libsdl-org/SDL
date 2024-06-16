/*
Copyright (c) 2008, Edgar Simo Serra
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the Simple Directmedia Layer (SDL) nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#include <stdlib.h>

static SDL_Haptic *haptic;
static SDLTest_CommonState *state;

/*
 * prototypes
 */
static void abort_execution(void);
static void HapticPrintSupported(SDL_Haptic *);

/**
 * The entry point of this force feedback demo.
 * \param[in] argc Number of arguments.
 * \param[in] argv Array of argc arguments.
 */
int main(int argc, char **argv)
{
    int i;
    char *name = NULL;
    int index = -1;
    SDL_HapticEffect efx[9];
    int id[9];
    int nefx;
    unsigned int supported;
    SDL_HapticID *haptics;
    int num_haptics;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!name && index < 0) {
                size_t len;
                name = argv[i];
                len = SDL_strlen(name);
                if (len < 3 && SDL_isdigit(name[0]) && (len == 1 || SDL_isdigit(name[1]))) {
                    index = SDL_atoi(name);
                    name = NULL;
                }
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[device]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            SDL_Log("\n");
            SDL_Log("If device is a two-digit number it'll use it as an index, otherwise\n"
                    "it'll use it as if it were part of the device's name.\n");
            return 1;
        }

        i += consumed;
    }

    /* Initialize the force feedbackness */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK |
             SDL_INIT_HAPTIC);
    haptics = SDL_GetHaptics(&num_haptics);
    SDL_Log("%d Haptic devices detected.\n", num_haptics);
    for (i = 0; i < num_haptics; ++i) {
        SDL_Log("    %s\n", SDL_GetHapticInstanceName(haptics[i]));
    }
    if (haptics) {
        if (num_haptics == 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No Haptic devices found!\n");
            SDL_free(haptics);
            return 1;
        }

        /* We'll just use index or the first force feedback device found */
        if (!name) {
            i = (index != -1) ? index : 0;

            if (i >= num_haptics) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Index out of range, aborting.\n");
                SDL_free(haptics);
                return 1;
            }
        }
        /* Try to find matching device */
        else {
            for (i = 0; i < num_haptics; i++) {
                if (SDL_strstr(SDL_GetHapticInstanceName(haptics[i]), name) != NULL) {
                    break;
                }
            }

            if (i >= num_haptics) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to find device matching '%s', aborting.\n", name);
                SDL_free(haptics);
                return 1;
            }
        }

        haptic = SDL_OpenHaptic(haptics[i]);
        if (!haptic) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create the haptic device: %s\n", SDL_GetError());
            SDL_free(haptics);
            return 1;
        }
        SDL_Log("Device: %s\n", SDL_GetHapticName(haptic));
        HapticPrintSupported(haptic);
        SDL_free(haptics);
    }

    /* We only want force feedback errors. */
    SDL_ClearError();

    /* Create effects. */
    SDL_memset(efx, 0, sizeof(efx));
    nefx = 0;
    supported = SDL_GetHapticFeatures(haptic);

    SDL_Log("\nUploading effects\n");
    /* First we'll try a SINE effect. */
    if (supported & SDL_HAPTIC_SINE) {
        SDL_Log("   effect %d: Sine Wave\n", nefx);
        efx[nefx].type = SDL_HAPTIC_SINE;
        efx[nefx].periodic.period = 1000;
        efx[nefx].periodic.magnitude = -0x2000; /* Negative magnitude and ...                      */
        efx[nefx].periodic.phase = 18000;       /* ... 180 degrees phase shift => cancel eachother */
        efx[nefx].periodic.length = 5000;
        efx[nefx].periodic.attack_length = 1000;
        efx[nefx].periodic.fade_length = 1000;
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }
    /* Now we'll try a SAWTOOTHUP */
    if (supported & SDL_HAPTIC_SAWTOOTHUP) {
        SDL_Log("   effect %d: Sawtooth Up\n", nefx);
        efx[nefx].type = SDL_HAPTIC_SAWTOOTHUP;
        efx[nefx].periodic.period = 500;
        efx[nefx].periodic.magnitude = 0x5000;
        efx[nefx].periodic.length = 5000;
        efx[nefx].periodic.attack_length = 1000;
        efx[nefx].periodic.fade_length = 1000;
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }

    /* Now the classical constant effect. */
    if (supported & SDL_HAPTIC_CONSTANT) {
        SDL_Log("   effect %d: Constant Force\n", nefx);
        efx[nefx].type = SDL_HAPTIC_CONSTANT;
        efx[nefx].constant.direction.type = SDL_HAPTIC_POLAR;
        efx[nefx].constant.direction.dir[0] = 20000; /* Force comes from the south-west. */
        efx[nefx].constant.length = 5000;
        efx[nefx].constant.level = 0x6000;
        efx[nefx].constant.attack_length = 1000;
        efx[nefx].constant.fade_length = 1000;
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }

    /* The cute spring effect. */
    if (supported & SDL_HAPTIC_SPRING) {
        SDL_Log("   effect %d: Condition Spring\n", nefx);
        efx[nefx].type = SDL_HAPTIC_SPRING;
        efx[nefx].condition.length = 5000;
        for (i = 0; i < SDL_GetNumHapticAxes(haptic); i++) {
            efx[nefx].condition.right_sat[i] = 0xFFFF;
            efx[nefx].condition.left_sat[i] = 0xFFFF;
            efx[nefx].condition.right_coeff[i] = 0x2000;
            efx[nefx].condition.left_coeff[i] = 0x2000;
            efx[nefx].condition.center[i] = 0x1000; /* Displace the center for it to move. */
        }
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }
    /* The interesting damper effect. */
    if (supported & SDL_HAPTIC_DAMPER) {
        SDL_Log("   effect %d: Condition Damper\n", nefx);
        efx[nefx].type = SDL_HAPTIC_DAMPER;
        efx[nefx].condition.length = 5000;
        for (i = 0; i < SDL_GetNumHapticAxes(haptic); i++) {
            efx[nefx].condition.right_sat[i] = 0xFFFF;
            efx[nefx].condition.left_sat[i] = 0xFFFF;
            efx[nefx].condition.right_coeff[i] = 0x2000;
            efx[nefx].condition.left_coeff[i] = 0x2000;
        }
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }
    /* The pretty awesome inertia effect. */
    if (supported & SDL_HAPTIC_INERTIA) {
        SDL_Log("   effect %d: Condition Inertia\n", nefx);
        efx[nefx].type = SDL_HAPTIC_INERTIA;
        efx[nefx].condition.length = 5000;
        for (i = 0; i < SDL_GetNumHapticAxes(haptic); i++) {
            efx[nefx].condition.right_sat[i] = 0xFFFF;
            efx[nefx].condition.left_sat[i] = 0xFFFF;
            efx[nefx].condition.right_coeff[i] = 0x2000;
            efx[nefx].condition.left_coeff[i] = 0x2000;
            efx[nefx].condition.deadband[i] = 0x1000; /* 1/16th of axis-range around the center is 'dead'. */
        }
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }
    /* The hot friction effect. */
    if (supported & SDL_HAPTIC_FRICTION) {
        SDL_Log("   effect %d: Condition Friction\n", nefx);
        efx[nefx].type = SDL_HAPTIC_FRICTION;
        efx[nefx].condition.length = 5000;
        for (i = 0; i < SDL_GetNumHapticAxes(haptic); i++) {
            efx[nefx].condition.right_sat[i] = 0xFFFF;
            efx[nefx].condition.left_sat[i] = 0xFFFF;
            efx[nefx].condition.right_coeff[i] = 0x2000;
            efx[nefx].condition.left_coeff[i] = 0x2000;
        }
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }

    /* Now we'll try a ramp effect */
    if (supported & SDL_HAPTIC_RAMP) {
        SDL_Log("   effect %d: Ramp\n", nefx);
        efx[nefx].type = SDL_HAPTIC_RAMP;
        efx[nefx].ramp.direction.type = SDL_HAPTIC_CARTESIAN;
        efx[nefx].ramp.direction.dir[0] = 1;  /* Force comes from                 */
        efx[nefx].ramp.direction.dir[1] = -1; /*                  the north-east. */
        efx[nefx].ramp.length = 5000;
        efx[nefx].ramp.start = 0x4000;
        efx[nefx].ramp.end = -0x4000;
        efx[nefx].ramp.attack_length = 1000;
        efx[nefx].ramp.fade_length = 1000;
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }

    /* Finally we'll try a left/right effect. */
    if (supported & SDL_HAPTIC_LEFTRIGHT) {
        SDL_Log("   effect %d: Left/Right\n", nefx);
        efx[nefx].type = SDL_HAPTIC_LEFTRIGHT;
        efx[nefx].leftright.length = 5000;
        efx[nefx].leftright.large_magnitude = 0x3000;
        efx[nefx].leftright.small_magnitude = 0xFFFF;
        id[nefx] = SDL_CreateHapticEffect(haptic, &efx[nefx]);
        if (id[nefx] < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "UPLOADING EFFECT ERROR: %s\n", SDL_GetError());
            abort_execution();
        }
        nefx++;
    }

    SDL_Log("\nNow playing effects for 5 seconds each with 1 second delay between\n");
    for (i = 0; i < nefx; i++) {
        SDL_Log("   Playing effect %d\n", i);
        SDL_RunHapticEffect(haptic, id[i], 1);
        SDL_Delay(6000); /* Effects only have length 5000 */
    }

    /* Quit */
    if (haptic) {
        SDL_CloseHaptic(haptic);
    }
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}

/**
 * Cleans up a bit.
 */
static void
abort_execution(void)
{
    SDL_Log("\nAborting program execution.\n");

    SDL_CloseHaptic(haptic);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    exit(1);
}

/**
 * Displays information about the haptic device.
 */
static void
HapticPrintSupported(SDL_Haptic *ptr)
{
    unsigned int supported;

    supported = SDL_GetHapticFeatures(ptr);
    SDL_Log("   Supported effects [%d effects, %d playing]:\n",
            SDL_GetMaxHapticEffects(ptr), SDL_GetMaxHapticEffectsPlaying(ptr));
    if (supported & SDL_HAPTIC_CONSTANT) {
        SDL_Log("      constant\n");
    }
    if (supported & SDL_HAPTIC_SINE) {
        SDL_Log("      sine\n");
    }
    if (supported & SDL_HAPTIC_SQUARE)
        SDL_Log("      square\n");
    if (supported & SDL_HAPTIC_TRIANGLE) {
        SDL_Log("      triangle\n");
    }
    if (supported & SDL_HAPTIC_SAWTOOTHUP) {
        SDL_Log("      sawtoothup\n");
    }
    if (supported & SDL_HAPTIC_SAWTOOTHDOWN) {
        SDL_Log("      sawtoothdown\n");
    }
    if (supported & SDL_HAPTIC_RAMP) {
        SDL_Log("      ramp\n");
    }
    if (supported & SDL_HAPTIC_FRICTION) {
        SDL_Log("      friction\n");
    }
    if (supported & SDL_HAPTIC_SPRING) {
        SDL_Log("      spring\n");
    }
    if (supported & SDL_HAPTIC_DAMPER) {
        SDL_Log("      damper\n");
    }
    if (supported & SDL_HAPTIC_INERTIA) {
        SDL_Log("      inertia\n");
    }
    if (supported & SDL_HAPTIC_CUSTOM) {
        SDL_Log("      custom\n");
    }
    if (supported & SDL_HAPTIC_LEFTRIGHT) {
        SDL_Log("      left/right\n");
    }
    SDL_Log("   Supported capabilities:\n");
    if (supported & SDL_HAPTIC_GAIN) {
        SDL_Log("      gain\n");
    }
    if (supported & SDL_HAPTIC_AUTOCENTER) {
        SDL_Log("      autocenter\n");
    }
    if (supported & SDL_HAPTIC_STATUS) {
        SDL_Log("      status\n");
    }
}
