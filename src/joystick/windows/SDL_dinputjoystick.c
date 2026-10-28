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

#include "../SDL_sysjoystick.h"

#ifdef SDL_JOYSTICK_DINPUT

#include "SDL_windowsjoystick_c.h"
#include "SDL_dinputjoystick_c.h"
#include "SDL_xinputjoystick_c.h"
#include "../../core/windows/SDL_gameinput.h"
#include "../hidapi/SDL_hidapijoystick_c.h"
#ifdef SDL_HAPTIC_DINPUT
#include "../../haptic/windows/SDL_dinputhaptic_c.h"
#endif

#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

#define INPUT_QSIZE        128                                                         // Buffer up to 128 input messages
#define JOY_AXIS_THRESHOLD (((SDL_JOYSTICK_AXIS_MAX) - (SDL_JOYSTICK_AXIS_MIN)) / 100) // 1% motion

#define CONVERT_MAGNITUDE(x) (((x)*10000) / 0x7FFF)

// external variables referenced.
extern HWND SDL_HelperWindow;

// local variables
static bool coinitialized = false;
static LPDIRECTINPUT8 dinput = NULL;

// The devices found by the last scan. Only used on the joystick device thread, and by Quit after it stops.
// A DirectInput device that has been checked, owned by the joystick device thread. Devices that aren't
// joysticks we use are remembered too, with no instance ID, so they aren't opened and queried on every scan.
typedef struct DINPUT_KnownDevice
{
    SDL_JoystickID instance_id; // Driver instance ID, not the public joystick ID (see SDL_JoystickDevice)
    GUID guidInstance;
    bool seen;
    struct DINPUT_KnownDevice *next;
} DINPUT_KnownDevice;

static DINPUT_KnownDevice *s_DInputKnownDevices;
static WINDOWS_JoystickRescan s_DInputRescan;

// The published devices, for adding haptic devices when haptics is initialized. App thread, joystick lock.
typedef struct DINPUT_PublishedDevice
{
    SDL_JoystickDevice *device;
    struct DINPUT_PublishedDevice *next;
} DINPUT_PublishedDevice;

static DINPUT_PublishedDevice *s_DInputPublishedDevices;

