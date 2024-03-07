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

#ifdef SDL_CAMERA_DRIVER_V4L2

#include <stddef.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>              // low-level i/o
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/videodev2.h>

#ifndef V4L2_CAP_DEVICE_CAPS
// device_caps was added to struct v4l2_capability as of kernel 3.4.
#define device_caps reserved[0]
SDL_COMPILE_TIME_ASSERT(v4l2devicecaps, offsetof(struct v4l2_capability,device_caps) == offsetof(struct v4l2_capability,capabilities) + 4);
#endif

#include "../SDL_syscamera.h"
#include "../SDL_camera_c.h"
#include "../../video/SDL_pixels_c.h"
#include "../../thread/SDL_systhread.h"
#include "../../core/linux/SDL_evdev_capabilities.h"
#include "../../core/linux/SDL_udev.h"

#ifndef SDL_USE_LIBUDEV
#include <dirent.h>
#endif

typedef struct V4L2DeviceHandle
{
    char *bus_info;
    char *path;
} V4L2DeviceHandle;


typedef enum io_method {
    IO_METHOD_INVALID,
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR
} io_method;

struct buffer {
    void   *start;
    size_t  length;
    int available; // Is available in userspace
};

struct SDL_PrivateCameraData
{
    int fd;
    io_method io;
    int nb_buffers;
    struct buffer *buffers;
    int driver_pitch;
};

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while ((r == -1) && (errno == EINTR));

    return r;
}

static int V4L2_WaitDevice(SDL_CameraDevice *device)
{
    const int fd = device->hidden->fd;

    int retval;

    do {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;

        retval = select(fd + 1, &fds, NULL, NULL, &tv);
        if ((retval == -1) && (errno == EINTR)) {
            retval = 0;  // pretend it was a timeout, keep looping.
        }
    } while (retval == 0);

    return retval;
}

static int V4L2_AcquireFrame(SDL_CameraDevice *device, SDL_Surface *frame, Uint64 *timestampNS)
{
    const int fd = device->hidden->fd;
    const io_method io = device->hidden->io;
    size_t size = device->hidden->buffers[0].length;
    struct v4l2_buffer buf;

    switch (io) {
        case IO_METHOD_READ:
            if (read(fd, device->hidden->buffers[0].start, size) == -1) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        // Could ignore EIO, see spec.
                        // fall through

                    default:
                        return SDL_SetError("read");
                }
            }

            *timestampNS = SDL_GetTicksNS();  // oh well, close enough.
            frame->pixels = device->hidden->buffers[0].start;
            frame->pitch = device->hidden->driver_pitch;
            break;

        case IO_METHOD_MMAP:
            SDL_zero(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        // Could ignore EIO, see spec.
                        // fall through

                    default:
                        return SDL_SetError("VIDIOC_DQBUF: %d", errno);
                }
            }

            if ((int)buf.index < 0 || (int)buf.index >= device->hidden->nb_buffers) {
                return SDL_SetError("invalid buffer index");
            }

            frame->pixels = device->hidden->buffers[buf.index].start;
            frame->pitch = device->hidden->driver_pitch;
            device->hidden->buffers[buf.index].available = 1;

            *timestampNS = (((Uint64) buf.timestamp.tv_sec) * SDL_NS_PER_SECOND) + SDL_US_TO_NS(buf.timestamp.tv_usec);

            #if DEBUG_CAMERA
            SDL_Log("CAMERA: debug mmap: image %d/%d  data[0]=%p", buf.index, device->hidden->nb_buffers, (void*)frame->pixels);
            #endif
            break;

        case IO_METHOD_USERPTR:
            SDL_zero(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        // Could ignore EIO, see spec.

                        // fall through

                    default:
                        return SDL_SetError("VIDIOC_DQBUF");
                }
            }

            int i;
            for (i = 0; i < device->hidden->nb_buffers; ++i) {
                if (buf.m.userptr == (unsigned long)device->hidden->buffers[i].start && buf.length == size) {
                    break;
                }
            }

            if (i >= device->hidden->nb_buffers) {
                return SDL_SetError("invalid buffer index");
            }

            frame->pixels = (void*)buf.m.userptr;
            frame->pitch = device->hidden->driver_pitch;
            device->hidden->buffers[i].available = 1;

            *timestampNS = (((Uint64) buf.timestamp.tv_sec) * SDL_NS_PER_SECOND) + SDL_US_TO_NS(buf.timestamp.tv_usec);

            #if DEBUG_CAMERA
            SDL_Log("CAMERA: debug userptr: image %d/%d  data[0]=%p", buf.index, device->hidden->nb_buffers, (void*)frame->pixels);
            #endif
            break;

        case IO_METHOD_INVALID:
            SDL_assert(!"Shouldn't have hit this");
            break;
    }

    return 1;
}

