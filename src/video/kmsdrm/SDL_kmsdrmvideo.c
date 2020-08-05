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
                    for (int i = 0; i < resources->count_connectors; i++) {
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

static int get_dricount(void)
{
    int devcount = 0;
    struct dirent *res;
    struct stat sb;
    DIR *folder;

    if (!(stat(KMSDRM_DRI_PATH, &sb) == 0
                && S_ISDIR(sb.st_mode))) {
        printf("The path %s cannot be opened or is not available\n",
               KMSDRM_DRI_PATH);
        return 0;
    }

    if (access(KMSDRM_DRI_PATH, F_OK) == -1) {
        printf("The path %s cannot be opened\n",
               KMSDRM_DRI_PATH);
        return 0;
    }

    folder = opendir(KMSDRM_DRI_PATH);
    if (folder) {
        while ((res = readdir(folder))) {
            int len = SDL_strlen(res->d_name);
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
    const int devcount = get_dricount();
    int i;

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

static int add_connector_property(drmModeAtomicReq *req, uint32_t obj_id,
					const char *name, uint64_t value)
{
        SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
	unsigned int i;
	int prop_id = 0;

	for (i = 0 ; i < dispdata->connector_props->count_props ; i++) {
		if (strcmp(dispdata->connector_props_info[i]->name, name) == 0) {
			prop_id = dispdata->connector_props_info[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no connector property: %s\n", name);
		return -EINVAL;
	}

	return KMSDRM_drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int add_crtc_property(drmModeAtomicReq *req, uint32_t obj_id,
				const char *name, uint64_t value)
{
        SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
	unsigned int i;
	int prop_id = -1;

	for (i = 0 ; i < dispdata->crtc_props->count_props ; i++) {
		if (strcmp(dispdata->crtc_props_info[i]->name, name) == 0) {
			prop_id = dispdata->crtc_props_info[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no crtc property: %s\n", name);
		return -EINVAL;
	}

	return KMSDRM_drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int add_plane_property(drmModeAtomicReq *req, uint32_t obj_id,
				const char *name, uint64_t value)
{
        SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
	unsigned int i;
	int prop_id = -1;

	for (i = 0 ; i < dispdata->plane_props->count_props ; i++) {
		if (strcmp(dispdata->plane_props_info[i]->name, name) == 0) {
			prop_id = dispdata->plane_props_info[i]->prop_id;
			break;
		}
	}

	if (prop_id < 0) {
		printf("no plane property: %s\n", name);
		return -EINVAL;
	}

	return KMSDRM_drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

#if 0

static void get_plane_properties() {
    uint32_t i;
    dispdata->plane_ = drmModeObjectGetProperties(viddata->drm_fd,
        plane->plane_id, DRM_MODE_OBJECT_PLANE);

    if (!dispdata->type.props) {
	    printf("could not get %s %u properties: %s\n",
			    #type, id, strerror(errno));
	    return NULL;
    }

    dispdata->type->props_info = calloc(dispdata->type.props->count_props,
		    sizeof(*dispdata->type->props_info));
    for (i = 0; i < dispdata->type->props->count_props; i++) {
	    dispdata->type.props_info[i] = drmModeGetProperty(viddata->drm_fd,
			    dispdata->type->props->props[i]);
    }
    return props;
}

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


    /* Remember that, to present a plane on screen, it has to be connected to a CRTC so the CRTC scans it,
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
    printf("--Usable CRTC that we have chosen: %d-- \n", dispdata->crtc->crtc_id);

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

/* Get a plane that is PRIMARY (there's no guarantee that we have overlays in all hardware,
   so we can really only count on having one primary plane) and can use the CRTC we have chosen. */ 
uint32_t get_plane_id(_THIS, drmModeRes *resources)
{
    drmModePlaneResPtr plane_resources;
    uint32_t i, j;
    uint32_t crtc_index = 0;
    int ret = -EINVAL;
    int found_primary = 0;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    /* Get the crtc_index for the current CRTC. Needed to find out if a plane supports the CRTC. */
    for (i = 0; i < resources->count_crtcs; i++) {
	if (resources->crtcs[i] == dispdata->crtc_id) {
	    crtc_index = i;
	    break;
	}
    }

    plane_resources = KMSDRM_drmModeGetPlaneResources(viddata->drm_fd);
    if (!plane_resources) {
	printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
	return -1;
    }

    /* Iterate on all the available planes. */
    for (i = 0; (i < plane_resources->count_planes) && !found_primary; i++) {

	uint32_t plane_id = plane_resources->planes[i];

	drmModePlanePtr plane = KMSDRM_drmModeGetPlane(viddata->drm_fd, plane_id);
	if (!plane) {
	    printf("drmModeGetPlane(%u) failed: %s\n", plane_id, strerror(errno));
	    continue;
	}

        /* See if the current CRTC is available for this plane. */
	if (plane->possible_crtcs & (1 << crtc_index)) {

	    drmModeObjectPropertiesPtr props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
                                                   plane_id, DRM_MODE_OBJECT_PLANE);
	    ret = plane_id;

            /* Search the plane props, to see if it's a primary plane. */
	    for (j = 0; j < props->count_props; j++) {

		drmModePropertyPtr p = KMSDRM_drmModeGetProperty(viddata->drm_fd, props->props[j]);

		if ((strcmp(p->name, "type") == 0) &&
		    (props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY)) {
		    /* found our primary plane, lets use that: */
		    found_primary = 1;
		}

                KMSDRM_drmModeFreeProperty(p);
	    }

	    KMSDRM_drmModeFreeObjectProperties(props);
	}

	KMSDRM_drmModeFreePlane(plane);
    }

    KMSDRM_drmModeFreePlaneResources(plane_resources);

    return ret;
}

int drm_atomic_commit(_THIS, uint32_t fb_id, uint32_t flags)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    uint32_t plane_id = dispdata->plane->plane_id;
    uint32_t blob_id;
    drmModeAtomicReq *req;
    int ret;

    req = KMSDRM_drmModeAtomicAlloc();

    if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
	if (add_connector_property(req, dispdata->connector->connector_id, "CRTC_ID",
				   dispdata->crtc_id) < 0)
	    return -1;

	if (KMSDRM_drmModeCreatePropertyBlob(viddata->drm_fd, &dispdata->mode, sizeof(dispdata->mode),
					     &blob_id) != 0)
	    return -1;

	if (add_crtc_property(req, dispdata->crtc_id, "MODE_ID", blob_id) < 0)
	    return -1;

	if (add_crtc_property(req, dispdata->crtc_id, "ACTIVE", 1) < 0)
	    return -1;
    }

    add_plane_property(req, plane_id, "FB_ID", fb_id);
    add_plane_property(req, plane_id, "CRTC_ID", dispdata->crtc_id);
    add_plane_property(req, plane_id, "SRC_X", 0);
    add_plane_property(req, plane_id, "SRC_Y", 0);
    add_plane_property(req, plane_id, "SRC_W", dispdata->mode.hdisplay << 16);
    add_plane_property(req, plane_id, "SRC_H", dispdata->mode.vdisplay << 16);
    add_plane_property(req, plane_id, "CRTC_X", 0);
    add_plane_property(req, plane_id, "CRTC_Y", 0);
    add_plane_property(req, plane_id, "CRTC_W", dispdata->mode.hdisplay);
    add_plane_property(req, plane_id, "CRTC_H", dispdata->mode.vdisplay);

    if (dispdata->kms_in_fence_fd != -1) {
	add_crtc_property(req, dispdata->crtc_id, "OUT_FENCE_PTR",
			  VOID2U64(&dispdata->kms_out_fence_fd));
	add_plane_property(req, plane_id, "IN_FENCE_FD", dispdata->kms_in_fence_fd);
    }

    ret = KMSDRM_drmModeAtomicCommit(viddata->drm_fd, req, flags, NULL);
    if (ret)
	goto out;

    if (dispdata->kms_in_fence_fd != -1) {
	close(dispdata->kms_in_fence_fd);
	dispdata->kms_in_fence_fd = -1;
    }

out:
    KMSDRM_drmModeAtomicFree(req);

    return ret;
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
        SDL_SetError("devindex (%d) must be between 0 and 99.\n", devindex);
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
    unsigned w,h;
    int ret;
    Uint32 stride, handle;

    /* Check for an existing framebuffer */
    KMSDRM_FBInfo *fb_info = (KMSDRM_FBInfo *)KMSDRM_gbm_bo_get_user_data(bo);

    if (fb_info) {
        return fb_info;
    }

    /* Create a structure that contains enough info to remove the framebuffer
       when the backing buffer is destroyed */
    fb_info = (KMSDRM_FBInfo *)SDL_calloc(1, sizeof(KMSDRM_FBInfo));

    if (!fb_info) {
        SDL_OutOfMemory();
        return NULL;
    }

    fb_info->drm_fd = viddata->drm_fd;

    /* Create framebuffer object for the buffer */
    w = KMSDRM_gbm_bo_get_width(bo);
    h = KMSDRM_gbm_bo_get_height(bo);
    stride = KMSDRM_gbm_bo_get_stride(bo);
    handle = KMSDRM_gbm_bo_get_handle(bo).u32;
    ret = KMSDRM_drmModeAddFB(viddata->drm_fd, w, h, 24, 32, stride, handle,
                                  &fb_info->fb_id);
    if (ret) {
      SDL_free(fb_info);
      return NULL;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "New DRM FB (%u): %ux%u, stride %u from BO %p",
                 fb_info->fb_id, w, h, stride, (void *)bo);

    /* Associate our DRM framebuffer with this buffer object */
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
    Uint32 surface_fmt = GBM_FORMAT_XRGB8888;
    Uint32 surface_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
#if SDL_VIDEO_OPENGL_EGL
    EGLContext egl_context;
#endif

    if (!KMSDRM_gbm_device_is_format_supported(viddata->gbm_dev, surface_fmt, surface_flags)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "GBM surface format not supported. Trying anyway.");
    }

#if SDL_VIDEO_OPENGL_EGL
    SDL_EGL_SetRequiredVisualId(_this, surface_fmt);
    egl_context = (EGLContext)SDL_GL_GetCurrentContext();
#endif

    KMSDRM_DestroySurfaces(_this, window);

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

    windata->egl_surface_dirty = SDL_FALSE;
#endif

    /* We take note here about the need to do a modeset in the atomic_commit(),
       called in KMSDRM_GLES_SwapWindow(). */
    dispdata->modeset_pending = SDL_TRUE;

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
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = KMSDRM_drmModeGetConnector(viddata->drm_fd, resources->connectors[i]);

        if (!conn) {
            continue;
        }

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found connector %d with %d modes.",
                         conn->connector_id, conn->count_modes);
            dispdata->connector = conn;
            dispdata->connector_id = conn->connector_id;
            break;
        }

        KMSDRM_drmModeFreeConnector(conn);
    }

    if (!dispdata->connector) {
        ret = SDL_SetError("No currently active connector found.");
        goto cleanup;
    }

    /* Try to find the connector's current encoder */
    for (int i = 0; i < resources->count_encoders; i++) {
        encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

        if (!encoder) {
          continue;
        }

        if (encoder->encoder_id == dispdata->connector->encoder_id) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found encoder %d.", encoder->encoder_id);
            break;
        }

        KMSDRM_drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        /* No encoder was connected, find the first supported one */
        for (int i = 0, j; i < resources->count_encoders; i++) {
            encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

            if (!encoder) {
              continue;
            }

            for (j = 0; j < dispdata->connector->count_encoders; j++) {
                if (dispdata->connector->encoders[j] == encoder->encoder_id) {
                    break;
                }
            }

            if (j != dispdata->connector->count_encoders) {
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
    dispdata->crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);

    /* If no CRTC was connected to the encoder, find the first CRTC that is supported by the encoder, and use that. */
    if (!dispdata->crtc) {
        for (int i = 0; i < resources->count_crtcs; i++) {
            if (encoder->possible_crtcs & (1 << i)) {
                encoder->crtc_id = resources->crtcs[i];
                dispdata->crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);
                break;
            }
        }
    }

    if (!dispdata->crtc) {
        ret = SDL_SetError("No CRTC found.");
        goto cleanup;
    }

    /* SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Saved crtc_id %u, fb_id %u, (%u,%u), %ux%u",
                 dispdata->crtc->crtc_id, dispdata->crtc->buffer_id, dispdata->crtc->x,
                 dispdata->crtc->y, dispdata->crtc->width, dispdata->crtc->height);
    */

    dispdata->crtc_id = dispdata->crtc->crtc_id;
    dispdata->crtc = KMSDRM_drmModeGetCrtc(viddata->drm_fd, dispdata->crtc_id);

    /****************/
    /* Atomic block */
    /****************/

    /* Initialize the fences and their fds: */
    dispdata->kms_fence = NULL;
    dispdata->gpu_fence = NULL;
    dispdata->kms_out_fence_fd = -1,
    dispdata->kms_in_fence_fd  = -1,
    dispdata->modeset_pending  = SDL_FALSE;

    /*********************/
    /* Atomic block ends */
    /*********************/

    /* Figure out the default mode to be set. If the current CRTC's mode isn't
       valid, select the first mode supported by the connector

       FIXME find first mode that specifies DRM_MODE_TYPE_PREFERRED */
    dispdata->mode = dispdata->crtc->mode;

    if (dispdata->crtc->mode_valid == 0) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
            "Current mode is invalid, selecting connector's mode #0.");
        dispdata->mode = dispdata->connector->modes[0];
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

    /* DRM mode index for the desktop mode is needed to complete desktop mode init NOW,
       so look for it in the DRM modes array. 
       This is needed because SetDisplayMode() uses the mode index, and some programs
       change to fullscreen desktop video mode as they start. */
    for (int i = 0; i < dispdata->connector->count_modes; i++) {
        if (!SDL_memcmp(dispdata->connector->modes + i, &dispdata->crtc->mode, sizeof(drmModeModeInfo))) {
            SDL_DisplayModeData *modedata = SDL_calloc(1, sizeof(SDL_DisplayModeData));
            if (modedata) {
                modedata->mode_index = i;
                display.desktop_mode.driverdata = modedata;
            }
        }
    }

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

    dispdata->plane_id = get_plane_id(_this, resources);
    if (!dispdata->plane_id) {
        ret = SDL_SetError("could not find a suitable plane.");
        goto cleanup;
    }

    dispdata->plane = KMSDRM_drmModeGetPlane(viddata->drm_fd, dispdata->plane_id);

    /* Use this if you ever need to see info on all available planes. */
#if 0
    get_planes_info(_this);
#endif

    /* We only do single plane to single crtc to single connector, no
     * fancy multi-monitor or multi-plane stuff.  So just grab the
     * plane/crtc/connector property info for one of each:
     */

    /* Get PLANE properties */
    dispdata->plane_props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        dispdata->plane_id, DRM_MODE_OBJECT_PLANE);	

    dispdata->plane_props_info = SDL_calloc(dispdata->plane_props->count_props,
        sizeof(dispdata->plane_props_info));	

    for (int i = 0; i < dispdata->plane_props->count_props; i++) {
	dispdata->plane_props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
                                            dispdata->plane_props->props[i]);
    }

    /* Get CRTC properties */
    dispdata->crtc_props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        dispdata->crtc_id, DRM_MODE_OBJECT_CRTC);	

    dispdata->crtc_props_info = SDL_calloc(dispdata->crtc_props->count_props,
        sizeof(dispdata->crtc_props_info));	

    for (int i = 0; i < dispdata->crtc_props->count_props; i++) {
	dispdata->crtc_props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
        dispdata->crtc_props->props[i]);
    }

    /* Get connector properties */
    dispdata->connector_props = KMSDRM_drmModeObjectGetProperties(viddata->drm_fd,
        dispdata->connector_id, DRM_MODE_OBJECT_CONNECTOR);	

    dispdata->connector_props_info = SDL_calloc(dispdata->connector_props->count_props,
        sizeof(dispdata->connector_props_info));	

    for (int i = 0; i < dispdata->connector_props->count_props; i++) {
	dispdata->connector_props_info[i] = KMSDRM_drmModeGetProperty(viddata->drm_fd,
        dispdata->connector_props->props[i]);
    }

    /*********************/
    /* Atomic block ends */
    /*********************/

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Init();
#endif

    if (encoder)
        KMSDRM_drmModeFreeEncoder(encoder);
    if (resources)
        KMSDRM_drmModeFreeResources(resources);

    KMSDRM_InitMouse(_this);

    return ret;

cleanup:
    if (encoder)
        KMSDRM_drmModeFreeEncoder(encoder);
    if (resources)
        KMSDRM_drmModeFreeResources(resources);

    if (ret != 0) {
        /* Error (complete) cleanup */
        if (dispdata->connector) {
            KMSDRM_drmModeFreeConnector(dispdata->connector);
            dispdata->connector = NULL;
        }
        if (dispdata->crtc) {
            KMSDRM_drmModeFreeCrtc(dispdata->crtc);
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

/* Fn to restore original video mode and crtc buffer on quit, using the atomic interface. */
/*int
restore_video (_THIS)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    ret = drm_atomic_commit(_this, fb->fb_id, flags);

}*/

void
KMSDRM_VideoQuit(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "KMSDRM_VideoQuit()");

    if (_this->gl_config.driver_loaded) {
        SDL_GL_UnloadLibrary();
    }

    /* Clear out the window list */
    SDL_free(viddata->windows);
    viddata->windows = NULL;
    viddata->max_windows = 0;
    viddata->num_windows = 0;

    /* Restore original videomode. */
    if (viddata->drm_fd >= 0 && dispdata && dispdata->connector && dispdata->crtc) {
        uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;

        int ret = drm_atomic_commit(_this, dispdata->crtc->buffer_id, flags);

        if (ret != 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Could not restore original videomode");
        }
    }
    /****************/
    /* Atomic block */
    /****************/
    if (dispdata && dispdata->connector_props_info) {
        SDL_free(dispdata->connector_props_info); 
        dispdata->connector_props_info = NULL;
    }
    if (dispdata && dispdata->crtc_props_info) {
        SDL_free(dispdata->crtc_props_info); 
        dispdata->crtc_props_info = NULL;
    }
    if (dispdata && dispdata->plane_props_info) {
        SDL_free(dispdata->plane_props_info); 
        dispdata->plane_props_info = NULL;
    }
    /*********************/
    /* Atomic block ends */
    /*********************/
    if (dispdata && dispdata->connector) {
        KMSDRM_drmModeFreeConnector(dispdata->connector);
        dispdata->connector = NULL;
    }
    if (dispdata && dispdata->crtc) {
        KMSDRM_drmModeFreeCrtc(dispdata->crtc);
        dispdata->crtc = NULL;
    }
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
    SDL_DisplayData *dispdata = display->driverdata;
    drmModeConnector *conn = dispdata->connector;
    SDL_DisplayMode mode;

    for (int i = 0; i < conn->count_modes; i++) {
        SDL_DisplayModeData *modedata = SDL_calloc(1, sizeof(SDL_DisplayModeData));

        if (modedata) {
          modedata->mode_index = i;
        }

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
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)display->driverdata;
    SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;
    drmModeConnector *conn = dispdata->connector;

    if (!modedata) {
        return SDL_SetError("Mode doesn't have an associated index");
    }

    dispdata->mode = conn->modes[modedata->mode_index];

    for (int i = 0; i < viddata->num_windows; i++) {
        SDL_Window *window = viddata->windows[i];

        /* Re-create GBM and EGL surfaces everytime we change the display mode. */
        if (KMSDRM_CreateSurfaces(_this, window)) {
            return -1;
        }

        /* Tell app about the resize */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, mode->w, mode->h);
    }

    return 0;
}

int
KMSDRM_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_WindowData *windata;

#if SDL_VIDEO_OPENGL_EGL
    if (!_this->egl_data) {
        if (SDL_GL_LoadLibrary(NULL) < 0) {
            goto error;
        }
    }
#endif

    /* Allocate window internal data */
    windata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));

    if (!windata) {
        SDL_OutOfMemory();
        goto error;
    }

    /* Init fields. */
    windata->egl_surface_dirty  = SDL_FALSE;

    /* First remember that certain functions in SDL_Video.c will call *_SetDisplayMode when the
       SDL_WINDOW_FULLSCREEN is set and SDL_WINDOW_FULLSCREEN_DESKTOP is not set.
       So I am determining here that the behavior when creating an SDL_Window() in KMSDRM, is:

       -Creating a normal non-fullscreen window won't change the display mode.
        They won't cover the full screen area, either, because that breaks the image aspect ratio.
       -Creating a SDL_WINDOW_FULLSCREEN window will change the display mode,
        because SDL_WINDOW_FULLSCREEN flag is set.
       -Creating a SDL_WINDOW_FULLSCREEN_DESKTOP window will not change the display mode,
        because even if the SDL_WINDOW_FULLSCREEN flag is set, SDL_WINDOW_FULLSCREEN_DESKTOP prevents it.

      If we ever decide that we want to have normal windows (non-SDL_WINDOW_FULLSCREEN) should cause a display
      mode change, we could force the SDL_WINDOW_FULLSCREEN flag again on every window.
      But remember that it will break games that check if a window is FULLSCREEN or not before setting
      a fullscreen mode with SDL_SetWindowFullscreen(), like sm64ex (sm64 pc port).
      If we ever decide that we want normal windows to cover the whole screen area, we can force window->w
      and window->h to the original display mode dimensions.

    Commented reference code for all this:
       
    window->flags |= (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);

    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);

    window->w = display->desktop_mode.w;
    window->h = display->desktop_mode.h; */

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
        int new_max_windows = viddata->max_windows + 1;
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

    for (int i = 0; i < viddata->num_windows; i++) {
        if (viddata->windows[i] == window) {
            viddata->num_windows--;

            for (int j = i; j < viddata->num_windows; j++) {
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
