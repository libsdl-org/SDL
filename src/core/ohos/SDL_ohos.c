#include "SDL_ohos.h"
#include "SDL_internal.h"

#ifdef SDL_PLATFORM_OHOS

#define OHOS_DELAY_FIFTY      50
#define OHOS_DELAY_TEN        10
#define OHOS_START_ARGS_INDEX 2
#define OHOS_INDEX_ARG0       0
#define OHOS_INDEX_ARG1       1
#define OHOS_INDEX_ARG2       2
#define OHOS_INDEX_ARG3       3
#define OHOS_INDEX_ARG4       4
#define OHOS_INDEX_ARG5       5
#define OHOS_INDEX_ARG6       6

#include "napi/native_api.h"

SDL_DisplayOrientation displayOrientation;
SDL_Mutex *g_ohosPageMutex = NULL;
SDL_Semaphore *g_ohosPauseSem = NULL;
SDL_Semaphore *g_ohosResumeSem = NULL;
void SDL_OHOS_PAGEMUTEX_Lock()
{
    SDL_LockMutex(g_ohosPageMutex);
}
void SDL_OHOS_PAGEMUTEX_Unlock()
{
    SDL_UnlockMutex(g_ohosPageMutex);
}
void SDL_OHOS_PAGEMUTEX_LockRunning()
{
    int pauseSignaled = 0;
    int resumeSignaled = 0;
retry:
    SDL_LockMutex(g_ohosPageMutex);
    pauseSignaled = SDL_GetSemaphoreValue(g_ohosPauseSem);
    resumeSignaled = SDL_GetSemaphoreValue(g_ohosResumeSem);
    if (pauseSignaled > resumeSignaled) {
        SDL_UnlockMutex(g_ohosPageMutex);
        SDL_Delay(OHOS_DELAY_FIFTY);
        goto retry;
    }
}

void SDL_OHOS_SetDisplayOrientation(int orientation)
{
    displayOrientation = (SDL_DisplayOrientation)orientation;
}

SDL_DisplayOrientation SDL_OHOS_GetDisplayOrientation()
{
    return displayOrientation;
}

static napi_value add(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL };

    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);

    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    double value0;
    napi_get_value_double(env, args[0], &value0);

    double value1;
    napi_get_value_double(env, args[1], &value1);

    napi_value sum;
    napi_create_double(env, value0 + value1, &sum);

    return sum;
}

static napi_value SDL_OHOS_NAPI_Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "add", NULL, add, NULL, NULL, NULL, napi_default, NULL }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

napi_module OHOS_NAPI_Module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = SDL_OHOS_NAPI_Init,
    .nm_modname = "SDL3",
    .nm_priv = ((void *)0),
    .reserved = { 0 },
};

__attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&OHOS_NAPI_Module);
}

#endif /* SDL_PLATFORM_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
