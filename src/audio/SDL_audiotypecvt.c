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

#include "SDL_sysaudio.h"

#define DIVBY2147483648 0.0000000004656612873077392578125f // 0x1p-31f

// start fallback scalar converters

// This code requires that floats are in the IEEE-754 binary32 format
SDL_COMPILE_TIME_ASSERT(float_bits, sizeof(float) == sizeof(uint32_t));

union float_bits {
    uint32_t u32;
    float f32;
};

static void SDL_Convert_S8_to_F32_Scalar(float *dst, const int8_t *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S8", "F32");

    for (i = num_samples - 1; i >= 0; --i) {
        /* 1) Construct a float in the range [65536.0, 65538.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        union float_bits x;
        x.u32 = (uint8_t)src[i] ^ 0x47800080u;
        dst[i] = x.f32 - 65537.0f;
    }
}

static void SDL_Convert_U8_to_F32_Scalar(float *dst, const uint8_t *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("U8", "F32");

    for (i = num_samples - 1; i >= 0; --i) {
        /* 1) Construct a float in the range [65536.0, 65538.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        union float_bits x;
        x.u32 = src[i] ^ 0x47800000u;
        dst[i] = x.f32 - 65537.0f;
    }
}

static void SDL_Convert_S16_to_F32_Scalar(float *dst, const int16_t *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S16", "F32");

    for (i = num_samples - 1; i >= 0; --i) {
        /* 1) Construct a float in the range [256.0, 258.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        union float_bits x;
        x.u32 = (uint16_t)src[i] ^ 0x43808000u;
        dst[i] = x.f32 - 257.0f;
    }
}

static void SDL_Convert_S32_to_F32_Scalar(float *dst, const int32_t *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S32", "F32");

    for (i = num_samples - 1; i >= 0; --i) {
        dst[i] = (float)src[i] * DIVBY2147483648;
    }
}

// Create a bit-mask based on the sign-bit. Should optimize to a single arithmetic-shift-right
#define SIGNMASK(x) (uint32_t)(0u - ((uint32_t)(x) >> 31))

static void SDL_Convert_F32_to_S8_Scalar(int8_t *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S8");

    for (i = 0; i < num_samples; ++i) {
        /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
         * 2) Shift the integer range from [0x47BFFF80, 0x47C00080] to [-128, 128]
         * 3) Clamp the value to [-128, 127] */
        union float_bits x;
        x.f32 = src[i] + 98304.0f;

        uint32_t y = x.u32 - 0x47C00000u;
        uint32_t z = 0x7Fu - (y ^ SIGNMASK(y));
        y = y ^ (z & SIGNMASK(z));

        dst[i] = (int8_t)(y & 0xFF);
    }
}

static void SDL_Convert_F32_to_U8_Scalar(uint8_t *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "U8");

    for (i = 0; i < num_samples; ++i) {
        /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
         * 2) Shift the integer range from [0x47BFFF80, 0x47C00080] to [-128, 128]
         * 3) Clamp the value to [-128, 127]
         * 4) Shift the integer range from [-128, 127] to [0, 255] */
        union float_bits x;
        x.f32 = src[i] + 98304.0f;

        uint32_t y = x.u32 - 0x47C00000u;
        uint32_t z = 0x7Fu - (y ^ SIGNMASK(y));
        y = (y ^ 0x80u) ^ (z & SIGNMASK(z));

        dst[i] = (uint8_t)(y & 0xFF);
    }
}

static void SDL_Convert_F32_to_S16_Scalar(int16_t *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S16");

    for (i = 0; i < num_samples; ++i) {
        /* 1) Shift the float range from [-1.0, 1.0] to [383.0, 385.0]
         * 2) Shift the integer range from [0x43BF8000, 0x43C08000] to [-32768, 32768]
         * 3) Clamp values outside the [-32768, 32767] range */
        union float_bits x;
        x.f32 = src[i] + 384.0f;

        uint32_t y = x.u32 - 0x43C00000u;
        uint32_t z = 0x7FFFu - (y ^ SIGNMASK(y));
        y = y ^ (z & SIGNMASK(z));

        dst[i] = (int16_t)(y & 0xFFFF);
    }
}

static void SDL_Convert_F32_to_S32_Scalar(int32_t *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S32");

    for (i = 0; i < num_samples; ++i) {
        /* 1) Shift the float range from [-1.0, 1.0] to [-2147483648.0, 2147483648.0]
         * 2) Set values outside the [-2147483648.0, 2147483647.0] range to -2147483648.0
         * 3) Convert the float to an integer, and fixup values outside the valid range */
        union float_bits x;
        x.f32 = src[i];

        uint32_t y = x.u32 + 0x0F800000u;
        uint32_t z = y - 0xCF000000u;
        z &= SIGNMASK(y ^ z);
        x.u32 = y - z;

        dst[i] = (int32_t)x.f32 ^ (int32_t)SIGNMASK(z);
    }
}

#undef SIGNMASK

static void SDL_Convert_Swap16_Scalar(uint16_t* dst, const uint16_t* src, int num_samples)
{
    int i;

    for (i = 0; i < num_samples; ++i) {
        dst[i] = SDL_Swap16(src[i]);
    }
}

static void SDL_Convert_Swap32_Scalar(uint32_t* dst, const uint32_t* src, int num_samples)
{
    int i;

    for (i = 0; i < num_samples; ++i) {
        dst[i] = SDL_Swap32(src[i]);
    }
}

// end fallback scalar converters

