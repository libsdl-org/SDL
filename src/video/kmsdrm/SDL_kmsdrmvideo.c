/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_hints.h"
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
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>

#define KMSDRM_DRI_PATH "/dev/dri/"

static int
check_modesetting(int devindex)
{
    SDL_bool available = SDL_FALSE;
    char device[512];
    int drm_fd;

    SDL_snprintf(device, sizeof (device), "%scard%d", KMSDRM_DRI_PATH, devindex);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "check_modesetting: probing \"%s\"", device);

    drm_fd = open(device, O_RDWR | O_CLOEXEC);
    if (drm_fd >= 0) {
        if (SDL_KMSDRM_LoadSymbols()) {
            drmModeRes *resources = KMSDRM_drmModeGetResources(drm_fd);
            if (resources) {
                SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%scard%d connector, encoder and CRTC counts are: %d %d %d",
                             KMSDRM_DRI_PATH, devindex,
                             resources->count_connectors, resources->count_encoders, resources->count_crtcs);

                if (resources->count_connectors > 0 && resources->count_encoders > 0 && resources->count_crtcs > 0) {
                    for (unsigned int i = 0; i < resources->count_connectors; i++) {
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

/*********************************/
/* Atomic helper functions block */
/*********************************/

#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

#if 0
static int add_connector_property(drmModeAtomicReq *req, struct connector *connector,
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
#endif

static int add_crtc_property(drmModeAtomicReq *req, struct crtc *crtc,
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

    drmModeObjectPropertiesPtr props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        plane->plane_id, DRM_MODE_OBJECT_PLANE);

    /* Search the plane props for the plane type. */
    for (int j = 0; j < props->count_props; j++) {

	drmModePropertyPtr p = KMSDRM_drmModeGetProperty(viddata->drm_fd, props->props[j]);

	if ((strcmp(p->name, "type") == 0)) {
	    type = props->prop_values[j];
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
    for (int i = 0; i < resources->count_crtcs; i++) {
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

	    drmModeObjectPropertiesPtr props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
                                                   plane_id, DRM_MODE_OBJECT_PLANE);
	    ret = plane_id;

            /* Iterate on the plane props to find the type of the plane,
               to see if it's of the type we want. */
	    for (j = 0; j < props->count_props; j++) {

		drmModePropertyPtr p = KMSDRM_drmModeGetProperty(viddata->drm_fd,
                                           props->props[j]);

		if ((strcmp(p->name, "type") == 0) &&
		    (props->prop_values[j] == plane_type)) {
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

/* Setup cursor plane and it's props. */
int
setup_plane(_THIS, struct plane **plane, uint32_t plane_type)
{
    uint32_t plane_id;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    *plane = SDL_calloc(1, sizeof(*plane));

    /* Get plane ID. */
    plane_id = get_plane_id(_this, plane_type);

    if (!plane_id) {
        goto cleanup;
    }

    /* Get the DRM plane itself. */
    (*plane)->plane = KMSDRM_drmModeGetPlane(viddata->drm_fd, plane_id);

    /* Get the DRM plane properties. */
    if ((*plane)->plane) {
        (*plane)->props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
	    (*plane)->plane->plane_id, DRM_MODE_OBJECT_PLANE);	

        (*plane)->props_info = SDL_calloc((*plane)->props->count_props,
            sizeof((*plane)->props_info));	

        for (unsigned int i = 0; i < (*plane)->props->count_props; i++) {
            (*plane)->props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
                (*plane)->props->props[i]);
        }
    }   

    return 0;

cleanup:
    SDL_free(*plane);
    *plane = NULL;
    return -1;
}

/* Free a plane and it's props. */
void
free_plane(struct plane **plane)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    if (dispdata && (*plane)) {
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
/*   over to a CONNECTOR->ENCODER system.                                         */
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
int
drm_atomic_set_plane_props(struct KMSDRM_PlaneInfo *info)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    /* Do we have a set of changes already in the making? If not, allocate a new one. */
    if (!dispdata->atomic_req)
        dispdata->atomic_req = KMSDRM_drmModeAtomicAlloc();
   
    if (add_plane_property(dispdata->atomic_req, info->plane, "FB_ID", info->fb_id) < 0)
        return SDL_SetError("Failed to set plane FB_ID prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "CRTC_ID", info->crtc_id) < 0)
        return SDL_SetError("Failed to set plane CRTC_ID prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "SRC_W", info->src_w << 16) < 0)
        return SDL_SetError("Failed to set plane SRC_W prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "SRC_H", info->src_h << 16) < 0)
        return SDL_SetError("Failed to set plane SRC_H prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "SRC_X", info->src_x) < 0)
        return SDL_SetError("Failed to set plane SRC_X prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "SRC_Y", info->src_y) < 0)
        return SDL_SetError("Failed to set plane SRC_Y prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "CRTC_W", info->crtc_w) < 0)
        return SDL_SetError("Failed to set plane CRTC_W prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "CRTC_H", info->crtc_h) < 0)
        return SDL_SetError("Failed to set plane CRTC_H prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "CRTC_X", info->crtc_x) < 0)
        return SDL_SetError("Failed to set plane CRTC_X prop");
    if (add_plane_property(dispdata->atomic_req, info->plane, "CRTC_Y", info->crtc_y) < 0)
        return SDL_SetError("Failed to set plane CRTC_Y prop");

    /* Set the IN_FENCE and OUT_FENCE props only if we're operating on the display plane,
       since that's the only plane for which we manage who and when should access the buffers
       it uses. */
    if (info->plane == dispdata->display_plane && dispdata->kms_in_fence_fd != -1)
    {
	if (add_crtc_property(dispdata->atomic_req, dispdata->crtc, "OUT_FENCE_PTR",
			          VOID2U64(&dispdata->kms_out_fence_fd)) < 0)
            return SDL_SetError("Failed to set CRTC OUT_FENCE_PTR prop");
	if (add_plane_property(dispdata->atomic_req, info->plane, "IN_FENCE_FD", dispdata->kms_in_fence_fd) < 0)
            return SDL_SetError("Failed to set plane IN_FENCE_FD prop");
    }

    return 0;
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
        //printf("ATOMIC COMMIT FAILED: %d.\n", ret);
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
    device->CreateSDLWindow = KMSDRM_CreateWindow;
    device->CreateSDLWindowFrom = KMSDRM_CreateWindowFrom;
    device->SetWindowTitle = KMSDRM_SetWindowTitle;
    device->SetWindowIcon = KMSDRM_SetWindowIcon;
    device->SetWindowPosition = KMSDRM_SetWindowPosition;
    device->SetWindowSize = KMSDRM_SetWindowSize;
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
    device->GL_LoadLibrary = KMSDRM_GLES_LoadLibrary;
    device->GL_GetProcAddress = KMSDRM_GLES_GetProcAddress;
    device->GL_UnloadLibrary = KMSDRM_GLES_UnloadLibrary;
    device->GL_CreateContext = KMSDRM_GLES_CreateContext;
    device->GL_MakeCurrent = KMSDRM_GLES_MakeCurrent;
    device->GL_SetSwapInterval = KMSDRM_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = KMSDRM_GLES_GetSwapInterval;

    if (SDL_GetHintBoolean(SDL_HINT_VIDEO_DOUBLE_BUFFER, SDL_FALSE))
        device->GL_SwapWindow = KMSDRM_GLES_SwapWindowDB;
    else
        device->GL_SwapWindow = KMSDRM_GLES_SwapWindow;

    device->GL_DeleteContext = KMSDRM_GLES_DeleteContext;
#endif
    device->PumpEvents = KMSDRM_PumpEvents;
    device->free = KMSDRM_DeleteDevice;

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

    for (int i = 0; i < num_planes; i++) {
	strides[i] = KMSDRM_gbm_bo_get_stride_for_plane(bo, i);
	handles[i] = KMSDRM_gbm_bo_get_handle(bo).u32;
	offsets[i] = KMSDRM_gbm_bo_get_offset(bo, i);
    }

    /* Create framebuffer object for the buffer.
       It's VERY important to note that fb_id is what we ise to set the FB_ID prop of a plane
       when using the ATOMIC interface, and we get fb_id it here. */
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

static void
KMSDRM_DestroySurfaces(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = (SDL_WindowData *)window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;

    /* CAUTION: Before destroying the GBM ane EGL surfaces, we must disconnect
       the display plane from the GBM surface buffer it's reading by setting 
       it's CRTC_ID and FB_ID props to 0.
    */
    KMSDRM_PlaneInfo info = {0};
    info.plane = dispdata->display_plane;

    drm_atomic_set_plane_props(&info);
    drm_atomic_commit(_this, SDL_TRUE);

    if (windata->bo) {
       KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
        windata->bo = NULL;
    }

    if (windata->next_bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->next_bo);
        windata->next_bo = NULL;
    }

#if SDL_VIDEO_OPENGL_EGL
    SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);

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
    Uint32 width = dispdata->mode.hdisplay;
    Uint32 height = dispdata->mode.vdisplay;
    Uint32 surface_fmt = GBM_FORMAT_ARGB8888;
    Uint32 surface_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
#if SDL_VIDEO_OPENGL_EGL
    EGLContext egl_context;
#endif

    /* Always try to destroy previous surfaces before creating new ones. */
    KMSDRM_DestroySurfaces(_this, window);

    if (!KMSDRM_gbm_device_is_format_supported(viddata->gbm_dev, surface_fmt, surface_flags)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "GBM surface format not supported. Trying anyway.");
    }

#if SDL_VIDEO_OPENGL_EGL
    SDL_EGL_SetRequiredVisualId(_this, surface_fmt);
    egl_context = (EGLContext)SDL_GL_GetCurrentContext();
#endif

    windata->gs = KMSDRM_gbm_surface_create(viddata->gbm_dev, width, height, surface_fmt, surface_flags);

    if (!windata->gs) {
        return SDL_SetError("Could not create GBM surface");
    }

#if SDL_VIDEO_OPENGL_EGL
    windata->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)windata->gs);

    if (windata->egl_surface == EGL_NO_SURFACE) {
        return SDL_SetError("Could not create EGL window surface");
    }

    SDL_EGL_MakeCurrent(_this, windata->egl_surface, egl_context);

#endif

    return 0;
}

int
KMSDRM_VideoInit(_THIS)
{
    int ret = 0;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = NULL;
    drmModeRes *resources = NULL;
    drmModeEncoder *encoder = NULL;
    char devname[32];
    SDL_VideoDisplay display = {0};

    dispdata = (SDL_DisplayData *) SDL_calloc(1, sizeof(SDL_DisplayData));
    dispdata->display_plane = calloc(1, sizeof(*dispdata->display_plane));
    dispdata->crtc = calloc(1, sizeof(*dispdata->crtc));
    dispdata->connector = calloc(1, sizeof(*dispdata->connector));

    dispdata->atomic_flags = 0;
    dispdata->atomic_req = NULL;
    dispdata->kms_fence = NULL;
    dispdata->gpu_fence = NULL;
    dispdata->kms_out_fence_fd = -1;
    dispdata->kms_in_fence_fd  = -1;

    if (!dispdata) {
        return SDL_OutOfMemory();
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "KMSDRM_VideoInit()");

    /* Open /dev/dri/cardNN */
    SDL_snprintf(devname, sizeof(devname), "/dev/dri/card%d", viddata->devindex);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opening device %s", devname);
    viddata->drm_fd = open(devname, O_RDWR | O_CLOEXEC);

    if (viddata->drm_fd < 0) {
        ret = SDL_SetError("Could not open %s", devname);
        goto cleanup;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opened DRM FD (%d)", viddata->drm_fd);

    viddata->gbm_dev = KMSDRM_gbm_create_device(viddata->drm_fd);
    if (!viddata->gbm_dev) {
        ret = SDL_SetError("Couldn't create gbm device.");
        goto cleanup;
    }

    /* Get all of the available connectors / devices / crtcs */
    resources = KMSDRM_drmModeGetResources(viddata->drm_fd);
    if (!resources) {
        ret = SDL_SetError("drmModeGetResources(%d) failed", viddata->drm_fd);
        goto cleanup;
    }

    /* Iterate on the available connectors to find a connected connector. */
    for (unsigned int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = KMSDRM_drmModeGetConnector(viddata->drm_fd, resources->connectors[i]);

        if (!conn) {
            continue;
        }

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found connector %d with %d modes.",
                         conn->connector_id, conn->count_modes);
            dispdata->connector->connector = conn;
            break;
        }

        KMSDRM_drmModeFreeConnector(conn);
    }

    if (!dispdata->connector->connector) {
        ret = SDL_SetError("No currently active connector found.");
        goto cleanup;
    }

    /* Try to find the connector's current encoder */
    for (unsigned int i = 0; i < resources->count_encoders; i++) {
        encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

        if (!encoder) {
          continue;
        }

        if (encoder->encoder_id == dispdata->connector->connector->encoder_id) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found encoder %d.", encoder->encoder_id);
            break;
        }

        KMSDRM_drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        /* No encoder was connected, find the first supported one */
        for (unsigned int i = 0, j; i < resources->count_encoders; i++) {
            encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

            if (!encoder) {
              continue;
            }

            for (j = 0; j < dispdata->connector->connector->count_encoders; j++) {
                if (dispdata->connector->connector->encoders[j] == encoder->encoder_id) {
                    break;
                }
            }

            if (j != dispdata->connector->connector->count_encoders) {
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
    dispdata->crtc->crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);

    /* If no CRTC was connected to the encoder, find the first CRTC that is supported by the encoder, and use that. */
    if (!dispdata->crtc->crtc) {
        for (unsigned int i = 0; i < resources->count_crtcs; i++) {
            if (encoder->possible_crtcs & (1 << i)) {
                encoder->crtc_id = resources->crtcs[i];
                dispdata->crtc->crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);
                break;
            }
        }
    }

    if (!dispdata->crtc->crtc) {
        ret = SDL_SetError("No CRTC found.");
        goto cleanup;
    }

    /* Figure out the default mode to be set. If the current CRTC's mode isn't
       valid, select the first mode supported by the connector

       FIXME find first mode that specifies DRM_MODE_TYPE_PREFERRED */
    dispdata->mode = dispdata->crtc->crtc->mode;

    if (dispdata->crtc->crtc->mode_valid == 0) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
            "Current mode is invalid, selecting connector's mode #0.");
        dispdata->mode = dispdata->connector->connector->modes[0];
    }

    /* Setup the single display that's available */

    display.desktop_mode.w = dispdata->mode.hdisplay;
    display.desktop_mode.h = dispdata->mode.vdisplay;
    display.desktop_mode.refresh_rate = dispdata->mode.vrefresh;
#if 1
    display.desktop_mode.format = SDL_PIXELFORMAT_ARGB8888;
#else
    /* FIXME */
    drmModeFB *fb = drmModeGetFB(viddata->drm_fd, dispdata->crtc->buffer_id);
    display.desktop_mode.format = drmToSDLPixelFormat(fb->bpp, fb->depth);
    drmModeFreeFB(fb);
#endif

    display.current_mode = display.desktop_mode;
    display.driverdata = dispdata;
    SDL_AddVideoDisplay(&display);

    /****************/
    /* Atomic block */
    /****************/

    ret = KMSDRM_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret) {
        ret = SDL_SetError("no atomic modesetting support.");
        goto cleanup;
    }

    ret = KMSDRM_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
        ret = SDL_SetError("no universal planes support.");
        goto cleanup;
    }

    /* Use this if you ever need to see info on all available planes. */
#if 0
    get_planes_info(_this);
#endif

    /* Setup display plane */
    ret = setup_plane(_this, &(dispdata->display_plane), DRM_PLANE_TYPE_PRIMARY);
    if (ret) {
        ret = SDL_SetError("can't find suitable display plane.");
        goto cleanup;

    }

    /* Get CRTC properties */
    dispdata->crtc->props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        dispdata->crtc->crtc->crtc_id, DRM_MODE_OBJECT_CRTC);	

    dispdata->crtc->props_info = SDL_calloc(dispdata->crtc->props->count_props,
        sizeof(dispdata->crtc->props_info));	

    for (unsigned int i = 0; i < dispdata->crtc->props->count_props; i++) {
	dispdata->crtc->props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
        dispdata->crtc->props->props[i]);
    }

    /* Get connector properties */
    dispdata->connector->props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        dispdata->connector->connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);	

    dispdata->connector->props_info = SDL_calloc(dispdata->connector->props->count_props,
        sizeof(dispdata->connector->props_info));	

    for (unsigned int i = 0; i < dispdata->connector->props->count_props; i++) {
	dispdata->connector->props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
        dispdata->connector->props->props[i]);
    }

    /*********************/
    /* Atomic block ends */
    /*********************/

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Init();
#endif

    KMSDRM_InitMouse(_this);

cleanup:

    if (encoder)
        KMSDRM_drmModeFreeEncoder(encoder);
    if (resources)
        KMSDRM_drmModeFreeResources(resources);
    if (ret != 0) {
        /* Error (complete) cleanup */
        if (dispdata->connector->connector) {
            KMSDRM_drmModeFreeConnector(dispdata->connector->connector);
            dispdata->connector = NULL;
        }
        if (dispdata->crtc->crtc) {
            KMSDRM_drmModeFreeCrtc(dispdata->crtc->crtc);
            dispdata->crtc = NULL;
        }
        if (viddata->gbm_dev) {
            KMSDRM_gbm_device_destroy(viddata->gbm_dev);
            viddata->gbm_dev = NULL;
        }
        if (viddata->drm_fd >= 0) {
            close(viddata->drm_fd);
            viddata->drm_fd = -1;
        }
        SDL_free(dispdata);
    }

    return ret;
}

/* The driverdata pointers, like dispdata, viddata, etc...
   are freed by SDL internals, so not our job. */
void
KMSDRM_VideoQuit(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "KMSDRM_VideoQuit()");

    /* Clear out the window list */
    SDL_free(viddata->windows);
    viddata->windows = NULL;
    viddata->max_windows = 0;
    viddata->num_windows = 0;

    if (_this->gl_config.driver_loaded) {
        SDL_GL_UnloadLibrary();
    }

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

    /* Free display plane */
    free_plane(&dispdata->display_plane);

    /* Free cursor plane (if still not freed) */
    free_plane(&dispdata->cursor_plane);

    /* Destroy GBM device. GBM surface is destroyed by DestroySurfaces(). */
    if (viddata->gbm_dev) {
        KMSDRM_gbm_device_destroy(viddata->gbm_dev);
        viddata->gbm_dev = NULL;
    }
    if (viddata->drm_fd >= 0) {
        close(viddata->drm_fd);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Closed DRM FD %d", viddata->drm_fd);
        viddata->drm_fd = -1;
    }
#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#endif
}

void
KMSDRM_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /* Only one display mode available: the current one */
    SDL_AddDisplayMode(display, &display->current_mode);
}