static void V4L2_ReleaseFrame(SDL_CameraDevice *device, SDL_Surface *frame)
{
    struct v4l2_buffer buf;
    const int fd = device->hidden->fd;
    const io_method io = device->hidden->io;
    int i;

    for (i = 0; i < device->hidden->nb_buffers; ++i) {
        if (frame->pixels == device->hidden->buffers[i].start) {
            break;
        }
    }

    if (i >= device->hidden->nb_buffers) {
        return;  // oh well, we didn't own this.
    }

    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
            SDL_zero(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                // !!! FIXME: disconnect the device.
                return; //SDL_SetError("VIDIOC_QBUF");
            }
            device->hidden->buffers[i].available = 0;
            break;

        case IO_METHOD_USERPTR:
            SDL_zero(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = i;
            buf.m.userptr = (unsigned long)frame->pixels;
            buf.length = (int) device->hidden->buffers[i].length;

            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                // !!! FIXME: disconnect the device.
                return; //SDL_SetError("VIDIOC_QBUF");
            }
            device->hidden->buffers[i].available = 0;
            break;

        case IO_METHOD_INVALID:
            SDL_assert(!"Shouldn't have hit this");
            break;
    }
}

static int EnqueueBuffers(SDL_CameraDevice *device)
{
    const int fd = device->hidden->fd;
    const io_method io = device->hidden->io;
    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
            for (int i = 0; i < device->hidden->nb_buffers; ++i) {
                if (device->hidden->buffers[i].available == 0) {
                    struct v4l2_buffer buf;

                    SDL_zero(buf);
                    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    buf.memory = V4L2_MEMORY_MMAP;
                    buf.index = i;

                    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                        return SDL_SetError("VIDIOC_QBUF");
                    }
                }
            }
            break;

        case IO_METHOD_USERPTR:
            for (int i = 0; i < device->hidden->nb_buffers; ++i) {
                if (device->hidden->buffers[i].available == 0) {
                    struct v4l2_buffer buf;

                    SDL_zero(buf);
                    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    buf.memory = V4L2_MEMORY_USERPTR;
                    buf.index = i;
                    buf.m.userptr = (unsigned long)device->hidden->buffers[i].start;
                    buf.length = (int) device->hidden->buffers[i].length;

                    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                        return SDL_SetError("VIDIOC_QBUF");
                    }
                }
            }
            break;

        case IO_METHOD_INVALID: SDL_assert(!"Shouldn't have hit this"); break;
    }
    return 0;
}

static int AllocBufferRead(SDL_CameraDevice *device, size_t buffer_size)
{
    device->hidden->buffers[0].length = buffer_size;
    device->hidden->buffers[0].start = SDL_calloc(1, buffer_size);
    return device->hidden->buffers[0].start ? 0 : -1;
}

