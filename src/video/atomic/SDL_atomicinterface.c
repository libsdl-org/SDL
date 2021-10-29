/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>
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

#if SDL_VIDEO_DRIVER_ATOMIC

#include <errno.h>
#include "SDL_atomicdyn.h"
#include "SDL_atomicvideo.h"
#include "SDL_atomicgles.h"

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

    return ATOMIC_drmModeAtomicAddProperty(req, connector->connector->connector_id, prop_id, value);
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

    return ATOMIC_drmModeAtomicAddProperty(req, crtc->crtc->crtc_id, prop_id, value);
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

    return ATOMIC_drmModeAtomicAddProperty(req, plane->plane->plane_id, prop_id, value);
}


/* Get the plane_id of a plane that is of the specified plane type (primary,
   overlay, cursor...) and can use specified CRTC. */
int get_plane_id(_THIS, unsigned int crtc_id, uint32_t plane_type)
{
    drmModeRes *resources = NULL;
    drmModePlaneResPtr plane_resources = NULL;
    uint32_t i, j;
    unsigned int crtc_index = 0;
    int ret = -EINVAL;
    int found = 0;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    resources = ATOMIC_drmModeGetResources(viddata->drm_fd);

    /* Get the crtc_index for the current CRTC.
       It's needed to find out if a plane supports the CRTC. */
    for (i = 0; i < resources->count_crtcs; i++) {
        if (resources->crtcs[i] == crtc_id) {
            crtc_index = i;
            break;
        }
    }

    plane_resources = ATOMIC_drmModeGetPlaneResources(viddata->drm_fd);
    if (!plane_resources) {
        return SDL_SetError("drmModeGetPlaneResources failed.");
    }

    /* Iterate on all the available planes. */
    for (i = 0; (i < plane_resources->count_planes) && !found; i++) {

        uint32_t plane_id = plane_resources->planes[i];

        drmModePlanePtr plane = ATOMIC_drmModeGetPlane(viddata->drm_fd, plane_id);
        if (!plane) {
            continue;
        }

        /* See if the current CRTC is available for this plane. */
        if (plane->possible_crtcs & (1 << crtc_index)) {

            drmModeObjectPropertiesPtr props = ATOMIC_drmModeObjectGetProperties(
                viddata->drm_fd, plane_id, DRM_MODE_OBJECT_PLANE);
            ret = plane_id;

            /* Iterate on the plane props to find the type of the plane,
               to see if it's of the type we want. */
            for (j = 0; j < props->count_props; j++) {

                drmModePropertyPtr p = ATOMIC_drmModeGetProperty(viddata->drm_fd,
                    props->props[j]);

                if ((strcmp(p->name, "type") == 0) && (props->prop_values[j] == plane_type)) {
                    /* found our plane, use that: */
                    found = 1;
                }

                ATOMIC_drmModeFreeProperty(p);
            }

            ATOMIC_drmModeFreeObjectProperties(props);
        }

        ATOMIC_drmModeFreePlane(plane);
    }

    ATOMIC_drmModeFreePlaneResources(plane_resources);
    ATOMIC_drmModeFreeResources(resources);

    return ret;
}

