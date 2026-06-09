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

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"
#include "../../video/SDL_pixels_c.h"
#include "../../video/SDL_surface_c.h"
#include "../../core/openharmony/SDL_openharmony.h"

#ifdef SDL_CAMERA_DRIVER_OPENHARMONY

/*
 * module.json5:
 *   { "name": "ohos.permission.CAMERA", "reason": "$string:perm_reason_CAMERA", "usedScene": { "abilities": [ "EntryAbility" ], "when": "inuse" } },
 *
 * Very likely SDL must be build with YUV support (done by default)
 */

// !!! FIXME: Using a "preview output" is the only way I could find to read pixels on the CPU. If I
// !!! FIXME:  try to use a "video output" with an image receiver, it doesn't work: I get data from
// !!! FIXME:  the camera and I can see motion in the output if I wave my hand in front of the camera,
// !!! FIXME:  but it's _definitely_ not the NV21 format it claims to be, and the image is some weird
// !!! FIXME:  scrambled thing split into rows.
// !!! FIXME: While preview output is "fine," it doesn't give us access to various camera framerates
// !!! FIXME:  (we currently lie and say 60fps), nor the maximum resolution of the camera.
// !!! FIXME: I think VideoOutput wants the system to control the whole pipeline, from lens to encoder
// !!! FIXME:  to disk, and preview output wants to add the GPU into the mix but will accept CPU access
// !!! FIXME:  if you want to do things like Snapchat filters or whatever on the data. I don't know.

// prevent unneeded rawfile headers (which contain C++ code) from getting included by other system headers.
typedef struct RawFileDescriptor RawFileDescriptor;
#define GLOBAL_RAW_FILE_H 1

#include <multimedia/image_framework/image/image_native.h>
#include <multimedia/image_framework/image/image_receiver_native.h>
#include <ohcamera/camera.h>
#include <ohcamera/camera_device.h>
#include <ohcamera/camera_input.h>
#include <ohcamera/capture_session.h>
#include <ohcamera/video_output.h>
#include <ohcamera/camera_manager.h>

#include "../../core/openharmony/SDL_openharmony.h"

struct SDL_PrivateCameraData
{
    Camera_Input *input;
    OH_ImageReceiverOptions *options;
    OH_ImageReceiverNative *image_receiver;
    Camera_PreviewOutput *output;
    //Camera_VideoOutput *output;
    Camera_CaptureSession *session;
    int rotation;  // degrees to rotate clockwise to get from camera's static orientation to device's native orientation. Apply this plus current phone rotation to get upright image!
    SDL_CameraSpec requested_spec;
};

static bool SetErrorStr(const char *what, const char *errstr, const int rc)
{
    char errbuf[128];
    if (!errstr) {
        SDL_snprintf(errbuf, sizeof (errbuf), "Unknown error #%d", rc);
        errstr = errbuf;
    }
    return SDL_SetError("%s: %s", what, errstr);
}

static const char *CameraErrorStr(const Camera_ErrorCode rc)
{
    switch (rc) {
        case CAMERA_OK: return "no error";
        case CAMERA_INVALID_ARGUMENT: return "invalid argument";
        case CAMERA_OPERATION_NOT_ALLOWED: return "operation not allowed";
        case CAMERA_SESSION_NOT_CONFIG: return "session not configured";
        case CAMERA_SESSION_NOT_RUNNING: return "session not running";
        case CAMERA_SESSION_CONFIG_LOCKED: return "session config locked";
        case CAMERA_DEVICE_SETTING_LOCKED: return "device setting locked";
        case CAMERA_CONFLICT_CAMERA: return "camera conflict";
        case CAMERA_DEVICE_DISABLED: return "device disabled";
        case CAMERA_DEVICE_PREEMPTED: return "device preempted";
        case CAMERA_UNRESOLVED_CONFLICTS_WITH_CURRENT_CONFIGURATIONS: return "unresolved conflicts with current configuration";
        case CAMERA_SERVICE_FATAL_ERROR: return "fatal error";
        default: break;
    }

    return NULL;  // unknown error
}

static bool SetCameraError(const char *what, const Camera_ErrorCode rc)
{
    return SetErrorStr(what, CameraErrorStr(rc), (int) rc);
}

