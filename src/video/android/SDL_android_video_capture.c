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

#include "SDL3/SDL.h"
#include "SDL3/SDL_video_capture.h"
#include "../SDL_sysvideocapture.h"
#include "../SDL_video_capture_c.h"
#include "../SDL_pixels_c.h"
#include "../../thread/SDL_systhread.h"

#define DEBUG_VIDEO_CAPTURE_CAPTURE 0

#if defined(SDL_PLATFORM_ANDROID) && __ANDROID_API__ >= 24

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
 * Add: #define SDL_VIDEO_CAPTURE 1
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

static void
create_cameraMgr(void)
{
    if (cameraMgr == NULL) {

        if (!Android_JNI_RequestPermission("android.permission.CAMERA")) {
            SDL_SetError("This app doesn't have CAMERA permission");
            return;
        }

        cameraMgr = ACameraManager_create();
        if (cameraMgr == NULL) {
            SDL_Log("Error creating ACameraManager");
        } else {
            SDL_Log("Create ACameraManager");
        }
    }
}

static void
delete_cameraMgr(void)
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

struct SDL_PrivateVideoCaptureData
{
    ACameraDevice *device;
    ACameraCaptureSession *session;
    ACameraDevice_StateCallbacks dev_callbacks;
    ACameraCaptureSession_stateCallbacks capture_callbacks;
    ACaptureSessionOutputContainer *sessionOutputContainer;
    AImageReader *reader;
    int num_formats;
    int count_formats[6]; // see format_2_id
};


/**/
#define FORMAT_SDL SDL_PIXELFORMAT_NV12

