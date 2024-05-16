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

// the Windows Media Foundation API

#ifdef SDL_CAMERA_DRIVER_MEDIAFOUNDATION

#define COBJMACROS

// this seems to be a bug in mfidl.h, just define this to avoid the problem section.
#define __IMFVideoProcessorControl3_INTERFACE_DEFINED__

#include "../../core/windows/SDL_windows.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"

static const IID SDL_IID_IMFMediaSource = { 0x279a808d, 0xaec7, 0x40c8, { 0x9c, 0x6b, 0xa6, 0xb4, 0x92, 0xc7, 0x8a, 0x66 } };
static const IID SDL_IID_IMF2DBuffer = { 0x7dc9d5f9, 0x9ed9, 0x44ec, { 0x9b, 0xbf, 0x06, 0x00, 0xbb, 0x58, 0x9f, 0xbb } };
static const IID SDL_IID_IMF2DBuffer2 = { 0x33ae5ea6, 0x4316, 0x436f, { 0x8d, 0xdd, 0xd7, 0x3d, 0x22, 0xf8, 0x29, 0xec } };
static const GUID SDL_MF_MT_DEFAULT_STRIDE = { 0x644b4e48, 0x1e02, 0x4516, { 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6 } };
static const GUID SDL_MF_MT_MAJOR_TYPE = { 0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f } };
static const GUID SDL_MF_MT_SUBTYPE = { 0xf7e34c9a, 0x42e8, 0x4714, { 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5 } };
static const GUID SDL_MF_MT_FRAME_SIZE = { 0x1652c33d, 0xd6b2, 0x4012, { 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d } };
static const GUID SDL_MF_MT_FRAME_RATE = { 0xc459a2e8, 0x3d2c, 0x4e44, { 0xb1, 0x32, 0xfe, 0xe5, 0x15, 0x6c, 0x7b, 0xb0 } };
static const GUID SDL_MFMediaType_Video = { 0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
#endif

#define SDL_DEFINE_MEDIATYPE_GUID(name, fmt) static const GUID SDL_##name = { fmt, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } }
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_RGB555, 24);
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_RGB565, 23);
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_RGB24, 20);
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_RGB32, 22);
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_ARGB32, 21);
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_A2R10G10B10, 31);
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_YV12, FCC('YV12'));
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_IYUV, FCC('IYUV'));
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_YUY2, FCC('YUY2'));
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_UYVY, FCC('UYVY'));
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_YVYU, FCC('YVYU'));
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_NV12, FCC('NV12'));
SDL_DEFINE_MEDIATYPE_GUID(MFVideoFormat_NV21, FCC('NV21'));
#undef SDL_DEFINE_MEDIATYPE_GUID

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static const struct
{
    const GUID *guid;
    const SDL_PixelFormatEnum sdlfmt;
} fmtmappings[] = {
    // This is not every possible format, just popular ones that SDL can reasonably handle.
    // (and we should probably trim this list more.)
    { &SDL_MFVideoFormat_RGB555, SDL_PIXELFORMAT_XRGB1555 },
    { &SDL_MFVideoFormat_RGB565, SDL_PIXELFORMAT_RGB565 },
    { &SDL_MFVideoFormat_RGB24, SDL_PIXELFORMAT_RGB24 },
    { &SDL_MFVideoFormat_RGB32, SDL_PIXELFORMAT_XRGB8888 },
    { &SDL_MFVideoFormat_ARGB32, SDL_PIXELFORMAT_ARGB8888 },
    { &SDL_MFVideoFormat_A2R10G10B10, SDL_PIXELFORMAT_ARGB2101010 },
    { &SDL_MFVideoFormat_YV12, SDL_PIXELFORMAT_YV12 },
    { &SDL_MFVideoFormat_IYUV, SDL_PIXELFORMAT_IYUV },
    { &SDL_MFVideoFormat_YUY2,  SDL_PIXELFORMAT_YUY2 },
    { &SDL_MFVideoFormat_UYVY, SDL_PIXELFORMAT_UYVY },
    { &SDL_MFVideoFormat_YVYU, SDL_PIXELFORMAT_YVYU },
    { &SDL_MFVideoFormat_NV12, SDL_PIXELFORMAT_NV12 },
    { &SDL_MFVideoFormat_NV21, SDL_PIXELFORMAT_NV21 }
};

