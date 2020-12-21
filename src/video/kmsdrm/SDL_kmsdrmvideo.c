/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>
  Atomic KMSDRM backend by Manuel Alfayate Corchete <redwindwanderer@gmail.com>

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

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_KMSDRM

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_syswm.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

#ifdef SDL_INPUT_LINUXEV
#include "../../core/linux/SDL_evdev.h"
#endif

/* KMS/DRM declarations */
#include "SDL_kmsdrmvideo.h"
#include "SDL_kmsdrmevents.h"
#include "SDL_kmsdrmopengles.h"
#include "SDL_kmsdrmmouse.h"
#include "SDL_kmsdrmdyn.h"
#include "SDL_kmsdrmvulkan.h"
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>

/* for older KMSDRM headers... */
#ifndef DRM_FORMAT_MOD_VENDOR_NONE
#define DRM_FORMAT_MOD_VENDOR_NONE 0
#endif
#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR fourcc_mod_code(NONE, 0)
#endif

#define KMSDRM_DRI_PATH "/dev/dri/"

static int set_client_caps (int fd)
{
    if (KMSDRM_drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
         return SDL_SetError("no atomic modesetting support.");
    }
    if (KMSDRM_drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
         return SDL_SetError("no universal planes support.");
    }
    return 0;
}

static int
check_modesetting(int devindex)
{
    SDL_bool available = SDL_FALSE;
    char device[512];
    unsigned int i;
    int drm_fd;

    SDL_snprintf(device, sizeof (device), "%scard%d", KMSDRM_DRI_PATH, devindex);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "check_modesetting: probing \"%s\"", device);

    drm_fd = open(device, O_RDWR | O_CLOEXEC);
    if (drm_fd >= 0) {
        if (SDL_KMSDRM_LoadSymbols()) {
            drmModeRes *resources = (set_client_caps(drm_fd) < 0) ? NULL : KMSDRM_drmModeGetResources(drm_fd);
            if (resources) {
                SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%scard%d connector, encoder and CRTC counts are: %d %d %d",
                             KMSDRM_DRI_PATH, devindex,
                             resources->count_connectors, resources->count_encoders, resources->count_crtcs);

                if (resources->count_connectors > 0 && resources->count_encoders > 0 && resources->count_crtcs > 0) {
                    for (i = 0; i < resources->count_connectors; i++) {
                        drmModeConnector *conn = KMSDRM_drmModeGetConnector(drm_fd, resources->connectors[i]);

                        if (!conn) {
                            continue;
                        }

                        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes) {
                            available = SDL_TRUE;
                        }

                        KMSDRM_drmModeFreeConnector(conn);
                        if (available) {
                            break;
                        }
                    }
                }
                KMSDRM_drmModeFreeResources(resources);
            }
            SDL_KMSDRM_UnloadSymbols();
        }
        close(drm_fd);
    }

    return available;
}

static unsigned int get_dricount(void)
{
    unsigned int devcount = 0;
    struct dirent *res;
    struct stat sb;
    DIR *folder;

    if (!(stat(KMSDRM_DRI_PATH, &sb) == 0 && S_ISDIR(sb.st_mode))) {
        SDL_SetError("The path %s cannot be opened or is not available",
        KMSDRM_DRI_PATH);
        return 0;
    }

    if (access(KMSDRM_DRI_PATH, F_OK) == -1) {
        SDL_SetError("The path %s cannot be opened",
            KMSDRM_DRI_PATH);
        return 0;
    }

    folder = opendir(KMSDRM_DRI_PATH);
    if (folder) {
        while ((res = readdir(folder))) {
            size_t len = SDL_strlen(res->d_name);
            if (len > 4 && SDL_strncmp(res->d_name, "card", 4) == 0) {
                devcount++;
            }
        }
        closedir(folder);
    }

    return devcount;
}

static int
get_driindex(void)
{
    const unsigned int devcount = get_dricount();
    unsigned int i;

    for (i = 0; i < devcount; i++) {
        if (check_modesetting(i)) {
            return i;
        }
    }

    return -ENOENT;
}

#if 0

/**********************/
/* DUMB BUFFER Block. */
/**********************/