// Convert forwards, when sizeof(*src) >= sizeof(*dst)
#define CONVERT_16_FWD(CVT1, CVT16)                          \
    int i = 0;                                               \
    if (num_samples >= 16) {                                 \
        while ((uintptr_t)(&dst[i]) & 15) { CVT1  ++i;     } \
        while ((i + 16) <= num_samples)   { CVT16 i += 16; } \
    }                                                        \
    while (i < num_samples)               { CVT1  ++i;     }

// Convert backwards, when sizeof(*src) <= sizeof(*dst)
#define CONVERT_16_REV(CVT1, CVT16)                          \
    int i = num_samples;                                     \
    if (i >= 16) {                                           \
        while ((uintptr_t)(&dst[i]) & 15) { --i;     CVT1  } \
        while (i >= 16)                   { i -= 16; CVT16 } \
    }                                                        \
    while (i > 0)                         { --i;     CVT1  }

#ifdef SDL_SSE2_INTRINSICS
static void SDL_TARGETING("sse2") SDL_Convert_S8_to_F32_SSE2(float *dst, const int8_t *src, int num_samples)
{
    /* 1) Flip the sign bit to convert from S8 to U8 format
     * 2) Construct a float in the range [65536.0, 65538.0)
     * 3) Shift the float range to [-1.0, 1.0)
     * dst[i] = i2f((src[i] ^ 0x80) | 0x47800000) - 65537.0 */
    const __m128i zero = _mm_setzero_si128();
    const __m128i flipper = _mm_set1_epi8(-0x80);
    const __m128i caster = _mm_set1_epi16(0x4780 /* 0x47800000 = f2i(65536.0) */);
    const __m128 offset = _mm_set1_ps(-65537.0);

    LOG_DEBUG_AUDIO_CONVERT("S8", "F32 (using SSE2)");

    CONVERT_16_REV({
        _mm_store_ss(&dst[i], _mm_add_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((uint8_t)src[i] ^ 0x47800080u)), offset));
    }, {
        const __m128i bytes = _mm_xor_si128(_mm_loadu_si128((const __m128i *)&src[i]), flipper);

        const __m128i shorts0 = _mm_unpacklo_epi8(bytes, zero);
        const __m128i shorts1 = _mm_unpackhi_epi8(bytes, zero);

        const __m128 floats0 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts0, caster)), offset);
        const __m128 floats1 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts0, caster)), offset);
        const __m128 floats2 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts1, caster)), offset);
        const __m128 floats3 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts1, caster)), offset);

        _mm_store_ps(&dst[i], floats0);
        _mm_store_ps(&dst[i + 4], floats1);
        _mm_store_ps(&dst[i + 8], floats2);
        _mm_store_ps(&dst[i + 12], floats3);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_U8_to_F32_SSE2(float *dst, const uint8_t *src, int num_samples)
{
    /* 1) Construct a float in the range [65536.0, 65538.0)
     * 2) Shift the float range to [-1.0, 1.0)
     * dst[i] = i2f(src[i] | 0x47800000) - 65537.0 */
    const __m128i zero = _mm_setzero_si128();
    const __m128i caster = _mm_set1_epi16(0x4780 /* 0x47800000 = f2i(65536.0) */);
    const __m128 offset = _mm_set1_ps(-65537.0);

    LOG_DEBUG_AUDIO_CONVERT("U8", "F32 (using SSE2)");

    CONVERT_16_REV({
        _mm_store_ss(&dst[i], _mm_add_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((uint8_t)src[i] ^ 0x47800000u)), offset));
    }, {
        const __m128i bytes = _mm_loadu_si128((const __m128i *)&src[i]);

        const __m128i shorts0 = _mm_unpacklo_epi8(bytes, zero);
        const __m128i shorts1 = _mm_unpackhi_epi8(bytes, zero);

        const __m128 floats0 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts0, caster)), offset);
        const __m128 floats1 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts0, caster)), offset);
        const __m128 floats2 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts1, caster)), offset);
        const __m128 floats3 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts1, caster)), offset);

        _mm_store_ps(&dst[i], floats0);
        _mm_store_ps(&dst[i + 4], floats1);
        _mm_store_ps(&dst[i + 8], floats2);
        _mm_store_ps(&dst[i + 12], floats3);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_S16_to_F32_SSE2(float *dst, const int16_t *src, int num_samples)
{
    /* 1) Flip the sign bit to convert from S16 to U16 format
     * 2) Construct a float in the range [256.0, 258.0)
     * 3) Shift the float range to [-1.0, 1.0)
     * dst[i] = i2f((src[i] ^ 0x8000) | 0x43800000) - 257.0 */
    const __m128i flipper = _mm_set1_epi16(-0x8000);
    const __m128i caster = _mm_set1_epi16(0x4380 /* 0x43800000 = f2i(256.0) */);
    const __m128 offset = _mm_set1_ps(-257.0f);

    LOG_DEBUG_AUDIO_CONVERT("S16", "F32 (using SSE2)");

    CONVERT_16_REV({
        _mm_store_ss(&dst[i], _mm_add_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((uint16_t)src[i] ^ 0x43808000u)), offset));
    }, {
        const __m128i shorts0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)&src[i]), flipper);
        const __m128i shorts1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)&src[i + 8]), flipper);

        const __m128 floats0 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts0, caster)), offset);
        const __m128 floats1 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts0, caster)), offset);
        const __m128 floats2 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts1, caster)), offset);
        const __m128 floats3 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts1, caster)), offset);

        _mm_store_ps(&dst[i], floats0);
        _mm_store_ps(&dst[i + 4], floats1);
        _mm_store_ps(&dst[i + 8], floats2);
        _mm_store_ps(&dst[i + 12], floats3);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_S32_to_F32_SSE2(float *dst, const int32_t *src, int num_samples)
{
    // dst[i] = f32(src[i]) / f32(0x80000000)
    const __m128 scaler = _mm_set1_ps(DIVBY2147483648);

    LOG_DEBUG_AUDIO_CONVERT("S32", "F32 (using SSE2)");

    CONVERT_16_FWD({
        _mm_store_ss(&dst[i], _mm_mul_ss(_mm_cvt_si2ss(_mm_setzero_ps(), src[i]), scaler));
    }, {
        const __m128i ints0 = _mm_loadu_si128((const __m128i *)&src[i]);
        const __m128i ints1 = _mm_loadu_si128((const __m128i *)&src[i + 4]);
        const __m128i ints2 = _mm_loadu_si128((const __m128i *)&src[i + 8]);
        const __m128i ints3 = _mm_loadu_si128((const __m128i *)&src[i + 12]);

        const __m128 floats0 = _mm_mul_ps(_mm_cvtepi32_ps(ints0), scaler);
        const __m128 floats1 = _mm_mul_ps(_mm_cvtepi32_ps(ints1), scaler);
        const __m128 floats2 = _mm_mul_ps(_mm_cvtepi32_ps(ints2), scaler);
        const __m128 floats3 = _mm_mul_ps(_mm_cvtepi32_ps(ints3), scaler);

        _mm_store_ps(&dst[i], floats0);
        _mm_store_ps(&dst[i + 4], floats1);
        _mm_store_ps(&dst[i + 8], floats2);
        _mm_store_ps(&dst[i + 12], floats3);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_S8_SSE2(int8_t *dst, const float *src, int num_samples)
{
    /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
     * 2) Extract the lowest 16 bits and clamp to [-128, 127]
     * Overflow is correctly handled for inputs between roughly [-255.0, 255.0]
     * dst[i] = clamp(i16(f2i(src[i] + 98304.0) & 0xFFFF), -128, 127) */
    const __m128 offset = _mm_set1_ps(98304.0f);
    const __m128i mask = _mm_set1_epi16(0xFF);

    LOG_DEBUG_AUDIO_CONVERT("F32", "S8 (using SSE2)");

    CONVERT_16_FWD({
        const __m128i ints = _mm_castps_si128(_mm_add_ss(_mm_load_ss(&src[i]), offset));
        dst[i] = (int8_t)(_mm_cvtsi128_si32(_mm_packs_epi16(ints, ints)) & 0xFF);
    }, {
        const __m128 floats0 = _mm_loadu_ps(&src[i]);
        const __m128 floats1 = _mm_loadu_ps(&src[i + 4]);
        const __m128 floats2 = _mm_loadu_ps(&src[i + 8]);
        const __m128 floats3 = _mm_loadu_ps(&src[i + 12]);

        const __m128i ints0 = _mm_castps_si128(_mm_add_ps(floats0, offset));
        const __m128i ints1 = _mm_castps_si128(_mm_add_ps(floats1, offset));
        const __m128i ints2 = _mm_castps_si128(_mm_add_ps(floats2, offset));
        const __m128i ints3 = _mm_castps_si128(_mm_add_ps(floats3, offset));

        const __m128i shorts0 = _mm_and_si128(_mm_packs_epi16(ints0, ints1), mask);
        const __m128i shorts1 = _mm_and_si128(_mm_packs_epi16(ints2, ints3), mask);

        const __m128i bytes = _mm_packus_epi16(shorts0, shorts1);

        _mm_store_si128((__m128i*)&dst[i], bytes);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_U8_SSE2(uint8_t *dst, const float *src, int num_samples)
{
    /* 1) Shift the float range from [-1.0, 1.0] to [98304.0, 98306.0]
     * 2) Extract the lowest 16 bits and clamp to [0, 255]
     * Overflow is correctly handled for inputs between roughly [-254.0, 254.0]
     * dst[i] = clamp(i16(f2i(src[i] + 98305.0) & 0xFFFF), 0, 255) */
    const __m128 offset = _mm_set1_ps(98305.0f);
    const __m128i mask = _mm_set1_epi16(0xFF);

    LOG_DEBUG_AUDIO_CONVERT("F32", "U8 (using SSE2)");

    CONVERT_16_FWD({
        const __m128i ints = _mm_castps_si128(_mm_add_ss(_mm_load_ss(&src[i]), offset));
        dst[i] = (uint8_t)(_mm_cvtsi128_si32(_mm_packus_epi16(ints, ints)) & 0xFF);
    }, {
        const __m128 floats0 = _mm_loadu_ps(&src[i]);
        const __m128 floats1 = _mm_loadu_ps(&src[i + 4]);
        const __m128 floats2 = _mm_loadu_ps(&src[i + 8]);
        const __m128 floats3 = _mm_loadu_ps(&src[i + 12]);

        const __m128i ints0 = _mm_castps_si128(_mm_add_ps(floats0, offset));
        const __m128i ints1 = _mm_castps_si128(_mm_add_ps(floats1, offset));
        const __m128i ints2 = _mm_castps_si128(_mm_add_ps(floats2, offset));
        const __m128i ints3 = _mm_castps_si128(_mm_add_ps(floats3, offset));

        const __m128i shorts0 = _mm_and_si128(_mm_packus_epi16(ints0, ints1), mask);
        const __m128i shorts1 = _mm_and_si128(_mm_packus_epi16(ints2, ints3), mask);

        const __m128i bytes = _mm_packus_epi16(shorts0, shorts1);

        _mm_store_si128((__m128i*)&dst[i], bytes);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_S16_SSE2(int16_t *dst, const float *src, int num_samples)
{
    /* 1) Shift the float range from [-1.0, 1.0] to [256.0, 258.0]
     * 2) Shift the int range from [0x43800000, 0x43810000] to [-32768,32768]
     * 3) Clamp to range [-32768,32767]
     * Overflow is correctly handled for inputs between roughly [-257.0, +inf)
     * dst[i] = clamp(f2i(src[i] + 257.0) - 0x43808000, -32768, 32767) */
    const __m128 offset = _mm_set1_ps(257.0f);

    LOG_DEBUG_AUDIO_CONVERT("F32", "S16 (using SSE2)");

    CONVERT_16_FWD({
        const __m128i ints = _mm_sub_epi32(_mm_castps_si128(_mm_add_ss(_mm_load_ss(&src[i]), offset)), _mm_castps_si128(offset));
        dst[i] = (int16_t)(_mm_cvtsi128_si32(_mm_packs_epi32(ints, ints)) & 0xFFFF);
    }, {
        const __m128 floats0 = _mm_loadu_ps(&src[i]);
        const __m128 floats1 = _mm_loadu_ps(&src[i + 4]);
        const __m128 floats2 = _mm_loadu_ps(&src[i + 8]);
        const __m128 floats3 = _mm_loadu_ps(&src[i + 12]);

        const __m128i ints0 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats0, offset)), _mm_castps_si128(offset));
        const __m128i ints1 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats1, offset)), _mm_castps_si128(offset));
        const __m128i ints2 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats2, offset)), _mm_castps_si128(offset));
        const __m128i ints3 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats3, offset)), _mm_castps_si128(offset));

        const __m128i shorts0 = _mm_packs_epi32(ints0, ints1);
        const __m128i shorts1 = _mm_packs_epi32(ints2, ints3);

        _mm_store_si128((__m128i*)&dst[i], shorts0);
        _mm_store_si128((__m128i*)&dst[i + 8], shorts1);
    })
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_S32_SSE2(int32_t *dst, const float *src, int num_samples)
{
    /* 1) Scale the float range from [-1.0, 1.0] to [-2147483648.0, 2147483648.0]
     * 2) Convert to integer (values too small/large become 0x80000000 = -2147483648)
     * 3) Fixup values which were too large (0x80000000 ^ 0xFFFFFFFF = 2147483647)
     * dst[i] = i32(src[i] * 2147483648.0) ^ ((src[i] >= 2147483648.0) ? 0xFFFFFFFF : 0x00000000) */
    const __m128 limit = _mm_set1_ps(2147483648.0f);

    LOG_DEBUG_AUDIO_CONVERT("F32", "S32 (using SSE2)");

    CONVERT_16_FWD({
        const __m128 floats = _mm_load_ss(&src[i]);
        const __m128 values = _mm_mul_ss(floats, limit);
        const __m128i ints = _mm_xor_si128(_mm_cvttps_epi32(values), _mm_castps_si128(_mm_cmpge_ss(values, limit)));
        dst[i] = (int32_t)_mm_cvtsi128_si32(ints);
    }, {
        const __m128 floats0 = _mm_loadu_ps(&src[i]);
        const __m128 floats1 = _mm_loadu_ps(&src[i + 4]);
        const __m128 floats2 = _mm_loadu_ps(&src[i + 8]);
        const __m128 floats3 = _mm_loadu_ps(&src[i + 12]);

        const __m128 values1 = _mm_mul_ps(floats0, limit);
        const __m128 values2 = _mm_mul_ps(floats1, limit);
        const __m128 values3 = _mm_mul_ps(floats2, limit);
        const __m128 values4 = _mm_mul_ps(floats3, limit);

        const __m128i ints0 = _mm_xor_si128(_mm_cvttps_epi32(values1), _mm_castps_si128(_mm_cmpge_ps(values1, limit)));
        const __m128i ints1 = _mm_xor_si128(_mm_cvttps_epi32(values2), _mm_castps_si128(_mm_cmpge_ps(values2, limit)));
        const __m128i ints2 = _mm_xor_si128(_mm_cvttps_epi32(values3), _mm_castps_si128(_mm_cmpge_ps(values3, limit)));
        const __m128i ints3 = _mm_xor_si128(_mm_cvttps_epi32(values4), _mm_castps_si128(_mm_cmpge_ps(values4, limit)));

        _mm_store_si128((__m128i*)&dst[i], ints0);
        _mm_store_si128((__m128i*)&dst[i + 4], ints1);
        _mm_store_si128((__m128i*)&dst[i + 8], ints2);
        _mm_store_si128((__m128i*)&dst[i + 12], ints3);
    })
}
#endif

