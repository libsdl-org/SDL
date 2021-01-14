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
#elif defined SDL_INPUT_WSCONS
#include "../../core/openbsd/SDL_wscons.h"
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
#include <poll.h>
#include <errno.h>

#ifdef __OpenBSD__
#define KMSDRM_DRI_PATH "/dev/"
#define KMSDRM_DRI_DEVFMT "%sdrm%d"
#define KMSDRM_DRI_DEVNAME "drm"
#define KMSDRM_DRI_DEVNAMESIZE 3
#define KMSDRM_DRI_CARDPATHFMT "/dev/drm%d"
#else
#define KMSDRM_DRI_PATH "/dev/dri/"
#define KMSDRM_DRI_DEVFMT "%scard%d"
#define KMSDRM_DRI_DEVNAME "card"
#define KMSDRM_DRI_DEVNAMESIZE 4
#define KMSDRM_DRI_CARDPATHFMT "/dev/dri/card%d"
#endif

static int
check_modestting(int devindex)
{
    SDL_bool available = SDL_FALSE;
    char device[512];
    int drm_fd;

    SDL_snprintf(device, sizeof (device), KMSDRM_DRI_DEVFMT, KMSDRM_DRI_PATH, devindex);

    drm_fd = open(device, O_RDWR | O_CLOEXEC);
    if (drm_fd >= 0) {
        if (SDL_KMSDRM_LoadSymbols()) {
            drmModeRes *resources = KMSDRM_drmModeGetResources(drm_fd);
            if (resources) {
                SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
                  KMSDRM_DRI_DEVFMT
                  " connector, encoder and CRTC counts are: %d %d %d",
                  KMSDRM_DRI_PATH, devindex,
                  resources->count_connectors, resources->count_encoders,
                  resources->count_crtcs);

                if (resources->count_connectors > 0
                 && resources->count_encoders > 0
                 && resources->count_crtcs > 0)
                {
                    available = SDL_TRUE;
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
            if (len > KMSDRM_DRI_DEVNAMESIZE && SDL_strncmp(res->d_name,
                      KMSDRM_DRI_DEVNAME, KMSDRM_DRI_DEVNAMESIZE) == 0) {
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

    /* Setup all functions which we can handle */
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

    device->GL_LoadLibrary = KMSDRM_GLES_LoadLibrary;
    device->GL_GetProcAddress = KMSDRM_GLES_GetProcAddress;
    device->GL_UnloadLibrary = KMSDRM_GLES_UnloadLibrary;
    device->GL_CreateContext = KMSDRM_GLES_CreateContext;
    device->GL_MakeCurrent = KMSDRM_GLES_MakeCurrent;
    device->GL_SetSwapInterval = KMSDRM_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = KMSDRM_GLES_GetSwapInterval;
    device->GL_SwapWindow = KMSDRM_GLES_SwapWindow;
    device->GL_DeleteContext = KMSDRM_GLES_DeleteContext;

#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = KMSDRM_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = KMSDRM_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = KMSDRM_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = KMSDRM_Vulkan_CreateSurface;
    device->Vulkan_GetDrawableSize = KMSDRM_Vulkan_GetDrawableSize;
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

static void
KMSDRM_FlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    *((SDL_bool *) data) = SDL_FALSE;
}

SDL_bool
KMSDRM_WaitPageFlip(_THIS, SDL_WindowData *windata) {

    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    drmEventContext ev = {0};
    struct pollfd pfd = {0};

    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = KMSDRM_FlipHandler;

    pfd.fd = viddata->drm_fd;
    pfd.events = POLLIN;

    /* Stay on the while loop until we get the desired event.
       We need the while the loop because we could be in a situation where:
       -We get events on the FD in time, thus not on exiting on return number 1.
       -These events are not errors, thus not exiting on return number 2.
       -These events are of POLLIN type, thus not exiting on return number 3,
        but if the event is not the pageflip we are waiting for, we arrive at the end
        of the loop and do loop re-entry, hoping the next event will be the pageflip.

        If it wasn't for the while loop, we could erroneously exit the function
        without the pageflip event to arrive!

      For example, vblank events hit the FD and they are POLLIN events too (POLLIN
      means "there's data to read on the FD"), but they are not the pageflip event
      we are waiting for, so the drmEventHandle() doesn't run the flip handler, and
      since waiting_for_flip is set on the pageflip handle, it's not set and we stay
      on the loop.
    */
    while (windata->waiting_for_flip) {

        pfd.revents = 0;

        /* poll() waits for events arriving on the FD, and returns < 0 if timeout
           passes with no events.
           We wait forever (timeout = -1), but even if we DO get an event,
           we have yet to see if it's of the required type. */
        if (poll(&pfd, 1, -1) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "DRM poll error");
            return SDL_FALSE; /* Return number 1. */
        }

        if (pfd.revents & (POLLHUP | POLLERR)) {
            /* An event arrived on the FD in time, but it's an error. */
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "DRM poll hup or error");
            return SDL_FALSE; /* Return number 2. */
        }

        if (pfd.revents & POLLIN) {
            /* There is data to read on the FD!
               Is the event a pageflip? We know it is a pageflip if it matches the
               event we are passing in &ev. If it does, drmHandleEvent() will unset
               windata->waiting_for_flip and we will get out of the "while" loop.
               If it's not, we keep iterating on the loop. */
            KMSDRM_drmHandleEvent(viddata->drm_fd, &ev);
        } else {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Dropping frame while waiting_for_flip");
            return SDL_FALSE; /* Return number 3. */
        }
    }

    return SDL_TRUE;
}

/* Given w and h, returns a DRM video mode from the modes available on the connector.
   Returns NULL if no mode matching the specified size is found. */
drmModeModeInfo*
KMSDRM_GetConnectorMode(drmModeConnector *connector, uint32_t width, uint32_t height) {
    int i = 0;
    for (i = 0; i < connector->count_modes; i++) {
	if (connector->modes[i].hdisplay == width &&
            connector->modes[i].vdisplay == height)
        {
	    return &connector->modes[i];
	}
    }
    return NULL;
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
	KMSDRM_drmModeFreeConnector(dispdata->connector);
	dispdata->connector = NULL;
    }

    /* Free CRTC */
    if (dispdata && dispdata->crtc) {
        KMSDRM_drmModeFreeCrtc(dispdata->crtc);
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

    int ret = 0;
    unsigned i,j;

    dispdata->gbm_init = SDL_FALSE;
    dispdata->modeset_pending = SDL_FALSE;
    dispdata->cursor_bo = NULL;

    /* Open /dev/dri/cardNN (/dev/drmN if on OpenBSD) */
    SDL_snprintf(viddata->devpath, sizeof(viddata->devpath), KMSDRM_DRI_CARDPATHFMT, viddata->devindex);

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opening device %s", viddata->devpath);
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);

    if (viddata->drm_fd < 0) {
        ret = SDL_SetError("Could not open %s", viddata->devpath);
        goto cleanup;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Opened DRM FD (%d)", viddata->drm_fd);

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
            break;
        }

        KMSDRM_drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        /* No encoder was connected, find the first supported one */
        for (i = 0; i < resources->count_encoders; i++) {
            encoder = KMSDRM_drmModeGetEncoder(viddata->drm_fd,
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

            KMSDRM_drmModeFreeEncoder(encoder);
            encoder = NULL;
        }
    }

    if (!encoder) {
        ret = SDL_SetError("No connected encoder found.");
        goto cleanup;
    }

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
        KMSDRM_drmModeFreeEncoder(encoder);
    if (resources)
        KMSDRM_drmModeFreeResources(resources);
    if (ret) {
        /* Error (complete) cleanup */
        if (dispdata->connector) {
            KMSDRM_drmModeFreeConnector(dispdata->connector);
            dispdata->connector = NULL;
        }
        if (dispdata->crtc) {
            KMSDRM_drmModeFreeCrtc(dispdata->crtc);
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
KMSDRM_GBMInit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    int ret = 0;

    /* Reopen the FD! */
    viddata->drm_fd = open(viddata->devpath, O_RDWR | O_CLOEXEC);

    /* Create the GBM device. */
    viddata->gbm_dev = KMSDRM_gbm_create_device(viddata->drm_fd);
    if (!viddata->gbm_dev) {
        ret = SDL_SetError("Couldn't create gbm device.");
    }

    dispdata->gbm_init = SDL_TRUE;

    return ret;
}

/* Deinit the Vulkan-incompatible KMSDRM stuff. */
void
KMSDRM_GBMDeinit (_THIS, SDL_DisplayData *dispdata)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);

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

void
KMSDRM_DestroySurfaces(_THIS, SDL_Window *window)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    int ret;

    /**********************************************/
    /* Wait for last issued pageflip to complete. */
    /**********************************************/
    KMSDRM_WaitPageFlip(_this, windata);

    /***********************************************************************/
    /* Restore the original CRTC configuration: configue the crtc with the */
    /* original video mode and make it point to the original TTY buffer.   */
    /***********************************************************************/

    ret = KMSDRM_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc->crtc_id,
            dispdata->crtc->buffer_id, 0, 0, &dispdata->connector->connector_id, 1,
            &dispdata->original_mode);

    /* If we failed to set the original mode, try to set the connector prefered mode. */
    if (ret && (dispdata->crtc->mode_valid == 0)) {
        ret = KMSDRM_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc->crtc_id,
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
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
        windata->bo = NULL;
    }

    if (windata->next_bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->next_bo);
        windata->next_bo = NULL;
    }

    /***************************/
    /* Destroy the GBM surface */
    /***************************/

    if (windata->gs) {
        KMSDRM_gbm_surface_destroy(windata->gs);
        windata->gs = NULL;
    }
}

