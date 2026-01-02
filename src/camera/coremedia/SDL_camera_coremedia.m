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

#ifdef SDL_CAMERA_DRIVER_COREMEDIA

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"
#include "../../thread/SDL_systhread.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

#if defined(SDL_PLATFORM_IOS) && !defined(SDL_PLATFORM_TVOS)
#define USE_UIKIT_DEVICE_ROTATION
#endif

#ifdef USE_UIKIT_DEVICE_ROTATION
#import <UIKit/UIKit.h>
#endif

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

static void CoreMediaFormatToSDL(FourCharCode fmt, SDL_PixelFormat *pixel_format, SDL_Colorspace *colorspace)
{
    switch (fmt) {
        #define CASE(x, y, z) case x: *pixel_format = y; *colorspace = z; return
        // the 16LE ones should use 16BE if we're on a Bigendian system like PowerPC,
        // but at current time there is no bigendian Apple platform that has CoreMedia.
        CASE(kCMPixelFormat_16LE555, SDL_PIXELFORMAT_XRGB1555, SDL_COLORSPACE_SRGB);
        CASE(kCMPixelFormat_16LE5551, SDL_PIXELFORMAT_RGBA5551, SDL_COLORSPACE_SRGB);
        CASE(kCMPixelFormat_16LE565, SDL_PIXELFORMAT_RGB565, SDL_COLORSPACE_SRGB);
        CASE(kCMPixelFormat_24RGB, SDL_PIXELFORMAT_RGB24, SDL_COLORSPACE_SRGB);
        CASE(kCMPixelFormat_32ARGB, SDL_PIXELFORMAT_ARGB32, SDL_COLORSPACE_SRGB);
        CASE(kCMPixelFormat_32BGRA, SDL_PIXELFORMAT_BGRA32, SDL_COLORSPACE_SRGB);
        CASE(kCMPixelFormat_422YpCbCr8, SDL_PIXELFORMAT_UYVY, SDL_COLORSPACE_BT709_LIMITED);
        CASE(kCMPixelFormat_422YpCbCr8_yuvs, SDL_PIXELFORMAT_YUY2, SDL_COLORSPACE_BT709_LIMITED);
        CASE(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, SDL_PIXELFORMAT_NV12, SDL_COLORSPACE_BT709_LIMITED);
        CASE(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, SDL_PIXELFORMAT_NV12, SDL_COLORSPACE_BT709_FULL);
        CASE(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, SDL_PIXELFORMAT_P010, SDL_COLORSPACE_BT2020_LIMITED);
        CASE(kCVPixelFormatType_420YpCbCr10BiPlanarFullRange, SDL_PIXELFORMAT_P010, SDL_COLORSPACE_BT2020_FULL);
        #undef CASE
        default:
            #if DEBUG_CAMERA
            SDL_Log("CAMERA: Unknown format FourCharCode '%d'", (int) fmt);
            #endif
            break;
    }
    *pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    *colorspace = SDL_COLORSPACE_UNKNOWN;
}

@class SDLCaptureVideoDataOutputSampleBufferDelegate;

// just a simple wrapper to help ARC manage memory...
@interface SDLPrivateCameraData : NSObject
@property(nonatomic, retain) AVCaptureSession *session;
@property(nonatomic, retain) SDLCaptureVideoDataOutputSampleBufferDelegate *delegate;
@property(nonatomic, assign) CMSampleBufferRef current_sample;
#ifdef USE_UIKIT_DEVICE_ROTATION
@property(nonatomic, assign) UIDeviceOrientation last_device_orientation;
#endif
@end

@implementation SDLPrivateCameraData
@end


static bool CheckCameraPermissions(SDL_Camera *device)
{
    if (device->permission == SDL_CAMERA_PERMISSION_STATE_PENDING) {  // still expecting a permission result.
        if (@available(macOS 14, *)) {
            const AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
            if (status != AVAuthorizationStatusNotDetermined) {   // NotDetermined == still waiting for an answer from the user.
                SDL_CameraPermissionOutcome(device, (status == AVAuthorizationStatusAuthorized) ? true : false);
            }
        } else {
            SDL_CameraPermissionOutcome(device, true);  // always allowed (or just unqueryable...?) on older macOS.
        }
    }

    return (device->permission > SDL_CAMERA_PERMISSION_STATE_PENDING);
}

