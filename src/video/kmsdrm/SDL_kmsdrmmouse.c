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

#include "SDL_kmsdrmvideo.h"
#include "SDL_kmsdrmmouse.h"
#include "SDL_kmsdrmdyn.h"
#include "SDL_assert.h"

#include "../../events/SDL_mouse_c.h"
#include "../../events/default_cursor.h"

static SDL_Cursor *KMSDRM_CreateDefaultCursor(void);
static SDL_Cursor *KMSDRM_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y);
static int KMSDRM_ShowCursor(SDL_Cursor * cursor);
static void KMSDRM_MoveCursor(SDL_Cursor * cursor);
static void KMSDRM_FreeCursor(SDL_Cursor * cursor);
static void KMSDRM_WarpMouse(SDL_Window * window, int x, int y);
static int KMSDRM_WarpMouseGlobal(int x, int y);

/**********************************/
/* Atomic helper functions block. */
/**********************************/

int
drm_atomic_movecursor(KMSDRM_CursorData *curdata, uint16_t x, uint16_t y)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    if (!dispdata->cursor_plane) /* We can't move a non-existing cursor, but that's ok. */
        return 0;

    /* Do we have a set of changes already in the making? If not, allocate a new one. */
    if (!dispdata->atomic_req)
        dispdata->atomic_req = KMSDRM_drmModeAtomicAlloc();
    
    add_plane_property(dispdata->atomic_req, dispdata->cursor_plane, "CRTC_X", x - curdata->hot_x);
    add_plane_property(dispdata->atomic_req, dispdata->cursor_plane, "CRTC_Y", y - curdata->hot_y);

    return 0;
}

/***************************************/
/* Atomic helper functions block ends. */
/***************************************/

/* Converts a pixel from straight-alpha [AA, RR, GG, BB], which the SDL cursor surface has,
   to premultiplied-alpha [AA. AA*RR, AA*GG, AA*BB].
   These multiplications have to be done with floats instead of uint32_t's, and the resulting values have 
   to be converted to be relative to the 0-255 interval, where 255 is 1.00 and anything between 0 and 255 is 0.xx. */
void alpha_premultiply_ARGB8888 (uint32_t *pixel) {

    uint32_t A, R, G, B;

    /* Component bytes extraction. */
    A = (*pixel >> (3 << 3)) & 0xFF;
    R = (*pixel >> (2 << 3)) & 0xFF;
    G = (*pixel >> (1 << 3)) & 0xFF;
    B = (*pixel >> (0 << 3)) & 0xFF;

    /* Alpha pre-multiplication of each component. */
    R = (float)A * ((float)R /255);
    G = (float)A * ((float)G /255);
    B = (float)A * ((float)B /255);

    /* ARGB8888 pixel recomposition. */
    (*pixel) = (((uint32_t)A << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8)) | ((uint32_t)B << 0);
}

static SDL_Cursor *
KMSDRM_CreateDefaultCursor(void)
{
    return SDL_CreateCursor(default_cdata, default_cmask, DEFAULT_CWIDTH, DEFAULT_CHEIGHT, DEFAULT_CHOTX, DEFAULT_CHOTY);
}

/* Create a GBM cursor from a surface, which means creating a hardware cursor.
   Most programs use software cursors, but protracker-clone for example uses
   an optional hardware cursor. */