// Taken from Wine - Thanks!
static DIOBJECTDATAFORMAT dfDIJoystick2[] = {
    { &GUID_XAxis, DIJOFS_X, DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_YAxis, DIJOFS_Y, DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_ZAxis, DIJOFS_Z, DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_RxAxis, DIJOFS_RX, DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_RyAxis, DIJOFS_RY, DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_RzAxis, DIJOFS_RZ, DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_Slider, DIJOFS_SLIDER(0), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_Slider, DIJOFS_SLIDER(1), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
    { &GUID_POV, DIJOFS_POV(0), DIDFT_OPTIONAL | DIDFT_POV | DIDFT_ANYINSTANCE, 0 },
    { &GUID_POV, DIJOFS_POV(1), DIDFT_OPTIONAL | DIDFT_POV | DIDFT_ANYINSTANCE, 0 },
    { &GUID_POV, DIJOFS_POV(2), DIDFT_OPTIONAL | DIDFT_POV | DIDFT_ANYINSTANCE, 0 },
    { &GUID_POV, DIJOFS_POV(3), DIDFT_OPTIONAL | DIDFT_POV | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(0), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(1), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(2), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(3), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(4), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(5), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(6), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(7), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(8), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(9), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(10), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(11), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(12), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(13), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(14), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(15), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(16), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(17), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(18), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(19), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(20), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(21), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(22), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(23), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(24), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(25), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(26), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(27), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(28), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(29), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(30), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(31), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(32), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(33), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(34), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(35), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(36), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(37), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(38), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(39), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(40), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(41), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(42), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(43), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(44), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(45), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(46), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(47), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(48), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(49), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(50), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(51), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(52), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(53), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(54), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(55), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(56), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(57), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(58), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(59), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(60), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(61), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(62), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(63), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(64), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(65), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(66), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(67), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(68), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(69), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(70), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(71), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(72), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(73), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(74), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(75), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(76), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(77), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(78), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(79), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(80), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(81), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(82), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(83), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(84), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(85), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(86), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(87), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(88), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(89), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(90), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(91), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(92), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(93), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(94), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(95), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(96), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(97), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(98), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(99), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(100), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(101), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(102), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(103), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(104), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(105), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(106), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(107), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(108), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(109), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(110), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(111), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(112), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(113), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(114), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(115), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(116), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(117), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(118), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(119), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(120), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(121), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(122), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(123), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(124), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(125), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(126), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { NULL, DIJOFS_BUTTON(127), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { &GUID_XAxis, FIELD_OFFSET(DIJOYSTATE2, lVX), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_YAxis, FIELD_OFFSET(DIJOYSTATE2, lVY), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_ZAxis, FIELD_OFFSET(DIJOYSTATE2, lVZ), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_RxAxis, FIELD_OFFSET(DIJOYSTATE2, lVRx), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_RyAxis, FIELD_OFFSET(DIJOYSTATE2, lVRy), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_RzAxis, FIELD_OFFSET(DIJOYSTATE2, lVRz), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    // note: dwOfs value matches Windows
    { &GUID_Slider, DIJOFS_SLIDER(0), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_Slider, DIJOFS_SLIDER(1), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTVELOCITY },
    { &GUID_XAxis, FIELD_OFFSET(DIJOYSTATE2, lAX), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_YAxis, FIELD_OFFSET(DIJOYSTATE2, lAY), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_ZAxis, FIELD_OFFSET(DIJOYSTATE2, lAZ), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_RxAxis, FIELD_OFFSET(DIJOYSTATE2, lARx), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_RyAxis, FIELD_OFFSET(DIJOYSTATE2, lARy), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_RzAxis, FIELD_OFFSET(DIJOYSTATE2, lARz), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    // note: dwOfs value matches Windows
    { &GUID_Slider, DIJOFS_SLIDER(0), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_Slider, DIJOFS_SLIDER(1), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTACCEL },
    { &GUID_XAxis, FIELD_OFFSET(DIJOYSTATE2, lFX), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    { &GUID_YAxis, FIELD_OFFSET(DIJOYSTATE2, lFY), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    { &GUID_ZAxis, FIELD_OFFSET(DIJOYSTATE2, lFZ), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    { &GUID_RxAxis, FIELD_OFFSET(DIJOYSTATE2, lFRx), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    { &GUID_RyAxis, FIELD_OFFSET(DIJOYSTATE2, lFRy), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    { &GUID_RzAxis, FIELD_OFFSET(DIJOYSTATE2, lFRz), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    // note: dwOfs value matches Windows
    { &GUID_Slider, DIJOFS_SLIDER(0), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
    { &GUID_Slider, DIJOFS_SLIDER(1), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, DIDOI_ASPECTFORCE },
};

const DIDATAFORMAT SDL_c_dfDIJoystick2 = {
    sizeof(DIDATAFORMAT),
    sizeof(DIOBJECTDATAFORMAT),
    DIDF_ABSAXIS,
    sizeof(DIJOYSTATE2),
    SDL_arraysize(dfDIJoystick2),
    dfDIJoystick2
};

// Convert a DirectInput return code to a text message
static bool SetDIerror(const char *function, HRESULT code)
{
    return SDL_SetError("%s() DirectX error 0x%8.8" SDL_PRIxSLONG, function, code);
}

static bool SDL_IsXInputDevice(Uint16 vendor_id, Uint16 product_id, const char *hidPath)
{
#if defined(SDL_JOYSTICK_XINPUT) || defined(SDL_JOYSTICK_GAMEINPUT)
    SDL_GamepadType type;

    // Some other backends will pick up XInput-compatible devices
    if (!SDL_XINPUT_Enabled() &&
        !SDL_UsingGameInputForXInputControllers()) {
        return false;
    }

    // If device path contains "IG_" then its an XInput device
    // See: https://docs.microsoft.com/windows/win32/xinput/xinput-and-directinput
    if (SDL_strstr(hidPath, "IG_") != NULL) {
        return true;
    }

    type = SDL_GetGamepadTypeFromVIDPID(vendor_id, product_id, NULL, false);
    if (type == SDL_GAMEPAD_TYPE_XBOX360 ||
        type == SDL_GAMEPAD_TYPE_XBOXONE ||
        (vendor_id == USB_VENDOR_VALVE && product_id == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD)) {
        return true;
    }
#endif // SDL_JOYSTICK_XINPUT || SDL_JOYSTICK_GAMEINPUT

    return false;
}

// State for one DirectInput scan
typedef struct DINPUT_ScanContext
{
    LPDIRECTINPUT8 dinput; // The scan's own DirectInput object, in the device thread's apartment
} DINPUT_ScanContext;

static bool QueryDeviceName(LPDIRECTINPUTDEVICE8 device, char **product_string)
{
    DIPROPSTRING dipstr;

    if (!device || !product_string) {
        return false;
    }

    dipstr.diph.dwSize = sizeof(dipstr);
    dipstr.diph.dwHeaderSize = sizeof(dipstr.diph);
    dipstr.diph.dwObj = 0;
    dipstr.diph.dwHow = DIPH_DEVICE;

    if (FAILED(IDirectInputDevice8_GetProperty(device, DIPROP_PRODUCTNAME, &dipstr.diph))) {
        return false;
    }

    *product_string = WIN_StringToUTF8W(dipstr.wsz);

    return true;
}

static bool QueryDevicePath(LPDIRECTINPUTDEVICE8 device, char **device_path)
{
    DIPROPGUIDANDPATH dippath;

    if (!device || !device_path) {
        return false;
    }

    dippath.diph.dwSize = sizeof(dippath);
    dippath.diph.dwHeaderSize = sizeof(dippath.diph);
    dippath.diph.dwObj = 0;
    dippath.diph.dwHow = DIPH_DEVICE;

    if (FAILED(IDirectInputDevice8_GetProperty(device, DIPROP_GUIDANDPATH, &dippath.diph))) {
        return false;
    }

    *device_path = WIN_StringToUTF8W(dippath.wszPath);

    // Normalize path to upper case.
    SDL_strupr(*device_path);

    return true;
}

static bool QueryDeviceInfo(LPDIRECTINPUTDEVICE8 device, Uint16 *vendor_id, Uint16 *product_id)
{
    DIPROPDWORD dipdw;

    if (!device || !vendor_id || !product_id) {
        return false;
    }

    dipdw.diph.dwSize = sizeof(dipdw);
    dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = 0;

    if (FAILED(IDirectInputDevice8_GetProperty(device, DIPROP_VIDPID, &dipdw.diph))) {
        return false;
    }

    *vendor_id = LOWORD(dipdw.dwData);
    *product_id = HIWORD(dipdw.dwData);

    return true;
}

void FreeRumbleEffectData(DIEFFECT *effect)
{
    if (!effect) {
        return;
    }
    SDL_free(effect->rgdwAxes);
    SDL_free(effect->rglDirection);
    SDL_free(effect->lpvTypeSpecificParams);
    SDL_free(effect);
}

DIEFFECT *CreateRumbleEffectData(Sint16 magnitude)
{
    DIEFFECT *effect;
    DIPERIODIC *periodic;

    // Create the effect
    effect = (DIEFFECT *)SDL_calloc(1, sizeof(*effect));
    if (!effect) {
        return NULL;
    }
    effect->dwSize = sizeof(*effect);
    effect->dwGain = 10000;
    effect->dwFlags = DIEFF_OBJECTOFFSETS;
    effect->dwDuration = SDL_MAX_RUMBLE_DURATION_MS * 1000; // In microseconds.
    effect->dwTriggerButton = DIEB_NOTRIGGER;

    effect->cAxes = 2;
    effect->rgdwAxes = (DWORD *)SDL_calloc(effect->cAxes, sizeof(DWORD));
    if (!effect->rgdwAxes) {
        FreeRumbleEffectData(effect);
        return NULL;
    }

    effect->rglDirection = (LONG *)SDL_calloc(effect->cAxes, sizeof(LONG));
    if (!effect->rglDirection) {
        FreeRumbleEffectData(effect);
        return NULL;
    }
    effect->dwFlags |= DIEFF_CARTESIAN;

    periodic = (DIPERIODIC *)SDL_calloc(1, sizeof(*periodic));
    if (!periodic) {
        FreeRumbleEffectData(effect);
        return NULL;
    }
    periodic->dwMagnitude = CONVERT_MAGNITUDE(magnitude);
    periodic->dwPeriod = 1000000;

    effect->cbTypeSpecificParams = sizeof(*periodic);
    effect->lpvTypeSpecificParams = periodic;

    return effect;
}

// Create and initialize a DirectInput object, on a thread with COM initialized
static bool DINPUT_CreateDirectInput(LPDIRECTINPUT8 *result_dinput)
{
    LPDIRECTINPUT8 new_dinput = NULL;
    HRESULT result;
    HINSTANCE instance;

    *result_dinput = NULL;

    result = CoCreateInstance(&CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IDirectInput8, (LPVOID *)&new_dinput);

    if (FAILED(result)) {
        return SetDIerror("CoCreateInstance", result);
    }

    // Because we used CoCreateInstance, we need to Initialize it, first.
    instance = GetModuleHandle(NULL);
    if (!instance) {
        IDirectInput8_Release(new_dinput);
        return SDL_SetError("GetModuleHandle() failed with error code %" SDL_PRIuULONG ".", GetLastError());
    }
    result = IDirectInput8_Initialize(new_dinput, instance, DIRECTINPUT_VERSION);

    if (FAILED(result)) {
        IDirectInput8_Release(new_dinput);
        return SetDIerror("IDirectInput::Initialize", result);
    }

    *result_dinput = new_dinput;
    return true;
}

static bool DINPUT_JoystickPreInit(void)
{
    HRESULT result;

    WINDOWS_InitJoystickDeviceNotification();

    if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_DIRECTINPUT, true)) {
        // In some environments, IDirectInput8_Initialize / _EnumDevices can take a minute even with no controllers.
        dinput = NULL;
        return true;
    }

    result = WIN_CoInitialize();
    if (FAILED(result)) {
        return SetDIerror("CoInitialize", result);
    }

    coinitialized = true;

    if (!DINPUT_CreateDirectInput(&dinput)) {
        return false;
    }
    return true;
}

static bool DINPUT_JoystickInit(void)
{
    return true;
}

static int GetSteamVirtualGamepadSlot(Uint16 vendor_id, Uint16 product_id, const char *device_path)
{
    int slot = -1;

    if (vendor_id == USB_VENDOR_VALVE &&
        product_id == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD) {
        (void)SDL_sscanf(device_path, "\\\\?\\HID#VID_28DE&PID_11FF&IG_0%d", &slot);
    }
    return slot;
}

static DINPUT_KnownDevice *DINPUT_FindKnownDevice(const GUID *guidInstance)
{
    DINPUT_KnownDevice *known;

    for (known = s_DInputKnownDevices; known; known = known->next) {
        if (SDL_memcmp(&known->guidInstance, guidInstance, sizeof(*guidInstance)) == 0) {
            return known;
        }
    }
    return NULL;
}

static DINPUT_KnownDevice *DINPUT_CreateKnownDevice(const GUID *guidInstance)
{
    DINPUT_KnownDevice *known = (DINPUT_KnownDevice *)SDL_calloc(1, sizeof(*known));
    if (!known) {
        return NULL;
    }
    known->guidInstance = *guidInstance;
    known->seen = true;
    known->next = s_DInputKnownDevices;
    s_DInputKnownDevices = known;
    return known;
}

static void DINPUT_RemoveKnownDevice(DINPUT_KnownDevice *device)
{
    DINPUT_KnownDevice *known, *prev;

    for (known = s_DInputKnownDevices, prev = NULL; known; prev = known, known = known->next) {
        if (known == device) {
            if (prev) {
                prev->next = known->next;
            } else {
                s_DInputKnownDevices = known->next;
            }
            break;
        }
    }

    if (device->instance_id) {
        SDL_ReportJoystickRemoved(device->instance_id);
    }
    SDL_free(device);
}

static void DINPUT_AddKnownDevice(LPCDIDEVICEINSTANCE pDeviceInstance, const char *hidPath, Uint16 vendor, Uint16 product, Uint16 version, const char *product_string)
{
    DINPUT_KnownDevice *known;
    DIDEVICEINSTANCE *data;
    SDL_JoystickDevice *device;
    char *name;
    SDL_GUID guid;

    known = DINPUT_CreateKnownDevice(&pDeviceInstance->guidInstance);
    if (!known) {
        return;
    }

    // If anything here fails, the device is forgotten again so the next scan tries again
    name = SDL_CreateJoystickName(vendor, product, NULL, product_string);
    if (!name) {
        DINPUT_RemoveKnownDevice(known);
        return;
    }

    if (vendor && product) {
        guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, vendor, product, version, NULL, product_string, 0, 0);
    } else {
        guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_BLUETOOTH, vendor, product, version, NULL, product_string, 0, 0);
    }

    data = (DIDEVICEINSTANCE *)SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_free(name);
        DINPUT_RemoveKnownDevice(known);
        return;
    }
    SDL_memcpy(data, pDeviceInstance, sizeof(*data));

    device = SDL_CreateJoystickDevice(&SDL_DINPUT_JoystickDriver);
    if (!device) {
        SDL_free(data);
        SDL_free(name);
        DINPUT_RemoveKnownDevice(known);
        return;
    }
    device->driver_data = data;
    SDL_SetJoystickDeviceDescription(device, name, hidPath, guid, GetSteamVirtualGamepadSlot(vendor, product, hidPath));
    SDL_free(name);
    device->vendor_id = vendor;
    device->product_id = product;
    device->type = SDL_GetGamepadTypeFromVIDPID(vendor, product, device->name, false);
    WIN_GetDeviceInterfaceContainerID(hidPath, &device->container_id);

    known->instance_id = device->driver_instance_id;
    SDL_ReportJoystickDevice(device);
}

// helper function for direct input, gets called for each connected joystick
static BOOL CALLBACK EnumJoystickDetectCallback(LPCDIDEVICEINSTANCE pDeviceInstance, LPVOID pContext)
{
#define CHECK(expression)  \
    {                      \
        if (!(expression)) \
            goto err;      \
    }
    DINPUT_KnownDevice *known;
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint16 version = 0;
    char *hidPath = NULL;
    char *product_string = NULL;
    LPDIRECTINPUTDEVICE8 device = NULL;

    if (SDL_JoystickDiscoveryQuitting()) {
        return DIENUM_STOP;
    }

    // Devices that have already been checked are skipped, without opening them
    known = DINPUT_FindKnownDevice(&pDeviceInstance->guidInstance);
    if (known) {
        known->seen = true;
        return DIENUM_CONTINUE;
    }

    // We are only supporting HID devices.
    if (!(pDeviceInstance->dwDevType & DIDEVTYPE_HID)) {
        DINPUT_CreateKnownDevice(&pDeviceInstance->guidInstance);
        return DIENUM_CONTINUE;
    }

    // If these fail, the device is checked again on the next scan
    CHECK(SUCCEEDED(IDirectInput8_CreateDevice(((DINPUT_ScanContext *)pContext)->dinput, &pDeviceInstance->guidInstance, &device, NULL)));
    CHECK(QueryDevicePath(device, &hidPath));
    CHECK(QueryDeviceInfo(device, &vendor, &product));
    CHECK(QueryDeviceName(device, &product_string));

    if (SDL_IsXInputDevice(vendor, product, hidPath) ||
        SDL_ShouldIgnoreJoystick(vendor, product, version, product_string)) {
        // Handled by another driver, or ignored, so remember it and don't check it again
        DINPUT_CreateKnownDevice(&pDeviceInstance->guidInstance);
        goto err;
    }

    DINPUT_AddKnownDevice(pDeviceInstance, hidPath, vendor, product, version, product_string);

err:
    SDL_free(hidPath);
    SDL_free(product_string);

    if (device) {
        IDirectInputDevice8_Release(device);
    }

    return DIENUM_CONTINUE; // get next device, please
#undef CHECK
}

static Sint32 DINPUT_JoystickDiscover(void)
{
    DINPUT_KnownDevice *known, *next;
    DINPUT_ScanContext context;
    Sint32 timeout;
    HRESULT coinitialized_here;

    if (!dinput) {
        return -1;
    }

    // EnumDevices is slow, so only look after a burst of device changes has settled, not as each change
    // arrives, see SDL_windowsjoystick.c. Without device notifications (Windows 7), poll every 3 seconds.
    if (!WINDOWS_JoystickRescanNeeded(&s_DInputRescan, false, 3000, &timeout)) {
        return timeout;
    }

    // This runs on the joystick device thread, which needs COM initialized too. It uses its own DirectInput
    // object, rather than the one the app thread uses for opened joysticks, which belongs to another apartment.
    coinitialized_here = WIN_CoInitialize();

    SDL_zero(context);
    if (!DINPUT_CreateDirectInput(&context.dinput)) {
        if (SUCCEEDED(coinitialized_here)) {
            WIN_CoUninitialize();
        }
        return timeout;
    }

    for (known = s_DInputKnownDevices; known; known = known->next) {
        known->seen = false;
    }

    IDirectInput8_EnumDevices(context.dinput, DI8DEVCLASS_GAMECTRL, EnumJoystickDetectCallback, &context, DIEDFL_ATTACHEDONLY);
    IDirectInput8_Release(context.dinput);

    if (!SDL_JoystickDiscoveryQuitting()) {
        for (known = s_DInputKnownDevices; known; known = next) {
            next = known->next;
            if (!known->seen) {
                DINPUT_RemoveKnownDevice(known);
            }
        }
    }

    if (SUCCEEDED(coinitialized_here)) {
        WIN_CoUninitialize();
    }
    return timeout;
}

static void DINPUT_JoystickDevicePublished(SDL_JoystickDevice *device, bool published)
{
    DINPUT_PublishedDevice *entry, *prev;

    SDL_AssertJoysticksLocked();

    if (published) {
        entry = (DINPUT_PublishedDevice *)SDL_calloc(1, sizeof(*entry));
        if (entry) {
            entry->device = device;
            entry->next = s_DInputPublishedDevices;
            s_DInputPublishedDevices = entry;
        }
#ifdef SDL_HAPTIC_DINPUT
        SDL_DINPUT_HapticMaybeAddDevice((const DIDEVICEINSTANCE *)device->driver_data);
#endif
    } else {
        for (entry = s_DInputPublishedDevices, prev = NULL; entry; prev = entry, entry = entry->next) {
            if (entry->device == device) {
                if (prev) {
                    prev->next = entry->next;
                } else {
                    s_DInputPublishedDevices = entry->next;
                }
                SDL_free(entry);
                break;
            }
        }
#ifdef SDL_HAPTIC_DINPUT
        SDL_DINPUT_HapticMaybeRemoveDevice((const DIDEVICEINSTANCE *)device->driver_data);
#endif
    }
}

void SDL_DINPUT_JoystickAddHapticDevices(void)
{
#ifdef SDL_HAPTIC_DINPUT
    DINPUT_PublishedDevice *entry;

    SDL_LockJoysticks();
    for (entry = s_DInputPublishedDevices; entry; entry = entry->next) {
        SDL_DINPUT_HapticMaybeAddDevice((const DIDEVICEINSTANCE *)entry->device->driver_data);
    }
    SDL_UnlockJoysticks();
#endif
}

static BOOL CALLBACK EnumDevObjectsCallback(LPCDIDEVICEOBJECTINSTANCE pDeviceObject, LPVOID pContext)
{
    SDL_Joystick *joystick = (SDL_Joystick *)pContext;
    HRESULT result;
    input_t *in = &joystick->hwdata->Inputs[joystick->hwdata->NumInputs];

    if (pDeviceObject->dwType & DIDFT_BUTTON) {
        in->type = BUTTON;
        in->num = (Uint8)joystick->nbuttons;
        in->ofs = DIJOFS_BUTTON(in->num);
        joystick->nbuttons++;
    } else if (pDeviceObject->dwType & DIDFT_POV) {
        // DIJOYSTATE2.rgdwPOV only has room for 4 POVs, ignore any beyond that.
        if (joystick->nhats >= 4) {
            return DIENUM_CONTINUE;
        }
        in->type = HAT;
        in->num = (Uint8)joystick->nhats;
        in->ofs = DIJOFS_POV(in->num);
        joystick->nhats++;
    } else if (pDeviceObject->dwType & DIDFT_AXIS) {
        DIPROPRANGE diprg;
        DIPROPDWORD dilong;

        in->type = AXIS;
        in->num = (Uint8)joystick->naxes;
        if (SDL_memcmp(&pDeviceObject->guidType, &GUID_XAxis, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_X;
        } else if (SDL_memcmp(&pDeviceObject->guidType, &GUID_YAxis, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_Y;
        } else if (SDL_memcmp(&pDeviceObject->guidType, &GUID_ZAxis, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_Z;
        } else if (SDL_memcmp(&pDeviceObject->guidType, &GUID_RxAxis, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_RX;
        } else if (SDL_memcmp(&pDeviceObject->guidType, &GUID_RyAxis, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_RY;
        } else if (SDL_memcmp(&pDeviceObject->guidType, &GUID_RzAxis, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_RZ;
        } else if (SDL_memcmp(&pDeviceObject->guidType, &GUID_Slider, sizeof(pDeviceObject->guidType)) == 0) {
            in->ofs = DIJOFS_SLIDER(joystick->hwdata->NumSliders);
            ++joystick->hwdata->NumSliders;
        } else {
            return DIENUM_CONTINUE; // not an axis we can grok
        }

        diprg.diph.dwSize = sizeof(diprg);
        diprg.diph.dwHeaderSize = sizeof(diprg.diph);
        diprg.diph.dwObj = pDeviceObject->dwType;
        diprg.diph.dwHow = DIPH_BYID;
        diprg.lMin = SDL_JOYSTICK_AXIS_MIN;
        diprg.lMax = SDL_JOYSTICK_AXIS_MAX;

        result =
            IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                            DIPROP_RANGE, &diprg.diph);
        if (FAILED(result)) {
            return DIENUM_CONTINUE; // don't use this axis
        }

        // Set dead zone to 0.
        dilong.diph.dwSize = sizeof(dilong);
        dilong.diph.dwHeaderSize = sizeof(dilong.diph);
        dilong.diph.dwObj = pDeviceObject->dwType;
        dilong.diph.dwHow = DIPH_BYID;
        dilong.dwData = 0;
        result =
            IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                            DIPROP_DEADZONE, &dilong.diph);
        if (FAILED(result)) {
            return DIENUM_CONTINUE; // don't use this axis
        }

        joystick->naxes++;
    } else {
        // not supported at this time
        return DIENUM_CONTINUE;
    }

    joystick->hwdata->NumInputs++;

    if (joystick->hwdata->NumInputs == MAX_INPUTS) {
        return DIENUM_STOP; // too many
    }

    return DIENUM_CONTINUE;
}

/* Sort using the data offset into the DInput struct.
 * This gives a reasonable ordering for the inputs.
 */
static int SDLCALL SortDevFunc(const void *a, const void *b)
{
    const input_t *inputA = (const input_t *)a;
    const input_t *inputB = (const input_t *)b;

    if (inputA->ofs < inputB->ofs) {
        return -1;
    }
    if (inputA->ofs > inputB->ofs) {
        return 1;
    }
    return 0;
}

// Sort the input objects and recalculate the indices for each input.
static void SortDevObjects(SDL_Joystick *joystick)
{
    input_t *inputs = joystick->hwdata->Inputs;
    Uint8 nButtons = 0;
    Uint8 nHats = 0;
    Uint8 nAxis = 0;
    int n;

    SDL_qsort(inputs, joystick->hwdata->NumInputs, sizeof(input_t), SortDevFunc);

    for (n = 0; n < joystick->hwdata->NumInputs; n++) {
        switch (inputs[n].type) {
        case BUTTON:
            inputs[n].num = nButtons;
            nButtons++;
            break;

        case HAT:
            inputs[n].num = nHats;
            nHats++;
            break;

        case AXIS:
            inputs[n].num = nAxis;
            nAxis++;
            break;
        }
    }
}

static bool DINPUT_JoystickOpenDevice(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    const DIDEVICEINSTANCE *dxdevice = (const DIDEVICEINSTANCE *)device->driver_data;
    HRESULT result;
    DIPROPDWORD dipdw;

    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(struct joystick_hwdata));
    if (!joystick->hwdata) {
        return false;
    }
    joystick->hwdata->guid = device->guid;

    joystick->hwdata->buffered = true;
    joystick->hwdata->Capabilities.dwSize = sizeof(DIDEVCAPS);

    SDL_zero(dipdw);
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);

    result =
        IDirectInput8_CreateDevice(dinput,
                                   &dxdevice->guidInstance,
                                   &joystick->hwdata->InputDevice,
                                   NULL);
    if (FAILED(result)) {
        return SetDIerror("IDirectInput::CreateDevice", result);
    }

    /* Acquire shared access. Exclusive access is required for forces,
     * though. */
    result =
        IDirectInputDevice8_SetCooperativeLevel(joystick->hwdata->InputDevice, SDL_HelperWindow,
                                                DISCL_EXCLUSIVE |
                                                    DISCL_BACKGROUND);
    if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SetCooperativeLevel", result);
    }

    // Use the extended data structure: DIJOYSTATE2.
    result =
        IDirectInputDevice8_SetDataFormat(joystick->hwdata->InputDevice,
                                          &SDL_c_dfDIJoystick2);
    if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SetDataFormat", result);
    }

    if (!WIN_HasBrokenEZFRD64DLL()) {
        // Get device capabilities to see if we are force feedback capable
        result =
            IDirectInputDevice8_GetCapabilities(joystick->hwdata->InputDevice,
                                                &joystick->hwdata->Capabilities);
        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::GetCapabilities", result);
        }
    }

    // Force capable?
    if (joystick->hwdata->Capabilities.dwFlags & DIDC_FORCEFEEDBACK) {
        result = IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::Acquire", result);
        }

        // reset all actuators.
        result =
            IDirectInputDevice8_SendForceFeedbackCommand(joystick->hwdata->InputDevice,
                                                         DISFFC_RESET);

        /* Not necessarily supported, ignore if not supported.
        if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SendForceFeedbackCommand", result);
        }
        */

        result = IDirectInputDevice8_Unacquire(joystick->hwdata->InputDevice);

        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::Unacquire", result);
        }

        /* Turn on auto-centering for a ForceFeedback device (until told
         * otherwise). */
        dipdw.diph.dwObj = 0;
        dipdw.diph.dwHow = DIPH_DEVICE;
        dipdw.dwData = DIPROPAUTOCENTER_ON;

        result =
            IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                            DIPROP_AUTOCENTER, &dipdw.diph);

        /* Not necessarily supported, ignore if not supported.
        if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SetProperty", result);
        }
        */

        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);
    }

    // What buttons and axes does it have?
    IDirectInputDevice8_EnumObjects(joystick->hwdata->InputDevice,
                                    EnumDevObjectsCallback, joystick,
                                    DIDFT_BUTTON | DIDFT_AXIS | DIDFT_POV);

    /* Reorder the input objects. Some devices do not report the X axis as
     * the first axis, for example. */
    SortDevObjects(joystick);

    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = INPUT_QSIZE;

    // Set the buffer size
    result =
        IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                        DIPROP_BUFFERSIZE, &dipdw.diph);

    if (result == DI_POLLEDDEVICE) {
        /* This device doesn't support buffering, so we're forced
         * to use less reliable polling. */
        joystick->hwdata->buffered = false;
    } else if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SetProperty", result);
    }
    joystick->hwdata->first_update = true;

    // Poll and wait for initial device state to be populated
    result = IDirectInputDevice8_Poll(joystick->hwdata->InputDevice);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        IDirectInputDevice8_Poll(joystick->hwdata->InputDevice);
    }
    SDL_Delay(50);

    return true;
}

