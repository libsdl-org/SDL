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

#include "SDL_atomicvideo.h"
#include "SDL_atomicmouse.h"
#include "SDL_atomicdyn.h"
#include "SDL_assert.h"

#include "../../events/SDL_mouse_c.h"
#include "../../events/default_cursor.h"

static SDL_Cursor *ATOMIC_CreateDefaultCursor(void);
static SDL_Cursor *ATOMIC_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y);
static int ATOMIC_ShowCursor(SDL_Cursor * cursor);
static void ATOMIC_MoveCursor(SDL_Cursor * cursor);
static void ATOMIC_FreeCursor(SDL_Cursor * cursor);
static void ATOMIC_WarpMouse(SDL_Window * window, int x, int y);
static int ATOMIC_WarpMouseGlobal(int x, int y);

/**************************************************************************************/
/* BEFORE CODING ANYTHING MOUSE/CURSOR RELATED, REMEMBER THIS.                        */
/* How does SDL manage cursors internally? First, mouse =! cursor. The mouse can have */
/* many cursors in mouse->cursors.                                                    */
/* -SDL tells us to create a cursor with ATOMIC_CreateCursor(). It can create many    */
/*  cursosr with this, not only one.                                                  */
/* -SDL stores those cursors in a cursors array, in mouse->cursors.                   */
/* -Whenever it wants (or the programmer wants) takes a cursor from that array        */
/*  and shows it on screen with ATOMIC_ShowCursor().                                  */
/*  ATOMIC_ShowCursor() simply shows or hides the cursor it receives: it does NOT     */
/*  mind if it's mouse->cur_cursor, etc.                                              */
/* -If ATOMIC_ShowCursor() returns succesfully, that cursor becomes mouse->cur_cursor */
/*  and mouse->cursor_shown is 1.                                                     */
/**************************************************************************************/

/**********************************/
/* Atomic helper functions block. */
/**********************************/

int
drm_atomic_movecursor(ATOMIC_CursorData *curdata, uint16_t x, uint16_t y)
{
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    if (!dispdata->cursor_plane) /* We can't move a non-existing cursor, but that's ok. */
        return 0;

    /* Do we have a set of changes already in the making? If not, allocate a new one. */
    if (!dispdata->atomic_req)
        dispdata->atomic_req = ATOMIC_drmModeAtomicAlloc();
    
    add_plane_property(dispdata->atomic_req,
            dispdata->cursor_plane, "CRTC_X", x - curdata->hot_x);
    add_plane_property(dispdata->atomic_req,
            dispdata->cursor_plane, "CRTC_Y", y - curdata->hot_y);

    return 0;
}

/***************************************/
/* Atomic helper functions block ends. */
/***************************************/

/* Converts a pixel from straight-alpha [AA, RR, GG, BB], which the SDL cursor surface has,
   to premultiplied-alpha [AA. AA*RR, AA*GG, AA*BB].
   These multiplications have to be done with floats instead of uint32_t's,
   and the resulting values have to be converted to be relative to the 0-255 interval,
   where 255 is 1.00 and anything between 0 and 255 is 0.xx. */
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
ATOMIC_CreateDefaultCursor(void)
{
    return SDL_CreateCursor(default_cdata, default_cmask, DEFAULT_CWIDTH, DEFAULT_CHEIGHT, DEFAULT_CHOTX, DEFAULT_CHOTY);
}

/* This simply gets the cursor soft-buffer ready. We don't copy it to a GBO BO
   until ShowCursor() because the cusor GBM BO (living in dispata) is destroyed
   and recreated when we recreate windows, etc. */
static SDL_Cursor *
ATOMIC_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    ATOMIC_CursorData *curdata;
    SDL_Cursor *cursor, *ret;

    curdata = NULL;
    ret = NULL;

    /* All code below assumes ARGB8888 format for the cursor surface,
       like other backends do. Also, the GBM BO pixels have to be
       alpha-premultiplied, but the SDL surface we receive has
       straight-alpha pixels, so we always have to convert. */ 
    SDL_assert(surface->format->format == SDL_PIXELFORMAT_ARGB8888);
    SDL_assert(surface->pitch == surface->w * 4);

    cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        SDL_OutOfMemory();
        goto cleanup;
    }
    curdata = (ATOMIC_CursorData *) SDL_calloc(1, sizeof(*curdata));
    if (!curdata) {
        SDL_OutOfMemory();
        goto cleanup;
    }

    /* hox_x and hot_y are the coordinates of the "tip of the cursor" from it's base. */
    curdata->hot_x = hot_x;
    curdata->hot_y = hot_y;
    curdata->w = surface->w;
    curdata->h = surface->h;
    curdata->buffer = NULL;

    /* Configure the cursor buffer info.
       This buffer has the original size of the cursor surface we are given. */
    curdata->buffer_pitch = surface->pitch;
    curdata->buffer_size = surface->pitch * surface->h;
    curdata->buffer = (uint32_t*)SDL_malloc(curdata->buffer_size);

    if (!curdata->buffer) {
        SDL_OutOfMemory();
        goto cleanup;
    }

    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) < 0) {
            /* Could not lock surface */
            goto cleanup;
        }
    }

    /* Copy the surface pixels to the cursor buffer, for future use in ShowCursor() */
    SDL_memcpy(curdata->buffer, surface->pixels, curdata->buffer_size);

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }

    cursor->driverdata = curdata;

    ret = cursor;

