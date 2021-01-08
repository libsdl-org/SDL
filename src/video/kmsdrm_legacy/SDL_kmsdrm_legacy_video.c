/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_log.h"
#include "SDL_hints.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

#ifdef SDL_INPUT_LINUXEV
#include "../../core/linux/SDL_evdev.h"
#endif

/* KMS/DRM declarations */
#include "SDL_kmsdrm_legacy_video.h"
#include "SDL_kmsdrm_legacy_events.h"
#include "SDL_kmsdrm_legacy_opengles.h"
#include "SDL_kmsdrm_legacy_mouse.h"
#include "SDL_kmsdrm_legacy_dyn.h"
#include "SDL_kmsdrm_legacy_vulkan.h"
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>

#ifdef __OpenBSD__
#define KMSDRM_LEGACY_DRI_PATH "/dev/"
#define KMSDRM_LEGACY_DRI_DEVFMT "%sdrm%d"
#define KMSDRM_LEGACY_DRI_DEVNAME "drm"
#define KMSDRM_LEGACY_DRI_DEVNAMESIZE 3
#define KMSDRM_LEGACY_DRI_CARDPATHFMT "/dev/drm%d"
#else
#define KMSDRM_LEGACY_DRI_PATH "/dev/dri/"
#define KMSDRM_LEGACY_DRI_DEVFMT "%scard%d"
#define KMSDRM_LEGACY_DRI_DEVNAME "card"
#define KMSDRM_LEGACY_DRI_DEVNAMESIZE 4
#define KMSDRM_LEGACY_DRI_CARDPATHFMT "/dev/dri/card%d"
#endif

