/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PLATFORM_OPENHARMONY

#include "../../events/SDL_events_c.h"

// !!! FIXME: work around some C++isms that leaked into OpenHarmony headers.
// !!! FIXME: HACK to prevent <AbilityKit/ability_runtime/start_options.h> from including. It has '&' instead of '*' for some args, which suggests it's only been tested with C++.
#define ABILITY_RUNTIME_START_OPTIONS_H
typedef enum AbilityRuntime_StartOptions AbilityRuntime_StartOptions;


// !!! FIXME: <rawfile/raw_file.h> has functions with C++ references. Prevent raw_file_manager.h from including it, and define the parts we need here.
#define GLOBAL_RAW_FILE_H
typedef struct RawFile RawFile;
typedef struct RawFile64 RawFile64;
typedef struct { int fd; long start; long length; } RawFileDescriptor;
typedef struct { int fd; int64_t start; int64_t length; } RawFileDescriptor64;
int64_t OH_ResourceManager_GetRawFileSize64(RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
int OH_ResourceManager_SeekRawFile64(const RawFile64 *rawFile, int64_t offset, int whence) __attribute__((__availability__(ohos, introduced=11.0.0)));
int64_t OH_ResourceManager_ReadRawFile64(const RawFile64 *rawFile, void *buf, int64_t length) __attribute__((__availability__(ohos, introduced=11.0.0)));
int64_t OH_ResourceManager_GetRawFileRemainingLength64(const RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
int64_t OH_ResourceManager_GetRawFileOffset64(const RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
void OH_ResourceManager_CloseRawFile64(RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
bool OH_ResourceManager_GetRawFileDescriptor64(const RawFile64 *rawFile, RawFileDescriptor64 *descriptor) __attribute__((__availability__(ohos, introduced=11.0.0)));
bool OH_ResourceManager_ReleaseRawFileDescriptor64(const RawFileDescriptor64 *descriptor) __attribute__((__availability__(ohos, introduced=11.0.0)));

#include <window_manager/oh_display_manager.h>

// !!! FIXME: these are defined as "const uint32_t VARNAME = VALUE;" in native_interface_xcomponent.h, which becomes a global variable in _our_ C code! Maybe C++ handles this differently...?
#define OH_XCOMPONENT_ID_LEN_MAX sdl_core_ohos_OH_XCOMPONENT_ID_LEN_MAX
#define OH_MAX_TOUCH_POINTS_NUMBER sdl_core_ohos_OH_MAX_TOUCH_POINTS_NUMBER
#include <ace/xcomponent/native_interface_xcomponent.h>

#include <AbilityKit/ability_runtime/application_context.h>
#include <BasicServicesKit/oh_commonevent.h>
#include <BasicServicesKit/oh_commonevent_support.h>
#include <deviceinfo.h>
#include <rawfile/raw_file_manager.h>
#include <hilog/log.h>
#include <stdlib.h>

// !!! FIXME: which of these headers do we actually need?
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <node_api.h>
#include <node_api_types.h>
#include <napi/native_api.h>

#include "SDL_openharmony.h"
#include "../../video/openharmony/SDL_openharmonyvideo.h"
#include "../../video/openharmony/SDL_openharmonyevents.h"

static CommonEvent_Subscriber *commonevent_subscriber = NULL;
static napi_ref ability_object_ref = NULL;
static napi_ref atmanager_ref = NULL;
static napi_ref window_ref = NULL;
static napi_ref ime_controller_ref = NULL;
static napi_ref pointer_ref = NULL;
static napi_ref on_insert_text_ref = NULL;
static napi_ref on_delete_left_ref = NULL;
static napi_ref i18nsystem_ref = NULL;
static NativeResourceManager *native_resource_mgr = NULL;
static napi_threadsafe_function syslocalechanged_threadsafefn = NULL;
static napi_threadsafe_function req_permissions_threadsafefn = NULL;
static napi_threadsafe_function open_url_threadsafefn = NULL;
static napi_threadsafe_function change_sysbars_threadsafefn = NULL;
static napi_threadsafe_function change_screensaver_threadsafefn = NULL;
static napi_threadsafe_function show_screenkeyboard_threadsafefn = NULL;
static napi_threadsafe_function hide_screenkeyboard_threadsafefn = NULL;
static napi_threadsafe_function change_mouseptr_threadsafefn = NULL;
static char *system_locale = NULL;
static SDL_SystemTheme system_theme = SDL_SYSTEM_THEME_UNKNOWN;
static SDL_DisplayOrientation device_orientation = SDL_ORIENTATION_PORTRAIT;


// Some NAPI helper code...

#define SDL_JS_ENTRY(expected_argc) \
    size_t argc = expected_argc; \
    napi_value argv[expected_argc]; \
    napi_value self = NULL; \
    napi_get_cb_info(env, info, &argc, argv, &self, NULL)

#define SDL_JS_ENTRY_USERDATA(expected_argc, userdata_type) \
    userdata_type *userdata = NULL; \
    size_t argc = expected_argc; \
    napi_value argv[expected_argc]; \
    napi_value self = NULL; \
    napi_get_cb_info(env, info, &argc, argv, &self, (void **) &userdata)

#define CallNapiThreadsafeFunction napi_call_threadsafe_function
#define SetNapiArrayElement napi_set_element
#define SetNapiObjField napi_set_named_property

static char *CreateSDLStringFromNAPIValue(napi_env env, napi_value val)
{
    char *retval = NULL;
    size_t buflen = 0;
    if (napi_get_value_string_utf8(env, val, NULL, 0, &buflen) != napi_ok) {
        return NULL;
    } else if ((retval = (char *) SDL_malloc(buflen + 1)) == NULL) {
        return NULL;
    } else if (napi_get_value_string_utf8(env, val, retval, buflen + 1, &buflen) != napi_ok) {
        SDL_free(retval);
        return NULL;
    }
    return retval;
}

static napi_value CreateNapiString(napi_env env, const char *str)
{
    napi_value retval = NULL;
    if (napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value CreateNapiObject(napi_env env)
{
    napi_value retval = NULL;
    if (napi_create_object(env, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value CreateNapiArray(napi_env env, size_t len)
{
    napi_value retval = NULL;
    if (napi_create_array_with_length(env, len, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value CreateNapiFunction(napi_env env, const char *name, napi_callback cb, void *userdata)
{
    napi_value retval = NULL;
    if (napi_create_function(env, name, name ? NAPI_AUTO_LENGTH : 0, cb, userdata, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value GetNapiArrayElement(napi_env env, napi_value arr, int idx)
{
    napi_value retval = NULL;
    if (napi_get_element(env, arr, idx, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value CreateNapiInt(napi_env env, int val)
{
    napi_value retval = NULL;
    if (napi_create_int32(env, (int32_t) val, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static int GetNapiInt(napi_env env, napi_value val, int deflt)
{
    int32_t retval = (int32_t) deflt;
    if (napi_get_value_int32(env, val, &retval) != napi_ok) {
        return deflt;
    }
    return (int) retval;
}

static napi_value GetNapiBoolean(napi_env env, bool b)
{
    napi_value retval = NULL;
    if (napi_get_boolean(env, b, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value GetNapiUndefined(napi_env env)
{
    napi_value retval = NULL;
    napi_get_undefined(env, &retval);
    return retval;
}

static napi_value GetNapiObjField(napi_env env, napi_value obj, const char *fieldname)
{
    napi_value retval = NULL;
    if (napi_get_named_property(env, obj, fieldname, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value GetNapiRefValue(napi_env env, napi_ref ref)
{
    napi_value retval = NULL;
    if (napi_get_reference_value(env, ref, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_threadsafe_function CreateNapiThreadsafeFunction(napi_env env, const char *name, napi_threadsafe_function_call_js fn)
{
    napi_threadsafe_function retval = NULL;
    if (napi_create_threadsafe_function(env, NULL, NULL, CreateNapiString(env, name), 0, 1, NULL, NULL, NULL, fn, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}

static napi_value CallNapiMethod(napi_env env, napi_value obj, const char *method, int num_args, napi_value *args)
{
    napi_value retval = NULL;
    if (napi_call_function(env, obj, GetNapiObjField(env, obj, method), num_args, args, &retval) != napi_ok) {
        return NULL;
    }
    return retval;
}


int SDL_GetOpenHarmonySDKVersion(void)
{
    static int sdk_version;
    if (!sdk_version) {
        sdk_version = OH_GetSdkApiVersion();
    }
    return sdk_version;
}

SDL_FormFactor SDL_GetOpenHarmonyDeviceFormFactor(void)
{
    static bool checked = false;
    static SDL_FormFactor form_factor = SDL_FORMFACTOR_UNKNOWN;
    if (!checked) {
        checked = true;
        const char *devtype = OH_GetDeviceType();
        if (devtype) {
            if ((SDL_strcmp(devtype, "phone") == 0) || (SDL_strcmp(devtype, "default") == 0)) {
                form_factor = SDL_FORMFACTOR_PHONE;
            } else if (SDL_strcmp(devtype, "wearable") == 0) {   // !!! FIXME: what is the difference between a "wearable" and "liteWearable"?
                form_factor = SDL_FORMFACTOR_WATCH;
            } else if (SDL_strcmp(devtype, "liteWearable") == 0) {
                form_factor = SDL_FORMFACTOR_WATCH;
            } else if (SDL_strcmp(devtype, "tablet") == 0) {
                form_factor = SDL_FORMFACTOR_TABLET;
            } else if (SDL_strcmp(devtype, "tv") == 0) {
                form_factor = SDL_FORMFACTOR_TV;
            } else if (SDL_strcmp(devtype, "car") == 0) {
                form_factor = SDL_FORMFACTOR_CAR;
            } else if (SDL_strcmp(devtype, "smartVision") == 0) {  // !!! FIXME: I assume this is either some sort of AI glasses or a VR headset...?
                form_factor = SDL_FORMFACTOR_HEADSET;
            } else {
                // !!! FIXME: aren't there desktops that can run HarmonyOS? Or are there just tablets?
            }
        }
    }
    return form_factor;
}

void SDL_DebugLogOpenHarmonyInfo(void)
{
    static bool already_logged = false;
    if (!already_logged) {
        already_logged = true;
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "SDL OpenHarmony system/device info:");
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Device type: %s", OH_GetDeviceType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Manufacturer: %s", OH_GetManufacture());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Brand: %s", OH_GetBrand());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Market name: %s", OH_GetMarketName());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Product series: %s", OH_GetProductSeries());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Product model: %s", OH_GetProductModel());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Software model: %s", OH_GetSoftwareModel());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Hardware model: %s", OH_GetHardwareModel());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Bootloader version: %s", OH_GetBootloaderVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - ABI list: %s", OH_GetAbiList());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Security patch tag: %s", OH_GetSecurityPatchTag());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Display version: %s", OH_GetDisplayVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Incremental version: %s", OH_GetIncrementalVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - OS release type: %s", OH_GetOsReleaseType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - OS full name: %s", OH_GetOSFullName());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - SDK API version: %d", SDL_GetOpenHarmonySDKVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - First API version: %d", OH_GetFirstApiVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Version ID: %s", OH_GetVersionId());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build type: %s", OH_GetBuildType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build user: %s", OH_GetBuildUser());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build host: %s", OH_GetBuildHost());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build time: %s", OH_GetBuildTime());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build root hash: %s", OH_GetBuildRootHash());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS name: %s", OH_GetDistributionOSName());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS version: %s", OH_GetDistributionOSVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS API version: %d", OH_GetDistributionOSApiVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS release type: %s", OH_GetDistributionOSReleaseType());
    }
}

const char *SDL_GetOpenHarmonySystemLocale(void)
{
    return system_locale;
}

SDL_SystemTheme SDL_GetOpenHarmonySystemTheme(void)
{
    return system_theme;
}

SDL_DisplayOrientation SDL_GetOpenHarmonyDeviceCurrentOrientation(void)
{
    return device_orientation;
}

const char *SDL_GetOpenHarmonyInternalStoragePath(void)
{
    static char *files_path = NULL;
    if (!files_path) {
        char *path = NULL;
        int32_t writelen = 0;
        size_t slen = 128;
        while (true) {
            void *ptr = SDL_realloc(path, slen + 1);  // +1 to save space for null terminator.
            if (!ptr) {
                SDL_free(path);
                return NULL;
            }
            path = (char *) ptr;

            const AbilityRuntime_ErrorCode rc = OH_AbilityRuntime_ApplicationContextGetFilesDir(path, slen, &writelen);
            if (rc == ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
                break;
            } else if (rc != ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID) {
                SDL_SetError("OH_AbilityRuntime_ApplicationContextGetFilesDir failed: %d", (int) rc);
                SDL_free(path);
                return NULL;
            }
            slen *= 2;  // try again with a bigger buffer.
        }

        files_path = (char *) SDL_realloc(path, SDL_strlen(path) + 1);  // shrink it down.
        if (!files_path) {
            files_path = path;  // oh well, _don't_ shrink it down...
        }
    }

    return files_path;
}


// Filesystem stuff...

bool SDL_OpenHarmonyRawFileOpen(void **puserdata, const char *fileName, const char *mode)
{
    if (SDL_strncmp(fileName, "assets://", 9) == 0) {
        fileName += 9;
    }

    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup.
    SDL_assert(puserdata != NULL);

    if (mode && (SDL_strcmp(mode, "r") != 0) && (SDL_strcmp(mode, "rb") != 0)) {
        return SDL_SetError("RawFile access is read-only");
    }

    RawFile64 *rf64 = OH_ResourceManager_OpenRawFile64(native_resource_mgr, fileName);
    if (!rf64) {
        return SDL_SetError("Failed to open RawFile");
    }

    *puserdata = rf64;
    return true;
}

Sint64 SDL_OpenHarmonyRawFileSize(void *userdata)
{
    return (Sint64) OH_ResourceManager_GetRawFileSize64((RawFile64 *) userdata);
}

Sint64 SDL_OpenHarmonyRawFileSeek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    const int ohwhence = (int) whence;  // these values happen to match.
    if (OH_ResourceManager_SeekRawFile64((const RawFile64 *) userdata, (int64_t) offset, ohwhence) < 0) {
        SDL_SetError("RawFile seek failed");
        return -1;
    }
    return (Sint64) OH_ResourceManager_GetRawFileOffset64((const RawFile64 *) userdata);
}

size_t SDL_OpenHarmonyRawFileRead(void *userdata, void *buffer, size_t size, SDL_IOStatus *status)
{
    const size_t br = (size_t) OH_ResourceManager_ReadRawFile64((const RawFile64 *) userdata, buffer, (int64_t) size);  // this returns 0 on eof/error, not a negative, so just cast to size_t.
    if (br < size) {
        if (OH_ResourceManager_GetRawFileRemainingLength64((const RawFile64 *) userdata) == 0) {
            *status = SDL_IO_STATUS_EOF;
        } else {
            *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("RawFile read failed");
        }
    }
    return br;
}

bool SDL_OpenHarmonyRawFileClose(void *userdata)
{
    OH_ResourceManager_CloseRawFile64((RawFile64 *) userdata);
    return true;
}

bool SDL_OpenHarmonyEnumerateAssetDirectory(const char *path, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    const char *origpath = path;
    if (SDL_strncmp(path, "assets://", 9) == 0) {
        path += 9;
    }

    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup.
    RawDir *rawdir = OH_ResourceManager_OpenRawDir(native_resource_mgr, path);
    if (!rawdir) {
        return SDL_SetError("RawDir open failed");
    }

    SDL_EnumerationResult result = SDL_ENUM_CONTINUE;
    const int total = OH_ResourceManager_GetRawFileCount(rawdir);
    for (int i = 0; (i < total) && (result == SDL_ENUM_CONTINUE); i++) {
        const char *fname = OH_ResourceManager_GetRawFileName(rawdir, i);
        result = cb(userdata, origpath, fname);
    }

    OH_ResourceManager_CloseRawDir(rawdir);

    return (result != SDL_ENUM_FAILURE);
}

bool SDL_OpenHarmonyGetAssetPathInfo(const char *path, SDL_PathInfo *info)
{
    if (SDL_strncmp(path, "assets://", 9) == 0) {
        path += 9;
    }

    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup.
    SDL_zerop(info);
    if (OH_ResourceManager_IsRawDir(native_resource_mgr, path)) {
        info->type = SDL_PATHTYPE_DIRECTORY;
    } else {
        RawFile64 *rf64 = OH_ResourceManager_OpenRawFile64(native_resource_mgr, path);
        if (!rf64) {
            return SDL_SetError("No such file or directory");
        }
        info->type = SDL_PATHTYPE_FILE;
        info->size = (Uint64) OH_ResourceManager_GetRawFileSize64(rf64);
        OH_ResourceManager_CloseRawFile64(rf64);
    }
    return true;
}


// ArkTS/C bridging...

typedef struct RequestPermissionData
{
    char *permission;
    SDL_RequestOpenHarmonyPermissionCallback callback;
    void *callback_userdata;
} RequestPermissionData;

// AsyncCallback when CallJSRequestPermissions() finishes its work.
static napi_value SDL_JS_RequestPermissionResult(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY_USERDATA(2, RequestPermissionData);
    napi_value result = GetNapiArrayElement(env, GetNapiObjField(env, argv[1], "authResults"), 0);
    const bool granted = (GetNapiInt(env, result, -1) == 0);
    userdata->callback(userdata->callback_userdata, userdata->permission, granted);

    SDL_free(userdata->permission);
    SDL_free(userdata);

    return GetNapiUndefined(env);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSRequestPermissions(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    RequestPermissionData *data = (RequestPermissionData *) userdata;
    napi_value ability = GetNapiRefValue(env, ability_object_ref);
    napi_value atmanager = GetNapiRefValue(env, atmanager_ref);
    napi_value args[] = { GetNapiObjField(env, ability, "context"), CreateNapiArray(env, 1), CreateNapiFunction(env, NULL, SDL_JS_RequestPermissionResult, data) };
    SetNapiArrayElement(env, args[1], 0, CreateNapiString(env, data->permission));
    CallNapiMethod(env, atmanager, "requestPermissionsFromUser", SDL_arraysize(args), args);
    // okay, assuming this worked out, we'll get a callback to SDL_JS_RequestPermissionResult() at some point in the future (if we haven't already).
}

bool SDL_RequestOpenHarmonyPermission(const char *permission, SDL_RequestOpenHarmonyPermissionCallback cb, void *userdata)
{
    RequestPermissionData *data = NULL;

    if (!permission) {
        return SDL_InvalidParamError("permission");
    } else if (!cb) {
        return SDL_InvalidParamError("cb");
    } else if (!atmanager_ref) {
        return SDL_SetError("atManager not initialized");
    } else if ((data = (RequestPermissionData *) SDL_calloc(1, sizeof (*data))) == NULL) {
        return false;
    } else if ((data->permission = SDL_strdup(permission)) == NULL) {
        SDL_free(data);
        return false;
    }

    data->callback = cb;
    data->callback_userdata = userdata;

    return (CallNapiThreadsafeFunction(req_permissions_threadsafefn, data, napi_tsfn_nonblocking) == napi_ok);
}

// AsyncCallback when CallJSOpenURL() finishes its work.
static napi_value SDL_JS_OpenURLResult(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY_USERDATA(1, char);
    SDL_free(userdata);
    return GetNapiUndefined(env);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSOpenURL(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    char *url = (char *) userdata;
    napi_value ability = GetNapiRefValue(env, ability_object_ref);
    napi_value abcontext = GetNapiObjField(env, ability, "context");
    napi_value args[] = { CreateNapiString(env, url), CreateNapiFunction(env, NULL, SDL_JS_OpenURLResult, url) };
    CallNapiMethod(env, abcontext, "openLink", SDL_arraysize(args), args);
}

bool SDL_OpenHarmonyOpenURL(const char *url)
{
    char *data = NULL;
    if (!url) {
        return SDL_InvalidParamError("url");
    } else if (!ability_object_ref) {
        return SDL_SetError("Ability not initialized");
    } else if ((data = SDL_strdup(url)) == NULL) {
        return false;
    }
    return (CallNapiThreadsafeFunction(open_url_threadsafefn, data, napi_tsfn_nonblocking) == napi_ok);
}


// AsyncCallback when CallJSChangeSysBars() finishes its work.
static napi_value SDL_JS_ChangeSysBarsResult(napi_env env, napi_callback_info info)
{
    return GetNapiUndefined(env);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSChangeSysBars(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    const intptr_t flags = (const intptr_t) userdata;
    const bool status_bar = (flags & (1 << 0)) != 0;
    const bool navigation_bar = (flags & (1 << 1)) != 0;
    int arrlen = 0;
    if (status_bar) { arrlen++; }
    if (navigation_bar) { arrlen++; }

    napi_value arr = CreateNapiArray(env, arrlen);
    arrlen = 0;
    if (status_bar) {
        SetNapiArrayElement(env, arr, arrlen++, CreateNapiString(env, "status"));
    }
    if (navigation_bar) {
        SetNapiArrayElement(env, arr, arrlen++, CreateNapiString(env, "navigation"));
    }

    napi_value window = GetNapiRefValue(env, window_ref);
    napi_value args[] = { arr, CreateNapiFunction(env, NULL, SDL_JS_ChangeSysBarsResult, NULL) };
    CallNapiMethod(env, window, "setSystemBarEnable", SDL_arraysize(args), args);
}

bool SDL_OpenHarmonyToggleSystemBars(bool status_bar, bool navigation_bar)
{
    const intptr_t flags = (status_bar ? (1 << 0) : 0) | (navigation_bar ? (1 << 1) : 0);
    if (!window_ref) {
        return SDL_SetError("Window not initialized");
    }
    return (CallNapiThreadsafeFunction(change_sysbars_threadsafefn, (void *) flags, napi_tsfn_nonblocking) == napi_ok);
}

// AsyncCallback when CallJSChangeScreenSaver() finishes its work.
static napi_value SDL_JS_ChangeScreenSaverResult(napi_env env, napi_callback_info info)
{
    return GetNapiUndefined(env);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSChangeScreenSaver(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    napi_value window = GetNapiRefValue(env, window_ref);
    napi_value args[] = { GetNapiBoolean(env, (userdata != NULL)), CreateNapiFunction(env, NULL, SDL_JS_ChangeScreenSaverResult, NULL) };
    CallNapiMethod(env, window, "setWindowKeepScreenOn", SDL_arraysize(args), args);
}

bool SDL_OpenHarmonyChangeScreenSaver(bool enable)
{
    if (!window_ref) {
        return SDL_SetError("Window not initialized");
    }
    return (CallNapiThreadsafeFunction(change_screensaver_threadsafefn, (void *) (size_t) (enable ? 0x1 : 0x0), napi_tsfn_nonblocking) == napi_ok);
}

// Called by the IME controller for insertText events.
static napi_value SDL_JS_IME_Controller_OnInsertText(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY(1);
    napi_value text = argv[0];
    char *utf8 = CreateSDLStringFromNAPIValue(env, text);
    if (utf8) {
        SDL_SendKeyboardText(utf8);
        SDL_free(utf8);
    }
    return GetNapiUndefined(env);
}

// Called by the IME controller for deleteLeft events.
static napi_value SDL_JS_IME_Controller_OnDeleteLeft(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY(1);
    napi_value number = argv[0];
    const int total = GetNapiInt(env, number, 0);
    for (int i = 0; i < total; i++) {
        SDL_SendKeyboardKey(0, SDL_DEFAULT_KEYBOARD_ID, 0, SDL_SCANCODE_BACKSPACE, true);
        SDL_SendKeyboardKey(0, SDL_DEFAULT_KEYBOARD_ID, 0, SDL_SCANCODE_BACKSPACE, false);
    }
    return GetNapiUndefined(env);
}


typedef struct ShowKeyboardData
{
    int input_type;
    int cap_type;
    SDL_Rect *text_input_rect;
    SDL_Rect text_input_rect_data;
} ShowKeyboardData;

// AsyncCallback when CallJSShowScreenKeyboard() finishes its work.
static napi_value SDL_JS_ShowScreenKeyboardResult(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY_USERDATA(0, ShowKeyboardData);
    napi_value ime_controller = GetNapiRefValue(env, ime_controller_ref);
    napi_value ime_oninserttext_args[] = { CreateNapiString(env, "insertText"), GetNapiRefValue(env, on_insert_text_ref) };
    CallNapiMethod(env, ime_controller, "on", SDL_arraysize(ime_oninserttext_args), ime_oninserttext_args);
    napi_value ime_ondeleteleft_args[] = { CreateNapiString(env, "deleteLeft"), GetNapiRefValue(env, on_delete_left_ref) };
    CallNapiMethod(env, ime_controller, "on", SDL_arraysize(ime_ondeleteleft_args), ime_ondeleteleft_args);
    SDL_SendScreenKeyboardShown();
    SDL_free(userdata);
    return GetNapiUndefined(env);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSShowScreenKeyboard(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    ShowKeyboardData *data = (ShowKeyboardData *) userdata;
    napi_value ime_controller = GetNapiRefValue(env, ime_controller_ref);

    napi_value inputAttribute = CreateNapiObject(env);
    SetNapiObjField(env, inputAttribute, "textInputType", CreateNapiInt(env, data->input_type));
    SetNapiObjField(env, inputAttribute, "capitalizeMode", CreateNapiInt(env, data->cap_type));
    SetNapiObjField(env, inputAttribute, "enterKeyType", CreateNapiInt(env, 0 /*inputMethod.enterKeyType.UNSPECIFIED*/));  // !!! FIXME

    napi_value attach_params = CreateNapiObject(env);
    SetNapiObjField(env, attach_params, "inputAttribute", inputAttribute);
    
    napi_value args[] = { GetNapiBoolean(env, true) /*showkeyboard*/, attach_params, CreateNapiFunction(env, NULL, SDL_JS_ShowScreenKeyboardResult, userdata) };
    CallNapiMethod(env, ime_controller, "attach", SDL_arraysize(args), args);
}

bool SDL_OpenHarmonyShowScreenKeyboard(int input_type, int cap_type, const SDL_Rect *text_input_rect)
{
    if (!ime_controller_ref) {
        return SDL_SetError("IME controller not initialized");
    }

    ShowKeyboardData *data = (ShowKeyboardData *) SDL_calloc(1, sizeof (*data));
    if (!data) {
        return false;
    }
    data->input_type = input_type;
    data->cap_type = cap_type;
    if (text_input_rect) {
        SDL_copyp(&data->text_input_rect_data, text_input_rect);
        data->text_input_rect = &data->text_input_rect_data;
    }

    return (CallNapiThreadsafeFunction(show_screenkeyboard_threadsafefn, data, napi_tsfn_nonblocking) == napi_ok);
}

// AsyncCallback when CallJSHideScreenKeyboard() finishes its work.
static napi_value SDL_JS_HideScreenKeyboardResult(napi_env env, napi_callback_info info)
{
    napi_value ime_controller = GetNapiRefValue(env, ime_controller_ref);
    napi_value ime_oninserttext_args[] = { CreateNapiString(env, "insertText"), GetNapiRefValue(env, on_insert_text_ref) };
    CallNapiMethod(env, ime_controller, "off", SDL_arraysize(ime_oninserttext_args), ime_oninserttext_args);
    napi_value ime_ondeleteleft_args[] = { CreateNapiString(env, "deleteLeft"), GetNapiRefValue(env, on_delete_left_ref) };
    CallNapiMethod(env, ime_controller, "off", SDL_arraysize(ime_ondeleteleft_args), ime_ondeleteleft_args);
    SDL_SendScreenKeyboardHidden();
    return GetNapiUndefined(env);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSHideScreenKeyboard(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    napi_value ime_controller = GetNapiRefValue(env, ime_controller_ref);
    napi_value args[] = { CreateNapiFunction(env, NULL, SDL_JS_HideScreenKeyboardResult, userdata) };
    CallNapiMethod(env, ime_controller, "hideTextInput", SDL_arraysize(args), args);
}

bool SDL_OpenHarmonyHideScreenKeyboard(void)
{
    if (!ime_controller_ref) {
        return SDL_SetError("IME controller not initialized");
    }
    return (CallNapiThreadsafeFunction(hide_screenkeyboard_threadsafefn, NULL, napi_tsfn_nonblocking) == napi_ok);
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSChangeMousePointer(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    const bool enable = (userdata != NULL);
    napi_value pointer = GetNapiRefValue(env, pointer_ref);
    napi_value args[] = { GetNapiBoolean(env, enable) };
//SDL_Log("%s MOUSE POINTER!", enable ? "SHOW" : "HIDE");
    CallNapiMethod(env, pointer, "setPointerVisibleSync", SDL_arraysize(args), args);
}

static bool SDL_OpenHarmonyChangeMousePointer(bool enable)
{
    if (!pointer_ref) {
        return SDL_SetError("Pointer namespace not initialized");
    }
    return (CallNapiThreadsafeFunction(change_mouseptr_threadsafefn, (void *) (size_t) (enable ? 0x1 : 0x0), napi_tsfn_nonblocking) == napi_ok);
}

bool SDL_OpenHarmonyShowMousePointer(void)
{
    return SDL_OpenHarmonyChangeMousePointer(true);
}

bool SDL_OpenHarmonyHideMousePointer(void)
{
    return SDL_OpenHarmonyChangeMousePointer(false);
}


// This _must_ be called from the Javascript thread, since it makes NAPI calls!
static bool UpdateSystemLocale(napi_env env)  // true if known locale changed, false otherwise.
{
    if (!i18nsystem_ref) {
        return false;
    }

    napi_value i18n = GetNapiRefValue(env, i18nsystem_ref);
    if (!i18n) {
        return false;  // uhoh.
    }

    napi_value language = CallNapiMethod(env, i18n, "getSystemLanguage", 0, NULL);
    napi_value region = CallNapiMethod(env, i18n, "getSystemRegion", 0, NULL);

    bool retval = false;
    if (language && region) {
        char *utf8lang = CreateSDLStringFromNAPIValue(env, language);
        char *utf8region = CreateSDLStringFromNAPIValue(env, region);
        if (utf8lang && utf8region) {
            char *ptr = SDL_strchr(utf8lang, '-');  // if we get "en-Latn-US" or whatever, we just want "en".
            if (ptr) {
                *ptr = '\0';  // chop it off.
            }
            char *utf8locale = NULL;
            if (SDL_asprintf(&utf8locale, "%s_%s", utf8lang, utf8region) > 0) {
                retval = (!system_locale || (SDL_strcmp(system_locale, utf8locale) != 0));
                if (!retval) {  // didn't change, free the new string.
                    SDL_free(utf8locale);
                } else {  // changed, swap them out, free the old string.
                    char *tmp = system_locale;
                    system_locale = utf8locale;
                    SDL_free(tmp);
                }
            }
        }
        SDL_free(utf8region);
        SDL_free(utf8lang);
    }

    return retval;
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSSystemLocaleChanged(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    if (UpdateSystemLocale(env)) {
        SDL_SendLocaleChangedEvent();
    }
}


// !!! FIXME: remove the `SDL_` prefix on these static functions.

// Callbacks into our custom XComponent.
static void SDL_XComponent_OnSurfaceCreatedCallback(OH_NativeXComponent* component, void* window)
{
    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup. Are you using our startup scripts?
    extern void SDL_OpenHarmonyMainSurfaceCreated(void);  // this is in src/main/openharmony/SDL_sysmain_runapp.c
    SDL_OpenHarmonyVideoSurfaceCreated(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
    SDL_OpenHarmonyMainSurfaceCreated();  // start the actual native code app if this is the first surface.
}

static void SDL_XComponent_OnFrameCallback(OH_NativeXComponent* component, uint64_t timestamp, uint64_t targetTimestamp)
{
    extern void SDL_OpenHarmonyOnFrameCallback(void);  // this is in src/main/openharmony/SDL_sysmain_callbacks.c
    SDL_OpenHarmonyOnFrameCallback();  // This fires SDL_AppInterate.
}

static void SDL_XComponent_OnSurfaceChangedCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyVideoSurfaceChanged(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_OnSurfaceDestroyedCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyVideoSurfaceDestroyed(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchTouchEventCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyDispatchTouchEvent(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchMouseEventCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyDispatchMouseEvent(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchHoverEventCallback(OH_NativeXComponent* component, bool isHover)
{
    // !!! FIXME: use this?
}

static void SDL_XComponent_DispatchUIInputEventCallback(OH_NativeXComponent* component, ArkUI_UIInputEvent* event, ArkUI_UIInputEvent_Type type)
{
    SDL_OpenHarmonyDispatchUIInputEvent(component, event, type);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchKeyEventCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyDispatchKeyEvent(component, window);
}

static void OpenHarmonyCommonEventReceiver(const CommonEvent_RcvData *data)
{
    const char *evname = OH_CommonEvent_GetEventFromRcvData(data);
    if (!evname) {
        return;
    } else if (SDL_strcmp(evname, COMMON_EVENT_LOCALE_CHANGED) == 0) {
        // make sure we're in the javascript thread so we can call into the i18n system object via NAPI.
        // this will update system_locale and then fire the SDL locale-changed event if appropriate.
        CallNapiThreadsafeFunction(syslocalechanged_threadsafefn, NULL, napi_tsfn_nonblocking);
    }
}

static void UpdateDeviceOrientation(void)
{
    NativeDisplayManager_Orientation orientation = DISPLAY_MANAGER_PORTRAIT;
    OH_NativeDisplayManager_GetDefaultDisplayOrientation(&orientation);

    switch (orientation) {
        #define CHECKROT(ohenum, sdlenum) case DISPLAY_MANAGER_##ohenum: device_orientation = SDL_ORIENTATION_##sdlenum; break
        CHECKROT(PORTRAIT, PORTRAIT);
        CHECKROT(LANDSCAPE, LANDSCAPE);
        CHECKROT(PORTRAIT_INVERTED, PORTRAIT_FLIPPED);
        CHECKROT(LANDSCAPE_INVERTED, LANDSCAPE_FLIPPED);
        CHECKROT(UNKNOWN, UNKNOWN);
        #undef CHECKROT
    }
}

// Called when phone/tablet rotates to a new orientation.
static void OnDisplayChangeCallback(uint64_t displayId)
{
    uint64_t defdpyid = 0;
    OH_NativeDisplayManager_GetDefaultDisplayId(&defdpyid);
    if (displayId != defdpyid) {
        return;  // we don't care if external displays change orientation.
    }
    UpdateDeviceOrientation();
}


// Called when windowStage.loadContent finishes.
static napi_value SDL_JS_LoadContentResult(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY(1);
    if (GetNapiInt(env, GetNapiObjField(env, argv[0], "code"), -1) != 0) {
        OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Failed to load the content page! Aborting!");
        exit(1);
    }
    //OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Succeeded in loading the content page. Startup may now continue.");

    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onDestroy().
static napi_value SDL_JS_UIAbility_OnDestroy(napi_env env, napi_callback_info info)
{
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_SendQuit();
    SDL_OnApplicationWillTerminate();

    if (commonevent_subscriber) {
        OH_CommonEvent_UnSubscribe(commonevent_subscriber);
        OH_CommonEvent_DestroySubscriber(commonevent_subscriber);
        commonevent_subscriber = NULL;
    }

    SDL_free(system_locale);
    system_locale = NULL;

    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onForeground().
// This is for older devices; on newer ones we use OnWillForeground and OnDidForeground instead.
static napi_value SDL_JS_UIAbility_OnForeground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationWillEnterForeground();
    SDL_OnApplicationDidEnterForeground();
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onWillForeground().
// This is for newer devices; on older ones we use OnForeground instead.
static napi_value SDL_JS_UIAbility_OnWillForeground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationWillEnterForeground();
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onDidForeground().
// This is for newer devices; on older ones we use OnForeground instead.
static napi_value SDL_JS_UIAbility_OnDidForeground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationDidEnterForeground();
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onBackground().
// This is for older devices; on newer ones we use OnWillBackground and OnDidBackground instead.
static napi_value SDL_JS_UIAbility_OnBackground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationWillEnterBackground();
    SDL_OnApplicationDidEnterBackground();
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onWillBackground().
// This is for newer devices; on older ones we use OnBackground instead.
static napi_value SDL_JS_UIAbility_OnWillBackground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationWillEnterBackground();
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onDidBackground().
// This is for newer devices; on older ones we use OnBackground instead.
static napi_value SDL_JS_UIAbility_OnDidBackground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationDidEnterBackground();
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onWindowStageCreate().
static napi_value SDL_JS_UIAbility_OnWindowStageCreate(napi_env env, napi_callback_info info)
{
    // grab the window object, load "pages/Index" to continue startup.
    //OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "%{public}s", SDL_FUNCTION);
    SDL_JS_ENTRY(1);
    napi_value windowStage = argv[0];
    napi_value window = CallNapiMethod(env, windowStage, "getMainWindowSync", 0, NULL);
    napi_create_reference(env, window, 1, &window_ref);

    napi_value args[] = { CreateNapiString(env, "pages/Index"), CreateNapiFunction(env, NULL, SDL_JS_LoadContentResult, NULL) };
    CallNapiMethod(env, windowStage, "loadContent", SDL_arraysize(args), args);

    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onWindowStageDestroy().
static napi_value SDL_JS_UIAbility_OnWindowStageDestroy(napi_env env, napi_callback_info info)
{
    return GetNapiUndefined(env);
}

// Our native version of UIAbility.onMemoryLevel().
static napi_value SDL_JS_UIAbility_OnMemoryLevel(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY(1);
    if (GetNapiInt(env, argv[0], 0) == 2 /*MEMORY_LEVEL_CRITICAL*/) {
        SDL_OnApplicationDidReceiveMemoryWarning();
    }
    return GetNapiUndefined(env);
}

static SDL_SystemTheme GetSDLSystemThemeFromAbilityConfig(napi_env env, napi_value config)
{
    switch (GetNapiInt(env, GetNapiObjField(env, config, "colorMode"), -1)) {
        case 0: return SDL_SYSTEM_THEME_DARK;
        case 1: return SDL_SYSTEM_THEME_LIGHT;
        default: return SDL_SYSTEM_THEME_UNKNOWN;
    }
}

// Our native version of UIAbility.onConfigurationUpdate().
static napi_value SDL_JS_UIAbility_OnConfigurationUpdate(napi_env env, napi_callback_info info)
{
    SDL_JS_ENTRY(1);
    system_theme = GetSDLSystemThemeFromAbilityConfig(env, argv[0]);
    SDL_SetSystemTheme(system_theme);
    return GetNapiUndefined(env);
}

static void TakeOverUIAbility(napi_env env, napi_value ability)
{
    const int apilevel = SDL_GetOpenHarmonySDKVersion();
    napi_value prototype = GetNapiObjField(env, GetNapiObjField(env, ability, "constructor"), "prototype");

    if (apilevel < 20) {
        SetNapiObjField(env, prototype, "onForeground", CreateNapiFunction(env, "onForeground", SDL_JS_UIAbility_OnForeground, NULL));
        SetNapiObjField(env, prototype, "onBackground", CreateNapiFunction(env, "onBackground", SDL_JS_UIAbility_OnBackground, NULL));
    } else {
        SetNapiObjField(env, prototype, "onWillForeground", CreateNapiFunction(env, "onWillForeground", SDL_JS_UIAbility_OnWillForeground, NULL));
        SetNapiObjField(env, prototype, "onDidForeground", CreateNapiFunction(env, "onDidForeground", SDL_JS_UIAbility_OnDidForeground, NULL));
        SetNapiObjField(env, prototype, "onWillBackground", CreateNapiFunction(env, "onWillBackground", SDL_JS_UIAbility_OnWillBackground, NULL));
        SetNapiObjField(env, prototype, "onDidBackground", CreateNapiFunction(env, "onDidBackground", SDL_JS_UIAbility_OnDidBackground, NULL));
    }

    SetNapiObjField(env, prototype, "onDestroy", CreateNapiFunction(env, "onDestroy", SDL_JS_UIAbility_OnDestroy, NULL));
    SetNapiObjField(env, prototype, "onWindowStageCreate", CreateNapiFunction(env, "onWindowStageCreate", SDL_JS_UIAbility_OnWindowStageCreate, NULL));
    SetNapiObjField(env, prototype, "onWindowStageDestroy", CreateNapiFunction(env, "onWindowStageDestroy", SDL_JS_UIAbility_OnWindowStageDestroy, NULL));
    SetNapiObjField(env, prototype, "onMemoryLevel", CreateNapiFunction(env, "onMemoryLevel", SDL_JS_UIAbility_OnMemoryLevel, NULL));
    SetNapiObjField(env, prototype, "onConfigurationUpdate", CreateNapiFunction(env, "onConfigurationUpdate", SDL_JS_UIAbility_OnConfigurationUpdate, NULL));
}

// ArkTS calls this once near startup to pass us the Ability, so we can call back into Javascript as necessary.
static napi_value SDL_JS_ProvideArkTSObjects(napi_env env, napi_callback_info info)
{
    SDL_assert(!native_resource_mgr);  // don't call this more than once!

    // we don't bother cleaning up most things in this function, because they are intended to live as long as the process.
    // !!! FIXME: but for completeness, maybe we should, in SDL_JS_UIAbility_OnDestroy().
    #define expected_argc 5
    SDL_JS_ENTRY(expected_argc);
    if (argc != expected_argc) {
        // if you hit this, we probably changed either this C code or the ArkTS code in EntryAbility.ets and you need to update one or both to get the back in sync.
        OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "ProvideArkTSObjects: expected %{public}d objects, but got %{public}d! Script is out of sync? Aborting!", (int) expected_argc, (int) argc);
        exit(1);
    }
    #undef expected_argc

    napi_value ability = argv[0];
    napi_value atmanager = argv[1];
    napi_value i18nsystem = argv[2];
    napi_value pointer = argv[3];
    napi_value ime_controller = argv[4];
    napi_create_reference(env, ability, 1, &ability_object_ref);
    napi_create_reference(env, atmanager, 1, &atmanager_ref);
    napi_create_reference(env, i18nsystem, 1, &i18nsystem_ref);
    napi_create_reference(env, pointer, 1, &pointer_ref);
    napi_create_reference(env, ime_controller, 1, &ime_controller_ref);

    TakeOverUIAbility(env, ability);

    napi_value context = GetNapiObjField(env, ability, "context");
    napi_value resourceManager = GetNapiObjField(env, context, "resourceManager");
    native_resource_mgr = OH_ResourceManager_InitNativeResourceManager(env, resourceManager);
    napi_value config = GetNapiObjField(env, context, "config");

    system_theme = GetSDLSystemThemeFromAbilityConfig(env, config);

    napi_value on_insert_text = CreateNapiFunction(env, "onInsertText", SDL_JS_IME_Controller_OnInsertText, NULL);
    napi_create_reference(env, on_insert_text, 1, &on_insert_text_ref);
    napi_value on_delete_left = CreateNapiFunction(env, "onDeleteLeft", SDL_JS_IME_Controller_OnDeleteLeft, NULL);
    napi_create_reference(env, on_delete_left, 1, &on_delete_left_ref);

    UpdateSystemLocale(env);  // do this at startup, so we have it saved off while we know we're on the Javascript thread.

    // the Video subsystem will also register one of these for tracking displays, but this is so we can track device orientation changes independently.
    uint32_t display_change_listener_idx = 0;
    OH_NativeDisplayManager_RegisterDisplayChangeListener(OnDisplayChangeCallback, &display_change_listener_idx);
    UpdateDeviceOrientation();

    // Set up some threadsafe functions, for calling back into ArkTS from the main thread, regardless of what thread native code is operating from.
    syslocalechanged_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_SystemLocaleChanged", CallJSSystemLocaleChanged);
    req_permissions_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_RequestOpenHarmonyPermission", CallJSRequestPermissions);
    open_url_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_OpenHarmonyOpenURL", CallJSOpenURL);
    change_sysbars_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_OpenHarmonyChangeSystemBars", CallJSChangeSysBars);
    change_screensaver_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_OpenHarmonyChangeScreenSaver", CallJSChangeScreenSaver);
    show_screenkeyboard_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_OpenHarmonyShowScreenKeyboard", CallJSShowScreenKeyboard);
    hide_screenkeyboard_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_OpenHarmonyHideScreenKeyboard", CallJSHideScreenKeyboard);
    change_mouseptr_threadsafefn = CreateNapiThreadsafeFunction(env, "SDL_OpenHarmonyChangeMousePointer", CallJSChangeMousePointer);

    return GetNapiUndefined(env);
}


// This is called by SDL_RegisterNativeInterfaces when the library is loaded, which sets up the entry points where
//  ArkTS code can call into our native code.
static napi_value SDL_Init_Native_Interfaces(napi_env env, napi_value exports)
{
    // Functions that we want to be able to call from ArkTS go here.
    // (declare them in C as `napi_value MyFunctionName(napi_env env, napi_callback_info info);`)
    napi_property_descriptor desc[] = {
        { "provideArkTSObjects", NULL, SDL_JS_ProvideArkTSObjects, NULL, NULL, NULL, napi_default, NULL },
    };
    napi_define_properties(env, exports, SDL_arraysize(desc), desc);

    // Wire into our XComponent, so we can take control from C code. If any of this fails, I assume the app will either blow up or do nothing.
    napi_value xcompobj = GetNapiObjField(env, exports, OH_NATIVE_XCOMPONENT_OBJ);
    if (!xcompobj) {
        return exports;
    }

    OH_NativeXComponent *nativeXComponent = NULL;
    if (napi_unwrap(env, xcompobj, (void **) &nativeXComponent) != napi_ok) {
        return exports;
    }

    static OH_NativeXComponent_Callback xcomp_callbacks = {  // this MUST be static! It keeps a pointer to this, and doesn't make a copy, afaict!
        .OnSurfaceCreated = SDL_XComponent_OnSurfaceCreatedCallback,
        .OnSurfaceChanged = SDL_XComponent_OnSurfaceChangedCallback,
        .OnSurfaceDestroyed = SDL_XComponent_OnSurfaceDestroyedCallback,
        .DispatchTouchEvent = SDL_XComponent_DispatchTouchEventCallback
    };
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &xcomp_callbacks);
    OH_NativeXComponent_RegisterOnFrameCallback(nativeXComponent, SDL_XComponent_OnFrameCallback);

    static OH_NativeXComponent_MouseEvent_Callback xcomp_mouse_callbacks = {
        .DispatchMouseEvent = SDL_XComponent_DispatchMouseEventCallback,
        .DispatchHoverEvent = SDL_XComponent_DispatchHoverEventCallback
    };
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &xcomp_mouse_callbacks);

    // only AXIS events supported here, at the moment, apparently, but most of the other things (mouse, touch, key) come through other supported callbacks.
    // "Axis" in this case only means mousewheel, afaict.
    OH_NativeXComponent_RegisterUIInputEventCallback(nativeXComponent, SDL_XComponent_DispatchUIInputEventCallback, ARKUI_UIINPUTEVENT_TYPE_AXIS);

    OH_NativeXComponent_RegisterKeyEventCallback(nativeXComponent, SDL_XComponent_DispatchKeyEventCallback);

    static const char * const common_events[] = { COMMON_EVENT_LOCALE_CHANGED };
    CommonEvent_SubscribeInfo *subinfo = OH_CommonEvent_CreateSubscribeInfo((const char **) common_events, SDL_arraysize(common_events));
    if (subinfo) {
        commonevent_subscriber = OH_CommonEvent_CreateSubscriber(subinfo, OpenHarmonyCommonEventReceiver);
        if (commonevent_subscriber) {
            OH_CommonEvent_Subscribe(commonevent_subscriber);
        }
        OH_CommonEvent_DestroySubscribeInfo(subinfo);
    }

    return exports;
}

// This runs when libSDL3.so loads, and registers the NAPI module, so ArkTS can call into this to get going.
void __attribute__((constructor)) SDL_RegisterNativeInterfaces(void)
{
    static napi_module sdl_napi_module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = NULL,
        .nm_register_func = SDL_Init_Native_Interfaces,
        .nm_modname = "SDL3",
        .nm_priv = ((void*)0),
        .reserved = { 0 },
    };
    napi_module_register(&sdl_napi_module);
}

#endif // SDL_PLATFORM_OPENHARMONY