cleanup:
    if (ret == NULL) {
	if (curdata) {
	    if (curdata->buffer) {
		SDL_free(curdata->buffer);
	    }
	    SDL_free(curdata);
	}
	if (cursor) {
	    SDL_free(cursor);
	}
    }

    return ret;
}

/* When we create a window, we have to test if we have to show the cursor,
   and explicily do so if necessary.
   This is because when we destroy a window, we take the cursor away from the
   cursor plane, and destroy the cusror GBM BO. So we have to re-show it,
   so to say. */
void
ATOMIC_InitCursor()
{
    SDL_Mouse *mouse = NULL;
    mouse = SDL_GetMouse();

    if (!mouse) {
        return;
    }
    if  (!(mouse->cur_cursor)) {
        return;
    }

    if  (!(mouse->cursor_shown)) {
        return;
    }

    ATOMIC_ShowCursor(mouse->cur_cursor);
}

/* Show the specified cursor, or hide if cursor is NULL or has no focus. */
static int
ATOMIC_ShowCursor(SDL_Cursor * cursor)
{
    SDL_VideoDevice *video_device = SDL_GetVideoDevice();
    //SDL_VideoData *viddata = ((SDL_VideoData *)video_device->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_Mouse *mouse;
    ATOMIC_CursorData *curdata;

    ATOMIC_FBInfo *fb;
    ATOMIC_PlaneInfo info = {0};

    size_t bo_stride;
    size_t bufsize;
    uint32_t *ready_buffer = NULL;
    uint32_t pixel;

    int i,j;
    int ret = 0;

    mouse = SDL_GetMouse();
    if (!mouse) {
        return SDL_SetError("No mouse.");
    }

    /*********************************************************/
    /* Hide cursor if it's NULL or it has no focus(=winwow). */
    /*********************************************************/
    if (!cursor || !mouse->focus) {
        if (dispdata->cursor_plane) {
	    /* Hide the drm cursor with no more considerations because
	    SDL_VideoQuit() takes us here after disabling the mouse
	    so there is no mouse->cur_cursor by now. */
	    info.plane = dispdata->cursor_plane;
	    /* The rest of the members are zeroed, so this takes away the cursor
	       from the cursor plane. */
	    drm_atomic_set_plane_props(&info);
	    if (drm_atomic_commit(video_device, SDL_TRUE, SDL_FALSE)) {
		ret = SDL_SetError("Failed atomic commit in ATOMIC_ShowCursor.");
	    }
        }
	return ret;
    }

    /************************************************/
    /* If cursor != NULL, DO show cursor on display */
    /************************************************/
    if (!dispdata->cursor_plane) {
        return SDL_SetError("Hardware cursor plane not initialized.");
    }
    
    curdata = (ATOMIC_CursorData *) cursor->driverdata;

    if (!curdata || !dispdata->cursor_bo) {
        return SDL_SetError("Cursor not initialized properly.");
    }

    /* Prepare a buffer we can dump to our GBM BO (different
       size, alpha premultiplication...) */
    bo_stride = ATOMIC_gbm_bo_get_stride(dispdata->cursor_bo);
    bufsize = bo_stride * curdata->h;

    ready_buffer = (uint32_t*)SDL_malloc(bufsize);
    if (!ready_buffer) {
        ret = SDL_OutOfMemory();
        goto cleanup;
    }

    /* Clean the whole buffer we are preparing. */
    SDL_memset(ready_buffer, 0x00, bo_stride * curdata->h);

    /* Copy from the cursor buffer to a buffer that we can dump to the GBM BO,
       pre-multiplying by alpha each pixel as we go. */
    for (i = 0; i < curdata->h; i++) {
        for (j = 0; j < curdata->w; j++) {
            pixel = ((uint32_t*)curdata->buffer)[i * curdata->w + j];
            alpha_premultiply_ARGB8888 (&pixel);
            SDL_memcpy(ready_buffer + (i * dispdata->cursor_w) + j, &pixel, 4);
        }
    }

    /* Dump the cursor buffer to our GBM BO. */
    if (ATOMIC_gbm_bo_write(dispdata->cursor_bo, ready_buffer, bufsize)) {
        ret = SDL_SetError("Could not write to GBM cursor BO");
        goto cleanup;
    }

    /* Get the fb_id for the GBM BO so we can show it on the cursor plane. */
    fb = ATOMIC_FBFromBO(video_device, dispdata->cursor_bo);

    /* Show the GBM BO buffer on the cursor plane. */
    info.plane = dispdata->cursor_plane;
    info.crtc_id = dispdata->crtc->crtc->crtc_id;
    info.fb_id = fb->fb_id; 
    info.src_w = dispdata->cursor_w;
    info.src_h = dispdata->cursor_h;
    info.crtc_x = mouse->x - curdata->hot_x;
    info.crtc_y = mouse->y - curdata->hot_y;
    info.crtc_w = curdata->w;
    info.crtc_h = curdata->h; 

    drm_atomic_set_plane_props(&info);

    if (drm_atomic_commit(video_device, SDL_TRUE, SDL_FALSE)) {
        ret = SDL_SetError("Failed atomic commit in ATOMIC_ShowCursor.");
        goto cleanup;
    }

    ret = 0;

cleanup:

    if (ready_buffer) {
        SDL_free(ready_buffer);
    }
    return ret;
}

