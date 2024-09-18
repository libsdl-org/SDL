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

/*

  Data generators for fuzzing test data in a reproducible way.

*/
#include <SDL3/SDL_test.h>

#include <float.h>  /* Needed for FLT_MAX and DBL_EPSILON */
#include <limits.h> /* Needed for UCHAR_MAX, etc. */

/**
 * Counter for fuzzer invocations
 */
static int fuzzerInvocationCounter = 0;

/**
 * Context for shared random number generator
 */
static uint64_t rndContext;

/*
 * Note: doxygen documentation markup for functions is in the header file.
 */

void SDLTest_FuzzerInit(uint64_t execKey)
{
    rndContext = execKey;
    fuzzerInvocationCounter = 0;
}

int SDLTest_GetFuzzerInvocationCount(void)
{
    return fuzzerInvocationCounter;
}

uint8_t SDLTest_RandomUint8(void)
{
    fuzzerInvocationCounter++;

    return (uint8_t)(SDL_rand_bits_r(&rndContext) >> 24);
}

int8_t SDLTest_RandomSint8(void)
{
    fuzzerInvocationCounter++;

    return (int8_t)(SDL_rand_bits_r(&rndContext) >> 24);
}

uint16_t SDLTest_RandomUint16(void)
{
    fuzzerInvocationCounter++;

    return (uint16_t)(SDL_rand_bits_r(&rndContext) >> 16);
}

int16_t SDLTest_RandomSint16(void)
{
    fuzzerInvocationCounter++;

    return (int16_t)(SDL_rand_bits_r(&rndContext) >> 16);
}

uint32_t SDLTest_RandomUint32(void)
{
    fuzzerInvocationCounter++;

    return SDL_rand_bits_r(&rndContext);
}

int32_t SDLTest_RandomSint32(void)
{
    fuzzerInvocationCounter++;

    return (int32_t)SDL_rand_bits_r(&rndContext);
}

uint64_t SDLTest_RandomUint64(void)
{
    union
    {
        uint64_t v64;
        uint32_t v32[2];
    } value;

    fuzzerInvocationCounter++;

    value.v32[0] = SDLTest_RandomUint32();
    value.v32[1] = SDLTest_RandomUint32();

    return value.v64;
}

int64_t SDLTest_RandomSint64(void)
{
    union
    {
        uint64_t v64;
        uint32_t v32[2];
    } value;

    fuzzerInvocationCounter++;

    value.v32[0] = SDLTest_RandomUint32();
    value.v32[1] = SDLTest_RandomUint32();

    return (int64_t)value.v64;
}

int32_t SDLTest_RandomIntegerInRange(int32_t min, int32_t max)
{
    fuzzerInvocationCounter++;

    if (min == max) {
        return min;
    }

    if (min > max) {
        int32_t temp = min;
        min = max;
        max = temp;
    }

    int32_t range = (max - min);
    SDL_assert(range < SDL_MAX_SINT32);
    return min + SDL_rand_r(&rndContext, range + 1);
}

/**
 * Generates a unsigned boundary value between the given boundaries.
 * Boundary values are inclusive. See the examples below.
 * If boundary2 < boundary1, the values are swapped.
 * If boundary1 == boundary2, value of boundary1 will be returned
 *
 * Generating boundary values for uint8_t:
 * BoundaryValues(UINT8_MAX, 10, 20, True) -> [10,11,19,20]
 * BoundaryValues(UINT8_MAX, 10, 20, False) -> [9,21]
 * BoundaryValues(UINT8_MAX, 0, 15, True) -> [0, 1, 14, 15]
 * BoundaryValues(UINT8_MAX, 0, 15, False) -> [16]
 * BoundaryValues(UINT8_MAX, 0, 0xFF, False) -> [0], error set
 *
 * Generator works the same for other types of unsigned integers.
 *
 * \param maxValue The biggest value that is acceptable for this data type.
 *                  For instance, for uint8_t -> 255, uint16_t -> 65536 etc.
 * \param boundary1 defines lower boundary
 * \param boundary2 defines upper boundary
 * \param validDomain Generate only for valid domain (for the data type)
 *
 * \returns Returns a random boundary value for the domain or 0 in case of error
 */
