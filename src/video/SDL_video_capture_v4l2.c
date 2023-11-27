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

#ifdef SDL_VIDEO_CAPTURE

#include "SDL3/SDL.h"
#include "SDL3/SDL_video_capture.h"
#include "SDL_sysvideocapture.h"
#include "SDL_video_capture_c.h"
#include "SDL_pixels_c.h"
#include "../thread/SDL_systhread.h"
#include "../../core/linux/SDL_evdev_capabilities.h"
#include "../../core/linux/SDL_udev.h"
#include <limits.h>      /* INT_MAX */

#define DEBUG_VIDEO_CAPTURE_CAPTURE 1

#if defined(__linux__) && !defined(__ANDROID__)


#define MAX_CAPTURE_DEVICES 128 /* It's doubtful someone has more than that */

static int MaybeAddDevice(const char *path);
#ifdef SDL_USE_LIBUDEV
static int MaybeRemoveDevice(const char *path);
static void capture_udev_callback(SDL_UDEV_deviceevent udev_type, int udev_class, const char *devpath);
#endif /* SDL_USE_LIBUDEV */

/*
 * List of available capture devices.
 */
typedef struct SDL_capturelist_item
{
    char *fname;        /* Dev path name (like /dev/video0) */
    char *bus_info;     /* don't add two paths with same bus_info (eg /dev/video0 and /dev/video1 */
    SDL_VideoCaptureDeviceID instance_id;
    SDL_VideoCaptureDevice *device; /* Associated device */
    struct SDL_capturelist_item *next;
} SDL_capturelist_item;

static SDL_capturelist_item *SDL_capturelist = NULL;
static SDL_capturelist_item *SDL_capturelist_tail = NULL;
static int num_video_captures = 0;



enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR
};

struct buffer {
    void   *start;
    size_t  length;
    int available; /* Is available in userspace */
};

struct SDL_PrivateVideoCaptureData
{
    int fd;
    enum io_method io;
    int nb_buffers;
    struct buffer *buffers;
    int first_start;
    int driver_pitch;
};

#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>              /* low-level i/o */
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/videodev2.h>

static int
xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (r == -1 && errno == EINTR);

    return r;
}

/* -1:error  1:frame 0:no frame*/
static int
acquire_frame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    struct v4l2_buffer buf;
    int i;

    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;
    size_t size = _this->hidden->buffers[0].length;

    switch (io) {
        case IO_METHOD_READ:
            if (read(fd, _this->hidden->buffers[0].start, size) == -1) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        return SDL_SetError("read");
                }
            }

            frame->num_planes = 1;
            frame->data[0] = _this->hidden->buffers[0].start;
            frame->pitch[0] = _this->hidden->driver_pitch;
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
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        return SDL_SetError("VIDIOC_DQBUF: %d", errno);
                }
            }

            if ((int)buf.index < 0 || (int)buf.index >= _this->hidden->nb_buffers) {
                return SDL_SetError("invalid buffer index");
            }

            frame->num_planes = 1;
            frame->data[0] = _this->hidden->buffers[buf.index].start;
            frame->pitch[0] = _this->hidden->driver_pitch;
            _this->hidden->buffers[buf.index].available = 1;

#if DEBUG_VIDEO_CAPTURE_CAPTURE
            SDL_Log("debug mmap: image %d/%d  num_planes:%d data[0]=%p", buf.index, _this->hidden->nb_buffers, frame->num_planes, (void*)frame->data[0]);
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
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        return SDL_SetError("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < _this->hidden->nb_buffers; ++i) {
                if (buf.m.userptr == (unsigned long)_this->hidden->buffers[i].start && buf.length == size) {
                    break;
                }
            }

            if (i >= _this->hidden->nb_buffers) {
                return SDL_SetError("invalid buffer index");
            }

            frame->num_planes = 1;
            frame->data[0] = (void*)buf.m.userptr;
            frame->pitch[0] = _this->hidden->driver_pitch;
            _this->hidden->buffers[i].available = 1;
