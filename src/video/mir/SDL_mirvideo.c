/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

/*
  Contributed by Brandon Schaefer, <brandon.schaefer@canonical.com>
*/

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MIR

#include "SDL_video.h"

#include "SDL_mirframebuffer.h"
#include "SDL_mirmouse.h"
#include "SDL_miropengl.h"
#include "SDL_mirvideo.h"
#include "SDL_mirwindow.h"

#include "SDL_mirdyn.h"

#define MIR_DRIVER_NAME "mir"

static int
MIR_VideoInit(_THIS);

static void
MIR_VideoQuit(_THIS);

static int
MIR_GetDisplayBounds(_THIS, SDL_VideoDisplay* display, SDL_Rect* rect);

static void
MIR_GetDisplayModes(_THIS, SDL_VideoDisplay* sdl_display);

static int
MIR_SetDisplayMode(_THIS, SDL_VideoDisplay* sdl_display, SDL_DisplayMode* mode);

static SDL_WindowShaper*
MIR_CreateShaper(SDL_Window* window)
{
    /* FIXME Im not sure if mir support this atm, will have to come back to this */
    return NULL;
}

static int
MIR_SetWindowShape(SDL_WindowShaper* shaper, SDL_Surface* shape, SDL_WindowShapeMode* shape_mode)
{
    return SDL_Unsupported();
}

static int
MIR_ResizeWindowShape(SDL_Window* window)
{
    return SDL_Unsupported();
}

static int
MIR_Available()
{
    int available = 0;

    if (SDL_MIR_LoadSymbols()) {
        /* !!! FIXME: try to make a MirConnection here. */
        available = 1;
        SDL_MIR_UnloadSymbols();
    }

    return available;
}

static void
MIR_DeleteDevice(SDL_VideoDevice* device)
{
    SDL_free(device);
    SDL_MIR_UnloadSymbols();
}

void
MIR_PumpEvents(_THIS)
{
}

static SDL_VideoDevice*
MIR_CreateDevice(int device_index)
{
    MIR_Data* mir_data;
    SDL_VideoDevice* device = NULL;

    if (!SDL_MIR_LoadSymbols()) {
        return NULL;
    }

    device = SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_MIR_UnloadSymbols();
        SDL_OutOfMemory();
        return NULL;
    }

    mir_data = SDL_calloc(1, sizeof(MIR_Data));
    if (!mir_data) {
        SDL_free(device);
        SDL_MIR_UnloadSymbols();
        SDL_OutOfMemory();
        return NULL;
    }

    device->driverdata = mir_data;

    /* mirvideo */
    device->VideoInit        = MIR_VideoInit;
    device->VideoQuit        = MIR_VideoQuit;
    device->GetDisplayBounds = MIR_GetDisplayBounds;
    device->GetDisplayModes  = MIR_GetDisplayModes;
    device->SetDisplayMode   = MIR_SetDisplayMode;
    device->free             = MIR_DeleteDevice;

    /* miropengles */
    device->GL_SwapWindow      = MIR_GL_SwapWindow;
    device->GL_MakeCurrent     = MIR_GL_MakeCurrent;
    device->GL_CreateContext   = MIR_GL_CreateContext;
    device->GL_DeleteContext   = MIR_GL_DeleteContext;
    device->GL_LoadLibrary     = MIR_GL_LoadLibrary;
    device->GL_UnloadLibrary   = MIR_GL_UnloadLibrary;
    device->GL_GetSwapInterval = MIR_GL_GetSwapInterval;
    device->GL_SetSwapInterval = MIR_GL_SetSwapInterval;
    device->GL_GetProcAddress  = MIR_GL_GetProcAddress;

    /* mirwindow */
    device->CreateWindow        = MIR_CreateWindow;
    device->DestroyWindow       = MIR_DestroyWindow;
    device->GetWindowWMInfo     = MIR_GetWindowWMInfo;
    device->SetWindowFullscreen = MIR_SetWindowFullscreen;
    device->MaximizeWindow      = MIR_MaximizeWindow;
    device->MinimizeWindow      = MIR_MinimizeWindow;
    device->RestoreWindow       = MIR_RestoreWindow;

    device->CreateWindowFrom     = NULL;
    device->SetWindowTitle       = NULL;
    device->SetWindowIcon        = NULL;
    device->SetWindowPosition    = NULL;
    device->SetWindowSize        = NULL;
    device->SetWindowMinimumSize = NULL;
    device->SetWindowMaximumSize = NULL;
    device->ShowWindow           = NULL;
    device->HideWindow           = NULL;
    device->RaiseWindow          = NULL;
    device->SetWindowBordered    = NULL;
    device->SetWindowGammaRamp   = NULL;
    device->GetWindowGammaRamp   = NULL;
    device->SetWindowGrab        = NULL;
    device->OnWindowEnter        = NULL;

    /* mirframebuffer */
    device->CreateWindowFramebuffer  = MIR_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer  = MIR_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = MIR_DestroyWindowFramebuffer;

    device->shape_driver.CreateShaper      = MIR_CreateShaper;
    device->shape_driver.SetWindowShape    = MIR_SetWindowShape;
    device->shape_driver.ResizeWindowShape = MIR_ResizeWindowShape;

    device->PumpEvents = MIR_PumpEvents;

    device->SuspendScreenSaver = NULL;

    device->StartTextInput   = NULL;
    device->StopTextInput    = NULL;
    device->SetTextInputRect = NULL;

    device->HasScreenKeyboardSupport = NULL;
    device->ShowScreenKeyboard       = NULL;
    device->HideScreenKeyboard       = NULL;
    device->IsScreenKeyboardShown    = NULL;

    device->SetClipboardText = NULL;
    device->GetClipboardText = NULL;
    device->HasClipboardText = NULL;

    device->ShowMessageBox = NULL;

    return device;
}

