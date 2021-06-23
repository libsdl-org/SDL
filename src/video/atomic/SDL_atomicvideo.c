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

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_syswm.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

#ifdef SDL_INPUT_LINUXEV
#include "../../core/linux/SDL_evdev.h"
#endif

/* KMSDRM ATOMIC declarations */
#include "SDL_atomicvideo.h"
#include "SDL_atomicevents.h"
#include "SDL_atomicgles.h"
#include "SDL_atomicmouse.h"
#include "SDL_atomicdyn.h"
#include "SDL_atomicvulkan.h"
#include "SDL_atomicinterface.h"
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>

/* for older ATOMIC headers... */
#ifndef DRM_FORMAT_MOD_VENDOR_NONE
#define DRM_FORMAT_MOD_VENDOR_NONE 0
#endif
#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR fourcc_mod_code(NONE, 0)
#endif

#define ATOMIC_DRI_PATH "/dev/dri/"

static int
set_client_caps (int fd)
{
    if (ATOMIC_drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
         return SDL_SetError("no atomic modesetting support.");
    }
    if (ATOMIC_drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
         return SDL_SetError("no universal planes support.");
    }
    return 0;
}

static int
check_modesetting(int devindex)
{
    SDL_bool available = SDL_FALSE;
    char device[512];
    int drm_fd;

    SDL_snprintf(device, sizeof (device), "%scard%d", ATOMIC_DRI_PATH, devindex);

    drm_fd = open(device, O_RDWR | O_CLOEXEC);
    if (drm_fd >= 0) {
        if (SDL_ATOMIC_LoadSymbols()) {
            drmModeRes *resources = ATOMIC_drmModeGetResources(drm_fd);
            if (resources) {

                SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                  "%scard%d connector, encoder and CRTC counts are: %d %d %d",
                   ATOMIC_DRI_PATH, devindex,
                   resources->count_connectors, resources->count_encoders,
                   resources->count_crtcs);

                if (resources->count_connectors > 0
                 && resources->count_encoders > 0
                 && resources->count_crtcs > 0)
                {
                    available = SDL_TRUE;
                }
                ATOMIC_drmModeFreeResources(resources);
            }
            SDL_ATOMIC_UnloadSymbols();
        }
        close(drm_fd);
    }

    return available;
}

