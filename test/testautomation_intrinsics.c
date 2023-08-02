/**
 * Intrinsics test suite
 */

#ifndef NO_BUILD_CONFIG
/* Disable intrinsics that are unsupported by the current compiler */
#include <build_config/SDL_build_config.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_intrin.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

// FIXME: missing tests for loongarch lsx/lasx
// FIXME: missing tests for powerpc altivec

/* ================= Test Case Implementation ================== */

/* Helper functions */

static int allocate_random_int_arrays(Sint32 **dest, Sint32 **a, Sint32 **b, size_t *size) {
    size_t i;

    *size = (size_t)SDLTest_RandomIntegerInRange(127, 999);
    *dest = SDL_malloc(sizeof(Sint32) * *size);
    *a = SDL_malloc(sizeof(Sint32) * *size);
    *b = SDL_malloc(sizeof(Sint32) * *size);

    if (!*dest || !*a || !*b) {
        SDLTest_AssertCheck(SDL_FALSE, "SDL_malloc failed");
        return -1;
    }

    for (i = 0; i < *size; ++i) {
        (*a)[i] = SDLTest_RandomSint32();
        (*b)[i] = SDLTest_RandomSint32();
    }
    return 0;
}

static int allocate_random_float_arrays(float **dest, float **a, float **b, size_t *size) {
    size_t i;

    *size = (size_t)SDLTest_RandomIntegerInRange(127, 999);
    *dest = SDL_malloc(sizeof(float) * *size);
    *a = SDL_malloc(sizeof(float) * *size);
    *b = SDL_malloc(sizeof(float) * *size);

    if (!*dest || !*a || !*b) {
        SDLTest_AssertCheck(SDL_FALSE, "SDL_malloc failed");
        return -1;
    }

    for (i = 0; i < *size; ++i) {
        (*a)[i] = SDLTest_RandomUnitFloat();
        (*b)[i] = SDLTest_RandomUnitFloat();
    }

    return 0;
}

static int allocate_random_double_arrays(double **dest, double **a, double **b, size_t *size) {
    size_t i;

    *size = (size_t)SDLTest_RandomIntegerInRange(127, 999);
    *dest = SDL_malloc(sizeof(double) * *size);
    *a = SDL_malloc(sizeof(double) * *size);
    *b = SDL_malloc(sizeof(double) * *size);

    if (!*dest || !*a || !*b) {
        SDLTest_AssertCheck(SDL_FALSE, "SDL_malloc failed");
        return -1;
    }

    for (i = 0; i < *size; ++i) {
        (*a)[i] = SDLTest_RandomUnitDouble();
        (*b)[i] = SDLTest_RandomUnitDouble();
    }

    return 0;
}

static void free_arrays(void *dest, void *a, void *b) {
    SDL_free(dest);
    SDL_free(a);
    SDL_free(b);
}

/**
 * Verify element-wise addition of 2 int arrays.
 */
static void verify_ints_addition(const Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size, const char *desc) {
    size_t i;
    int all_good = 1;

    for (i = 0; i < size; ++i) {
        Sint32 expected = a[i] + b[i];
        if (dest[i] != expected) {
            SDLTest_AssertCheck(SDL_FALSE, "%" SDL_PRIs32 " + %" SDL_PRIs32 " = %" SDL_PRIs32 ", expected %" SDL_PRIs32 " ([%" SDL_PRIu32 "/%" SDL_PRIu32 "] %s)",
                                a[i], b[i], dest[i], expected, (Uint32)i, (Uint32)size, desc);
            all_good = 0;
        }
    }
    if (all_good) {
        SDLTest_AssertCheck(SDL_TRUE, "All int additions were correct (%s)", desc);
    }
}

/**
 * Verify element-wise multiplication of 2 int arrays.
 */
