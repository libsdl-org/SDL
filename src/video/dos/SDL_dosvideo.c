/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_DOSVESA

// SDL internals
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../SDL_sysvideo.h"

// DOS declarations
#include "SDL_dosevents_c.h"
#include "SDL_dosframebuffer_c.h"
#include "SDL_dosmodes.h"
#include "SDL_dosmouse.h"
#include "SDL_dosvideo.h"

// Apply a display mode for a window: set the hardware mode, store it as
// the window's requested fullscreen mode, and update window dimensions.
static void DOSVESA_ApplyModeForWindow(SDL_VideoDisplay *display, SDL_Window *window, SDL_DisplayMode *mode)
{
    SDL_SetDisplayModeForDisplay(display, mode);

    if (mode) {
        SDL_copyp(&window->requested_fullscreen_mode, mode);
        window->floating.w = window->windowed.w = window->w = mode->w;
        window->floating.h = window->windowed.h = window->h = mode->h;
    }
}

static bool DOSVESA_CreateWindow(SDL_VideoDevice *device, SDL_Window *window, SDL_PropertiesID create_props)
{
    // Allocate window internal data
    SDL_WindowData *wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!wdata) {
        return false;
    }

    // Setup driver data for this window
    window->internal = wdata;

    // One window, it always has focus
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    {
        SDL_VideoDisplay *display = SDL_GetVideoDisplayForWindow(window);
        if (!display) {
            return true;
        }

        SDL_DisplayMode *mode = NULL;
        SDL_DisplayMode closest;
        if (window->requested_fullscreen_mode.internal) {
            // App explicitly set a fullscreen mode.
            mode = &window->requested_fullscreen_mode;
        } else if (window->floating.w > 0 && window->floating.h > 0) {
            if (SDL_GetClosestFullscreenDisplayMode(display->id, window->floating.w, window->floating.h, 0.0f, false, &closest)) {
                mode = &closest;
            }
        }
        if (!mode) {
            return true;
        }

        DOSVESA_ApplyModeForWindow(display, window, mode);
    }

    return true;
}

static void DOSVESA_SetWindowSize(SDL_VideoDevice *device, SDL_Window *window)
{
    SDL_VideoDisplay *display = SDL_GetVideoDisplayForWindow(window);
    if (!display) {
        return;
    }

    SDL_DisplayMode closest;
    SDL_DisplayMode *mode = NULL;
    if (SDL_GetClosestFullscreenDisplayMode(display->id, window->floating.w, window->floating.h, 0.0f, false, &closest)) {
        mode = &closest;
    }

    DOSVESA_ApplyModeForWindow(display, window, mode);

    // Invalidate the framebuffer so it gets recreated at the new size.
    window->surface_valid = false;
}

// Critical for performance: this function must be implemented and as simple
// as possible to avoid slowdowns during calls to SDL_UpdateWindowSurface().
// A few pointer dereferences here can cost 10% of performance easily.
static void DOSVESA_GetWindowSizeInPixels(SDL_VideoDevice *device, SDL_Window *window, int *w, int *h)
{
    *w = window->w;
    *h = window->h;
}

static void DOSVESA_DestroyWindow(SDL_VideoDevice *device, SDL_Window *window)
{
    SDL_free(window->internal);
    window->internal = NULL;
}