static int
format_2_id(int fmt) {
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

static int
id_2_format(int fmt) {
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

static Uint32
format_android_2_sdl(Uint32 fmt)
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

static Uint32
format_sdl_2_android(Uint32 fmt)
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


static void
onDisconnected(void *context, ACameraDevice *device)
{
    // SDL_VideoCaptureDevice *_this = (SDL_VideoCaptureDevice *) context;
    SDL_Log("CB onDisconnected");
}

static void
onError(void *context, ACameraDevice *device, int error)
{
    // SDL_VideoCaptureDevice *_this = (SDL_VideoCaptureDevice *) context;
    SDL_Log("CB onError");
}


static void
onClosed(void* context, ACameraCaptureSession *session)
{
    // SDL_VideoCaptureDevice *_this = (SDL_VideoCaptureDevice *) context;
    SDL_Log("CB onClosed");
}

static void
onReady(void* context, ACameraCaptureSession *session)
{
    // SDL_VideoCaptureDevice *_this = (SDL_VideoCaptureDevice *) context;
    SDL_Log("CB onReady");
}

static void
onActive(void* context, ACameraCaptureSession *session)
{
    // SDL_VideoCaptureDevice *_this = (SDL_VideoCaptureDevice *) context;
    SDL_Log("CB onActive");
}

int
OpenDevice(SDL_VideoCaptureDevice *_this)
{
    camera_status_t res;

    /* Cannot open a second camera, while the first one is opened.
     * If you want to play several camera, they must all be opened first, then played.
     *
     * https://developer.android.com/reference/android/hardware/camera2/CameraManager
     * "All camera devices intended to be operated concurrently, must be opened using openCamera(String, CameraDevice.StateCallback, Handler),
     * before configuring sessions on any of the camera devices.  * "
     *
     */
    if (check_device_playing()) {
        return SDL_SetError("A camera is already playing");
    }

    _this->hidden = (struct SDL_PrivateVideoCaptureData *) SDL_calloc(1, sizeof (struct SDL_PrivateVideoCaptureData));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    create_cameraMgr();

    _this->hidden->dev_callbacks.context = (void *) _this;
    _this->hidden->dev_callbacks.onDisconnected = onDisconnected;
    _this->hidden->dev_callbacks.onError = onError;

    res = ACameraManager_openCamera(cameraMgr, _this->dev_name, &_this->hidden->dev_callbacks, &_this->hidden->device);
    if (res != ACAMERA_OK) {
        return SDL_SetError("Failed to open camera");
    }

    return 0;
}

void
CloseDevice(SDL_VideoCaptureDevice *_this)
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

    if (check_all_device_closed()) {
        delete_cameraMgr();
    }
}

int
InitDevice(SDL_VideoCaptureDevice *_this)
{
    size_t size, pitch;
    SDL_CalculateSize(_this->spec.format, _this->spec.width, _this->spec.height, &size, &pitch, SDL_FALSE);
    SDL_Log("Buffer size: %d x %d", _this->spec.width, _this->spec.height);
    return 0;
}

int
GetDeviceSpec(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureSpec *spec)
{
    if (spec) {
        *spec = _this->spec;
        return 0;
    }
    return -1;
}

int
StartCapture(SDL_VideoCaptureDevice *_this)
{
    camera_status_t res;
    media_status_t res2;
    ANativeWindow *window = NULL;
    ACaptureSessionOutput *sessionOutput;
    ACameraOutputTarget *outputTarget;
    ACaptureRequest *request;

    res2 = AImageReader_new(_this->spec.width, _this->spec.height, format_sdl_2_android(_this->spec.format), 10 /* nb buffers */, &_this->hidden->reader);
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

int
StopCapture(SDL_VideoCaptureDevice *_this)
{
    ACameraCaptureSession_close(_this->hidden->session);
    _this->hidden->session = NULL;
    return 0;
}

int
AcquireFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    media_status_t res;
    AImage *image;
    res = AImageReader_acquireNextImage(_this->hidden->reader, &image);
    /* We could also use this one:
    res = AImageReader_acquireLatestImage(_this->hidden->reader, &image);
    */
    if (res == AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE ) {

        SDL_Delay(20); // TODO fix some delay
#if DEBUG_VIDEO_CAPTURE_CAPTURE
//        SDL_Log("AImageReader_acquireNextImage: AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE");
#endif
        return 0;
    } else if (res == AMEDIA_OK ) {
        int i = 0;
        int32_t numPlanes = 0;
        AImage_getNumberOfPlanes(image, &numPlanes);

        frame->timestampNS = SDL_GetTicksNS();

        for (i = 0; i < numPlanes && i < 3; i++) {
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
        return 0;
    } else if (res == AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED) {
        SDL_SetError("AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED");
    } else {
        SDL_SetError("AImageReader_acquireNextImage: %d", res);
    }

    return -1;
}

int
ReleaseFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    if (frame->internal){
        AImage_delete((AImage *)frame->internal);
    }
    return 0;
}

int
GetNumFormats(SDL_VideoCaptureDevice *_this)
{
    camera_status_t res;
    int i;
    int unknown = 0;
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

    for (i = 0; i < entry.count; i += 4) {
        int32_t format = entry.data.i32[i + 0];
        int32_t type = entry.data.i32[i + 3];
        Uint32 fmt;

        if (type == ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
            continue;
        }

        fmt = format_android_2_sdl(format);
        _this->hidden->count_formats[format_2_id(fmt)] += 1;

#if DEBUG_VIDEO_CAPTURE_CAPTURE
        if (fmt != SDL_PIXELFORMAT_UNKNOWN) {
            int w = entry.data.i32[i + 1];
            int h = entry.data.i32[i + 2];
            SDL_Log("Got format android 0x%08x -> %s %d x %d", format, SDL_GetPixelFormatName(fmt), w, h);
        } else {
            unknown += 1;
        }
#endif
    }

#if DEBUG_VIDEO_CAPTURE_CAPTURE
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

int
GetFormat(SDL_VideoCaptureDevice *_this, int index, Uint32 *format)
{
    int i;
    int i2 = 0;

    if (_this->hidden->num_formats == 0) {
        GetNumFormats(_this);
    }

    if (index < 0 || index >= _this->hidden->num_formats) {
        return -1;
    }

    for (i = 0; i < SDL_arraysize(_this->hidden->count_formats); i++) {
        if (_this->hidden->count_formats[i] == 0) {
            continue;
        }

        if (i2 == index) {
            *format = id_2_format(i);
        }

        i2++;

    }
    return 0;
}

int
GetNumFrameSizes(SDL_VideoCaptureDevice *_this, Uint32 format)
{
    int i, i2 = 0, index;
    if (_this->hidden->num_formats == 0) {
        GetNumFormats(_this);
    }

    index = format_2_id(format);

    for (i = 0; i < SDL_arraysize(_this->hidden->count_formats); i++) {
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

int
GetFrameSize(SDL_VideoCaptureDevice *_this, Uint32 format, int index, int *width, int *height)
{
    camera_status_t res;
    int i, i2 = 0;
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

    for (i = 0; i < entry.count; i += 4) {
        int32_t f = entry.data.i32[i + 0];
        int w = entry.data.i32[i + 1];
        int h = entry.data.i32[i + 2];
        int32_t type = entry.data.i32[i + 3];
        Uint32 fmt;

        if (type == ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
            continue;
        }


        fmt = format_android_2_sdl(f);
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

static int GetNumDevices(void);

int
GetDeviceName(SDL_VideoCaptureDeviceID instance_id, char *buf, int size)
{
    int index = instance_id - 1;
    create_cameraMgr();

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

static int
GetNumDevices(void)
{
    camera_status_t res;
    create_cameraMgr();

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

SDL_VideoCaptureDeviceID *GetVideoCaptureDevices(int *count)
{
    /* hard-coded list of ID */
    int i;
    int num = GetNumDevices();
    SDL_VideoCaptureDeviceID *ret;

    ret = (SDL_VideoCaptureDeviceID *)SDL_malloc((num + 1) * sizeof(*ret));

    if (ret == NULL) {
        SDL_OutOfMemory();
        *count = 0;
        return NULL;
    }

    for (i = 0; i < num; i++) {
        ret[i] = i + 1;
    }
    ret[num] = 0;
    *count = num;
    return ret;
}

int SDL_SYS_VideoCaptureInit(void) {
    return 0;
}

int SDL_SYS_VideoCaptureQuit(void) {
    return 0;
}


#endif