static SDL_Cursor *
KMSDRM_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    SDL_VideoDevice *dev = SDL_GetVideoDevice();
    SDL_VideoData *viddata = ((SDL_VideoData *)dev->driverdata);
    KMSDRM_CursorData *curdata;
    SDL_Cursor *cursor;
    uint64_t usable_cursor_w, usable_cursor_h;
    uint32_t bo_stride, pixel;
    uint32_t *buffer = NULL;
    size_t bufsize;

    /* All code below assumes ARGB8888 format for the cursor surface, like other backends do.
       Also, the GBM BO pixels have to be alpha-premultiplied, but the SDL surface we receive has
       straight-alpha pixels, so we always have to convert. */ 
    SDL_assert(surface->format->format == SDL_PIXELFORMAT_ARGB8888);
    SDL_assert(surface->pitch == surface->w * 4);

    if (!KMSDRM_gbm_device_is_format_supported(viddata->gbm_dev, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE)) {
        SDL_SetError("Unsupported pixel format for cursor");
        return NULL;
    }

    cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        SDL_OutOfMemory();
        return NULL;
    }
    curdata = (KMSDRM_CursorData *) SDL_calloc(1, sizeof(*curdata));
    if (!curdata) {
        SDL_OutOfMemory();
        SDL_free(cursor);
        return NULL;
    }

    /* Find out what GBM cursor size is recommended by the driver. */
    if (KMSDRM_drmGetCap(viddata->drm_fd, DRM_CAP_CURSOR_WIDTH,  &usable_cursor_w) ||
        KMSDRM_drmGetCap(viddata->drm_fd, DRM_CAP_CURSOR_HEIGHT, &usable_cursor_h)) {
            SDL_SetError("Could not get the recommended GBM cursor size");
            goto cleanup;
    }

    if (usable_cursor_w == 0 || usable_cursor_w == 0) {
            SDL_SetError("Could not get an usable GBM cursor size");
            goto cleanup;
    }

    /* hox_x and hot_y are the coordinates of the "tip of the cursor" from it's base. */
    curdata->hot_x = hot_x;
    curdata->hot_y = hot_y;
    curdata->w = usable_cursor_w;
    curdata->h = usable_cursor_h;

    curdata->bo = KMSDRM_gbm_bo_create(viddata->gbm_dev, usable_cursor_w, usable_cursor_h, GBM_FORMAT_ARGB8888,
                                       GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

    if (!curdata->bo) {
        SDL_SetError("Could not create GBM cursor BO");
        goto cleanup;
    }

    bo_stride = KMSDRM_gbm_bo_get_stride(curdata->bo);
    bufsize = bo_stride * curdata->h;

    /* Always use a temp buffer: it serves the purpose of storing the
       alpha-premultiplied pixels (so we can copy them to the gbm BO
       with a single gb_bo_write() call), and also copying from the
       SDL surface, line by line, to a gbm BO with different pitch. */
    buffer = (uint32_t*)SDL_malloc(bufsize);
    if (!buffer) {
	SDL_OutOfMemory();
	goto cleanup;
    }

    if (SDL_MUSTLOCK(surface)) {
	if (SDL_LockSurface(surface) < 0) {
	    /* Could not lock surface */
	    goto cleanup;
	}
    }

    /* Clean the whole temporary buffer. */
    SDL_memset(buffer, 0x00, bo_stride * curdata->h);

    /* Copy from SDL surface to buffer, pre-multiplying by alpha each pixel as we go. */
    for (int i = 0; i < surface->h; i++) {
        for (int j = 0; j < surface->w; j++) {
            pixel = ((uint32_t*)surface->pixels)[i * surface->w + j];
            alpha_premultiply_ARGB8888 (&pixel);
	    SDL_memcpy(buffer + (i * curdata->w)  + j, &pixel, 4);
        }
    }

    if (SDL_MUSTLOCK(surface)) {
	SDL_UnlockSurface(surface);
    }

    if (KMSDRM_gbm_bo_write(curdata->bo, buffer, bufsize)) {
	SDL_SetError("Could not write to GBM cursor BO");
	goto cleanup;
    }

    /* Free temporary buffer */
    SDL_free(buffer);
    buffer = NULL;

    cursor->driverdata = curdata;

    return cursor;

cleanup:
    if (buffer) {
        SDL_free(buffer);
    }
    if (cursor) {
        SDL_free(cursor);
    }
    if (curdata) {
        if (curdata->bo) {
            KMSDRM_gbm_bo_destroy(curdata->bo);
        }
        SDL_free(curdata);
    }
    return NULL;
}

/* Show the specified cursor, or hide if cursor is NULL.
   cur_cursor is the current cursor, and cursor is the new cursor.
   A cursor is displayed on a display, so we have to add a pointer to dispdata
   to the driverdata 
 */
