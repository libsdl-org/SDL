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

/**
 *  \file SDL_camera.h
 *
 *  Video Capture for the SDL library.
 */

#ifndef SDL_camera_h_
#define SDL_camera_h_

#include "SDL3/SDL_video.h"

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is a unique ID for a camera device for the time it is connected to the system,
 * and is never reused for the lifetime of the application. If the device is
 * disconnected and reconnected, it will get a new ID.
 *
 * The ID value starts at 1 and increments from there. The value 0 is an invalid ID.
 *
 * \sa SDL_GetCameraDevices
 */
typedef Uint32 SDL_CameraDeviceID;


/**
 * The structure used to identify an SDL camera device
 */
struct SDL_CameraDevice;
typedef struct SDL_CameraDevice SDL_CameraDevice;

#define SDL_CAMERA_ALLOW_ANY_CHANGE          1

/**
 *  SDL_CameraSpec structure
 *
 *  Only those field can be 'desired' when configuring the device:
 *  - format
 *  - width
 *  - height
 *
 *  \sa SDL_GetCameraFormat
 *  \sa SDL_GetCameraFrameSize
 *
 */
typedef struct SDL_CameraSpec
{
    Uint32 format;          /**< Frame SDL_PixelFormatEnum format */
    int width;              /**< Frame width */
    int height;             /**< Frame height */
} SDL_CameraSpec;

/**
 *  SDL Camera Status
 *
 *  Change states but calling the function in this order:
 *
 *  SDL_OpenCamera()
 *  SDL_SetCameraSpec()  -> Init
 *  SDL_StartCamera()    -> Playing
 *  SDL_StopCamera()     -> Stopped
 *  SDL_CloseCamera()
 *
 */
typedef enum
{
    SDL_CAMERA_FAIL = -1,    /**< Failed */
    SDL_CAMERA_INIT = 0,     /**< Init, spec hasn't been set */
    SDL_CAMERA_STOPPED,      /**< Stopped */
    SDL_CAMERA_PLAYING       /**< Playing */
} SDL_CameraStatus;

/**
 *  SDL Video Capture Status
 */
typedef struct SDL_CameraFrame
{
    Uint64 timestampNS;         /**< Frame timestamp in nanoseconds when read from the driver */
    int num_planes;             /**< Number of planes */
    Uint8 *data[3];             /**< Pointer to data of i-th plane */
    int pitch[3];               /**< Pitch of i-th plane */
    void *internal;             /**< Private field */
} SDL_CameraFrame;


/**
 * Use this function to get the number of built-in camera drivers.
 *
 * This function returns a hardcoded number. This never returns a negative
 * value; if there are no drivers compiled into this build of SDL, this
 * function returns zero. The presence of a driver in this list does not mean
 * it will function, it just means SDL is capable of interacting with that
 * interface. For example, a build of SDL might have v4l2 support, but if
 * there's no kernel support available, SDL's v4l2 driver would fail if used.
 *
 * By default, SDL tries all drivers, in its preferred order, until one is
 * found to be usable.
 *
 * \returns the number of built-in camera drivers.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetCameraDriver
 */
extern DECLSPEC int SDLCALL SDL_GetNumCameraDrivers(void);

/**
 * Use this function to get the name of a built in camera driver.
 *
 * The list of camera drivers is given in the order that they are normally
 * initialized by default; the drivers that seem more reasonable to choose
 * first (as far as the SDL developers believe) are earlier in the list.
 *
 * The names of drivers are all simple, low-ASCII identifiers, like "v4l2",
 * "coremedia" or "android". These never have Unicode characters, and are not
 * meant to be proper names.
 *
 * \param index the index of the camera driver; the value ranges from 0 to
 *              SDL_GetNumCameraDrivers() - 1
 * \returns the name of the camera driver at the requested index, or NULL if an
 *          invalid index was specified.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumCameraDrivers
 */
extern DECLSPEC const char *SDLCALL SDL_GetCameraDriver(int index);

/**
 * Get the name of the current camera driver.
 *
 * The returned string points to internal static memory and thus never becomes
 * invalid, even if you quit the camera subsystem and initialize a new driver
 * (although such a case would return a different static string from another
 * call to this function, of course). As such, you should not modify or free
 * the returned string.
 *
 * \returns the name of the current camera driver or NULL if no driver has been
 *          initialized.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC const char *SDLCALL SDL_GetCurrentCameraDriver(void);

/**
 * Get a list of currently connected camera devices.
 *
 * \param count a pointer filled in with the number of camera devices
 * \returns a 0 terminated array of camera instance IDs which should be
 *          freed with SDL_free(), or NULL on error; call SDL_GetError() for
 *          more details.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenCamera
 */
extern DECLSPEC SDL_CameraDeviceID *SDLCALL SDL_GetCameraDevices(int *count);

/**
 * Open a Video Capture device
 *
 * \param instance_id the camera device instance ID
 * \returns device, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetCameraDeviceName
 * \sa SDL_GetCameraDevices
 * \sa SDL_OpenCameraWithSpec
 */
extern DECLSPEC SDL_CameraDevice *SDLCALL SDL_OpenCamera(SDL_CameraDeviceID instance_id);