// FIXME: SDL doesn't have SSSE3 detection, so use the next one up
#ifdef SDL_SSE4_1_INTRINSICS
static void SDL_TARGETING("ssse3") SDL_Convert_Swap16_SSSE3(uint16_t* dst, const uint16_t* src, int num_samples)
{
    const __m128i shuffle = _mm_set_epi8(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);

    CONVERT_16_FWD({
        dst[i] = SDL_Swap16(src[i]);
    }, {
        __m128i ints0 = _mm_loadu_si128((const __m128i*)&src[i]);
        __m128i ints1 = _mm_loadu_si128((const __m128i*)&src[i + 8]);

        ints0 = _mm_shuffle_epi8(ints0, shuffle);
        ints1 = _mm_shuffle_epi8(ints1, shuffle);

        _mm_store_si128((__m128i*)&dst[i], ints0);
        _mm_store_si128((__m128i*)&dst[i + 8], ints1);
    })
}

static void SDL_TARGETING("ssse3") SDL_Convert_Swap32_SSSE3(uint32_t* dst, const uint32_t* src, int num_samples)
{
    const __m128i shuffle = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);

    CONVERT_16_FWD({
        dst[i] = SDL_Swap32(src[i]);
    }, {
        __m128i ints0 = _mm_loadu_si128((const __m128i*)&src[i]);
        __m128i ints1 = _mm_loadu_si128((const __m128i*)&src[i + 4]);
        __m128i ints2 = _mm_loadu_si128((const __m128i*)&src[i + 8]);
        __m128i ints3 = _mm_loadu_si128((const __m128i*)&src[i + 12]);

        ints0 = _mm_shuffle_epi8(ints0, shuffle);
        ints1 = _mm_shuffle_epi8(ints1, shuffle);
        ints2 = _mm_shuffle_epi8(ints2, shuffle);
        ints3 = _mm_shuffle_epi8(ints3, shuffle);

        _mm_store_si128((__m128i*)&dst[i], ints0);
        _mm_store_si128((__m128i*)&dst[i + 4], ints1);
        _mm_store_si128((__m128i*)&dst[i + 8], ints2);
        _mm_store_si128((__m128i*)&dst[i + 12], ints3);
    })
}
#endif