/* This determines the size of the fb, which comes from the GBM surface
   that we create here. */
int
KMSDRM_CreateSurfaces(_THIS, SDL_Window * window)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_WindowData *windata = (SDL_WindowData *)window->driverdata;

    uint32_t surface_fmt = GBM_FORMAT_ARGB8888;
    uint32_t surface_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;

    EGLContext egl_context;

    int ret = 0;

    /* If the current window already has surfaces, destroy them before creating other.
       This is mainly for ReconfigureWindow(), where we simply call CreateSurfaces()
       for regenerating a window's surfaces. */
    if (windata->gs) {
        KMSDRM_DestroySurfaces(_this, window);
    }

    if (!KMSDRM_gbm_device_is_format_supported(viddata->gbm_dev,
             surface_fmt, surface_flags)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
        "GBM surface format not supported. Trying anyway.");
    }

    windata->gs = KMSDRM_gbm_surface_create(viddata->gbm_dev,
                      windata->surface_w, windata->surface_h, surface_fmt, surface_flags);

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
            KMSDRM_gbm_surface_destroy(windata->gs);
            windata->gs = NULL;
        }
    }

    return ret;
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

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Init();
#elif defined(SDL_INPUT_WSCONS)
    SDL_WSCONS_Init();
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
KMSDRM_VideoQuit(_THIS)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    KMSDRM_DisplayDataDeinit(_this, dispdata);