#if DEBUG_VIDEO_CAPTURE_CAPTURE
            SDL_Log("debug userptr: image %d/%d  num_planes:%d data[0]=%p", buf.index, _this->hidden->nb_buffers, frame->num_planes, (void*)frame->data[0]);
#endif
            break;
    }

    return 1;
}


int
ReleaseFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    struct v4l2_buffer buf;
    int i;
    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;

    for (i = 0; i < _this->hidden->nb_buffers; ++i) {
        if (frame->num_planes && frame->data[0] == _this->hidden->buffers[i].start) {
            break;
        }
    }

    if (i >= _this->hidden->nb_buffers) {
        return SDL_SetError("invalid buffer index");
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
                return SDL_SetError("VIDIOC_QBUF");
            }
            _this->hidden->buffers[i].available = 0;
            break;

        case IO_METHOD_USERPTR:
            SDL_zero(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = i;
            buf.m.userptr = (unsigned long)frame->data[0];
            buf.length = (int) _this->hidden->buffers[i].length;

            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                return SDL_SetError("VIDIOC_QBUF");
            }
            _this->hidden->buffers[i].available = 0;
            break;
    }

    return 0;
}


int
AcquireFrame(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureFrame *frame)
{
    fd_set fds;
    struct timeval tv;
    int ret;

    int fd = _this->hidden->fd;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    /* Timeout. */
    tv.tv_sec = 0;
    tv.tv_usec = 300 * 1000;

    ret = select(fd + 1, &fds, NULL, NULL, &tv);

    if (ret == -1) {
        if (errno == EINTR) {
#if DEBUG_VIDEO_CAPTURE_CAPTURE
            SDL_Log("continue ..");
#endif
            return 0;
        }
        return SDL_SetError("select");
    }

    if (ret == 0) {
        /* Timeout. Not an error */
        SDL_SetError("timeout select");
        return 0;
    }

    ret = acquire_frame(_this, frame);
    if (ret < 0) {
        return -1;
    }

    if (ret == 1){
        frame->timestampNS = SDL_GetTicksNS();
    } else if (ret == 0) {
#if DEBUG_VIDEO_CAPTURE_CAPTURE
        SDL_Log("No frame continue: %s", SDL_GetError());
#endif
    }

    /* EAGAIN - continue select loop. */
    return 0;
}


int
StopCapture(SDL_VideoCaptureDevice *_this)
{
    enum v4l2_buf_type type;
    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;

    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
                return SDL_SetError("VIDIOC_STREAMOFF");
            }
            break;
    }

    return 0;
}

static int
enqueue_buffers(SDL_VideoCaptureDevice *_this)
{
    int i;
    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;
    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < _this->hidden->nb_buffers; ++i) {
                if (_this->hidden->buffers[i].available == 0) {
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
            for (i = 0; i < _this->hidden->nb_buffers; ++i) {
                if (_this->hidden->buffers[i].available == 0) {
                    struct v4l2_buffer buf;

                    SDL_zero(buf);
                    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    buf.memory = V4L2_MEMORY_USERPTR;
                    buf.index = i;
                    buf.m.userptr = (unsigned long)_this->hidden->buffers[i].start;
                    buf.length = (int) _this->hidden->buffers[i].length;

                    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                        return SDL_SetError("VIDIOC_QBUF");
                    }
                }
            }
            break;
    }
    return 0;
}

