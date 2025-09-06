#include "SDL_internal.h"
#include "events/SDL_keyboard_c.h"
#include "video/ohos/SDL_ohosmouse.h"
#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <dlfcn.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <node_api.h>
#include <node_api_types.h>
#include <stdint.h>

#ifdef SDL_PLATFORM_OHOS

#include "../../video/ohos/SDL_ohoskeyboard.h"
#include "../../video/ohos/SDL_ohostouch.h"
#include "../../video/ohos/SDL_ohosvideo.h"
#include "SDL3/SDL_mutex.h"
#include "SDL_ohos.h"
#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>

static OHNativeWindow *g_ohosNativeWindow;
static SDL_Mutex *g_ohosPageMutex = NULL;
static OH_NativeXComponent_Callback callback;
static OH_NativeXComponent_MouseEvent_Callback mouseCallback;
static int x, y, wid, hei;
static struct
{
    napi_env env;
    napi_threadsafe_function func;
    napi_ref interface;
} napiEnv;

typedef enum
{
    Int,
    Long,
    Double,
    String
} napiArgType;

typedef struct
{
    napiArgType type;
    bool enabled;
    union
    {
        int i;
        long long l;
        double d;
        const char *str;
    } data;
} napiCallbackArg;
typedef struct
{
    const char *func;
    int argCount;
    napiCallbackArg arg[16];
    napiArgType type;
    napiCallbackArg ret;
    bool returned;
} napiCallbackData;

void OHOS_windowUpdateAttributes(SDL_Window *w)
{
    w->x = x;
    w->y = y;
    w->w = wid;
    w->h = hei;
    
    SDL_SetWindowSize(w, wid, hei);
}

void OHOS_windowDataFill(SDL_Window *w)
{
    w->internal = SDL_calloc(1, sizeof(SDL_WindowData));
    OHOS_windowUpdateAttributes(w);
    w->internal->native_window = g_ohosNativeWindow;

    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this->windows == NULL) {
        _this->windows = w;
    } else {
        _this->windows->next = w;
        w->prev = _this->windows;
    }

#ifdef SDL_VIDEO_OPENGL_EGL
    if (w->flags & SDL_WINDOW_OPENGL) {
        SDL_LockMutex(g_ohosPageMutex);
        if (w->internal->egl_surface == EGL_NO_SURFACE) {
            w->internal->egl_surface = SDL_EGL_CreateSurface(_this, w, (NativeWindowType)g_ohosNativeWindow);
        }
        SDL_UnlockMutex(g_ohosPageMutex);
    }
#endif
}
void OHOS_removeWindow(SDL_Window *w)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this->windows == w) {
        _this->windows = _this->windows->next;
    } else {
        SDL_Window *curWin = _this->windows;
        while (curWin != NULL) {
            if (curWin == w) {
                if (curWin->next == NULL) {
                    curWin->prev->next = NULL;
                } else {
                    curWin->prev->next = curWin->next;
                    curWin->next->prev = curWin->prev;
                }
                break;
            }
            curWin = curWin->next;
        }
    }

#ifdef SDL_VIDEO_OPENGL_EGL
    if (w->flags & SDL_WINDOW_OPENGL) {
        SDL_LockMutex(g_ohosPageMutex);
        if (w->internal->egl_context) {
            SDL_EGL_DestroyContext(_this, w->internal->egl_context);
        }
        if (w->internal->egl_surface != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(_this, w->internal->egl_surface);
        }
        SDL_UnlockMutex(g_ohosPageMutex);
    }
    SDL_free(w->internal);
#endif
}

void OHOS_LockPage()
{
    SDL_LockMutex(g_ohosPageMutex);
}
void OHOS_UnlockPage()
{
    SDL_UnlockMutex(g_ohosPageMutex);
}

int OHOS_FetchWidth()
{
    return wid;
}
int OHOS_FetchHeight()
{
    return hei;
}