static bool SDL_DINPUT_JoystickInitRumble(SDL_Joystick *joystick, Sint16 magnitude)
{
    HRESULT result;

    // Reset and then enable actuators
    result = IDirectInputDevice8_SendForceFeedbackCommand(joystick->hwdata->InputDevice, DISFFC_RESET);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTEXCLUSIVEACQUIRED) {
        result = IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        if (SUCCEEDED(result)) {
            result = IDirectInputDevice8_SendForceFeedbackCommand(joystick->hwdata->InputDevice, DISFFC_RESET);
        }
    }
    if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SendForceFeedbackCommand(DISFFC_RESET)", result);
    }

    result = IDirectInputDevice8_SendForceFeedbackCommand(joystick->hwdata->InputDevice, DISFFC_SETACTUATORSON);
    if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::SendForceFeedbackCommand(DISFFC_SETACTUATORSON)", result);
    }

    // Create the effect
    joystick->hwdata->ffeffect = CreateRumbleEffectData(magnitude);
    if (!joystick->hwdata->ffeffect) {
        return false;
    }

    result = IDirectInputDevice8_CreateEffect(joystick->hwdata->InputDevice, &GUID_Sine,
                                              joystick->hwdata->ffeffect, &joystick->hwdata->ffeffect_ref, NULL);
    if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::CreateEffect", result);
    }
    return true;
}

