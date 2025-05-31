#include "SDL_internal.h"
#include <EGL/egl.h>
#include <EGL/eglplatform.h>

#ifdef SDL_PLATFORM_OHOS

#include "napi/native_api.h"
#include "SDL_ohos.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include "../../video/ohos/SDL_ohosvideo.h"
#include "SDL3/SDL_mutex.h"

OHNativeWindow *nativeWindow;
SDL_Mutex *g_ohosPageMutex = NULL;
static OH_NativeXComponent_Callback callback;
static OH_NativeXComponent_MouseEvent_Callback mouseCallback;
SDL_WindowData windowData;
SDL_VideoData videoData;

static napi_value minus(napi_env env, napi_callback_info info)
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
    napi_create_double(env, value0 - value1, &sum);

    return sum;
}

static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window)
{
    nativeWindow = (OHNativeWindow *)window;

    uint64_t width;
    uint64_t height;
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);

    SDL_Log("Native Window: %p", nativeWindow);

    SDL_LockMutex(g_ohosPageMutex);
    windowData.native_window = nativeWindow;
    windowData.width = width;
    windowData.height = height;
    windowData.x = offsetX;
    windowData.y = offsetY;
    SDL_UnlockMutex(g_ohosPageMutex);
}
static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window)
{
    nativeWindow = (OHNativeWindow *)window;

    uint64_t width;
    uint64_t height;
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);

    SDL_Log("Native Window: %p", nativeWindow);

    SDL_LockMutex(g_ohosPageMutex);
    windowData.native_window = nativeWindow;
    windowData.width = width;
    windowData.height = height;
    windowData.x = offsetX;
    windowData.y = offsetY;

    SDL_UnlockMutex(g_ohosPageMutex);
}
static void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window)
{
    // SDL_VideoDevice* _this = SDL_GetVideoDevice();
}
static void onKeyEvent(OH_NativeXComponent *component, void *window) {}
static void onNativeTouch(OH_NativeXComponent *component, void *window) {}
static void onNativeMouse(OH_NativeXComponent *component, void *window) {}
static void OnDispatchTouchEventCB(OH_NativeXComponent *component, void *window) {}
void OnHoverEvent(OH_NativeXComponent *component, bool isHover) {}
void OnFocusEvent(OH_NativeXComponent *component, void *window) {}
void OnBlurEvent(OH_NativeXComponent *component, void *window) {}

static napi_value SDL_OHOS_NAPI_Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "minus", NULL, minus, NULL, NULL, NULL, napi_default, NULL }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    napi_value exportInstance = NULL;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
        return exports;
    }
    OH_NativeXComponent *nativeXComponent;
    if (napi_unwrap(env, exportInstance, (void **)(&nativeXComponent)) != napi_ok) {
        return exports;
    }

    callback.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback.OnSurfaceChanged = OnSurfaceChangedCB;
    callback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    callback.DispatchTouchEvent = onNativeTouch;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);

    mouseCallback.DispatchMouseEvent = OnDispatchTouchEventCB;
    mouseCallback.DispatchMouseEvent = onNativeMouse;
    mouseCallback.DispatchHoverEvent = OnHoverEvent;
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &mouseCallback);

    OH_NativeXComponent_RegisterKeyEventCallback(nativeXComponent, onKeyEvent);
    OH_NativeXComponent_RegisterFocusEventCallback(nativeXComponent, OnFocusEvent);
    OH_NativeXComponent_RegisterBlurEventCallback(nativeXComponent, OnBlurEvent);

    g_ohosPageMutex = SDL_CreateMutex();

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

#endif