#ifdef SDL_NEON_INTRINSICS
static void SDL_Convert_S8_to_F32_NEON(float *dst, const int8_t *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("S8", "F32 (using NEON)");

    CONVERT_16_REV({
        vst1_lane_f32(&dst[i], vcvt_n_f32_s32(vdup_n_s32(src[i]), 7), 0);
    }, {
        int8x16_t bytes = vld1q_s8(&src[i]);

        int16x8_t shorts0 = vmovl_s8(vget_low_s8(bytes));
        int16x8_t shorts1 = vmovl_s8(vget_high_s8(bytes));

        float32x4_t floats0 = vcvtq_n_f32_s32(vmovl_s16(vget_low_s16(shorts0)), 7);
        float32x4_t floats1 = vcvtq_n_f32_s32(vmovl_s16(vget_high_s16(shorts0)), 7);
        float32x4_t floats2 = vcvtq_n_f32_s32(vmovl_s16(vget_low_s16(shorts1)), 7);
        float32x4_t floats3 = vcvtq_n_f32_s32(vmovl_s16(vget_high_s16(shorts1)), 7);

        vst1q_f32(&dst[i], floats0);
        vst1q_f32(&dst[i + 4], floats1);
        vst1q_f32(&dst[i + 8], floats2);
        vst1q_f32(&dst[i + 12], floats3);
    })
}