static const char *ImageErrorStr(const Image_ErrorCode rc)
{
    switch (rc) {
        case IMAGE_SUCCESS: return "no error";
        case IMAGE_BAD_PARAMETER: return "bad parameter";
        case IMAGE_UNSUPPORTED_MIME_TYPE: return "unsupported mimetype";
        case IMAGE_UNKNOWN_MIME_TYPE: return "unknown mimetype";
        case IMAGE_TOO_LARGE: return "too large";
        case IMAGE_GET_IMAGE_DATA_FAILED: return "get image data failed";
        case IMAGE_DMA_NOT_EXIST: return "DMA doesn't exist";
        case IMAGE_DMA_OPERATION_FAILED: return "DMA operation failed";
        case IMAGE_UNSUPPORTED_OPERATION: return "unsupported operation";
        case IMAGE_UNSUPPORTED_METADATA: return "unsupported metadata";
        case IMAGE_UNSUPPORTED_CONVERSION: return "unsupported conversion";
        case IMAGE_INVALID_REGION: return "invalid region";
        case IMAGE_UNSUPPORTED_MEMORY_FORMAT: return "unsupported memory format";
        case IMAGE_INVALID_PARAMETER: return "invalid parameter";
        case IMAGE_UNSUPPORTED_DATA_FORMAT: return "unsupported data format";
        case IMAGE_ALLOC_FAILED: return "allocation failed";
        case IMAGE_COPY_FAILED: return "copy failed";
        case IMAGE_LOCK_UNLOCK_FAILED: return "unlock failed";
        case IMAGE_INIT_FAILED: return "init failed";
        case IMAGE_CREATE_PIXELMAP_FAILED: return "create pixelmap failed";
        case IMAGE_ALLOCATOR_MODE_UNSUPPORTED: return "allocator mode unsupported";
        case IMAGE_UNKNOWN_ERROR: return "unknown error";
        case IMAGE_BAD_SOURCE: return "bad source";
        case IMAGE_SOURCE_UNSUPPORTED_MIME_TYPE: return "unsupported source mimetype";
        case IMAGE_SOURCE_TOO_LARGE: return "source too large";
        case IMAGE_SOURCE_UNSUPPORTED_ALLOCATOR_TYPE: return "unsupported source allocator type";
        case IMAGE_SOURCE_UNSUPPORTED_METADATA: return "unsupported source metadata";
        case IMAGE_SOURCE_UNSUPPORTED_OPTIONS: return "unsupported source options";
        case IMAGE_SOURCE_INVALID_PARAMETER: return "invalid source parameter";
        case IMAGE_DECODE_FAILED: return "decode failed";
        case IMAGE_SOURCE_ALLOC_FAILED: return "source allocation failed";
        case IMAGE_PACKER_INVALID_PARAMETER: return "invalid packer parameter";
        case IMAGE_ENCODE_FAILED: return "encode failed";
        case IMAGE_RECEIVER_INVALID_PARAMETER: return "invalid receiver parameter";
        default: break;
    }

    return NULL;  // unknown error
}

static bool SetImageError(const char *what, const Image_ErrorCode rc)
{
    return SetErrorStr(what, ImageErrorStr(rc), (int) rc);
}


static Camera_Manager *camera_manager = NULL;

static bool CreateCameraManager(void)
{
    SDL_assert(camera_manager == NULL);
    const Camera_ErrorCode rc = OH_Camera_GetCameraManager(&camera_manager);
    if (rc != CAMERA_OK) {
        return SetCameraError("Error creating Camera_Manager", rc);
    }
    return true;
}

static void DestroyCameraManager(void)
{
    if (camera_manager) {
        OH_Camera_DeleteCameraManager(camera_manager);
        camera_manager = NULL;
    }
}

static void ConvertOpenHarmonyFormatToSDL(Camera_Format fmt, SDL_PixelFormat *format, SDL_Colorspace *colorspace)
{
    switch (fmt) {
        #define CASE(x, y, z)  case x: *format = y; *colorspace = z; return
        CASE(CAMERA_FORMAT_YUV_420_SP, SDL_PIXELFORMAT_NV21, SDL_COLORSPACE_BT709_LIMITED);
        CASE(CAMERA_FORMAT_JPEG, SDL_PIXELFORMAT_MJPG, SDL_COLORSPACE_SRGB);
        CASE(CAMERA_FORMAT_RGBA_8888, SDL_PIXELFORMAT_RGBA8888, SDL_COLORSPACE_SRGB);
        #undef CASE
        default: break;
    }

    #if DEBUG_CAMERA
    SDL_Log("CAMERA: Unknown format Camera_Format '%d'", fmt);
    #endif

    *format = SDL_PIXELFORMAT_UNKNOWN;
    *colorspace = SDL_COLORSPACE_UNKNOWN;
}