static SDL_PixelFormatEnum MFVidFmtGuidToSDLFmt(const GUID *guid)
{
    for (size_t i = 0; i < SDL_arraysize(fmtmappings); i++) {
        if (WIN_IsEqualGUID(guid, fmtmappings[i].guid)) {
            return fmtmappings[i].sdlfmt;
        }
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}

static const GUID *SDLFmtToMFVidFmtGuid(SDL_PixelFormatEnum sdlfmt)
{
    for (size_t i = 0; i < SDL_arraysize(fmtmappings); i++) {
        if (fmtmappings[i].sdlfmt == sdlfmt) {
            return fmtmappings[i].guid;
        }
    }
    return NULL;
}


// handle to Media Foundation libs--Vista and later!--for access to the Media Foundation API.

// mf.dll ...
static HMODULE libmf = NULL;
typedef HRESULT(WINAPI *pfnMFEnumDeviceSources)(IMFAttributes *,IMFActivate ***,UINT32 *);
typedef HRESULT(WINAPI *pfnMFCreateDeviceSource)(IMFAttributes  *, IMFMediaSource **);
static pfnMFEnumDeviceSources pMFEnumDeviceSources = NULL;
static pfnMFCreateDeviceSource pMFCreateDeviceSource = NULL;

// mfplat.dll ...
static HMODULE libmfplat = NULL;
typedef HRESULT(WINAPI *pfnMFStartup)(ULONG, DWORD);
typedef HRESULT(WINAPI *pfnMFShutdown)(void);
typedef HRESULT(WINAPI *pfnMFCreateAttributes)(IMFAttributes **, UINT32);
typedef HRESULT(WINAPI *pfnMFCreateMediaType)(IMFMediaType **);
typedef HRESULT(WINAPI *pfnMFGetStrideForBitmapInfoHeader)(DWORD, DWORD, LONG *);

static pfnMFStartup pMFStartup = NULL;
static pfnMFShutdown pMFShutdown = NULL;
static pfnMFCreateAttributes pMFCreateAttributes = NULL;
static pfnMFCreateMediaType pMFCreateMediaType = NULL;
static pfnMFGetStrideForBitmapInfoHeader pMFGetStrideForBitmapInfoHeader = NULL;

// mfreadwrite.dll ...
static HMODULE libmfreadwrite = NULL;
typedef HRESULT(WINAPI *pfnMFCreateSourceReaderFromMediaSource)(IMFMediaSource *, IMFAttributes *, IMFSourceReader **);
static pfnMFCreateSourceReaderFromMediaSource pMFCreateSourceReaderFromMediaSource = NULL;


typedef struct SDL_PrivateCameraData
{
    IMFSourceReader *srcreader;
    IMFSample *current_sample;
    int pitch;
} SDL_PrivateCameraData;

static int MEDIAFOUNDATION_WaitDevice(SDL_CameraDevice *device)
{
    SDL_assert(device->hidden->current_sample == NULL);

    IMFSourceReader *srcreader = device->hidden->srcreader;
    IMFSample *sample = NULL;

    while (!SDL_AtomicGet(&device->shutdown)) {
        DWORD stream_flags = 0;
        const HRESULT ret = IMFSourceReader_ReadSample(srcreader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &stream_flags, NULL, &sample);
        if (FAILED(ret)) {
            return -1;   // ruh roh.
        }

        // we currently ignore stream_flags format changes, but my _hope_ is that IMFSourceReader is handling this and
        // will continue to give us the explictly-specified format we requested when opening the device, though, and
        // we don't have to manually deal with it.

        if (sample != NULL) {
            break;
        } else if (stream_flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)) {
            return -1;  // apparently this camera has gone down.  :/
        }

        // otherwise, there was some minor burp, probably; just try again.
    }

    device->hidden->current_sample = sample;

    return 0;
}


#ifdef KEEP_ACQUIRED_BUFFERS_LOCKED

#define PROP_SURFACE_IMFOBJS_POINTER "SDL.camera.mediafoundation.imfobjs"

typedef struct SDL_IMFObjects
{
    IMF2DBuffer2 *buffer2d2;
    IMF2DBuffer *buffer2d;
    IMFMediaBuffer *buffer;
    IMFSample *sample;
} SDL_IMFObjects;

static void SDLCALL CleanupIMF2DBuffer2(void *userdata, void *value)
{
    SDL_IMFObjects *objs = (SDL_IMFObjects *)value;
    IMF2DBuffer2_Unlock2D(objs->buffer2d2);
    IMF2DBuffer2_Release(objs->buffer2d2);
    IMFMediaBuffer_Release(objs->buffer);
    IMFSample_Release(objs->sample);
    SDL_free(objs);
}

static void SDLCALL CleanupIMF2DBuffer(void *userdata, void *value)
{
    SDL_IMFObjects *objs = (SDL_IMFObjects *)value;
    IMF2DBuffer_Unlock2D(objs->buffer2d);
    IMF2DBuffer_Release(objs->buffer2d);
    IMFMediaBuffer_Release(objs->buffer);
    IMFSample_Release(objs->sample);
    SDL_free(objs);
}