static int
KMSDRM_ShowCursor(SDL_Cursor * cursor)
{
    SDL_VideoDevice *video_device = SDL_GetVideoDevice();
    //SDL_VideoData *viddata = ((SDL_VideoData *)dev->driverdata);
    SDL_Mouse *mouse;
    KMSDRM_CursorData *curdata;
    SDL_VideoDisplay *display = NULL;
    SDL_DisplayData *dispdata = NULL;
    KMSDRM_FBInfo *fb;
    KMSDRM_PlaneInfo info = {0};
    int ret;

    mouse = SDL_GetMouse();
    if (!mouse) {
        return SDL_SetError("No mouse.");
    }

    if (mouse->focus) {
        display = SDL_GetDisplayForWindow(mouse->focus);
        if (display) {
            dispdata = (SDL_DisplayData*) display->driverdata;
        }
    }

    /**********************************/
    /* if cursor == NULL, HIDE cursor */
    /**********************************/
    if (!cursor) {
        /* Hide CURRENT cursor, a cursor that is already on screen
           and SDL stores in mouse->cur_cursor. */
        if (mouse->cur_cursor && mouse->cur_cursor->driverdata) {
            if (dispdata && dispdata->cursor_plane) {
                info.plane = dispdata->cursor_plane; /* The rest of the members are zeroed. */
                ret = drm_atomic_set_plane_props(&info); 
                //drm_atomic_commit(display->device, SDL_TRUE);
                if (ret) {
                    SDL_SetError("Could not hide current cursor.");
                    return ret;
                }

                return 0;

            }
        }

        return SDL_SetError("Couldn't find cursor to hide.");
    }

    /************************************************/
    /* If cursor != NULL, DO show cursor on display */
    /************************************************/

    if (!display) {
        return SDL_SetError("Could not get display for mouse.");
    }
    if (!dispdata) {
        return SDL_SetError("Could not get display driverdata.");
    }
    if (!dispdata->cursor_plane) {
        return SDL_SetError("Hardware cursor plane not initialized.");
    }
    
    curdata = (KMSDRM_CursorData *) cursor->driverdata;

    if (!curdata || !curdata->bo) {
        return SDL_SetError("Cursor not initialized properly.");
    }

    curdata->crtc_id  = dispdata->crtc->crtc->crtc_id;
    curdata->plane    = dispdata->cursor_plane;
    curdata->video    = video_device;

    fb = KMSDRM_FBFromBO(curdata->video, curdata->bo);

    info.plane = dispdata->cursor_plane;
    info.crtc_id = curdata->crtc_id;
    info.fb_id = fb->fb_id; 
    info.src_w = curdata->w;
    info.src_h = curdata->h;
    info.crtc_x = mouse->x - curdata->hot_x;
    info.crtc_y = mouse->y - curdata->hot_y;
    info.crtc_w = curdata->w;
    info.crtc_h = curdata->h; 

    ret = drm_atomic_set_plane_props(&info);
    //drm_atomic_commit(display->device, SDL_TRUE);

    if (ret) {
        SDL_SetError("KMSDRM_ShowCursor failed.");
        return ret;
    }

    return 0;
}

/* Unset the cursor from the cursor plane, and ONLY WHEN THAT'S DONE,
   DONE FOR REAL, and not only requested, destroy it by destroying the curso BO.
   Destroying the cursor BO is an special an delicate situation,
   because drm_atomic_set_plane_props() returns immediately, and we DON'T 
   want to get to gbm_bo_destroy() before the prop changes requested
   in drm_atomic_set_plane_props() have effectively been done. So we
   issue a BLOCKING atomic_commit here to avoid that situation.
   REMEMBER you yan issue an atomic_commit whenever you want, and
   the changes requested until that moment (for any planes, crtcs, etc.)
   will be done. */