static Camera_Format ConvertSDLFormatToOpenHarmony(SDL_PixelFormat fmt)
{
    switch (fmt) {
        #define CASE(x, y)  case y: return x
        CASE(CAMERA_FORMAT_YUV_420_SP, SDL_PIXELFORMAT_NV21);
        CASE(CAMERA_FORMAT_JPEG, SDL_PIXELFORMAT_MJPG);
        CASE(CAMERA_FORMAT_RGBA_8888, SDL_PIXELFORMAT_RGBA8888);
        #undef CASE
        default: return 0;
    }
}

static bool FindOpenHarmonyCameraByDevicePtr(SDL_Camera *device, void *userdata)
{
    const Camera_Device *ohdevice = (const Camera_Device *) userdata;
    return (SDL_strcmp(ohdevice->cameraId, ((const Camera_Device *) device->handle)->cameraId) == 0);
}

static bool FindOpenHarmonyCameraByImageReceiver(SDL_Camera *device, void *userdata)
{
    OH_ImageReceiverNative *receiver = (OH_ImageReceiverNative *) userdata;
    struct SDL_PrivateCameraData *hidden = device->hidden;
    return (hidden && (hidden->image_receiver == receiver));
}

static bool FindOpenHarmonyCameraByInput(SDL_Camera *device, void *userdata)
{
    Camera_Input *input = (Camera_Input *) userdata;
    struct SDL_PrivateCameraData *hidden = device->hidden;
    return (hidden && (hidden->input == input));
}

static bool OPENHARMONYCAMERA_WaitDevice(SDL_Camera *device)
{
    return true;  // this isn't used atm, since we run our own thread via onImageAvailable callbacks.
}

static SDL_CameraFrameResult OPENHARMONYCAMERA_AcquireFrame(SDL_Camera *device, SDL_Surface *frame, Uint64 *timestampNS, float *rotation)
{
    struct SDL_PrivateCameraData *hidden = device->hidden;
    SDL_CameraFrameResult result = SDL_CAMERA_FRAME_READY;
    Image_ErrorCode imgrc = IMAGE_SUCCESS;

    OH_ImageNative *image = NULL;
    if ((imgrc = OH_ImageReceiverNative_ReadNextImage(hidden->image_receiver, &image)) != IMAGE_SUCCESS) {
        SetImageError("Failed to read next image", imgrc);
        return SDL_CAMERA_FRAME_ERROR;
    }

    Image_Size size;
    if ((imgrc = OH_ImageNative_GetImageSize(image, &size)) != IMAGE_SUCCESS) {
        SetImageError("Failed to get image size", imgrc);
        OH_ImageNative_Release(image);
        return SDL_CAMERA_FRAME_ERROR;
    }

    int64_t ohtimestamp = 0;
    if (OH_ImageNative_GetTimestamp(image, &ohtimestamp) == IMAGE_SUCCESS) {
        *timestampNS = (Uint64) ohtimestamp;
    } else {
        *timestampNS = 0;
    }

    uint32_t components_buf[16];
    uint32_t *components = components_buf;
    size_t num_components = SDL_arraysize(components_buf);
    if ((imgrc = OH_ImageNative_GetComponentTypes(image, &components, &num_components)) != IMAGE_SUCCESS) {
        SetImageError("Failed to get image component types", imgrc);
        OH_ImageNative_Release(image);
        return SDL_CAMERA_FRAME_ERROR;
    }
    SDL_assert(num_components <= SDL_arraysize(components_buf));  // just in case.

    int num_planes = (int) num_components;

#if 0
SDL_Log("CAMERA: %d planes:", (int) num_components);
for (size_t i = 0; i < num_components; i++) { SDL_Log("CAMERA:  - %d: %d", (int) i, (int) components[i]); }

    // !!! FIXME: this currently copies the data to the surface (see FIXME about non-contiguous planar surfaces, but in theory we could just keep this locked until ReleaseFrame...
    if ((num_planes == 3) && (device->spec.format == SDL_PIXELFORMAT_NV21)) {
SDL_Log("THIS IS AN NV21 IMAGE, ALLEGEDLY.");
        num_planes--;   // treat the interleaved planes as one.
    }
#endif

    int32_t stride = 0;
    // !!! FIXME: this works because of luck of how YUV420 and NV21 are layed out, but we _should_ check this for each component.
    if ((imgrc = OH_ImageNative_GetRowStride(image, components[0], &stride)) != IMAGE_SUCCESS) {
        SetImageError("Failed to get image row stride", imgrc);
        OH_ImageNative_Release(image);
        return SDL_CAMERA_FRAME_ERROR;
    }

    frame->pitch = (int) stride;

    size_t buflen = 0;
    for (int i = 0; (i < num_planes) && (i < 3); i++) {
        size_t datalen = 0;
        OH_ImageNative_GetBufferSize(image, components[i], &datalen);
        buflen += datalen;
    }

    frame->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), buflen);
    if (frame->pixels == NULL) {
        result = SDL_CAMERA_FRAME_ERROR;
    } else {
        Uint8 *dst = frame->pixels;

        for (int i = 0; i < num_planes; i++) {
            OH_NativeBuffer *imagebuf = NULL;
            OH_ImageNative_GetByteBuffer(image, components[i], &imagebuf);

            size_t datalen = 0;
            OH_ImageNative_GetBufferSize(image, components[i], &datalen);

            void *data = NULL;
            if (OH_NativeBuffer_Map(imagebuf, &data) != 0) {
                SDL_memset(dst, 0, datalen);  // oh well.
            } else {
                SDL_memcpy(dst, data, datalen);
                OH_NativeBuffer_Unmap(imagebuf);
            }

            dst += datalen;
        }
    }

    OH_ImageNative_Release(image);

    int dev_rotation = 0;
    switch (SDL_GetOpenHarmonyDeviceCurrentOrientation()) {
        case SDL_ORIENTATION_PORTRAIT: dev_rotation = 0; break;
        case SDL_ORIENTATION_LANDSCAPE: dev_rotation = 90; break;
        case SDL_ORIENTATION_PORTRAIT_FLIPPED: dev_rotation = 180; break;
        case SDL_ORIENTATION_LANDSCAPE_FLIPPED: dev_rotation = 270; break;
        default: SDL_assert(!"Unexpected device rotation!"); dev_rotation = 0; break;
    }

    if (device->position == SDL_CAMERA_POSITION_BACK_FACING) {
        dev_rotation = -dev_rotation;  // we want to subtract this value, instead of add, if back-facing.
    }

    *rotation = (float) (dev_rotation + device->hidden->rotation);   // current phone orientation, static camera orientation in relation to phone.

    return result;
}