static int
check_modestting(int devindex)
{
    SDL_bool available = SDL_FALSE;
    char device[512];
    int drm_fd;

    SDL_snprintf(device, sizeof (device), KMSDRM_LEGACY_DRI_DEVFMT, KMSDRM_LEGACY_DRI_PATH, devindex);

    drm_fd = open(device, O_RDWR | O_CLOEXEC);
    if (drm_fd >= 0) {
        if (SDL_KMSDRM_LEGACY_LoadSymbols()) {
            drmModeRes *resources = KMSDRM_LEGACY_drmModeGetResources(drm_fd);
            if (resources) {
                SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                  KMSDRM_LEGACY_DRI_DEVFMT
                  " connector, encoder and CRTC counts are: %d %d %d",
                  KMSDRM_LEGACY_DRI_PATH, devindex,
                  resources->count_connectors, resources->count_encoders,
                  resources->count_crtcs);

                if (resources->count_connectors > 0
                 && resources->count_encoders > 0
                 && resources->count_crtcs > 0)
                {
                    available = SDL_TRUE;
                }
                KMSDRM_LEGACY_drmModeFreeResources(resources);
            }
            SDL_KMSDRM_LEGACY_UnloadSymbols();
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

    if (!(stat(KMSDRM_LEGACY_DRI_PATH, &sb) == 0
                && S_ISDIR(sb.st_mode))) {
        printf("The path %s cannot be opened or is not available\n",
               KMSDRM_LEGACY_DRI_PATH);
        return 0;
    }

    if (access(KMSDRM_LEGACY_DRI_PATH, F_OK) == -1) {
        printf("The path %s cannot be opened\n",
               KMSDRM_LEGACY_DRI_PATH);
        return 0;
    }

    folder = opendir(KMSDRM_LEGACY_DRI_PATH);
    if (folder) {
        while ((res = readdir(folder))) {
            int len = SDL_strlen(res->d_name);
            if (len > KMSDRM_LEGACY_DRI_DEVNAMESIZE && SDL_strncmp(res->d_name,
                      KMSDRM_LEGACY_DRI_DEVNAME, KMSDRM_LEGACY_DRI_DEVNAMESIZE) == 0) {
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
        if (check_modestting(i)) {
            return i;
        }
    }

    return -ENOENT;
}

static int
KMSDRM_LEGACY_Available(void)
{
    int ret = -ENOENT;

    ret = get_driindex();
    if (ret >= 0)
        return 1;

    return ret;
}

static void
KMSDRM_LEGACY_DeleteDevice(SDL_VideoDevice * device)
{
    if (device->driverdata) {
        SDL_free(device->driverdata);
        device->driverdata = NULL;
    }

    SDL_free(device);

    SDL_KMSDRM_LEGACY_UnloadSymbols();
}

static SDL_VideoDevice *
KMSDRM_LEGACY_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *viddata;

    if (!KMSDRM_LEGACY_Available()) {
        return NULL;
    }

    if (!devindex || (devindex > 99)) {
        devindex = get_driindex();
    }

    if (devindex < 0) {
        SDL_SetError("devindex (%d) must be between 0 and 99.\n", devindex);
        return NULL;
    }

    if (!SDL_KMSDRM_LEGACY_LoadSymbols()) {
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

    /* Setup all functions which we can handle */
    device->VideoInit = KMSDRM_LEGACY_VideoInit;
    device->VideoQuit = KMSDRM_LEGACY_VideoQuit;
    device->GetDisplayModes = KMSDRM_LEGACY_GetDisplayModes;
    device->SetDisplayMode = KMSDRM_LEGACY_SetDisplayMode;
    device->CreateSDLWindow = KMSDRM_LEGACY_CreateWindow;
    device->CreateSDLWindowFrom = KMSDRM_LEGACY_CreateWindowFrom;
    device->SetWindowTitle = KMSDRM_LEGACY_SetWindowTitle;
    device->SetWindowIcon = KMSDRM_LEGACY_SetWindowIcon;
    device->SetWindowPosition = KMSDRM_LEGACY_SetWindowPosition;
    device->SetWindowSize = KMSDRM_LEGACY_SetWindowSize;
    device->SetWindowFullscreen = KMSDRM_LEGACY_SetWindowFullscreen;
    device->ShowWindow = KMSDRM_LEGACY_ShowWindow;
    device->HideWindow = KMSDRM_LEGACY_HideWindow;
    device->RaiseWindow = KMSDRM_LEGACY_RaiseWindow;
    device->MaximizeWindow = KMSDRM_LEGACY_MaximizeWindow;
    device->MinimizeWindow = KMSDRM_LEGACY_MinimizeWindow;
    device->RestoreWindow = KMSDRM_LEGACY_RestoreWindow;
    device->SetWindowGrab = KMSDRM_LEGACY_SetWindowGrab;
    device->DestroyWindow = KMSDRM_LEGACY_DestroyWindow;
    device->GetWindowWMInfo = KMSDRM_LEGACY_GetWindowWMInfo;

    device->GL_LoadLibrary = KMSDRM_LEGACY_GLES_LoadLibrary;
    device->GL_GetProcAddress = KMSDRM_LEGACY_GLES_GetProcAddress;
    device->GL_UnloadLibrary = KMSDRM_LEGACY_GLES_UnloadLibrary;
    device->GL_CreateContext = KMSDRM_LEGACY_GLES_CreateContext;
    device->GL_MakeCurrent = KMSDRM_LEGACY_GLES_MakeCurrent;
    device->GL_SetSwapInterval = KMSDRM_LEGACY_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = KMSDRM_LEGACY_GLES_GetSwapInterval;
    device->GL_SwapWindow = KMSDRM_LEGACY_GLES_SwapWindow;
    device->GL_DeleteContext = KMSDRM_LEGACY_GLES_DeleteContext;

#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = KMSDRM_LEGACY_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = KMSDRM_LEGACY_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = KMSDRM_LEGACY_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = KMSDRM_LEGACY_Vulkan_CreateSurface;
    device->Vulkan_GetDrawableSize = KMSDRM_LEGACY_Vulkan_GetDrawableSize;
#endif

    device->PumpEvents = KMSDRM_LEGACY_PumpEvents;
    device->free = KMSDRM_LEGACY_DeleteDevice;

    return device;

cleanup:
    if (device)
        SDL_free(device);
    if (viddata)
        SDL_free(viddata);
    return NULL;
}

VideoBootStrap KMSDRM_LEGACY_bootstrap = {
    "KMSDRM_LEGACY",
    "KMS/DRM Video Driver",
    KMSDRM_LEGACY_CreateDevice
};

static void
KMSDRM_LEGACY_FBDestroyCallback(struct gbm_bo *bo, void *data)
{
    KMSDRM_LEGACY_FBInfo *fb_info = (KMSDRM_LEGACY_FBInfo *)data;

    if (fb_info && fb_info->drm_fd >= 0 && fb_info->fb_id != 0) {
        KMSDRM_LEGACY_drmModeRmFB(fb_info->drm_fd, fb_info->fb_id);
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Delete DRM FB %u", fb_info->fb_id);
    }

    SDL_free(fb_info);
}

KMSDRM_LEGACY_FBInfo *
KMSDRM_LEGACY_FBFromBO(_THIS, struct gbm_bo *bo)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    unsigned w,h;
    int ret;
    Uint32 stride, handle;

    /* Check for an existing framebuffer */
    KMSDRM_LEGACY_FBInfo *fb_info = (KMSDRM_LEGACY_FBInfo *)KMSDRM_LEGACY_gbm_bo_get_user_data(bo);

    if (fb_info) {
        return fb_info;
    }

    /* Create a structure that contains enough info to remove the framebuffer
       when the backing buffer is destroyed */
    fb_info = (KMSDRM_LEGACY_FBInfo *)SDL_calloc(1, sizeof(KMSDRM_LEGACY_FBInfo));

    if (!fb_info) {
        SDL_OutOfMemory();
        return NULL;
    }

    fb_info->drm_fd = viddata->drm_fd;

    /* Create framebuffer object for the buffer */
    w = KMSDRM_LEGACY_gbm_bo_get_width(bo);
    h = KMSDRM_LEGACY_gbm_bo_get_height(bo);
    stride = KMSDRM_LEGACY_gbm_bo_get_stride(bo);
    handle = KMSDRM_LEGACY_gbm_bo_get_handle(bo).u32;
    ret = KMSDRM_LEGACY_drmModeAddFB(viddata->drm_fd, w, h, 24, 32, stride, handle,
                                  &fb_info->fb_id);
    if (ret) {
      SDL_free(fb_info);
      return NULL;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "New DRM FB (%u): %ux%u, stride %u from BO %p",
                 fb_info->fb_id, w, h, stride, (void *)bo);

    /* Associate our DRM framebuffer with this buffer object */
    KMSDRM_LEGACY_gbm_bo_set_user_data(bo, fb_info, KMSDRM_LEGACY_FBDestroyCallback);

    return fb_info;
}

static void
KMSDRM_LEGACY_FlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    *((SDL_bool *) data) = SDL_FALSE;
}

SDL_bool
KMSDRM_LEGACY_WaitPageFlip(_THIS, SDL_WindowData *windata, int timeout) {
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    drmEventContext ev = {0};
    struct pollfd pfd = {0};

    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = KMSDRM_LEGACY_FlipHandler;

    pfd.fd = viddata->drm_fd;
    pfd.events = POLLIN;

    while (windata->waiting_for_flip) {
        pfd.revents = 0;

        if (poll(&pfd, 1, timeout) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "DRM poll error");
            return SDL_FALSE;
        }

        if (pfd.revents & (POLLHUP | POLLERR)) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "DRM poll hup or error");
            return SDL_FALSE;
        }

        if (pfd.revents & POLLIN) {
            /* Page flip? If so, drmHandleEvent will unset windata->waiting_for_flip */
            KMSDRM_LEGACY_drmHandleEvent(viddata->drm_fd, &ev);
        } else {
            /* Timed out and page flip didn't happen */
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Dropping frame while waiting_for_flip");
            return SDL_FALSE;
        }
    }

    return SDL_TRUE;
}

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/* _this is a SDL_VideoDevice *                                              */
/*****************************************************************************/