VideoBootStrap MIR_bootstrap = {
    MIR_DRIVER_NAME, "SDL Mir video driver",
    MIR_Available, MIR_CreateDevice
};

static void
MIR_SetCurrentDisplayMode(MirDisplayOutput const* out, SDL_VideoDisplay* display)
{
    SDL_DisplayMode mode = {
        .format = SDL_PIXELFORMAT_RGB888,
        .w = out->modes[out->current_mode].horizontal_resolution,
        .h = out->modes[out->current_mode].vertical_resolution,
        .refresh_rate = out->modes[out->current_mode].refresh_rate,
        .driverdata = NULL
    };

    display->desktop_mode = mode;
    display->current_mode = mode;
}

static void
MIR_AddAllModesFromDisplay(MirDisplayOutput const* out, SDL_VideoDisplay* display)
{
    int n_mode;
    for (n_mode = 0; n_mode < out->num_modes; ++n_mode) {
        SDL_DisplayMode mode = {
            .format = SDL_PIXELFORMAT_RGB888,
            .w = out->modes[n_mode].horizontal_resolution,
            .h = out->modes[n_mode].vertical_resolution,
            .refresh_rate = out->modes[n_mode].refresh_rate,
            .driverdata = NULL
        };

        SDL_AddDisplayMode(display, &mode);
    }
}

static void
MIR_InitDisplays(_THIS)
{
    MIR_Data* mir_data = _this->driverdata;
    int d;

    MirDisplayConfiguration* display_config = MIR_mir_connection_create_display_config(mir_data->connection);

    for (d = 0; d < display_config->num_outputs; d++) {
        MirDisplayOutput const* out = display_config->outputs + d;

        SDL_VideoDisplay display;
        SDL_zero(display);

        if (out->used &&
            out->connected &&
            out->num_modes &&
            out->current_mode < out->num_modes) {

            MIR_SetCurrentDisplayMode(out, &display);
            MIR_AddAllModesFromDisplay(out, &display);

            SDL_AddVideoDisplay(&display);
        }
    }

    MIR_mir_display_config_destroy(display_config);
}

int
MIR_VideoInit(_THIS)
{
    MIR_Data* mir_data = _this->driverdata;

    mir_data->connection = MIR_mir_connect_sync(NULL, __PRETTY_FUNCTION__);
    mir_data->software = SDL_FALSE;

    if (!MIR_mir_connection_is_valid(mir_data->connection))
        return SDL_SetError("Failed to connect to the Mir Server");

    MIR_InitDisplays(_this);
    MIR_InitMouse();

    return 0;
}

void
MIR_VideoQuit(_THIS)
{
    MIR_Data* mir_data = _this->driverdata;

    MIR_FiniMouse();

    MIR_GL_DeleteContext(_this, NULL);
    MIR_GL_UnloadLibrary(_this);

    MIR_mir_connection_release(mir_data->connection);

    SDL_free(mir_data);
    _this->driverdata = NULL;
}

static int
MIR_GetDisplayBounds(_THIS, SDL_VideoDisplay* display, SDL_Rect* rect)
{
    MIR_Data* mir_data = _this->driverdata;
    int d;

    MirDisplayConfiguration* display_config = MIR_mir_connection_create_display_config(mir_data->connection);

    for (d = 0; d < display_config->num_outputs; d++) {
        MirDisplayOutput const* out = display_config->outputs + d;

        if (out->used &&
            out->connected &&
            out->num_modes &&
            out->current_mode < out->num_modes) {

            rect->x = out->position_x;
            rect->y = out->position_y;
            rect->w = out->modes->horizontal_resolution;
            rect->h = out->modes->vertical_resolution;
        }
    }

    MIR_mir_display_config_destroy(display_config);

    return 0;
}

static void
MIR_GetDisplayModes(_THIS, SDL_VideoDisplay* sdl_display)
{
}

static int
MIR_SetDisplayMode(_THIS, SDL_VideoDisplay* sdl_display, SDL_DisplayMode* mode)
{
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_MIR */

/* vi: set ts=4 sw=4 expandtab: */

