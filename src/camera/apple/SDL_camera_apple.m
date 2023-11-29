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

#ifdef SDL_CAMERA_APPLE

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"
#include "../thread/SDL_systhread.h"

#if defined(HAVE_COREMEDIA) && defined(SDL_PLATFORM_MACOS) && (__MAC_OS_X_VERSION_MAX_ALLOWED < 101500)
// AVCaptureDeviceTypeBuiltInWideAngleCamera requires macOS SDK 10.15
#undef HAVE_COREMEDIA
#endif

#ifdef SDL_PLATFORM_TVOS
#undef HAVE_COREMEDIA
#endif

// !!! FIXME: use the dummy driver
// !!! FIXME: actually, move everything over to backend callbacks instead.
#ifndef HAVE_COREMEDIA
int InitDevice(SDL_CameraDevice *_this) {
    return -1;
}
int OpenDevice(SDL_CameraDevice *_this) {
    return -1;
}
int AcquireFrame(SDL_CameraDevice *_this, SDL_CameraFrame *frame) {
    return -1;
}
void CloseDevice(SDL_CameraDevice *_this) {
}
int GetCameraDeviceName(SDL_CameraDeviceID instance_id, char *buf, int size) {
    return -1;
}
int GetDeviceSpec(SDL_CameraDevice *_this, SDL_CameraSpec *spec) {
    return -1;
}
int GetFormat(SDL_CameraDevice *_this, int index, Uint32 *format) {
    return -1;
}
int GetFrameSize(SDL_CameraDevice *_this, Uint32 format, int index, int *width, int *height) {
    return -1;
}
SDL_CameraDeviceID *GetCameraDevices(int *count) {
    return NULL;
}
int GetNumFormats(SDL_CameraDevice *_this) {
    return 0;
}
int GetNumFrameSizes(SDL_CameraDevice *_this, Uint32 format) {
    return 0;
}
int ReleaseFrame(SDL_CameraDevice *_this, SDL_CameraFrame *frame) {
    return 0;
}
int StartCapture(SDL_CameraDevice *_this) {
    return 0;
}
int StopCapture(SDL_CameraDevice *_this) {
    return 0;
}
int SDL_SYS_CameraInit(void) {
    return 0;
}
int SDL_SYS_CameraQuit(void) {
    return 0;
}


#else

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
 *
 *
 * IOS:
 *
 * - Need to link with:: CoreMedia CoreVideo
 * - Add #define SDL_CAMERA 1
 *   to SDL_build_config_ios.h
 */

@class MySampleBufferDelegate;

struct SDL_PrivateCameraData
{
    dispatch_queue_t queue;
    MySampleBufferDelegate *delegate;
    AVCaptureSession *session;
    CMSimpleQueueRef frame_queue;
};

static NSString *fourcc_to_nstring(Uint32 code)
{
    Uint8 buf[4];
    *(Uint32 *)buf = code;
    return [NSString stringWithFormat:@"%c%c%c%c", buf[3], buf[2], buf[1], buf[0]];
}

static NSArray<AVCaptureDevice *> *DiscoverCameraDevices()
{
    NSArray *deviceType = @[AVCaptureDeviceTypeBuiltInWideAngleCamera];

    AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession
                    discoverySessionWithDeviceTypes:deviceType
                    mediaType:AVMediaTypeVideo
                    position:AVCaptureDevicePositionUnspecified];

    NSArray<AVCaptureDevice *> *devices = discoverySession.devices;

    if ([devices count] > 0) {
        return devices;
    } else {
        AVCaptureDevice *captureDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        if (captureDevice == nil) {
            return devices;
        } else {
            NSArray<AVCaptureDevice *> *default_device = @[ captureDevice ];
            return default_device;
        }
    }

    return devices;
}

static AVCaptureDevice *GetCameraDeviceByName(const char *dev_name)
{
    NSArray<AVCaptureDevice *> *devices = DiscoverCameraDevices();

    for (AVCaptureDevice *device in devices) {
        char buf[1024];
        NSString *cameraID = [device localizedName];
        const char *str = [cameraID UTF8String];
        SDL_snprintf(buf, sizeof (buf) - 1, "%s", str);
        if (SDL_strcmp(buf, dev_name) == 0) {
            return device;
        }
    }
    return nil;
}

