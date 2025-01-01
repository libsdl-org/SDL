/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_PSP

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "SDL_render.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../render/SDL_sysrender.h"


/* PSP declarations */
#include "SDL_pspvideo.h"
#include "SDL_pspevents_c.h"
#include "SDL_pspgl_c.h"

#include <psputility.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <vram.h>
#include "../../render/psp/SDL_render_psp.h"

#define SDL_WINDOWTEXTUREDATA "_SDL_WindowTextureData"

typedef struct
{
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    void *pixels;
    int pitch;
    int bytes_per_pixel;
} SDL_WindowTextureData;

int PSP_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_RendererInfo info;
    SDL_WindowTextureData *data = SDL_GetWindowData(window, SDL_WINDOWTEXTUREDATA);
    int i, w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    if (w != PSP_SCREEN_WIDTH) {
        return SDL_SetError("Unexpected window size");
    }

    if (!data) {
        SDL_Renderer *renderer = NULL;
        for (i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
            SDL_GetRenderDriverInfo(i, &info);
            if (SDL_strcmp(info.name, "software") != 0) {
                renderer = SDL_CreateRenderer(window, i, 0);
                if (renderer && (SDL_GetRendererInfo(renderer, &info) == 0) && (info.flags & SDL_RENDERER_ACCELERATED)) {
                    break; /* this will work. */
                }
                if (renderer) { /* wasn't accelerated, etc, skip it. */
                    SDL_DestroyRenderer(renderer);
                    renderer = NULL;
                }
            }
        }
        if (!renderer) {
            return SDL_SetError("No hardware accelerated renderers available");
        }

        SDL_assert(renderer != NULL); /* should have explicitly checked this above. */

        /* Create the data after we successfully create the renderer (bug #1116) */
        data = (SDL_WindowTextureData *)SDL_calloc(1, sizeof(*data));

        if (!data) {
            SDL_DestroyRenderer(renderer);
            return SDL_OutOfMemory();
        }

        SDL_SetWindowData(window, SDL_WINDOWTEXTUREDATA, data);
        data->renderer = renderer;
    } else {
        if (SDL_GetRendererInfo(data->renderer, &info) == -1) {
            return -1;
        }
    }

    /* Find the first format without an alpha channel */
    *format = info.texture_formats[0];

    for (i = 0; i < (int)info.num_texture_formats; ++i) {
        if (!SDL_ISPIXELFORMAT_FOURCC(info.texture_formats[i]) &&
            !SDL_ISPIXELFORMAT_ALPHA(info.texture_formats[i])) {
            *format = info.texture_formats[i];
            break;
        }
    }

    /* get the PSP renderer's "private" data */
    SDL_PSP_RenderGetProp(data->renderer, SDL_PSP_RENDERPROPS_FRONTBUFFER, &data->pixels);

    /* Create framebuffer data */
    data->bytes_per_pixel = SDL_BYTESPERPIXEL(*format);
    /* since we point directly to VRAM's frontbuffer, we have to use
       the upscaled pitch of 512 width - since PSP requires all textures to be
       powers of 2. */
    data->pitch = 512 * data->bytes_per_pixel;
    *pixels = data->pixels;
    *pitch = data->pitch;

    /* Make sure we're not double-scaling the viewport */
    SDL_RenderSetViewport(data->renderer, NULL);

    return 0;
}

int PSP_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowTextureData *data;
    data = SDL_GetWindowData(window, SDL_WINDOWTEXTUREDATA);
    if (!data || !data->renderer || !window->surface) {
        return -1;
    }
    data->renderer->RenderPresent(data->renderer);
    SDL_PSP_RenderGetProp(data->renderer, SDL_PSP_RENDERPROPS_BACKBUFFER, &window->surface->pixels);
    return 0;
}

void PSP_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_WindowTextureData *data = SDL_GetWindowData(window, SDL_WINDOWTEXTUREDATA);
    if (!data || !data->renderer) {
        return;
    }
    SDL_DestroyRenderer(data->renderer);
    data->renderer = NULL;
}

/* unused
static SDL_bool PSP_initialized = SDL_FALSE;
*/

static void PSP_Destroy(SDL_VideoDevice *device)
{
    /*    SDL_VideoData *phdata = (SDL_VideoData *) device->driverdata; */

    if (device->driverdata) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *PSP_Create()
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;
    SDL_GLDriverData *gldata;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal PSP specific data */
    phdata = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!phdata) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    gldata = (SDL_GLDriverData *)SDL_calloc(1, sizeof(SDL_GLDriverData));
    if (!gldata) {
        SDL_OutOfMemory();
        SDL_free(device);
        SDL_free(phdata);
        return NULL;
    }
    device->gl_data = gldata;

    device->driverdata = phdata;

    phdata->egl_initialized = SDL_TRUE;

    /* Setup amount of available displays */
    device->num_displays = 0;

    /* Set device free function */
    device->free = PSP_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = PSP_VideoInit;
    device->VideoQuit = PSP_VideoQuit;
    device->GetDisplayModes = PSP_GetDisplayModes;
    device->SetDisplayMode = PSP_SetDisplayMode;
    device->CreateSDLWindow = PSP_CreateWindow;
    device->CreateSDLWindowFrom = PSP_CreateWindowFrom;
    device->SetWindowTitle = PSP_SetWindowTitle;
    device->SetWindowIcon = PSP_SetWindowIcon;
    device->SetWindowPosition = PSP_SetWindowPosition;
    device->SetWindowSize = PSP_SetWindowSize;
    device->ShowWindow = PSP_ShowWindow;
    device->HideWindow = PSP_HideWindow;
    device->RaiseWindow = PSP_RaiseWindow;
    device->MaximizeWindow = PSP_MaximizeWindow;
    device->MinimizeWindow = PSP_MinimizeWindow;
    device->RestoreWindow = PSP_RestoreWindow;
    device->DestroyWindow = PSP_DestroyWindow;