static bool DINPUT_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    HRESULT result;

    // Scale and average the two rumble strengths
    Sint16 magnitude = (Sint16)(((low_frequency_rumble / 2) + (high_frequency_rumble / 2)) / 2);

    if (!(joystick->hwdata->Capabilities.dwFlags & DIDC_FORCEFEEDBACK)) {
        return SDL_Unsupported();
    }

    if (joystick->hwdata->ff_initialized) {
        DIPERIODIC *periodic = ((DIPERIODIC *)joystick->hwdata->ffeffect->lpvTypeSpecificParams);
        periodic->dwMagnitude = CONVERT_MAGNITUDE(magnitude);

        result = IDirectInputEffect_SetParameters(joystick->hwdata->ffeffect_ref, joystick->hwdata->ffeffect, (DIEP_DURATION | DIEP_TYPESPECIFICPARAMS));
        if (result == DIERR_INPUTLOST) {
            result = IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
            if (SUCCEEDED(result)) {
                result = IDirectInputEffect_SetParameters(joystick->hwdata->ffeffect_ref, joystick->hwdata->ffeffect, (DIEP_DURATION | DIEP_TYPESPECIFICPARAMS));
            }
        }
        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::SetParameters", result);
        }
    } else {
        if (!SDL_DINPUT_JoystickInitRumble(joystick, magnitude)) {
            return false;
        }
        joystick->hwdata->ff_initialized = true;
    }

    result = IDirectInputEffect_Start(joystick->hwdata->ffeffect_ref, 1, 0);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTEXCLUSIVEACQUIRED) {
        result = IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        if (SUCCEEDED(result)) {
            result = IDirectInputEffect_Start(joystick->hwdata->ffeffect_ref, 1, 0);
        }
    }
    if (FAILED(result)) {
        return SetDIerror("IDirectInputDevice8::Start", result);
    }
    return true;
}