static bool DOSVESA_VideoInit(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;

    // Verify that the "fat DS" nearptr trick is active. Without it,
    // DOS_PhysicalToLinear() produces garbage pointers and we crash.
    // SDL_RunApp() enables this automatically; if the app defined
    // SDL_MAIN_HANDLED it must call __djgpp_nearptr_enable() itself.
    if (__djgpp_conventional_base == 0) {
        return SDL_SetError("DOSVESA: __djgpp_nearptr_enable() was not called. "
                            "Did you define SDL_MAIN_HANDLED without enabling the fat DS trick?");
    }

    // We are probably in text mode at startup, so we don't have a real "desktop mode" atm.
    // Pick something _super_ conservative for now.
    //  We'll change to a real video mode after enumerating available modes below.
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = 320;
    mode.h = 200;

    SDL_VideoDisplay vdisplay;
    SDL_zero(vdisplay);
    SDL_memcpy(&vdisplay.desktop_mode, &mode, sizeof(mode));
    vdisplay.name = (char *)DOSVESA_GetGPUName();
    SDL_DisplayID display_id = SDL_AddVideoDisplay(&vdisplay, false);
    if (!display_id) {
        return false;
    }

    SDL_zero(data->current_mode);

    SDL_VideoDisplay *display = SDL_GetVideoDisplay(display_id);
    if (!display || !DOSVESA_GetDisplayModes(device, display)) {
        return false;
    }

    // Pick a sensible default desktop mode. This determines the window
    // size for FULLSCREEN_ONLY. Target 640x480 as a safe default; apps
    // that want something else should call SDL_SetWindowFullscreenMode.
    {
        SDL_DisplayMode closest;
        if (SDL_GetClosestFullscreenDisplayMode(display_id, 640, 480, 0.0f, false, &closest)) {
            // Deep-copy the mode into desktop_mode. We need our own
            // internal allocation because SDL frees desktop_mode.internal
            // and fullscreen_modes[].internal independently.
            SDL_DisplayModeData *desktop_internal = (SDL_DisplayModeData *)SDL_malloc(sizeof(*desktop_internal));
            if (desktop_internal) {
                SDL_copyp(desktop_internal, (const SDL_DisplayModeData *)closest.internal);
                SDL_copyp(&display->desktop_mode, &closest);
                display->desktop_mode.internal = desktop_internal;
            }
        }
    }

    // Save the current video mode so we can restore it on quit.
    // The VBE calls (0x4F03, 0x4F04) return 0x004F on success; on cards
    // without VBE support (or VBE < 1.2) they simply fail and we fall
    // back to restoring standard text mode 0x03.
    {
        __dpmi_regs regs;
        SDL_zero(regs);
        regs.x.ax = 0x4F03; // VBE Get Current Mode
        __dpmi_int(0x10, &regs);
        if (regs.x.ax == 0x004F) {
            data->original_vbe_mode = regs.x.bx;
        } else {
            data->original_vbe_mode = 0x03; // assume text mode
        }

        // Save VBE state via VBE function 0x4F04 so we can do a full restore later.
        // Step 1: query the required buffer size (subfunction 0x00).
        SDL_zero(regs);
        regs.x.ax = 0x4F04;
        regs.x.dx = 0x00; // subfunction 0: get state buffer size
        regs.x.cx = 0x0F; // save all state: hardware + BIOS data + DAC + SVGA
        __dpmi_int(0x10, &regs);
        if (regs.x.ax == 0x004F) {
            // regs.x.bx contains size in 64-byte blocks.
            Uint32 state_size = (Uint32)regs.x.bx * 64;
            _go32_dpmi_seginfo state_seginfo;
            void *state_buf = DOS_AllocateConventionalMemory(state_size, &state_seginfo);
            if (state_buf) {
                // Step 2: save state (subfunction 0x01) into conventional memory buffer.
                SDL_zero(regs);
                regs.x.ax = 0x4F04;
                regs.x.dx = 0x01; // subfunction 1: save state
                regs.x.cx = 0x0F; // all state
                regs.x.es = DOS_LinearToPhysical(state_buf) / 16;
                regs.x.bx = DOS_LinearToPhysical(state_buf) & 0xF;
                __dpmi_int(0x10, &regs);
                if (regs.x.ax == 0x004F) {
                    // Copy state from conventional memory to our heap so we
                    // can free the low-memory buffer now.
                    data->vbe_state_buffer = SDL_malloc(state_size);
                    if (data->vbe_state_buffer) {
                        SDL_memcpy(data->vbe_state_buffer, state_buf, state_size);
                        data->vbe_state_buffer_size = state_size;
                    }
                }
                DOS_FreeConventionalMemory(&state_seginfo);
            }
        }
    }

    DOSVESA_InitMouse(device);
    DOSVESA_InitKeyboard(device);

    return true;
}