/**
 * Set specification
 *
 * \param device opened camera device
 * \param desired desired camera spec
 * \param obtained obtained camera spec
 * \param allowed_changes allow changes or not
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenCamera
 * \sa SDL_OpenCameraWithSpec
 * \sa SDL_GetCameraSpec
 */
extern DECLSPEC int SDLCALL SDL_SetCameraSpec(SDL_CameraDevice *device,
                                                    const SDL_CameraSpec *desired,
                                                    SDL_CameraSpec *obtained,
                                                    int allowed_changes);

/**
 * Open a Video Capture device and set specification
 *
 * \param instance_id the camera device instance ID
 * \param desired desired camera spec
 * \param obtained obtained camera spec
 * \param allowed_changes allow changes or not
 * \returns device, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenCamera
 * \sa SDL_SetCameraSpec
 * \sa SDL_GetCameraSpec
 */
extern DECLSPEC SDL_CameraDevice *SDLCALL SDL_OpenCameraWithSpec(SDL_CameraDeviceID instance_id,
                                                                              const SDL_CameraSpec *desired,
                                                                              SDL_CameraSpec *obtained,
                                                                              int allowed_changes);

/**
 * Get device name
 *
 * \param instance_id the camera device instance ID
 * \returns device name, shouldn't be freed
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetCameraDevices
 */
extern DECLSPEC const char * SDLCALL SDL_GetCameraDeviceName(SDL_CameraDeviceID instance_id);

/**
 * Get the obtained camera spec
 *
 * \param device opened camera device
 * \param spec The SDL_CameraSpec to be initialized by this function.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetCameraSpec
 * \sa SDL_OpenCameraWithSpec
 */
extern DECLSPEC int SDLCALL SDL_GetCameraSpec(SDL_CameraDevice *device, SDL_CameraSpec *spec);


/**
 * Get frame format of camera device.
 *
 * The value can be used to fill SDL_CameraSpec structure.
 *
 * \param device opened camera device
 * \param index format between 0 and num -1
 * \param format pointer output format (SDL_PixelFormatEnum)
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumCameraFormats
 */
extern DECLSPEC int SDLCALL SDL_GetCameraFormat(SDL_CameraDevice *device,
                                                      int index,
                                                      Uint32 *format);

/**
 * Number of available formats for the device
 *
 * \param device opened camera device
 * \returns number of formats or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetCameraFormat
 * \sa SDL_SetCameraSpec
 */
extern DECLSPEC int SDLCALL SDL_GetNumCameraFormats(SDL_CameraDevice *device);

/**
 * Get frame sizes of the device and the specified input format.
 *
 * The value can be used to fill SDL_CameraSpec structure.
 *
 * \param device opened camera device
 * \param format a format that can be used by the device (SDL_PixelFormatEnum)
 * \param index framesize between 0 and num -1
 * \param width output width
 * \param height output height
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumCameraFrameSizes
 */
extern DECLSPEC int SDLCALL SDL_GetCameraFrameSize(SDL_CameraDevice *device, Uint32 format, int index, int *width, int *height);

/**
 * Number of different framesizes available for the device and pixel format.
 *
 * \param device opened camera device
 * \param format frame pixel format (SDL_PixelFormatEnum)
 * \returns number of framesizes or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetCameraFrameSize
 * \sa SDL_SetCameraSpec
 */
extern DECLSPEC int SDLCALL SDL_GetNumCameraFrameSizes(SDL_CameraDevice *device, Uint32 format);


/**
 * Get camera status
 *
 * \param device opened camera device
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CameraStatus
 */
extern DECLSPEC SDL_CameraStatus SDLCALL SDL_GetCameraStatus(SDL_CameraDevice *device);

/**
 * Start camera
 *
 * \param device opened camera device
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StopCamera
 */
extern DECLSPEC int SDLCALL SDL_StartCamera(SDL_CameraDevice *device);

/**
 * Acquire a frame.
 *
 * The frame is a memory pointer to the image data, whose size and format are
 * given by the the obtained spec.
 *
 * Non blocking API. If there is a frame available, frame->num_planes is non
 * 0. If frame->num_planes is 0 and returned code is 0, there is no frame at
 * that time.
 *
 * After used, the frame should be released with SDL_ReleaseCameraFrame
 *
 * \param device opened camera device
 * \param frame pointer to get the frame
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ReleaseCameraFrame
 */
extern DECLSPEC int SDLCALL SDL_AcquireCameraFrame(SDL_CameraDevice *device, SDL_CameraFrame *frame);

/**
 * Release a frame.
 *
 * Let the back-end re-use the internal buffer for camera.
 *
 * All acquired frames should be released before closing the device.
 *
 * \param device opened camera device
 * \param frame frame pointer.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireCameraFrame
 */
extern DECLSPEC int SDLCALL SDL_ReleaseCameraFrame(SDL_CameraDevice *device, SDL_CameraFrame *frame);

/**
 * Stop Video Capture
 *
 * \param device opened camera device
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartCamera
 */
extern DECLSPEC int SDLCALL SDL_StopCamera(SDL_CameraDevice *device);

/**
 * Use this function to shut down camera processing and close the
 * camera device.
 *
 * \param device opened camera device
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenCameraWithSpec
 * \sa SDL_OpenCamera
 */
extern DECLSPEC void SDLCALL SDL_CloseCamera(SDL_CameraDevice *device);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_camera_h_ */