static unsigned int
get_dricount(void)
{
    unsigned int devcount = 0;
    struct dirent *res;
    struct stat sb;
    DIR *folder;

    if (!(stat(ATOMIC_DRI_PATH, &sb) == 0 && S_ISDIR(sb.st_mode))) {
        SDL_SetError("The path %s cannot be opened or is not available",
        ATOMIC_DRI_PATH);
        return 0;
    }

    if (access(ATOMIC_DRI_PATH, F_OK) == -1) {
        SDL_SetError("The path %s cannot be opened",
            ATOMIC_DRI_PATH);
        return 0;
    }

    folder = opendir(ATOMIC_DRI_PATH);
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

static int
ATOMIC_Available(void)
{
    int ret = -ENOENT;

    ret = get_driindex();
    if (ret >= 0)
        return 1;

   return ret;
}

static void
ATOMIC_DeleteDevice(SDL_VideoDevice * device)
{
    if (device->driverdata) {
        SDL_free(device->driverdata);
        device->driverdata = NULL;
    }

    SDL_free(device);

    SDL_ATOMIC_UnloadSymbols();
}

static int
ATOMIC_GetDisplayDPI(_THIS, SDL_VideoDisplay * display, float * ddpi, float * hdpi, float * vdpi)
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
ATOMIC_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *viddata;

    if (!ATOMIC_Available()) {
        return NULL;
    }

    if (!devindex || (devindex > 99)) {
        devindex = get_driindex();
    }

    if (devindex < 0) {
        SDL_SetError("devindex (%d) must be between 0 and 99.", devindex);
        return NULL;
    }

    if (!SDL_ATOMIC_LoadSymbols()) {
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

    viddata->vulkan_mode = SDL_FALSE;

    device->driverdata = viddata;

    /* Setup all functions that can be handled from this backend. */
    device->VideoInit = ATOMIC_VideoInit;
    device->VideoQuit = ATOMIC_VideoQuit;
    device->GetDisplayModes = ATOMIC_GetDisplayModes;
    device->SetDisplayMode = ATOMIC_SetDisplayMode;
    device->GetDisplayDPI = ATOMIC_GetDisplayDPI;
    device->CreateSDLWindow = ATOMIC_CreateWindow;
    device->CreateSDLWindowFrom = ATOMIC_CreateWindowFrom;
    device->SetWindowTitle = ATOMIC_SetWindowTitle;
    device->SetWindowIcon = ATOMIC_SetWindowIcon;
    device->SetWindowPosition = ATOMIC_SetWindowPosition;
    device->SetWindowSize = ATOMIC_SetWindowSize;
    device->SetWindowFullscreen = ATOMIC_SetWindowFullscreen;
    device->ShowWindow = ATOMIC_ShowWindow;
    device->HideWindow = ATOMIC_HideWindow;
    device->RaiseWindow = ATOMIC_RaiseWindow;
    device->MaximizeWindow = ATOMIC_MaximizeWindow;
    device->MinimizeWindow = ATOMIC_MinimizeWindow;
    device->RestoreWindow = ATOMIC_RestoreWindow;
    device->DestroyWindow = ATOMIC_DestroyWindow;
    device->GetWindowWMInfo = ATOMIC_GetWindowWMInfo;

    device->GL_DefaultProfileConfig = ATOMIC_GLES_DefaultProfileConfig;
    device->GL_GetProcAddress = ATOMIC_GLES_GetProcAddress;
    device->GL_CreateContext = ATOMIC_GLES_CreateContext;
    device->GL_MakeCurrent = ATOMIC_GLES_MakeCurrent;
    device->GL_SetSwapInterval = ATOMIC_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = ATOMIC_GLES_GetSwapInterval;
    device->GL_SwapWindow = ATOMIC_GLES_SwapWindow;
    device->GL_DeleteContext = ATOMIC_GLES_DeleteContext;
    /* Those two functions are dummy. We do these things manually. */
    device->GL_LoadLibrary = ATOMIC_GLES_LoadLibrary;
    device->GL_UnloadLibrary = ATOMIC_GLES_UnloadLibrary;

#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = ATOMIC_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = ATOMIC_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = ATOMIC_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = ATOMIC_Vulkan_CreateSurface;
    device->Vulkan_GetDrawableSize = ATOMIC_Vulkan_GetDrawableSize;
#endif

    device->PumpEvents = ATOMIC_PumpEvents;
    device->free = ATOMIC_DeleteDevice;

    return device;

cleanup:
    if (device)
        SDL_free(device);
    if (viddata)
        SDL_free(viddata);
    return NULL;
}

VideoBootStrap ATOMIC_bootstrap = {
    "ATOMIC",
    "ATOMIC KMS/DRM Video Driver",
    ATOMIC_CreateDevice
};

static void
ATOMIC_FBDestroyCallback(struct gbm_bo *bo, void *data)
{
    ATOMIC_FBInfo *fb_info = (ATOMIC_FBInfo *)data;

    if (fb_info && fb_info->drm_fd >= 0 && fb_info->fb_id != 0) {
        ATOMIC_drmModeRmFB(fb_info->drm_fd, fb_info->fb_id);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Delete DRM FB %u", fb_info->fb_id);
    }

    SDL_free(fb_info);
}

ATOMIC_FBInfo *
ATOMIC_FBFromBO(_THIS, struct gbm_bo *bo)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    unsigned width, height;
    uint32_t format, strides[4] = {0}, handles[4] = {0}, offsets[4] = {0};
    const int num_planes = ATOMIC_gbm_bo_get_plane_count(bo);
    unsigned int i;
    int ret;

    /* Check for an existing framebuffer */
    ATOMIC_FBInfo *fb_info = (ATOMIC_FBInfo *)ATOMIC_gbm_bo_get_user_data(bo);

    if (fb_info) {
        return fb_info;
    }

    /* Create a structure that contains the info about framebuffer
       that we need to use it. */
    fb_info = (ATOMIC_FBInfo *)SDL_calloc(1, sizeof(ATOMIC_FBInfo));
    if (!fb_info) {
        SDL_OutOfMemory();
        return NULL;
    }

    fb_info->drm_fd = viddata->drm_fd;

    width = ATOMIC_gbm_bo_get_width(bo);
    height = ATOMIC_gbm_bo_get_height(bo);
    format = ATOMIC_gbm_bo_get_format(bo);

    for (i = 0; i < num_planes; i++) {
        strides[i] = ATOMIC_gbm_bo_get_stride_for_plane(bo, i);
        handles[i] = ATOMIC_gbm_bo_get_handle(bo).u32;
        offsets[i] = ATOMIC_gbm_bo_get_offset(bo, i);
    }

    /* Create framebuffer object for the buffer.
       It's VERY important to note that fb_id is what we use to set the FB_ID prop
       of a plane when using the ATOMIC interface, and we get the fb_id here. */
    ret = ATOMIC_drmModeAddFB2(viddata->drm_fd, width, height, format,
            handles, strides, offsets, &fb_info->fb_id, 0);

    if (ret) {
      SDL_free(fb_info);
      return NULL;
    }

    /* Set the userdata pointer. This pointer is used to store custom data that we need
       to access in the future, so we store the fb_id here for later use, because fb_id is
       what we need to set the FB_ID property of a plane when using the ATOMIC interface. */
    ATOMIC_gbm_bo_set_user_data(bo, fb_info, ATOMIC_FBDestroyCallback);

    return fb_info;
}

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions.                  */
/* _this is an SDL_VideoDevice *                                              */
/*****************************************************************************/