// this delegate just receives new video frames on a Grand Central Dispatch queue, and fires off the
// main device thread iterate function directly to consume it.
@interface SDLCaptureVideoDataOutputSampleBufferDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
    @property SDL_Camera *device;
    -(id) init:(SDL_Camera *) dev;
    -(void) captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
@end

@implementation SDLCaptureVideoDataOutputSampleBufferDelegate

    -(id) init:(SDL_Camera *) dev {
        if ( self = [super init] ) {
            _device = dev;
        }
        return self;
    }

    - (void) captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
    {
        SDL_Camera *device = self.device;
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

static bool COREMEDIA_WaitDevice(SDL_Camera *device)
{
    return true;  // this isn't used atm, since we run our own thread out of Grand Central Dispatch.
}

static SDL_CameraFrameResult COREMEDIA_AcquireFrame(SDL_Camera *device, SDL_Surface *frame, Uint64 *timestampNS, float *rotation)
{
    SDL_CameraFrameResult result = SDL_CAMERA_FRAME_READY;
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

    frame->w = (int)CVPixelBufferGetWidth(image);
    frame->h = (int)CVPixelBufferGetHeight(image);

    if ((planar == 0) && (numPlanes == 0)) {
        const int pitch = (int) CVPixelBufferGetBytesPerRow(image);
        const size_t buflen = pitch * frame->h;
        frame->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), buflen);
        if (frame->pixels == NULL) {
            result = SDL_CAMERA_FRAME_ERROR;
        } else {
            frame->pitch = pitch;
            SDL_memcpy(frame->pixels, CVPixelBufferGetBaseAddress(image), buflen);
        }
    } else {
        // !!! FIXME: we have an open issue in SDL3 to allow SDL_Surface to support non-contiguous planar data, but we don't have it yet.
        size_t buflen = 0;
        for (int i = 0; i < numPlanes; i++) {
            size_t plane_height = CVPixelBufferGetHeightOfPlane(image, i);
            size_t plane_pitch = CVPixelBufferGetBytesPerRowOfPlane(image, i);
            size_t plane_size = (plane_pitch * plane_height);
            buflen += plane_size;
        }

        frame->pitch = (int)CVPixelBufferGetBytesPerRowOfPlane(image, 0);  // this is what SDL3 currently expects
        frame->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), buflen);
        if (frame->pixels == NULL) {
            result = SDL_CAMERA_FRAME_ERROR;
        } else {
            Uint8 *dst = frame->pixels;
            for (int i = 0; i < numPlanes; i++) {
                const void *src = CVPixelBufferGetBaseAddressOfPlane(image, i);
                size_t plane_height = CVPixelBufferGetHeightOfPlane(image, i);
                size_t plane_pitch = CVPixelBufferGetBytesPerRowOfPlane(image, i);
                size_t plane_size = (plane_pitch * plane_height);
                SDL_memcpy(dst, src, plane_size);
                dst += plane_size;
            }
        }
    }

    CVPixelBufferUnlockBaseAddress(image, 0);

    #ifdef USE_UIKIT_DEVICE_ROTATION
    UIDeviceOrientation device_orientation = [[UIDevice currentDevice] orientation];
    if (!UIDeviceOrientationIsValidInterfaceOrientation(device_orientation)) {
        device_orientation = hidden.last_device_orientation;  // possible the phone is laying flat or something went wrong, just stay with the last known-good orientation.
    } else {
        hidden.last_device_orientation = device_orientation;  // update the last known-good orientation for later.
    }

    const UIInterfaceOrientation ui_orientation = [UIApplication sharedApplication].statusBarOrientation;

    // there is probably math for this, but this is easy to slap into a table.
    // rotation = rotations[uiorientation-1][devorientation-1];
    if (device->position == SDL_CAMERA_POSITION_BACK_FACING) {
        static const Uint16 back_rotations[4][4] = {
            {   90,  90,  90,  90 },  // ui portrait
            {  270, 270, 270, 270 },  // ui portait upside down
            {    0,   0,   0,   0 },  // ui landscape left
            {  180, 180, 180, 180 }   // ui landscape right
        };
        *rotation = (float) back_rotations[ui_orientation - 1][device_orientation - 1];
    } else {
        static const Uint16 front_rotations[4][4] = {
            {   90,  90, 270, 270 },  // ui portrait
            {  270, 270,  90,  90 },  // ui portait upside down
            {    0,   0, 180, 180 },  // ui landscape left
            {  180, 180,   0,   0 }   // ui landscape right
        };
        *rotation = (float) front_rotations[ui_orientation - 1][device_orientation - 1];
    }
    #endif

    return result;
}