/* Create a dumb buffer, mmap the dumb buffer and fill it with pixels, */
/* then create a KMS framebuffer wrapping the dumb buffer.             */
static dumb_buffer *KMSDRM_CreateDumbBuffer(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    struct drm_mode_create_dumb create;
    struct drm_mode_map_dumb map;
    struct drm_mode_destroy_dumb destroy;

    dumb_buffer *ret = SDL_calloc(1, sizeof(*ret));
    if (!ret) {
        SDL_OutOfMemory();
        return NULL;
    }

    /*
     * The create ioctl uses the combination of depth and bpp to infer
     * a format; 24/32 refers to DRM_FORMAT_XRGB8888 as defined in
     * the drm_fourcc.h header. These arguments are the same as given
     * to drmModeAddFB, which has since been superseded by
     * drmModeAddFB2 as the latter takes an explicit format token.
     *
     * We only specify these arguments; the driver calculates the
     * pitch (also known as stride or row length) and total buffer size
     * for us, also returning us the GEM handle.
     */
    create = (struct drm_mode_create_dumb) {
        .width = dispdata->mode.hdisplay,
        .height = dispdata->mode.vdisplay,
        .bpp = 32,
    };

    if (KMSDRM_drmIoctl(viddata->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create)) {
        SDL_SetError("failed to create dumb buffer\n");
        goto err;
    }

    ret->gem_handles[0] = create.handle;
    ret->format = DRM_FORMAT_XRGB8888;
    ret->modifier = DRM_FORMAT_MOD_LINEAR;
    ret->width = create.width;
    ret->height = create.height;
    ret->pitches[0] = create.pitch;

    /*
     * In order to map the buffer, we call an ioctl specific to the buffer
     * type, which returns us a fake offset to use with the mmap syscall.
     * mmap itself then works as you expect.
     *
     * Note this means it is not possible to map arbitrary offsets of
     * buffers without specifically requesting it from the kernel.
     */
    map = (struct drm_mode_map_dumb) {
        .handle = ret->gem_handles[0],
    };

    if (KMSDRM_drmIoctl(viddata->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map)) {
        SDL_SetError("failed to get mmap offset for the dumb buffer.");
        goto err_dumb;
    }

    ret->dumb.mem = mmap(NULL, create.size, PROT_WRITE, MAP_SHARED,
                         viddata->drm_fd, map.offset);

    if (ret->dumb.mem == MAP_FAILED) {
        SDL_SetError("failed to get mmap offset for the dumb buffer.");
        goto err_dumb;
    }
    ret->dumb.size = create.size;

    return ret;

err_dumb:
    destroy = (struct drm_mode_destroy_dumb) { .handle = create.handle };
    KMSDRM_drmIoctl(viddata->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
err:
    SDL_free(ret);
    return NULL;
}

static void
KMSDRM_DestroyDumbBuffer(_THIS, dumb_buffer **buffer)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    struct drm_mode_destroy_dumb destroy = {
        .handle = (*buffer)->gem_handles[0],
    };

    KMSDRM_drmModeRmFB(viddata->drm_fd, (*buffer)->fb_id);

    munmap((*buffer)->dumb.mem, (*buffer)->dumb.size);
    KMSDRM_drmIoctl(viddata->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    free(*buffer);
    *buffer = NULL;
}

/* Using the CPU mapping, fill the dumb buffer with black pixels. */
static void
KMSDRM_FillDumbBuffer(dumb_buffer *buffer)
{
    unsigned int x, y;
    for (y = 0; y < buffer->height; y++) {
    uint32_t *pix = (uint32_t *) ((uint8_t *) buffer->dumb.mem + (y * buffer->pitches[0]));
        for (x = 0; x < buffer->width; x++) {
            *pix++ = (0x00000000);
        }
    }
}

static dumb_buffer *KMSDRM_CreateBuffer(_THIS)
{
    dumb_buffer *ret;
    int err;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    ret = KMSDRM_CreateDumbBuffer(_this);

    if (!ret)
        return NULL;

    /*
     * Wrap our GEM buffer in a KMS framebuffer, so we can then attach it
     * to a plane. Here's where we get out fb_id!
     */
    err = KMSDRM_drmModeAddFB2(viddata->drm_fd, ret->width, ret->height,
        ret->format, ret->gem_handles, ret->pitches,
        ret->offsets, &ret->fb_id, 0);

    if (err != 0 || ret->fb_id == 0) {
        SDL_SetError("Failed AddFB2 on dumb buffer\n");
        goto err;
    }
    return ret;

err:
    KMSDRM_DestroyDumbBuffer(_this, &ret);
    return NULL;
}

/***************************/
/* DUMB BUFFER Block ends. */
/***************************/

#endif

/*********************************/
/* Atomic helper functions block */
/*********************************/

#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

int add_connector_property(drmModeAtomicReq *req, struct connector *connector,
                                     const char *name, uint64_t value)
{
    unsigned int i;
    int prop_id = 0;

    for (i = 0 ; i < connector->props->count_props ; i++) {
        if (strcmp(connector->props_info[i]->name, name) == 0) {
            prop_id = connector->props_info[i]->prop_id;
            break;
        }
    }

    if (prop_id < 0) {
        SDL_SetError("no connector property: %s", name);
        return -EINVAL;
    }

    return KMSDRM_drmModeAtomicAddProperty(req, connector->connector->connector_id, prop_id, value);
}

int add_crtc_property(drmModeAtomicReq *req, struct crtc *crtc,
                         const char *name, uint64_t value)
{
    unsigned int i;
    int prop_id = -1;

    for (i = 0 ; i < crtc->props->count_props ; i++) {
        if (strcmp(crtc->props_info[i]->name, name) == 0) {
            prop_id = crtc->props_info[i]->prop_id;
            break;
        }
    }

    if (prop_id < 0) {
        SDL_SetError("no crtc property: %s", name);
        return -EINVAL;
    }

    return KMSDRM_drmModeAtomicAddProperty(req, crtc->crtc->crtc_id, prop_id, value);
}

int add_plane_property(drmModeAtomicReq *req, struct plane *plane,
                          const char *name, uint64_t value)
{
    unsigned int i;
    int prop_id = -1;

    for (i = 0 ; i < plane->props->count_props ; i++) {
        if (strcmp(plane->props_info[i]->name, name) == 0) {
            prop_id = plane->props_info[i]->prop_id;
            break;
        }
    }

    if (prop_id < 0) {
        SDL_SetError("no plane property: %s", name);
        return -EINVAL;
    }

    return KMSDRM_drmModeAtomicAddProperty(req, plane->plane->plane_id, prop_id, value);
}

#if 0

void print_plane_info(_THIS, drmModePlanePtr plane)
{
    char *plane_type;
    drmModeRes *resources;
    uint32_t type = 0;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    int i;

    drmModeObjectPropertiesPtr props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        plane->plane_id, DRM_MODE_OBJECT_PLANE);

    /* Search the plane props for the plane type. */
    for (i = 0; i < props->count_props; i++) {
        drmModePropertyPtr p = KMSDRM_drmModeGetProperty(viddata->drm_fd, props->props[i]);
        if ((strcmp(p->name, "type") == 0)) {
            type = props->prop_values[i];
        }

        KMSDRM_drmModeFreeProperty(p);
    }

    switch (type) {
        case DRM_PLANE_TYPE_OVERLAY:
            plane_type = "overlay";
            break;

        case DRM_PLANE_TYPE_PRIMARY:
            plane_type = "primary";
            break;

        case DRM_PLANE_TYPE_CURSOR:
            plane_type = "cursor";
            break;
    }


    /* Remember that to present a plane on screen, it has to be
       connected to a CRTC so the CRTC scans it,
       scales it, etc... and presents it on screen. */

    /* Now we look for the CRTCs supported by the plane. */
    resources = KMSDRM_drmModeGetResources(viddata->drm_fd);
    if (!resources)
        return;

    printf("--PLANE ID: %d\nPLANE TYPE: %s\nCRTC READING THIS PLANE: %d\nCRTCS SUPPORTED BY THIS PLANE: ",  plane->plane_id, plane_type, plane->crtc_id);
    for (i = 0; i < resources->count_crtcs; i++) {
        if (plane->possible_crtcs & (1 << i)) {
            uint32_t crtc_id = resources->crtcs[i];
            printf ("%d", crtc_id);
            break;
        }
    }

    printf ("\n\n");
}

void get_planes_info(_THIS)
{
    drmModePlaneResPtr plane_resources;
    uint32_t i;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    plane_resources = KMSDRM_drmModeGetPlaneResources(viddata->drm_fd);
    if (!plane_resources) {
        printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
        return;
    }

    printf("--Number of planes found: %d-- \n", plane_resources->count_planes);
    printf("--Usable CRTC that we have chosen: %d-- \n", dispdata->crtc->crtc->crtc_id);

    /* Iterate on all the available planes. */
    for (i = 0; (i < plane_resources->count_planes); i++) {

        uint32_t plane_id = plane_resources->planes[i];

        drmModePlanePtr plane = KMSDRM_drmModeGetPlane(viddata->drm_fd, plane_id);
        if (!plane) {
            printf("drmModeGetPlane(%u) failed: %s\n", plane_id, strerror(errno));
            continue;
        }

        /* Print plane info. */
        print_plane_info(_this, plane);
        KMSDRM_drmModeFreePlane(plane);
    }

    KMSDRM_drmModeFreePlaneResources(plane_resources);
}

#endif

/* Get the plane_id of a plane that is of the specified plane type (primary,
   overlay, cursor...) and can use the CRTC we have chosen previously. */