static void DOSVESA_VideoQuit(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;

    if (data->mapping.size) {
        __dpmi_free_physical_address_mapping(&data->mapping); // dump existing video mapping.
        SDL_zero(data->mapping);
    }

    // Restore saved VBE state if available.
    if (data->vbe_state_buffer && data->vbe_state_buffer_size > 0) {
        _go32_dpmi_seginfo restore_seginfo;
        void *restore_buf = DOS_AllocateConventionalMemory(data->vbe_state_buffer_size, &restore_seginfo);
        if (restore_buf) {
            SDL_memcpy(restore_buf, data->vbe_state_buffer, data->vbe_state_buffer_size);
            __dpmi_regs regs;
            SDL_zero(regs);
            regs.x.ax = 0x4F04;
            regs.x.dx = 0x02; // subfunction 2: restore state
            regs.x.cx = 0x0F; // all state
            regs.x.es = DOS_LinearToPhysical(restore_buf) / 16;
            regs.x.bx = DOS_LinearToPhysical(restore_buf) & 0xF;
            __dpmi_int(0x10, &regs);
            DOS_FreeConventionalMemory(&restore_seginfo);
        }
        SDL_free(data->vbe_state_buffer);
        data->vbe_state_buffer = NULL;
        data->vbe_state_buffer_size = 0;
    }

    // Restore the original video mode.
    {
        __dpmi_regs regs;
        bool restored = false;

        // Try VBE mode restore first (only works on VBE 1.2+).
        if (data->original_vbe_mode != 0x03) {
            SDL_zero(regs);
            regs.x.ax = 0x4F02;
            regs.x.bx = data->original_vbe_mode;
            __dpmi_int(0x10, &regs);
            restored = (regs.x.ax == 0x004F);
        }

        // Fall back to standard BIOS text mode (works on any VGA).
        if (!restored) {
            SDL_zero(regs);
            regs.x.ax = 0x0003;
            __dpmi_int(0x10, &regs);
        }
    }

    SDL_zero(data->current_mode);

    DOSVESA_QuitMouse(device);
    DOSVESA_QuitKeyboard(device);
}

static void DOSVESA_Destroy(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;
    SDL_free(data->vbe_state_buffer);
    SDL_free(device->internal);
    SDL_free(device);
    DOSVESA_FreeVESAInfo();
}

static SDL_VideoDevice *DOSVESA_CreateDevice(void)
{
    if (!DOSVESA_SupportsVESA()) {
        return NULL;
    }

    SDL_VideoDevice *device;
    SDL_VideoData *phdata;

    // Initialize SDL_VideoDevice structure
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    // Initialize internal data
    phdata = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!phdata) {
        SDL_free(device);
        return NULL;
    }

    device->internal = phdata;
    device->free = DOSVESA_Destroy;
    device->VideoInit = DOSVESA_VideoInit;
    device->VideoQuit = DOSVESA_VideoQuit;
    device->GetDisplayModes = DOSVESA_GetDisplayModes;
    device->SetDisplayMode = DOSVESA_SetDisplayMode;
    device->CreateSDLWindow = DOSVESA_CreateWindow;
    device->SetWindowSize = DOSVESA_SetWindowSize;
    device->DestroyWindow = DOSVESA_DestroyWindow;
    device->CreateWindowFramebuffer = DOSVESA_CreateWindowFramebuffer;
    device->SetWindowFramebufferVSync = DOSVESA_SetWindowFramebufferVSync;
    device->GetWindowFramebufferVSync = DOSVESA_GetWindowFramebufferVSync;
    device->UpdateWindowFramebuffer = DOSVESA_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = DOSVESA_DestroyWindowFramebuffer;
    device->GetWindowSizeInPixels = DOSVESA_GetWindowSizeInPixels;
    device->PumpEvents = DOSVESA_PumpEvents;
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;

    return device;
}

VideoBootStrap DOSVESA_bootstrap = {
    "vesa",
    "DOS VESA Video Driver",
    DOSVESA_CreateDevice,
    NULL, // no ShowMessageBox implementation
    false
};

#endif // SDL_VIDEO_DRIVER_DOSVESA