static int AllocBufferMmap(SDL_CameraDevice *device)
{
    const int fd = device->hidden->fd;
    int i;
    for (i = 0; i < device->hidden->nb_buffers; ++i) {
        struct v4l2_buffer buf;

        SDL_zero(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            return SDL_SetError("VIDIOC_QUERYBUF");
        }

        device->hidden->buffers[i].length = buf.length;
        device->hidden->buffers[i].start =
            mmap(NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    fd, buf.m.offset);

        if (MAP_FAILED == device->hidden->buffers[i].start) {
            return SDL_SetError("mmap");
        }
    }
    return 0;
}

static int AllocBufferUserPtr(SDL_CameraDevice *device, size_t buffer_size)
{
    int i;
    for (i = 0; i < device->hidden->nb_buffers; ++i) {
        device->hidden->buffers[i].length = buffer_size;
        device->hidden->buffers[i].start = SDL_calloc(1, buffer_size);

        if (!device->hidden->buffers[i].start) {
            return -1;
        }
    }
    return 0;
}

static Uint32 format_v4l2_to_sdl(Uint32 fmt)
{
    switch (fmt) {
        #define CASE(x, y)  case x: return y
        CASE(V4L2_PIX_FMT_YUYV, SDL_PIXELFORMAT_YUY2);
        CASE(V4L2_PIX_FMT_MJPEG, SDL_PIXELFORMAT_UNKNOWN);
        #undef CASE
        default:
            #if DEBUG_CAMERA
            SDL_Log("CAMERA: Unknown format V4L2_PIX_FORMAT '%d'", fmt);
            #endif
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static Uint32 format_sdl_to_v4l2(Uint32 fmt)
{
    switch (fmt) {
        #define CASE(y, x)  case x: return y
        CASE(V4L2_PIX_FMT_YUYV, SDL_PIXELFORMAT_YUY2);
        CASE(V4L2_PIX_FMT_MJPEG, SDL_PIXELFORMAT_UNKNOWN);
        #undef CASE
        default:
            return 0;
    }
}

static void V4L2_CloseDevice(SDL_CameraDevice *device)
{
    if (!device) {
        return;
    }

    if (device->hidden) {
        const io_method io = device->hidden->io;
        const int fd = device->hidden->fd;

        if ((io == IO_METHOD_MMAP) || (io == IO_METHOD_USERPTR)) {
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(fd, VIDIOC_STREAMOFF, &type);
        }

        if (device->hidden->buffers) {
            switch (io) {
                case IO_METHOD_INVALID:
                    break;

                case IO_METHOD_READ:
                    SDL_free(device->hidden->buffers[0].start);
                    break;

                case IO_METHOD_MMAP:
                    for (int i = 0; i < device->hidden->nb_buffers; ++i) {
                        if (munmap(device->hidden->buffers[i].start, device->hidden->buffers[i].length) == -1) {
                            SDL_SetError("munmap");
                        }
                    }
                    break;

                case IO_METHOD_USERPTR:
                    for (int i = 0; i < device->hidden->nb_buffers; ++i) {
                        SDL_free(device->hidden->buffers[i].start);
                    }
                    break;
            }

            SDL_free(device->hidden->buffers);
        }

        if (fd != -1) {
            close(fd);
        }
        SDL_free(device->hidden);

        device->hidden = NULL;
    }
}

static int V4L2_OpenDevice(SDL_CameraDevice *device, const SDL_CameraSpec *spec)
{
    const V4L2DeviceHandle *handle = (const V4L2DeviceHandle *) device->handle;
    struct stat st;
    struct v4l2_capability cap;
    const int fd = open(handle->path, O_RDWR /* required */ | O_NONBLOCK, 0);

    // most of this probably shouldn't fail unless the filesystem node changed out from under us since MaybeAddDevice().
    if (fd == -1) {
        return SDL_SetError("Cannot open '%s': %d, %s", handle->path, errno, strerror(errno));
    } else if (fstat(fd, &st) == -1) {
        close(fd);
        return SDL_SetError("Cannot identify '%s': %d, %s", handle->path, errno, strerror(errno));
    } else if (!S_ISCHR(st.st_mode)) {
        close(fd);
        return SDL_SetError("%s is not a character device", handle->path);
    } else if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        const int err = errno;
        close(fd);
        if (err == EINVAL) {
            return SDL_SetError("%s is unexpectedly not a V4L2 device", handle->path);
        }
        return SDL_SetError("Error VIDIOC_QUERYCAP errno=%d device%s is no V4L2 device", err, handle->path);
    } else if ((cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        close(fd);
        return SDL_SetError("%s is unexpectedly not a video capture device", handle->path);
    }

    device->hidden = (struct SDL_PrivateCameraData *) SDL_calloc(1, sizeof (struct SDL_PrivateCameraData));
    if (device->hidden == NULL) {
        close(fd);
        return -1;
    }

    device->hidden->fd = fd;
    device->hidden->io = IO_METHOD_INVALID;

    // Select video input, video standard and tune here.
    // errors in the crop code are not fatal.
    struct v4l2_cropcap cropcap;
    SDL_zero(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0) {
        struct v4l2_crop crop;
        SDL_zero(crop);
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; // reset to default
        xioctl(fd, VIDIOC_S_CROP, &crop);
    }

    struct v4l2_format fmt;
    SDL_zero(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = spec->width;
    fmt.fmt.pix.height = spec->height;
    fmt.fmt.pix.pixelformat = format_sdl_to_v4l2(spec->format);
    //fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    #if DEBUG_CAMERA
    SDL_Log("CAMERA: set SDL format %s", SDL_GetPixelFormatName(spec->format));
    { const Uint32 f = fmt.fmt.pix.pixelformat; SDL_Log("CAMERA: set format V4L2_format=%d  %c%c%c%c", f, (f >> 0) & 0xff, (f >> 8) & 0xff, (f >> 16) & 0xff, (f >> 24) & 0xff); }
    #endif

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        return SDL_SetError("Error VIDIOC_S_FMT");
    }

    if (spec->interval_numerator && spec->interval_denominator) {
        struct v4l2_streamparm setfps;
        SDL_zero(setfps);
        setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_G_PARM, &setfps) == 0) {
            if ( (setfps.parm.capture.timeperframe.numerator != spec->interval_numerator) ||
                 (setfps.parm.capture.timeperframe.denominator = spec->interval_denominator) ) {
                setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                setfps.parm.capture.timeperframe.numerator = spec->interval_numerator;
                setfps.parm.capture.timeperframe.denominator = spec->interval_denominator;
                if (xioctl(fd, VIDIOC_S_PARM, &setfps) == -1) {
                    return SDL_SetError("Error VIDIOC_S_PARM");
                }
            }
        }
    }

    SDL_zero(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
        return SDL_SetError("Error VIDIOC_G_FMT");
    }
    device->hidden->driver_pitch = fmt.fmt.pix.bytesperline;

    io_method io = IO_METHOD_INVALID;
    if ((io == IO_METHOD_INVALID) && (cap.device_caps & V4L2_CAP_STREAMING)) {
        struct v4l2_requestbuffers req;
        SDL_zero(req);
        req.count = 8;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if ((xioctl(fd, VIDIOC_REQBUFS, &req) == 0) && (req.count >= 2)) {
            io = IO_METHOD_MMAP;
            device->hidden->nb_buffers = req.count;
        } else {  // mmap didn't work out? Try USERPTR.
            SDL_zero(req);
            req.count = 8;
            req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            req.memory = V4L2_MEMORY_USERPTR;
            if (xioctl(fd, VIDIOC_REQBUFS, &req) == 0) {
                io = IO_METHOD_USERPTR;
                device->hidden->nb_buffers = 8;
            }
        }
    }

    if ((io == IO_METHOD_INVALID) && (cap.device_caps & V4L2_CAP_READWRITE)) {
        io = IO_METHOD_READ;
        device->hidden->nb_buffers = 1;
    }

    if (io == IO_METHOD_INVALID) {
        return SDL_SetError("Don't have a way to talk to this device");
    }

    device->hidden->io = io;

    device->hidden->buffers = SDL_calloc(device->hidden->nb_buffers, sizeof(*device->hidden->buffers));
    if (!device->hidden->buffers) {
        return -1;
    }

    size_t size, pitch;
    SDL_CalculateSurfaceSize(device->spec.format, device->spec.width, device->spec.height, &size, &pitch, SDL_FALSE);

    int rc = 0;
    switch (io) {
        case IO_METHOD_READ:
            rc = AllocBufferRead(device, size);
            break;

        case IO_METHOD_MMAP:
            rc = AllocBufferMmap(device);
            break;

        case IO_METHOD_USERPTR:
            rc = AllocBufferUserPtr(device, size);
            break;

        case IO_METHOD_INVALID:
            SDL_assert(!"Shouldn't have hit this");
            break;
    }

    if (rc < 0) {
        return -1;
    } else if (EnqueueBuffers(device) < 0) {
        return -1;
    } else if (io != IO_METHOD_READ) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
            return SDL_SetError("VIDIOC_STREAMON");
        }
    }

    // Currently there is no user permission prompt for camera access, but maybe there will be a D-Bus portal interface at some point.
    SDL_CameraDevicePermissionOutcome(device, SDL_TRUE);

    return 0;
}