static void SDLCALL CleanupIMFMediaBuffer(void *userdata, void *value)
{
    SDL_IMFObjects *objs = (SDL_IMFObjects *)value;
    IMFMediaBuffer_Unlock(objs->buffer);
    IMFMediaBuffer_Release(objs->buffer);
    IMFSample_Release(objs->sample);
    SDL_free(objs);
}

static int MEDIAFOUNDATION_AcquireFrame(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS)
{
    SDL_assert(device->hidden->current_sample != NULL);

    int retval = 1;
    HRESULT ret;
    LONGLONG timestamp100NS = 0;
    SDL_IMFObjects *objs = (SDL_IMFObjects *) SDL_calloc(1, sizeof (SDL_IMFObjects));

    if (objs == NULL) {
        return -1;
    }

    objs->sample = device->hidden->current_sample;
    device->hidden->current_sample = NULL;

    const SDL_PropertiesID surfprops = SDL_GetSurfaceProperties(frame);
    if (!surfprops) {
        retval = -1;
    } else {
        ret = IMFSample_GetSampleTime(objs->sample, &timestamp100NS);
        if (FAILED(ret)) {
            retval = -1;
        }

        *timestampNS = timestamp100NS * 100;  // the timestamps are in 100-nanosecond increments; move to full nanoseconds.
    }

    ret = (retval < 0) ? E_FAIL : IMFSample_ConvertToContiguousBuffer(objs->sample, &objs->buffer);  /*IMFSample_GetBufferByIndex(objs->sample, 0, &objs->buffer);*/

    if (FAILED(ret)) {
        SDL_free(objs);
        retval = -1;
    } else {
        BYTE *pixels = NULL;
        LONG pitch = 0;

        if (SUCCEEDED(IMFMediaBuffer_QueryInterface(objs->buffer, &SDL_IID_IMF2DBuffer2, (void **)&objs->buffer2d2))) {
            BYTE *bufstart = NULL;
            DWORD buflen = 0;
            ret = IMF2DBuffer2_Lock2DSize(objs->buffer2d2, MF2DBuffer_LockFlags_Read, &pixels, &pitch, &bufstart, &buflen);
            if (FAILED(ret)) {
                retval = -1;
                CleanupIMF2DBuffer2(NULL, objs);
            } else {
                frame->pixels = pixels;
                frame->pitch = (int) pitch;
                if (SDL_SetPropertyWithCleanup(surfprops, PROP_SURFACE_IMFOBJS_POINTER, objs, CleanupIMF2DBuffer2, NULL) == -1) {
                    retval = -1;
                }
            }
        } else if (SUCCEEDED(IMFMediaBuffer_QueryInterface(objs->buffer, &SDL_IID_IMF2DBuffer, (void **)&objs->buffer2d))) {
            ret = IMF2DBuffer_Lock2D(objs->buffer2d, &pixels, &pitch);
            if (FAILED(ret)) {
                CleanupIMF2DBuffer(NULL, objs);
                retval = -1;
            } else {
                frame->pixels = pixels;
                frame->pitch = (int) pitch;
                if (SDL_SetPropertyWithCleanup(surfprops, PROP_SURFACE_IMFOBJS_POINTER, objs, CleanupIMF2DBuffer, NULL) == -1) {
                    retval = -1;
                }
            }
        } else {
            DWORD maxlen = 0, currentlen = 0;
            ret = IMFMediaBuffer_Lock(objs->buffer, &pixels, &maxlen, &currentlen);
            if (FAILED(ret)) {
                CleanupIMFMediaBuffer(NULL, objs);
                retval = -1;
            } else {
                pitch = (LONG) device->hidden->pitch;
                if (pitch < 0) {  // image rows are reversed.
                    pixels += -pitch * (frame->h - 1);
                }
                frame->pixels = pixels;
                frame->pitch = (int) pitch;
                if (SDL_SetPropertyWithCleanup(surfprops, PROP_SURFACE_IMFOBJS_POINTER, objs, CleanupIMFMediaBuffer, NULL) == -1) {
                    retval = -1;
                }
            }
        }
    }

    if (retval < 0) {
        *timestampNS = 0;
    }

    return retval;
}

static void MEDIAFOUNDATION_ReleaseFrame(SDL_CameraDevice *device, SDL_Surface *frame)
{
    const SDL_PropertiesID surfprops = SDL_GetSurfaceProperties(frame);
    if (surfprops) {
        // this will release the IMFBuffer and IMFSample objects for this frame.
        SDL_ClearProperty(surfprops, PROP_SURFACE_IMFOBJS_POINTER);
    }
}

#else