/* Deinitializes the dispdata members needed for KMSDRM operation that are
   inoffeensive for VK compatibility. */
void KMSDRM_LEGACY_DisplayDataDeinit (_THIS, SDL_DisplayData *dispdata) {
    /* Free connector */
    if (dispdata && dispdata->connector) {
	KMSDRM_LEGACY_drmModeFreeConnector(dispdata->connector);
	dispdata->connector = NULL;
    }

    /* Free CRTC */
    if (dispdata && dispdata->crtc) {
        KMSDRM_LEGACY_drmModeFreeCrtc(dispdata->crtc);
        dispdata->crtc = NULL;
    }
}

/* Initializes the dispdata members needed for KMSDRM operation that are
   inoffeensive for VK compatibility, except we must leave the drm_fd
   closed when we get to the end of this function.
   This is to be called early, in VideoInit(), because it gets us
   the videomode information, which SDL needs immediately after VideoInit(). */
int KMSDRM_LEGACY_DisplayDataInit (_THIS, SDL_DisplayData *dispdata) {
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    drmModeRes *resources = NULL;
    drmModeEncoder *encoder = NULL;
    drmModeConnector *connector = NULL;
    drmModeCrtc *crtc = NULL;

    int ret = 0;
    unsigned i,j;

    dispdata->modeset_pending = SDL_FALSE;
    dispdata->gbm_init = SDL_FALSE;

    dispdata->cursor_bo = NULL;

    /* Open /dev/dri/cardNN (/dev/drmN if on OpenBSD) */
    SDL_snprintf(viddata->devpath, sizeof(viddata->devpath), KMSDRM_LEGACY_DRI_CARDPATHFMT, viddata->devindex);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opening device %s", viddata->devpath);
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);

    if (viddata->drm_fd < 0) {
        ret = SDL_SetError("Could not open %s", viddata->devpath);
        goto cleanup;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opened DRM FD (%d)", viddata->drm_fd);

    /* Activate universal planes. */
    KMSDRM_LEGACY_drmSetClientCap(viddata->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    /* Get all of the available connectors / devices / crtcs */
    resources = KMSDRM_LEGACY_drmModeGetResources(viddata->drm_fd);
    if (!resources) {
        ret = SDL_SetError("drmModeGetResources(%d) failed", viddata->drm_fd);
        goto cleanup;
    }

    /* Iterate on the available connectors to find a connected connector. */
    for (i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = KMSDRM_LEGACY_drmModeGetConnector(viddata->drm_fd,
            resources->connectors[i]);

        if (!conn) {
            continue;
        }

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes) {
            connector = conn;
            break;
        }

        KMSDRM_LEGACY_drmModeFreeConnector(conn);
    }

    if (!connector) {
        ret = SDL_SetError("No currently active connector found.");
        goto cleanup;
    }

    /* Try to find the connector's current encoder */
    for (i = 0; i < resources->count_encoders; i++) {
        encoder = KMSDRM_LEGACY_drmModeGetEncoder(viddata->drm_fd, resources->encoders[i]);

        if (!encoder) {
          continue;
        }

        if (encoder->encoder_id == connector->encoder_id) {
            break;
        }

        KMSDRM_LEGACY_drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        /* No encoder was connected, find the first supported one */
        for (i = 0; i < resources->count_encoders; i++) {
            encoder = KMSDRM_LEGACY_drmModeGetEncoder(viddata->drm_fd,
                          resources->encoders[i]);

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

            KMSDRM_LEGACY_drmModeFreeEncoder(encoder);
            encoder = NULL;
        }
    }

    if (!encoder) {
        ret = SDL_SetError("No connected encoder found.");
        goto cleanup;
    }

    /* Try to find a CRTC connected to this encoder */
    crtc = KMSDRM_LEGACY_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);

    /* If no CRTC was connected to the encoder, find the first CRTC
       that is supported by the encoder, and use that. */
    if (!crtc) {
        for (i = 0; i < resources->count_crtcs; i++) {
            if (encoder->possible_crtcs & (1 << i)) {
                encoder->crtc_id = resources->crtcs[i];
                crtc = KMSDRM_LEGACY_drmModeGetCrtc(viddata->drm_fd, encoder->crtc_id);
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

    /* Store the connector and crtc for future use. These are all we keep
       from this function, and these are just structs, inoffensive to VK. */
    dispdata->connector = connector;
    dispdata->crtc = crtc;

    /***********************************/
    /* Block for Vulkan compatibility. */
    /***********************************/

    /* THIS IS FOR VULKAN! Leave the FD closed, so VK can work.
       Will reopen this in CreateWindow, but only if requested a non-VK window. */
    close (viddata->drm_fd);
    viddata->drm_fd = -1;

cleanup:
    if (encoder)
        KMSDRM_LEGACY_drmModeFreeEncoder(encoder);
    if (resources)
        KMSDRM_LEGACY_drmModeFreeResources(resources);
    if (ret) {
        /* Error (complete) cleanup */
        if (dispdata->connector) {
            KMSDRM_LEGACY_drmModeFreeConnector(dispdata->connector);
            dispdata->connector = NULL;
        }
        if (dispdata->crtc) {
            KMSDRM_LEGACY_drmModeFreeCrtc(dispdata->crtc);
            dispdata->crtc = NULL;
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
KMSDRM_LEGACY_GBMInit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    int ret = 0;

    /* Reopen the FD! */
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);

    /* Create the GBM device. */
    viddata->gbm_dev = KMSDRM_LEGACY_gbm_create_device(viddata->drm_fd);
    if (!viddata->gbm_dev) {
        ret = SDL_SetError("Couldn't create gbm device.");
    }

    dispdata->gbm_init = SDL_TRUE;

    return ret;
}

/* Deinit the Vulkan-incompatible KMSDRM stuff. */
void
KMSDRM_LEGACY_GBMDeinit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

    /* Destroy GBM device. GBM surface is destroyed by DestroySurfaces(),
       already called when we get here. */
    if (viddata->gbm_dev) {
        KMSDRM_LEGACY_gbm_device_destroy(viddata->gbm_dev);
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
KMSDRM_LEGACY_DestroySurfaces(_THIS, SDL_Window *window)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    int ret;

    /**********************************************/
    /* Wait for last issued pageflip to complete. */
    /**********************************************/
    KMSDRM_LEGACY_WaitPageFlip(_this, windata, -1);

    /***********************************************************************/
    /* Restore the original CRTC configuration: configue the crtc with the */
    /* original video mode and make it point to the original TTY buffer.   */
    /***********************************************************************/

    ret = KMSDRM_LEGACY_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc->crtc_id,
            dispdata->crtc->buffer_id, 0, 0, &dispdata->connector->connector_id, 1,
            &dispdata->original_mode);

    /* If we failed to set the original mode, try to set the connector prefered mode. */
    if (ret && (dispdata->crtc->mode_valid == 0)) {
        ret = KMSDRM_LEGACY_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc->crtc_id,
                dispdata->crtc->buffer_id, 0, 0, &dispdata->connector->connector_id, 1,
                &dispdata->original_mode);
    }

    if(ret) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not restore CRTC");
    }

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
        KMSDRM_LEGACY_gbm_surface_release_buffer(windata->gs, windata->bo);
        windata->bo = NULL;
    }

    if (windata->next_bo) {
        KMSDRM_LEGACY_gbm_surface_release_buffer(windata->gs, windata->next_bo);
        windata->next_bo = NULL;
    }

    /***************************/
    /* Destroy the GBM surface */
    /***************************/

    if (windata->gs) {
        KMSDRM_LEGACY_gbm_surface_destroy(windata->gs);
        windata->gs = NULL;
    }
}