static void OPENHARMONYCAMERA_ReleaseFrame(SDL_Camera *device, SDL_Surface *frame)
{
    // !!! FIXME: this currently copies the data to the surface, but in theory we could just keep the OH_ImageNative until ReleaseFrame...
    SDL_aligned_free(frame->pixels);
}

static void OnImageReceiverFrame(OH_ImageReceiverNative *receiver)
{
    #if DEBUG_CAMERA
    SDL_Log("CAMERA: CB OnImageReceiverFrame");
    #endif
    SDL_Camera *device = SDL_FindPhysicalCameraByCallback(FindOpenHarmonyCameraByImageReceiver, receiver);
    if (device) {
        SDL_CameraThreadIterate(device);
    }
}

static void OnVideoOutputFrameStart(Camera_PreviewOutput *output)
//static void OnVideoOutputFrameStart(Camera_VideoOutput *output)
{
    #if DEBUG_CAMERA
    SDL_Log("CAMERA: CB OnVideoOutputFrameStart");
    #endif
}

static void OnVideoOutputFrameEnd(Camera_PreviewOutput *output, int32_t frame_count)
//static void OnVideoOutputFrameEnd(Camera_VideoOutput *output, int32_t frame_count)
{
    #if DEBUG_CAMERA
    SDL_Log("CAMERA: CB OnVideoOutputFrameEnd");
    #endif
}

static void OnVideoOutputError(Camera_PreviewOutput *output, Camera_ErrorCode err)
//static void OnVideoOutputError(Camera_VideoOutput *output, Camera_ErrorCode err)
{
    #if DEBUG_CAMERA
    SDL_Log("CAMERA: CB OnVideoOutputError");
    #endif
}

static void OnCameraInputError(const Camera_Input *input, Camera_ErrorCode err)
{
    #if DEBUG_CAMERA
    SDL_Log("CAMERA: CB OnCameraInputError");
    #endif
    SDL_Camera *device = SDL_FindPhysicalCameraByCallback(FindOpenHarmonyCameraByInput, (void *) input);
    if (device) {
        SDL_CameraDisconnected(device);
    }
}

static void OPENHARMONYCAMERA_CloseDevice(SDL_Camera *device)
{
    if (device && device->hidden) {
        struct SDL_PrivateCameraData *hidden = device->hidden;

        if (hidden->session) {
            OH_CaptureSession_Stop(hidden->session);
            OH_CaptureSession_Release(hidden->session);
        }

        if (hidden->input) {
            OH_CameraInput_Close(hidden->input);
            OH_CameraInput_Release(hidden->input);
        }

        if (hidden->output) {
            OH_PreviewOutput_Stop(hidden->output);
            OH_PreviewOutput_Release(hidden->output);
            //OH_VideoOutput_Stop(hidden->output);
            //OH_VideoOutput_Release(hidden->output);
        }

        if (hidden->image_receiver) {
            OH_ImageReceiverNative_Release(hidden->image_receiver);
        }

        if (hidden->options) {
            OH_ImageReceiverOptions_Release(hidden->options);
        }

        device->hidden = NULL;

        SDL_free(hidden);
    }
}