static void SDL_Convert_U8_to_F32_NEON(float *dst, const uint8_t *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("U8", "F32 (using NEON)");

    uint8x16_t flipper = vdupq_n_u8(0x80);

    CONVERT_16_REV({
        vst1_lane_f32(&dst[i], vcvt_n_f32_s32(vdup_n_s32((int8_t)(src[i] ^ 0x80)), 7), 0);
    }, {
        int8x16_t bytes = vreinterpretq_s8_u8(veorq_u8(vld1q_u8(&src[i]), flipper));

        int16x8_t shorts0 = vmovl_s8(vget_low_s8(bytes));
        int16x8_t shorts1 = vmovl_s8(vget_high_s8(bytes));

        float32x4_t floats0 = vcvtq_n_f32_s32(vmovl_s16(vget_low_s16(shorts0)), 7);
        float32x4_t floats1 = vcvtq_n_f32_s32(vmovl_s16(vget_high_s16(shorts0)), 7);
        float32x4_t floats2 = vcvtq_n_f32_s32(vmovl_s16(vget_low_s16(shorts1)), 7);
        float32x4_t floats3 = vcvtq_n_f32_s32(vmovl_s16(vget_high_s16(shorts1)), 7);

        vst1q_f32(&dst[i], floats0);
        vst1q_f32(&dst[i + 4], floats1);
        vst1q_f32(&dst[i + 8], floats2);
        vst1q_f32(&dst[i + 12], floats3);
    })
}

static void SDL_Convert_S16_to_F32_NEON(float *dst, const int16_t *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("S16", "F32 (using NEON)");

    CONVERT_16_REV({
        vst1_lane_f32(&dst[i], vcvt_n_f32_s32(vdup_n_s32(src[i]), 15), 0);
    }, {
        int16x8_t shorts0 = vld1q_s16(&src[i]);
        int16x8_t shorts1 = vld1q_s16(&src[i + 8]);

        float32x4_t floats0 = vcvtq_n_f32_s32(vmovl_s16(vget_low_s16(shorts0)), 15);
        float32x4_t floats1 = vcvtq_n_f32_s32(vmovl_s16(vget_high_s16(shorts0)), 15);
        float32x4_t floats2 = vcvtq_n_f32_s32(vmovl_s16(vget_low_s16(shorts1)), 15);
        float32x4_t floats3 = vcvtq_n_f32_s32(vmovl_s16(vget_high_s16(shorts1)), 15);

        vst1q_f32(&dst[i], floats0);
        vst1q_f32(&dst[i + 4], floats1);
        vst1q_f32(&dst[i + 8], floats2);
        vst1q_f32(&dst[i + 12], floats3);
    })
}

