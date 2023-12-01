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

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"
#include "../../video/SDL_pixels_c.h"
#include "../../thread/SDL_systhread.h"

#if defined(SDL_CAMERA_DRIVER_ANDROID)

#if __ANDROID_API__ >= 24

/*
 * APP_PLATFORM=android-24
 * minSdkVersion=24
 *
 * link with: -lcamera2ndk -lmediandk
 *
 * AndroidManifest.xml:
 *   <uses-permission android:name="android.permission.CAMERA"></uses-permission>
 *   <uses-feature android:name="android.hardware.camera" />
 *
 *
 * Add: #define SDL_CAMERA 1
 * in:  include/build_config/SDL_build_config_android.h
 *
 *
 * Very likely SDL must be build with YUV support (done by default)
 *
 * https://developer.android.com/reference/android/hardware/camera2/CameraManager
 * "All camera devices intended to be operated concurrently, must be opened using openCamera(String, CameraDevice.StateCallback, Handler),
 * before configuring sessions on any of the camera devices.  * "
 */

#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include "../../core/android/SDL_android.h"


static ACameraManager *cameraMgr = NULL;
static ACameraIdList *cameraIdList = NULL;

static int CreateCameraManager(void)
{
    if (cameraMgr == NULL) {
        #if 0  // !!! FIXME: this is getting replaced in a different branch.
        if (!Android_JNI_RequestPermission("android.permission.CAMERA")) {
            SDL_SetError("This app doesn't have CAMERA permission");
            return;
        }
        #endif
        cameraMgr = ACameraManager_create();
        if (cameraMgr == NULL) {
            SDL_Log("Error creating ACameraManager");
        } else {
            SDL_Log("Create ACameraManager");
        }
    }

    cameraMgr = ACameraManager_create();

    return cameraMgr ? 0 : SDL_SetError("Error creating ACameraManager");
}

static void DestroyCameraManager(void)
{
    if (cameraIdList) {
        ACameraManager_deleteCameraIdList(cameraIdList);
        cameraIdList = NULL;
    }

    if (cameraMgr) {
        ACameraManager_delete(cameraMgr);
        cameraMgr = NULL;
    }
}

struct SDL_PrivateCameraData
{
    ACameraDevice *device;
    ACameraCaptureSession *session;
    ACameraDevice_StateCallbacks dev_callbacks;
    ACameraCaptureSession_stateCallbacks capture_callbacks;
    ACaptureSessionOutputContainer *sessionOutputContainer;
    AImageReader *reader;
    int num_formats;
    int count_formats[6]; // see format_to_id
};


#define FORMAT_SDL SDL_PIXELFORMAT_NV12

static int format_to_id(int fmt) {
     switch (fmt) {
        #define CASE(x, y)  case x: return y
        CASE(FORMAT_SDL, 0);
        CASE(SDL_PIXELFORMAT_RGB565, 1);
        CASE(SDL_PIXELFORMAT_XRGB8888, 2);
        CASE(SDL_PIXELFORMAT_RGBA8888, 3);
        CASE(SDL_PIXELFORMAT_RGBX8888, 4);
        CASE(SDL_PIXELFORMAT_UNKNOWN, 5);
        #undef CASE
        default:
            return 5;
    }
}

