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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PS3

#include "../../events/SDL_mouse_c.h"

#include <io/mouse.h>

#include "SDL_PS3mouse_c.h"

// void checkMouseConnected(_THIS) {
//     SDL_DeviceData *data =
//         (SDL_DeviceData *) _this->driverdata;

//     CellMouseInfo mouseinfo;
//     cellMouseGetInfo(&mouseinfo);

//     if (mouseinfo.status[0] == 1 && !data->_mouseConnected) // Connected
//     {
//         data->_mouseConnected = true;
//         data->_mouseButtons = 0;

//         // Old events in the queue are discarded
//         cellMouseClearBuf(0);
//     }
//     else if (mouseinfo.status[0] != 1 && data->_mouseConnected) // Disconnected
//     {
//         data->_mouseConnected = false;
//         data->_mouseButtons = 0;
//     }
// }

// void updateMouseButtons(_THIS, const CellMouseData *mouse) {
//     SDL_DeviceData *data =
//         (SDL_DeviceData *) _this->driverdata;
//     // There should only be one window
//     SDL_Window *window = _this->windows;

//     // Check left mouse button changes
//     bool oldLMB = data->_mouseButtons & 1;
//     bool newLMB = mouse->buttons & 1;
//     if (newLMB != oldLMB) {
//         SDL_SendMouseButton(window, 0, newLMB ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT);
//     }

//     // Check rigth mouse button changes
//     bool oldRMB = data->_mouseButtons & 2;
//     bool newRMB = mouse->buttons & 2;
//     if (newRMB != oldRMB) {
//         SDL_SendMouseButton(window, 0, newRMB ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_RIGHT);
//     }

//     // Check middle mouse button changes
//     bool oldMMB = data->_mouseButtons & 4;
//     bool newMMB = mouse->buttons & 4;
//     if (newMMB != oldMMB) {
//         SDL_SendMouseButton(window, 0, newMMB ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_MIDDLE);
//     }

//     data->_mouseButtons = mouse->buttons;
// }

// void updateMousePosition(_THIS, const CellMouseData *mouse) {
//     // There should only be one window
//     SDL_Window *window = _this->windows;

//     // Mouse movement is relative
//     SDL_SendMouseMotion(window, 0, 1, mouse->x_axis, mouse->y_axis);
// }

// void updateMouseWheel(_THIS, const CellMouseData *mouse) {
//     // There should only be one window
//     SDL_Window *window = _this->windows;

//     SDL_SendMouseWheel(window, 0, mouse->tilt, mouse->wheel, SDL_MOUSEWHEEL_NORMAL);
// }

void PS3_PumpMouse(void)
{
    // SDL_DeviceData *data =
    //     (SDL_DeviceData *) _this->driverdata;

    // // Check if a mouse has been connected / disconnected
    // checkMouseConnected(_this);

    // if (data->_mouseConnected)
    // {
    //     CellMouseDataList datalist;
    //     cellMouseGetDataList(0, &datalist);

    //     int i;
    //     for (i = 0; i < datalist.list_num; i++) {
    //         // Send SDL events
    //         updateMouseButtons(_this, &datalist.list[i]);
    //         updateMousePosition(_this, &datalist.list[i]);
    //         updateMouseWheel(_this, &datalist.list[i]);
    //     }
    // }
}

void PS3_InitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    // mouse->CreateCursor = Emscripten_CreateCursor;
    // mouse->ShowCursor = Emscripten_ShowCursor;
    // mouse->FreeCursor = Emscripten_FreeCursor;
    // mouse->CreateSystemCursor = Emscripten_CreateSystemCursor;
    // mouse->SetRelativeMouseMode = Emscripten_SetRelativeMouseMode;

    // SDL_DeviceData *data =
    //     (SDL_DeviceData *) _this->driverdata;

    // Support only one mouse for now.
    ioMouseInit(1);

    // data->_mouseConnected = false;
}

void PS3_QuitMouse(void)
{
    ioMouseEnd();
}

#endif // SDL_VIDEO_DRIVER_PS3

/* vi: set ts=4 sw=4 expandtab: */