static int get_plane_id(_THIS, uint32_t plane_type)
{
    drmModeRes *resources = NULL;
    drmModePlaneResPtr plane_resources = NULL;
    uint32_t i, j;
    unsigned int crtc_index = 0;
    int ret = -EINVAL;
    int found = 0;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    resources = KMSDRM_drmModeGetResources(viddata->drm_fd);

    /* Get the crtc_index for the current CRTC.
       It's needed to find out if a plane supports the CRTC. */
    for (i = 0; i < resources->count_crtcs; i++) {
        if (resources->crtcs[i] == dispdata->crtc->crtc->crtc_id) {
            crtc_index = i;
            break;
        }
    }

    plane_resources = KMSDRM_drmModeGetPlaneResources(viddata->drm_fd);
    if (!plane_resources) {
        return SDL_SetError("drmModeGetPlaneResources failed.");
    }

    /* Iterate on all the available planes. */
    for (i = 0; (i < plane_resources->count_planes) && !found; i++) {

        uint32_t plane_id = plane_resources->planes[i];

        drmModePlanePtr plane = KMSDRM_drmModeGetPlane(viddata->drm_fd, plane_id);
        if (!plane) {
            continue;
        }

        /* See if the current CRTC is available for this plane. */
        if (plane->possible_crtcs & (1 << crtc_index)) {

            drmModeObjectPropertiesPtr props = KMSDRM_drmModeObjectGetProperties(
                viddata->drm_fd, plane_id, DRM_MODE_OBJECT_PLANE);
            ret = plane_id;

            /* Iterate on the plane props to find the type of the plane,
               to see if it's of the type we want. */
            for (j = 0; j < props->count_props; j++) {

                drmModePropertyPtr p = KMSDRM_drmModeGetProperty(viddata->drm_fd,
                    props->props[j]);

                if ((strcmp(p->name, "type") == 0) && (props->prop_values[j] == plane_type)) {
                    /* found our plane, use that: */
                    found = 1;
                }

                KMSDRM_drmModeFreeProperty(p);
            }

            KMSDRM_drmModeFreeObjectProperties(props);
        }

        KMSDRM_drmModeFreePlane(plane);
    }

    KMSDRM_drmModeFreePlaneResources(plane_resources);
    KMSDRM_drmModeFreeResources(resources);

    return ret;
}

/* Setup a plane and it's props. */
int
setup_plane(_THIS, struct plane **plane, uint32_t plane_type)
{
    uint32_t plane_id;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    int ret = 0;

    *plane = SDL_calloc(1, sizeof(**plane));
    if (!(*plane)) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    /* Get plane ID. */
    plane_id = get_plane_id(_this, plane_type);

    if (!plane_id) {
        ret = SDL_SetError("Invalid Plane ID");
        goto cleanup;
    }

    /* Get the DRM plane itself. */
    (*plane)->plane = KMSDRM_drmModeGetPlane(viddata->drm_fd, plane_id);

    /* Get the DRM plane properties. */
    if ((*plane)->plane) {
        unsigned int i;
        (*plane)->props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        (*plane)->plane->plane_id, DRM_MODE_OBJECT_PLANE);
        (*plane)->props_info = SDL_calloc((*plane)->props->count_props,
            sizeof(*(*plane)->props_info));

        if ( !((*plane)->props_info) ) {
            ret = SDL_OutOfMemory();
            goto cleanup;
        }

        for (i = 0; i < (*plane)->props->count_props; i++) {
            (*plane)->props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
                (*plane)->props->props[i]);
        }
    }

cleanup:

    if (ret) {
        if (*plane) {
            SDL_free(*plane);
            *plane = NULL;
        }
    }
    return ret;
}

/* Free a plane and it's props. */
void
free_plane(struct plane **plane)
{
    if (*plane) {
        if ((*plane)->plane) {
            KMSDRM_drmModeFreePlane((*plane)->plane);
            (*plane)->plane = NULL;
        }
        if ((*plane)->props_info) {
            SDL_free((*plane)->props_info);
            (*plane)->props_info = NULL;
        }
        SDL_free(*plane);
        *plane = NULL;
    }
}

/**********************************************************************************/
/* The most important ATOMIC fn of the backend.                                   */
/* A PLANE reads a BUFFER, and a CRTC reads a PLANE and sends it's contents       */
/*   over to a CONNECTOR->ENCODER system (several CONNECTORS can be connected     */
/*   to the same PLANE).                                                          */
/*   Think of a plane as a "frame" sorrounding a picture, where the "picture"     */
/*   is the buffer, and we move the "frame" from  a picture to another,           */
/*   and the one that has the "frame" is the one sent over to the screen          */
/*   via the CONNECTOR->ENCODER system.                                           */
/*   Think of a PLANE as being "in the middle", it's the CENTRAL part             */
/*   bewteen the CRTC and the BUFFER that is shown on screen.                     */
/*   What we do here is connect a PLANE to a CRTC and a BUFFER.                   */
/*   -ALWAYS set the CRTC_ID and FB_ID attribs of a plane at the same time,       */
/*   meaning IN THE SAME atomic request.                                          */
/*   -And NEVER destroy a GBM surface whose buffers are being read by a plane:    */
/*   first, move the plane away from those buffers and ONLY THEN destroy the      */
/*   buffers and/or the GBM surface containig them.                               */
/**********************************************************************************/
void
drm_atomic_set_plane_props(struct KMSDRM_PlaneInfo *info)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    /* Do we have a set of changes already in the making? If not, allocate a new one. */
    if (!dispdata->atomic_req)
        dispdata->atomic_req = KMSDRM_drmModeAtomicAlloc();

    add_plane_property(dispdata->atomic_req, info->plane, "FB_ID", info->fb_id);
    add_plane_property(dispdata->atomic_req, info->plane, "CRTC_ID", info->crtc_id);
    add_plane_property(dispdata->atomic_req, info->plane, "SRC_W", info->src_w << 16);
    add_plane_property(dispdata->atomic_req, info->plane, "SRC_H", info->src_h << 16);
    add_plane_property(dispdata->atomic_req, info->plane, "SRC_X", info->src_x);
    add_plane_property(dispdata->atomic_req, info->plane, "SRC_Y", info->src_y);
    add_plane_property(dispdata->atomic_req, info->plane, "CRTC_W", info->crtc_w);
    add_plane_property(dispdata->atomic_req, info->plane, "CRTC_H", info->crtc_h);
    add_plane_property(dispdata->atomic_req, info->plane, "CRTC_X", info->crtc_x);
    add_plane_property(dispdata->atomic_req, info->plane, "CRTC_Y", info->crtc_y);
}

int drm_atomic_commit(_THIS, SDL_bool blocking)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    int ret;

    if (!blocking)
        dispdata->atomic_flags |= DRM_MODE_ATOMIC_NONBLOCK;

    /* Never issue a new atomic commit if previous has not yet completed, or it will error. */
    drm_atomic_waitpending(_this);

    ret = KMSDRM_drmModeAtomicCommit(viddata->drm_fd, dispdata->atomic_req, dispdata->atomic_flags, NULL);
    if (ret) {
        SDL_SetError("Atomic commit failed, returned %d.", ret);
        /* Uncomment this for fast-debugging */
#if 0
        printf("ATOMIC COMMIT FAILED: %s.\n", strerror(errno));
#endif
        goto out;
    }

    if (dispdata->kms_in_fence_fd != -1) {
        close(dispdata->kms_in_fence_fd);
        dispdata->kms_in_fence_fd = -1;
    }

out:
    KMSDRM_drmModeAtomicFree(dispdata->atomic_req);
    dispdata->atomic_req = NULL;
    dispdata->atomic_flags = 0;

    return ret;
}

void
drm_atomic_waitpending(_THIS)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    /* Will return immediately if we have already destroyed the fence, because we NULL-ify it just after.
       Also, will return immediately in double-buffer mode, because kms_fence will alsawys be NULL. */
    if (dispdata->kms_fence) {
        EGLint status;

        do {
            status = _this->egl_data->eglClientWaitSyncKHR(_this->egl_data->egl_display,
                dispdata->kms_fence, 0, EGL_FOREVER_KHR);
        } while (status != EGL_CONDITION_SATISFIED_KHR);

        _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->kms_fence);
            dispdata->kms_fence = NULL;
    }
}