// this is where the "opening" of the camera happens, after permission is granted.
static bool PrepareCamera(SDL_Camera *device)
{
    struct SDL_PrivateCameraData *hidden = device->hidden;
    SDL_assert(hidden != NULL);

    // just in case SDL_OpenCamera is overwriting device->spec as CameraPermissionCallback runs, we work from a different copy.
    const SDL_CameraSpec *spec = &hidden->requested_spec;

    Camera_ErrorCode rc = CAMERA_OK;
    Image_ErrorCode imgrc = IMAGE_SUCCESS;
    Camera_Device *ohdevice = (Camera_Device *) device->handle;

    const Camera_Profile video_profile = { ConvertSDLFormatToOpenHarmony(spec->format), { (uint32_t) spec->width, (uint32_t) spec->height } };
    //const Camera_VideoProfile video_profile = { ConvertSDLFormatToOpenHarmony(spec->format), { (uint32_t) spec->width, (uint32_t) spec->height }, { spec->framerate_numerator, spec->framerate_numerator } };
    const Image_Size imgsize = { (uint32_t) spec->width, (uint32_t) spec->height };
    const int32_t imgcapacity = 8; // image buffer queue size. docs recommend this be 8: https://developer.huawei.com/consumer/en/doc/harmonyos-guides/native-camera-preview-imagereceiver
    uint64_t receiver_surface_id = 0;
    char receiver_surface_id_str[64];
    uint32_t orientation = 0;

    CameraInput_Callbacks camera_input_callbacks = { OnCameraInputError };
    PreviewOutput_Callbacks video_output_callbacks = { OnVideoOutputFrameStart, OnVideoOutputFrameEnd, OnVideoOutputError };
    //VideoOutput_Callbacks video_output_callbacks = { OnVideoOutputFrameStart, OnVideoOutputFrameEnd, OnVideoOutputError };

    if ((imgrc = OH_ImageReceiverOptions_Create(&hidden->options)) != IMAGE_SUCCESS) {
        return SetImageError("Failed to create image receiver options", imgrc);
    } else if ((imgrc = OH_ImageReceiverOptions_SetSize(hidden->options, imgsize)) != IMAGE_SUCCESS) {
        return SetImageError("Failed to set image receiver size option", imgrc);
    } else if ((imgrc = OH_ImageReceiverOptions_SetCapacity(hidden->options, imgcapacity)) != IMAGE_SUCCESS) {
        return SetImageError("Failed to set image receiver capacity option", imgrc);
    } else if ((imgrc = OH_ImageReceiverNative_Create(hidden->options, &hidden->image_receiver)) != IMAGE_SUCCESS) {
        return SetImageError("Failed to create native image receiver", imgrc);
    } else if ((imgrc = OH_ImageReceiverNative_On(hidden->image_receiver, OnImageReceiverFrame)) != IMAGE_SUCCESS) {
        return SetImageError("Failed to set native image receiver callback", imgrc);
    } else if ((imgrc = OH_ImageReceiverNative_GetReceivingSurfaceId(hidden->image_receiver, &receiver_surface_id)) != IMAGE_SUCCESS) {
        return SetImageError("Failed to create native image receiver", imgrc);
    } else if (SDL_snprintf(receiver_surface_id_str, sizeof (receiver_surface_id_str), "%" SDL_PRIu64, receiver_surface_id) >= sizeof (receiver_surface_id_str)) {
        return SDL_OutOfMemory();  // shouldn't happen.
    } else if ((rc = OH_CameraDevice_GetCameraOrientation(ohdevice, &orientation)) != CAMERA_OK) {
        return SetCameraError("Failed to determine camera orientation", rc);
    } else if ((rc = OH_CameraManager_CreatePreviewOutput(camera_manager, &video_profile, receiver_surface_id_str, &hidden->output)) != CAMERA_OK) {
    //} else if ((rc = OH_CameraManager_CreateVideoOutput(camera_manager, &video_profile, receiver_surface_id_str, &hidden->output)) != CAMERA_OK) {
        return SetCameraError("Failed to create video output", rc);
    } else if ((rc = OH_PreviewOutput_RegisterCallback(hidden->output, &video_output_callbacks)) != CAMERA_OK) {
    //} else if ((rc = OH_VideoOutput_RegisterCallback(hidden->output, &video_output_callbacks)) != CAMERA_OK) {
        return SetCameraError("Failed to set video output callbacks", rc);
    } else if ((rc = OH_CameraManager_CreateCameraInput(camera_manager, ohdevice, &hidden->input)) != CAMERA_OK) {
        return SetCameraError("Failed to create camera input", rc);
    } else if ((rc = OH_CameraInput_RegisterCallback(hidden->input, &camera_input_callbacks)) != CAMERA_OK) {
        return SetCameraError("Failed to register camera input callbacks", rc);
    } else if ((rc = OH_CameraInput_Open(hidden->input)) != CAMERA_OK) {
        return SetCameraError("Failed to open camera input", rc);
    } else if ((rc = OH_CameraManager_CreateCaptureSession(camera_manager, &hidden->session)) != CAMERA_OK) {
        return SetCameraError("Failed to create camera capture session", rc);
    } else if ((rc = OH_CaptureSession_SetSessionMode(hidden->session, NORMAL_VIDEO)) != CAMERA_OK) {
        return SetCameraError("Failed to set camera capture session mode", rc);
    } else if ((rc = OH_CaptureSession_BeginConfig(hidden->session)) != CAMERA_OK) {
        return SetCameraError("Failed to begin camera capture session config", rc);
    // !!! FIXME: maybe? } else if ((rc = OH_CaptureSession_SetActiveColorSpace(hidden->session, colorspace)) != CAMERA_OK) {
    // !!! FIXME: maybe?     return SetCameraError("Failed to set active colorspace in session config", rc);
    } else if ((rc = OH_CaptureSession_AddInput(hidden->session, hidden->input)) != CAMERA_OK) {
        return SetCameraError("Failed to add device input to camera capture session", rc);
    } else if ((rc = OH_CaptureSession_AddPreviewOutput(hidden->session, hidden->output)) != CAMERA_OK) {
    //} else if ((rc = OH_CaptureSession_AddVideoOutput(hidden->session, hidden->output)) != CAMERA_OK) {
        return SetCameraError("Failed to add video output to camera capture session", rc);
    } else if ((rc = OH_CaptureSession_CommitConfig(hidden->session)) != CAMERA_OK) {
        return SetCameraError("Failed to commit camera capture session config", rc);
    } else if ((rc = OH_CaptureSession_Start(hidden->session)) != CAMERA_OK) {
        return SetCameraError("Failed to start camera capture session", rc);
    //} else if ((rc = OH_VideoOutput_Start(hidden->output)) != CAMERA_OK) {
    //    return SetCameraError("Failed to start video output", rc);
    }

    // we don't need this, it's only in `hidden` so it'll clean up if we fail halfway through this.
    OH_ImageReceiverOptions_Release(hidden->options);
    hidden->options = NULL;

    hidden->rotation = (int) (orientation % 360);

    return true;
}