static SDL_bool FindV4L2CameraDeviceByBusInfoCallback(SDL_CameraDevice *device, void *userdata)
{
    const V4L2DeviceHandle *handle = (const V4L2DeviceHandle *) device->handle;
    return (SDL_strcmp(handle->bus_info, (const char *) userdata) == 0);
}

static int AddCameraFormat(const int fd, CameraFormatAddData *data, Uint32 sdlfmt, Uint32 v4l2fmt, int w, int h)
{
    struct v4l2_frmivalenum frmivalenum;
    SDL_zero(frmivalenum);
    frmivalenum.pixel_format = v4l2fmt;
    frmivalenum.width = (Uint32) w;
    frmivalenum.height = (Uint32) h;

    while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum) == 0) {
        if (frmivalenum.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            const int numerator = (int) frmivalenum.discrete.numerator;
            const int denominator = (int) frmivalenum.discrete.denominator;
            #if DEBUG_CAMERA
            const float fps = (float) denominator / (float) numerator;
            SDL_Log("CAMERA:       * Has discrete frame interval (%d / %d), fps=%f", numerator, denominator, fps);
            #endif
            if (SDL_AddCameraFormat(data, sdlfmt, w, h, numerator, denominator) == -1) {
                return -1;  // Probably out of memory; we'll go with what we have, if anything.
            }
            frmivalenum.index++;  // set up for the next one.
        } else if ((frmivalenum.type == V4L2_FRMIVAL_TYPE_STEPWISE) || (frmivalenum.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)) {
            int d = frmivalenum.stepwise.min.denominator;
            // !!! FIXME: should we step by the numerator...?
            for (int n = (int) frmivalenum.stepwise.min.numerator; n <= (int) frmivalenum.stepwise.max.numerator; n += (int) frmivalenum.stepwise.step.numerator) {
                #if DEBUG_CAMERA
                const float fps = (float) d / (float) n;
                SDL_Log("CAMERA:       * Has %s frame interval (%d / %d), fps=%f", (frmivalenum.type == V4L2_FRMIVAL_TYPE_STEPWISE) ? "stepwise" : "continuous", n, d, fps);
                #endif
                if (SDL_AddCameraFormat(data, sdlfmt, w, h, n, d) == -1) {
                    return -1;  // Probably out of memory; we'll go with what we have, if anything.
                }
                d += (int) frmivalenum.stepwise.step.denominator;
            }
            break;
        }
    }

    return 0;
}