#if 0
    device->GetWindowWMInfo = PSP_GetWindowWMInfo;
#endif
    device->GL_LoadLibrary = PSP_GL_LoadLibrary;
    device->GL_GetProcAddress = PSP_GL_GetProcAddress;
    device->GL_UnloadLibrary = PSP_GL_UnloadLibrary;
    device->GL_CreateContext = PSP_GL_CreateContext;
    device->GL_MakeCurrent = PSP_GL_MakeCurrent;
    device->GL_SetSwapInterval = PSP_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PSP_GL_GetSwapInterval;
    device->GL_SwapWindow = PSP_GL_SwapWindow;
    device->GL_DeleteContext = PSP_GL_DeleteContext;
    device->HasScreenKeyboardSupport = PSP_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PSP_ShowScreenKeyboard;
    device->HideScreenKeyboard = PSP_HideScreenKeyboard;
    device->IsScreenKeyboardShown = PSP_IsScreenKeyboardShown;

    device->PumpEvents = PSP_PumpEvents;

    /* backend to use VRAM directly as a framebuffer using
       SDL_GetWindowSurface() API. */
    device->checked_texture_framebuffer = 1;
    device->CreateWindowFramebuffer = PSP_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PSP_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PSP_DestroyWindowFramebuffer;

    return device;
}

static void configure_dialog(pspUtilityMsgDialogParams *dialog, size_t dialog_size)
{
	/* clear structure and setup size */
	SDL_memset(dialog, 0, dialog_size);
	dialog->base.size = dialog_size;

	/* set language */
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &dialog->base.language);

	/* set X/O swap */
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &dialog->base.buttonSwap);

	/* set thread priorities */
	/* TODO: understand how these work */
	dialog->base.soundThread = 0x10;
	dialog->base.graphicsThread = 0x11;
	dialog->base.fontThread = 0x12;
	dialog->base.accessThread = 0x13;
}

static void *setup_temporal_gu(void *list)
{
    // Using GU_PSM_8888 for the framebuffer
	int bpp = 4;
	
	void *doublebuffer = vramalloc(PSP_FRAME_BUFFER_SIZE * bpp * 2);
    void *backbuffer = doublebuffer;
    void *frontbuffer = ((uint8_t *)doublebuffer) + PSP_FRAME_BUFFER_SIZE * bpp;

    sceGuInit();

    sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888, vrelptr(frontbuffer), PSP_FRAME_BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, vrelptr(backbuffer), PSP_FRAME_BUFFER_WIDTH);

    sceGuOffset(2048 - (PSP_SCREEN_WIDTH >> 1), 2048 - (PSP_SCREEN_HEIGHT >> 1));
    sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

    sceGuDisable(GU_DEPTH_TEST);

    /* Scissoring */
    sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuFinish();
    sceGuSync(0,0);
    
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

	return doublebuffer;
}

static void term_temporal_gu(void *guBuffer)
{
	sceGuTerm();
	vfree(guBuffer);
	sceDisplayWaitVblankStart();
}

int PSP_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
	unsigned char list[64] __attribute__((aligned(64)));
	pspUtilityMsgDialogParams dialog;
	int status;
	void *guBuffer = NULL;

	/* check if it's possible to use existing video context */
	if (SDL_GetFocusWindow() == NULL) {
		guBuffer = setup_temporal_gu(list);
	}

	/* configure dialog */
	configure_dialog(&dialog, sizeof(dialog));

	/* setup dialog options for text */
	dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_TEXT;

	/* copy the message in, 512 bytes max */
	SDL_snprintf(dialog.message, sizeof(dialog.message), "%s\r\n\r\n%s", messageboxdata->title, messageboxdata->message);

	/* too many buttons */
	if (messageboxdata->numbuttons > 2)
		return SDL_SetError("messageboxdata->numbuttons valid values are 0, 1, 2");

	/* we only have two options, "yes/no" or "ok" */
	if (messageboxdata->numbuttons == 2)
		dialog.options |= PSP_UTILITY_MSGDIALOG_OPTION_YESNO_BUTTONS | PSP_UTILITY_MSGDIALOG_OPTION_DEFAULT_NO;

	/* start dialog */
	if (sceUtilityMsgDialogInitStart(&dialog) != 0)
		return SDL_SetError("sceUtilityMsgDialogInitStart() failed for some reason");

	/* loop while the dialog is active */
	status = PSP_UTILITY_DIALOG_NONE;
	do
	{
		sceGuStart(GU_DIRECT, list);
		sceGuClearColor(0);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
		sceGuFinish();
		sceGuSync(0,0);

		status = sceUtilityMsgDialogGetStatus();

		switch (status)
		{
			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityMsgDialogUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityMsgDialogShutdownStart();
				break;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();

	} while (status != PSP_UTILITY_DIALOG_NONE);

	/* cleanup */
	if (guBuffer)
	{
		term_temporal_gu(guBuffer);
	}

	/* success */
	if (dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_YES)
		*buttonid = messageboxdata->buttons[0].buttonid;
	else if (dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_NO)
		*buttonid = messageboxdata->buttons[1].buttonid;
	else
		*buttonid = messageboxdata->buttons[0].buttonid;

	return 0;
}