static int MEDIAFOUNDATION_AcquireFrame(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS)
{
    SDL_assert(device->hidden->current_sample != NULL);

    int retval = 1;
    HRESULT ret;
    LONGLONG timestamp100NS = 0;

    IMFSample *sample = device->hidden->current_sample;
    device->hidden->current_sample = NULL;

    const SDL_PropertiesID surfprops = SDL_GetSurfaceProperties(frame);
    if (!surfprops) {
        retval = -1;
    } else {
        ret = IMFSample_GetSampleTime(sample, &timestamp100NS);
        if (FAILED(ret)) {
            retval = -1;
        }

        *timestampNS = timestamp100NS * 100; // the timestamps are in 100-nanosecond increments; move to full nanoseconds.
    }

    IMFMediaBuffer *buffer = NULL;
    ret = (retval < 0) ? E_FAIL : IMFSample_ConvertToContiguousBuffer(sample, &buffer); /*IMFSample_GetBufferByIndex(sample, 0, &buffer);*/

    if (FAILED(ret)) {
        retval = -1;
    } else {
        IMF2DBuffer *buffer2d = NULL;
        IMF2DBuffer2 *buffer2d2 = NULL;
        BYTE *pixels = NULL;
        LONG pitch = 0;

        if (SUCCEEDED(IMFMediaBuffer_QueryInterface(buffer, &SDL_IID_IMF2DBuffer2, (void **)&buffer2d2))) {
            BYTE *bufstart = NULL;
            DWORD buflen = 0;
            ret = IMF2DBuffer2_Lock2DSize(buffer2d2, MF2DBuffer_LockFlags_Read, &pixels, &pitch, &bufstart, &buflen);
            if (FAILED(ret)) {
                retval = -1;
            } else {
                frame->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), buflen);
                if (frame->pixels == NULL) {
                    retval = -1;
                } else {
                    SDL_memcpy(frame->pixels, pixels, buflen);
                    frame->pitch = (int)pitch;
                }
                IMF2DBuffer2_Unlock2D(buffer2d2);
            }
            IMF2DBuffer2_Release(buffer2d2);
        } else if (SUCCEEDED(IMFMediaBuffer_QueryInterface(buffer, &SDL_IID_IMF2DBuffer, (void **)&buffer2d))) {
            ret = IMF2DBuffer_Lock2D(buffer2d, &pixels, &pitch);
            if (FAILED(ret)) {
                retval = -1;
            } else {
                BYTE *bufstart = pixels;
                const DWORD buflen = (SDL_abs((int)pitch) * frame->w) * frame->h;
                if (pitch < 0) { // image rows are reversed.
                    bufstart += -pitch * (frame->h - 1);
                }
                frame->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), buflen);
                if (frame->pixels == NULL) {
                    retval = -1;
                } else {
                    SDL_memcpy(frame->pixels, bufstart, buflen);
                    frame->pitch = (int)pitch;
                }
                IMF2DBuffer_Unlock2D(buffer2d);
            }
            IMF2DBuffer_Release(buffer2d);
        } else {
            DWORD maxlen = 0, currentlen = 0;
            ret = IMFMediaBuffer_Lock(buffer, &pixels, &maxlen, &currentlen);
            if (FAILED(ret)) {
                retval = -1;
            } else {
                BYTE *bufstart = pixels;
                pitch = (LONG)device->hidden->pitch;
                const DWORD buflen = (SDL_abs((int)pitch) * frame->w) * frame->h;
                if (pitch < 0) { // image rows are reversed.
                    bufstart += -pitch * (frame->h - 1);
                }
                frame->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), buflen);
                if (frame->pixels == NULL) {
                    retval = -1;
                } else {
                    SDL_memcpy(frame->pixels, bufstart, buflen);
                    frame->pitch = (int)pitch;
                }
                IMFMediaBuffer_Unlock(buffer);
            }
        }
        IMFMediaBuffer_Release(buffer);
    }

    IMFSample_Release(sample);

    if (retval < 0) {
        *timestampNS = 0;
    }

    return retval;
}

static void MEDIAFOUNDATION_ReleaseFrame(SDL_CameraDevice *device, SDL_Surface *frame)
{
    SDL_aligned_free(frame->pixels);
}

#endif