/***************************************/
/* End of Atomic helper functions block*/
/***************************************/

static int
KMSDRM_Available(void)
{
    int ret = -ENOENT;

    ret = get_driindex();
    if (ret >= 0)
        return 1;

    return ret;
}

static void
KMSDRM_DeleteDevice(SDL_VideoDevice * device)
{
    if (device->driverdata) {
        SDL_free(device->driverdata);
        device->driverdata = NULL;
    }

    SDL_free(device);

    SDL_KMSDRM_UnloadSymbols();
}

static int
KMSDRM_GetDisplayDPI(_THIS, SDL_VideoDisplay * display, float * ddpi, float * hdpi, float * vdpi)
{
    int w, h;

    uint32_t display_mm_width;
    uint32_t display_mm_height;

    SDL_DisplayData *dispdata;

    dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0); //viddata->devindex);


    if (!dispdata) {
        return SDL_SetError("No available displays");
    }

    display_mm_width = dispdata->connector->connector->mmWidth;
    display_mm_height = dispdata->connector->connector->mmHeight;

    w = dispdata->mode.hdisplay;
    h = dispdata->mode.vdisplay;

    *hdpi = display_mm_width ? (((float) w) * 25.4f / display_mm_width) : 0.0f;
    *vdpi = display_mm_height ? (((float) h) * 25.4f / display_mm_height) : 0.0f;
    *ddpi = SDL_ComputeDiagonalDPI(w, h, ((float) display_mm_width) / 25.4f,((float) display_mm_height) / 25.4f);

    return 0;
}

static SDL_VideoDevice *
KMSDRM_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *viddata;

    if (!KMSDRM_Available()) {
        return NULL;
    }

    if (!devindex || (devindex > 99)) {
        devindex = get_driindex();
    }

    if (devindex < 0) {
        SDL_SetError("devindex (%d) must be between 0 and 99.", devindex);
        return NULL;
    }

    if (!SDL_KMSDRM_LoadSymbols()) {
        return NULL;
    }

    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    viddata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (!viddata) {
        SDL_OutOfMemory();
        goto cleanup;
    }
    viddata->devindex = devindex;
    viddata->drm_fd = -1;

    device->driverdata = viddata;

    /* Setup all functions that can be handled from this backend. */
    device->VideoInit = KMSDRM_VideoInit;
    device->VideoQuit = KMSDRM_VideoQuit;
    device->GetDisplayModes = KMSDRM_GetDisplayModes;
    device->SetDisplayMode = KMSDRM_SetDisplayMode;
    device->GetDisplayDPI = KMSDRM_GetDisplayDPI;
    device->CreateSDLWindow = KMSDRM_CreateWindow;
    device->CreateSDLWindowFrom = KMSDRM_CreateWindowFrom;
    device->SetWindowTitle = KMSDRM_SetWindowTitle;
    device->SetWindowIcon = KMSDRM_SetWindowIcon;
    device->SetWindowPosition = KMSDRM_SetWindowPosition;
    device->SetWindowSize = KMSDRM_SetWindowSize;
    device->SetWindowFullscreen = KMSDRM_SetWindowFullscreen;
    device->ShowWindow = KMSDRM_ShowWindow;
    device->HideWindow = KMSDRM_HideWindow;
    device->RaiseWindow = KMSDRM_RaiseWindow;
    device->MaximizeWindow = KMSDRM_MaximizeWindow;
    device->MinimizeWindow = KMSDRM_MinimizeWindow;
    device->RestoreWindow = KMSDRM_RestoreWindow;
    device->SetWindowGrab = KMSDRM_SetWindowGrab;
    device->DestroyWindow = KMSDRM_DestroyWindow;
    device->GetWindowWMInfo = KMSDRM_GetWindowWMInfo;
#if SDL_VIDEO_OPENGL_EGL
    device->GL_DefaultProfileConfig = KMSDRM_GLES_DefaultProfileConfig;
    device->GL_LoadLibrary = KMSDRM_GLES_LoadLibrary;
    device->GL_GetProcAddress = KMSDRM_GLES_GetProcAddress;
    device->GL_UnloadLibrary = KMSDRM_GLES_UnloadLibrary;
    device->GL_CreateContext = KMSDRM_GLES_CreateContext;
    device->GL_MakeCurrent = KMSDRM_GLES_MakeCurrent;
    device->GL_SetSwapInterval = KMSDRM_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = KMSDRM_GLES_GetSwapInterval;
    device->GL_SwapWindow = KMSDRM_GLES_SwapWindow;
    device->GL_DeleteContext = KMSDRM_GLES_DeleteContext;
#endif
    device->PumpEvents = KMSDRM_PumpEvents;
    device->free = KMSDRM_DeleteDevice;
#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = KMSDRM_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = KMSDRM_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = KMSDRM_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = KMSDRM_Vulkan_CreateSurface;
    device->Vulkan_GetDrawableSize = KMSDRM_Vulkan_GetDrawableSize;
#endif
    return device;

cleanup:
    if (device)
        SDL_free(device);
    if (viddata)
        SDL_free(viddata);
    return NULL;
}

VideoBootStrap KMSDRM_bootstrap = {
    "KMSDRM",
    "KMS/DRM Video Driver",
    KMSDRM_CreateDevice
};


static void
KMSDRM_FBDestroyCallback(struct gbm_bo *bo, void *data)
{
    KMSDRM_FBInfo *fb_info = (KMSDRM_FBInfo *)data;

    if (fb_info && fb_info->drm_fd >= 0 && fb_info->fb_id != 0) {
        KMSDRM_drmModeRmFB(fb_info->drm_fd, fb_info->fb_id);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Delete DRM FB %u", fb_info->fb_id);
    }

    SDL_free(fb_info);
}

KMSDRM_FBInfo *
KMSDRM_FBFromBO(_THIS, struct gbm_bo *bo)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    unsigned width, height;
    uint32_t format, strides[4] = {0}, handles[4] = {0}, offsets[4] = {0};
    const int num_planes = KMSDRM_gbm_bo_get_plane_count(bo);
    unsigned int i;
    int ret;

    /* Check for an existing framebuffer */
    KMSDRM_FBInfo *fb_info = (KMSDRM_FBInfo *)KMSDRM_gbm_bo_get_user_data(bo);

    if (fb_info) {
        return fb_info;
    }

    /* Create a structure that contains the info about framebuffer
       that we need to use it. */
    fb_info = (KMSDRM_FBInfo *)SDL_calloc(1, sizeof(KMSDRM_FBInfo));
    if (!fb_info) {
        SDL_OutOfMemory();
        return NULL;
    }

    fb_info->drm_fd = viddata->drm_fd;

    width = KMSDRM_gbm_bo_get_width(bo);
    height = KMSDRM_gbm_bo_get_height(bo);
    format = KMSDRM_gbm_bo_get_format(bo);

    for (i = 0; i < num_planes; i++) {
        strides[i] = KMSDRM_gbm_bo_get_stride_for_plane(bo, i);
        handles[i] = KMSDRM_gbm_bo_get_handle(bo).u32;
        offsets[i] = KMSDRM_gbm_bo_get_offset(bo, i);
    }

    /* Create framebuffer object for the buffer.
       It's VERY important to note that fb_id is what we use to set the FB_ID prop
       of a plane when using the ATOMIC interface, and we get the fb_id here. */
    ret = KMSDRM_drmModeAddFB2(viddata->drm_fd, width, height, format,
            handles, strides, offsets, &fb_info->fb_id, 0);

    if (ret) {
      SDL_free(fb_info);
      return NULL;
    }

    /* Set the userdata pointer. This pointer is used to store custom data that we need
       to access in the future, so we store the fb_id here for later use, because fb_id is
       what we need to set the FB_ID property of a plane when using the ATOMIC interface. */
    KMSDRM_gbm_bo_set_user_data(bo, fb_info, KMSDRM_FBDestroyCallback);

    return fb_info;
}

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/* _this is a SDL_VideoDevice *                                              */
/*****************************************************************************/