static void
KMSDRM_FreeCursor(SDL_Cursor * cursor)
{
    KMSDRM_CursorData *curdata = NULL;
    SDL_VideoDevice *video = NULL;
    KMSDRM_PlaneInfo info = {0};

    if (cursor) {
        curdata = (KMSDRM_CursorData *) cursor->driverdata;
        if (video && curdata->bo && curdata->plane) {
            info.plane = curdata->plane; /* The other members are zeroed. */
	    drm_atomic_set_plane_props(&info);
            /* Wait until the cursor is unset from the cursor plane before destroying it's BO. */
            drm_atomic_commit(video, SDL_TRUE);
	    KMSDRM_gbm_bo_destroy(curdata->bo);
	    curdata->bo = NULL;

	    SDL_free(cursor->driverdata);
	    SDL_free(cursor);
        }
    }
}

/* Warp the mouse to (x,y) */
static void
KMSDRM_WarpMouse(SDL_Window * window, int x, int y)
{
    /* Only one global/fullscreen window is supported */
    KMSDRM_WarpMouseGlobal(x, y);
}

/* Warp the mouse to (x,y) */
static int
KMSDRM_WarpMouseGlobal(int x, int y)
{
    KMSDRM_CursorData *curdata;
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse && mouse->cur_cursor && mouse->cur_cursor->driverdata) {
        /* Update internal mouse position. */
        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, 0, x, y);

        /* And now update the cursor graphic position on screen. */
        curdata = (KMSDRM_CursorData *) mouse->cur_cursor->driverdata;
        if (curdata->bo) {
	    int ret;

            ret = drm_atomic_movecursor(curdata, x, y);

	    if (ret) {
		SDL_SetError("drm_atomic_movecursor() failed.");
	    }

	    return ret;

        } else {
            return SDL_SetError("Cursor not initialized properly.");
        }
    } else {
        return SDL_SetError("No mouse or current cursor.");
    }
return 0;
}

void
KMSDRM_InitMouse(_THIS)
{
    /* FIXME: Using UDEV it should be possible to scan all mice
     * but there's no point in doing so as there's no multimice support...yet!
     */

    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = KMSDRM_CreateCursor;
    mouse->ShowCursor = KMSDRM_ShowCursor;
    mouse->MoveCursor = KMSDRM_MoveCursor;
    mouse->FreeCursor = KMSDRM_FreeCursor;
    mouse->WarpMouse = KMSDRM_WarpMouse;
    mouse->WarpMouseGlobal = KMSDRM_WarpMouseGlobal;

    /* Init cursor plane, if we haven't yet. */
    if (!dispdata->cursor_plane) {
	setup_plane(_this, &(dispdata->cursor_plane), DRM_PLANE_TYPE_CURSOR);
    }

    SDL_SetDefaultCursor(KMSDRM_CreateDefaultCursor());
}

void
KMSDRM_QuitMouse(_THIS)
{
    /* Free the plane on which the cursor was being shown. */
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    free_plane(&dispdata->cursor_plane);
}

/* This is called when a mouse motion event occurs */
static void
KMSDRM_MoveCursor(SDL_Cursor * cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    KMSDRM_CursorData *curdata;
    int ret;

    /* We must NOT call SDL_SendMouseMotion() here or we will enter recursivity!
       That's why we move the cursor graphic ONLY. */
    if (mouse && mouse->cur_cursor && mouse->cur_cursor->driverdata) {
        curdata = (KMSDRM_CursorData *) mouse->cur_cursor->driverdata;
        /* Some programs expect cursor movement even while they don't do SwapWindow() calls,
           and since we ride on the atomic_commit() in SwapWindow() for cursor movement,
           cursor won't move in these situations. We could do an atomic_commit() for each
           cursor movement request, but it cripples the movement to 30FPS, so a future solution
           is needed. SDLPoP "QUIT?" menu is an example of this situation. */
        ret = drm_atomic_movecursor(curdata, mouse->x, mouse->y);

	if (ret) {
	    SDL_SetError("drm_atomic_movecursor() failed.");
	}
    }
}

#endif /* SDL_VIDEO_DRIVER_KMSDRM */

/* vi: set ts=4 sw=4 expandtab: */