static void MaybeAddDevice(const char *path)
{
    if (!path) {
        return;
    }

    struct stat st;
    const int fd = open(path, O_RDWR /* required */ | O_NONBLOCK, 0);
    if (fd == -1) {
        return;  // can't open it? skip it.
    } else if (fstat(fd, &st) == -1) {
        close(fd);
        return;  // can't stat it? skip it.
    } else if (!S_ISCHR(st.st_mode)) {
        close(fd);
        return;  // not a character device.
    }

    struct v4l2_capability vcap;
    const int rc = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
    if (rc != 0) {
        close(fd);
        return;  // probably not a v4l2 device at all.
    } else if ((vcap.device_caps & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        close(fd);
        return;  // not a video capture device.
    } else if (SDL_FindPhysicalCameraDeviceByCallback(FindV4L2CameraDeviceByBusInfoCallback, vcap.bus_info)) {
        close(fd);
        return;  // already have it.
    }

    #if DEBUG_CAMERA
    SDL_Log("CAMERA: V4L2 camera path='%s' bus_info='%s' name='%s'", path, (const char *) vcap.bus_info, vcap.card);
    #endif

    CameraFormatAddData add_data;
    SDL_zero(add_data);

    struct v4l2_fmtdesc fmtdesc;
    SDL_zero(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        const Uint32 sdlfmt = format_v4l2_to_sdl(fmtdesc.pixelformat);

        #if DEBUG_CAMERA
        SDL_Log("CAMERA:   - Has format '%s'%s%s", SDL_GetPixelFormatName(sdlfmt),
                (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED) ? " [EMULATED]" : "",
                (fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED) ? " [COMPRESSED]" : "");
        #endif

        fmtdesc.index++;  // prepare for next iteration.

        if (sdlfmt == SDL_PIXELFORMAT_UNKNOWN) {
            continue;  // unsupported by SDL atm.
        }

        struct v4l2_frmsizeenum frmsizeenum;
        SDL_zero(frmsizeenum);
        frmsizeenum.pixel_format = fmtdesc.pixelformat;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == 0) {
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                const int w = (int) frmsizeenum.discrete.width;
                const int h = (int) frmsizeenum.discrete.height;
                #if DEBUG_CAMERA
                SDL_Log("CAMERA:     * Has discrete size %dx%d", w, h);
                #endif
                if (AddCameraFormat(fd, &add_data, sdlfmt, fmtdesc.pixelformat, w, h) == -1) {
                    break;  // Probably out of memory; we'll go with what we have, if anything.
                }
                frmsizeenum.index++;  // set up for the next one.
            } else if ((frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE) || (frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)) {
                const int minw = (int) frmsizeenum.stepwise.min_width;
                const int minh = (int) frmsizeenum.stepwise.min_height;
                const int maxw = (int) frmsizeenum.stepwise.max_width;
                const int maxh = (int) frmsizeenum.stepwise.max_height;
                const int stepw = (int) frmsizeenum.stepwise.step_width;
                const int steph = (int) frmsizeenum.stepwise.step_height;
                for (int w = minw; w <= maxw; w += stepw) {
                    for (int h = minh; w <= maxh; w += steph) {
                        #if DEBUG_CAMERA
                        SDL_Log("CAMERA:     * Has %s size %dx%d", (frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE) ? "stepwise" : "continuous", w, h);
                        #endif
                        if (AddCameraFormat(fd, &add_data, sdlfmt, fmtdesc.pixelformat, w, h) == -1) {
                            break;  // Probably out of memory; we'll go with what we have, if anything.
                        }
                    }
                }
                break;
            }
        }
    }

    close(fd);

    #if DEBUG_CAMERA
    SDL_Log("CAMERA: (total specs: %d)", add_data.num_specs);
    #endif

    if (add_data.num_specs > 0) {
        V4L2DeviceHandle *handle = (V4L2DeviceHandle *) SDL_calloc(1, sizeof (V4L2DeviceHandle));
        if (handle) {
            handle->path = SDL_strdup(path);
            if (handle->path) {
                handle->bus_info = SDL_strdup((char *)vcap.bus_info);
                if (handle->bus_info) {
                    if (SDL_AddCameraDevice((const char *) vcap.card, SDL_CAMERA_POSITION_UNKNOWN, add_data.num_specs, add_data.specs, handle)) {
                        SDL_free(add_data.specs);
                        return;  // good to go.
                    }
                    SDL_free(handle->bus_info);
                }
                SDL_free(handle->path);
            }
            SDL_free(handle);
        }
    }
    SDL_free(add_data.specs);
}

