/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_CAMERA_DRIVER_COREMEDIA

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"
#include "../../thread/SDL_systhread.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

/*
 * Need to link with:: CoreMedia CoreVideo
 *
 * Add in pInfo.list:
 *  <key>NSCameraUsageDescription</key> <string>Access camera</string>
 *
 *
 * MACOSX:
 * Add to the Code Sign Entitlement file:
 * <key>com.apple.security.device.camera</key> <true/>
 */

static Uint32 CoreMediaFormatToSDL(FourCharCode fmt)
{
    switch (fmt) {
        #define CASE(x, y) case x: return y
        // the 16LE ones should use 16BE if we're on a Bigendian system like PowerPC,
        // but at current time there is no bigendian Apple platform that has CoreMedia.
        CASE(kCMPixelFormat_16LE555, SDL_PIXELFORMAT_RGB555);
        CASE(kCMPixelFormat_16LE5551, SDL_PIXELFORMAT_RGBA5551);
        CASE(kCMPixelFormat_16LE565, SDL_PIXELFORMAT_RGB565);
        CASE(kCMPixelFormat_24RGB, SDL_PIXELFORMAT_RGB24);
        CASE(kCMPixelFormat_32ARGB, SDL_PIXELFORMAT_ARGB32);
        CASE(kCMPixelFormat_32BGRA, SDL_PIXELFORMAT_BGRA32);
        CASE(kCMPixelFormat_422YpCbCr8, SDL_PIXELFORMAT_YUY2);
        CASE(kCMPixelFormat_422YpCbCr8_yuvs, SDL_PIXELFORMAT_UYVY);
        #undef CASE
        default:
            #if DEBUG_CAMERA
            SDL_Log("CAMERA: Unknown format FourCharCode '%d'", (int) fmt);
            #endif
            break;
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}

@class SDLCaptureVideoDataOutputSampleBufferDelegate;

// just a simple wrapper to help ARC manage memory...
@interface SDLPrivateCameraData : NSObject
@property(nonatomic, retain) AVCaptureSession *session;
@property(nonatomic, retain) SDLCaptureVideoDataOutputSampleBufferDelegate *delegate;
@property(nonatomic, assign) CMSampleBufferRef current_sample;
@end

@implementation SDLPrivateCameraData
@end


static SDL_bool CheckCameraPermissions(SDL_CameraDevice *device)
{
    if (device->permission == 0) {  // still expecting a permission result.
        if (@available(macOS 14, *)) {
            const AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
            if (status != AVAuthorizationStatusNotDetermined) {   // NotDetermined == still waiting for an answer from the user.
                SDL_CameraDevicePermissionOutcome(device, (status == AVAuthorizationStatusAuthorized) ? SDL_TRUE : SDL_FALSE);
            }
        } else {
            SDL_CameraDevicePermissionOutcome(device, SDL_TRUE);  // always allowed (or just unqueryable...?) on older macOS.
        }
    }

    return (device->permission > 0);
}

// this delegate just receives new video frames on a Grand Central Dispatch queue, and fires off the
// main device thread iterate function directly to consume it.
@interface SDLCaptureVideoDataOutputSampleBufferDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
    @property SDL_CameraDevice *device;
    -(id) init:(SDL_CameraDevice *) dev;
    -(void) captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
@end

@implementation SDLCaptureVideoDataOutputSampleBufferDelegate

    -(id) init:(SDL_CameraDevice *) dev {
        if ( self = [super init] ) {
            _device = dev;
        }
        return self;
    }

    - (void) captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
    {
        SDL_CameraDevice *device = self.device;
        if (!device || !device->hidden) {
            return;  // oh well.
        }

        if (!CheckCameraPermissions(device)) {
            return;  // nothing to do right now, dump what is probably a completely black frame.
        }

        SDLPrivateCameraData *hidden = (__bridge SDLPrivateCameraData *) device->hidden;
        hidden.current_sample = sampleBuffer;
        SDL_CameraThreadIterate(device);
        hidden.current_sample = NULL;
    }

    - (void)captureOutput:(AVCaptureOutput *)output didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
    {
        #if DEBUG_CAMERA
        SDL_Log("CAMERA: Drop frame.");
        #endif
    }
@end

static int COREMEDIA_WaitDevice(SDL_CameraDevice *device)
{
    return 0;  // this isn't used atm, since we run our own thread out of Grand Central Dispatch.
}

static int COREMEDIA_AcquireFrame(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS)
{
    int retval = 1;
    SDLPrivateCameraData *hidden = (__bridge SDLPrivateCameraData *) device->hidden;
    CMSampleBufferRef sample_buffer = hidden.current_sample;
    hidden.current_sample = NULL;
    SDL_assert(sample_buffer != NULL);  // should only have been called from our delegate with a new frame.

    CMSampleTimingInfo timinginfo;
    if (CMSampleBufferGetSampleTimingInfo(sample_buffer, 0, &timinginfo) == noErr) {
        *timestampNS = (Uint64) (CMTimeGetSeconds(timinginfo.presentationTimeStamp) * ((Float64) SDL_NS_PER_SECOND));
    } else {
        SDL_assert(!"this shouldn't happen, I think.");
        *timestampNS = 0;
    }

    CVImageBufferRef image = CMSampleBufferGetImageBuffer(sample_buffer);  // does not retain `image` (and we don't want it to).
    const int numPlanes = (int) CVPixelBufferGetPlaneCount(image);
    const int planar = (int) CVPixelBufferIsPlanar(image);

    #if DEBUG_CAMERA
    const int w = (int) CVPixelBufferGetWidth(image);
    const int h = (int) CVPixelBufferGetHeight(image);
    const int sz = (int) CVPixelBufferGetDataSize(image);
    const int pitch = (int) CVPixelBufferGetBytesPerRow(image);
    SDL_Log("CAMERA: buffer planar=%d numPlanes=%d %d x %d sz=%d pitch=%d", planar, numPlanes, w, h, sz, pitch);
    #endif

    // !!! FIXME: this currently copies the data to the surface (see FIXME about non-contiguous planar surfaces, but in theory we could just keep this locked until ReleaseFrame...
    CVPixelBufferLockBaseAddress(image, 0);

    if ((planar == 0) && (numPlanes == 0)) {
        const int pitch = (int) CVPixelBufferGetBytesPerRow(image);
        const size_t buflen = pitch * frame->h;
        frame->pixels = SDL_aligned_alloc(SDL_SIMDGetAlignment(), buflen);
        if (frame->pixels == NULL) {
            retval = -1;
        } else {
            frame->pitch = pitch;
            SDL_memcpy(frame->pixels, CVPixelBufferGetBaseAddress(image), buflen);
        }
    } else {
        // !!! FIXME: we have an open issue in SDL3 to allow SDL_Surface to support non-contiguous planar data, but we don't have it yet.
        size_t buflen = 0;
        for (int i = 0; (i < numPlanes) && (i < 3); i++) {
            buflen += CVPixelBufferGetBytesPerRowOfPlane(image, i);
        }
        buflen *= frame->h;

        frame->pixels = SDL_aligned_alloc(SDL_SIMDGetAlignment(), buflen);
        if (frame->pixels == NULL) {
            retval = -1;
        } else {
            Uint8 *dst = frame->pixels;
            frame->pitch = (int) CVPixelBufferGetBytesPerRowOfPlane(image, 0);  // this is what SDL3 currently expects, probably incorrectly.
            for (int i = 0; (i < numPlanes) && (i < 3); i++) {
                const void *src = CVPixelBufferGetBaseAddressOfPlane(image, i);
                const size_t pitch = CVPixelBufferGetBytesPerRowOfPlane(image, i);
                SDL_memcpy(dst, src, pitch * frame->h);
                dst += pitch * frame->h;
            }
        }
    }

    CVPixelBufferUnlockBaseAddress(image, 0);

    return retval;
}

static void COREMEDIA_ReleaseFrame(SDL_CameraDevice *device, SDL_Surface *frame)
{
    // !!! FIXME: this currently copies the data to the surface, but in theory we could just keep this locked until ReleaseFrame...
    SDL_aligned_free(frame->pixels);
}

static void COREMEDIA_CloseDevice(SDL_CameraDevice *device)
{
    if (device && device->hidden) {
        SDLPrivateCameraData *hidden = (SDLPrivateCameraData *) CFBridgingRelease(device->hidden);
        device->hidden = NULL;

        AVCaptureSession *session = hidden.session;
        if (session) {
            hidden.session = nil;
            [session stopRunning];
            [session removeInput:[session.inputs objectAtIndex:0]];
            [session removeOutput:(AVCaptureVideoDataOutput*)[session.outputs objectAtIndex:0]];
            session = nil;
        }

        hidden.delegate = NULL;
        hidden.current_sample = NULL;
    }
}

static int COREMEDIA_OpenDevice(SDL_CameraDevice *device, const SDL_CameraSpec *spec)
{
    AVCaptureDevice *avdevice = (__bridge AVCaptureDevice *) device->handle;

    // Pick format that matches the spec
    const Uint32 sdlfmt = spec->format;
    const int w = spec->width;
    const int h = spec->height;
    const int rate = spec->interval_denominator;
    AVCaptureDeviceFormat *spec_format = nil;
    NSArray<AVCaptureDeviceFormat *> *formats = [avdevice formats];
    for (AVCaptureDeviceFormat *format in formats) {
        CMFormatDescriptionRef formatDescription = [format formatDescription];
        if (CoreMediaFormatToSDL(CMFormatDescriptionGetMediaSubType(formatDescription)) != sdlfmt) {
            continue;
        }

        const CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(formatDescription);
        if ( ((int) dim.width != w) || (((int) dim.height) != h) ) {
            continue;
        }

        for (AVFrameRateRange *framerate in format.videoSupportedFrameRateRanges) {
            if ((rate == (int) SDL_ceil((double) framerate.minFrameRate)) || (rate == (int) SDL_floor((double) framerate.maxFrameRate))) {
                spec_format = format;
                break;
            }
        }

        if (spec_format != nil) {
            break;
        }
    }

    if (spec_format == nil) {
        return SDL_SetError("camera spec format not available");
    } else if (![avdevice lockForConfiguration:NULL]) {
        return SDL_SetError("Cannot lockForConfiguration");
    }

    avdevice.activeFormat = spec_format;
    [avdevice unlockForConfiguration];

    AVCaptureSession *session = [[AVCaptureSession alloc] init];
    if (session == nil) {
        return SDL_SetError("Failed to allocate/init AVCaptureSession");
    }

    session.sessionPreset = AVCaptureSessionPresetHigh;

    NSError *error = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:avdevice error:&error];
    if (!input) {
        return SDL_SetError("Cannot create AVCaptureDeviceInput");
    }

    AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
    if (!output) {
        return SDL_SetError("Cannot create AVCaptureVideoDataOutput");
    }

    char threadname[64];
    SDL_GetCameraThreadName(device, threadname, sizeof (threadname));
    dispatch_queue_t queue = dispatch_queue_create(threadname, NULL);
    //dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    if (!queue) {
        return SDL_SetError("dispatch_queue_create() failed");
    }

    SDLCaptureVideoDataOutputSampleBufferDelegate *delegate = [[SDLCaptureVideoDataOutputSampleBufferDelegate alloc] init:device];
    if (delegate == nil) {
        return SDL_SetError("Cannot create SDLCaptureVideoDataOutputSampleBufferDelegate");
    }
    [output setSampleBufferDelegate:delegate queue:queue];

    if (![session canAddInput:input]) {
        return SDL_SetError("Cannot add AVCaptureDeviceInput");
    }
    [session addInput:input];

    if (![session canAddOutput:output]) {
        return SDL_SetError("Cannot add AVCaptureVideoDataOutput");
    }
    [session addOutput:output];

    [session commitConfiguration];

    SDLPrivateCameraData *hidden = [[SDLPrivateCameraData alloc] init];
    if (hidden == nil) {
        return SDL_SetError("Cannot create SDLPrivateCameraData");
    }

    hidden.session = session;
    hidden.delegate = delegate;
    hidden.current_sample = NULL;
    device->hidden = (struct SDL_PrivateCameraData *)CFBridgingRetain(hidden);

    [session startRunning];  // !!! FIXME: docs say this can block while camera warms up and shouldn't be done on main thread. Maybe push through `queue`?

    CheckCameraPermissions(device);  // check right away, in case the process is already granted permission.

    return 0;
}