static int id_to_format(int fmt) {
     switch (fmt) {
        #define CASE(x, y)  case y: return x
        CASE(FORMAT_SDL, 0);
        CASE(SDL_PIXELFORMAT_RGB565, 1);
        CASE(SDL_PIXELFORMAT_XRGB8888, 2);
        CASE(SDL_PIXELFORMAT_RGBA8888, 3);
        CASE(SDL_PIXELFORMAT_RGBX8888, 4);
        CASE(SDL_PIXELFORMAT_UNKNOWN, 5);
        #undef CASE
        default:
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static Uint32 format_android_to_sdl(Uint32 fmt)
{
    switch (fmt) {
        #define CASE(x, y)  case x: return y
        CASE(AIMAGE_FORMAT_YUV_420_888, FORMAT_SDL);
        CASE(AIMAGE_FORMAT_RGB_565,     SDL_PIXELFORMAT_RGB565);
        CASE(AIMAGE_FORMAT_RGB_888,     SDL_PIXELFORMAT_XRGB8888);
        CASE(AIMAGE_FORMAT_RGBA_8888,   SDL_PIXELFORMAT_RGBA8888);
        CASE(AIMAGE_FORMAT_RGBX_8888,   SDL_PIXELFORMAT_RGBX8888);

        CASE(AIMAGE_FORMAT_RGBA_FP16,   SDL_PIXELFORMAT_UNKNOWN); // 64bits
        CASE(AIMAGE_FORMAT_RAW_PRIVATE, SDL_PIXELFORMAT_UNKNOWN);
        CASE(AIMAGE_FORMAT_JPEG,        SDL_PIXELFORMAT_UNKNOWN);
        #undef CASE
        default:
            SDL_Log("Unknown format AIMAGE_FORMAT '%d'", fmt);
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static Uint32 format_sdl_to_android(Uint32 fmt)
{
    switch (fmt) {
        #define CASE(x, y)  case y: return x
        CASE(AIMAGE_FORMAT_YUV_420_888, FORMAT_SDL);
        CASE(AIMAGE_FORMAT_RGB_565,     SDL_PIXELFORMAT_RGB565);
        CASE(AIMAGE_FORMAT_RGB_888,     SDL_PIXELFORMAT_XRGB8888);
        CASE(AIMAGE_FORMAT_RGBA_8888,   SDL_PIXELFORMAT_RGBA8888);
        CASE(AIMAGE_FORMAT_RGBX_8888,   SDL_PIXELFORMAT_RGBX8888);
        #undef CASE
        default:
            return 0;
    }
}


static void onDisconnected(void *context, ACameraDevice *device)
{
    // SDL_CameraDevice *_this = (SDL_CameraDevice *) context;
    #if DEBUG_CAMERA
    SDL_Log("CB onDisconnected");
    #endif
}

static void onError(void *context, ACameraDevice *device, int error)
{
    // SDL_CameraDevice *_this = (SDL_CameraDevice *) context;
    #if DEBUG_CAMERA
    SDL_Log("CB onError");
    #endif
}


static void onClosed(void* context, ACameraCaptureSession *session)
{
    // SDL_CameraDevice *_this = (SDL_CameraDevice *) context;
    #if DEBUG_CAMERA
    SDL_Log("CB onClosed");
    #endif
}

static void onReady(void* context, ACameraCaptureSession *session)
{
    // SDL_CameraDevice *_this = (SDL_CameraDevice *) context;
    #if DEBUG_CAMERA
    SDL_Log("CB onReady");
    #endif
}

static void onActive(void* context, ACameraCaptureSession *session)
{
    // SDL_CameraDevice *_this = (SDL_CameraDevice *) context;
    #if DEBUG_CAMERA
    SDL_Log("CB onActive");
    #endif
}

static int ANDROIDCAMERA_OpenDevice(SDL_CameraDevice *_this)
{
    /* Cannot open a second camera, while the first one is opened.
     * If you want to play several camera, they must all be opened first, then played.
     *
     * https://developer.android.com/reference/android/hardware/camera2/CameraManager
     * "All camera devices intended to be operated concurrently, must be opened using openCamera(String, CameraDevice.StateCallback, Handler),
     * before configuring sessions on any of the camera devices.  * "
     *
     */
    if (CheckDevicePlaying()) {
        return SDL_SetError("A camera is already playing");
    }

    _this->hidden = (struct SDL_PrivateCameraData *) SDL_calloc(1, sizeof (struct SDL_PrivateCameraData));
    if (_this->hidden == NULL) {
        return -1;
    }

    CreateCameraManager();

    _this->hidden->dev_callbacks.context = (void *) _this;
    _this->hidden->dev_callbacks.onDisconnected = onDisconnected;
    _this->hidden->dev_callbacks.onError = onError;

    camera_status_t res = ACameraManager_openCamera(cameraMgr, _this->dev_name, &_this->hidden->dev_callbacks, &_this->hidden->device);
    if (res != ACAMERA_OK) {
        return SDL_SetError("Failed to open camera");
    }

    return 0;
}

static void ANDROIDCAMERA_CloseDevice(SDL_CameraDevice *_this)
{
    if (_this && _this->hidden) {
        if (_this->hidden->session) {
            ACameraCaptureSession_close(_this->hidden->session);
        }

        if (_this->hidden->sessionOutputContainer) {
            ACaptureSessionOutputContainer_free(_this->hidden->sessionOutputContainer);
        }

        if (_this->hidden->reader) {
            AImageReader_delete(_this->hidden->reader);
        }

        if (_this->hidden->device) {
            ACameraDevice_close(_this->hidden->device);
        }

        SDL_free(_this->hidden);

        _this->hidden = NULL;
    }
}

static int ANDROIDCAMERA_InitDevice(SDL_CameraDevice *_this)
{
    size_t size, pitch;
    SDL_CalculateSize(_this->spec.format, _this->spec.width, _this->spec.height, &size, &pitch, SDL_FALSE);
    SDL_Log("Buffer size: %d x %d", _this->spec.width, _this->spec.height);
    return 0;
}

static int ANDROIDCAMERA_GetDeviceSpec(SDL_CameraDevice *_this, SDL_CameraSpec *spec)
{
    // !!! FIXME: catch NULLs at higher level
    if (spec) {
        SDL_copyp(spec, &_this->spec);
        return 0;
    }
    return -1;
}

static int ANDROIDCAMERA_StartCamera(SDL_CameraDevice *_this)
{
    // !!! FIXME: maybe log the error code in SDL_SetError
    camera_status_t res;
    media_status_t res2;
    ANativeWindow *window = NULL;
    ACaptureSessionOutput *sessionOutput;
    ACameraOutputTarget *outputTarget;
    ACaptureRequest *request;

    res2 = AImageReader_new(_this->spec.width, _this->spec.height, format_sdl_to_android(_this->spec.format), 10 /* nb buffers */, &_this->hidden->reader);
    if (res2 != AMEDIA_OK) {
        SDL_SetError("Error AImageReader_new");
        goto error;
    }
    res2 = AImageReader_getWindow(_this->hidden->reader, &window);
    if (res2 != AMEDIA_OK) {
        SDL_SetError("Error AImageReader_new");
        goto error;
    }

    res = ACaptureSessionOutput_create(window, &sessionOutput);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACaptureSessionOutput_create");
        goto error;
    }
    res = ACaptureSessionOutputContainer_create(&_this->hidden->sessionOutputContainer);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACaptureSessionOutputContainer_create");
        goto error;
    }
    res = ACaptureSessionOutputContainer_add(_this->hidden->sessionOutputContainer, sessionOutput);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACaptureSessionOutputContainer_add");
        goto error;
    }

    res = ACameraOutputTarget_create(window, &outputTarget);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACameraOutputTarget_create");
        goto error;
    }

    res = ACameraDevice_createCaptureRequest(_this->hidden->device, TEMPLATE_RECORD, &request);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACameraDevice_createCaptureRequest");
        goto error;
    }

    res = ACaptureRequest_addTarget(request, outputTarget);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACaptureRequest_addTarget");
        goto error;
    }

    _this->hidden->capture_callbacks.context = (void *) _this;
    _this->hidden->capture_callbacks.onClosed = onClosed;
    _this->hidden->capture_callbacks.onReady = onReady;
    _this->hidden->capture_callbacks.onActive = onActive;

    res = ACameraDevice_createCaptureSession(_this->hidden->device,
            _this->hidden->sessionOutputContainer,
            &_this->hidden->capture_callbacks,
            &_this->hidden->session);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACameraDevice_createCaptureSession");
        goto error;
    }

    res = ACameraCaptureSession_setRepeatingRequest(_this->hidden->session, NULL, 1, &request, NULL);
    if (res != ACAMERA_OK) {
        SDL_SetError("Error ACameraDevice_createCaptureSession");
        goto error;
    }

    return 0;