static void SDL_Convert_S32_to_F32_NEON(float *dst, const int32_t *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("S32", "F32 (using NEON)");

    CONVERT_16_FWD({
        vst1_lane_f32(&dst[i], vcvt_n_f32_s32(vld1_dup_s32(&src[i]), 31), 0);
    }, {
        int32x4_t ints0 = vld1q_s32(&src[i]);
        int32x4_t ints1 = vld1q_s32(&src[i + 4]);
        int32x4_t ints2 = vld1q_s32(&src[i + 8]);
        int32x4_t ints3 = vld1q_s32(&src[i + 12]);

        float32x4_t floats0 = vcvtq_n_f32_s32(ints0, 31);
        float32x4_t floats1 = vcvtq_n_f32_s32(ints1, 31);
        float32x4_t floats2 = vcvtq_n_f32_s32(ints2, 31);
        float32x4_t floats3 = vcvtq_n_f32_s32(ints3, 31);

        vst1q_f32(&dst[i], floats0);
        vst1q_f32(&dst[i + 4], floats1);
        vst1q_f32(&dst[i + 8], floats2);
        vst1q_f32(&dst[i + 12], floats3);
    })
}

static void SDL_Convert_F32_to_S8_NEON(int8_t *dst, const float *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("F32", "S8 (using NEON)");

    CONVERT_16_FWD({
        vst1_lane_s8(&dst[i], vreinterpret_s8_s32(vcvt_n_s32_f32(vld1_dup_f32(&src[i]), 31)), 3);
    }, {
        float32x4_t floats0 = vld1q_f32(&src[i]);
        float32x4_t floats1 = vld1q_f32(&src[i + 4]);
        float32x4_t floats2 = vld1q_f32(&src[i + 8]);
        float32x4_t floats3 = vld1q_f32(&src[i + 12]);

        int32x4_t ints0 = vcvtq_n_s32_f32(floats0, 31);
        int32x4_t ints1 = vcvtq_n_s32_f32(floats1, 31);
        int32x4_t ints2 = vcvtq_n_s32_f32(floats2, 31);
        int32x4_t ints3 = vcvtq_n_s32_f32(floats3, 31);

        int16x8_t shorts0 = vcombine_s16(vshrn_n_s32(ints0, 16), vshrn_n_s32(ints1, 16));
        int16x8_t shorts1 = vcombine_s16(vshrn_n_s32(ints2, 16), vshrn_n_s32(ints3, 16));

        int8x16_t bytes = vcombine_s8(vshrn_n_s16(shorts0, 8), vshrn_n_s16(shorts1, 8));

        vst1q_s8(&dst[i], bytes);
    })
}

static void SDL_Convert_F32_to_U8_NEON(uint8_t *dst, const float *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("F32", "U8 (using NEON)");

    uint8x16_t flipper = vdupq_n_u8(0x80);

    CONVERT_16_FWD({
        vst1_lane_u8(&dst[i],
            veor_u8(vreinterpret_u8_s32(vcvt_n_s32_f32(vld1_dup_f32(&src[i]), 31)),
                vget_low_u8(flipper)), 3);
    }, {
        float32x4_t floats0 = vld1q_f32(&src[i]);
        float32x4_t floats1 = vld1q_f32(&src[i + 4]);
        float32x4_t floats2 = vld1q_f32(&src[i + 8]);
        float32x4_t floats3 = vld1q_f32(&src[i + 12]);

        int32x4_t ints0 = vcvtq_n_s32_f32(floats0, 31);
        int32x4_t ints1 = vcvtq_n_s32_f32(floats1, 31);
        int32x4_t ints2 = vcvtq_n_s32_f32(floats2, 31);
        int32x4_t ints3 = vcvtq_n_s32_f32(floats3, 31);

        int16x8_t shorts0 = vcombine_s16(vshrn_n_s32(ints0, 16), vshrn_n_s32(ints1, 16));
        int16x8_t shorts1 = vcombine_s16(vshrn_n_s32(ints2, 16), vshrn_n_s32(ints3, 16));

        uint8x16_t bytes = veorq_u8(vreinterpretq_u8_s8(
            vcombine_s8(vshrn_n_s16(shorts0, 8), vshrn_n_s16(shorts1, 8))),
            flipper);

        vst1q_u8(&dst[i], bytes);
    })
}

static void SDL_Convert_F32_to_S16_NEON(int16_t *dst, const float *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("F32", "S16 (using NEON)");

    CONVERT_16_FWD({
        vst1_lane_s16(&dst[i], vreinterpret_s16_s32(vcvt_n_s32_f32(vld1_dup_f32(&src[i]), 31)), 1);
    }, {
        float32x4_t floats0 = vld1q_f32(&src[i]);
        float32x4_t floats1 = vld1q_f32(&src[i + 4]);
        float32x4_t floats2 = vld1q_f32(&src[i + 8]);
        float32x4_t floats3 = vld1q_f32(&src[i + 12]);

        int32x4_t ints0 = vcvtq_n_s32_f32(floats0, 31);
        int32x4_t ints1 = vcvtq_n_s32_f32(floats1, 31);
        int32x4_t ints2 = vcvtq_n_s32_f32(floats2, 31);
        int32x4_t ints3 = vcvtq_n_s32_f32(floats3, 31);

        int16x8_t shorts0 = vcombine_s16(vshrn_n_s32(ints0, 16), vshrn_n_s32(ints1, 16));
        int16x8_t shorts1 = vcombine_s16(vshrn_n_s32(ints2, 16), vshrn_n_s32(ints3, 16));

        vst1q_s16(&dst[i], shorts0);
        vst1q_s16(&dst[i + 8], shorts1);
    })
}