static Uint8 TranslatePOV(DWORD value)
{
    const Uint8 HAT_VALS[] = {
        SDL_HAT_UP,
        SDL_HAT_UP | SDL_HAT_RIGHT,
        SDL_HAT_RIGHT,
        SDL_HAT_DOWN | SDL_HAT_RIGHT,
        SDL_HAT_DOWN,
        SDL_HAT_DOWN | SDL_HAT_LEFT,
        SDL_HAT_LEFT,
        SDL_HAT_UP | SDL_HAT_LEFT
    };

    if (LOWORD(value) == 0xFFFF) {
        return SDL_HAT_CENTERED;
    }

    // Round the value up:
    value += 4500 / 2;
    value %= 36000;
    value /= 4500;

    if (value >= 8) {
        return SDL_HAT_CENTERED; // shouldn't happen
    }

    return HAT_VALS[value];
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
static void UpdateDINPUTJoystickState_Polled(SDL_Joystick *joystick)
{
    DIJOYSTATE2 state;
    HRESULT result;
    int i;
    Uint64 timestamp = SDL_GetTicksNS();

    result =
        IDirectInputDevice8_GetDeviceState(joystick->hwdata->InputDevice,
                                           sizeof(DIJOYSTATE2), &state);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        result =
            IDirectInputDevice8_GetDeviceState(joystick->hwdata->InputDevice,
                                               sizeof(DIJOYSTATE2), &state);
    }

    if (result != DI_OK) {
        return;
    }

    // Set each known axis, button and POV.
    for (i = 0; i < joystick->hwdata->NumInputs; ++i) {
        const input_t *in = &joystick->hwdata->Inputs[i];

        switch (in->type) {
        case AXIS:
            switch (in->ofs) {
            case DIJOFS_X:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.lX);
                break;
            case DIJOFS_Y:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.lY);
                break;
            case DIJOFS_Z:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.lZ);
                break;
            case DIJOFS_RX:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.lRx);
                break;
            case DIJOFS_RY:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.lRy);
                break;
            case DIJOFS_RZ:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.lRz);
                break;
            case DIJOFS_SLIDER(0):
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.rglSlider[0]);
                break;
            case DIJOFS_SLIDER(1):
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)state.rglSlider[1]);
                break;
            }
            break;

        case BUTTON:
            SDL_SendJoystickButton(timestamp, joystick, in->num,
                                      (state.rgbButtons[in->ofs - DIJOFS_BUTTON0] != 0));
            break;
        case HAT:
        {
            Uint8 pos = TranslatePOV(state.rgdwPOV[in->num]);
            SDL_SendJoystickHat(timestamp, joystick, in->num, pos);
            break;
        }
        }
    }
}