static void COREMEDIA_FreeDeviceHandle(SDL_CameraDevice *device)
{
    if (device && device->handle) {
        CFBridgingRelease(device->handle);
    }
}

static void GatherCameraSpecs(AVCaptureDevice *device, CameraFormatAddData *add_data)
{
    SDL_zerop(add_data);

    for (AVCaptureDeviceFormat *fmt in device.formats) {
        if (CMFormatDescriptionGetMediaType(fmt.formatDescription) != kCMMediaType_Video) {
            continue;
        }

        const Uint32 sdlfmt = CoreMediaFormatToSDL(CMFormatDescriptionGetMediaSubType(fmt.formatDescription));
        if (sdlfmt == SDL_PIXELFORMAT_UNKNOWN) {
            continue;
        }

        const CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(fmt.formatDescription);
        const int w = (int) dims.width;
        const int h = (int) dims.height;
        for (AVFrameRateRange *framerate in fmt.videoSupportedFrameRateRanges) {
            int rate;

            rate = (int) SDL_ceil((double) framerate.minFrameRate);
            if (rate) {
                SDL_AddCameraFormat(add_data, sdlfmt, w, h, 1, rate);
            }
            rate = (int) SDL_floor((double) framerate.maxFrameRate);
            if (rate) {
                SDL_AddCameraFormat(add_data, sdlfmt, w, h, 1, rate);
            }
        }
    }
}