#ifdef SDL_INPUT_LINUXEV
    SDL_EVDEV_Quit();
#elif defined(SDL_INPUT_WSCONS)
    SDL_WSCONS_Quit();
#endif

    /* Clear out the window list */
    SDL_free(viddata->windows);
    viddata->windows = NULL;
    viddata->max_windows = 0;
    viddata->num_windows = 0;
    viddata->video_init = SDL_FALSE;
}

/* Read modes from the connector modes, and store them in display->display_modes. */
void
KMSDRM_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
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
KMSDRM_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    /* Set the dispdata->mode to the new mode and leave actual modesetting
       pending to be done on SwapWindow() via drmModeSetCrtc() */

    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_DisplayData *dispdata = (SDL_DisplayData *)display->driverdata;
    SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;
    drmModeConnector *conn = dispdata->connector;
    int i;

    /* Don't do anything if we are in Vulkan mode. */
    if (viddata->vulkan_mode) {
        return 0;
    }

    if (!modedata) {
        return SDL_SetError("Mode doesn't have an associated index");
    }

    /* Take note of the new mode to be set, and leave the CRTC modeset pending
       so it's done in SwapWindow. */
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

    if ( !is_vulkan && dispdata->gbm_init ) {

        /* Destroy cursor GBM plane. */
        KMSDRM_DeinitMouse(_this);

        /* Destroy GBM surface and buffers. */
        KMSDRM_DestroySurfaces(_this, window);

        /* Unload EGL library. */
        if (_this->egl_data) {
            SDL_EGL_UnloadLibrary(_this);
        }

        /* Unload GL library. */
        if (_this->gl_config.driver_loaded) {
            SDL_GL_UnloadLibrary();
        }

        /* Free display plane, and destroy GBM device. */
        KMSDRM_GBMDeinit(_this, dispdata);
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

/**********************************************************************/
/* We simply IGNORE if it's a fullscreen window, window->flags don't  */
/* reflect it: if it's fullscreen, KMSDRM_SetWindwoFullscreen() which */
/* will be called by SDL later, and we can manage it there.           */
/**********************************************************************/ 
int
KMSDRM_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = NULL;
    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *dispdata = display->driverdata;
    SDL_bool is_vulkan = window->flags & SDL_WINDOW_VULKAN; /* Is this a VK window? */
    SDL_bool vulkan_mode = viddata->vulkan_mode; /* Do we have any Vulkan windows? */
    NativeDisplayType egl_display;
    int ret = 0;
    drmModeModeInfo *mode;

    /* Allocate window internal data */
    windata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!windata) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    /* Setup driver data for this window */
    windata->viddata = viddata;
    window->driverdata = windata;

    if (!is_vulkan && !vulkan_mode) { /* NON-Vulkan block. */

	if (!(dispdata->gbm_init)) {

            /* In order for the GL_CreateRenderer() and GL_LoadFunctions() calls
               in SDL_CreateWindow succeed (no doing so causes a windo re-creation),
               At the end of this block, we must have: 
               -Marked the window as being OPENGL
               -Loaded the GL library (which can't be loaded until the GBM
	        device has been created) because SDL_EGL_Library() function uses it.
             */

	    /* Maybe you didn't ask for an OPENGL window, but that's what you will get.
	       See previous comment on why. */
	    window->flags |= SDL_WINDOW_OPENGL;

	    /* Reopen FD, create gbm dev, setup display plane, etc,.
		but only when we come here for the first time,
		and only if it's not a VK window. */
	    if ((ret = KMSDRM_GBMInit(_this, dispdata))) {
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
	    KMSDRM_InitMouse(_this);

	    /* Since we take cursor buffer way from the cursor plane and
	       destroy the cursor GBM BO when we destroy a window, we must
	       also manually re-show the cursor on screen, if necessary,
	       when we create a window. */
	    KMSDRM_InitCursor();
	}

        /* Try to find a matching video mode for the window, fallback to the
           original mode if not available, and configure the mode we chose
           into the CRTC.
           You may be tempted to not do a videomode change, remaining always on
           the original resolution, and use the SendWindowEvent() parameters to
           make SDL2 pre-scale the image for us to an AR-corrected size inside
           the original mode, but DON'T: vectorized games (GL games) are rendered
           with the size specified in SendWindowEvent(),instead of being rendered
           at the original size and then scaled.
           It makes sense because GL is used to render the scene in GL games,
           so the scene is rendered at the window size. */
	mode = KMSDRM_GetConnectorMode(dispdata->connector, window->w, window->h );

        if (mode) {
	    windata->surface_w = window->w;
	    windata->surface_h = window->h;
	    dispdata->mode = *mode;
	}
	else {
	    windata->surface_w = dispdata->original_mode.hdisplay;
	    windata->surface_h = dispdata->original_mode.vdisplay;
	    dispdata->mode = dispdata->original_mode;
	}

        /* Take note to do the modesettng on the CRTC in SwapWindow. */
	dispdata->modeset_pending = SDL_TRUE;

        /* Create the window surfaces with the size we have just chosen.
           Needs the window diverdata in place. */
        if ((ret = KMSDRM_CreateSurfaces(_this, window))) {
            goto cleanup;
        }

        /* Tell app about the size we have determined for the window,
           so SDL pre-scales to that size for us. */
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED,
        windata->surface_w, windata->surface_h);
    } /* NON-Vulkan block ends. */

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
        KMSDRM_DestroyWindow(_this, window);
    }
    return ret;
}