static int
pre_enqueue_buffers(SDL_VideoCaptureDevice *_this)
{
    struct v4l2_requestbuffers req;
    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;

    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
            {
                SDL_zero(req);
                req.count = _this->hidden->nb_buffers;
                req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                req.memory = V4L2_MEMORY_MMAP;

                if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
                    if (errno == EINVAL) {
                        return SDL_SetError("Does not support memory mapping");
                    } else {
                        return SDL_SetError("VIDIOC_REQBUFS");
                    }
                }

                if (req.count < 2) {
                    return SDL_SetError("Insufficient buffer memory");
                }

                _this->hidden->nb_buffers = req.count;
            }
            break;

        case IO_METHOD_USERPTR:
            {
                SDL_zero(req);
                req.count  = _this->hidden->nb_buffers;
                req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                req.memory = V4L2_MEMORY_USERPTR;

                if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
                    if (errno == EINVAL) {
                        return SDL_SetError("Does not support user pointer i/o");
                    } else {
                        return SDL_SetError("VIDIOC_REQBUFS");
                    }
                }
            }
            break;
    }
    return 0;
}

int
StartCapture(SDL_VideoCaptureDevice *_this)
{
    enum v4l2_buf_type type;

    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;


    if (_this->hidden->first_start == 0) {
        _this->hidden->first_start = 1;
    } else {
        int old = _this->hidden->nb_buffers;
        // TODO mmap; doesn't work with stop->start
#if 1
        /* Can change nb_buffers for mmap */
        if (pre_enqueue_buffers(_this) < 0) {
            return -1;
        }
        if (old != _this->hidden->nb_buffers) {
            SDL_SetError("different nb of buffers requested");
            return -1;
        }
#endif
        _this->hidden->first_start = 1;
    }

    if (enqueue_buffers(_this) < 0) {
        return -1;
    }

    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
                return SDL_SetError("VIDIOC_STREAMON");
            }
            break;
    }

    return 0;
}

static int alloc_buffer_read(SDL_VideoCaptureDevice *_this, size_t buffer_size)
{
    _this->hidden->buffers[0].length = buffer_size;
    _this->hidden->buffers[0].start = SDL_calloc(1, buffer_size);

    if (!_this->hidden->buffers[0].start) {
        return SDL_OutOfMemory();
    }
    return 0;
}

static int
alloc_buffer_mmap(SDL_VideoCaptureDevice *_this)
{
    int fd = _this->hidden->fd;
    int i;
    for (i = 0; i < _this->hidden->nb_buffers; ++i) {
        struct v4l2_buffer buf;

        SDL_zero(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            return SDL_SetError("VIDIOC_QUERYBUF");
        }

        _this->hidden->buffers[i].length = buf.length;
        _this->hidden->buffers[i].start =
            mmap(NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    fd, buf.m.offset);

        if (MAP_FAILED == _this->hidden->buffers[i].start) {
            return SDL_SetError("mmap");
        }
    }
    return 0;
}

static int
alloc_buffer_userp(SDL_VideoCaptureDevice *_this, size_t buffer_size)
{
    int i;
    for (i = 0; i < _this->hidden->nb_buffers; ++i) {
        _this->hidden->buffers[i].length = buffer_size;
        _this->hidden->buffers[i].start = SDL_calloc(1, buffer_size);

        if (!_this->hidden->buffers[i].start) {
            return SDL_OutOfMemory();
        }
    }
    return 0;
}

static Uint32
format_v4l2_2_sdl(Uint32 fmt)
{
    switch (fmt) {
#define CASE(x, y)  case x: return y
        CASE(V4L2_PIX_FMT_YUYV, SDL_PIXELFORMAT_YUY2);
        CASE(V4L2_PIX_FMT_MJPEG, SDL_PIXELFORMAT_UNKNOWN);
#undef CASE
        default:
            SDL_Log("Unknown format V4L2_PIX_FORMAT '%d'", fmt);
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static Uint32
format_sdl_2_v4l2(Uint32 fmt)
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

int
GetNumFormats(SDL_VideoCaptureDevice *_this)
{
    int fd = _this->hidden->fd;
    int i = 0;
    struct v4l2_fmtdesc fmtdesc;

    SDL_zero(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc) == 0) {
        fmtdesc.index++;
        i++;
    }
    return i;
}

int
GetFormat(SDL_VideoCaptureDevice *_this, int index, Uint32 *format)
{
    int fd = _this->hidden->fd;
    struct v4l2_fmtdesc fmtdesc;

    SDL_zero(fmtdesc);
    fmtdesc.index = index;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc) == 0) {
        *format = format_v4l2_2_sdl(fmtdesc.pixelformat);

#if DEBUG_VIDEO_CAPTURE_CAPTURE
        if (fmtdesc.flags & V4L2_FMT_FLAG_EMULATED) {
            SDL_Log("%s format emulated", SDL_GetPixelFormatName(*format));
        }
        if (fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED) {
            SDL_Log("%s format compressed", SDL_GetPixelFormatName(*format));
        }
#endif
        return 0;
    }

    return -1;
}