/* Setup a plane and it's props. */
int
setup_plane(_THIS, struct plane **plane, uint32_t plane_type)
{
    uint32_t plane_id;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    int ret = 0;

    *plane = SDL_calloc(1, sizeof(**plane));
    if (!(*plane)) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    /* Get plane ID for a given CRTC and plane type. */
    plane_id = get_plane_id(_this, dispdata->crtc->crtc->crtc_id, plane_type);

    if (!plane_id) {
        ret = SDL_SetError("Invalid Plane ID");
        goto cleanup;
    }

    /* Get the DRM plane itself. */
    (*plane)->plane = ATOMIC_drmModeGetPlane(viddata->drm_fd, plane_id);

    /* Get the DRM plane properties. */
    if ((*plane)->plane) {
        unsigned int i;
        (*plane)->props = ATOMIC_drmModeObjectGetProperties(viddata->drm_fd,
        (*plane)->plane->plane_id, DRM_MODE_OBJECT_PLANE);
        (*plane)->props_info = SDL_calloc((*plane)->props->count_props,
            sizeof(*(*plane)->props_info));

        if ( !((*plane)->props_info) ) {
            ret = SDL_OutOfMemory();
            goto cleanup;
        }

        for (i = 0; i < (*plane)->props->count_props; i++) {
            (*plane)->props_info[i] = ATOMIC_drmModeGetProperty(viddata->drm_fd,
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
            ATOMIC_drmModeFreePlane((*plane)->plane);
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
/*   is the buffer, and we move the "frame" from a picture to another,            */
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
drm_atomic_set_plane_props(struct ATOMIC_PlaneInfo *info)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    /* Do we have a set of changes already in the making? If not, allocate a new one. */
    if (!dispdata->atomic_req)
        dispdata->atomic_req = ATOMIC_drmModeAtomicAlloc();

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

int drm_atomic_commit(_THIS, SDL_bool blocking, SDL_bool allow_modeset)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    uint32_t atomic_flags = 0;
    int ret;

    if (!blocking) {
        atomic_flags |= DRM_MODE_ATOMIC_NONBLOCK;
    }

    if (allow_modeset) {
        atomic_flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }

    /* Never issue a new atomic commit if previous has not yet completed,
       or it will error. */
    drm_atomic_waitpending(_this);

    ret = ATOMIC_drmModeAtomicCommit(viddata->drm_fd, dispdata->atomic_req,
              atomic_flags, NULL);

    if (ret) {
        SDL_SetError("Atomic commit failed, returned %d.", ret);
        /* Uncomment this for fast-debugging */
#if 1
        printf("ATOMIC COMMIT FAILED: %s.\n", strerror(errno));
#endif
        goto out;
    }

    if (dispdata->kms_in_fence_fd != -1) {
        close(dispdata->kms_in_fence_fd);
        dispdata->kms_in_fence_fd = -1;
    }

out:
    ATOMIC_drmModeAtomicFree(dispdata->atomic_req);
    dispdata->atomic_req = NULL;

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

#if 0

void print_plane_info(_THIS, drmModePlanePtr plane)
{
    char *plane_type;
    drmModeRes *resources;
    uint32_t type = 0;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    int i;

    drmModeObjectPropertiesPtr props = ATOMIC_drmModeObjectGetProperties(viddata->drm_fd,
        plane->plane_id, DRM_MODE_OBJECT_PLANE);

    /* Search the plane props for the plane type. */
    for (i = 0; i < props->count_props; i++) {
        drmModePropertyPtr p = ATOMIC_drmModeGetProperty(viddata->drm_fd, props->props[i]);
        if ((strcmp(p->name, "type") == 0)) {
            type = props->prop_values[i];
        }

        ATOMIC_drmModeFreeProperty(p);
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
    resources = ATOMIC_drmModeGetResources(viddata->drm_fd);
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

    plane_resources = ATOMIC_drmModeGetPlaneResources(viddata->drm_fd);
    if (!plane_resources) {
        printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
        return;
    }

    printf("--Number of planes found: %d-- \n", plane_resources->count_planes);
    printf("--Usable CRTC that we have chosen: %d-- \n", dispdata->crtc->crtc->crtc_id);

    /* Iterate on all the available planes. */
    for (i = 0; (i < plane_resources->count_planes); i++) {

        uint32_t plane_id = plane_resources->planes[i];

        drmModePlanePtr plane = ATOMIC_drmModeGetPlane(viddata->drm_fd, plane_id);
        if (!plane) {
            printf("drmModeGetPlane(%u) failed: %s\n", plane_id, strerror(errno));
            continue;
        }

        /* Print plane info. */
        print_plane_info(_this, plane);
        ATOMIC_drmModeFreePlane(plane);
    }

    ATOMIC_drmModeFreePlaneResources(plane_resources);
}

#endif

#endif /* SDL_VIDEO_DRIVER_ATOMIC */

/* vi: set ts=4 sw=4 expandtab: */
