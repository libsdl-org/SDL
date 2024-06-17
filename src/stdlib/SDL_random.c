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

/* This file contains portable random functions for SDL */

static Uint64 SDL_rand_state;
static SDL_bool SDL_rand_initialized = SDL_FALSE;

void SDL_srand(Uint64 seed)
{
    if (!seed) {
        seed = SDL_GetPerformanceCounter();
    }
    SDL_rand_state = seed;
    SDL_rand_initialized = SDL_TRUE;
}

Uint32 SDL_rand(void)
{
    if(!SDL_rand_initialized) {
        SDL_srand(0);
    }
    return SDL_rand_r(&SDL_rand_state);
}

/*
 * Return a number between [0, n)
 * Fast but slightly biased. Don't run your casino with this.
 */
Sint32 SDL_rand_n(Sint32 n)
{
	// On 32-bit arch, the compiler will optimize to a single 32-bit multiply
	Uint64 val = (Uint64)SDL_rand() * n;
	return (Sint32)(val >> 32);
}

/*
 * Random float in range [0,1)
 */
float SDL_rand_float(void)
{
	return (SDL_rand() >> (32-24)) * 0x1p-24f;
}

/* A fast psuedo-random number generator.
 * Not suitable for cryptography or gambling
 */
Uint32 SDL_rand_r(Uint64 *state)
{
    if (!state) {
        return 0;
    }

	// Multiplier from Table 6 of
	// Steele GL, Vigna S. Computationally easy, spectrally good multipliers
	// for congruential pseudorandom number generators.
	// Softw Pract Exper. 2022;52(2):443-458. doi: 10.1002/spe.3030

	// 32-bit 'a' improves performance on 32-bit architectures
	// 'c' can be any odd number, but < 256 generates smaller code on some arch
	*state = *state * 0xf9b25d65ul + 0xFD;

	// Only return top 32 bits because they have a longer period
	return (Uint32)(*state >> 32);
}
