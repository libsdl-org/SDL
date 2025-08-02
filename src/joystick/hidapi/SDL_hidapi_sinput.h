/*
  Simple DirectMedia Layer
  Copyright (C) 2025 Antheas Kapenekakis <git@antheas.dev>

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

// Subtypes define the buttons and axes available on the device to
// generate a mapping string. Everything else is dynamic.
// Xinput means all Xbox360 buttons.
// Xinput_share adds a share button, and then dual, quad add 2, 4 paddles
// to those. Click adds a touchpad click. Touchpad is not used in the name
// because not all of them have clicks. In fact, there are no controllers
// with dual touchpads w/ clicks so there is no point in adding them. Yet.
typedef enum
{
    k_eSinputControllerType_FullMapping = 0x00,
    k_eSinputControllerType_XInputOnly = 0x01,
    k_eSinputControllerType_XInputShareNone = 0x02,
    k_eSinputControllerType_XInputShareDual = 0x03,
    k_eSinputControllerType_XInputShareQuad = 0x04,
    k_eSinputControllerType_XInputShareNoneClick = 0x05,
    k_eSinputControllerType_XInputShareDualClick = 0x06,
    k_eSinputControllerType_XInputShareQuadClick = 0x07,
    k_eSinputControllerType_LoadFirmware = 0xff,
} ESinputControllerType;