static uint64_t SDLTest_GenerateUnsignedBoundaryValues(const uint64_t maxValue, uint64_t boundary1, uint64_t boundary2, bool validDomain)
{
    uint64_t b1, b2;
    uint64_t delta;
    uint64_t tempBuf[4];
    uint8_t index;

    /* Maybe swap */
    if (boundary1 > boundary2) {
        b1 = boundary2;
        b2 = boundary1;
    } else {
        b1 = boundary1;
        b2 = boundary2;
    }

    index = 0;
    if (validDomain == true) {
        if (b1 == b2) {
            return b1;
        }

        /* Generate up to 4 values within bounds */
        delta = b2 - b1;
        if (delta < 4) {
            do {
                tempBuf[index] = b1 + index;
                index++;
            } while (index < delta);
        } else {
            tempBuf[index] = b1;
            index++;
            tempBuf[index] = b1 + 1;
            index++;
            tempBuf[index] = b2 - 1;
            index++;
            tempBuf[index] = b2;
            index++;
        }
    } else {
        /* Generate up to 2 values outside of bounds */
        if (b1 > 0) {
            tempBuf[index] = b1 - 1;
            index++;
        }

        if (b2 < maxValue) {
            tempBuf[index] = b2 + 1;
            index++;
        }
    }

    if (index == 0) {
        /* There are no valid boundaries */
        SDL_Unsupported();
        return 0;
    }

    return tempBuf[SDLTest_RandomUint8() % index];
}

uint8_t SDLTest_RandomUint8BoundaryValue(uint8_t boundary1, uint8_t boundary2, bool validDomain)
{
    /* max value for uint8_t */
    const uint64_t maxValue = UCHAR_MAX;
    return (uint8_t)SDLTest_GenerateUnsignedBoundaryValues(maxValue,
                                                         (uint64_t)boundary1, (uint64_t)boundary2,
                                                         validDomain);
}

uint16_t SDLTest_RandomUint16BoundaryValue(uint16_t boundary1, uint16_t boundary2, bool validDomain)
{
    /* max value for uint16_t */
    const uint64_t maxValue = USHRT_MAX;
    return (uint16_t)SDLTest_GenerateUnsignedBoundaryValues(maxValue,
                                                          (uint64_t)boundary1, (uint64_t)boundary2,
                                                          validDomain);
}

uint32_t SDLTest_RandomUint32BoundaryValue(uint32_t boundary1, uint32_t boundary2, bool validDomain)
{
/* max value for uint32_t */
#if ((ULONG_MAX) == (UINT_MAX))
    const uint64_t maxValue = ULONG_MAX;
#else
    const uint64_t maxValue = UINT_MAX;
#endif
    return (uint32_t)SDLTest_GenerateUnsignedBoundaryValues(maxValue,
                                                          (uint64_t)boundary1, (uint64_t)boundary2,
                                                          validDomain);
}

uint64_t SDLTest_RandomUint64BoundaryValue(uint64_t boundary1, uint64_t boundary2, bool validDomain)
{
    /* max value for uint64_t */
    const uint64_t maxValue = UINT64_MAX;
    return SDLTest_GenerateUnsignedBoundaryValues(maxValue,
                                                  boundary1, boundary2,
                                                  validDomain);
}

/**
 * Generates a signed boundary value between the given boundaries.
 * Boundary values are inclusive. See the examples below.
 * If boundary2 < boundary1, the values are swapped.
 * If boundary1 == boundary2, value of boundary1 will be returned
 *
 * Generating boundary values for int8_t:
 * SignedBoundaryValues(SCHAR_MIN, SCHAR_MAX, -10, 20, True) -> [-10,-9,19,20]
 * SignedBoundaryValues(SCHAR_MIN, SCHAR_MAX, -10, 20, False) -> [-11,21]
 * SignedBoundaryValues(SCHAR_MIN, SCHAR_MAX, -30, -15, True) -> [-30, -29, -16, -15]
 * SignedBoundaryValues(SCHAR_MIN, SCHAR_MAX, -127, 15, False) -> [16]
 * SignedBoundaryValues(SCHAR_MIN, SCHAR_MAX, -127, 127, False) -> [0], error set
 *
 * Generator works the same for other types of signed integers.
 *
 * \param minValue The smallest value that is acceptable for this data type.
 *                  For instance, for uint8_t -> -127, etc.
 * \param maxValue The biggest value that is acceptable for this data type.
 *                  For instance, for uint8_t -> 127, etc.
 * \param boundary1 defines lower boundary
 * \param boundary2 defines upper boundary
 * \param validDomain Generate only for valid domain (for the data type)
 *
 * \returns Returns a random boundary value for the domain or 0 in case of error
 */
