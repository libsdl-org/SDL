/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_UIKIT

#include "SDL_uikitevents.h"
#include "SDL_uikitpen.h"
#include "SDL_uikitwindow.h"

#include "../../events/SDL_pen_c.h"

SDL_PenID penId;

typedef struct UIKit_PenHandle
{
    SDL_PenID pen;
} UIKit_PenHandle;

bool UIKit_InitPen(SDL_VideoDevice *_this)
{
    return true;
}

void UIKit_HandlePenEnter()
{
    SDL_PenInfo penInfo;
    SDL_zero(penInfo);
    penInfo.capabilities = SDL_PEN_CAPABILITY_PRESSURE | SDL_PEN_CAPABILITY_ROTATION | SDL_PEN_CAPABILITY_XTILT | SDL_PEN_CAPABILITY_YTILT | SDL_PEN_CAPABILITY_TANGENTIAL_PRESSURE;
    penInfo.max_tilt = 90.0f;
    penInfo.num_buttons = 0;
    penInfo.subtype = SDL_PEN_TYPE_PENCIL;

    // probably make this better
    penId = SDL_AddPenDevice(0, [@"Apple Pencil" UTF8String], &penInfo, calloc(1, sizeof(UIKit_PenHandle)));
}

void UIKit_HandlePenHover(SDL_uikitview *view, CGPoint point)
{
    SDL_SendPenMotion(0, penId, [view getSDLWindow], point.x, point.y);
}

void UIKit_HandlePenMotion(SDL_uikitview *view, UITouch *pencil)
{
    CGPoint point = [pencil locationInView:view];
    SDL_SendPenMotion(UIKit_GetEventTimestamp([pencil timestamp]), penId, [view getSDLWindow], point.x, point.y);
    SDL_SendPenAxis(UIKit_GetEventTimestamp([pencil timestamp]), penId, [view getSDLWindow], SDL_PEN_AXIS_PRESSURE, [pencil force] / [pencil maximumPossibleForce]);
    NSLog(@"ALTITUDE: %f", [pencil altitudeAngle]);
    NSLog(@"AZIMUTH VECTOR: %@", [NSValue valueWithCGVector: [pencil azimuthUnitVectorInView:view]]);
    NSLog(@"AZIMUTH ANGLE: %f", [pencil azimuthAngleInView:view]);
    // hold it
    //    SDL_SendPenAxis(0, penId, [view getSDLWindow], SDL_PEN_AXIS_XTILT, [pencil altitudeAngle] / M_PI);
}

void UIKit_HandlePenPress(SDL_uikitview *view, UITouch *pencil)
{
    SDL_SendPenTouch(UIKit_GetEventTimestamp([pencil timestamp]), penId, [view getSDLWindow], false, true);
}

void UIKit_HandlePenRelease(SDL_uikitview *view, UITouch *pencil)
{
    SDL_SendPenTouch(UIKit_GetEventTimestamp([pencil timestamp]), penId, [view getSDLWindow], false, false);
}

void UIKit_HandlePenLeave()
{
    SDL_RemovePenDevice(0, penId);
    penId = 0;
}

void UIKit_QuitPen(SDL_VideoDevice *_this)
{
}

#endif // SDL_VIDEO_DRIVER_UIKIT