/* Deinitializes the dispdata members needed for ATOMIC operation that are
   inoffeensive for VK compatibility. */
void ATOMIC_DisplayDataDeinit (_THIS, SDL_DisplayData *dispdata) {
    /* Free connector */
    if (dispdata && dispdata->connector) {
        if (dispdata->connector->connector) {
            ATOMIC_drmModeFreeConnector(dispdata->connector->connector);
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
            ATOMIC_drmModeFreeCrtc(dispdata->crtc->crtc);
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

/* Initializes the dispdata members needed for ATOMIC operation that are
   inoffeensive for VK compatibility, except we must leave the drm_fd
   closed when we get to the end of this function.
   This is to be called early, in VideoInit(), because it gets us
   the videomode information, which SDL needs immediately after VideoInit(). */
int ATOMIC_DisplayDataInit (_THIS, SDL_DisplayData *dispdata) {
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    drmModeRes *resources = NULL;
    drmModeEncoder *encoder = NULL;
    drmModeConnector *connector = NULL;
    drmModeCrtc *crtc = NULL;

    char devname[32];
    int ret = 0;
    unsigned i,j;

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
    resources = ATOMIC_drmModeGetResources(viddata->drm_fd);
    if (!resources) {
        ret = SDL_SetError("drmModeGetResources(%d) failed", viddata->drm_fd);
        goto cleanup;
    }

    /* Iterate on the available connectors to find a connected connector. */
    for (i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = ATOMIC_drmModeGetConnector(viddata->drm_fd,
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

        ATOMIC_drmModeFreeConnector(conn);
    }

    if (!connector) {
        ret = SDL_SetError("No currently active connector found.");
        goto cleanup;
    }

    /* Try to find the connector's current encoder */
    for (i = 0; i < resources->count_encoders; i++) {
        encoder = ATOMIC_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

        if (!encoder) {
          continue;
        }

        if (encoder->encoder_id == connector->encoder_id) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found encoder %d.", encoder->encoder_id);
            break;
        }

        ATOMIC_drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        /* No encoder was connected, find the first supported one */
        for (i = 0; i < resources->count_encoders; i++) {
            encoder = ATOMIC_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

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

            ATOMIC_drmModeFreeEncoder(encoder);
            encoder = NULL;
        }
    }

    if (!encoder) {
        ret = SDL_SetError("No connected encoder found.");
        goto cleanup;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Found encoder %d.", encoder->encoder_id);

    /* Try to find a CRTC connected to this encoder */
    crtc = ATOMIC_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);

    /* If no CRTC was connected to the encoder, find the first CRTC
       that is supported by the encoder, and use that. */
    if (!crtc) {
        for (i = 0; i < resources->count_crtcs; i++) {
            if (encoder->possible_crtcs & (1 << i)) {
                encoder->crtc_id = resources->crtcs[i];
                crtc = ATOMIC_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);
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

    /* Save the original mode for restoration on quit. */
    dispdata->original_mode = dispdata->mode;

    if (dispdata->mode.hdisplay == 0 || dispdata->mode.vdisplay == 0 ) {
        ret = SDL_SetError("Couldn't get a valid connector videomode.");
        goto cleanup;
    }

    /* Get CRTC properties */
    dispdata->crtc->props = ATOMIC_drmModeObjectGetProperties(viddata->drm_fd,
        crtc->crtc_id, DRM_MODE_OBJECT_CRTC);

    dispdata->crtc->props_info = SDL_calloc(dispdata->crtc->props->count_props,
        sizeof(*dispdata->crtc->props_info));

    if (!dispdata->crtc->props_info) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    for (i = 0; i < dispdata->crtc->props->count_props; i++) {
        dispdata->crtc->props_info[i] = ATOMIC_drmModeGetProperty(viddata->drm_fd,
        dispdata->crtc->props->props[i]);
    }

    /* Get connector properties */
    dispdata->connector->props = ATOMIC_drmModeObjectGetProperties(viddata->drm_fd,
        connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);

    dispdata->connector->props_info = SDL_calloc(dispdata->connector->props->count_props,
        sizeof(*dispdata->connector->props_info));

    if (!dispdata->connector->props_info) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    for (i = 0; i < dispdata->connector->props->count_props; i++) {
        dispdata->connector->props_info[i] = ATOMIC_drmModeGetProperty(viddata->drm_fd,
        dispdata->connector->props->props[i]);
    }

    /* Store the connector and crtc for future use. This is all we keep from this function,
       and these are just structs, inoffensive to VK. */
    dispdata->connector->connector = connector;
    dispdata->crtc->crtc = crtc;

    /***********************************/
    /* Block for Vulkan compatibility. */
    /***********************************/

    /* Leave the FD closed, so VULKAN can work.
       This will be reopen in CreateWindow, but only if a non-VK window is requested. */
    ATOMIC_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_ATOMIC, 0);
    ATOMIC_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 0);
    close (viddata->drm_fd);
    viddata->drm_fd = -1;

cleanup:
    if (encoder)
        ATOMIC_drmModeFreeEncoder(encoder);
    if (resources)
        ATOMIC_drmModeFreeResources(resources);
    if (ret) {
        /* Error (complete) cleanup */
        if (dispdata->connector->connector) {
            ATOMIC_drmModeFreeConnector(dispdata->connector->connector);
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
            ATOMIC_drmModeFreeCrtc(dispdata->crtc->crtc);
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
ATOMIC_GBMInit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    int ret = 0;

    /* Reopen the FD! */
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);
    set_client_caps(viddata->drm_fd);

    /* Create the GBM device. */
    viddata->gbm_dev = ATOMIC_gbm_create_device(viddata->drm_fd);
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

/* Deinit the Vulkan-incompatible ATOMIC stuff. */
void
ATOMIC_GBMDeinit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    /* Free display plane */
    free_plane(&dispdata->display_plane);

    /* Free cursor plane (if still not freed) */
    free_plane(&dispdata->cursor_plane);

    /* Destroy GBM device. GBM surface is destroyed by DestroySurfaces(),
       already called when we get here. */
    if (viddata->gbm_dev) {
        ATOMIC_gbm_device_destroy(viddata->gbm_dev);
        viddata->gbm_dev = NULL;
    }

    /* Finally close DRM FD. May be reopen on next non-vulkan window creation. */
    if (viddata->drm_fd >= 0) {
        close(viddata->drm_fd);
        viddata->drm_fd = -1;
    }

    dispdata->gbm_init = SDL_FALSE;
}

void
ATOMIC_DestroySurfaces(_THIS, SDL_Window *window)
{
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    ATOMIC_PlaneInfo plane_info = {0};

    /* TODO : Continue investigating why this doesn't work. We should do this instead
       of making the display plane point to the TTY console, which isn't there
       after creating and destroying a Vulkan window. */
#if 0
    /* Disconnect the connector from the CRTC (remember: several connectors
       can read a CRTC), deactivate the CRTC, and set the PRIMARY PLANE props
       CRTC_ID and FB_ID to 0. Then we can destroy the GBM buffers and surface. */
    add_connector_property(dispdata->atomic_req, dispdata->connector , "CRTC_ID", 0);
    add_crtc_property(dispdata->atomic_req, dispdata->crtc , "MODE_ID", 0); 
    add_crtc_property(dispdata->atomic_req, dispdata->crtc , "active", 0); 

    plane_info.plane = dispdata->display_plane;
    plane_info.crtc_id = 0;
    plane_info.fb_id = 0;

    drm_atomic_set_plane_props(&plane_info);

    /* Issue atomic commit that is blocking and allows modesetting. */
    if (drm_atomic_commit(_this, SDL_TRUE, SDL_TRUE)) {
        SDL_SetError("Failed to issue atomic commit on surfaces destruction.");
    }
#endif

#if 1
    /************************************************************/
    /* Make the display plane point to the original TTY buffer. */
    /* We have to configure it's input and output scaling       */
    /* parameters accordingly.                                  */
    /************************************************************/

    plane_info.plane = dispdata->display_plane;
    plane_info.crtc_id = dispdata->crtc->crtc->crtc_id;
    plane_info.fb_id = dispdata->crtc->crtc->buffer_id;
    plane_info.src_w = dispdata->original_mode.hdisplay;
    plane_info.src_h = dispdata->original_mode.vdisplay;
    plane_info.crtc_w = dispdata->original_mode.hdisplay;
    plane_info.crtc_h = dispdata->original_mode.vdisplay;

    drm_atomic_set_plane_props(&plane_info);

    if (drm_atomic_commit(_this, SDL_TRUE, SDL_FALSE)) {
        SDL_SetError("Failed to issue atomic commit on surfaces destruction.");
    }
#endif

    /***************************/
    /* Destroy the EGL surface */
    /***************************/

    SDL_EGL_MakeCurrent(_this, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (windata->egl_surface != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, windata->egl_surface);
        windata->egl_surface = EGL_NO_SURFACE;
    }

    /***************************/
    /* Destroy the GBM buffers */
    /***************************/

    if (windata->bo) {
        ATOMIC_gbm_surface_release_buffer(windata->gs, windata->bo);
        windata->bo = NULL;
    }

    if (windata->next_bo) {
        ATOMIC_gbm_surface_release_buffer(windata->gs, windata->next_bo);
        windata->next_bo = NULL;
    }

    /***************************/
    /* Destroy the GBM surface */
    /***************************/

    if (windata->gs) {
        ATOMIC_gbm_surface_destroy(windata->gs);
        windata->gs = NULL;
    }
}

int
ATOMIC_CreateSurfaces(_THIS, SDL_Window * window)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_WindowData *windata = (SDL_WindowData *)window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    uint32_t surface_fmt = GBM_FORMAT_ARGB8888;
    uint32_t surface_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    uint32_t width, height;

    EGLContext egl_context;

    int ret = 0;

    /* If the current window already has surfaces, destroy them before creating other.
       This is mainly for ReconfigureWindow(), where we simply call CreateSurfaces()
       for regenerating a window's surfaces. */
    if (windata->gs) {
        ATOMIC_DestroySurfaces(_this, window);
    }

    if ( ((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
      || ((window->flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN))
    {
        width = dispdata->mode.hdisplay;
        height = dispdata->mode.vdisplay;
    } else {
        width = window->w;
        height = window->h;
    }

    if (!ATOMIC_gbm_device_is_format_supported(viddata->gbm_dev,
        surface_fmt, surface_flags)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "GBM surface format not supported. Trying anyway.");
    }

    windata->gs = ATOMIC_gbm_surface_create(viddata->gbm_dev,
        width, height, surface_fmt, surface_flags);

    if (!windata->gs) {
        return SDL_SetError("Could not create GBM surface");
    }

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

cleanup:

    if (ret) {
        /* Error (complete) cleanup. */
        if (windata->gs) {
            ATOMIC_gbm_surface_destroy(windata->gs);
            windata->gs = NULL;
        }
    }

    return ret;
}

void
ATOMIC_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_VideoData *viddata = windata->viddata;
    SDL_bool is_vulkan = window->flags & SDL_WINDOW_VULKAN; /* Is this a VK window? */
    unsigned int i, j;

    if (!windata) {
        return;
    }

    if ( !is_vulkan && dispdata->gbm_init ) {

        /* Free cursor plane. */
        ATOMIC_DeinitMouse(_this);

        /* Destroy GBM surface and buffers. */
        ATOMIC_DestroySurfaces(_this, window);

        /* Unload EGL library. */
        if (_this->egl_data) {
            SDL_EGL_UnloadLibrary(_this);
        }

        /* Unload GL library. */
        if (_this->gl_config.driver_loaded) {
            SDL_GL_UnloadLibrary();
        }

        /* Free display plane, and destroy GBM device. */
        ATOMIC_GBMDeinit(_this, dispdata);
    }

    else {
        /* If we were in Vulkan mode, get out of it. */
        if (viddata->vulkan_mode) {
            viddata->vulkan_mode = SDL_FALSE;
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
ATOMIC_ReconfigureWindow( _THIS, SDL_Window * window) {
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
        /* Normal non-fullscreen windows are scaled using the PRIMARY PLANE, so here we store:
           input size (ie: the size of the window buffers),
           output size (ie: the mode configured on the CRTC),
           X position (to compensate for AR correction).
           These are used when we set the PRIMARY PLANE props in SwapWindow() */
        ratio = (float)window->w / (float)window->h;
        windata->src_w = window->w;
        windata->src_h = window->h;
        windata->output_w = dispdata->mode.vdisplay * ratio;
        windata->output_h = dispdata->mode.vdisplay;
        windata->output_x = (dispdata->mode.hdisplay - windata->output_w) / 2;
    }

    if (!is_vulkan) {
        if (ATOMIC_CreateSurfaces(_this, window)) {
            return -1;
        }
    }
    return 0;
}

int
ATOMIC_VideoInit(_THIS)
{
    int ret = 0;

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = NULL;
    SDL_VideoDisplay display = {0};

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "ATOMIC_VideoInit()");

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

    /* Get ATOMIC resources info and store what we need. Getting and storing
       this info isn't a problem for VK compatibility.
       For VK-incompatible initializations we have ATOMIC_GBMInit(), which is
       called on window creation, and only when we know it's not a VK window. */
    if (ATOMIC_DisplayDataInit(_this, dispdata)) {
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

    /* Since we create and show the default cursor on ATOMIC_InitMouse() and
       we call ATOMIC_InitMouse() everytime we create a new window, we have
       to be sure to create and show the default cursor only the first time.
       If we don't, new default cursors would stack up on mouse->cursors and SDL
       would have to hide and delete them at quit, not to mention the memory leak... */
    dispdata->set_default_cursor_pending = SDL_TRUE;

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
ATOMIC_VideoQuit(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    ATOMIC_DisplayDataDeinit(_this, dispdata);

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
ATOMIC_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /* Only one display mode available: the current one */
    SDL_AddDisplayMode(display, &display->current_mode);
}
#endif

/* We only change the video mode for FULLSCREEN windows
   that are not FULLSCREEN_DESKTOP.
   Normal non-fullscreen windows are scaled using the CRTC. */
void
ATOMIC_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
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
ATOMIC_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    /* Set the dispdata->mode to the new mode and leave actual modesetting
       pending to be done on SwapWindow(), to be included on next atomic
       commit changeset. */

    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)display->driverdata;
    SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;
    drmModeConnector *conn = dispdata->connector->connector;
    int i;

    /* Don't do anything if we are in Vulkan mode. */
    if (viddata->vulkan_mode) {
        return 0;
    }

    if (!modedata) {
        return SDL_SetError("Mode doesn't have an associated index");
    }

    /* Take note of the new mode to be set. */
    dispdata->mode = conn->modes[modedata->mode_index];

    /* Take note that we have to change mode in SwapWindow(). We have to do it there
       because we need a buffer of the new size so the commit that contains the
       mode change can be completed OK.  */
    dispdata->modeset_pending = SDL_TRUE;

    for (i = 0; i < viddata->num_windows; i++) {
        SDL_Window *window = viddata->windows[i];

    if (ATOMIC_CreateSurfaces(_this, window)) {
        return -1;
    }

        /* Tell app about the window resize */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, mode->w, mode->h);
    }

    return 0;
}

int
ATOMIC_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = NULL;
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *dispdata = display->driverdata;
    SDL_bool is_vulkan = window->flags & SDL_WINDOW_VULKAN; /* Is this a VK window? */
    SDL_bool vulkan_mode = viddata->vulkan_mode; /* Do we have any Vulkan windows? */
    NativeDisplayType egl_display;
    float ratio;
    int ret = 0;

    if ( !(dispdata->gbm_init) && !is_vulkan && !vulkan_mode ) {
 
         /* If this is not a Vulkan Window, then this is a GL window, so at the
            end of this function, we must have marked the window as being OPENGL
            and we must have loaded the GL library: both things are needed so the
            GL_CreateRenderer() and GL_LoadFunctions() calls in SDL_CreateWindow()
            succeed without having to re-create the window.
            We must load the EGL library too, which can't be loaded until the GBM
            device has been created, because SDL_EGL_Library() function uses it. */ 

         /* Maybe you didn't ask for an OPENGL window, but that's what you will get.
            See previous comment on why. */
         window->flags |= SDL_WINDOW_OPENGL;

         /* Reopen FD, create gbm dev, setup display plane, etc,.
            but only when we come here for the first time,
            and only if it's not a VK window. */
         if ((ret = ATOMIC_GBMInit(_this, dispdata))) {
                 goto cleanup;
         }

         /* Manually load the GL library. ATOMIC_EGL_LoadLibrary() has already
            been called by SDL_CreateWindow() but we don't do anything there,
            precisely to be able to load it here.
            If we let SDL_CreateWindow() load the lib, it will be loaded
            before we call ATOMIC_GBMInit(), causing GLES programs to fail. */
         if (!_this->egl_data) {
             egl_display = (NativeDisplayType)((SDL_VideoData *)_this->driverdata)->gbm_dev;
             if (SDL_EGL_LoadLibrary(_this, NULL, egl_display, EGL_PLATFORM_GBM_MESA)) {
                 goto cleanup;
             }

             if (SDL_GL_LoadLibrary(NULL) < 0) {
                goto cleanup;
             }
         }
     
         /* Can't init mouse stuff sooner because cursor plane is not ready,
            so we do it here. */
         ATOMIC_InitMouse(_this);

         /* Since we take cursor buffer away from the cursor plane and
            destroy the cursor GBM BO when we destroy a window, we must
            also manually re-show the cursor on screen, if necessary,
            when we create a window. */
         ATOMIC_InitCursor();
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

    if (!is_vulkan && !vulkan_mode) {
        /* Create the window surfaces. Needs the window diverdata in place. */
        if ((ret = ATOMIC_CreateSurfaces(_this, window))) {
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

    /* If we have just created a Vulkan window, establish that we are in Vulkan mode now. */
    viddata->vulkan_mode = is_vulkan;

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
        /* Allocated windata will be freed in ATOMIC_DestroyWindow */
        ATOMIC_DestroyWindow(_this, window);
    }
    return ret;
}

int
ATOMIC_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void
ATOMIC_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
ATOMIC_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
ATOMIC_SetWindowPosition(_THIS, SDL_Window * window)
{
}

void
ATOMIC_SetWindowSize(_THIS, SDL_Window * window)
{
    if (ATOMIC_ReconfigureWindow(_this, window)) {
        SDL_SetError("Can't reconfigure window on SetWindowSize.");
    }
}

void
ATOMIC_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen)
{
    if (ATOMIC_ReconfigureWindow(_this, window)) {
        SDL_SetError("Can't reconfigure window on SetWindowFullscreen.");
    }
}

void
ATOMIC_ShowWindow(_THIS, SDL_Window * window)
{
}
void
ATOMIC_HideWindow(_THIS, SDL_Window * window)
{
}
void
ATOMIC_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
ATOMIC_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
ATOMIC_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
ATOMIC_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
ATOMIC_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{

}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
ATOMIC_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
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

#endif /* SDL_VIDEO_DRIVER_ATOMIC */

/* vi: set ts=4 sw=4 expandtab: */