static int64_t SDLTest_GenerateSignedBoundaryValues(const int64_t minValue, const int64_t maxValue, int64_t boundary1, int64_t boundary2, bool validDomain)
{
    int64_t b1, b2;
    int64_t delta;
    int64_t tempBuf[4];
    uint8_t index;

    /* Maybe swap */
    if (boundary1 > boundary2) {
        b1 = boundary2;
        b2 = boundary1;
    } else {
        b1 = boundary1;
        b2 = boundary2;
    }

    index = 0;
    if (validDomain == true) {
        if (b1 == b2) {
            return b1;
        }

        /* Generate up to 4 values within bounds */
        delta = b2 - b1;
        if (delta < 4) {
            do {
                tempBuf[index] = b1 + index;
                index++;
            } while (index < delta);
        } else {
            tempBuf[index] = b1;
            index++;
            tempBuf[index] = b1 + 1;
            index++;
            tempBuf[index] = b2 - 1;
            index++;
            tempBuf[index] = b2;
            index++;
        }
    } else {
        /* Generate up to 2 values outside of bounds */
        if (b1 > minValue) {
            tempBuf[index] = b1 - 1;
            index++;
        }

        if (b2 < maxValue) {
            tempBuf[index] = b2 + 1;
            index++;
        }
    }

    if (index == 0) {
        /* There are no valid boundaries */
        SDL_Unsupported();
        return minValue;
    }

    return tempBuf[SDLTest_RandomUint8() % index];
}

int8_t SDLTest_RandomSint8BoundaryValue(int8_t boundary1, int8_t boundary2, bool validDomain)
{
    /* min & max values for int8_t */
    const int64_t maxValue = SCHAR_MAX;
    const int64_t minValue = SCHAR_MIN;
    return (int8_t)SDLTest_GenerateSignedBoundaryValues(minValue, maxValue,
                                                       (int64_t)boundary1, (int64_t)boundary2,
                                                       validDomain);
}

int16_t SDLTest_RandomSint16BoundaryValue(int16_t boundary1, int16_t boundary2, bool validDomain)
{
    /* min & max values for int16_t */
    const int64_t maxValue = SHRT_MAX;
    const int64_t minValue = SHRT_MIN;
    return (int16_t)SDLTest_GenerateSignedBoundaryValues(minValue, maxValue,
                                                        (int64_t)boundary1, (int64_t)boundary2,
                                                        validDomain);
}

int32_t SDLTest_RandomSint32BoundaryValue(int32_t boundary1, int32_t boundary2, bool validDomain)
{
/* min & max values for int32_t */
#if ((ULONG_MAX) == (UINT_MAX))
    const int64_t maxValue = LONG_MAX;
    const int64_t minValue = LONG_MIN;
#else
    const int64_t maxValue = INT_MAX;
    const int64_t minValue = INT_MIN;
#endif
    return (int32_t)SDLTest_GenerateSignedBoundaryValues(minValue, maxValue,
                                                        (int64_t)boundary1, (int64_t)boundary2,
                                                        validDomain);
}

int64_t SDLTest_RandomSint64BoundaryValue(int64_t boundary1, int64_t boundary2, bool validDomain)
{
    /* min & max values for int64_t */
    const int64_t maxValue = INT64_MAX;
    const int64_t minValue = INT64_MIN;
    return SDLTest_GenerateSignedBoundaryValues(minValue, maxValue,
                                                boundary1, boundary2,
                                                validDomain);
}

float SDLTest_RandomUnitFloat(void)
{
    return SDL_randf_r(&rndContext);
}

float SDLTest_RandomFloat(void)
{
    union
    {
        float f;
        uint32_t v32;
    } value;

    do {
        value.v32 = SDLTest_RandomUint32();
    } while (SDL_isnanf(value.f) || SDL_isinff(value.f));

    return value.f;
}

double SDLTest_RandomUnitDouble(void)
{
    return (double)(SDLTest_RandomUint64() >> (64-53)) * 0x1.0p-53;
}

double SDLTest_RandomDouble(void)
{
    union
    {
        double d;
        uint64_t v64;
    } value;

    do {
        value.v64 = SDLTest_RandomUint64();
    } while (SDL_isnan(value.d) || SDL_isinf(value.d));

    return value.d;
}

char *SDLTest_RandomAsciiString(void)
{
    return SDLTest_RandomAsciiStringWithMaximumLength(255);
}

char *SDLTest_RandomAsciiStringWithMaximumLength(int maxLength)
{
    int size;

    if (maxLength < 1) {
        SDL_InvalidParamError("maxLength");
        return NULL;
    }

    size = (SDLTest_RandomUint32() % (maxLength + 1));
    if (size == 0) {
        size = 1;
    }
    return SDLTest_RandomAsciiStringOfSize(size);
}

char *SDLTest_RandomAsciiStringOfSize(int size)
{
    char *string;
    int counter;

    if (size < 1) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    string = (char *)SDL_malloc((size + 1) * sizeof(char));
    if (!string) {
        return NULL;
    }

    for (counter = 0; counter < size; ++counter) {
        string[counter] = (char)SDLTest_RandomIntegerInRange(32, 126);
    }

    string[counter] = '\0';

    fuzzerInvocationCounter++;

    return string;
}