static void SDLCALL CameraPermissionCallback(void *userdata, const char *permission, bool granted)
{
    SDL_Camera *device = (SDL_Camera *) userdata;
    if (device->hidden != NULL) {   // if device was already closed, don't send an event.
        if (!granted) {
            SDL_CameraPermissionOutcome(device, false);  // sorry, permission denied.
        } else if (!PrepareCamera(device)) {  // permission given? Actually open the camera now.
            // uhoh, setup failed; since the app thinks we already "opened" the device, mark it as disconnected and don't report the permission.
            SDL_CameraDisconnected(device);
        } else {
            // okay! We have permission to use the camera _and_ opening the hardware worked out, report that the camera is usable!
            SDL_CameraPermissionOutcome(device, true);  // go go go!
        }
    }

    UnrefPhysicalCamera(device);   // we ref'd this in OpenDevice, release the extra reference.
}


static bool OPENHARMONYCAMERA_OpenDevice(SDL_Camera *device, const SDL_CameraSpec *spec)
{
#if 0
    // !!! FIXME: Android requires all concurrent cameras to be opened before assigning them all to a session.
    // !!! FIXME: OpenHarmony, in API level 18, added OH_CameraInput_OpenConcurrentCameras() and some other support APIs,
    // !!! FIXME: which is probably the way to handle this, but we're adopting SDL's current Android attitude for now: if it fails, okay.
    if (CheckDevicePlaying()) {
        return SDL_SetError("A camera is already playing");
    }
#endif

    if (spec->framerate_denominator != 1) {
        return SDL_SetError("Invalid framerate");  // we force these to fps/1
    }

    device->hidden = (struct SDL_PrivateCameraData *) SDL_calloc(1, sizeof (struct SDL_PrivateCameraData));
    if (device->hidden == NULL) {
        return false;
    }

    RefPhysicalCamera(device);  // ref'd until permission callback fires.

    // just in case SDL_OpenCamera is overwriting device->spec as CameraPermissionCallback runs, we work from a different copy.
    SDL_copyp(&device->hidden->requested_spec, spec);
    if (!SDL_RequestOpenHarmonyPermission("ohos.permission.CAMERA", CameraPermissionCallback, device)) {
        UnrefPhysicalCamera(device);
        return false;
    }

    return true;  // we don't open the camera until permission is granted, so always succeed for now.
}

