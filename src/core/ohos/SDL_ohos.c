#include "SDL_internal.h"

#ifdef SDL_PLATFORM_OHOS

#include "napi/native_api.h"

static napi_value SDL_OHOS_NAPI_Init(napi_env env, napi_value exports)
{
    return exports;
}

napi_module OHOS_NAPI_Module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = SDL_OHOS_NAPI_Init,
    .nm_modname = "SDL3",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

__attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&OHOS_NAPI_Module);
}

#endif /* SDL_PLATFORM_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