/* This is only for freeing the SDL_cursor.*/
static void
ATOMIC_FreeCursor(SDL_Cursor * cursor)
{
    ATOMIC_CursorData *curdata;

    /* Even if the cursor is not ours, free it. */
    if (cursor) {
        curdata = (ATOMIC_CursorData *) cursor->driverdata;
        /* Free cursor buffer */
        if (curdata->buffer) {
            SDL_free(curdata->buffer);
            curdata->buffer = NULL;
        }
        /* Free cursor itself */
        if (cursor->driverdata) {
            SDL_free(cursor->driverdata);
        }
        SDL_free(cursor);
    }
}

/* Warp the mouse to (x,y) */
static void
ATOMIC_WarpMouse(SDL_Window * window, int x, int y)
{
    /* Only one global/fullscreen window is supported */
    ATOMIC_WarpMouseGlobal(x, y);
}

/* Warp the mouse to (x,y) */
static int
ATOMIC_WarpMouseGlobal(int x, int y)
{
    ATOMIC_CursorData *curdata;
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);

    if (mouse && mouse->cur_cursor && mouse->cur_cursor->driverdata) {
        /* Update internal mouse position. */
        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, 0, x, y);

        /* And now update the cursor graphic position on screen. */
        curdata = (ATOMIC_CursorData *) mouse->cur_cursor->driverdata;
        if (dispdata->cursor_bo) {
            if (drm_atomic_movecursor(curdata, x, y)) {
                return SDL_SetError("drm_atomic_movecursor() failed.");
            }
        } else {
            return SDL_SetError("Cursor not initialized properly.");
        }
    } else {
        return SDL_SetError("No mouse or current cursor.");
    }

    return 0;
}

/* UNDO WHAT WE DID IN ATOMIC_InitMouse(). */
void
ATOMIC_DeinitMouse(_THIS)
{
    SDL_VideoDevice *video_device = SDL_GetVideoDevice();
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    ATOMIC_PlaneInfo info = {0};
 
    /* 1- Destroy the curso GBM BO. */
    if (video_device && dispdata->cursor_bo) {
	/* Unsethe the cursor BO from the cursor plane.
	   (The other members of the plane info are zeroed). */
	info.plane = dispdata->cursor_plane;
	drm_atomic_set_plane_props(&info);
	/* Wait until the cursor is unset from the cursor plane
	   before destroying it's BO. */
	if (drm_atomic_commit(video_device, SDL_TRUE, SDL_FALSE)) {
	    SDL_SetError("Failed atomic commit in ATOMIC_DenitMouse.");
	}
	/* ..and finally destroy the cursor DRM BO! */
	ATOMIC_gbm_bo_destroy(dispdata->cursor_bo);
	dispdata->cursor_bo = NULL;
    }

    /* 2- Free the cursor plane, on which the cursor was being shown. */
    if (dispdata->cursor_plane) {
        free_plane(&dispdata->cursor_plane);
    }
}