static void UpdateDINPUTJoystickState_Buffered(SDL_Joystick *joystick)
{
    int i;
    HRESULT result;
    DWORD numevents;
    DIDEVICEOBJECTDATA evtbuf[INPUT_QSIZE];
    Uint64 timestamp = SDL_GetTicksNS();

    numevents = INPUT_QSIZE;
    result =
        IDirectInputDevice8_GetDeviceData(joystick->hwdata->InputDevice,
                                          sizeof(DIDEVICEOBJECTDATA), evtbuf,
                                          &numevents, 0);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        result =
            IDirectInputDevice8_GetDeviceData(joystick->hwdata->InputDevice,
                                              sizeof(DIDEVICEOBJECTDATA),
                                              evtbuf, &numevents, 0);
    }

    // Handle the events or punt
    if (FAILED(result)) {
        return;
    }

    for (i = 0; i < (int)numevents; ++i) {
        int j;

        for (j = 0; j < joystick->hwdata->NumInputs; ++j) {
            const input_t *in = &joystick->hwdata->Inputs[j];

            if (evtbuf[i].dwOfs != in->ofs) {
                continue;
            }

            switch (in->type) {
            case AXIS:
                SDL_SendJoystickAxis(timestamp, joystick, in->num, (Sint16)evtbuf[i].dwData);
                break;
            case BUTTON:
                SDL_SendJoystickButton(timestamp, joystick, in->num,
                                          (evtbuf[i].dwData != 0));
                break;
            case HAT:
            {
                Uint8 pos = TranslatePOV(evtbuf[i].dwData);
                SDL_SendJoystickHat(timestamp, joystick, in->num, pos);
            } break;
            }
        }
    }

    if (result == DI_BUFFEROVERFLOW) {
        /* Our buffer wasn't big enough to hold all the queued events,
         * so poll the device to make sure we have the complete state.
         */
        UpdateDINPUTJoystickState_Polled(joystick);
    }
}

