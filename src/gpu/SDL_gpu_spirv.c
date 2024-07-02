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
#include "SDL_gpu_driver.h"
#include "SDL_gpu_spirv_c.h"
#include "spirv_cross_c.h"

#if defined(_WIN32)
#define SPIRV_CROSS_DLL "spirv-cross-c-shared.dll"
#elif defined(__APPLE__)
#define SPIRV_CROSS_DLL "libspirv-cross-c-shared.0.dylib"
#else
#define SPIRV_CROSS_DLL "libspirv-cross-c-shared.so.0"
#endif

#define SPVC_ERROR(func) \
    SDL_SetError(#func " failed: %s", SDL_spvc_context_get_last_error_string(context))

static void *spirvcross_dll = NULL;

typedef spvc_result (*pfn_spvc_context_create)(spvc_context *context);
typedef void (*pfn_spvc_context_destroy)(spvc_context);
typedef spvc_result (*pfn_spvc_context_parse_spirv)(spvc_context, const SpvId *, size_t, spvc_parsed_ir *);
typedef spvc_result (*pfn_spvc_context_create_compiler)(spvc_context, spvc_backend, spvc_parsed_ir, spvc_capture_mode, spvc_compiler *);
typedef spvc_result (*pfn_spvc_compiler_create_compiler_options)(spvc_compiler, spvc_compiler_options *);
typedef spvc_result (*pfn_spvc_compiler_options_set_uint)(spvc_compiler_options, spvc_compiler_option, unsigned);
typedef spvc_result (*pfn_spvc_compiler_install_compiler_options)(spvc_compiler, spvc_compiler_options);
typedef spvc_result (*pfn_spvc_compiler_compile)(spvc_compiler, const char **);
typedef const char *(*pfn_spvc_context_get_last_error_string)(spvc_context);
typedef SpvExecutionModel (*pfn_spvc_compiler_get_execution_model)(spvc_compiler compiler);
typedef const char *(*pfn_spvc_compiler_get_cleansed_entry_point_name)(spvc_compiler compiler, const char *name, SpvExecutionModel model);

static pfn_spvc_context_create SDL_spvc_context_create = NULL;
static pfn_spvc_context_destroy SDL_spvc_context_destroy = NULL;
static pfn_spvc_context_parse_spirv SDL_spvc_context_parse_spirv = NULL;
static pfn_spvc_context_create_compiler SDL_spvc_context_create_compiler = NULL;
static pfn_spvc_compiler_create_compiler_options SDL_spvc_compiler_create_compiler_options = NULL;
static pfn_spvc_compiler_options_set_uint SDL_spvc_compiler_options_set_uint = NULL;
static pfn_spvc_compiler_install_compiler_options SDL_spvc_compiler_install_compiler_options = NULL;
static pfn_spvc_compiler_compile SDL_spvc_compiler_compile = NULL;
static pfn_spvc_context_get_last_error_string SDL_spvc_context_get_last_error_string = NULL;
static pfn_spvc_compiler_get_execution_model SDL_spvc_compiler_get_execution_model = NULL;
static pfn_spvc_compiler_get_cleansed_entry_point_name SDL_spvc_compiler_get_cleansed_entry_point_name = NULL;

void *SDL_CompileFromSPIRV(
    SDL_GpuDevice *device,
    void *originalCreateInfo,
    SDL_bool isCompute)
{
    SDL_GpuShaderCreateInfo *createInfo;
    spvc_result result;
    spvc_backend backend;
    SDL_GpuShaderFormat format;
    spvc_context context = NULL;
    spvc_parsed_ir ir = NULL;
    spvc_compiler compiler = NULL;
    spvc_compiler_options options = NULL;
    const char *translated_source;
    const char *cleansed_entrypoint;
    void *compiledResult;

    /* SDL_GpuShaderCreateInfo and SDL_GpuComputePipelineCreateInfo
     * share the same struct layout for their first 3 members, which
     * is all we need to transpile them!
     */
    createInfo = (SDL_GpuShaderCreateInfo *)originalCreateInfo;

    switch (SDL_GpuGetBackend(device)) {
    case SDL_GPU_BACKEND_D3D11:
        backend = SPVC_BACKEND_HLSL;
        format = SDL_GPU_SHADERFORMAT_HLSL;
        break;
    case SDL_GPU_BACKEND_METAL:
        backend = SPVC_BACKEND_MSL;
        format = SDL_GPU_SHADERFORMAT_MSL;
        break;
    default:
        SDL_SetError("SDL_CreateShaderFromSPIRV: Unexpected SDL_GpuBackend");
        return NULL;
    }

    /* FIXME: spirv-cross could probably be loaded in a better spot */
    if (spirvcross_dll == NULL) {
        spirvcross_dll = SDL_LoadObject(SPIRV_CROSS_DLL);
        if (spirvcross_dll == NULL) {
            return NULL;
        }
    }

#define CHECK_FUNC(func)                                                  \
    if (SDL_##func == NULL) {                                             \
        SDL_##func = (pfn_##func)SDL_LoadFunction(spirvcross_dll, #func); \
        if (SDL_##func == NULL) {                                         \
            return NULL;                                                  \
        }                                                                 \
    }
    CHECK_FUNC(spvc_context_create)
    CHECK_FUNC(spvc_context_destroy)
    CHECK_FUNC(spvc_context_parse_spirv)
    CHECK_FUNC(spvc_context_create_compiler)
    CHECK_FUNC(spvc_compiler_create_compiler_options)
    CHECK_FUNC(spvc_compiler_options_set_uint)
    CHECK_FUNC(spvc_compiler_install_compiler_options)
    CHECK_FUNC(spvc_compiler_compile)
    CHECK_FUNC(spvc_context_get_last_error_string)
    CHECK_FUNC(spvc_compiler_get_execution_model)
    CHECK_FUNC(spvc_compiler_get_cleansed_entry_point_name)
#undef CHECK_FUNC

    /* Create the SPIRV-Cross context */
    result = SDL_spvc_context_create(&context);
    if (result < 0) {
        SDL_SetError("spvc_context_create failed: %X", result);
        return NULL;
    }

    /* Parse the SPIR-V into IR */
    result = SDL_spvc_context_parse_spirv(context, (const SpvId *)createInfo->code, createInfo->codeSize / sizeof(SpvId), &ir);
    if (result < 0) {
        SPVC_ERROR(spvc_context_parse_spirv);
        SDL_spvc_context_destroy(context);
        return NULL;
    }

    /* Create the cross-compiler */
    result = SDL_spvc_context_create_compiler(context, backend, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
    if (result < 0) {
        SPVC_ERROR(spvc_context_create_compiler);
        SDL_spvc_context_destroy(context);
        return NULL;
    }

    /* Set up the cross-compiler options */
    result = SDL_spvc_compiler_create_compiler_options(compiler, &options);
    if (result < 0) {
        SPVC_ERROR(spvc_compiler_create_compiler_options);
        SDL_spvc_context_destroy(context);
        return NULL;
    }

    if (backend == SPVC_BACKEND_HLSL) {
        SDL_spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, 50);
        SDL_spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_HLSL_NONWRITABLE_UAV_TEXTURE_AS_SRV, 1);
    }

    result = SDL_spvc_compiler_install_compiler_options(compiler, options);
    if (result < 0) {
        SPVC_ERROR(spvc_compiler_install_compiler_options);
        SDL_spvc_context_destroy(context);
        return NULL;
    }

    /* Compile to the target shader language */
    result = SDL_spvc_compiler_compile(compiler, &translated_source);
    if (result < 0) {
        SPVC_ERROR(spvc_compiler_compile);
        SDL_spvc_context_destroy(context);
        return NULL;
    }

    /* Determine the "cleansed" entrypoint name (e.g. main -> main0 on MSL) */
    cleansed_entrypoint = SDL_spvc_compiler_get_cleansed_entry_point_name(
        compiler,
        createInfo->entryPointName,
        SDL_spvc_compiler_get_execution_model(compiler));

    /* Copy the original create info, but with the new source code */
    if (isCompute) {
        SDL_GpuComputePipelineCreateInfo newCreateInfo;
        newCreateInfo = *(SDL_GpuComputePipelineCreateInfo *)createInfo;
        newCreateInfo.format = format;
        newCreateInfo.code = (const Uint8 *)translated_source;
        newCreateInfo.codeSize = SDL_strlen(translated_source) + 1;
        newCreateInfo.entryPointName = cleansed_entrypoint;

        /* Create the pipeline! */
        compiledResult = SDL_GpuCreateComputePipeline(device, &newCreateInfo);
    } else {

        SDL_GpuShaderCreateInfo newCreateInfo;
        newCreateInfo = *createInfo;
        newCreateInfo.format = format;
        newCreateInfo.code = (const Uint8 *)translated_source;
        newCreateInfo.codeSize = SDL_strlen(translated_source) + 1;
        newCreateInfo.entryPointName = cleansed_entrypoint;

        /* Create the shader! */
        compiledResult = SDL_GpuCreateShader(device, &newCreateInfo);
    }

    /* Clean up */
    SDL_spvc_context_destroy(context);

    return compiledResult;
}