VideoBootStrap PSP_bootstrap = {
    "PSP",
    "PSP Video Driver",
    PSP_Create,
    PSP_ShowMessageBox
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int PSP_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    if (PSP_EventInit(_this) == -1) {
        return -1;  /* error string would already be set */
    }

    SDL_zero(current_mode);

    current_mode.w = PSP_SCREEN_WIDTH;
    current_mode.h = PSP_SCREEN_HEIGHT;

    current_mode.refresh_rate = 60;
    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;

    SDL_AddDisplayMode(&display, &current_mode);

    /* 16 bpp secondary mode */
    current_mode.format = SDL_PIXELFORMAT_BGR565;
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    SDL_AddDisplayMode(&display, &current_mode);

    SDL_AddVideoDisplay(&display, SDL_FALSE);

    return 0;
}

void PSP_VideoQuit(_THIS)
{
    PSP_EventQuit(_this);
}

void PSP_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
}

int PSP_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}
#define EGLCHK(stmt)                           \
    do {                                       \
        EGLint err;                            \
                                               \
        stmt;                                  \
        err = eglGetError();                   \
        if (err != EGL_SUCCESS) {              \
            SDL_SetError("EGL error %d", err); \
            return 0;                          \
        }                                      \
    } while (0)

int PSP_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *wdata;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!wdata) {
        return SDL_OutOfMemory();
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

int PSP_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    return SDL_Unsupported();
}

void PSP_SetWindowTitle(_THIS, SDL_Window *window)
{
}
void PSP_SetWindowIcon(_THIS, SDL_Window *window, SDL_Surface *icon)
{
}
void PSP_SetWindowPosition(_THIS, SDL_Window *window)
{
}
void PSP_SetWindowSize(_THIS, SDL_Window *window)
{
}
void PSP_ShowWindow(_THIS, SDL_Window *window)
{
}
void PSP_HideWindow(_THIS, SDL_Window *window)
{
}
void PSP_RaiseWindow(_THIS, SDL_Window *window)
{
}
void PSP_MaximizeWindow(_THIS, SDL_Window *window)
{
}
void PSP_MinimizeWindow(_THIS, SDL_Window *window)
{
}
void PSP_RestoreWindow(_THIS, SDL_Window *window)
{
}
void PSP_DestroyWindow(_THIS, SDL_Window *window)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
#if 0
SDL_bool PSP_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d",
                     SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}
#endif



SDL_bool PSP_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return SDL_TRUE;
}

void PSP_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    char list[0x20000] __attribute__((aligned(64)));  // Needed for sceGuStart to work
    int i;
    int done = 0;
    int input_text_length = 32; // SDL_SendKeyboardText supports up to 32 characters per event
    unsigned short outtext[input_text_length];
    char text_string[input_text_length];

    SceUtilityOskData data;
    SceUtilityOskParams params;

    SDL_memset(outtext, 0, input_text_length * sizeof(unsigned short));

    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = NULL;
    data.intext = NULL;
    data.outtextlength = input_text_length;
    data.outtextlimit = input_text_length;
    data.outtext = outtext;

    params.base.size = sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &params.base.buttonSwap);
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    sceUtilityOskInitStart(&params);

    while(!done) {
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(0,0);

        switch(sceUtilityOskGetStatus())
        {
            case PSP_UTILITY_DIALOG_VISIBLE:
                sceUtilityOskUpdate(1);
                break;
            case PSP_UTILITY_DIALOG_QUIT:
                sceUtilityOskShutdownStart();
                break;
            case PSP_UTILITY_DIALOG_NONE:
                done = 1;
                break;
            default :
                break;
        }
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    // Convert input list to string
    for (i = 0; i < input_text_length; i++) {
        text_string[i] = outtext[i];
    }
    SDL_SendKeyboardText((const char *) text_string);
}
void PSP_HideScreenKeyboard(_THIS, SDL_Window *window)
{
}
SDL_bool PSP_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    return SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_PSP */

/* vi: set ts=4 sw=4 expandtab: */