/* Deinitializes the dispdata members needed for KMSDRM operation that are
   inoffeensive for VK compatibility. */
void KMSDRM_DisplayDataDeinit (_THIS, SDL_DisplayData *dispdata) {
    /* Free connector */
    if (dispdata && dispdata->connector) {
        if (dispdata->connector->connector) {
            KMSDRM_drmModeFreeConnector(dispdata->connector->connector);
            dispdata->connector->connector = NULL;
        }
        if (dispdata->connector->props_info) {
            SDL_free(dispdata->connector->props_info);
            dispdata->connector->props_info = NULL;
        }
        SDL_free(dispdata->connector);
        dispdata->connector = NULL;
    }

    /* Free CRTC */
    if (dispdata && dispdata->crtc) {
        if (dispdata->crtc->crtc) {
            KMSDRM_drmModeFreeCrtc(dispdata->crtc->crtc);
            dispdata->crtc->crtc = NULL;
        }
        if (dispdata->crtc->props_info) {
            SDL_free(dispdata->crtc->props_info);
            dispdata->crtc->props_info = NULL;
        }
        SDL_free(dispdata->crtc);
        dispdata->crtc = NULL;
    }
}

/* Initializes the dispdata members needed for KMSDRM operation that are
   inoffeensive for VK compatibility, except we must leave the drm_fd
   closed when we get to the end of this function.
   This is to be called early, in VideoInit(), because it gets us
   the videomode information, which SDL needs immediately after VideoInit(). */
int KMSDRM_DisplayDataInit (_THIS, SDL_DisplayData *dispdata) {
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    drmModeRes *resources = NULL;
    drmModeEncoder *encoder = NULL;
    drmModeConnector *connector = NULL;
    drmModeCrtc *crtc = NULL;

    char devname[32];
    int ret = 0;
    unsigned i,j;

    dispdata->atomic_flags = 0;
    dispdata->atomic_req = NULL;
    dispdata->kms_fence = NULL;
    dispdata->gpu_fence = NULL;
    dispdata->kms_out_fence_fd = -1;
    dispdata->modeset_pending = SDL_FALSE;
    dispdata->gbm_init = SDL_FALSE;

    dispdata->display_plane = NULL;
    dispdata->cursor_plane = NULL;

    dispdata->cursor_bo = NULL;

    /* Open /dev/dri/cardNN */
    SDL_snprintf(viddata->devpath, sizeof(viddata->devpath), "/dev/dri/card%d", viddata->devindex);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opening device %s", devname);
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);

    if (viddata->drm_fd < 0) {
        ret = SDL_SetError("Could not open %s", devname);
        goto cleanup;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opened DRM FD (%d)", viddata->drm_fd);

    /********************************************/
    /* Block for enabling ATOMIC compatibility. */
    /********************************************/

    /* Set ATOMIC & UNIVERSAL PLANES compatibility */
    ret = set_client_caps(viddata->drm_fd);
    if (ret) {
        goto cleanup;
    }

    /*******************************************/
    /* Block for getting the ATOMIC resources. */
    /*******************************************/

    /* Get all of the available connectors / devices / crtcs */
    resources = KMSDRM_drmModeGetResources(viddata->drm_fd);
    if (!resources) {
        ret = SDL_SetError("drmModeGetResources(%d) failed", viddata->drm_fd);
        goto cleanup;
    }

    /* Iterate on the available connectors to find a connected connector. */
    for (i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = KMSDRM_drmModeGetConnector(viddata->drm_fd,
            resources->connectors[i]);

        if (!conn) {
            continue;
        }

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found connector %d with %d modes.",
                         conn->connector_id, conn->count_modes);
            connector = conn;

            break;
        }

        KMSDRM_drmModeFreeConnector(conn);
    }

    if (!connector) {
        ret = SDL_SetError("No currently active connector found.");
        goto cleanup;
    }

    /* Try to find the connector's current encoder */
    for (i = 0; i < resources->count_encoders; i++) {
        encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

        if (!encoder) {
          continue;
        }

        if (encoder->encoder_id == connector->encoder_id) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found encoder %d.", encoder->encoder_id);
            break;
        }

        KMSDRM_drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        /* No encoder was connected, find the first supported one */
        for (i = 0; i < resources->count_encoders; i++) {
            encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

            if (!encoder) {
              continue;
            }

            for (j = 0; j < connector->count_encoders; j++) {
                if (connector->encoders[j] == encoder->encoder_id) {
                    break;
                }
            }

            if (j != connector->count_encoders) {
              break;
            }

            KMSDRM_drmModeFreeEncoder(encoder);
            encoder = NULL;
        }
    }

    if (!encoder) {
        ret = SDL_SetError("No connected encoder found.");
        goto cleanup;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found encoder %d.", encoder->encoder_id);

    /* Try to find a CRTC connected to this encoder */
    crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);

    /* If no CRTC was connected to the encoder, find the first CRTC
       that is supported by the encoder, and use that. */
    if (!crtc) {
        for (i = 0; i < resources->count_crtcs; i++) {
            if (encoder->possible_crtcs & (1 << i)) {
                encoder->crtc_id = resources->crtcs[i];
                crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);
                break;
            }
        }
    }

    if (!crtc) {
        ret = SDL_SetError("No CRTC found.");
        goto cleanup;
    }

    /* Figure out the default mode to be set. */
    dispdata->mode = crtc->mode;

    /* Find the connector's preferred mode, to be used in case the current mode
       is not valid, or if restoring the current mode fails.
       We can always count on the preferred mode! */
    for (i = 0; i < connector->count_modes; i++) {
        if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
            dispdata->preferred_mode = connector->modes[i];
        }
    }

    /* If the current CRTC's mode isn't valid, select the preferred
       mode of the connector. */
    if (crtc->mode_valid == 0) {
        dispdata->mode = dispdata->preferred_mode;
    }

    if (dispdata->mode.hdisplay == 0 || dispdata->mode.vdisplay == 0 ) {
        ret = SDL_SetError("Couldn't get a valid connector videomode.");
        goto cleanup;
    }

    /* Get CRTC properties */
    dispdata->crtc->props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        crtc->crtc_id, DRM_MODE_OBJECT_CRTC);

    dispdata->crtc->props_info = SDL_calloc(dispdata->crtc->props->count_props,
        sizeof(*dispdata->crtc->props_info));

    if (!dispdata->crtc->props_info) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    for (i = 0; i < dispdata->crtc->props->count_props; i++) {
        dispdata->crtc->props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
        dispdata->crtc->props->props[i]);
    }

    /* Get connector properties */
    dispdata->connector->props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);

    dispdata->connector->props_info = SDL_calloc(dispdata->connector->props->count_props,
        sizeof(*dispdata->connector->props_info));

    if (!dispdata->connector->props_info) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    for (i = 0; i < dispdata->connector->props->count_props; i++) {
        dispdata->connector->props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
        dispdata->connector->props->props[i]);
    }

    /* Store the connector and crtc for future use. This is all we keep from this function,
       and these are just structs, inoffensive to VK. */
    dispdata->connector->connector = connector;
    dispdata->crtc->crtc = crtc;

    /***********************************/
    /* Block fpr Vulkan compatibility. */
    /***********************************/

    /* THIS IS FOR VULKAN! Leave the FD closed, so VK can work.
       Will reopen this in CreateWindow, but only if requested a non-VK window. */
    KMSDRM_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_ATOMIC, 0);
    KMSDRM_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 0);
    close (viddata->drm_fd);
    viddata->drm_fd = -1;

