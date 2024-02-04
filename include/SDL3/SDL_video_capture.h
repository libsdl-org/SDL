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
 *  \file SDL_video_capture.h
 *
 *  Video Capture for the SDL library.
 */

#ifndef SDL_video_capture_h_
#define SDL_video_capture_h_

#include "SDL3/SDL_video.h"

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is a unique ID for a video capture device for the time it is connected to the system,
 * and is never reused for the lifetime of the application. If the device is
 * disconnected and reconnected, it will get a new ID.
 *
 * The ID value starts at 1 and increments from there. The value 0 is an invalid ID.
 *
 * \sa SDL_GetVideoCaptureDevices
 */
typedef Uint32 SDL_VideoCaptureDeviceID;


/**
 * The structure used to identify an SDL video capture device
 */
struct SDL_VideoCaptureDevice;
typedef struct SDL_VideoCaptureDevice SDL_VideoCaptureDevice;

#define SDL_VIDEO_CAPTURE_ALLOW_ANY_CHANGE          1

/**
 *  SDL_VideoCaptureSpec structure
 *
 *  Only those field can be 'desired' when configuring the device:
 *  - format
 *  - width
 *  - height
 *
 *  \sa SDL_GetVideoCaptureFormat
 *  \sa SDL_GetVideoCaptureFrameSize
 *
 */
typedef struct SDL_VideoCaptureSpec
{
    Uint32 format;          /**< Frame SDL_PixelFormatEnum format */
    int width;              /**< Frame width */
    int height;             /**< Frame height */
} SDL_VideoCaptureSpec;

/**
 *  SDL Video Capture Status
 *
 *  Change states but calling the function in this order:
 *
 *  SDL_OpenVideoCapture()
 *  SDL_SetVideoCaptureSpec()  -> Init
 *  SDL_StartVideoCapture()    -> Playing
 *  SDL_StopVideoCapture()     -> Stopped
 *  SDL_CloseVideoCapture()
 *
 */
typedef enum
{
    SDL_VIDEO_CAPTURE_FAIL = -1,    /**< Failed */
    SDL_VIDEO_CAPTURE_INIT = 0,     /**< Init, spec hasn't been set */
    SDL_VIDEO_CAPTURE_STOPPED,      /**< Stopped */
    SDL_VIDEO_CAPTURE_PLAYING       /**< Playing */
} SDL_VideoCaptureStatus;

/**
 *  SDL Video Capture Status
 */
typedef struct SDL_VideoCaptureFrame
{
    Uint64 timestampNS;         /**< Frame timestamp in nanoseconds when read from the driver */
    int num_planes;             /**< Number of planes */
    Uint8 *data[3];             /**< Pointer to data of i-th plane */
    int pitch[3];               /**< Pitch of i-th plane */
    void *internal;             /**< Private field */
} SDL_VideoCaptureFrame;


/**
 * Get a list of currently connected video capture devices.
 *
 * \param count a pointer filled in with the number of video capture devices
 * \returns a 0 terminated array of video capture instance IDs which should be
 *          freed with SDL_free(), or NULL on error; call SDL_GetError() for
 *          more details.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenVideoCapture
 */
extern DECLSPEC SDL_VideoCaptureDeviceID *SDLCALL SDL_GetVideoCaptureDevices(int *count);

/**
 * Open a Video Capture device
 *
 * \param instance_id the video capture device instance ID
 * \returns device, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetVideoCaptureDeviceName
 * \sa SDL_GetVideoCaptureDevices
 * \sa SDL_OpenVideoCaptureWithSpec
 */
extern DECLSPEC SDL_VideoCaptureDevice *SDLCALL SDL_OpenVideoCapture(SDL_VideoCaptureDeviceID instance_id);

/**
 * Set specification
 *
 * \param device opened video capture device
 * \param desired desired video capture spec
 * \param obtained obtained video capture spec
 * \param allowed_changes allow changes or not
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenVideoCapture
 * \sa SDL_OpenVideoCaptureWithSpec
 * \sa SDL_GetVideoCaptureSpec
 */
extern DECLSPEC int SDLCALL SDL_SetVideoCaptureSpec(SDL_VideoCaptureDevice *device,
                                                    const SDL_VideoCaptureSpec *desired,
                                                    SDL_VideoCaptureSpec *obtained,
                                                    int allowed_changes);

/**
 * Open a Video Capture device and set specification
 *
 * \param instance_id the video capture device instance ID
 * \param desired desired video capture spec
 * \param obtained obtained video capture spec
 * \param allowed_changes allow changes or not
 * \returns device, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenVideoCapture
 * \sa SDL_SetVideoCaptureSpec
 * \sa SDL_GetVideoCaptureSpec
 */
