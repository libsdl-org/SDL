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
#include "../SDL_internal.h"

#ifndef SDL_syscamera_h_
#define SDL_syscamera_h_

#include "../SDL_hashtable.h"

#define DEBUG_CAMERA 0

typedef struct SDL_CameraDevice SDL_CameraDevice;

/* Backends should call this as devices are added to the system (such as
   a USB camera being plugged in), and should also be called for
   for every device found during DetectDevices(). */
extern SDL_CameraDevice *SDL_AddCameraDevice(const char *name, SDL_CameraPosition position, int num_specs, const SDL_CameraSpec *specs, void *handle);

/* Backends should call this if an opened camera device is lost.
   This can happen due to i/o errors, or a device being unplugged, etc. */
extern void SDL_CameraDeviceDisconnected(SDL_CameraDevice *device);

// Find an SDL_CameraDevice, selected by a callback. NULL if not found. DOES NOT LOCK THE DEVICE.
extern SDL_CameraDevice *SDL_FindPhysicalCameraDeviceByCallback(SDL_bool (*callback)(SDL_CameraDevice *device, void *userdata), void *userdata);

// Backends should call this when the user has approved/denied access to a camera.
extern void SDL_CameraDevicePermissionOutcome(SDL_CameraDevice *device, SDL_bool approved);

// Backends can call this to get a standardized name for a thread to power a specific camera device.
extern char *SDL_GetCameraThreadName(SDL_CameraDevice *device, char *buf, size_t buflen);

// Backends can call these to change a device's refcount.
extern void RefPhysicalCameraDevice(SDL_CameraDevice *device);
extern void UnrefPhysicalCameraDevice(SDL_CameraDevice *device);

// These functions are the heart of the camera threads. Backends can call them directly if they aren't using the SDL-provided thread.
extern void SDL_CameraThreadSetup(SDL_CameraDevice *device);
extern SDL_bool SDL_CameraThreadIterate(SDL_CameraDevice *device);
extern void SDL_CameraThreadShutdown(SDL_CameraDevice *device);

// common utility functionality to gather up camera specs. Not required!
typedef struct CameraFormatAddData
{
    SDL_CameraSpec *specs;
    int num_specs;
    int allocated_specs;
} CameraFormatAddData;

int SDL_AddCameraFormat(CameraFormatAddData *data, SDL_PixelFormatEnum format, SDL_Colorspace colorspace, int w, int h, int framerate_numerator, int framerate_denominator);

typedef struct SurfaceList
{
    SDL_Surface *surface;
    Uint64 timestampNS;
    struct SurfaceList *next;
} SurfaceList;

// Define the SDL camera driver structure
struct SDL_CameraDevice
{
    // A mutex for locking
    SDL_Mutex *lock;

    // Human-readable device name.
    char *name;

    // Position of camera (front-facing, back-facing, etc).
    SDL_CameraPosition position;

    // When refcount hits zero, we destroy the device object.
    SDL_AtomicInt refcount;

    // These are, initially, set from camera_driver, but we might swap them out with Zombie versions on disconnect/failure.
    int (*WaitDevice)(SDL_CameraDevice *device);
    int (*AcquireFrame)(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS);
    void (*ReleaseFrame)(SDL_CameraDevice *device, SDL_Surface *frame);

    // All supported formats/dimensions for this device.
    SDL_CameraSpec *all_specs;

    // Elements in all_specs.
    int num_specs;

    // The device's actual specification that the camera is outputting, before conversion.
    SDL_CameraSpec actual_spec;

    // The device's current camera specification, after conversions.
    SDL_CameraSpec spec;

    // Unique value assigned at creation time.
    SDL_CameraDeviceID instance_id;

    // Driver-specific hardware data on how to open device (`hidden` is driver-specific data _when opened_).
    void *handle;

    // Dropping the first frame(s) after open seems to help timing on some platforms.
    int drop_frames;

    // Backend timestamp of first acquired frame, so we can keep these meaningful regardless of epoch.
    Uint64 base_timestamp;