static void V4L2_FreeDeviceHandle(SDL_CameraDevice *device)
{
    if (device) {
        V4L2DeviceHandle *handle = (V4L2DeviceHandle *) device->handle;
        SDL_free(handle->path);
        SDL_free(handle->bus_info);
        SDL_free(handle);
    }
}

#ifdef SDL_USE_LIBUDEV
static SDL_bool FindV4L2CameraDeviceByPathCallback(SDL_CameraDevice *device, void *userdata)
{
    const V4L2DeviceHandle *handle = (const V4L2DeviceHandle *) device->handle;
    return (SDL_strcmp(handle->path, (const char *) userdata) == 0);
}

static void MaybeRemoveDevice(const char *path)
{
    if (path) {
        SDL_CameraDeviceDisconnected(SDL_FindPhysicalCameraDeviceByCallback(FindV4L2CameraDeviceByPathCallback, (void *) path));
    }
}

static void CameraUdevCallback(SDL_UDEV_deviceevent udev_type, int udev_class, const char *devpath)
{
    if (devpath && (udev_class & SDL_UDEV_DEVICE_VIDEO_CAPTURE)) {
        if (udev_type == SDL_UDEV_DEVICEADDED) {
            MaybeAddDevice(devpath);
        } else if (udev_type == SDL_UDEV_DEVICEREMOVED) {
            MaybeRemoveDevice(devpath);
        }
    }
}
#endif // SDL_USE_LIBUDEV