cleanup:
    if (encoder)
        KMSDRM_drmModeFreeEncoder(encoder);
    if (resources)
        KMSDRM_drmModeFreeResources(resources);
    if (ret) {
        /* Error (complete) cleanup */
        if (dispdata->connector->connector) {
            KMSDRM_drmModeFreeConnector(dispdata->connector->connector);
            dispdata->connector->connector = NULL;
        }
        if (dispdata->crtc->props_info) {
            SDL_free(dispdata->crtc->props_info);
            dispdata->crtc->props_info = NULL;
        }
        if (dispdata->connector->props_info) {
            SDL_free(dispdata->connector->props_info);
            dispdata->connector->props_info = NULL;
        }
        if (dispdata->crtc->crtc) {
            KMSDRM_drmModeFreeCrtc(dispdata->crtc->crtc);
            dispdata->crtc->crtc = NULL;
        }
        if (viddata->drm_fd >= 0) {
            close(viddata->drm_fd);
            viddata->drm_fd = -1;
        }
    }

    return ret;
}

/* Init the Vulkan-INCOMPATIBLE stuff:
   Reopen FD, create gbm dev, create dumb buffer and setup display plane.
   This is to be called late, in WindowCreate(), and ONLY if this is not
   a Vulkan window.
   We are doing this so late to allow Vulkan to work if we build a VK window.
   These things are incompatible with Vulkan, which accesses the same resources
   internally so they must be free when trying to build a Vulkan surface.
*/
int
KMSDRM_GBMInit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    int ret = 0;

    /* Reopen the FD! */
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);
    set_client_caps(viddata->drm_fd);

    /* Create the GBM device. */
    viddata->gbm_dev = KMSDRM_gbm_create_device(viddata->drm_fd);
    if (!viddata->gbm_dev) {
        ret = SDL_SetError("Couldn't create gbm device.");
    }

    /* Setup the display plane. ONLY do this after dispdata has the right
       crtc and connector, because these are used in this function. */
    ret = setup_plane(_this, &(dispdata->display_plane), DRM_PLANE_TYPE_PRIMARY);
    if (ret) {
        ret = SDL_SetError("can't find suitable display plane.");
    }

    dispdata->gbm_init = SDL_TRUE;

    return ret;
}

/* Deinit the Vulkan-incompatible KMSDRM stuff. */
void
KMSDRM_GBMDeinit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    /* Free display plane */
    free_plane(&dispdata->display_plane);

    /* Free cursor plane (if still not freed) */
    free_plane(&dispdata->cursor_plane);

    /* Destroy GBM device. GBM surface is destroyed by DestroySurfaces(),
       already called when we get here. */
    if (viddata->gbm_dev) {
        KMSDRM_gbm_device_destroy(viddata->gbm_dev);
        viddata->gbm_dev = NULL;
    }

    /* Finally close DRM FD. May be reopen on next non-vulkan window creation. */
    if (viddata->drm_fd >= 0) {
        close(viddata->drm_fd);
        viddata->drm_fd = -1;
    }

    dispdata->gbm_init = SDL_FALSE;
}

/* Destroy the window surfaces and buffers. Have the PRIMARY PLANE
   disconnected from these buffers before doing so, or have the PRIMARY PLANE
   reading the original FB where the TTY lives, before doing this, or CRTC will
   be disconnected by the kernel. */
void
KMSDRM_DestroySurfaces(_THIS, SDL_Window *window)
{
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    KMSDRM_PlaneInfo plane_info = {0};

#if SDL_VIDEO_OPENGL_EGL
    EGLContext egl_context;
#endif

    /********************************************************************/
    /* BLOCK 1: protect the PRIMARY PLANE before destroying the buffers */
    /* it's using, by making it point to the original CRTC buffer,      */
    /* where the TTY console should be.                                 */
    /********************************************************************/

    plane_info.plane = dispdata->display_plane;
    plane_info.crtc_id = dispdata->crtc->crtc->crtc_id;
    plane_info.fb_id = dispdata->crtc->crtc->buffer_id;
    plane_info.src_w = dispdata->mode.hdisplay;
    plane_info.src_h = dispdata->mode.vdisplay;
    plane_info.crtc_w = dispdata->mode.hdisplay;
    plane_info.crtc_h = dispdata->mode.vdisplay;

    drm_atomic_set_plane_props(&plane_info);

    /* Issue blocking atomic commit. */
    if (drm_atomic_commit(_this, SDL_TRUE)) {
        SDL_SetError("Failed to issue atomic commit on surfaces destruction.");
    }

    /****************************************************************************/
    /* BLOCK 2: We can finally destroy the window GBM and EGL surfaces, and     */
    /* GBM buffers now that the buffers are not being used by the PRIMARY PLANE */
    /* anymore.                                                                 */
    /****************************************************************************/

    /* Destroy the GBM surface and buffers. */
    if (windata->bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
        windata->bo = NULL;
    }

    if (windata->next_bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->next_bo);
        windata->next_bo = NULL;
    }

    /***************************************************************************/
    /* Destroy the EGL surface.                                                */
    /* In this eglMakeCurrent() call, we disable the current EGL surface       */
    /* because we're going to destroy it, but DON'T disable the EGL context,   */
    /* because it won't be enabled again until the programs ask for a pageflip */
    /* so we get to SwapWindow().                                              */
    /* If we disable the context until then and a program tries to retrieve    */
    /* the context version info before calling for a pageflip, the program     */
    /* will get wrong info and we will be in trouble.                          */
    /***************************************************************************/

#if SDL_VIDEO_OPENGL_EGL
    egl_context = (EGLContext)SDL_GL_GetCurrentContext();
    SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, egl_context);

    if (windata->egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, windata->egl_surface);
        windata->egl_surface = EGL_NO_SURFACE;
    }
#endif

    if (windata->gs) {
        KMSDRM_gbm_surface_destroy(windata->gs);
        windata->gs = NULL;
    }

}