static Uint32 nsfourcc_to_sdlformat(NSString *nsfourcc)
{
    const char *str = [nsfourcc UTF8String];

    /* FIXME
     * on IOS this mode gives 2 planes, and it's NV12
     * on macos, 1 plane/ YVYU
     */
    #ifdef SDL_PLATFORM_MACOS
    if (SDL_strcmp("420v", str) == 0)  return SDL_PIXELFORMAT_YVYU;
    #else
    if (SDL_strcmp("420v", str) == 0)  return SDL_PIXELFORMAT_NV12;
    #endif

    if (SDL_strcmp("yuvs", str) == 0)  return SDL_PIXELFORMAT_UYVY;
    if (SDL_strcmp("420f", str) == 0)  return SDL_PIXELFORMAT_UNKNOWN;

    SDL_Log("Unknown format '%s'", str);

    return SDL_PIXELFORMAT_UNKNOWN;
}

static NSString *sdlformat_to_nsfourcc(Uint32 fmt)
{
    const char *str = "";
    NSString *result;

#ifdef SDL_PLATFORM_MACOS
    if (fmt == SDL_PIXELFORMAT_YVYU)  str = "420v";
#else
    if (fmt == SDL_PIXELFORMAT_NV12)  str = "420v";
#endif
    if (fmt == SDL_PIXELFORMAT_UYVY)  str = "yuvs";

    return [[NSString alloc] initWithUTF8String: str];
}


@interface MySampleBufferDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
    @property struct SDL_PrivateCameraData *hidden;
    - (void) set: (struct SDL_PrivateCameraData *) val;
@end

@implementation MySampleBufferDelegate

    - (void) set: (struct SDL_PrivateCameraData *) val {
        _hidden = val;
    }

    - (void) captureOutput:(AVCaptureOutput *)output
        didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
        fromConnection:(AVCaptureConnection *) connection {
            CFRetain(sampleBuffer);
            CMSimpleQueueEnqueue(_hidden->frame_queue, sampleBuffer);
        }

    - (void)captureOutput:(AVCaptureOutput *)output
        didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
        fromConnection:(AVCaptureConnection *)connection {
            // !!! FIXME #if DEBUG_CAMERA
            SDL_Log("Drop frame..");
        }
@end

int OpenDevice(SDL_CameraDevice *_this)
{
    _this->hidden = (struct SDL_PrivateCameraData *) SDL_calloc(1, sizeof (struct SDL_PrivateCameraData));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    return 0;
}

void CloseDevice(SDL_CameraDevice *_this)
{
    if (!_this) {
        return;
    }

    if (_this->hidden) {
        AVCaptureSession *session = _this->hidden->session;

        if (session) {
            AVCaptureInput *input;
            AVCaptureVideoDataOutput *output;
            input = [session.inputs objectAtIndex:0];
            [session removeInput:input];
            output = (AVCaptureVideoDataOutput*)[session.outputs objectAtIndex:0];
            [session removeOutput:output];
            // TODO more cleanup ?
        }

        if (_this->hidden->frame_queue) {
            CFRelease(_this->hidden->frame_queue);
        }

        SDL_free(_this->hidden);
        _this->hidden = NULL;
    }
}

int InitDevice(SDL_CameraDevice *_this)
{
    // !!! FIXME: autorelease pool?
    NSString *fmt = sdlformat_to_nsfourcc(_this->spec.format);
    int w = _this->spec.width;
    int h = _this->spec.height;

    NSError *error = nil;
    AVCaptureDevice *device = nil;
    AVCaptureDeviceInput *input = nil;
    AVCaptureVideoDataOutput *output = nil;

    AVCaptureDeviceFormat *spec_format = nil;

#ifdef SDL_PLATFORM_MACOS
    if (@available(macOS 10.15, *)) {
        // good.
    } else {
        return -1;
    }
#endif

    device = GetCameraDeviceByName(_this->dev_name);
    if (!device) {
        goto error;
    }

    _this->hidden->session = [[AVCaptureSession alloc] init];
    if (_this->hidden->session == nil) {
        goto error;
    }

    [_this->hidden->session setSessionPreset:AVCaptureSessionPresetHigh];

    // Pick format that matches the spec
    NSArray<AVCaptureDeviceFormat *> *formats = [device formats];
    for (AVCaptureDeviceFormat *format in formats) {
        CMFormatDescriptionRef formatDescription = [format formatDescription];
        FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(formatDescription);
        NSString *str = fourcc_to_nstring(mediaSubType);
        if ([str isEqualToString:fmt]) {
            CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(formatDescription);
                if (dim.width == w && dim.height == h) {
                    spec_format = format;
                    break;
                }
            }
        }
    }

    if (spec_format == nil) {
        return SDL_SetError("format not found");
    }

    // Set format
    if ([device lockForConfiguration:NULL] == YES) {
        device.activeFormat = spec_format;
        [device unlockForConfiguration];
    } else {
        return SDL_SetError("Cannot lockForConfiguration");
    }

    // Input
    input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (!input) {
        return SDL_SetError("Cannot create AVCaptureDeviceInput");
    }

    // Output
    output = [[AVCaptureVideoDataOutput alloc] init];