int
GetNumFrameSizes(SDL_VideoCaptureDevice *_this, Uint32 format)
{
    int fd = _this->hidden->fd;
    int i = 0;
    struct v4l2_frmsizeenum frmsizeenum;

    SDL_zero(frmsizeenum);
    frmsizeenum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frmsizeenum.pixel_format = format_sdl_2_v4l2(format);
    while (ioctl(fd,VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == 0) {
        frmsizeenum.index++;
        if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            i++;
        } else if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            i += (1 + (frmsizeenum.stepwise.max_width - frmsizeenum.stepwise.min_width) / frmsizeenum.stepwise.step_width)
                * (1 + (frmsizeenum.stepwise.max_height - frmsizeenum.stepwise.min_height) / frmsizeenum.stepwise.step_height);
        } else if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
            SDL_SetError("V4L2_FRMSIZE_TYPE_CONTINUOUS not handled");
        }
    }
    return i;
}

int
GetFrameSize(SDL_VideoCaptureDevice *_this, Uint32 format, int index, int *width, int *height)
{
    int fd = _this->hidden->fd;
    struct v4l2_frmsizeenum frmsizeenum;
    int i = 0;

    SDL_zero(frmsizeenum);
    frmsizeenum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frmsizeenum.pixel_format = format_sdl_2_v4l2(format);
    while (ioctl(fd,VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == 0) {
        frmsizeenum.index++;

        if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            if (i == index) {
                *width = frmsizeenum.discrete.width;
                *height = frmsizeenum.discrete.height;
                return 0;
            }
            i++;
        } else if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            unsigned int w;
            for (w = frmsizeenum.stepwise.min_width; w <= frmsizeenum.stepwise.max_width; w += frmsizeenum.stepwise.step_width) {
                unsigned int h;
                for (h = frmsizeenum.stepwise.min_height; h <= frmsizeenum.stepwise.max_height; h += frmsizeenum.stepwise.step_height) {
                    if (i == index) {
                        *width = h;
                        *height = w;
                        return 0;
                    }
                    i++;
                }
            }
        } else if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        }
    }

    return -1;
}



static void
dbg_v4l2_pixelformat(const char *str, int f) {
    SDL_Log("%s  V4L2_format=%d  %c%c%c%c", str, f,
                (f >> 0) & 0xff,
                (f >> 8) & 0xff,
                (f >> 16) & 0xff,
                (f >> 24) & 0xff);
}

int
GetDeviceSpec(SDL_VideoCaptureDevice *_this, SDL_VideoCaptureSpec *spec)
{
    struct v4l2_format fmt;
    int fd = _this->hidden->fd;
    unsigned int min;

    SDL_zero(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* Preserve original settings as set by v4l2-ctl for example */
    if (xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
        return SDL_SetError("Error VIDIOC_G_FMT");
    }

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min) {
        fmt.fmt.pix.bytesperline = min;
    }
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min) {
        fmt.fmt.pix.sizeimage = min;
    }

    //spec->width = fmt.fmt.pix.width;
    //spec->height = fmt.fmt.pix.height;
    _this->hidden->driver_pitch = fmt.fmt.pix.bytesperline;
    //spec->format = format_v4l2_2_sdl(fmt.fmt.pix.pixelformat);

    return 0;
}