static void DINPUT_JoystickUpdate(SDL_Joystick *joystick)
{
    HRESULT result;

    if (!joystick->hwdata) {
        return;
    }

    result = IDirectInputDevice8_Poll(joystick->hwdata->InputDevice);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        IDirectInputDevice8_Poll(joystick->hwdata->InputDevice);
    }

    if (joystick->hwdata->first_update) {
        // Poll to get the initial state of the joystick
        UpdateDINPUTJoystickState_Polled(joystick);
        joystick->hwdata->first_update = false;
        return;
    }

    if (joystick->hwdata->buffered ) {
        UpdateDINPUTJoystickState_Buffered(joystick);
    } else {
        UpdateDINPUTJoystickState_Polled(joystick);
    }
}

static void DINPUT_JoystickClose(SDL_Joystick *joystick)
{
    if (!joystick->hwdata) {
        return;
    }

    if (joystick->hwdata->ffeffect_ref) {
        IDirectInputEffect_Unload(joystick->hwdata->ffeffect_ref);
        joystick->hwdata->ffeffect_ref = NULL;
    }
    if (joystick->hwdata->ffeffect) {
        FreeRumbleEffectData(joystick->hwdata->ffeffect);
        joystick->hwdata->ffeffect = NULL;
    }
    if (joystick->hwdata->InputDevice) {
        IDirectInputDevice8_Unacquire(joystick->hwdata->InputDevice);
        IDirectInputDevice8_Release(joystick->hwdata->InputDevice);
    }
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}