static void COREMEDIA_ReleaseFrame(SDL_Camera *device, SDL_Surface *frame)
{
    // !!! FIXME: this currently copies the data to the surface, but in theory we could just keep this locked until ReleaseFrame...
    SDL_aligned_free(frame->pixels);
}

static void COREMEDIA_CloseDevice(SDL_Camera *device)
{
    if (device && device->hidden) {
        #ifdef USE_UIKIT_DEVICE_ROTATION
        [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
        #endif

        SDLPrivateCameraData *hidden = (SDLPrivateCameraData *) CFBridgingRelease(device->hidden);
        device->hidden = NULL;

        AVCaptureSession *session = hidden.session;
        if (session) {
            hidden.session = nil;
            [session stopRunning];
            [session removeInput:[session.inputs objectAtIndex:0]];
            [session removeOutput:(AVCaptureVideoDataOutput *)[session.outputs objectAtIndex:0]];
            session = nil;
        }

        hidden.delegate = NULL;
        hidden.current_sample = NULL;
    }
}

static bool COREMEDIA_OpenDevice(SDL_Camera *device, const SDL_CameraSpec *spec)
{
    AVCaptureDevice *avdevice = (__bridge AVCaptureDevice *) device->handle;

    // Pick format that matches the spec
    const int w = spec->width;
    const int h = spec->height;
    const float rate = (float)spec->framerate_numerator / spec->framerate_denominator;
    AVCaptureDeviceFormat *spec_format = nil;
    NSArray<AVCaptureDeviceFormat *> *formats = [avdevice formats];
    for (AVCaptureDeviceFormat *format in formats) {
        CMFormatDescriptionRef formatDescription = [format formatDescription];
        SDL_PixelFormat device_format = SDL_PIXELFORMAT_UNKNOWN;
        SDL_Colorspace device_colorspace = SDL_COLORSPACE_UNKNOWN;
        CoreMediaFormatToSDL(CMFormatDescriptionGetMediaSubType(formatDescription), &device_format, &device_colorspace);
        if (device_format != spec->format || device_colorspace != spec->colorspace) {
            continue;
        }

        const CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(formatDescription);
        if ((int)dim.width != w || (int)dim.height != h) {
            continue;
        }

        const float FRAMERATE_EPSILON = 0.01f;
        for (AVFrameRateRange *framerate in format.videoSupportedFrameRateRanges) {
            // Check if the requested rate is within the supported range
            if (rate >= (framerate.minFrameRate - FRAMERATE_EPSILON) &&
                rate <= (framerate.maxFrameRate + FRAMERATE_EPSILON)) {

                // Prefer formats with narrower frame rate ranges that are closer to our target
                // This helps avoid formats that support a wide range (like 10-60 FPS)
                // when we want a specific rate (like 30 FPS)
                bool should_select = false;
                if (spec_format == nil) {
                    should_select = true;
                } else {
                    AVFrameRateRange *current_range = spec_format.videoSupportedFrameRateRanges.firstObject;
                    float current_range_width = current_range.maxFrameRate - current_range.minFrameRate;
                    float new_range_width = framerate.maxFrameRate - framerate.minFrameRate;

                    // Prefer formats with narrower ranges, or if ranges are similar, prefer closer to target
                    if (new_range_width < current_range_width) {
                        should_select = true;
                    } else if (SDL_fabsf(new_range_width - current_range_width) < 0.1f) {
                        // Similar range width, prefer the one closer to our target rate
                        float current_distance = SDL_fabsf(rate - current_range.minFrameRate);
                        float new_distance = SDL_fabsf(rate - framerate.minFrameRate);
                        if (new_distance < current_distance) {
                            should_select = true;
                        }
                    }
                }

                if (should_select) {
                    spec_format = format;
                }
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

    // Try to set the frame duration to enforce the requested frame rate
    const float frameRate = (float)spec->framerate_numerator / spec->framerate_denominator;
    const CMTime frameDuration = CMTimeMake(1, (int32_t)frameRate);

    // Check if the device supports setting frame duration
    if ([avdevice respondsToSelector:@selector(setActiveVideoMinFrameDuration:)] &&
        [avdevice respondsToSelector:@selector(setActiveVideoMaxFrameDuration:)]) {
        @try {
            avdevice.activeVideoMinFrameDuration = frameDuration;
            avdevice.activeVideoMaxFrameDuration = frameDuration;
        } @catch (NSException *exception) {
            // Some devices don't support setting frame duration, that's okay
        }
    }

    [avdevice unlockForConfiguration];

    AVCaptureSession *session = [[AVCaptureSession alloc] init];
    if (session == nil) {
        return SDL_SetError("Failed to allocate/init AVCaptureSession");
    }

    session.sessionPreset = AVCaptureSessionPresetHigh;
#if defined(SDL_PLATFORM_IOS)
    if (@available(iOS 10.0, tvOS 17.0, *)) {
        session.automaticallyConfiguresCaptureDeviceForWideColor = NO;
    }
#endif

    NSError *error = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:avdevice error:&error];
    if (!input) {
        return SDL_SetError("Cannot create AVCaptureDeviceInput");
    }

    AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
    if (!output) {
        return SDL_SetError("Cannot create AVCaptureVideoDataOutput");
    }

    output.videoSettings = @{
        (id)kCVPixelBufferWidthKey : @(spec->width),
        (id)kCVPixelBufferHeightKey : @(spec->height),
        (id)kCVPixelBufferPixelFormatTypeKey : @(CMFormatDescriptionGetMediaSubType([spec_format formatDescription]))
    };

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

    // Try to set the frame rate on the connection
    AVCaptureConnection *connection = [output connectionWithMediaType:AVMediaTypeVideo];
    if (connection && connection.isVideoMinFrameDurationSupported) {
        connection.videoMinFrameDuration = frameDuration;
        if (connection.isVideoMaxFrameDurationSupported) {
            connection.videoMaxFrameDuration = frameDuration;
        }
    }

    [session commitConfiguration];

    SDLPrivateCameraData *hidden = [[SDLPrivateCameraData alloc] init];
    if (hidden == nil) {
        return SDL_SetError("Cannot create SDLPrivateCameraData");
    }

    hidden.session = session;
    hidden.delegate = delegate;
    hidden.current_sample = NULL;

    #ifdef USE_UIKIT_DEVICE_ROTATION
    // When using a camera, we turn on device orientation tracking. The docs note that this turns on
    // the device's accelerometer, so I assume this burns power, so we don't leave this running all
    // the time. These calls nest, so we just need to call the matching `end` message when we close.
    // You _can_ get an actual events through this mechanism, but we just want to be able to call
    // -[UIDevice orientation], which will update with real info while notificatons are enabled.
    UIDevice *uidevice = [UIDevice currentDevice];
    [uidevice beginGeneratingDeviceOrientationNotifications];
    hidden.last_device_orientation = uidevice.orientation;
    if (!UIDeviceOrientationIsValidInterfaceOrientation(hidden.last_device_orientation)) {
        // accelerometer isn't ready yet or the phone is laying flat or something. Just try to guess from how the UI is oriented at the moment.
        switch ([UIApplication sharedApplication].statusBarOrientation) {
            case UIInterfaceOrientationPortrait: hidden.last_device_orientation = UIDeviceOrientationPortrait; break;
            case UIInterfaceOrientationPortraitUpsideDown: hidden.last_device_orientation = UIDeviceOrientationPortraitUpsideDown; break;
            case UIInterfaceOrientationLandscapeLeft: hidden.last_device_orientation = UIDeviceOrientationLandscapeRight; break;  // Apple docs say UI and device orientations are reversed in landscape.
            case UIInterfaceOrientationLandscapeRight: hidden.last_device_orientation = UIDeviceOrientationLandscapeLeft; break;
            default: hidden.last_device_orientation = UIDeviceOrientationPortrait; break;  // oh well.
        }
    }
    #endif

    device->hidden = (struct SDL_PrivateCameraData *)CFBridgingRetain(hidden);

    [session startRunning];  // !!! FIXME: docs say this can block while camera warms up and shouldn't be done on main thread. Maybe push through `queue`?

    CheckCameraPermissions(device);  // check right away, in case the process is already granted permission.

    return true;
}

static void COREMEDIA_FreeDeviceHandle(SDL_Camera *device)
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

//NSLog(@"Available camera format: %@\n", fmt);
        SDL_PixelFormat device_format = SDL_PIXELFORMAT_UNKNOWN;
        SDL_Colorspace device_colorspace = SDL_COLORSPACE_UNKNOWN;
        CoreMediaFormatToSDL(CMFormatDescriptionGetMediaSubType(fmt.formatDescription), &device_format, &device_colorspace);
        if (device_format == SDL_PIXELFORMAT_UNKNOWN) {
            continue;
        }

        const CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(fmt.formatDescription);
        const int w = (int) dims.width;
        const int h = (int) dims.height;
        for (AVFrameRateRange *framerate in fmt.videoSupportedFrameRateRanges) {
            int min_numerator = 0, min_denominator = 1;
            int max_numerator = 0, max_denominator = 1;

            SDL_CalculateFraction(framerate.minFrameRate, &min_numerator, &min_denominator);
            SDL_AddCameraFormat(add_data, device_format, device_colorspace, w, h, min_numerator, min_denominator);
            SDL_CalculateFraction(framerate.maxFrameRate, &max_numerator, &max_denominator);
            if (max_numerator != min_numerator || max_denominator != min_denominator) {
                SDL_AddCameraFormat(add_data, device_format, device_colorspace, w, h, max_numerator, max_denominator);
            }
        }
    }
}

static bool FindCoreMediaCameraByUniqueID(SDL_Camera *device, void *userdata)
{
    NSString *uniqueid = (__bridge NSString *) userdata;
    AVCaptureDevice *avdev = (__bridge AVCaptureDevice *) device->handle;
    return ([uniqueid isEqualToString:avdev.uniqueID]) ? true : false;
}

static void MaybeAddDevice(AVCaptureDevice *avdevice)
{
    if (!avdevice.connected) {
        return;  // not connected.
    } else if (![avdevice hasMediaType:AVMediaTypeVideo]) {
        return;  // not a camera.
    } else if (SDL_FindPhysicalCameraByCallback(FindCoreMediaCameraByUniqueID, (__bridge void *) avdevice.uniqueID)) {
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
        SDL_AddCamera(avdevice.localizedName.UTF8String, position, add_data.num_specs, add_data.specs, (void *) CFBridgingRetain(avdevice));
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

static bool COREMEDIA_Init(SDL_CameraDriverImpl *impl)
{
    impl->DetectDevices = COREMEDIA_DetectDevices;
    impl->OpenDevice = COREMEDIA_OpenDevice;
    impl->CloseDevice = COREMEDIA_CloseDevice;
    impl->WaitDevice = COREMEDIA_WaitDevice;
    impl->AcquireFrame = COREMEDIA_AcquireFrame;
    impl->ReleaseFrame = COREMEDIA_ReleaseFrame;
    impl->FreeDeviceHandle = COREMEDIA_FreeDeviceHandle;
    impl->Deinitialize = COREMEDIA_Deinitialize;

    impl->ProvidesOwnCallbackThread = true;

    return true;
}

CameraBootStrap COREMEDIA_bootstrap = {
    "coremedia", "SDL Apple CoreMedia camera driver", COREMEDIA_Init, false
};

#endif // SDL_CAMERA_DRIVER_COREMEDIA