error:
    return -1;
}

static int ANDROIDCAMERA_StopCamera(SDL_CameraDevice *_this)
{
    ACameraCaptureSession_close(_this->hidden->session);
    _this->hidden->session = NULL;
    return 0;
}

static int ANDROIDCAMERA_AcquireFrame(SDL_CameraDevice *_this, SDL_CameraFrame *frame)
{
    media_status_t res;
    AImage *image;
    res = AImageReader_acquireNextImage(_this->hidden->reader, &image);
    /* We could also use this one:
    res = AImageReader_acquireLatestImage(_this->hidden->reader, &image);
    */
    if (res == AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE ) {
        SDL_Delay(20); // TODO fix some delay
        #if DEBUG_CAMERA
        //SDL_Log("AImageReader_acquireNextImage: AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE");
        #endif
    } else if (res == AMEDIA_OK ) {
        int32_t numPlanes = 0;
        AImage_getNumberOfPlanes(image, &numPlanes);

        frame->timestampNS = SDL_GetTicksNS();

        for (int i = 0; i < numPlanes && i < 3; i++) {
            int dataLength = 0;
            int rowStride = 0;
            uint8_t *data = NULL;
            frame->num_planes += 1;
            AImage_getPlaneRowStride(image, i, &rowStride);
            res = AImage_getPlaneData(image, i, &data, &dataLength);
            if (res == AMEDIA_OK) {
                frame->data[i] = data;
                frame->pitch[i] = rowStride;
            }
        }

        if (frame->num_planes == 3) {
            /* plane 2 and 3 are interleaved NV12. SDL only takes two planes for this format */
            int pixelStride = 0;
            AImage_getPlanePixelStride(image, 1, &pixelStride);
            if (pixelStride == 2) {
                frame->num_planes -= 1;
            }
        }

        frame->internal = (void*)image;
    } else if (res == AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED) {
        return SDL_SetError("AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED");
    } else {
        return SDL_SetError("AImageReader_acquireNextImage: %d", res);
    }

    return 0;
}