int
KMSDRM_LEGACY_CreateSurfaces(_THIS, SDL_Window * window)
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
        KMSDRM_LEGACY_DestroySurfaces(_this, window);
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

    if (!KMSDRM_LEGACY_gbm_device_is_format_supported(viddata->gbm_dev,
             surface_fmt, surface_flags)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
        "GBM surface format not supported. Trying anyway.");
    }

    windata->gs = KMSDRM_LEGACY_gbm_surface_create(viddata->gbm_dev,
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
            KMSDRM_LEGACY_gbm_surface_destroy(windata->gs);
            windata->gs = NULL;
        }
    }

    return ret;
}

int
KMSDRM_LEGACY_VideoInit(_THIS)
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

    /* Get KMSDRM resources info and store what we need. Getting and storing
       this info isn't a problem for VK compatibility.
       For VK-incompatible initializations we have KMSDRM_LEGACY_GBMInit(), which is
       called on window creation, and only when we know it's not a VK window. */
    if (KMSDRM_LEGACY_DisplayDataInit(_this, dispdata)) {
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

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Init();
#endif

    /* Since we create and show the default cursor on KMSDRM_InitMouse() and
       we call KMSDRM_InitMouse() everytime we create a new window, we have
       to be sure to create and show the default cursor only the first time.
       If we don't, new default cursors would stack up on mouse->cursors and SDL
       would have to hide and delete them at quit, not to mention the memory leak... */
    dispdata->set_default_cursor_pending = SDL_TRUE;

    viddata->video_init = SDL_TRUE;

cleanup:

    if (ret) {
        /* Error (complete) cleanup */
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
KMSDRM_LEGACY_VideoQuit(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    KMSDRM_LEGACY_DisplayDataDeinit(_this, dispdata);

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

void
KMSDRM_LEGACY_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DisplayData *dispdata = display->driverdata;
    drmModeConnector *conn = dispdata->connector;
    SDL_DisplayMode mode;
    int i;

    for (i = 0; i < conn->count_modes; i++) {
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
KMSDRM_LEGACY_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)display->driverdata;
    SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;
    drmModeConnector *conn = dispdata->connector;
    int i;

    if (!modedata) {
        return SDL_SetError("Mode doesn't have an associated index");
    }

    dispdata->mode = conn->modes[modedata->mode_index];

    for (i = 0; i < viddata->num_windows; i++) {
        SDL_Window *window = viddata->windows[i];
        SDL_WindowData *windata = (SDL_WindowData *)window->driverdata;

        /* Can't recreate EGL surfaces right now, need to wait until SwapWindow
           so the correct thread-local surface and context state are available */
        windata->egl_surface_dirty = 1;

        /* Tell app about the resize */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, mode->w, mode->h);
    }

    return 0;
}

void
KMSDRM_LEGACY_DestroyWindow(_THIS, SDL_Window *window)
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

        /* Destroy cursor GBM plane. */
        KMSDRM_LEGACY_DeinitMouse(_this);

        /* Destroy GBM surface and buffers. */
        KMSDRM_LEGACY_DestroySurfaces(_this, window);

        /* Unload EGL library. */
        if (_this->egl_data) {
            SDL_EGL_UnloadLibrary(_this);
        }

        /* Unload GL library. */
        if (_this->gl_config.driver_loaded) {
            SDL_GL_UnloadLibrary();
        }

        /* Free display plane, and destroy GBM device. */
        KMSDRM_LEGACY_GBMDeinit(_this, dispdata);
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

int
KMSDRM_LEGACY_CreateWindow(_THIS, SDL_Window * window)
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

         /* We need that the fb that SDL gives us has the same size as the videomode
            currently configure on the CRTC, because the LEGACY interface doesn't
            support scaling on the primary plane on most hardware (and overlay
            planes are not present in all hw), so the CRTC reads the PRIMARY PLANE
            without any scaling, and that's all.
            So AR-correctin is also impossible on the LEGACY interface. */
         window->w = dispdata->mode.hdisplay;
         window->h = dispdata->mode.vdisplay;

         /* Reopen FD, create gbm dev, setup display plane, etc,.
            but only when we come here for the first time,
            and only if it's not a VK window. */
         if ((ret = KMSDRM_LEGACY_GBMInit(_this, dispdata))) {
                 goto cleanup;
         }

         /* Manually load the GL library. KMSDRM_EGL_LoadLibrary() has already
            been called by SDL_CreateWindow() but we don't do anything there,
            precisely to be able to load it here.
            If we let SDL_CreateWindow() load the lib, it will be loaded
            before we call KMSDRM_GBMInit(), causing GLES programs to fail. */
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
         KMSDRM_LEGACY_InitMouse(_this);

         /* Since we take cursor buffer way from the cursor plane and
            destroy the cursor GBM BO when we destroy a window, we must
            also manually re-show the cursor on screen, if necessary,
            when we create a window. */
         KMSDRM_LEGACY_InitCursor();
    }

    /* Allocate window internal data */
    windata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!windata) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    if (((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
        || ((window->flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN))
    {
        windata->src_w = dispdata->mode.hdisplay;
        windata->src_h = dispdata->mode.vdisplay;
        windata->output_w = dispdata->mode.hdisplay;
        windata->output_h = dispdata->mode.vdisplay;
        windata->output_x = 0;
    } else {
        /* Normal non-fullscreen windows are scaled to the in-use video mode
           using a PLANE connected to the CRTC, so get input size,
           output (CRTC) size, and position. */
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
        if ((ret = KMSDRM_LEGACY_CreateSurfaces(_this, window))) {
            goto cleanup;
        }

        /***************************************************************************/
        /* This is fundamental.                                                    */
        /* We can't display an fb smaller than the resolution currently configured */
        /* on the CRTC, because the CRTC would be scanning out of bounds, and      */
        /* drmModeSetCrtc() would fail.                                            */
        /* A possible solution would be scaling on the primary plane with          */
        /* drmModeSetPlane(), but primary plane scaling is not supported in most   */
        /* LEGACY-only hardware, so never use drmModeSetPlane().                   */
        /***************************************************************************/

        ret = KMSDRM_LEGACY_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc->crtc_id,
            /*fb_info->fb_id*/ -1, 0, 0, &dispdata->connector->connector_id, 1,
            &dispdata->mode);

	if (ret) {
	    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not set CRTC");
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
        /* Allocated windata will be freed in KMSDRM_DestroyWindow */
        KMSDRM_LEGACY_DestroyWindow(_this, window);
    }
    return ret;
}

/*****************************************************************************/
/* Reconfigure the window scaling parameters and re-construct it's surfaces, */
/* without destroying the window itself.                                     */
/* To be used by SetWindowSize() and SetWindowFullscreen().                  */
/*****************************************************************************/
static int
KMSDRM_LEGACY_ReconfigureWindow( _THIS, SDL_Window * window) {
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
        if (KMSDRM_LEGACY_CreateSurfaces(_this, window)) {
            return -1;
        }
    }
    return 0;
}

int
KMSDRM_LEGACY_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void
KMSDRM_LEGACY_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
KMSDRM_LEGACY_SetWindowPosition(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_SetWindowSize(_THIS, SDL_Window * window)
{
    if (KMSDRM_LEGACY_ReconfigureWindow(_this, window)) {
        SDL_SetError("Can't reconfigure window on SetWindowSize.");
    }
}
void
KMSDRM_LEGACY_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen)
{
    if (KMSDRM_LEGACY_ReconfigureWindow(_this, window)) {
        SDL_SetError("Can't reconfigure window on SetWindowFullscreen.");
    }
}
void
KMSDRM_LEGACY_ShowWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_HideWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
KMSDRM_LEGACY_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{

}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
KMSDRM_LEGACY_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
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