int
InitDevice(SDL_VideoCaptureDevice *_this)
{
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    int fd = _this->hidden->fd;
    enum io_method io = _this->hidden->io;
    int ret = -1;

    /* Select video input, video standard and tune here. */
    SDL_zero(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (xioctl(fd, VIDIOC_S_CROP, &crop) == -1) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {
        /* Errors ignored. */
    }


    {
        struct v4l2_format fmt;
        SDL_zero(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width       = _this->spec.width;
        fmt.fmt.pix.height      = _this->spec.height;


        fmt.fmt.pix.pixelformat = format_sdl_2_v4l2(_this->spec.format);
        //    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
        fmt.fmt.pix.field       = V4L2_FIELD_ANY;

#if DEBUG_VIDEO_CAPTURE_CAPTURE
        SDL_Log("set SDL format %s", SDL_GetPixelFormatName(_this->spec.format));
        dbg_v4l2_pixelformat("set format", fmt.fmt.pix.pixelformat);
#endif

        if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
            return SDL_SetError("Error VIDIOC_S_FMT");
        }
    }

    GetDeviceSpec(_this, &_this->spec);

    if (pre_enqueue_buffers(_this) < 0) {
        return -1;
    }

    {
        _this->hidden->buffers = SDL_calloc(_this->hidden->nb_buffers, sizeof(*_this->hidden->buffers));
        if (!_this->hidden->buffers) {
            return SDL_OutOfMemory();
        }
    }

    {
        size_t size, pitch;
        SDL_CalculateSize(_this->spec.format, _this->spec.width, _this->spec.height, &size, &pitch, SDL_FALSE);

        switch (io) {
            case IO_METHOD_READ:
                ret = alloc_buffer_read(_this, size);
                break;

            case IO_METHOD_MMAP:
                ret = alloc_buffer_mmap(_this);
                break;

            case IO_METHOD_USERPTR:
                ret = alloc_buffer_userp(_this, size);
                break;
        }
    }

    if (ret < 0) {
        return -1;
    }

    return 0;
}

void
CloseDevice(SDL_VideoCaptureDevice *_this)
{
    if (!_this) {
        return;
    }

    if (_this->hidden) {
        if (_this->hidden->buffers) {
            int i;
            enum io_method io = _this->hidden->io;

            switch (io) {
                case IO_METHOD_READ:
                    SDL_free(_this->hidden->buffers[0].start);
                    break;

                case IO_METHOD_MMAP:
                    for (i = 0; i < _this->hidden->nb_buffers; ++i) {
                        if (munmap(_this->hidden->buffers[i].start, _this->hidden->buffers[i].length) == -1) {
                            SDL_SetError("munmap");
                        }
                    }
                    break;

                case IO_METHOD_USERPTR:
                    for (i = 0; i < _this->hidden->nb_buffers; ++i) {
                        SDL_free(_this->hidden->buffers[i].start);
                    }
                    break;
            }

            SDL_free(_this->hidden->buffers);
        }

        if (_this->hidden->fd != -1) {
            if (close(_this->hidden->fd)) {
                SDL_SetError("close video capture device");
            }
        }
        SDL_free(_this->hidden);

        _this->hidden = NULL;
    }
}


int
OpenDevice(SDL_VideoCaptureDevice *_this)
{
    struct stat st;
    struct v4l2_capability cap;
    int fd;
    enum io_method io;

    _this->hidden = (struct SDL_PrivateVideoCaptureData *) SDL_calloc(1, sizeof (struct SDL_PrivateVideoCaptureData));
    if (_this->hidden == NULL) {
        SDL_OutOfMemory();
        return -1;
    }

    _this->hidden->fd = -1;

    if (stat(_this->dev_name, &st) == -1) {
        SDL_SetError("Cannot identify '%s': %d, %s", _this->dev_name, errno, strerror(errno));
        return -1;
    }

    if (!S_ISCHR(st.st_mode)) {
        SDL_SetError("%s is no device", _this->dev_name);
        return -1;
    }

    fd = open(_this->dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    if (fd == -1) {
        SDL_SetError("Cannot open '%s': %d, %s", _this->dev_name, errno, strerror(errno));
        return -1;
    }

    _this->hidden->fd = fd;
    _this->hidden->io = IO_METHOD_MMAP;
//    _this->hidden->io = IO_METHOD_USERPTR;
//    _this->hidden->io = IO_METHOD_READ;
//
    if (_this->hidden->io == IO_METHOD_READ) {
        _this->hidden->nb_buffers = 1;
    } else {
        _this->hidden->nb_buffers = 8; /* Number of image as internal buffer, */
    }
    io = _this->hidden->io;

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        if (errno == EINVAL) {
            return SDL_SetError("%s is no V4L2 device", _this->dev_name);
        } else {
            return SDL_SetError("Error VIDIOC_QUERYCAP errno=%d device%s is no V4L2 device", errno, _this->dev_name);
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        return SDL_SetError("%s is no video capture device", _this->dev_name);
    }

#if 0
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        SDL_Log("%s is video capture device - single plane", _this->dev_name);
    }
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        SDL_Log("%s is video capture device - multiple planes", _this->dev_name);
    }
#endif

    switch (io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                return SDL_SetError("%s does not support read i/o", _this->dev_name);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                return SDL_SetError("%s does not support streaming i/o", _this->dev_name);
            }
            break;
    }




    return 0;
}