int
KMSDRM_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    /************************************************************************/
    /* DO NOT add dynamic videomode changes unless you can REALLY test      */
    /* on all available KMS drivers and fix them in-kernel, and also test   */
    /* all SDL2 software: things will fail one way or another, and it       */
    /* greatly increases backend complexiity thus compromising it's         */
    /* maintenance. It's NOT as easy as reconstructing GBM and EGL surfaces.*/
    /************************************************************************/
    return 0;
}

int
KMSDRM_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_VideoDisplay *display = NULL;
    SDL_WindowData *windata = NULL;

#if SDL_VIDEO_OPENGL_EGL
    if (!_this->egl_data) {
        if (SDL_GL_LoadLibrary(NULL) < 0) {
            goto error;
        }
    }
#endif

    /* Allocate window internal data */
    windata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));

    display = SDL_GetDisplayForWindow(window);

    /* Windows have one size for now */
    window->w = display->desktop_mode.w;
    window->h = display->desktop_mode.h;

    /* Don't force fullscreen on all windows: it confuses programs that try
       to set a window fullscreen after creating it as non-fullscreen (sm64ex) */
    // window->flags |= SDL_WINDOW_FULLSCREEN;

    /* Setup driver data for this window */
    windata->viddata = viddata;
    window->driverdata = windata;

    if (KMSDRM_CreateSurfaces(_this, window)) {
      goto error;
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
            SDL_OutOfMemory();
            goto error;
        }
    }

    viddata->windows[viddata->num_windows++] = window;

    /* Focus on the newly created window */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    return 0;

error:
    KMSDRM_DestroyWindow(_this, window);

    return -1;
}

void
KMSDRM_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_VideoData *viddata;
    if (!windata) {
        return;
    }

    /* Remove from the internal window list */
    viddata = windata->viddata;

    for (unsigned int i = 0; i < viddata->num_windows; i++) {
        if (viddata->windows[i] == window) {
            viddata->num_windows--;

            for (unsigned int j = i; j < viddata->num_windows; j++) {
                viddata->windows[j] = viddata->windows[j + 1];
            }

            break;
        }
    }

    KMSDRM_DestroySurfaces(_this, window);

    window->driverdata = NULL;

    SDL_free(windata);
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