int
KMSDRM_CreateSurfaces(_THIS, SDL_Window * window)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_WindowData *windata = (SDL_WindowData *)window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    uint32_t surface_fmt = GBM_FORMAT_ARGB8888;
    uint32_t surface_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    uint32_t width, height;

    EGLContext egl_context;

    int ret = 0;

    if (((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ||
       ((window->flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN)) {

        width = dispdata->mode.hdisplay;
        height = dispdata->mode.vdisplay;
    } else {
        width = window->w;
        height = window->h;
    }

    if (!KMSDRM_gbm_device_is_format_supported(viddata->gbm_dev, surface_fmt, surface_flags)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "GBM surface format not supported. Trying anyway.");
    }

    windata->gs = KMSDRM_gbm_surface_create(viddata->gbm_dev, width, height, surface_fmt, surface_flags);

    if (!windata->gs) {
        return SDL_SetError("Could not create GBM surface");
    }

#if SDL_VIDEO_OPENGL_EGL
    /* We can't get the EGL context yet because SDL_CreateRenderer has not been called,
       but we need an EGL surface NOW, or GL won't be able to render into any surface
       and we won't see the first frame. */
    SDL_EGL_SetRequiredVisualId(_this, surface_fmt);
    windata->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)windata->gs);

    if (windata->egl_surface == EGL_NO_SURFACE) {
        ret = SDL_SetError("Could not create EGL window surface");
        goto cleanup;
    }

    /* Current context passing to EGL is now done here. If something fails,
       go back to delayed SDL_EGL_MakeCurrent() call in SwapWindow. */
    egl_context = (EGLContext)SDL_GL_GetCurrentContext();
    ret = SDL_EGL_MakeCurrent(_this, windata->egl_surface, egl_context);

#endif

cleanup:

    if (ret) {
        /* Error (complete) cleanup. */
        if (windata->gs) {
            KMSDRM_gbm_surface_destroy(windata->gs);
            windata->gs = NULL;
        }
    }

    return ret;
}

void
KMSDRM_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_VideoData *viddata = windata->viddata;
    SDL_bool is_vulkan = window->flags & SDL_WINDOW_VULKAN; /* Is this a VK window? */
    unsigned int i, j;

    if (!windata) {
        return;
    }

    if (!is_vulkan) {
        KMSDRM_DestroySurfaces(_this, window);
#if SDL_VIDEO_OPENGL_EGL
        if (_this->egl_data) {
            SDL_EGL_UnloadLibrary(_this);
        }
#endif
        if (dispdata->gbm_init) {
            KMSDRM_DeinitMouse(_this);
            KMSDRM_GBMDeinit(_this, dispdata);
        }
    }

    /********************************************/
    /* Remove from the internal SDL window list */
    /********************************************/

    for (i = 0; i < viddata->num_windows; i++) {
        if (viddata->windows[i] == window) {
            viddata->num_windows--;

            for (j = i; j < viddata->num_windows; j++) {
                viddata->windows[j] = viddata->windows[j + 1];
            }

            break;
        }
    }

    /*********************************************************************/
    /* Free the window driverdata. Bye bye, surface and buffer pointers! */
    /*********************************************************************/
    window->driverdata = NULL;
    SDL_free(windata);
}

/*****************************************************************************/
/* Reconfigure the window scaling parameters and re-construct it's surfaces, */
/* without destroying the window itself.                                     */
/* To be used by SetWindowSize() and SetWindowFullscreen().                  */
/*****************************************************************************/
static int
KMSDRM_ReconfigureWindow( _THIS, SDL_Window * window) {
    SDL_WindowData *windata = window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_bool is_vulkan = window->flags & SDL_WINDOW_VULKAN; /* Is this a VK window? */
    float ratio;

    if (((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ||
       ((window->flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN)) {

        windata->src_w = dispdata->mode.hdisplay;
        windata->src_h = dispdata->mode.vdisplay;
        windata->output_w = dispdata->mode.hdisplay;
        windata->output_h = dispdata->mode.vdisplay;
        windata->output_x = 0;

    } else {

        /* Normal non-fullscreen windows are scaled using the CRTC,
           so get output (CRTC) size and position, for AR correction. */
        ratio = (float)window->w / (float)window->h;
        windata->src_w = window->w;
        windata->src_h = window->h;
        windata->output_w = dispdata->mode.vdisplay * ratio;
        windata->output_h = dispdata->mode.vdisplay;
        windata->output_x = (dispdata->mode.hdisplay - windata->output_w) / 2;

    }

    if (!is_vulkan) {
        if (KMSDRM_CreateSurfaces(_this, window)) {
            return -1;
        }
    }
    return 0;
}

int
KMSDRM_VideoInit(_THIS)
{
    int ret = 0;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = NULL;
    SDL_VideoDisplay display = {0};

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "KMSDRM_VideoInit()");

    viddata->video_init = SDL_FALSE;

    dispdata = (SDL_DisplayData *) SDL_calloc(1, sizeof(SDL_DisplayData));
    if (!dispdata) {
        return SDL_OutOfMemory();
    }

    /* Alloc memory for these. */
    dispdata->display_plane = SDL_calloc(1, sizeof(*dispdata->display_plane));
    dispdata->crtc = SDL_calloc(1, sizeof(*dispdata->crtc));
    dispdata->connector = SDL_calloc(1, sizeof(*dispdata->connector));
    if (!(dispdata->display_plane) || !(dispdata->crtc) || !(dispdata->connector)) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    /* Get KMSDRM resources info and store what we need. Getting and storing
       this info isn't a problem for VK compatibility.
       For VK-incompatible initializations we have KMSDRM_GBMInit(), which is
       called on window creation, and only when we know it's not a VK window. */
    if (KMSDRM_DisplayDataInit(_this, dispdata)) {
        ret = SDL_SetError("error getting KMS/DRM information");
        goto cleanup;
    }

    /* Setup the single display that's available.
       There's no problem with it being still incomplete. */
    display.driverdata = dispdata;
    display.desktop_mode.w = dispdata->mode.hdisplay;
    display.desktop_mode.h = dispdata->mode.vdisplay;
    display.desktop_mode.refresh_rate = dispdata->mode.vrefresh;
    display.desktop_mode.format = SDL_PIXELFORMAT_ARGB8888;
    display.current_mode = display.desktop_mode;

    /* Add the display only when it's ready, */
    SDL_AddVideoDisplay(&display, SDL_FALSE);

    /* Use this if you ever need to see info on all available planes. */
#if 0
    get_planes_info(_this);
#endif

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Init();
#endif

    viddata->video_init = SDL_TRUE;

cleanup:

    if (ret) {
        /* Error (complete) cleanup */
        if (dispdata->display_plane) {
            SDL_free(dispdata->display_plane);
        }
        if (dispdata->crtc) {
            SDL_free(dispdata->crtc);
        }
        if (dispdata->connector) {
            SDL_free(dispdata->connector);
        }

        SDL_free(dispdata);
    }

    return ret;
}

/* The driverdata pointers, like dispdata, viddata, windata, etc...
   are freed by SDL internals, so not our job. */
void
KMSDRM_VideoQuit(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    KMSDRM_DisplayDataDeinit(_this, dispdata);

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#endif

    /* Clear out the window list */
    SDL_free(viddata->windows);
    viddata->windows = NULL;
    viddata->max_windows = 0;
    viddata->num_windows = 0;
    viddata->video_init = SDL_FALSE;
}

#if 0
void
KMSDRM_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /* Only one display mode available: the current one */
    SDL_AddDisplayMode(display, &display->current_mode);
}
#endif

/* We only change the video mode for FULLSCREEN windows
   that are not FULLSCREEN_DESKTOP.
   Normal non-fullscreen windows are scaled using the CRTC. */
void
KMSDRM_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DisplayData *dispdata = display->driverdata;
    drmModeConnector *conn = dispdata->connector->connector;
    SDL_DisplayMode mode;
    int i;

    for (i = 0; i < conn->count_modes; i++) {
        SDL_DisplayModeData *modedata = SDL_calloc(1, sizeof(SDL_DisplayModeData));

        if (!modedata) {
            SDL_OutOfMemory();
            return;
        }

        modedata->mode_index = i;

        mode.w = conn->modes[i].hdisplay;
        mode.h = conn->modes[i].vdisplay;
        mode.refresh_rate = conn->modes[i].vrefresh;
        mode.format = SDL_PIXELFORMAT_ARGB8888;
        mode.driverdata = modedata;

        if (!SDL_AddDisplayMode(display, &mode)) {
            SDL_free(modedata);
        }
    }
}