int
GetDeviceName(SDL_VideoCaptureDeviceID instance_id, char *buf, int size)
{
    SDL_capturelist_item *item;
    for (item = SDL_capturelist; item; item = item->next) {
        if (item->instance_id == instance_id) {
            SDL_snprintf(buf, size, "%s", item->fname);
            return 0;
        }
    }

    /* unknown instance_id */
    return -1;
}


SDL_VideoCaptureDeviceID *GetVideoCaptureDevices(int *count)
{
    /* real list of ID */
    int i = 0;
    int num = num_video_captures;
    SDL_VideoCaptureDeviceID *ret;
    SDL_capturelist_item *item;

    ret = (SDL_VideoCaptureDeviceID *)SDL_malloc((num + 1) * sizeof(*ret));

    if (ret == NULL) {
        SDL_OutOfMemory();
        *count = 0;
        return NULL;
    }

    for (item = SDL_capturelist; item; item = item->next) {
        ret[i] = item->instance_id;
        i++;
    }

    ret[num] = 0;
    *count = num;
    return ret;
}


/*
 * Initializes the subsystem by finding available devices.
 */
int SDL_SYS_VideoCaptureInit(void)
{
    const char pattern[] = "/dev/video%d";
    char path[PATH_MAX];
    int i, j;

    /*
     * Limit amount of checks to MAX_CAPTURE_DEVICES since we may or may not have
     * permission to some or all devices.
     */
    i = 0;
    for (j = 0; j < MAX_CAPTURE_DEVICES; ++j) {
        (void)SDL_snprintf(path, PATH_MAX, pattern, i++);
        if (MaybeAddDevice(path) == -2) {
            break;
        }
    }

#ifdef SDL_USE_LIBUDEV
    if (SDL_UDEV_Init() < 0) {
        return SDL_SetError("Could not initialize UDEV");
    }

    if (SDL_UDEV_AddCallback(capture_udev_callback) < 0) {
        SDL_UDEV_Quit();
        return SDL_SetError("Could not setup Video Capture <-> udev callback");
    }

    /* Force a scan to build the initial device list */
    SDL_UDEV_Scan();
#endif /* SDL_USE_LIBUDEV */

    return num_video_captures;
}


int SDL_SYS_VideoCaptureQuit(void)
{
    SDL_capturelist_item *item;
    for (item = SDL_capturelist; item; ) {
        SDL_capturelist_item *tmp = item->next;

        SDL_free(item->fname);
        SDL_free(item->bus_info);
        SDL_free(item);
        item = tmp;
    }

    num_video_captures = 0;
    SDL_capturelist = NULL;
    SDL_capturelist_tail = NULL;

    return SDL_FALSE;
}