    // SDL timestamp of first acquired frame, so we can roughly convert to SDL ticks.
    Uint64 adjust_timestamp;

    // Pixel data flows from the driver into these, then gets converted for the app if necessary.
    SDL_Surface *acquire_surface;

    // acquire_surface converts or scales to this surface before landing in output_surfaces, if necessary.
    SDL_Surface *conversion_surface;

    // A queue of surfaces that buffer converted/scaled frames of video until the app claims them.
    SurfaceList output_surfaces[8];
    SurfaceList filled_output_surfaces;        // this is FIFO
    SurfaceList empty_output_surfaces;         // this is LIFO
    SurfaceList app_held_output_surfaces;

    // A fake video frame we allocate if the camera fails/disconnects.
    Uint8 *zombie_pixels;

    // non-zero if acquire_surface needs to be scaled for final output.
    int needs_scaling;  // -1: downscale, 0: no scaling, 1: upscale

    // SDL_TRUE if acquire_surface needs to be converted for final output.
    SDL_bool needs_conversion;

    // Current state flags
    SDL_AtomicInt shutdown;
    SDL_AtomicInt zombie;

    // A thread to feed the camera device
    SDL_Thread *thread;

    // Optional properties.
    SDL_PropertiesID props;

    // -1: user denied permission, 0: waiting for user response, 1: user approved permission.
    int permission;

    // Data private to this driver, used when device is opened and running.
    struct SDL_PrivateCameraData *hidden;
};

typedef struct SDL_CameraDriverImpl
{
    void (*DetectDevices)(void);
    int (*OpenDevice)(SDL_CameraDevice *device, const SDL_CameraSpec *spec);
    void (*CloseDevice)(SDL_CameraDevice *device);
    int (*WaitDevice)(SDL_CameraDevice *device);
    int (*AcquireFrame)(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS); // set frame->pixels, frame->pitch, and *timestampNS!
    void (*ReleaseFrame)(SDL_CameraDevice *device, SDL_Surface *frame); // Reclaim frame->pixels and frame->pitch!
    void (*FreeDeviceHandle)(SDL_CameraDevice *device); // SDL is done with this device; free the handle from SDL_AddCameraDevice()
    void (*Deinitialize)(void);

    SDL_bool ProvidesOwnCallbackThread;
} SDL_CameraDriverImpl;

typedef struct SDL_PendingCameraDeviceEvent
{
    Uint32 type;
    SDL_CameraDeviceID devid;
    struct SDL_PendingCameraDeviceEvent *next;
} SDL_PendingCameraDeviceEvent;

typedef struct SDL_CameraDriver
{
    const char *name;  // The name of this camera driver
    const char *desc;  // The description of this camera driver
    SDL_CameraDriverImpl impl; // the backend's interface

    SDL_RWLock *device_hash_lock;  // A rwlock that protects `device_hash`
    SDL_HashTable *device_hash;  // the collection of currently-available camera devices
    SDL_PendingCameraDeviceEvent pending_events;
    SDL_PendingCameraDeviceEvent *pending_events_tail;

    SDL_AtomicInt device_count;
    SDL_AtomicInt shutting_down;  // non-zero during SDL_Quit, so we known not to accept any last-minute device hotplugs.
} SDL_CameraDriver;

typedef struct CameraBootStrap
{
    const char *name;
    const char *desc;
    SDL_bool (*init)(SDL_CameraDriverImpl *impl);
    SDL_bool demand_only; // if SDL_TRUE: request explicitly, or it won't be available.
} CameraBootStrap;

// Not all of these are available in a given build. Use #ifdefs, etc.
extern CameraBootStrap DUMMYCAMERA_bootstrap;
extern CameraBootStrap PIPEWIRECAMERA_bootstrap;
extern CameraBootStrap V4L2_bootstrap;
extern CameraBootStrap COREMEDIA_bootstrap;
extern CameraBootStrap ANDROIDCAMERA_bootstrap;
extern CameraBootStrap EMSCRIPTENCAMERA_bootstrap;
extern CameraBootStrap MEDIAFOUNDATION_bootstrap;

#endif // SDL_syscamera_h_