static SDL_bool FindCoreMediaCameraDeviceByUniqueID(SDL_CameraDevice *device, void *userdata)
{
    NSString *uniqueid = (__bridge NSString *) userdata;
    AVCaptureDevice *avdev = (__bridge AVCaptureDevice *) device->handle;
    return ([uniqueid isEqualToString:avdev.uniqueID]) ? SDL_TRUE : SDL_FALSE;
}

static void MaybeAddDevice(AVCaptureDevice *avdevice)
{
    if (!avdevice.connected) {
        return;  // not connected.
    } else if (![avdevice hasMediaType:AVMediaTypeVideo]) {
        return;  // not a camera.
    } else if (SDL_FindPhysicalCameraDeviceByCallback(FindCoreMediaCameraDeviceByUniqueID, (__bridge void *) avdevice.uniqueID)) {
        return;  // already have this one.
    }

    CameraFormatAddData add_data;
    GatherCameraSpecs(avdevice, &add_data);
    if (add_data.num_specs > 0) {
        SDL_CameraPosition position = SDL_CAMERA_POSITION_UNKNOWN;
        if (avdevice.position == AVCaptureDevicePositionFront) {
            position = SDL_CAMERA_POSITION_FRONT_FACING;
        } else if (avdevice.position == AVCaptureDevicePositionBack) {
            position = SDL_CAMERA_POSITION_BACK_FACING;
        }
        SDL_AddCameraDevice(avdevice.localizedName.UTF8String, position, add_data.num_specs, add_data.specs, (void *) CFBridgingRetain(avdevice));
    }

    SDL_free(add_data.specs);
}