void
ATOMIC_InitMouse(_THIS)
{
    SDL_VideoDevice *dev = SDL_GetVideoDevice();
    SDL_VideoData *viddata = ((SDL_VideoData *)dev->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)SDL_GetDisplayDriverData(0);
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = ATOMIC_CreateCursor;
    mouse->ShowCursor = ATOMIC_ShowCursor;
    mouse->MoveCursor = ATOMIC_MoveCursor;
    mouse->FreeCursor = ATOMIC_FreeCursor;
    mouse->WarpMouse = ATOMIC_WarpMouse;
    mouse->WarpMouseGlobal = ATOMIC_WarpMouseGlobal;

    /***************************************************************************/
    /* REMEMBER TO BE SURE OF UNDOING ALL THESE STEPS PROPERLY BEFORE CALLING  */
    /* gbm_device_destroy, OR YOU WON'T BE ABLE TO CREATE A NEW ONE (ERROR -13 */
    /* on gbm_create_device).                                                  */
    /***************************************************************************/

    /* 1- Init cursor plane, if we haven't yet. */
    if (!dispdata->cursor_plane) {
        setup_plane(_this, &(dispdata->cursor_plane), DRM_PLANE_TYPE_CURSOR);
    }

    /* 2- Create the cursor GBM BO, if we haven't yet. */
    if (!dispdata->cursor_bo) {

        if (!ATOMIC_gbm_device_is_format_supported(viddata->gbm_dev, GBM_FORMAT_ARGB8888,
                                               GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE))
        {
            SDL_SetError("Unsupported pixel format for cursor");
            return;
        }

	if (ATOMIC_drmGetCap(viddata->drm_fd, DRM_CAP_CURSOR_WIDTH,  &dispdata->cursor_w) ||
	    ATOMIC_drmGetCap(viddata->drm_fd, DRM_CAP_CURSOR_HEIGHT, &dispdata->cursor_h))
	{
	    SDL_SetError("Could not get the recommended GBM cursor size");
	    goto cleanup;
	}

	if (dispdata->cursor_w == 0 || dispdata->cursor_h == 0) {
	    SDL_SetError("Could not get an usable GBM cursor size");
	    goto cleanup;
	}

	dispdata->cursor_bo = ATOMIC_gbm_bo_create(viddata->gbm_dev,
	    dispdata->cursor_w, dispdata->cursor_h,
	    GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

	if (!dispdata->cursor_bo) {
	    SDL_SetError("Could not create GBM cursor BO");
	    goto cleanup;
	}
    }

    /* SDL expects to set the default cursor on screen when we init the mouse,
       but since we have moved the ATOMIC_InitMouse() call to ATOMIC_CreateWindow(),
       we end up calling ATOMIC_InitMouse() every time we create a window, so we
       have to prevent this from being done every time a new window is created.
       If we don't, new default cursors would stack up on mouse->cursors and SDL
       would have to hide and delete them at quit, not to mention the memory leak... */
    if(dispdata->set_default_cursor_pending) { 
        SDL_SetDefaultCursor(ATOMIC_CreateDefaultCursor());
        dispdata->set_default_cursor_pending = SDL_FALSE;
    }

    return;

cleanup:
    if (dispdata->cursor_bo) {
	ATOMIC_gbm_bo_destroy(dispdata->cursor_bo);
	dispdata->cursor_bo = NULL;
    }
}

/* This is called when a mouse motion event occurs */
static void
ATOMIC_MoveCursor(SDL_Cursor * cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    ATOMIC_CursorData *curdata;

    /* We must NOT call SDL_SendMouseMotion() here or we will enter recursivity!
       That's why we move the cursor graphic ONLY. */
    if (mouse && mouse->cur_cursor && mouse->cur_cursor->driverdata) {
        curdata = (ATOMIC_CursorData *) mouse->cur_cursor->driverdata;

        /* Some programs expect cursor movement even while they don't do SwapWindow() calls,
           and since we ride on the atomic_commit() in SwapWindow() for cursor movement,
           cursor won't move in these situations. We could do an atomic_commit() here
           for each cursor movement request, but it cripples the movement to 30FPS,
           so a future solution is needed. SDLPoP "QUIT?" menu is an example of this
           situation. */

        if (drm_atomic_movecursor(curdata, mouse->x, mouse->y)) {
            SDL_SetError("drm_atomic_movecursor() failed.");
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_ATOMIC */

/* vi: set ts=4 sw=4 expandtab: */