int
KMSDRM_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    /* Set the dispdata->mode to the new mode and leave actual modesetting
       pending to be done on SwapWindow(), to be included on next atomic
       commit changeset. */

    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)display->driverdata;
    SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;
    drmModeConnector *conn = dispdata->connector->connector;
    int i;

    if (!modedata) {
        return SDL_SetError("Mode doesn't have an associated index");
    }

    /* Take note of the new mode. It will be used in SwapWindow to
       set the props needed for mode setting. */
    dispdata->mode = conn->modes[modedata->mode_index];

    dispdata->modeset_pending = SDL_TRUE;

    for (i = 0; i < viddata->num_windows; i++) {
        SDL_Window *window = viddata->windows[i];

    if (KMSDRM_CreateSurfaces(_this, window)) {
        return -1;
    }

        /* Tell app about the window resize */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, mode->w, mode->h);
    }

    return 0;
}

int
KMSDRM_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = NULL;
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *dispdata = display->driverdata;
    SDL_bool is_vulkan = window->flags & SDL_WINDOW_VULKAN; /* Is this a VK window? */
    NativeDisplayType egl_display;
    float ratio;
    int ret = 0;

    if ( !(dispdata->gbm_init) && (!is_vulkan)) {
         /* Reopen FD, create gbm dev, setup display plane, etc,.
            but only when we come here for the first time,
            and only if it's not a VK window. */
         if ((ret = KMSDRM_GBMInit(_this, dispdata))) {
                 goto cleanup;
         }

#if SDL_VIDEO_OPENGL_EGL
         /* Manually load the EGL library. KMSDRM_EGL_LoadLibrary() has already
            been called by SDL_CreateWindow() but we don't do anything there,
            precisely to be able to load it here.
            If we let SDL_CreateWindow() load the lib, it will be loaded
            before we call KMSDRM_GBMInit(), causing GLES programs to fail. */
         if (!_this->egl_data) {
             egl_display = (NativeDisplayType)((SDL_VideoData *)_this->driverdata)->gbm_dev;
             if ((ret = SDL_EGL_LoadLibrary(_this, NULL, egl_display, EGL_PLATFORM_GBM_MESA))) {
                 goto cleanup;
             }
         }
#endif

         /* Can't init mouse stuff sooner because cursor plane is not ready. */
         KMSDRM_InitMouse(_this);

         /* Since we take cursor buffer way from the cursor plane and
            destroy the cursor GBM BO when we destroy a window, we must
            also manually re-show the cursor on screen, if necessary,
            when we create a window. */
         KMSDRM_InitCursor();
    }

    /* Allocate window internal data */
    windata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!windata) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    if (((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ||
       ((window->flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN))
    {
        windata->src_w = dispdata->mode.hdisplay;
        windata->src_h = dispdata->mode.vdisplay;
        windata->output_w = dispdata->mode.hdisplay;
        windata->output_h = dispdata->mode.vdisplay;
        windata->output_x = 0;
    } else {
        /* Normal non-fullscreen windows are scaled using the CRTC,
           so get output (CRTC) size and position, for AR correction. */
        ratio = (float)window->w / (float)window->h;
        windata->src_w = window->w;
        windata->src_h = window->h;
        windata->output_w = dispdata->mode.vdisplay * ratio;
        windata->output_h = dispdata->mode.vdisplay;
        windata->output_x = (dispdata->mode.hdisplay - windata->output_w) / 2;
    }

    /* Don't force fullscreen on all windows: it confuses programs that try
       to set a window fullscreen after creating it as non-fullscreen (sm64ex) */
    // window->flags |= SDL_WINDOW_FULLSCREEN;

    /* Setup driver data for this window */
    windata->viddata = viddata;
    window->driverdata = windata;

    if (!is_vulkan) {
        if ((ret = KMSDRM_CreateSurfaces(_this, window))) {
            goto cleanup;
        }
    }

    /* Add window to the internal list of tracked windows. Note, while it may
       seem odd to support multiple fullscreen windows, some apps create an
       extra window as a dummy surface when working with multiple contexts */
    if (viddata->num_windows >= viddata->max_windows) {
        unsigned int new_max_windows = viddata->max_windows + 1;
        viddata->windows = (SDL_Window **)SDL_realloc(viddata->windows,
              new_max_windows * sizeof(SDL_Window *));
        viddata->max_windows = new_max_windows;

        if (!viddata->windows) {
            ret = SDL_OutOfMemory();
            goto cleanup;
        }
    }

    viddata->windows[viddata->num_windows++] = window;

    /* Focus on the newly created window */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    /***********************************************************/
    /* Tell SDL that the mouse has entered the window using an */
    /* artificial event: we have no windowing system to tell   */
    /* SDL that it has happened. This makes SDL set the        */
    /* SDL_WINDOW_MOUSE_FOCUS on this window, thus fixing      */
    /* Scummvm sticky-on-sides software cursor.                */
    /***********************************************************/
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_ENTER, 0, 0);

cleanup:

    if (ret) {
        /* Allocated windata will be freed in KMSDRM_DestroyWindow */
        KMSDRM_DestroyWindow(_this, window);
    }
    return ret;
}

int
KMSDRM_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void
KMSDRM_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
KMSDRM_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
KMSDRM_SetWindowPosition(_THIS, SDL_Window * window)
{
}

void
KMSDRM_SetWindowSize(_THIS, SDL_Window * window)
{
    if (KMSDRM_ReconfigureWindow(_this, window)) {
        SDL_SetError("Can't reconfigure window on SetWindowSize.");
    }
}

void
KMSDRM_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen)
{
    if (KMSDRM_ReconfigureWindow(_this, window)) {
        SDL_SetError("Can't reconfigure window on SetWindowFullscreen.");
    }
}

void
KMSDRM_ShowWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_HideWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{

}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
KMSDRM_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("application not compiled with SDL %d.%d\n",
                      SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_KMSDRM */

/* vi: set ts=4 sw=4 expandtab: */