static void sdlJSCallback(napi_env env, napi_value jsCb, void *content, void *data)
{
    napiCallbackData *ar = (napiCallbackData *)data;

    napi_value callb = NULL;
    napi_get_reference_value(env, napiEnv.interface, &callb);
    napi_value jsMethod = NULL;
    napi_get_named_property(env, callb, ar->func, &jsMethod);

    napi_value args[16];
    SDL_Log("[SDL] calling js function %s with %d args", ar->func, ar->argCount);
    for (int i = 0; i < ar->argCount; i++) {
        if (!ar->arg[i].enabled) {
            continue;
        }
        switch (ar->arg[i].type) {
        case Int:
        {
            napi_create_int32(env, ar->arg[i].data.i, args + i);
            break;
        }
        case Long:
        {
            napi_create_int64(env, ar->arg[i].data.l, args + i);
            break;
        }
        case Double:
        {
            napi_create_double(env, ar->arg[i].data.d, args + i);
            break;
        }
        case String:
        {
            const char* p = ar->arg[i].data.str; int l = 0;
            while (*p) {
                l++;
                p++;
            }
            napi_create_string_utf8(env, ar->arg[i].data.str, l, args + i);
            break;
        }
        }
    }

    napi_value v;
    napi_call_function(env, NULL, jsMethod, ar->argCount, args, &v);
    switch (ar->type) {
    case Int:
    {
        napi_get_value_int32(env, v, &ar->ret.data.i);
        break;
    }
    case Long:
    {
        napi_get_value_int64(env, v, (int64_t *)&ar->ret.data.l);
        break;
    }
    case String:
    {
        size_t stringSize = 0;
        napi_get_value_string_utf8(env, v, NULL, 0, &stringSize);
        char *value = SDL_malloc(stringSize + 1);
        napi_get_value_string_utf8(env, v, value, stringSize + 1, &stringSize);
        ar->ret.data.str = value;
        break;
    }
    case Double:
    {
        napi_get_value_double(env, v, &ar->ret.data.d);
        break;
    }
    }
    ar->returned = true;
}

void OHOS_SetClipboardText(const char* c)
{
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "setPasteboardString";
    data->argCount = 1;
    data->arg[0].type = String;
    data->arg[0].enabled = true;
    data->arg[0].data.str = c;

    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_nonblocking);
}

bool OHOS_IsScreenKeyboardShown()
{
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "textEditing";
    data->argCount = 0;
    data->type = Int;
    data->returned = false;
    
    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_nonblocking);
    
    while (!data->returned) {}
    
    bool d = data->ret.data.i == 1;
    SDL_free(data);
    return d;
}
void OHOS_StartTextInput()
{
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "startTextInput";
    data->argCount = 0;
    data->returned = false;

    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_blocking);
    while (!data->returned) {}
}
void OHOS_StopTextInput()
{
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "stopTextInput";
    data->argCount = 0;
    data->returned = false;

    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_blocking);
    while (!data->returned) {}
}

void OHOS_MessageBox(const char* title, const char* message)
{
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "showDialog";
    data->argCount = 2;
    data->arg[0].type = String;
    data->arg[0].enabled = true;
    data->arg[0].data.str = title;
    data->arg[1].type = String;
    data->arg[1].enabled = true;
    data->arg[1].data.str = message;

    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_nonblocking);
}

const char* OHOS_Locale()
{
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "fetchLocale";
    data->argCount = 0;
    data->type = String;
    data->returned = false;
    
    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_nonblocking);
    
    while (!data->returned) {}
    
    const char* d = data->ret.data.str;
    SDL_free(data);
    return d;
}

static napi_value sdlCallbackInit(napi_env env, napi_callback_info info)
{
    napiEnv.env = env;
    size_t argc = 1;
    napi_value args[1] = { NULL };

    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    napi_create_reference(env, args[0], 1, &napiEnv.interface);

    napi_value resName = NULL;
    napi_create_string_utf8(env, "SDLThreadSafe", NAPI_AUTO_LENGTH, &resName);
    napi_create_threadsafe_function(env, args[0], NULL, resName, 0, 1, NULL, NULL, NULL, sdlJSCallback, &napiEnv.func);
    
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    SDL_memset(data, 0, sizeof(napiCallbackData));
    data->func = "test";
    data->argCount = 0;
    data->type = Int;
    
    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_nonblocking);

    napi_value result;
    napi_create_int32(env, 0, &result);
    return result;
}

typedef struct entrypoint_info_ {
    char* libname;
    char* func;
} entrypoint_info;
static int sdlLaunchMainInternal(void* reserved)
{
    if (!reserved) {
        return -1;
    }
    entrypoint_info *data = (entrypoint_info*)reserved;
    void *lib = dlopen(data->libname, RTLD_LAZY);
    void *func = dlsym(lib, data->func);
    typedef int (*test)();
    int d = ((test)func)();
    dlclose(lib);
    SDL_free(reserved);
    
    return d;
}

static SDL_Thread *mainThread;