static void verify_ints_multiplication(const Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size, const char *desc) {
    size_t i;
    int all_good = 1;

    for (i = 0; i < size; ++i) {
        Sint32 expected = a[i] * b[i];
        if (dest[i] != expected) {
            SDLTest_AssertCheck(SDL_FALSE, "%" SDL_PRIs32 " * %" SDL_PRIs32 " = %" SDL_PRIs32 ", expected %" SDL_PRIs32 " ([%" SDL_PRIu32 "/%" SDL_PRIu32 "] %s)",
                                a[i], b[i], dest[i], expected, (Uint32)i, (Uint32)size, desc);
            all_good = 0;
        }
    }
    if (all_good) {
        SDLTest_AssertCheck(SDL_TRUE, "All int multiplication were correct (%s)", desc);
    }
}

/**
 * Verify element-wise addition of 2 float arrays.
 */
static void verify_floats_addition(const float *dest, const float *a, const float *b, size_t size, const char *desc) {
    size_t i;
    int all_good = 1;

    for (i = 0; i < size; ++i) {
        float expected = a[i] + b[i];
        float abs_error = SDL_fabsf(dest[i] - expected);
        if (abs_error > 1.0e-5f) {
            SDLTest_AssertCheck(SDL_FALSE, "%g + %g = %g, expected %g (error = %g) ([%" SDL_PRIu32 "/%" SDL_PRIu32 "] %s)",
                                a[i], b[i], dest[i], expected, abs_error, (Uint32) i, (Uint32) size, desc);
            all_good = 0;
        }
    }
    if (all_good) {
        SDLTest_AssertCheck(SDL_TRUE, "All float additions were correct (%s)", desc);
    }
}

/**
 * Verify element-wise addition of 2 double arrays.
 */
static void verify_doubles_addition(const double *dest, const double *a, const double *b, size_t size, const char *desc) {
    size_t i;
    int all_good = 1;

    for (i = 0; i < size; ++i) {
        double expected = a[i] + b[i];
        double abs_error = SDL_fabs(dest[i] - expected);
        if (abs_error > 1.0e-5) {
            SDLTest_AssertCheck(abs_error < 1.0e-5f, "%g + %g = %g, expected %g (error = %g) ([%" SDL_PRIu32 "/%" SDL_PRIu32 "] %s)",
                                a[i], b[i], dest[i], expected, abs_error, (Uint32) i, (Uint32) size, desc);
            all_good = SDL_FALSE;
        }
    }
    if (all_good) {
        SDLTest_AssertCheck(SDL_TRUE, "All double additions were correct (%s)", desc);
    }
}

/* Intrinsic kernels */