extern DECLSPEC SDL_VideoCaptureDevice *SDLCALL SDL_OpenVideoCaptureWithSpec(SDL_VideoCaptureDeviceID instance_id,
                                                                              const SDL_VideoCaptureSpec *desired,
                                                                              SDL_VideoCaptureSpec *obtained,
                                                                              int allowed_changes);

/**
 * Get device name
 *
 * \param instance_id the video capture device instance ID
 * \returns device name, shouldn't be freed
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetVideoCaptureDevices
 */
extern DECLSPEC const char * SDLCALL SDL_GetVideoCaptureDeviceName(SDL_VideoCaptureDeviceID instance_id);

/**
 * Get the obtained video capture spec
 *
 * \param device opened video capture device
 * \param spec The SDL_VideoCaptureSpec to be initialized by this function.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetVideoCaptureSpec
 * \sa SDL_OpenVideoCaptureWithSpec
 */
extern DECLSPEC int SDLCALL SDL_GetVideoCaptureSpec(SDL_VideoCaptureDevice *device, SDL_VideoCaptureSpec *spec);


/**
 * Get frame format of video capture device.
 *
 * The value can be used to fill SDL_VideoCaptureSpec structure.
 *
 * \param device opened video capture device
 * \param index format between 0 and num -1
 * \param format pointer output format (SDL_PixelFormatEnum)
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumVideoCaptureFormats
 */
extern DECLSPEC int SDLCALL SDL_GetVideoCaptureFormat(SDL_VideoCaptureDevice *device,
                                                      int index,
                                                      Uint32 *format);

/**
 * Number of available formats for the device
 *
 * \param device opened video capture device
 * \returns number of formats or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetVideoCaptureFormat
 * \sa SDL_SetVideoCaptureSpec
 */
extern DECLSPEC int SDLCALL SDL_GetNumVideoCaptureFormats(SDL_VideoCaptureDevice *device);

/**
 * Get frame sizes of the device and the specified input format.
 *
 * The value can be used to fill SDL_VideoCaptureSpec structure.
 *
 * \param device opened video capture device
 * \param format a format that can be used by the device (SDL_PixelFormatEnum)
 * \param index framesize between 0 and num -1
 * \param width output width
 * \param height output height
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumVideoCaptureFrameSizes
 */
extern DECLSPEC int SDLCALL SDL_GetVideoCaptureFrameSize(SDL_VideoCaptureDevice *device, Uint32 format, int index, int *width, int *height);

/**
 * Number of different framesizes available for the device and pixel format.
 *
 * \param device opened video capture device
 * \param format frame pixel format (SDL_PixelFormatEnum)
 * \returns number of framesizes or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetVideoCaptureFrameSize
 * \sa SDL_SetVideoCaptureSpec
 */
extern DECLSPEC int SDLCALL SDL_GetNumVideoCaptureFrameSizes(SDL_VideoCaptureDevice *device, Uint32 format);


/**
 * Get video capture status
 *
 * \param device opened video capture device
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_VideoCaptureStatus
 */
extern DECLSPEC SDL_VideoCaptureStatus SDLCALL SDL_GetVideoCaptureStatus(SDL_VideoCaptureDevice *device);

/**
 * Start video capture
 *
 * \param device opened video capture device
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StopVideoCapture
 */
extern DECLSPEC int SDLCALL SDL_StartVideoCapture(SDL_VideoCaptureDevice *device);

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
 * After used, the frame should be released with SDL_ReleaseVideoCaptureFrame
 *
 * \param device opened video capture device
 * \param frame pointer to get the frame
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ReleaseVideoCaptureFrame
 */
extern DECLSPEC int SDLCALL SDL_AcquireVideoCaptureFrame(SDL_VideoCaptureDevice *device, SDL_VideoCaptureFrame *frame);

/**
 * Release a frame.
 *
 * Let the back-end re-use the internal buffer for video capture.
 *
 * All acquired frames should be released before closing the device.
 *
 * \param device opened video capture device
 * \param frame frame pointer.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireVideoCaptureFrame
 */
extern DECLSPEC int SDLCALL SDL_ReleaseVideoCaptureFrame(SDL_VideoCaptureDevice *device, SDL_VideoCaptureFrame *frame);

/**
 * Stop Video Capture
 *
 * \param device opened video capture device
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartVideoCapture
 */
extern DECLSPEC int SDLCALL SDL_StopVideoCapture(SDL_VideoCaptureDevice *device);

/**
 * Use this function to shut down video_capture processing and close the
 * video_capture device.
 *
 * \param device opened video capture device
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenVideoCaptureWithSpec
 * \sa SDL_OpenVideoCapture
 */
extern DECLSPEC void SDLCALL SDL_CloseVideoCapture(SDL_VideoCaptureDevice *device);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_video_capture_h_ */