static napi_value sdlLaunchMain(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    size_t libstringSize = 0;
    napi_get_value_string_utf8(env, args[0], NULL, 0, &libstringSize);
    char *libname = SDL_malloc(libstringSize + 1);
    napi_get_value_string_utf8(env, args[0], libname, libstringSize + 1, &libstringSize);

    size_t fstringSize = 0;
    napi_get_value_string_utf8(env, args[1], NULL, 0, &fstringSize);
    char *fname = SDL_malloc(fstringSize + 1);
    napi_get_value_string_utf8(env, args[1], fname, fstringSize + 1, &fstringSize);
    
    entrypoint_info *entry = (entrypoint_info*)SDL_malloc(sizeof(entrypoint_info));
    entry->func = fname;
    entry->libname = libname;
    mainThread = SDL_CreateThread(sdlLaunchMainInternal, "SDL App Thread", entry);
    SDL_SetMainReady();

    napi_value result;
    napi_create_int32(env, 0, &result);
    return result;
}

static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window)
{
    SDL_Log("Native window: %p", window);
    g_ohosNativeWindow = (OHNativeWindow *)window;

    uint64_t width;
    uint64_t height;
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);

    SDL_LockMutex(g_ohosPageMutex);
    wid = width;
    hei = height;
    x = (int)offsetX;
    y = (int)offsetY;
    SDL_UnlockMutex(g_ohosPageMutex);
    
    napiCallbackData *data = SDL_malloc(sizeof(napiCallbackData));
    data->func = "onMainLaunch";
    data->argCount = 0;

    napi_call_threadsafe_function(napiEnv.func, data, napi_tsfn_nonblocking);
}
static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window)
{
    g_ohosNativeWindow = (OHNativeWindow *)window;

    uint64_t width;
    uint64_t height;
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);

    SDL_LockMutex(g_ohosPageMutex);
    wid = width;
    hei = height;
    x = (int)offsetX;
    y = (int)offsetY;
    SDL_UnlockMutex(g_ohosPageMutex);
    
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this && _this->windows) {
        SDL_Window *win = _this->windows;
        OHOS_windowUpdateAttributes(win);
    }
}
static void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Window *win = _this->windows;
    while (win != NULL) {
#ifdef SDL_VIDEO_OPENGL_EGL
        if (win->flags & SDL_WINDOW_OPENGL) {
            if (win->internal->egl_context) {
                SDL_EGL_DestroyContext(_this, win->internal->egl_context);
            }
            if (win->internal->egl_surface) {
                SDL_EGL_DestroySurface(_this, win->internal->egl_surface);
            }
        }
#endif
        win = win->next;
    }
}
static void onKeyEvent(OH_NativeXComponent *component, void *window)
{
    OH_NativeXComponent_KeyEvent *keyEvent = NULL;
    SDL_Log("key!");
    if (OH_NativeXComponent_GetKeyEvent(component, &keyEvent) >= 0) {
        OH_NativeXComponent_KeyAction action;
        OH_NativeXComponent_KeyCode code;
        OH_NativeXComponent_EventSourceType sourceType;

        OH_NativeXComponent_GetKeyEventAction(keyEvent, &action);
        OH_NativeXComponent_GetKeyEventCode(keyEvent, &code);
        OH_NativeXComponent_GetKeyEventSourceType(keyEvent, &sourceType);

        if (sourceType == OH_NATIVEXCOMPONENT_SOURCE_TYPE_KEYBOARD) {
            if (OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN == action) {
                OHOS_OnKeyDown(code);
            } else if (OH_NATIVEXCOMPONENT_KEY_ACTION_UP == action) {
                OHOS_OnKeyUp(code);
            }
        }
    }
}