static void COREMEDIA_DetectDevices(void)
{
    NSArray<AVCaptureDevice *> *devices = nil;

    if (@available(macOS 10.15, iOS 13, *)) {
        // kind of annoying that there isn't a "give me anything that looks like a camera" option,
        // so this list will need to be updated when Apple decides to add
        // AVCaptureDeviceTypeBuiltInQuadrupleCamera some day.
        NSArray *device_types = @[
            #ifdef SDL_PLATFORM_IOS
            AVCaptureDeviceTypeBuiltInTelephotoCamera,
            AVCaptureDeviceTypeBuiltInDualCamera,
            AVCaptureDeviceTypeBuiltInDualWideCamera,
            AVCaptureDeviceTypeBuiltInTripleCamera,
            AVCaptureDeviceTypeBuiltInUltraWideCamera,
            #else
            AVCaptureDeviceTypeExternalUnknown,
            #endif
            AVCaptureDeviceTypeBuiltInWideAngleCamera
        ];

        AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession
                        discoverySessionWithDeviceTypes:device_types
                        mediaType:AVMediaTypeVideo
                        position:AVCaptureDevicePositionUnspecified];

        devices = discoverySession.devices;
        // !!! FIXME: this can use Key Value Observation to get hotplug events.
    } else {
        // this is deprecated but works back to macOS 10.7; 10.15 added AVCaptureDeviceDiscoverySession as a replacement.
        devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        // !!! FIXME: this can use AVCaptureDeviceWasConnectedNotification and AVCaptureDeviceWasDisconnectedNotification with NSNotificationCenter to get hotplug events.
    }

    for (AVCaptureDevice *device in devices) {
        MaybeAddDevice(device);
    }
}

static void COREMEDIA_Deinitialize(void)
{
    // !!! FIXME: disable hotplug.
}

static SDL_bool COREMEDIA_Init(SDL_CameraDriverImpl *impl)
{
    impl->DetectDevices = COREMEDIA_DetectDevices;
    impl->OpenDevice = COREMEDIA_OpenDevice;
    impl->CloseDevice = COREMEDIA_CloseDevice;
    impl->WaitDevice = COREMEDIA_WaitDevice;
    impl->AcquireFrame = COREMEDIA_AcquireFrame;
    impl->ReleaseFrame = COREMEDIA_ReleaseFrame;
    impl->FreeDeviceHandle = COREMEDIA_FreeDeviceHandle;
    impl->Deinitialize = COREMEDIA_Deinitialize;

    impl->ProvidesOwnCallbackThread = SDL_TRUE;

    return SDL_TRUE;
}

CameraBootStrap COREMEDIA_bootstrap = {
    "coremedia", "SDL Apple CoreMedia camera driver", COREMEDIA_Init, SDL_FALSE
};

#endif // SDL_CAMERA_DRIVER_COREMEDIA