static void kernel_ints_add_cpu(Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size) {
    for (; size; --size, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}

static void kernel_ints_mul_cpu(Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size) {
    for (; size; --size, ++dest, ++a, ++b) {
        *dest = *a * *b;
    }
}

static void kernel_floats_add_cpu(float *dest, const float *a, const float *b, size_t size) {
    for (; size; --size, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}

static void kernel_doubles_add_cpu(double *dest, const double *a, const double *b, size_t size) {
    for (; size; --size, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}

#ifdef SDL_MMX_INTRINSICS
SDL_TARGETING("mmx") static void kernel_ints_add_mmx(Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size) {
    for (; size >= 2; size -= 2, dest += 2, a += 2, b += 2) {
        *(__m64*)dest = _mm_add_pi32(*(__m64*)a, *(__m64*)b);
    }
    if (size) {
        *dest = *a + *b;
    }
    _mm_empty();
}
#endif

#ifdef SDL_SSE_INTRINSICS
SDL_TARGETING("sse") static void kernel_floats_add_sse(float *dest, const float *a, const float *b, size_t size) {
    for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
        _mm_storeu_ps(dest, _mm_add_ps(_mm_loadu_ps(a), _mm_loadu_ps (b)));
    }
    for (; size; size--, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}
#endif

#ifdef SDL_SSE2_INTRINSICS
SDL_TARGETING("sse2") static void kernel_doubles_add_sse2(double *dest, const double *a, const double *b, size_t size) {
    for (; size >= 2; size -= 2, dest += 2, a += 2, b += 2) {
        _mm_storeu_pd(dest, _mm_add_pd(_mm_loadu_pd(a), _mm_loadu_pd(b)));
    }
    if (size) {
        *dest = *a + *b;
    }
}
#endif

#ifdef SDL_SSE3_INTRINSICS
SDL_TARGETING("sse3") static void kernel_ints_add_sse3(Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size) {
    for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
        _mm_storeu_si128((__m128i*)dest, _mm_add_epi32(_mm_lddqu_si128((__m128i*)a), _mm_lddqu_si128((__m128i*)b)));
    }
    for (;size; --size, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}
#endif

#ifdef SDL_SSE4_1_INTRINSICS
SDL_TARGETING("sse4.1") static void kernel_ints_mul_sse4_1(Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size) {
    for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
        _mm_storeu_si128((__m128i*)dest, _mm_mullo_epi32(_mm_lddqu_si128((__m128i*)a), _mm_lddqu_si128((__m128i*)b)));
    }
    for (;size; --size, ++dest, ++a, ++b) {
        *dest = *a * *b;
    }
}
#endif

#ifdef SDL_SSE4_2_INTRINSICS
SDL_TARGETING("sse4.2") static Uint32 calculate_crc32c_sse4_2(const char *text) {
    Uint32 crc32c = ~0u;
    size_t len = SDL_strlen(text);

#if defined(__x86_64__) || defined(_M_X64)
    for (; len >= 8; len -= 8, text += 8) {
        crc32c = (Uint32)_mm_crc32_u64(crc32c, *(Sint64*)text);
    }
    if (len >= 4) {
        crc32c = (Uint32)_mm_crc32_u32(crc32c, *(Sint32*)text);
        len -= 4;
        text += 4;
    }
#else
    for (; len >= 4; len -= 4, text += 4) {
        crc32c = (Uint32)_mm_crc32_u32(crc32c, *(Sint32*)text);
    }
#endif
    if (len >= 2) {
        crc32c = (Uint32)_mm_crc32_u16(crc32c, *(Sint16*)text);
        len -= 2;
        text += 2;
    }
    if (len) {
        crc32c = (Uint32)_mm_crc32_u8(crc32c, *text);
    }
    return ~crc32c;
}
#endif

#ifdef SDL_AVX_INTRINSICS
SDL_TARGETING("avx") static void kernel_floats_add_avx(float *dest, const float *a, const float *b, size_t size) {
    for (; size >= 8; size -= 8, dest += 8, a += 8, b += 8) {
        _mm256_storeu_ps(dest, _mm256_add_ps(_mm256_loadu_ps(a), _mm256_loadu_ps(b)));
    }
    for (; size; size--, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}
#endif

#ifdef SDL_AVX2_INTRINSICS
SDL_TARGETING("avx2") static void kernel_ints_add_avx2(Sint32 *dest, const Sint32 *a, const Sint32 *b, size_t size) {
    for (; size >= 8; size -= 8, dest += 8, a += 8, b += 8) {
        _mm256_storeu_si256((__m256i*)dest, _mm256_add_epi32(_mm256_loadu_si256((__m256i*)a), _mm256_loadu_si256((__m256i*)b)));
    }
    for (; size; size--, ++dest, ++a, ++b) {
        *dest = *a + *b;
    }
}
#endif

#ifdef SDL_AVX512F_INTRINSICS
SDL_TARGETING("avx512f") static void kernel_floats_add_avx512f(float *dest, const float *a, const float *b, size_t size) {
    for (; size >= 16; size -= 16, dest += 16, a += 16, b += 16) {
        _mm512_storeu_ps(dest, _mm512_add_ps(_mm512_loadu_ps(a), _mm512_loadu_ps(b)));
    }
    for (; size; --size) {
        *dest++ = *a++ + *b++;
    }
}
#endif

/* Test case functions */

static int intrinsics_selftest(void *arg)
{
    {
        size_t size;
        Sint32 *dest, *a, *b;
        if (allocate_random_int_arrays(&dest, &a, &b, &size) < 0) {
            return TEST_ABORTED;
        }
        kernel_ints_mul_cpu(dest, a, b, size);
        verify_ints_multiplication(dest, a, b, size, "CPU");
        free_arrays(dest, a, b);
    }
    {
        size_t size;
        Sint32 *dest, *a, *b;
        if (allocate_random_int_arrays(&dest, &a, &b, &size) < 0) {
            return TEST_ABORTED;
        }
        kernel_ints_add_cpu(dest, a, b, size);
        verify_ints_addition(dest, a, b, size, "CPU");
        free_arrays(dest, a, b);
    }
    {
        size_t size;
        float *dest, *a, *b;
        if (allocate_random_float_arrays(&dest, &a, &b, &size) < 0) {
            return TEST_ABORTED;
        }
        kernel_floats_add_cpu(dest, a, b, size);
        verify_floats_addition(dest, a, b, size, "CPU");
        free_arrays(dest, a, b);
    }
    {
        size_t size;
        double *dest, *a, *b;
        if (allocate_random_double_arrays(&dest, &a, &b, &size) < 0) {
            return TEST_ABORTED;
        }
        kernel_doubles_add_cpu(dest, a, b, size);
        verify_doubles_addition(dest, a, b, size, "CPU");
        free_arrays(dest, a, b);
    }
    return TEST_COMPLETED;
}

static int intrinsics_testMMX(void *arg)
{
    if (SDL_HasMMX()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has MMX support.");
#ifdef SDL_MMX_INTRINSICS
        {
            size_t size;
            Sint32 *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses MMX intrinsics.");
            if (allocate_random_int_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_ints_add_mmx(dest, a, b, size);
            verify_ints_addition(dest, a, b, size, "MMX");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use MMX intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO MMX support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testSSE(void *arg)
{
    if (SDL_HasSSE()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has SSE support.");
#ifdef SDL_SSE_INTRINSICS
        {
            size_t size;
            float *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses SSE intrinsics.");
            if (allocate_random_float_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_floats_add_sse(dest, a, b, size);
            verify_floats_addition(dest, a, b, size, "SSE");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use SSE intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO SSE support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testSSE2(void *arg)
{
    if (SDL_HasSSE2()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has SSE2 support.");
#ifdef SDL_SSE2_INTRINSICS
        {
            size_t size;
            double *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses SSE2 intrinsics.");
            if (allocate_random_double_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_doubles_add_sse2(dest, a, b, size);
            verify_doubles_addition(dest, a, b, size, "SSE2");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use SSE2 intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO SSE2 support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testSSE3(void *arg)
{
    if (SDL_HasSSE3()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has SSE3 support.");
#ifdef SDL_SSE3_INTRINSICS
        {
            size_t size;
            Sint32 *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses SSE3 intrinsics.");
            if (allocate_random_int_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_ints_add_sse3(dest, a, b, size);
            verify_ints_addition(dest, a, b, size, "SSE3");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use SSE3 intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO SSE3 support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testSSE4_1(void *arg)
{
    if (SDL_HasSSE41()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has SSE4.1 support.");
#ifdef SDL_SSE4_1_INTRINSICS
        {
            size_t size;
            Sint32 *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses SSE4.1 intrinsics.");
            if (allocate_random_int_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_ints_mul_sse4_1(dest, a, b, size);
            verify_ints_multiplication(dest, a, b, size, "SSE4.1");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use SSE4.1 intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO SSE4.1 support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testSSE4_2(void *arg)
{
    if (SDL_HasSSE42()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has SSE4.2 support.");
#ifdef SDL_SSE4_2_INTRINSICS
        {
            struct {
                const char *input;
                Uint32 crc32c;
            } references[] = {
                {"", 0x00000000},
                {"Hello world", 0x72b51f78},
                {"Simple DirectMedia Layer", 0x56f85341, },
            };
            size_t i;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses SSE4.2 intrinsics.");

            for (i = 0; i < SDL_arraysize(references); ++i) {
                Uint32 actual = calculate_crc32c_sse4_2(references[i].input);
                SDLTest_AssertCheck(actual == references[i].crc32c, "CRC32-C(\"%s\")=0x%08x, got 0x%08x",
                                    references[i].input, references[i].crc32c, actual);
            }

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use SSE4.2 intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO SSE4.2 support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testAVX(void *arg)
{
    if (SDL_HasAVX()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has AVX support.");
#ifdef SDL_AVX_INTRINSICS
        {
            size_t size;
            float *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses AVX intrinsics.");
            if (allocate_random_float_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_floats_add_avx(dest, a, b, size);
            verify_floats_addition(dest, a, b, size, "AVX");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use AVX intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO AVX support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testAVX2(void *arg)
{
    if (SDL_HasAVX2()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has AVX2 support.");
#ifdef SDL_AVX2_INTRINSICS
        {
            size_t size;
            Sint32 *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses AVX2 intrinsics.");
            if (allocate_random_int_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_ints_add_avx2(dest, a, b, size);
            verify_ints_addition(dest, a, b, size, "AVX2");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use AVX2 intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO AVX2 support.");
    }
    return TEST_SKIPPED;
}

static int intrinsics_testAVX512F(void *arg)
{
    if (SDL_HasAVX512F()) {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has AVX512F support.");
#ifdef SDL_AVX512F_INTRINSICS
        {
            size_t size;
            float *dest, *a, *b;

            SDLTest_AssertCheck(SDL_TRUE, "Test executable uses AVX512F intrinsics.");
            if (allocate_random_float_arrays(&dest, &a, &b, &size) < 0) {
                return TEST_ABORTED;
            }
            kernel_floats_add_avx512f(dest, a, b, size);
            verify_floats_addition(dest, a, b, size, "AVX512F");
            free_arrays(dest, a, b);

            return TEST_COMPLETED;
        }
#else
        SDLTest_AssertCheck(SDL_TRUE, "Test executable does NOT use AVX512F intrinsics.");
#endif
    } else {
        SDLTest_AssertCheck(SDL_TRUE, "CPU of test machine has NO AVX512F support.");
    }

    return TEST_SKIPPED;
}

/* ================= Test References ================== */

/* Intrinsics test cases */

static const SDLTest_TestCaseReference intrinsicsTest1 = {
    (SDLTest_TestCaseFp)intrinsics_selftest, "intrinsics_selftest", "Intrinsics testautomation selftest", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest2 = {
    (SDLTest_TestCaseFp)intrinsics_testMMX, "intrinsics_testMMX", "Tests MMX intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest3 = {
    (SDLTest_TestCaseFp)intrinsics_testSSE, "intrinsics_testSSE", "Tests SSE intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest4 = {
    (SDLTest_TestCaseFp)intrinsics_testSSE2, "intrinsics_testSSE2", "Tests SSE2 intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest5 = {
    (SDLTest_TestCaseFp)intrinsics_testSSE3, "intrinsics_testSSE3", "Tests SSE3 intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest6 = {
    (SDLTest_TestCaseFp)intrinsics_testSSE4_1, "intrinsics_testSSE4.1", "Tests SSE4.1 intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest7 = {
    (SDLTest_TestCaseFp)intrinsics_testSSE4_2, "intrinsics_testSSE4.2", "Tests SSE4.2 intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest8 = {
    (SDLTest_TestCaseFp)intrinsics_testAVX, "intrinsics_testAVX", "Tests AVX intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest9 = {
    (SDLTest_TestCaseFp)intrinsics_testAVX2, "intrinsics_testAVX2", "Tests AVX2 intrinsics", TEST_ENABLED
};

static const SDLTest_TestCaseReference intrinsicsTest10 = {
    (SDLTest_TestCaseFp)intrinsics_testAVX512F, "intrinsics_testAVX512F", "Tests AVX512F intrinsics", TEST_ENABLED
};

/* Sequence of Platform test cases */
static const SDLTest_TestCaseReference *platformTests[] = {
    &intrinsicsTest1,
    &intrinsicsTest2,
    &intrinsicsTest3,
    &intrinsicsTest4,
    &intrinsicsTest5,
    &intrinsicsTest6,
    &intrinsicsTest7,
    &intrinsicsTest8,
    &intrinsicsTest9,
    &intrinsicsTest10,
    NULL
};

/* Platform test suite (global) */
SDLTest_TestSuiteReference intrinsicsTestSuite = {
    "Intrinsics",
    NULL,
    platformTests,
    NULL
};