static void onNativeTouch(OH_NativeXComponent *component, void *window)
{
    OH_NativeXComponent_TouchEvent touchEvent;
    OH_NativeXComponent_TouchPointToolType toolType = OH_NATIVEXCOMPONENT_TOOL_TYPE_UNKNOWN;

    OHOS_LockPage();
    OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);
    OH_NativeXComponent_GetTouchPointToolType(component, 0, &toolType);

    for (int i = 0; i < touchEvent.numPoints; i++) {
        SDL_OHOSTouchEvent e;
        e.timestamp = touchEvent.timeStamp;
        // skip assertions
        e.deviceId = touchEvent.deviceId + 1;
        e.fingerId = touchEvent.touchPoints[i].id + 1;
        e.area = touchEvent.touchPoints[i].size;
        e.x = touchEvent.touchPoints[i].x / (float)wid;
        e.y = touchEvent.touchPoints[i].y / (float)hei;
        e.p = touchEvent.touchPoints[i].force;

        switch (touchEvent.touchPoints[i].type) {
        case OH_NATIVEXCOMPONENT_DOWN:
            e.type = SDL_EVENT_FINGER_DOWN;
            break;
        case OH_NATIVEXCOMPONENT_MOVE:
            e.type = SDL_EVENT_FINGER_MOTION;
            break;
        case OH_NATIVEXCOMPONENT_UP:
            e.type = SDL_EVENT_FINGER_UP;
            break;
        case OH_NATIVEXCOMPONENT_CANCEL:
        case OH_NATIVEXCOMPONENT_UNKNOWN:
            e.type = SDL_EVENT_FINGER_CANCELED;
            break;
        }
        
        OHOS_OnTouch(e);
    }
    
    OHOS_UnlockPage();
}
// TODO mouse data
static void onNativeMouse(OH_NativeXComponent *component, void *window) {
    OH_NativeXComponent_MouseEvent event;
    OHOS_LockPage();
    OH_NativeXComponent_GetMouseEvent(component, window, &event);
    if (event.button == OH_NATIVEXCOMPONENT_NONE_BUTTON || event.action == OH_NATIVEXCOMPONENT_MOUSE_NONE) {
        return;
    }
    SDL_OHOSMouseEvent e;
    e.x = event.x;
    e.y = event.y;
    e.timestamp = event.timestamp;
    switch (event.button)
    {
    case OH_NATIVEXCOMPONENT_LEFT_BUTTON:
        e.button = SDL_BUTTON_LEFT;
        break;
    case OH_NATIVEXCOMPONENT_RIGHT_BUTTON:
        e.button = SDL_BUTTON_RIGHT;
        break;
    case OH_NATIVEXCOMPONENT_MIDDLE_BUTTON:
        e.button = SDL_BUTTON_MIDDLE;
        break;
    case OH_NATIVEXCOMPONENT_BACK_BUTTON:
        e.button = SDL_BUTTON_X1;
        break;
    case OH_NATIVEXCOMPONENT_FORWARD_BUTTON:
        e.button = SDL_BUTTON_X2;
        break;
    }
    
    if (event.action == OH_NATIVEXCOMPONENT_MOUSE_MOVE) {
        e.motion = true;    
    }
    else {
        e.down = event.action == OH_NATIVEXCOMPONENT_MOUSE_PRESS;
    }
    OHOS_OnMouse(e);
    
    OHOS_UnlockPage();
}

static napi_value sdlKeyEvent(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { NULL, NULL };
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    
    int keycode, type;
    napi_get_value_int32(env, args[0], &keycode);
    napi_get_value_int32(env, args[1], &type);
    
    if (type == 0) {
        OHOS_OnKeyDown(keycode);    
    }
    else {
        OHOS_OnKeyUp(keycode);
    }
    
    napi_value result;
    napi_create_int32(env, 0, &result);
    return result;
}

static napi_value sdlTextAppend(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { NULL };
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    
    size_t fstringSize = 0;
    napi_get_value_string_utf8(env, args[0], NULL, 0, &fstringSize);
    char *fname = SDL_malloc(fstringSize + 1);
    napi_get_value_string_utf8(env, args[0], fname, fstringSize + 1, &fstringSize);
    
    SDL_SendKeyboardText(fname);
    
    napi_value result;
    napi_create_int32(env, 0, &result);
    return result;
}

static napi_value sdlTextEditing(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = { NULL, NULL, NULL };
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    
    size_t fstringSize = 0;
    napi_get_value_string_utf8(env, args[0], NULL, 0, &fstringSize);
    char *fname = SDL_malloc(fstringSize + 1);
    napi_get_value_string_utf8(env, args[0], fname, fstringSize + 1, &fstringSize);
    
    int start, len;
    napi_get_value_int32(env, args[1], &start);
    napi_get_value_int32(env, args[2], &len);
    
    SDL_SendEditingText(fname, start, len);
    
    napi_value result;
    napi_create_int32(env, 0, &result);
    return result;
}

static napi_value SDL_OHOS_NAPI_Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "sdlCallbackInit", NULL, sdlCallbackInit, NULL, NULL, NULL, napi_default, NULL },
        { "sdlLaunchMain", NULL, sdlLaunchMain, NULL, NULL, NULL, napi_default, NULL }, 
        { "sdlKeyEvent", NULL, sdlKeyEvent, NULL, NULL, NULL, napi_default, NULL },
        { "sdlTextAppend", NULL, sdlTextAppend, NULL, NULL, NULL, napi_default, NULL }, 
        { "sdlTextEditing", NULL, sdlTextEditing, NULL, NULL, NULL, napi_default, NULL }
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

    mouseCallback.DispatchMouseEvent = onNativeMouse;
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &mouseCallback);

    OH_NativeXComponent_RegisterKeyEventCallback(nativeXComponent, onKeyEvent);

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