#ifdef SDL_PLATFORM_MACOS
    // FIXME this now fail on ios ... but not using anything works...

    // Specify the pixel format
    output.videoSettings =
        [NSDictionary dictionaryWithObject:
        [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr8]
            forKey:(id)kCVPixelBufferPixelFormatTypeKey];
#endif

    _this->hidden->delegate = [[MySampleBufferDelegate alloc] init];
    [_this->hidden->delegate set:_this->hidden];


    CMSimpleQueueCreate(kCFAllocatorDefault, 30 /* buffers */, &_this->hidden->frame_queue);
    if (_this->hidden->frame_queue == nil) {
        return SDL_SetError("CMSimpleQueueCreate() failed");
    }

    _this->hidden->queue = dispatch_queue_create("my_queue", NULL);
    [output setSampleBufferDelegate:_this->hidden->delegate queue:_this->hidden->queue];

    if ([_this->hidden->session canAddInput:input] ){
        [_this->hidden->session addInput:input];
    } else {
        return SDL_SetError("Cannot add AVCaptureDeviceInput");
    }

    if ([_this->hidden->session canAddOutput:output] ){
        [_this->hidden->session addOutput:output];
    } else {
        return SDL_SetError("Cannot add AVCaptureVideoDataOutput");
    }

    [_this->hidden->session commitConfiguration];

    return 0;
}

int GetDeviceSpec(SDL_CameraDevice *_this, SDL_CameraSpec *spec)
{
    // !!! FIXME: make sure higher level checks spec != NULL
    if (spec) {
        SDL_copyp(spec, &_this->spec);
        return 0;
    }
    return -1;
}

int StartCamera(SDL_CameraDevice *_this)
{
    [_this->hidden->session startRunning];
    return 0;
}

int StopCamera(SDL_CameraDevice *_this)
{
    [_this->hidden->session stopRunning];
    return 0;
}

int AcquireFrame(SDL_CameraDevice *_this, SDL_CameraFrame *frame)
{
    if (CMSimpleQueueGetCount(_this->hidden->frame_queue) > 0) {
        CMSampleBufferRef sampleBuffer = (CMSampleBufferRef)CMSimpleQueueDequeue(_this->hidden->frame_queue);
        frame->internal = (void *) sampleBuffer;
        frame->timestampNS = SDL_GetTicksNS();

        CVImageBufferRef image = CMSampleBufferGetImageBuffer(sampleBuffer);
        const int numPlanes = CVPixelBufferGetPlaneCount(image);
        const int planar = CVPixelBufferIsPlanar(image);

#if 0
        const int w = CVPixelBufferGetWidth(image);
        const int h = CVPixelBufferGetHeight(image);
        const int sz = CVPixelBufferGetDataSize(image);
        const int pitch = CVPixelBufferGetBytesPerRow(image);
        SDL_Log("buffer planar=%d count:%d %d x %d sz=%d pitch=%d", planar, numPlanes, w, h, sz, pitch);
#endif

        CVPixelBufferLockBaseAddress(image, 0);

        if ((planar == 0) && (numPlanes == 0)) {
            frame->pitch[0] = CVPixelBufferGetBytesPerRow(image);
            frame->data[0] = CVPixelBufferGetBaseAddress(image);
            frame->num_planes = 1;
        } else {
            for (int i = 0; (i < numPlanes) && (i < 3); i++) {
                frame->num_planes += 1;
                frame->data[i] = CVPixelBufferGetBaseAddressOfPlane(image, i);
                frame->pitch[i] = CVPixelBufferGetBytesPerRowOfPlane(image, i);
            }
        }

        // Unlocked when frame is released
    } else {
        // no frame
        SDL_Delay(20); // TODO fix some delay
    }
    return 0;
}

int ReleaseFrame(SDL_CameraDevice *_this, SDL_CameraFrame *frame)
{
    if (frame->internal) {
        CMSampleBufferRef sampleBuffer = (CMSampleBufferRef) frame->internal;
        CVImageBufferRef image = CMSampleBufferGetImageBuffer(sampleBuffer);
        CVPixelBufferUnlockBaseAddress(image, 0);
        CFRelease(sampleBuffer);
    }

    return 0;
}