static void SDL_Convert_F32_to_S32_NEON(int32_t *dst, const float *src, int num_samples)
{
    LOG_DEBUG_AUDIO_CONVERT("F32", "S32 (using NEON)");

    CONVERT_16_FWD({
        vst1_lane_s32(&dst[i], vcvt_n_s32_f32(vld1_dup_f32(&src[i]), 31), 0);
    }, {
        float32x4_t floats0 = vld1q_f32(&src[i]);
        float32x4_t floats1 = vld1q_f32(&src[i + 4]);
        float32x4_t floats2 = vld1q_f32(&src[i + 8]);
        float32x4_t floats3 = vld1q_f32(&src[i + 12]);

        int32x4_t ints0 = vcvtq_n_s32_f32(floats0, 31);
        int32x4_t ints1 = vcvtq_n_s32_f32(floats1, 31);
        int32x4_t ints2 = vcvtq_n_s32_f32(floats2, 31);
        int32x4_t ints3 = vcvtq_n_s32_f32(floats3, 31);

        vst1q_s32(&dst[i], ints0);
        vst1q_s32(&dst[i + 4], ints1);
        vst1q_s32(&dst[i + 8], ints2);
        vst1q_s32(&dst[i + 12], ints3);
    })
}

static void SDL_Convert_Swap16_NEON(uint16_t* dst, const uint16_t* src, int num_samples)
{
    CONVERT_16_FWD({
        dst[i] = SDL_Swap16(src[i]);
    }, {
        uint8x16_t ints0 = vld1q_u8((const uint8_t*)&src[i]);
        uint8x16_t ints1 = vld1q_u8((const uint8_t*)&src[i + 8]);

        ints0 = vrev16q_u8(ints0);
        ints1 = vrev16q_u8(ints1);

        vst1q_u8((uint8_t*)&dst[i], ints0);
        vst1q_u8((uint8_t*)&dst[i + 8], ints1);
    })
}

static void SDL_Convert_Swap32_NEON(uint32_t* dst, const uint32_t* src, int num_samples)
{
    CONVERT_16_FWD({
        dst[i] = SDL_Swap32(src[i]);
    }, {
        uint8x16_t ints0 = vld1q_u8((const uint8_t*)&src[i]);
        uint8x16_t ints1 = vld1q_u8((const uint8_t*)&src[i + 4]);
        uint8x16_t ints2 = vld1q_u8((const uint8_t*)&src[i + 8]);
        uint8x16_t ints3 = vld1q_u8((const uint8_t*)&src[i + 12]);

        ints0 = vrev32q_u8(ints0);
        ints1 = vrev32q_u8(ints1);
        ints2 = vrev32q_u8(ints2);
        ints3 = vrev32q_u8(ints3);

        vst1q_u8((uint8_t*)&dst[i], ints0);
        vst1q_u8((uint8_t*)&dst[i + 4], ints1);
        vst1q_u8((uint8_t*)&dst[i + 8], ints2);
        vst1q_u8((uint8_t*)&dst[i + 12], ints3);
    })
}
#endif

#undef CONVERT_16_FWD
#undef CONVERT_16_REV

// Function pointers set to a CPU-specific implementation.
static void (*SDL_Convert_S8_to_F32)(float *dst, const int8_t *src, int num_samples) = NULL;
static void (*SDL_Convert_U8_to_F32)(float *dst, const uint8_t *src, int num_samples) = NULL;
static void (*SDL_Convert_S16_to_F32)(float *dst, const int16_t *src, int num_samples) = NULL;
static void (*SDL_Convert_S32_to_F32)(float *dst, const int32_t *src, int num_samples) = NULL;
static void (*SDL_Convert_F32_to_S8)(int8_t *dst, const float *src, int num_samples) = NULL;
static void (*SDL_Convert_F32_to_U8)(uint8_t *dst, const float *src, int num_samples) = NULL;
static void (*SDL_Convert_F32_to_S16)(int16_t *dst, const float *src, int num_samples) = NULL;
static void (*SDL_Convert_F32_to_S32)(int32_t *dst, const float *src, int num_samples) = NULL;

static void (*SDL_Convert_Swap16)(uint16_t* dst, const uint16_t* src, int num_samples) = NULL;
static void (*SDL_Convert_Swap32)(uint32_t* dst, const uint32_t* src, int num_samples) = NULL;