static void MEDIAFOUNDATION_CloseDevice(SDL_CameraDevice *device)
{
    if (device && device->hidden) {
        if (device->hidden->srcreader) {
            IMFSourceReader_Release(device->hidden->srcreader);
        }
        if (device->hidden->current_sample) {
            IMFSample_Release(device->hidden->current_sample);
        }
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

// this function is from https://learn.microsoft.com/en-us/windows/win32/medfound/uncompressed-video-buffers
static HRESULT GetDefaultStride(IMFMediaType *pType, LONG *plStride)
{
    LONG lStride = 0;

    // Try to get the default stride from the media type.
    HRESULT ret = IMFMediaType_GetUINT32(pType, &SDL_MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
    if (FAILED(ret)) {
        // Attribute not set. Try to calculate the default stride.

        GUID subtype = GUID_NULL;
        UINT32 width = 0;
        /* UINT32 height = 0; */
        UINT64 val = 0;

        // Get the subtype and the image size.
        ret = IMFMediaType_GetGUID(pType, &SDL_MF_MT_SUBTYPE, &subtype);
        if (FAILED(ret)) {
            goto done;
        }

        ret = IMFMediaType_GetUINT64(pType, &SDL_MF_MT_FRAME_SIZE, &val);
        if (FAILED(ret)) {
            goto done;
        }

        width = (UINT32) (val >> 32);
        /* height = (UINT32) val; */

        ret = pMFGetStrideForBitmapInfoHeader(subtype.Data1, width, &lStride);
        if (FAILED(ret)) {
            goto done;
        }

        // Set the attribute for later reference.
        IMFMediaType_SetUINT32(pType, &SDL_MF_MT_DEFAULT_STRIDE, (UINT32) lStride);
    }

    if (SUCCEEDED(ret)) {
        *plStride = lStride;
    }

done:
    return ret;
}


static int MEDIAFOUNDATION_OpenDevice(SDL_CameraDevice *device, const SDL_CameraSpec *spec)
{
    const char *utf8symlink = (const char *) device->handle;
    IMFAttributes *attrs = NULL;
    LPWSTR wstrsymlink = NULL;
    IMFMediaSource *source = NULL;
    IMFMediaType *mediatype = NULL;
    IMFSourceReader *srcreader = NULL;
#if 0
    DWORD num_streams = 0;
#endif
    LONG lstride = 0;
    //PROPVARIANT var;
    HRESULT ret;

    #if 0
    IMFStreamDescriptor *streamdesc = NULL;
    IMFPresentationDescriptor *presentdesc = NULL;
    IMFMediaTypeHandler *handler = NULL;
    #endif

    #if DEBUG_CAMERA
    SDL_Log("CAMERA: opening device with symlink of '%s'", utf8symlink);
    #endif

    wstrsymlink = WIN_UTF8ToString(utf8symlink);
    if (!wstrsymlink) {
        goto failed;
    }

    #define CHECK_HRESULT(what, r) if (FAILED(r)) { WIN_SetErrorFromHRESULT(what " failed", r); goto failed; }

    ret = pMFCreateAttributes(&attrs, 1);
    CHECK_HRESULT("MFCreateAttributes", ret);

    ret = IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    CHECK_HRESULT("IMFAttributes_SetGUID(srctype)", ret);

    ret = IMFAttributes_SetString(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, wstrsymlink);
    CHECK_HRESULT("IMFAttributes_SetString(symlink)", ret);

    ret = pMFCreateDeviceSource(attrs, &source);
    CHECK_HRESULT("MFCreateDeviceSource", ret);

    IMFAttributes_Release(attrs);
    SDL_free(wstrsymlink);
    attrs = NULL;
    wstrsymlink = NULL;

    // !!! FIXME: I think it'd be nice to do this without an IMFSourceReader,
    // since it's just utility code that has to handle more complex media streams
    // than we're dealing with, but this will do for now. The docs are slightly
    // insistent that you should use one, though...Maybe it's extremely hard
    // to handle directly at the IMFMediaSource layer...?
    ret = pMFCreateSourceReaderFromMediaSource(source, NULL, &srcreader);
    CHECK_HRESULT("MFCreateSourceReaderFromMediaSource", ret);

    // !!! FIXME: do we actually have to find the media type object in the source reader or can we just roll our own like this?
    ret = pMFCreateMediaType(&mediatype);
    CHECK_HRESULT("MFCreateMediaType", ret);

    ret = IMFMediaType_SetGUID(mediatype, &SDL_MF_MT_MAJOR_TYPE, &SDL_MFMediaType_Video);
    CHECK_HRESULT("IMFMediaType_SetGUID(major_type)", ret);

    ret = IMFMediaType_SetGUID(mediatype, &SDL_MF_MT_SUBTYPE, SDLFmtToMFVidFmtGuid(spec->format));
    CHECK_HRESULT("IMFMediaType_SetGUID(subtype)", ret);

    ret = IMFMediaType_SetUINT64(mediatype, &SDL_MF_MT_FRAME_SIZE, (((UINT64)spec->width) << 32) | ((UINT64)spec->height));
    CHECK_HRESULT("MFSetAttributeSize(frame_size)", ret);

    ret = IMFMediaType_SetUINT64(mediatype, &SDL_MF_MT_FRAME_RATE, (((UINT64)spec->interval_numerator) << 32) | ((UINT64)spec->interval_denominator));
    CHECK_HRESULT("MFSetAttributeRatio(frame_rate)", ret);

    ret = IMFSourceReader_SetCurrentMediaType(srcreader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mediatype);
    CHECK_HRESULT("IMFSourceReader_SetCurrentMediaType", ret);

    #if 0  // this (untested thing) is what we would do to get started with a IMFMediaSource that _doesn't_ use IMFSourceReader...
    ret = IMFMediaSource_CreatePresentationDescriptor(source, &presentdesc);
    CHECK_HRESULT("IMFMediaSource_CreatePresentationDescriptor", ret);

    ret = IMFPresentationDescriptor_GetStreamDescriptorCount(presentdesc, &num_streams);
    CHECK_HRESULT("IMFPresentationDescriptor_GetStreamDescriptorCount", ret);

    for (DWORD i = 0; i < num_streams; i++) {
        BOOL selected = FALSE;
        ret = IMFPresentationDescriptor_GetStreamDescriptorByIndex(presentdesc, i, &selected, &streamdesc);
        CHECK_HRESULT("IMFPresentationDescriptor_GetStreamDescriptorByIndex", ret);

        if (selected) {
            ret = IMFStreamDescriptor_GetMediaTypeHandler(streamdesc, &handler);
            CHECK_HRESULT("IMFStreamDescriptor_GetMediaTypeHandler", ret);
            IMFMediaTypeHandler_SetCurrentMediaType(handler, mediatype);
            IMFMediaTypeHandler_Release(handler);
            handler = NULL;
        }

        IMFStreamDescriptor_Release(streamdesc);
        streamdesc = NULL;
    }

    PropVariantInit(&var);
    var.vt = VT_EMPTY;
    ret = IMFMediaSource_Start(source, presentdesc, NULL, &var);
    PropVariantClear(&var);
    CHECK_HRESULT("IMFMediaSource_Start", ret);

    IMFPresentationDescriptor_Release(presentdesc);
    presentdesc = NULL;
    #endif

    ret = GetDefaultStride(mediatype, &lstride);
    CHECK_HRESULT("GetDefaultStride", ret);

    IMFMediaType_Release(mediatype);
    mediatype = NULL;

    device->hidden = (SDL_PrivateCameraData *) SDL_calloc(1, sizeof (SDL_PrivateCameraData));
    if (!device->hidden) {
        goto failed;
    }

    device->hidden->pitch = (int) lstride;
    device->hidden->srcreader = srcreader;
    IMFMediaSource_Release(source);  // srcreader is holding a reference to this.

    // There is no user permission prompt for camera access (I think?)
    SDL_CameraDevicePermissionOutcome(device, SDL_TRUE);

    #undef CHECK_HRESULT

    return 0;

failed:

    if (srcreader) {
        IMFSourceReader_Release(srcreader);
    }

    #if 0
    if (handler) {
        IMFMediaTypeHandler_Release(handler);
    }

    if (streamdesc) {
        IMFStreamDescriptor_Release(streamdesc);
    }

    if (presentdesc) {
        IMFPresentationDescriptor_Release(presentdesc);
    }
    #endif

    if (source) {
        IMFMediaSource_Shutdown(source);
        IMFMediaSource_Release(source);
    }

    if (mediatype) {
        IMFMediaType_Release(mediatype);
    }

    if (attrs) {
        IMFAttributes_Release(attrs);
    }
    SDL_free(wstrsymlink);

    return -1;
}

static void MEDIAFOUNDATION_FreeDeviceHandle(SDL_CameraDevice *device)
{
    if (device) {
        SDL_free(device->handle);  // the device's symlink string.
    }
}

static char *QueryActivationObjectString(IMFActivate *activation, const GUID *pguid)
{
    LPWSTR wstr = NULL;
    UINT32 wlen = 0;
    HRESULT ret = IMFActivate_GetAllocatedString(activation, pguid, &wstr, &wlen);
    if (FAILED(ret)) {
        return NULL;
    }

    char *utf8str = WIN_StringToUTF8(wstr);
    CoTaskMemFree(wstr);
    return utf8str;
}

static void GatherCameraSpecs(IMFMediaSource *source, CameraFormatAddData *add_data)
{
    HRESULT ret;

    // this has like a thousand steps.  :/

    SDL_zerop(add_data);

    IMFPresentationDescriptor *presentdesc = NULL;
    ret = IMFMediaSource_CreatePresentationDescriptor(source, &presentdesc);
    if (FAILED(ret) || !presentdesc) {
        return;
    }

    DWORD num_streams = 0;
    ret = IMFPresentationDescriptor_GetStreamDescriptorCount(presentdesc, &num_streams);
    if (FAILED(ret)) {
        num_streams = 0;
    }

    for (DWORD i = 0; i < num_streams; i++) {
        IMFStreamDescriptor *streamdesc = NULL;
        BOOL selected = FALSE;
        ret = IMFPresentationDescriptor_GetStreamDescriptorByIndex(presentdesc, i, &selected, &streamdesc);
        if (FAILED(ret) || !streamdesc) {
            continue;
        }

        if (selected) {
            IMFMediaTypeHandler *handler = NULL;
            ret = IMFStreamDescriptor_GetMediaTypeHandler(streamdesc, &handler);
            if (SUCCEEDED(ret) && handler) {
                DWORD num_mediatype = 0;
                ret = IMFMediaTypeHandler_GetMediaTypeCount(handler, &num_mediatype);
                if (FAILED(ret)) {
                    num_mediatype = 0;
                }

                for (DWORD j = 0; j < num_mediatype; j++) {
                    IMFMediaType *mediatype = NULL;
                    ret = IMFMediaTypeHandler_GetMediaTypeByIndex(handler, j, &mediatype);
                    if (SUCCEEDED(ret) && mediatype) {
                        GUID type;
                        ret = IMFMediaType_GetGUID(mediatype, &SDL_MF_MT_MAJOR_TYPE, &type);
                        if (SUCCEEDED(ret) && WIN_IsEqualGUID(&type, &SDL_MFMediaType_Video)) {
                            ret = IMFMediaType_GetGUID(mediatype, &SDL_MF_MT_SUBTYPE, &type);
                            if (SUCCEEDED(ret)) {
                                const SDL_PixelFormatEnum sdlfmt = MFVidFmtGuidToSDLFmt(&type);
                                if (sdlfmt != SDL_PIXELFORMAT_UNKNOWN) {
                                    UINT64 val = 0;
                                    UINT32 w = 0, h = 0;
                                    ret = IMFMediaType_GetUINT64(mediatype, &SDL_MF_MT_FRAME_SIZE, &val);
                                    w = (UINT32)(val >> 32);
                                    h = (UINT32)val;
                                    if (SUCCEEDED(ret) && w && h) {
                                        UINT32 interval_numerator = 0, interval_denominator = 0;
                                        ret = IMFMediaType_GetUINT64(mediatype, &SDL_MF_MT_FRAME_RATE, &val);
                                        interval_numerator = (UINT32)(val >> 32);
                                        interval_denominator = (UINT32)val;
                                        if (SUCCEEDED(ret) && interval_numerator && interval_denominator) {
                                            SDL_AddCameraFormat(add_data, sdlfmt, (int) w, (int) h, (int) interval_numerator, (int) interval_denominator);  // whew.
                                        }
                                    }
                                }
                            }
                        }
                        IMFMediaType_Release(mediatype);
                    }
                }
                IMFMediaTypeHandler_Release(handler);
            }
        }
        IMFStreamDescriptor_Release(streamdesc);
    }

    IMFPresentationDescriptor_Release(presentdesc);
}

static SDL_bool FindMediaFoundationCameraDeviceBySymlink(SDL_CameraDevice *device, void *userdata)
{
    return (SDL_strcmp((const char *) device->handle, (const char *) userdata) == 0);
}

static void MaybeAddDevice(IMFActivate *activation)
{
    char *symlink = QueryActivationObjectString(activation, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);

    if (SDL_FindPhysicalCameraDeviceByCallback(FindMediaFoundationCameraDeviceBySymlink, symlink)) {
        SDL_free(symlink);
        return;  // already have this one.
    }

    char *name = QueryActivationObjectString(activation, &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
    if (name && symlink) {
        IMFMediaSource *source = NULL;
        // "activating" here only creates an object, it doesn't open the actual camera hardware or start recording.
        HRESULT ret = IMFActivate_ActivateObject(activation, &SDL_IID_IMFMediaSource, (void**)&source);
        if (SUCCEEDED(ret) && source) {
            CameraFormatAddData add_data;
            GatherCameraSpecs(source, &add_data);
            if (add_data.num_specs > 0) {
                SDL_AddCameraDevice(name, SDL_CAMERA_POSITION_UNKNOWN, add_data.num_specs, add_data.specs, symlink);
            }
            SDL_free(add_data.specs);
            IMFActivate_ShutdownObject(activation);
            IMFMediaSource_Release(source);
        }
    }

    SDL_free(name);
}

static void MEDIAFOUNDATION_DetectDevices(void)
{
    // !!! FIXME: use CM_Register_Notification (Win8+) to get device notifications.
    // !!! FIXME: Earlier versions can use RegisterDeviceNotification, but I'm not bothering: no hotplug for you!
    HRESULT ret;

    IMFAttributes *attrs = NULL;
    ret = pMFCreateAttributes(&attrs, 1);
    if (FAILED(ret)) {
        return;  // oh well, no cameras for you.
    }

    // !!! FIXME: We need these GUIDs hardcoded in this file.
    ret = IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(ret)) {
        IMFAttributes_Release(attrs);
        return;  // oh well, no cameras for you.
    }

    IMFActivate **activations = NULL;
    UINT32 total = 0;
    ret = pMFEnumDeviceSources(attrs, &activations, &total);
    IMFAttributes_Release(attrs);
    if (FAILED(ret)) {
        return;  // oh well, no cameras for you.
    }

    for (UINT32 i = 0; i < total; i++) {
        MaybeAddDevice(activations[i]);
        IMFActivate_Release(activations[i]);
    }

    CoTaskMemFree(activations);
}

static void MEDIAFOUNDATION_Deinitialize(void)
{
    pMFShutdown();

    FreeLibrary(libmfreadwrite);
    libmfreadwrite = NULL;
    FreeLibrary(libmfplat);
    libmfplat = NULL;
    FreeLibrary(libmf);
    libmf = NULL;

    pMFEnumDeviceSources = NULL;
    pMFCreateDeviceSource = NULL;
    pMFStartup = NULL;
    pMFShutdown = NULL;
    pMFCreateAttributes = NULL;
    pMFCreateMediaType = NULL;
    pMFCreateSourceReaderFromMediaSource = NULL;
    pMFGetStrideForBitmapInfoHeader = NULL;
}

static SDL_bool MEDIAFOUNDATION_Init(SDL_CameraDriverImpl *impl)
{
    // !!! FIXME: slide this off into a subroutine
    HMODULE mf = LoadLibrary(TEXT("Mf.dll")); // this library is available in Vista and later, but also can be on XP with service packs and Windows
    if (!mf) {
        return SDL_FALSE;
    }

    HMODULE mfplat = LoadLibrary(TEXT("Mfplat.dll")); // this library is available in Vista and later. No WinXP, so have to LoadLibrary to use it for now!
    if (!mfplat) {
        FreeLibrary(mf);
        return SDL_FALSE;
    }

    HMODULE mfreadwrite = LoadLibrary(TEXT("Mfreadwrite.dll")); // this library is available in Vista and later. No WinXP, so have to LoadLibrary to use it for now!
    if (!mfreadwrite) {
        FreeLibrary(mfplat);
        FreeLibrary(mf);
        return SDL_FALSE;
    }

    SDL_bool okay = SDL_TRUE;
    #define LOADSYM(lib, fn) if (okay) { p##fn = (pfn##fn) GetProcAddress(lib, #fn); if (!p##fn) { okay = SDL_FALSE; } }
    LOADSYM(mf, MFEnumDeviceSources);
    LOADSYM(mf, MFCreateDeviceSource);
    LOADSYM(mfplat, MFStartup);
    LOADSYM(mfplat, MFShutdown);
    LOADSYM(mfplat, MFCreateAttributes);
    LOADSYM(mfplat, MFCreateMediaType);
    LOADSYM(mfplat, MFGetStrideForBitmapInfoHeader);
    LOADSYM(mfreadwrite, MFCreateSourceReaderFromMediaSource);
    #undef LOADSYM

    if (!okay) {
        FreeLibrary(mfreadwrite);
        FreeLibrary(mfplat);
        FreeLibrary(mf);
    }

    libmf = mf;
    libmfplat = mfplat;
    libmfreadwrite = mfreadwrite;

    const HRESULT ret = pMFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(ret)) {
        FreeLibrary(libmfplat);
        libmfplat = NULL;
        FreeLibrary(libmf);
        libmf = NULL;
        return SDL_FALSE;
    }

    impl->DetectDevices = MEDIAFOUNDATION_DetectDevices;
    impl->OpenDevice = MEDIAFOUNDATION_OpenDevice;
    impl->CloseDevice = MEDIAFOUNDATION_CloseDevice;
    impl->WaitDevice = MEDIAFOUNDATION_WaitDevice;
    impl->AcquireFrame = MEDIAFOUNDATION_AcquireFrame;
    impl->ReleaseFrame = MEDIAFOUNDATION_ReleaseFrame;
    impl->FreeDeviceHandle = MEDIAFOUNDATION_FreeDeviceHandle;
    impl->Deinitialize = MEDIAFOUNDATION_Deinitialize;

    return SDL_TRUE;
}

CameraBootStrap MEDIAFOUNDATION_bootstrap = {
    "mediafoundation", "SDL Windows Media Foundation camera driver", MEDIAFOUNDATION_Init, SDL_FALSE
};

#endif // SDL_CAMERA_DRIVER_MEDIAFOUNDATION