static void V4L2_Deinitialize(void)
{
#ifdef SDL_USE_LIBUDEV
    SDL_UDEV_DelCallback(CameraUdevCallback);
    SDL_UDEV_Quit();
#endif // SDL_USE_LIBUDEV
}

static void V4L2_DetectDevices(void)
{
#ifdef SDL_USE_LIBUDEV
    if (SDL_UDEV_Init() == 0) {
        if (SDL_UDEV_AddCallback(CameraUdevCallback) == 0) {
            SDL_UDEV_Scan();  // Force a scan to build the initial device list
        }
    }
#else
    DIR *dirp = opendir("/dev");
    if (dirp) {
        struct dirent *dent;
        while ((dent = readdir(dirp)) != NULL) {
            int num = 0;
            if (SDL_sscanf(dent->d_name, "video%d", &num) == 1) {
                char fullpath[64];
                SDL_snprintf(fullpath, sizeof (fullpath), "/dev/video%d", num);
                MaybeAddDevice(fullpath);
            }
        }
        closedir(dirp);
    }
#endif // SDL_USE_LIBUDEV
}

static SDL_bool V4L2_Init(SDL_CameraDriverImpl *impl)
{
    impl->DetectDevices = V4L2_DetectDevices;
    impl->OpenDevice = V4L2_OpenDevice;
    impl->CloseDevice = V4L2_CloseDevice;
    impl->WaitDevice = V4L2_WaitDevice;
    impl->AcquireFrame = V4L2_AcquireFrame;
    impl->ReleaseFrame = V4L2_ReleaseFrame;
    impl->FreeDeviceHandle = V4L2_FreeDeviceHandle;
    impl->Deinitialize = V4L2_Deinitialize;

    return SDL_TRUE;
}

CameraBootStrap V4L2_bootstrap = {
    "v4l2", "SDL Video4Linux2 camera driver", V4L2_Init, SDL_FALSE
};

#endif // SDL_CAMERA_DRIVER_V4L2