int GetNumFormats(SDL_CameraDevice *_this)
{
    AVCaptureDevice *device = GetCameraDeviceByName(_this->dev_name);
    if (device) {
        // LIST FORMATS
        NSMutableOrderedSet<NSString *> *array_formats = [NSMutableOrderedSet new];
        NSArray<AVCaptureDeviceFormat *> *formats = [device formats];
        for (AVCaptureDeviceFormat *format in formats) {
            // NSLog(@"%@", formats);
            CMFormatDescriptionRef formatDescription = [format formatDescription];
            //NSLog(@"%@", formatDescription);
            FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(formatDescription);
            NSString *str = fourcc_to_nstring(mediaSubType);
            [array_formats addObject:str];
        }
        return [array_formats count];
    }
    return 0;
}

int GetFormat(SDL_CameraDevice *_this, int index, Uint32 *format)
{
    AVCaptureDevice *device = GetCameraDeviceByName(_this->dev_name);
    if (device) {
        // LIST FORMATS
        NSMutableOrderedSet<NSString *> *array_formats = [NSMutableOrderedSet new];
        NSArray<AVCaptureDeviceFormat *> *formats = [device formats];
        NSString *str;

        for (AVCaptureDeviceFormat *f in formats) {
            FourCharCode mediaSubType;
            CMFormatDescriptionRef formatDescription;

            formatDescription = [f formatDescription];
            mediaSubType = CMFormatDescriptionGetMediaSubType(formatDescription);
            str = fourcc_to_nstring(mediaSubType);
            [array_formats addObject:str];
        }

        str = array_formats[index];
        *format = nsfourcc_to_sdlformat(str);

        return 0;
    }
    return -1;
}

int GetNumFrameSizes(SDL_CameraDevice *_this, Uint32 format)
{
    AVCaptureDevice *device = GetCameraDeviceByName(_this->dev_name);
    if (device) {
        NSString *fmt = sdlformat_to_nsfourcc(format);
        int count = 0;

        NSArray<AVCaptureDeviceFormat *> *formats = [device formats];
        for (AVCaptureDeviceFormat *f in formats) {
            CMFormatDescriptionRef formatDescription = [f formatDescription];
            FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(formatDescription);
            NSString *str = fourcc_to_nstring(mediaSubType);

            if ([str isEqualToString:fmt]) {
                count++;
            }
        }
        return count;
    }
    return 0;
}

int
GetFrameSize(SDL_CameraDevice *_this, Uint32 format, int index, int *width, int *height)
{
    AVCaptureDevice *device = GetCameraDeviceByName(_this->dev_name);
    if (device) {
        NSString *fmt = sdlformat_to_nsfourcc(format);
        int count = 0;

        NSArray<AVCaptureDeviceFormat *> *formats = [device formats];
        for (AVCaptureDeviceFormat *f in formats) {
            CMFormatDescriptionRef formatDescription = [f formatDescription];
            FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(formatDescription);
            NSString *str = fourcc_to_nstring(mediaSubType);

            if ([str isEqualToString:fmt]) {
                if (index == count) {
                    CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(formatDescription);
                    *width = dim.width;
                    *height = dim.height;
                    return 0;
                }
                count++;
            }
        }
    }
    return -1;
}

int GetCameraDeviceName(SDL_CameraDeviceID instance_id, char *buf, int size)
{
    int index = instance_id - 1;
    NSArray<AVCaptureDevice *> *devices = DiscoverCameraDevices();
    if (index < [devices count]) {
        AVCaptureDevice *device = devices[index];
        NSString *cameraID = [device localizedName];
        const char *str = [cameraID UTF8String];
        SDL_snprintf(buf, size, "%s", str);
        return 0;
    }
    return -1;
}

static int GetNumDevices(void)
{
    NSArray<AVCaptureDevice *> *devices = DiscoverCameraDevices();
    return [devices count];
}

SDL_CameraDeviceID *GetCameraDevices(int *count)
{
    // hard-coded list of ID
    const int num = GetNumDevices();
    SDL_CameraDeviceID *retval = (SDL_CameraDeviceID *)SDL_calloc((num + 1), sizeof(*ret));

    if (retval == NULL) {
        SDL_OutOfMemory();
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < num; i++) {
        retval[i] = i + 1;
    }
    retval[num] = 0;
    *count = num;
    return ret;
}

int SDL_SYS_CameraInit(void)
{
    return 0;
}

int SDL_SYS_CameraQuit(void)
{
    return 0;
}

#endif // HAVE_COREMEDIA

#endif // SDL_CAMERA_APPLE