#ifdef SDL_USE_LIBUDEV
static void capture_udev_callback(SDL_UDEV_deviceevent udev_type, int udev_class, const char *devpath)
{
    if (!devpath || !(udev_class & SDL_UDEV_DEVICE_VIDEO_CAPTURE)) {
        return;
    }

    switch (udev_type) {
    case SDL_UDEV_DEVICEADDED:
        MaybeAddDevice(devpath);
        break;

    case SDL_UDEV_DEVICEREMOVED:
        MaybeRemoveDevice(devpath);
        break;

    default:
        break;
    }
}
#endif /* SDL_USE_LIBUDEV */

static SDL_bool DeviceExists(const char *path, const char *bus_info) {
    SDL_capturelist_item *item;

    for (item = SDL_capturelist; item; item = item->next) {
        /* found same dev name */
        if (SDL_strcmp(path, item->fname) == 0) {
            return SDL_TRUE;
        }
        /* found same bus_info */
        if (SDL_strcmp(bus_info, item->bus_info) == 0) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static int MaybeAddDevice(const char *path)
{
    char *bus_info = NULL;
    struct v4l2_capability vcap;
    int err;
    int fd;
    SDL_capturelist_item *item;

    if (!path) {
        return -1;
    }

    fd = open(path, O_RDWR);
    if (fd < 0) {
        return -2; /* stop iterating /dev/video%d */
    }
    err = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
    close(fd);
    if (err) {
        return -1;
    }

    bus_info = SDL_strdup((char *)vcap.bus_info);

    if (DeviceExists(path, bus_info)) {
        SDL_free(bus_info);
        return 0;
    }


    /* Add new item */
    item = (SDL_capturelist_item *)SDL_calloc(1, sizeof(SDL_capturelist_item));
    if (!item) {
        SDL_free(bus_info);
        return -1;
    }

    item->fname = SDL_strdup(path);
    if (!item->fname) {
        SDL_free(item);
        SDL_free(bus_info);
        return -1;
    }

    item->fname = SDL_strdup(path);
    item->bus_info = bus_info;
    item->instance_id = SDL_GetNextObjectID();


    if (!SDL_capturelist_tail) {
        SDL_capturelist = SDL_capturelist_tail = item;
    } else {
        SDL_capturelist_tail->next = item;
        SDL_capturelist_tail = item;
    }

    ++num_video_captures;

    /* !!! TODO: Send a add event? */
#if DEBUG_VIDEO_CAPTURE_CAPTURE
    SDL_Log("Added video capture ID: %d %s (%s) (total: %d)", item->instance_id, path, bus_info, num_video_captures);
#endif
    return 0;
}

#ifdef SDL_USE_LIBUDEV
static int MaybeRemoveDevice(const char *path)
{

    SDL_capturelist_item *item;
    SDL_capturelist_item *prev = NULL;
#if DEBUG_VIDEO_CAPTURE_CAPTURE
    SDL_Log("Remove video capture %s", path);
#endif
    if (!path) {
        return -1;
    }

    for (item = SDL_capturelist; item; item = item->next) {
        /* found it, remove it. */
        if (SDL_strcmp(path, item->fname) == 0) {
            if (prev) {
                prev->next = item->next;
            } else {
                SDL_assert(SDL_capturelist == item);
                SDL_capturelist = item->next;
            }
            if (item == SDL_capturelist_tail) {
                SDL_capturelist_tail = prev;
            }

            /* Need to decrement the count */
            --num_video_captures;
            /* !!! TODO: Send a remove event? */

            SDL_free(item->fname);
            SDL_free(item->bus_info);
            SDL_free(item);
            return 0;
        }
        prev = item;
    }
    return 0;
}
#endif /* SDL_USE_LIBUDEV */



#endif

#endif /* SDL_VIDEO_CAPTURE */