static int ANDROIDCAMERA_ReleaseFrame(SDL_CameraDevice *_this, SDL_CameraFrame *frame)
{
    if (frame->internal){
        AImage_delete((AImage *)frame->internal);
    }
    return 0;
}

static int ANDROIDCAMERA_GetNumFormats(SDL_CameraDevice *_this)
{
    camera_status_t res;
    SDL_bool unknown = SDL_FALSE;
    ACameraMetadata *metadata;
    ACameraMetadata_const_entry entry;

    if (_this->hidden->num_formats != 0) {
        return _this->hidden->num_formats;
    }

    res = ACameraManager_getCameraCharacteristics(cameraMgr, _this->dev_name, &metadata);
    if (res != ACAMERA_OK) {
        return -1;
    }

    res = ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
    if (res != ACAMERA_OK) {
        return -1;
    }

    SDL_Log("got entry ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS");

    for (int i = 0; i < entry.count; i += 4) {
        const int32_t format = entry.data.i32[i + 0];
        const int32_t type = entry.data.i32[i + 3];

        if (type == ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
            continue;
        }

        const Uint32 fmt = format_android_to_sdl(format);
        _this->hidden->count_formats[format_to_id(fmt)] += 1;

        #if DEBUG_CAMERA
        if (fmt != SDL_PIXELFORMAT_UNKNOWN) {
            int w = entry.data.i32[i + 1];
            int h = entry.data.i32[i + 2];
            SDL_Log("Got format android 0x%08x -> %s %d x %d", format, SDL_GetPixelFormatName(fmt), w, h);
        } else {
            unknown = SDL_TRUE;
        }
        #endif
    }

    #if DEBUG_CAMERA
    if (unknown) {
        SDL_Log("Got unknown android");
    }
    #endif


    if ( _this->hidden->count_formats[0]) _this->hidden->num_formats += 1;
    if ( _this->hidden->count_formats[1]) _this->hidden->num_formats += 1;
    if ( _this->hidden->count_formats[2]) _this->hidden->num_formats += 1;
    if ( _this->hidden->count_formats[3]) _this->hidden->num_formats += 1;
    if ( _this->hidden->count_formats[4]) _this->hidden->num_formats += 1;
    if ( _this->hidden->count_formats[5]) _this->hidden->num_formats += 1;

    return _this->hidden->num_formats;
}

static int ANDROIDCAMERA_GetFormat(SDL_CameraDevice *_this, int index, Uint32 *format)
{
    int i2 = 0;

    if (_this->hidden->num_formats == 0) {
        GetNumFormats(_this);
    }

    if (index < 0 || index >= _this->hidden->num_formats) {
        // !!! FIXME: call SDL_SetError()?
        return -1;
    }

    for (int i = 0; i < SDL_arraysize(_this->hidden->count_formats); i++) {
        if (_this->hidden->count_formats[i] == 0) {
            continue;
        }

        if (i2 == index) {
            *format = id_to_format(i);
        }

        i2++;

    }
    return 0;
}

static int ANDROIDCAMERA_GetNumFrameSizes(SDL_CameraDevice *_this, Uint32 format)
{
    // !!! FIXME: call SDL_SetError()?
    if (_this->hidden->num_formats == 0) {
        GetNumFormats(_this);
    }

    const int index = format_to_id(format);

    int i2 = 0;
    for (int i = 0; i < SDL_arraysize(_this->hidden->count_formats); i++) {
        if (_this->hidden->count_formats[i] == 0) {
            continue;
        }

        if (i2 == index) {
            /* number of resolution for this format */
            return _this->hidden->count_formats[i];
        }

        i2++;
    }

    return -1;
}