static void DINPUT_JoystickQuit(void)
{
    // The joystick device thread has stopped, and the core has removed the devices
    while (s_DInputKnownDevices) {
        DINPUT_KnownDevice *next = s_DInputKnownDevices->next;
        SDL_free(s_DInputKnownDevices);
        s_DInputKnownDevices = next;
    }
    while (s_DInputPublishedDevices) {
        DINPUT_PublishedDevice *next = s_DInputPublishedDevices->next;
        SDL_free(s_DInputPublishedDevices);
        s_DInputPublishedDevices = next;
    }
    SDL_zero(s_DInputRescan);

    WINDOWS_QuitJoystickDeviceNotification();

    if (dinput != NULL) {
        IDirectInput8_Release(dinput);
        dinput = NULL;
    }

    if (coinitialized) {
        WIN_CoUninitialize();
        coinitialized = false;
    }
}

static bool DINPUT_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    if (!DINPUT_JoystickOpenDevice(joystick, device)) {
        DINPUT_JoystickClose(joystick);
        return false;
    }
    return true;
}

static void DINPUT_JoystickSetPlayerIndex(SDL_JoystickDevice *device, int player_index)
{
}

static bool DINPUT_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    return false;
}

SDL_JoystickDriver SDL_DINPUT_JoystickDriver = {
    .PreInit = DINPUT_JoystickPreInit,
    .Init = DINPUT_JoystickInit,
    .Discover = DINPUT_JoystickDiscover,
#ifdef SDL_JOYSTICK_HIDAPI
    .DevicePublishing = HIDAPI_UpdateJoystickDeviceName,
#endif
    .DevicePublished = DINPUT_JoystickDevicePublished,
    .SetPlayerIndex = DINPUT_JoystickSetPlayerIndex,
    .GetGamepadMapping = DINPUT_JoystickGetGamepadMapping,
    .Open = DINPUT_JoystickOpen,
    .Rumble = DINPUT_JoystickRumble,
    .Update = DINPUT_JoystickUpdate,
    .Close = DINPUT_JoystickClose,
    .Quit = DINPUT_JoystickQuit,
};

#else // !SDL_JOYSTICK_DINPUT

void SDL_DINPUT_JoystickAddHapticDevices(void)
{
}

#endif // SDL_JOYSTICK_DINPUT