static void OPENHARMONYCAMERA_FreeDeviceHandle(SDL_Camera *device)
{
    if (device) {
        Camera_Device *ohdevice = (Camera_Device *) device->handle;
        SDL_free(ohdevice->cameraId);
        SDL_free(ohdevice);
    }
}

static void GatherCameraSpecs(Camera_Device *ohdevice, CameraFormatAddData *add_data, char **fullname, SDL_CameraPosition *position)
{
    Camera_ErrorCode rc = CAMERA_OK;
    SDL_zerop(add_data);

    *fullname = NULL;  // !!! FIXME: there doesn't appear to be a "get device's name" API for obtaining something like "Logitech WebCam".

    if (ohdevice->cameraPosition == CAMERA_POSITION_FRONT) {
        *position = SDL_CAMERA_POSITION_FRONT_FACING;
        if (!*fullname) {
            *fullname = SDL_strdup("Front-facing camera");
        }
    } else if (ohdevice->cameraPosition == CAMERA_POSITION_BACK) {
        *position = SDL_CAMERA_POSITION_BACK_FACING;
        if (!*fullname) {
            *fullname = SDL_strdup("Back-facing camera");
        }
    } else {
        *position = SDL_CAMERA_POSITION_UNKNOWN;
    }

    if (!*fullname) {
        *fullname = SDL_strdup("Generic camera");   // we tried.
    }

    Camera_OutputCapability *output_caps = NULL;
    rc = OH_CameraManager_GetSupportedCameraOutputCapabilityWithSceneMode(camera_manager, ohdevice, NORMAL_VIDEO, &output_caps);
    if ((rc != CAMERA_OK) || !output_caps) {
        return;
    }

    const uint32_t total = output_caps->previewProfilesSize;
    //const uint32_t total = output_caps->videoProfilesSize;
    for (uint32_t i = 0; i < total; i++) {
        const Camera_Profile *vidprof = output_caps->previewProfiles[i];
        //const Camera_VideoProfile *vidprof = output_caps->videoProfiles[i];
        SDL_PixelFormat device_format = SDL_PIXELFORMAT_UNKNOWN;
        SDL_Colorspace device_colorspace = SDL_COLORSPACE_UNKNOWN;
        const int w = (int) vidprof->size.width;
        const int h = (int) vidprof->size.height;
        if ((w <= 0) || (h <= 0)) {
            continue;
        } else {
            ConvertOpenHarmonyFormatToSDL(vidprof->format, &device_format, &device_colorspace);
            if (device_format == SDL_PIXELFORMAT_UNKNOWN) {
                continue;
            }
        }

        SDL_AddCameraFormat(add_data, device_format, device_colorspace, w, h, 60, 1);
        //SDL_AddCameraFormat(add_data, device_format, device_colorspace, w, h, vidprof->range.min, 1);
        //if (vidprof->range.min != vidprof->range.max) {
        //    SDL_AddCameraFormat(add_data, device_format, device_colorspace, w, h, vidprof->range.max, 1);
        //}
    }

    OH_CameraManager_DeleteSupportedCameraOutputCapability(camera_manager, output_caps);
}