static int ANDROIDCAMERA_GetFrameSize(SDL_CameraDevice *_this, Uint32 format, int index, int *width, int *height)
{
    // !!! FIXME: call SDL_SetError()?
    camera_status_t res;
    ACameraMetadata *metadata;
    ACameraMetadata_const_entry entry;

    if (_this->hidden->num_formats == 0) {
        GetNumFormats(_this);
    }

    res = ACameraManager_getCameraCharacteristics(cameraMgr, _this->dev_name, &metadata);
    if (res != ACAMERA_OK) {
        return -1;
    }

    res = ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
    if (res != ACAMERA_OK) {
        return -1;
    }

    int i2 = 0;
    for (int i = 0; i < entry.count; i += 4) {
        int32_t f = entry.data.i32[i + 0];
        const int w = entry.data.i32[i + 1];
        const int h = entry.data.i32[i + 2];
        int32_t type = entry.data.i32[i + 3];

        if (type == ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
            continue;
        }

        Uint32 fmt = format_android_to_sdl(f);
        if (fmt != format) {
            continue;
        }

        if (i2 == index) {
            *width = w;
            *height = h;
            return 0;
        }

        i2++;
    }

    return -1;
}

static int ANDROIDCAMERA_GetNumDevices(void)
{
    camera_status_t res;
    CreateCameraManager();

    if (cameraIdList) {
        ACameraManager_deleteCameraIdList(cameraIdList);
        cameraIdList = NULL;
    }

    res = ACameraManager_getCameraIdList(cameraMgr, &cameraIdList);

    if (res == ACAMERA_OK) {
        if (cameraIdList) {
            return cameraIdList->numCameras;
        }
    }
    return -1;
}

static int ANDROIDCAMERA_GetDeviceName(SDL_CameraDeviceID instance_id, char *buf, int size)
{
    // !!! FIXME: call SDL_SetError()?
    int index = instance_id - 1;
    CreateCameraManager();

    if (cameraIdList == NULL) {
        GetNumDevices();
    }

    if (cameraIdList) {
        if (index >= 0 && index < cameraIdList->numCameras) {
            SDL_snprintf(buf, size, "%s", cameraIdList->cameraIds[index]);
            return 0;
        }
    }

    return -1;
}

static SDL_CameraDeviceID *ANDROIDCAMERA_GetDevices(int *count)
{
    // hard-coded list of ID
    const int num = GetNumDevices();
    SDL_CameraDeviceID *retval = (SDL_CameraDeviceID *)SDL_malloc((num + 1) * sizeof(*ret));

    if (retval == NULL) {
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < num; i++) {
        retval[i] = i + 1;
    }
    retval[num] = 0;
    *count = num;
    return retval;
}

static void ANDROIDCAMERA_Deinitialize(void)
{
    DestroyCameraManager();
}

#endif  // __ANDROID_API__ >= 24


static SDL_bool ANDROIDCAMERA_Init(SDL_CameraDriverImpl *impl)
{
#if __ANDROID_API__ < 24
    return SDL_FALSE;
#else
    if (CreateCameraManager() < 0) {
        return SDL_FALSE;
    }

    impl->DetectDevices = ANDROIDCAMERA_DetectDevices;
    impl->OpenDevice = ANDROIDCAMERA_OpenDevice;
    impl->CloseDevice = ANDROIDCAMERA_CloseDevice;
    impl->InitDevice = ANDROIDCAMERA_InitDevice;
    impl->GetDeviceSpec = ANDROIDCAMERA_GetDeviceSpec;
    impl->StartCamera = ANDROIDCAMERA_StartCamera;
    impl->StopCamera = ANDROIDCAMERA_StopCamera;
    impl->AcquireFrame = ANDROIDCAMERA_AcquireFrame;
    impl->ReleaseFrame = ANDROIDCAMERA_ReleaseFrame;
    impl->GetNumFormats = ANDROIDCAMERA_GetNumFormats;
    impl->GetFormat = ANDROIDCAMERA_GetFormat;
    impl->GetNumFrameSizes = ANDROIDCAMERA_GetNumFrameSizes;
    impl->GetFrameSize = ANDROIDCAMERA_GetFrameSize;
    impl->GetDeviceName = ANDROIDCAMERA_GetDeviceName;
    impl->GetDevices = ANDROIDCAMERA_GetDevices;
    impl->Deinitialize = ANDROIDCAMERA_Deinitialize;

    return SDL_TRUE;
#endif
}

CameraBootStrap ANDROIDCAMERA_bootstrap = {
    "android", "SDL Android camera driver", ANDROIDCAMERA_Init, SDL_FALSE
};

#endif