/*****************************************************************************/
/* Re-create a window surfaces without destroying the window itself,         */ 
/* and set a videomode on the CRTC that matches the surfaces size.           */
/* To be used by SetWindowSize() and SetWindowFullscreen().                  */
/*****************************************************************************/
void
KMSDRM_ReconfigureWindow( _THIS, SDL_Window * window) {
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;

    if ((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ==
			 SDL_WINDOW_FULLSCREEN_DESKTOP)
    {
	/* Update the current mode to the desktop mode,
	   take note of pending mode configuration to the CRTC,
	   and recreate the GBM surface with the same size as the size. */
	windata->surface_w = dispdata->original_mode.hdisplay;
	windata->surface_h = dispdata->original_mode.vdisplay;
	dispdata->mode = dispdata->original_mode;
    }
    else
    {
	/* Try to find a valid video mode matching the size of the window. */
	drmModeModeInfo *mode = KMSDRM_GetConnectorMode(dispdata->connector,
	  window->windowed.w, window->windowed.h );

	if (mode) {
            /* If matching mode found, recreate the GBM surface with the size
               of that mode and configure it on the CRTC. */
	    windata->surface_w = window->windowed.w;
	    windata->surface_h = window->windowed.h;
	    dispdata->mode = *mode;
	}
	else {
            /* If not matching mode found, recreate the GBM surfaces with the
               size of the mode that was originally configured on the CRTC,
               and setup that mode on the CRTC. */
	    windata->surface_w = dispdata->original_mode.hdisplay;
	    windata->surface_h = dispdata->original_mode.vdisplay;
	    dispdata->mode = dispdata->original_mode;
        }
    }

    /* Recreate the GBM (and EGL) surfaces, and mark the CRTC mode/fb setting
       as pending so it's done on SwaWindon.  */
    KMSDRM_CreateSurfaces(_this, window);
    dispdata->modeset_pending = SDL_TRUE;

    /* Tell app about the size we have determined for the window,
       so SDL pre-scales to that size for us. */
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED,
                        windata->surface_w, windata->surface_h);
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
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    if (!viddata->vulkan_mode) {
        KMSDRM_ReconfigureWindow(_this, window);
    }
}
void
KMSDRM_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    if (!viddata->vulkan_mode) {
        KMSDRM_ReconfigureWindow(_this, window);
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