static void MaybeAddDevice(Camera_Device *ohdevice)
{
    const char *devid = ohdevice->cameraId;
    Camera_ErrorCode rc;

    #if DEBUG_CAMERA
    SDL_Log("CAMERA: MaybeAddDevice('%s')", devid);
    #endif

    if (SDL_FindPhysicalCameraByCallback(FindOpenHarmonyCameraByDevicePtr, ohdevice)) {
        return;  // already have this one.
    }

    Camera_SceneMode *scene_modes = NULL;
    uint32_t num_scene_modes = 0;
    rc = OH_CameraManager_GetSupportedSceneModes(ohdevice, &scene_modes, &num_scene_modes);
    if ((rc != CAMERA_OK) || !scene_modes) {
        return;
    }

    bool okay = false;
    for (uint32_t i = 0; i < num_scene_modes; i++) {
        if (scene_modes[i] == NORMAL_VIDEO) {
            okay = true;
            break;
        }
    }

    OH_CameraManager_DeleteSceneModes(camera_manager, scene_modes);

    if (!okay) {
        return;  // uh...weird camera...?
    }

    SDL_CameraPosition position = SDL_CAMERA_POSITION_UNKNOWN;
    char *fullname = NULL;
    CameraFormatAddData add_data;
    GatherCameraSpecs(ohdevice, &add_data, &fullname, &position);
    if (add_data.num_specs > 0) {
        SDL_Camera *device = NULL;
        Camera_Device *devcpy = (Camera_Device *) SDL_malloc(sizeof (*devcpy));
        if (devcpy) {
            devcpy->cameraId = SDL_strdup(devid);
            if (devcpy->cameraId) {
                device = SDL_AddCamera(fullname, position, add_data.num_specs, add_data.specs, devcpy);
            }
        }

        if (!device) {
            if (devcpy) {
                SDL_free(devcpy->cameraId);
                SDL_free(devcpy);
            }
        }
    }

    SDL_free(fullname);
    SDL_free(add_data.specs);
}

// note that camera "availability" covers both hotplugging and whether another
//  has the device opened, but for something like OpenHarmony, it's probably fine
//  to treat both unplugging and loss of access as disconnection events. When
//  the other app closes the camera, we get an available event as if it was
//  just plugged back in.

static void OnCameraDeviceStatus(Camera_Manager *cameraManager, Camera_StatusInfo *status)
{
    #if DEBUG_CAMERA
    SDL_Log("CAMERA: CB OnCameraDeviceStatus('%s', %d)", status->camera->cameraId, (int) status->status);
    #endif
    SDL_assert(status != NULL);
    SDL_assert(status->camera != NULL);
    SDL_assert(status->camera->cameraId != NULL);
    if (status->status == CAMERA_STATUS_AVAILABLE) {
        MaybeAddDevice(status->camera);
    } else if (status->status == CAMERA_STATUS_UNAVAILABLE) {
        // !!! FIXME: this comment is from Android, is this true here, too?
        // THIS CALLBACK FIRES WHEN YOU OPEN THE DEVICE YOURSELF.  :(
        // Make sure we don't have the device opened, in which case a different callback will fire instead if actually lost.
        SDL_Camera *device = SDL_FindPhysicalCameraByCallback(FindOpenHarmonyCameraByDevicePtr, status->camera);
        if (device && !device->hidden) {
            SDL_CameraDisconnected(device);
        }
    }
}


static CameraManager_Callbacks camera_availability_listener = { OnCameraDeviceStatus };

static void OPENHARMONYCAMERA_DetectDevices(void)
{
    Camera_ErrorCode rc = CAMERA_OK;
    Camera_Device *cameras = NULL;
    uint32_t total = 0;
    rc = OH_CameraManager_GetSupportedCameras(camera_manager, &cameras, &total);

    if ((rc == CAMERA_OK) && (total > 0))  {  // if this fails, maybe the callback catches things later.
        for (uint32_t i = 0; i < total; i++) {
            MaybeAddDevice(&cameras[i]);
        }
        OH_CameraManager_DeleteSupportedCameras(camera_manager, cameras, total);
    }

    OH_CameraManager_RegisterCallback(camera_manager, &camera_availability_listener);
}

static void OPENHARMONYCAMERA_Deinitialize(void)
{
    OH_CameraManager_UnregisterCallback(camera_manager, &camera_availability_listener);
    DestroyCameraManager();
}

static bool OPENHARMONYCAMERA_Init(SDL_CameraDriverImpl *impl)
{
    if (!CreateCameraManager()) {
        return false;
    }

    impl->DetectDevices = OPENHARMONYCAMERA_DetectDevices;
    impl->OpenDevice = OPENHARMONYCAMERA_OpenDevice;
    impl->CloseDevice = OPENHARMONYCAMERA_CloseDevice;
    impl->WaitDevice = OPENHARMONYCAMERA_WaitDevice;
    impl->AcquireFrame = OPENHARMONYCAMERA_AcquireFrame;
    impl->ReleaseFrame = OPENHARMONYCAMERA_ReleaseFrame;
    impl->FreeDeviceHandle = OPENHARMONYCAMERA_FreeDeviceHandle;
    impl->Deinitialize = OPENHARMONYCAMERA_Deinitialize;

    impl->ProvidesOwnCallbackThread = true;

    return true;
}

CameraBootStrap OPENHARMONYCAMERA_bootstrap = {
    "ohcamera", "SDL OpenHarmony camera driver", OPENHARMONYCAMERA_Init, false
};

#endif