void ConvertAudioToFloat(float *dst, const void *src, int num_samples, SDL_AudioFormat src_fmt)
{
    switch (src_fmt) {
        case SDL_AUDIO_S8:
            SDL_Convert_S8_to_F32(dst, (const int8_t *) src, num_samples);
            break;

        case SDL_AUDIO_U8:
            SDL_Convert_U8_to_F32(dst, (const uint8_t *) src, num_samples);
            break;

        case SDL_AUDIO_S16:
            SDL_Convert_S16_to_F32(dst, (const int16_t *) src, num_samples);
            break;

        case SDL_AUDIO_S16 ^ SDL_AUDIO_MASK_BIG_ENDIAN:
            SDL_Convert_Swap16((uint16_t*) dst, (const uint16_t*) src, num_samples);
            SDL_Convert_S16_to_F32(dst, (const int16_t *) dst, num_samples);
            break;

        case SDL_AUDIO_S32:
            SDL_Convert_S32_to_F32(dst, (const int32_t *) src, num_samples);
            break;

        case SDL_AUDIO_S32 ^ SDL_AUDIO_MASK_BIG_ENDIAN:
            SDL_Convert_Swap32((uint32_t*) dst, (const uint32_t*) src, num_samples);
            SDL_Convert_S32_to_F32(dst, (const int32_t *) dst, num_samples);
            break;

        case SDL_AUDIO_F32 ^ SDL_AUDIO_MASK_BIG_ENDIAN:
            SDL_Convert_Swap32((uint32_t*) dst, (const uint32_t*) src, num_samples);
            break;

        default: SDL_assert(!"Unexpected audio format!"); break;
    }
}

void ConvertAudioFromFloat(void *dst, const float *src, int num_samples, SDL_AudioFormat dst_fmt)
{
    switch (dst_fmt) {
        case SDL_AUDIO_S8:
            SDL_Convert_F32_to_S8((int8_t *) dst, src, num_samples);
            break;

        case SDL_AUDIO_U8:
            SDL_Convert_F32_to_U8((uint8_t *) dst, src, num_samples);
            break;

        case SDL_AUDIO_S16:
            SDL_Convert_F32_to_S16((int16_t *) dst, src, num_samples);
            break;

        case SDL_AUDIO_S16 ^ SDL_AUDIO_MASK_BIG_ENDIAN:
            SDL_Convert_F32_to_S16((int16_t *) dst, src, num_samples);
            SDL_Convert_Swap16((uint16_t*) dst, (const uint16_t*) dst, num_samples);
            break;

        case SDL_AUDIO_S32:
            SDL_Convert_F32_to_S32((int32_t *) dst, src, num_samples);
            break;

        case SDL_AUDIO_S32 ^ SDL_AUDIO_MASK_BIG_ENDIAN:
            SDL_Convert_F32_to_S32((int32_t *) dst, src, num_samples);
            SDL_Convert_Swap32((uint32_t*) dst, (const uint32_t*) dst, num_samples);
            break;

        case SDL_AUDIO_F32 ^ SDL_AUDIO_MASK_BIG_ENDIAN:
            SDL_Convert_Swap32((uint32_t*) dst, (const uint32_t*) src, num_samples);
            break;

        default: SDL_assert(!"Unexpected audio format!"); break;
    }
}

void ConvertAudioSwapEndian(void* dst, const void* src, int num_samples, int bitsize)
{
    switch (bitsize) {
        case 16: SDL_Convert_Swap16((uint16_t*) dst, (const uint16_t*) src, num_samples); break;
        case 32: SDL_Convert_Swap32((uint32_t*) dst, (const uint32_t*) src, num_samples); break;
        default: SDL_assert(!"Unexpected audio format!"); break;
    }
}

void SDL_ChooseAudioConverters(void)
{
    static bool converters_chosen = false;
    if (converters_chosen) {
        return;
    }

#define SET_CONVERTER_FUNCS(fntype) \
    SDL_Convert_Swap16 = SDL_Convert_Swap16_##fntype; \
    SDL_Convert_Swap32 = SDL_Convert_Swap32_##fntype;

#ifdef SDL_SSE4_1_INTRINSICS
    if (SDL_HasSSE41()) {
        SET_CONVERTER_FUNCS(SSSE3);
    } else
#endif
#ifdef SDL_NEON_INTRINSICS
    if (SDL_HasNEON()) {
        SET_CONVERTER_FUNCS(NEON);
    } else
#endif
    {
        SET_CONVERTER_FUNCS(Scalar);
    }

#undef SET_CONVERTER_FUNCS

#define SET_CONVERTER_FUNCS(fntype) \
    SDL_Convert_S8_to_F32 = SDL_Convert_S8_to_F32_##fntype; \
    SDL_Convert_U8_to_F32 = SDL_Convert_U8_to_F32_##fntype; \
    SDL_Convert_S16_to_F32 = SDL_Convert_S16_to_F32_##fntype; \
    SDL_Convert_S32_to_F32 = SDL_Convert_S32_to_F32_##fntype; \
    SDL_Convert_F32_to_S8 = SDL_Convert_F32_to_S8_##fntype; \
    SDL_Convert_F32_to_U8 = SDL_Convert_F32_to_U8_##fntype; \
    SDL_Convert_F32_to_S16 = SDL_Convert_F32_to_S16_##fntype; \
    SDL_Convert_F32_to_S32 = SDL_Convert_F32_to_S32_##fntype; \

#ifdef SDL_SSE2_INTRINSICS
    if (SDL_HasSSE2()) {
        SET_CONVERTER_FUNCS(SSE2);
    } else
#endif
#ifdef SDL_NEON_INTRINSICS
    if (SDL_HasNEON()) {
        SET_CONVERTER_FUNCS(NEON);
    } else
#endif
    {
        SET_CONVERTER_FUNCS(Scalar);
    }

#undef SET_CONVERTER_FUNCS

    converters_chosen = true;
}
