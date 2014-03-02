/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_DINPUT

/* DirectInput joystick driver; written by Glenn Maynard, based on Andrei de
 * A. Formiga's WINMM driver.
 *
 * Hats and sliders are completely untested; the app I'm writing this for mostly
 * doesn't use them and I don't own any joysticks with them.
 *
 * We don't bother to use event notification here.  It doesn't seem to work
 * with polled devices, and it's fine to call IDirectInputDevice8_GetDeviceData and
 * let it return 0 events. */

#include "SDL_error.h"
#include "SDL_assert.h"
#include "SDL_events.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_mutex.h"
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#if !SDL_EVENTS_DISABLED
#include "../../events/SDL_events_c.h"
#endif

#define INITGUID /* Only set here, if set twice will cause mingw32 to break. */
#include "SDL_dxjoystick_c.h"

#if SDL_HAPTIC_DINPUT
#include "../../haptic/windows/SDL_syshaptic_c.h"    /* For haptic hot plugging */
#endif

#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL      0x80000000
#endif

DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);


#define INPUT_QSIZE 32      /* Buffer up to 32 input messages */
#define AXIS_MIN    -32768  /* minimum value for axis coordinate */
#define AXIS_MAX    32767   /* maximum value for axis coordinate */
#define JOY_AXIS_THRESHOLD  (((AXIS_MAX)-(AXIS_MIN))/100)   /* 1% motion */

/* external variables referenced. */
extern HWND SDL_HelperWindow;


/* local variables */
static SDL_bool coinitialized = SDL_FALSE;
static LPDIRECTINPUT8 dinput = NULL;
static SDL_bool s_bDeviceAdded = SDL_FALSE;
static SDL_bool s_bDeviceRemoved = SDL_FALSE;
static SDL_JoystickID s_nInstanceID = -1;
static SDL_cond *s_condJoystickThread = NULL;
static SDL_mutex *s_mutexJoyStickEnum = NULL;
static SDL_Thread *s_threadJoystick = NULL;
static SDL_bool s_bJoystickThreadQuit = SDL_FALSE;
static SDL_bool s_bXInputEnabled = SDL_TRUE;

XInputGetState_t SDL_XInputGetState = NULL;
XInputSetState_t SDL_XInputSetState = NULL;
XInputGetCapabilities_t SDL_XInputGetCapabilities = NULL;
DWORD SDL_XInputVersion = 0;

static HANDLE s_pXInputDLL = 0;
static int s_XInputDLLRefCount = 0;

int
WIN_LoadXInputDLL(void)
{
    DWORD version = 0;

    if (s_pXInputDLL) {
        SDL_assert(s_XInputDLLRefCount > 0);
        s_XInputDLLRefCount++;
        return 0;  /* already loaded */
    }

    version = (1 << 16) | 4;
    s_pXInputDLL = LoadLibrary( L"XInput1_4.dll" );  /* 1.4 Ships with Windows 8. */
    if (!s_pXInputDLL) {
        version = (1 << 16) | 3;
        s_pXInputDLL = LoadLibrary( L"XInput1_3.dll" );  /* 1.3 Ships with Vista and Win7, can be installed as a redistributable component. */
    }
    if (!s_pXInputDLL) {
        s_pXInputDLL = LoadLibrary( L"bin\\XInput1_3.dll" );
    }
    if (!s_pXInputDLL) {
        return -1;
    }

    SDL_assert(s_XInputDLLRefCount == 0);
    SDL_XInputVersion = version;
    s_XInputDLLRefCount = 1;

    /* 100 is the ordinal for _XInputGetStateEx, which returns the same struct as XinputGetState, but with extra data in wButtons for the guide button, we think... */
    SDL_XInputGetState = (XInputGetState_t)GetProcAddress( (HMODULE)s_pXInputDLL, (LPCSTR)100 );
    SDL_XInputSetState = (XInputSetState_t)GetProcAddress( (HMODULE)s_pXInputDLL, "XInputSetState" );
    SDL_XInputGetCapabilities = (XInputGetCapabilities_t)GetProcAddress( (HMODULE)s_pXInputDLL, "XInputGetCapabilities" );
    if ( !SDL_XInputGetState || !SDL_XInputSetState || !SDL_XInputGetCapabilities ) {
        WIN_UnloadXInputDLL();
        return -1;
    }

    return 0;
}

void
WIN_UnloadXInputDLL(void)
{
    if ( s_pXInputDLL ) {
        SDL_assert(s_XInputDLLRefCount > 0);
        if (--s_XInputDLLRefCount == 0) {
            FreeLibrary( s_pXInputDLL );
            s_pXInputDLL = NULL;
        }
    } else {
        SDL_assert(s_XInputDLLRefCount == 0);
    }
}


extern HRESULT(WINAPI * DInputCreate) (HINSTANCE hinst, DWORD dwVersion,
                                       LPDIRECTINPUT * ppDI,
                                       LPUNKNOWN punkOuter);
struct JoyStick_DeviceData_
{
    SDL_JoystickGUID guid;
    DIDEVICEINSTANCE dxdevice;
    char *joystickname;
    Uint8 send_add_event;
    SDL_JoystickID nInstanceID;
    SDL_bool bXInputDevice;
    Uint8 XInputUserId;
    struct JoyStick_DeviceData_ *pNext;
};

typedef struct JoyStick_DeviceData_ JoyStick_DeviceData;

static JoyStick_DeviceData *SYS_Joystick;    /* array to hold joystick ID values */

/* local prototypes */
static int SetDIerror(const char *function, HRESULT code);
static BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *
                                           pdidInstance, VOID * pContext);
static BOOL CALLBACK EnumDevObjectsCallback(LPCDIDEVICEOBJECTINSTANCE dev,
                                            LPVOID pvRef);
static void SortDevObjects(SDL_Joystick *joystick);
static Uint8 TranslatePOV(DWORD value);
static int SDL_PrivateJoystickAxis_Int(SDL_Joystick * joystick, Uint8 axis,
                                       Sint16 value);
static int SDL_PrivateJoystickHat_Int(SDL_Joystick * joystick, Uint8 hat,
                                      Uint8 value);
static int SDL_PrivateJoystickButton_Int(SDL_Joystick * joystick,
                                         Uint8 button, Uint8 state);

/* Taken from Wine - Thanks! */
DIOBJECTDATAFORMAT dfDIJoystick2[] = {
  { &GUID_XAxis,DIJOFS_X,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_YAxis,DIJOFS_Y,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_ZAxis,DIJOFS_Z,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RxAxis,DIJOFS_RX,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RyAxis,DIJOFS_RY,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RzAxis,DIJOFS_RZ,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_POV,DIJOFS_POV(0),DIDFT_OPTIONAL|DIDFT_POV|DIDFT_ANYINSTANCE,0},
  { &GUID_POV,DIJOFS_POV(1),DIDFT_OPTIONAL|DIDFT_POV|DIDFT_ANYINSTANCE,0},
  { &GUID_POV,DIJOFS_POV(2),DIDFT_OPTIONAL|DIDFT_POV|DIDFT_ANYINSTANCE,0},
  { &GUID_POV,DIJOFS_POV(3),DIDFT_OPTIONAL|DIDFT_POV|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(0),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(1),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(2),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(3),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(4),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(5),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(6),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(7),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(8),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(9),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(10),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(11),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(12),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(13),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(14),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(15),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(16),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(17),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(18),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(19),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(20),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(21),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(22),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(23),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(24),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(25),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(26),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(27),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(28),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(29),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(30),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(31),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(32),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(33),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(34),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(35),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(36),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(37),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(38),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(39),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(40),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(41),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(42),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(43),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(44),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(45),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(46),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(47),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(48),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(49),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(50),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(51),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(52),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(53),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(54),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(55),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(56),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(57),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(58),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(59),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(60),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(61),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(62),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(63),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(64),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(65),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(66),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(67),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(68),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(69),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(70),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(71),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(72),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(73),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(74),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(75),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(76),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(77),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(78),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(79),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(80),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(81),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(82),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(83),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(84),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(85),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(86),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(87),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(88),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(89),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(90),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(91),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(92),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(93),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(94),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(95),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(96),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(97),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(98),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(99),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(100),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(101),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(102),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(103),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(104),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(105),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(106),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(107),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(108),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(109),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(110),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(111),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(112),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(113),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(114),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(115),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(116),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(117),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(118),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(119),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(120),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(121),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(122),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(123),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(124),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(125),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(126),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { NULL,DIJOFS_BUTTON(127),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_XAxis,FIELD_OFFSET(DIJOYSTATE2,lVX),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_YAxis,FIELD_OFFSET(DIJOYSTATE2,lVY),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_ZAxis,FIELD_OFFSET(DIJOYSTATE2,lVZ),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RxAxis,FIELD_OFFSET(DIJOYSTATE2,lVRx),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RyAxis,FIELD_OFFSET(DIJOYSTATE2,lVRy),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RzAxis,FIELD_OFFSET(DIJOYSTATE2,lVRz),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,FIELD_OFFSET(DIJOYSTATE2,rglVSlider[0]),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,FIELD_OFFSET(DIJOYSTATE2,rglVSlider[1]),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_XAxis,FIELD_OFFSET(DIJOYSTATE2,lAX),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_YAxis,FIELD_OFFSET(DIJOYSTATE2,lAY),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_ZAxis,FIELD_OFFSET(DIJOYSTATE2,lAZ),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RxAxis,FIELD_OFFSET(DIJOYSTATE2,lARx),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RyAxis,FIELD_OFFSET(DIJOYSTATE2,lARy),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RzAxis,FIELD_OFFSET(DIJOYSTATE2,lARz),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,FIELD_OFFSET(DIJOYSTATE2,rglASlider[0]),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,FIELD_OFFSET(DIJOYSTATE2,rglASlider[1]),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_XAxis,FIELD_OFFSET(DIJOYSTATE2,lFX),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_YAxis,FIELD_OFFSET(DIJOYSTATE2,lFY),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_ZAxis,FIELD_OFFSET(DIJOYSTATE2,lFZ),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RxAxis,FIELD_OFFSET(DIJOYSTATE2,lFRx),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RyAxis,FIELD_OFFSET(DIJOYSTATE2,lFRy),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RzAxis,FIELD_OFFSET(DIJOYSTATE2,lFRz),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,FIELD_OFFSET(DIJOYSTATE2,rglFSlider[0]),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Slider,FIELD_OFFSET(DIJOYSTATE2,rglFSlider[1]),DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
};

const DIDATAFORMAT c_dfDIJoystick2 = {
    sizeof(DIDATAFORMAT),
    sizeof(DIOBJECTDATAFORMAT),
    DIDF_ABSAXIS,
    sizeof(DIJOYSTATE2),
    SDL_arraysize(dfDIJoystick2),
    dfDIJoystick2
};


/* Convert a DirectInput return code to a text message */
static int
SetDIerror(const char *function, HRESULT code)
{
    /*
    return SDL_SetError("%s() [%s]: %s", function,
                 DXGetErrorString9A(code), DXGetErrorDescription9A(code));
     */
    return SDL_SetError("%s() DirectX error %d", function, code);
}


#define SAFE_RELEASE(p)                             \
{                                                   \
    if (p) {                                        \
        (p)->lpVtbl->Release((p));                  \
        (p) = 0;                                    \
    }                                               \
}

DEFINE_GUID(IID_ValveStreamingGamepad,  MAKELONG( 0x28DE, 0x11FF ),0x0000,0x0000,0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44);
DEFINE_GUID(IID_X360WiredGamepad,  MAKELONG( 0x045E, 0x02A1 ),0x0000,0x0000,0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44);
DEFINE_GUID(IID_X360WirelessGamepad,  MAKELONG( 0x045E, 0x028E ),0x0000,0x0000,0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44);

static PRAWINPUTDEVICELIST SDL_RawDevList = NULL;
static UINT SDL_RawDevListCount = 0;

static SDL_bool
SDL_IsXInputDevice( const GUID* pGuidProductFromDirectInput )
{
    static const GUID *s_XInputProductGUID[] = {
        &IID_ValveStreamingGamepad,
        &IID_X360WiredGamepad,   /* Microsoft's wired X360 controller for Windows. */
        &IID_X360WirelessGamepad /* Microsoft's wireless X360 controller for Windows. */
    };

    size_t iDevice;
    UINT i;

    if (!s_bXInputEnabled) {
        return SDL_FALSE;
    }

    /* Check for well known XInput device GUIDs */
    /* This lets us skip RAWINPUT for popular devices. Also, we need to do this for the Valve Streaming Gamepad because it's virtualized and doesn't show up in the device list. */
    for ( iDevice = 0; iDevice < SDL_arraysize(s_XInputProductGUID); ++iDevice ) {
        if (SDL_memcmp(pGuidProductFromDirectInput, s_XInputProductGUID[iDevice], sizeof(GUID)) == 0) {
            return SDL_TRUE;
        }
    }

    /* Go through RAWINPUT (WinXP and later) to find HID devices. */
    /* Cache this if we end up using it. */
    if (SDL_RawDevList == NULL) {
        if ((GetRawInputDeviceList(NULL, &SDL_RawDevListCount, sizeof (RAWINPUTDEVICELIST)) == -1) || (!SDL_RawDevListCount)) {
            return SDL_FALSE;  /* oh well. */
        }

        SDL_RawDevList = (PRAWINPUTDEVICELIST) SDL_malloc(sizeof (RAWINPUTDEVICELIST) * SDL_RawDevListCount);
        if (SDL_RawDevList == NULL) {
            SDL_OutOfMemory();
            return SDL_FALSE;
        }

        if (GetRawInputDeviceList(SDL_RawDevList, &SDL_RawDevListCount, sizeof (RAWINPUTDEVICELIST)) == -1) {
             SDL_free(SDL_RawDevList);
             SDL_RawDevList = NULL;
             return SDL_FALSE;  /* oh well. */
        }
    }

    for (i = 0; i < SDL_RawDevListCount; i++) {
        RID_DEVICE_INFO rdi;
        char devName[128];
        UINT rdiSize = sizeof (rdi);
        UINT nameSize = SDL_arraysize(devName);

        rdi.cbSize = sizeof (rdi);
        if ( (SDL_RawDevList[i].dwType == RIM_TYPEHID) &&
             (GetRawInputDeviceInfoA(SDL_RawDevList[i].hDevice, RIDI_DEVICEINFO, &rdi, &rdiSize) != ((UINT)-1)) &&
             (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) == ((LONG)pGuidProductFromDirectInput->Data1)) &&
             (GetRawInputDeviceInfoA(SDL_RawDevList[i].hDevice, RIDI_DEVICENAME, devName, &nameSize) != ((UINT)-1)) &&
             (SDL_strstr(devName, "IG_") != NULL) ) {
             return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}


static SDL_bool s_bWindowsDeviceChanged = SDL_FALSE;

/* windowproc for our joystick detect thread message only window, to detect any USB device addition/removal
 */
LRESULT CALLBACK SDL_PrivateJoystickDetectProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)    {
    switch (message)    {
    case WM_DEVICECHANGE:
        switch (wParam) {
        case DBT_DEVICEARRIVAL:
            if (((DEV_BROADCAST_HDR*)lParam)->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)    {
                s_bWindowsDeviceChanged = SDL_TRUE;
            }
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            if (((DEV_BROADCAST_HDR*)lParam)->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)    {
                s_bWindowsDeviceChanged = SDL_TRUE;
            }
            break;
        }
        return 0;
    }

    return DefWindowProc (hwnd, message, wParam, lParam);
}


DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, \
    0xC0, 0x4F, 0xB9, 0x51, 0xED);

/* Function/thread to scan the system for joysticks.
 */
static int
SDL_JoystickThread(void *_data)
{
    HWND messageWindow = 0;
    HDEVNOTIFY hNotify = 0;
    DEV_BROADCAST_DEVICEINTERFACE dbh;
    SDL_bool bOpenedXInputDevices[SDL_XINPUT_MAX_DEVICES];
    WNDCLASSEX wincl;

    SDL_zero(bOpenedXInputDevices);

    WIN_CoInitialize();

    SDL_memset( &wincl, 0x0, sizeof(wincl) );
    wincl.hInstance = GetModuleHandle( NULL );
    wincl.lpszClassName = L"Message";
    wincl.lpfnWndProc = SDL_PrivateJoystickDetectProc;      /* This function is called by windows */
    wincl.cbSize = sizeof (WNDCLASSEX);

    if (!RegisterClassEx (&wincl))
    {
        return SDL_SetError("Failed to create register class for joystick autodetect.", GetLastError());
    }

    messageWindow = (HWND)CreateWindowEx( 0,  L"Message", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL );
    if ( !messageWindow )
    {
        return SDL_SetError("Failed to create message window for joystick autodetect.", GetLastError());
    }

    SDL_zero(dbh);

    dbh.dbcc_size = sizeof(dbh);
    dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbh.dbcc_classguid = GUID_DEVINTERFACE_HID;

    hNotify = RegisterDeviceNotification( messageWindow, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE );
    if ( !hNotify )
    {
        return SDL_SetError("Failed to create notify device for joystick autodetect.", GetLastError());
    }

    SDL_LockMutex( s_mutexJoyStickEnum );
    while ( s_bJoystickThreadQuit == SDL_FALSE )
    {
        MSG messages;
        SDL_bool bXInputChanged = SDL_FALSE;

        SDL_CondWaitTimeout( s_condJoystickThread, s_mutexJoyStickEnum, 300 );

        while ( s_bJoystickThreadQuit == SDL_FALSE && PeekMessage(&messages, messageWindow, 0, 0, PM_NOREMOVE) )
        {
            if ( GetMessage(&messages, messageWindow, 0, 0) != 0 )  {
                TranslateMessage(&messages);
                DispatchMessage(&messages);
            }
        }

        if ( s_bXInputEnabled && XINPUTGETCAPABILITIES ) {
            /* scan for any change in XInput devices */
            Uint8 userId;
            for (userId = 0; userId < SDL_XINPUT_MAX_DEVICES; userId++) {
                XINPUT_CAPABILITIES capabilities;
                const DWORD result = XINPUTGETCAPABILITIES( userId, XINPUT_FLAG_GAMEPAD, &capabilities );
                const SDL_bool available = (result == ERROR_SUCCESS);
                if (bOpenedXInputDevices[userId] != available) {
                    bXInputChanged = SDL_TRUE;
                    bOpenedXInputDevices[userId] = available;
                }
            }
        }

        if (s_bWindowsDeviceChanged || bXInputChanged) {
            SDL_UnlockMutex( s_mutexJoyStickEnum );  /* let main thread go while we SDL_Delay(). */
            SDL_Delay( 300 ); /* wait for direct input to find out about this device */
            SDL_LockMutex( s_mutexJoyStickEnum );

            s_bDeviceRemoved = SDL_TRUE;
            s_bDeviceAdded = SDL_TRUE;
            s_bWindowsDeviceChanged = SDL_FALSE;
        }
    }
    SDL_UnlockMutex( s_mutexJoyStickEnum );

    if ( hNotify )
        UnregisterDeviceNotification( hNotify );

    if ( messageWindow )
        DestroyWindow( messageWindow );

    UnregisterClass( wincl.lpszClassName, wincl.hInstance );
    messageWindow = 0;
    WIN_CoUninitialize();
    return 1;
}


/* Function to scan the system for joysticks.
 * This function should set SDL_numjoysticks to the number of available
 * joysticks.  Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    HRESULT result;
    HINSTANCE instance;
    const char *env = SDL_GetHint(SDL_HINT_XINPUT_ENABLED);
    if (env && !SDL_atoi(env)) {
        s_bXInputEnabled = SDL_FALSE;
    }

    result = WIN_CoInitialize();
    if (FAILED(result)) {
        return SetDIerror("CoInitialize", result);
    }

    coinitialized = SDL_TRUE;

    result = CoCreateInstance(&CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IDirectInput8, (LPVOID)&dinput);

    if (FAILED(result)) {
        SDL_SYS_JoystickQuit();
        return SetDIerror("CoCreateInstance", result);
    }

    /* Because we used CoCreateInstance, we need to Initialize it, first. */
    instance = GetModuleHandle(NULL);
    if (instance == NULL) {
        SDL_SYS_JoystickQuit();
        return SDL_SetError("GetModuleHandle() failed with error code %d.", GetLastError());
    }
    result = IDirectInput8_Initialize(dinput, instance, DIRECTINPUT_VERSION);

    if (FAILED(result)) {
        SDL_SYS_JoystickQuit();
        return SetDIerror("IDirectInput::Initialize", result);
    }

    if ((s_bXInputEnabled) && (WIN_LoadXInputDLL() == -1)) {
        s_bXInputEnabled = SDL_FALSE;  /* oh well. */
    }

    s_mutexJoyStickEnum = SDL_CreateMutex();
    s_condJoystickThread = SDL_CreateCond();
    s_bDeviceAdded = SDL_TRUE; /* force a scan of the system for joysticks this first time */

    SDL_SYS_JoystickDetect();

    if ( !s_threadJoystick )
    {
        s_bJoystickThreadQuit = SDL_FALSE;
        /* spin up the thread to detect hotplug of devices */
#if defined(__WIN32__) && !defined(HAVE_LIBC)
#undef SDL_CreateThread
#if SDL_DYNAMIC_API
        s_threadJoystick= SDL_CreateThread_REAL( SDL_JoystickThread, "SDL_joystick", NULL, NULL, NULL );
#else
        s_threadJoystick= SDL_CreateThread( SDL_JoystickThread, "SDL_joystick", NULL, NULL, NULL );
#endif
#else
        s_threadJoystick = SDL_CreateThread( SDL_JoystickThread, "SDL_joystick", NULL );
#endif
    }
        return SDL_SYS_NumJoysticks();
}

/* return the number of joysticks that are connected right now */
int SDL_SYS_NumJoysticks()
{
    int nJoysticks = 0;
    JoyStick_DeviceData *device = SYS_Joystick;
    while ( device )
    {
        nJoysticks++;
        device = device->pNext;
    }

    return nJoysticks;
}

/* helper function for direct input, gets called for each connected joystick */
static BOOL CALLBACK
    EnumJoysticksCallback(const DIDEVICEINSTANCE * pdidInstance, VOID * pContext)
{
    JoyStick_DeviceData *pNewJoystick;
    JoyStick_DeviceData *pPrevJoystick = NULL;

    if (SDL_IsXInputDevice( &pdidInstance->guidProduct )) {
        return DIENUM_CONTINUE;  /* ignore XInput devices here, keep going. */
    }

    pNewJoystick = *(JoyStick_DeviceData **)pContext;
    while ( pNewJoystick )
    {
        if ( !SDL_memcmp( &pNewJoystick->dxdevice.guidInstance, &pdidInstance->guidInstance, sizeof(pNewJoystick->dxdevice.guidInstance) ) )
        {
            /* if we are replacing the front of the list then update it */
            if ( pNewJoystick == *(JoyStick_DeviceData **)pContext )
            {
                *(JoyStick_DeviceData **)pContext = pNewJoystick->pNext;
            }
            else if ( pPrevJoystick )
            {
                pPrevJoystick->pNext = pNewJoystick->pNext;
            }

            pNewJoystick->pNext = SYS_Joystick;
            SYS_Joystick = pNewJoystick;

            return DIENUM_CONTINUE; /* already have this joystick loaded, just keep going */
        }

        pPrevJoystick = pNewJoystick;
        pNewJoystick = pNewJoystick->pNext;
    }

    pNewJoystick = (JoyStick_DeviceData *)SDL_malloc( sizeof(JoyStick_DeviceData) );
    if (!pNewJoystick) {
        return DIENUM_CONTINUE; /* better luck next time? */
    }

    SDL_zerop(pNewJoystick);
    pNewJoystick->joystickname = WIN_StringToUTF8(pdidInstance->tszProductName);
    if (!pNewJoystick->joystickname) {
        SDL_free(pNewJoystick);
        return DIENUM_CONTINUE; /* better luck next time? */
    }

    SDL_memcpy(&(pNewJoystick->dxdevice), pdidInstance,
        sizeof(DIDEVICEINSTANCE));

    pNewJoystick->XInputUserId = INVALID_XINPUT_USERID;
    pNewJoystick->send_add_event = 1;
    pNewJoystick->nInstanceID = ++s_nInstanceID;
    SDL_memcpy( &pNewJoystick->guid, &pdidInstance->guidProduct, sizeof(pNewJoystick->guid) );
    pNewJoystick->pNext = SYS_Joystick;
    SYS_Joystick = pNewJoystick;

    s_bDeviceAdded = SDL_TRUE;

    return DIENUM_CONTINUE; /* get next device, please */
}

static void
AddXInputDevice(const Uint8 userid, JoyStick_DeviceData **pContext)
{
    char name[32];
    JoyStick_DeviceData *pPrevJoystick = NULL;
    JoyStick_DeviceData *pNewJoystick = *pContext;

    while (pNewJoystick) {
        if ((pNewJoystick->bXInputDevice) && (pNewJoystick->XInputUserId == userid)) {
            /* if we are replacing the front of the list then update it */
            if (pNewJoystick == *pContext) {
                *pContext = pNewJoystick->pNext;
            } else if (pPrevJoystick) {
                pPrevJoystick->pNext = pNewJoystick->pNext;
            }

            pNewJoystick->pNext = SYS_Joystick;
            SYS_Joystick = pNewJoystick;
            return;   /* already in the list. */
        }

        pPrevJoystick = pNewJoystick;
        pNewJoystick = pNewJoystick->pNext;
    }

    pNewJoystick = (JoyStick_DeviceData *) SDL_malloc(sizeof (JoyStick_DeviceData));
    if (!pNewJoystick) {
        return; /* better luck next time? */
    }
    SDL_zerop(pNewJoystick);

    SDL_snprintf(name, sizeof (name), "XInput Controller #%u", ((unsigned int) userid) + 1);
    pNewJoystick->joystickname = SDL_strdup(name);
    if (!pNewJoystick->joystickname) {
        SDL_free(pNewJoystick);
        return; /* better luck next time? */
    }

    pNewJoystick->bXInputDevice = SDL_TRUE;
    pNewJoystick->XInputUserId = userid;
    pNewJoystick->send_add_event = 1;
    pNewJoystick->nInstanceID = ++s_nInstanceID;
    pNewJoystick->pNext = SYS_Joystick;
    SYS_Joystick = pNewJoystick;

    s_bDeviceAdded = SDL_TRUE;
}

static void
EnumXInputDevices(JoyStick_DeviceData **pContext)
{
    if (s_bXInputEnabled) {
        int iuserid;
        /* iterate in reverse, so these are in the final list in ascending numeric order. */
        for (iuserid = SDL_XINPUT_MAX_DEVICES-1; iuserid >= 0; iuserid--) {
            const Uint8 userid = (Uint8) iuserid;
            XINPUT_CAPABILITIES capabilities;
            if (XINPUTGETCAPABILITIES(userid, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS) {
                /* Current version of XInput mistakenly returns 0 as the Type. Ignore it and ensure the subtype is a gamepad. */
                /* !!! FIXME: we might want to support steering wheels or guitars or whatever later. */
                if (capabilities.SubType == XINPUT_DEVSUBTYPE_GAMEPAD) {
                    AddXInputDevice(userid, pContext);
                }
            }
        }
    }
}


/* detect any new joysticks being inserted into the system */
void SDL_SYS_JoystickDetect()
{
    JoyStick_DeviceData *pCurList = NULL;
    /* only enum the devices if the joystick thread told us something changed */
    if ( s_bDeviceAdded || s_bDeviceRemoved )
    {
        SDL_LockMutex( s_mutexJoyStickEnum );

        s_bDeviceAdded = SDL_FALSE;
        s_bDeviceRemoved = SDL_FALSE;

        pCurList = SYS_Joystick;
        SYS_Joystick = NULL;

        /* Look for DirectInput joysticks, wheels, head trackers, gamepads, etc.. */
        IDirectInput8_EnumDevices(dinput,
            DI8DEVCLASS_GAMECTRL,
            EnumJoysticksCallback,
            &pCurList, DIEDFL_ATTACHEDONLY);

        SDL_free(SDL_RawDevList);  /* in case we used this in DirectInput enumerator. */
        SDL_RawDevList = NULL;
        SDL_RawDevListCount = 0;

        /* Look for XInput devices. Do this last, so they're first in the final list. */
        EnumXInputDevices(&pCurList);

        SDL_UnlockMutex( s_mutexJoyStickEnum );
    }

    if ( pCurList )
    {
        while ( pCurList )
        {
            JoyStick_DeviceData *pListNext = NULL;

#if SDL_HAPTIC_DINPUT
            if (pCurList->bXInputDevice) {
                XInputHaptic_MaybeRemoveDevice(pCurList->XInputUserId);
            } else {
                DirectInputHaptic_MaybeRemoveDevice(&pCurList->dxdevice);
            }
#endif

#if !SDL_EVENTS_DISABLED
            {
            SDL_Event event;
            event.type = SDL_JOYDEVICEREMOVED;

            if (SDL_GetEventState(event.type) == SDL_ENABLE) {
                event.jdevice.which = pCurList->nInstanceID;
                if ((SDL_EventOK == NULL)
                    || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
                        SDL_PushEvent(&event);
                }
            }
            }
#endif /* !SDL_EVENTS_DISABLED */

            pListNext = pCurList->pNext;
            SDL_free(pCurList->joystickname);
            SDL_free(pCurList);
            pCurList = pListNext;
        }

    }

    if ( s_bDeviceAdded )
    {
        JoyStick_DeviceData *pNewJoystick;
        int device_index = 0;
        s_bDeviceAdded = SDL_FALSE;
        pNewJoystick = SYS_Joystick;
        while ( pNewJoystick )
        {
            if ( pNewJoystick->send_add_event )
            {
#if SDL_HAPTIC_DINPUT
                if (pNewJoystick->bXInputDevice) {
                    XInputHaptic_MaybeAddDevice(pNewJoystick->XInputUserId);
                } else {
                    DirectInputHaptic_MaybeAddDevice(&pNewJoystick->dxdevice);
                }
#endif

#if !SDL_EVENTS_DISABLED
                {
                SDL_Event event;
                event.type = SDL_JOYDEVICEADDED;

                if (SDL_GetEventState(event.type) == SDL_ENABLE) {
                    event.jdevice.which = device_index;
                    if ((SDL_EventOK == NULL)
                        || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
                            SDL_PushEvent(&event);
                    }
                }
                }
#endif /* !SDL_EVENTS_DISABLED */
                pNewJoystick->send_add_event = 0;
            }
            device_index++;
            pNewJoystick = pNewJoystick->pNext;
        }
    }
}

/* we need to poll if we have pending hotplug device changes or connected devices */
SDL_bool SDL_SYS_JoystickNeedsPolling()
{
    /* we have a new device or one was pulled, we need to think this frame please */
    if ( s_bDeviceAdded || s_bDeviceRemoved )
        return SDL_TRUE;

    return SDL_FALSE;
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    JoyStick_DeviceData *device = SYS_Joystick;

    for (; device_index > 0; device_index--)
        device = device->pNext;

    return device->joystickname;
}

/* Function to perform the mapping between current device instance and this joysticks instance id */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    JoyStick_DeviceData *device = SYS_Joystick;
    int index;

    for (index = device_index; index > 0; index--)
        device = device->pNext;

    return device->nInstanceID;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    HRESULT result;
    JoyStick_DeviceData *joystickdevice = SYS_Joystick;

    for (; device_index > 0; device_index--)
        joystickdevice = joystickdevice->pNext;

    /* allocate memory for system specific hardware data */
    joystick->instance_id = joystickdevice->nInstanceID;
    joystick->closed = 0;
    joystick->hwdata =
        (struct joystick_hwdata *) SDL_malloc(sizeof(struct joystick_hwdata));
    if (joystick->hwdata == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(joystick->hwdata);

    if (joystickdevice->bXInputDevice) {
        const SDL_bool bIs14OrLater = (SDL_XInputVersion >= ((1<<16)|4));
        const Uint8 userId = joystickdevice->XInputUserId;
        XINPUT_CAPABILITIES capabilities;

        SDL_assert(s_bXInputEnabled);
        SDL_assert(XINPUTGETCAPABILITIES);
        SDL_assert(userId >= 0);
        SDL_assert(userId < SDL_XINPUT_MAX_DEVICES);

        joystick->hwdata->bXInputDevice = SDL_TRUE;

        if (XINPUTGETCAPABILITIES(userId, XINPUT_FLAG_GAMEPAD, &capabilities) != ERROR_SUCCESS) {
            SDL_free(joystick->hwdata);
            joystick->hwdata = NULL;
            return SDL_SetError("Failed to obtain XInput device capabilities. Device disconnected?");
        } else {
            /* Current version of XInput mistakenly returns 0 as the Type. Ignore it and ensure the subtype is a gamepad. */
            SDL_assert(capabilities.SubType == XINPUT_DEVSUBTYPE_GAMEPAD);
            if ((!bIs14OrLater) || (capabilities.Flags & XINPUT_CAPS_FFB_SUPPORTED)) {
                joystick->hwdata->bXInputHaptic = SDL_TRUE;
            }
            joystick->hwdata->userid = userId;

            /* The XInput API has a hard coded button/axis mapping, so we just match it */
            joystick->naxes = 6;
            joystick->nbuttons = 15;
            joystick->nballs = 0;
            joystick->nhats = 0;
		}
    } else {  /* use DirectInput, not XInput. */
        LPDIRECTINPUTDEVICE8 device;
        DIPROPDWORD dipdw;

        joystick->hwdata->buffered = 1;
        joystick->hwdata->removed = 0;
        joystick->hwdata->Capabilities.dwSize = sizeof(DIDEVCAPS);
        joystick->hwdata->guid = joystickdevice->guid;

        SDL_zero(dipdw);
        dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);

        result =
            IDirectInput8_CreateDevice(dinput,
                                      &(joystickdevice->dxdevice.guidInstance), &device, NULL);
        if (FAILED(result)) {
            return SetDIerror("IDirectInput::CreateDevice", result);
        }

        /* Now get the IDirectInputDevice8 interface, instead. */
        result = IDirectInputDevice8_QueryInterface(device,
                                                   &IID_IDirectInputDevice8,
                                                   (LPVOID *) & joystick->
                                                   hwdata->InputDevice);
        /* We are done with this object.  Use the stored one from now on. */
        IDirectInputDevice8_Release(device);

        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::QueryInterface", result);
        }

        /* Acquire shared access. Exclusive access is required for forces,
         * though. */
        result =
            IDirectInputDevice8_SetCooperativeLevel(joystick->hwdata->
                                                    InputDevice, SDL_HelperWindow,
                                                    DISCL_NONEXCLUSIVE |
                                                    DISCL_BACKGROUND);
        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::SetCooperativeLevel", result);
        }

        /* Use the extended data structure: DIJOYSTATE2. */
        result =
            IDirectInputDevice8_SetDataFormat(joystick->hwdata->InputDevice,
                                              &c_dfDIJoystick2);
        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::SetDataFormat", result);
        }

        /* Get device capabilities */
        result =
            IDirectInputDevice8_GetCapabilities(joystick->hwdata->InputDevice,
                                                &joystick->hwdata->Capabilities);

        if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::GetCapabilities", result);
        }

        /* Force capable? */
        if (joystick->hwdata->Capabilities.dwFlags & DIDC_FORCEFEEDBACK) {

            result = IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);

            if (FAILED(result)) {
                return SetDIerror("IDirectInputDevice8::Acquire", result);
            }

            /* reset all accuators. */
            result =
                IDirectInputDevice8_SendForceFeedbackCommand(joystick->hwdata->
                                                             InputDevice,
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
        }

        /* What buttons and axes does it have? */
        IDirectInputDevice8_EnumObjects(joystick->hwdata->InputDevice,
                                        EnumDevObjectsCallback, joystick,
                                        DIDFT_BUTTON | DIDFT_AXIS | DIDFT_POV);

        /* Reorder the input objects. Some devices do not report the X axis as
         * the first axis, for example. */
        SortDevObjects(joystick);

        dipdw.diph.dwObj = 0;
        dipdw.diph.dwHow = DIPH_DEVICE;
        dipdw.dwData = INPUT_QSIZE;

        /* Set the buffer size */
        result =
            IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                            DIPROP_BUFFERSIZE, &dipdw.diph);

        if (result == DI_POLLEDDEVICE) {
            /* This device doesn't support buffering, so we're forced
             * to use less reliable polling. */
            joystick->hwdata->buffered = 0;
        } else if (FAILED(result)) {
            return SetDIerror("IDirectInputDevice8::SetProperty", result);
        }
    }
    return (0);
}

/* return true if this joystick is plugged in right now */
SDL_bool SDL_SYS_JoystickAttached( SDL_Joystick * joystick )
{
    return joystick->closed == 0 && joystick->hwdata->removed == 0;
}


/* Sort using the data offset into the DInput struct.
 * This gives a reasonable ordering for the inputs. */
static int
SortDevFunc(const void *a, const void *b)
{
    const input_t *inputA = (const input_t*)a;
    const input_t *inputB = (const input_t*)b;

    if (inputA->ofs < inputB->ofs)
        return -1;
    if (inputA->ofs > inputB->ofs)
        return 1;
    return 0;
}

/* Sort the input objects and recalculate the indices for each input. */
static void
SortDevObjects(SDL_Joystick *joystick)
{
    input_t *inputs = joystick->hwdata->Inputs;
    int nButtons = 0;
    int nHats = 0;
    int nAxis = 0;
    int n;

    SDL_qsort(inputs, joystick->hwdata->NumInputs, sizeof(input_t), SortDevFunc);

    for (n = 0; n < joystick->hwdata->NumInputs; n++)
    {
        switch (inputs[n].type)
        {
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

static BOOL CALLBACK
EnumDevObjectsCallback(LPCDIDEVICEOBJECTINSTANCE dev, LPVOID pvRef)
{
    SDL_Joystick *joystick = (SDL_Joystick *) pvRef;
    HRESULT result;
    input_t *in = &joystick->hwdata->Inputs[joystick->hwdata->NumInputs];

    if (dev->dwType & DIDFT_BUTTON) {
        in->type = BUTTON;
        in->num = joystick->nbuttons;
        in->ofs = DIJOFS_BUTTON( in->num );
        joystick->nbuttons++;
    } else if (dev->dwType & DIDFT_POV) {
        in->type = HAT;
        in->num = joystick->nhats;
        in->ofs = DIJOFS_POV( in->num );
        joystick->nhats++;
    } else if (dev->dwType & DIDFT_AXIS) {
        DIPROPRANGE diprg;
        DIPROPDWORD dilong;

        in->type = AXIS;
        in->num = joystick->naxes;
        /* work our the axis this guy maps too, thanks for the code icculus! */
        if ( !SDL_memcmp( &dev->guidType, &GUID_XAxis, sizeof(dev->guidType) ) )
            in->ofs = DIJOFS_X;
        else if ( !SDL_memcmp( &dev->guidType, &GUID_YAxis, sizeof(dev->guidType) ) )
            in->ofs = DIJOFS_Y;
        else if ( !SDL_memcmp( &dev->guidType, &GUID_ZAxis, sizeof(dev->guidType) ) )
            in->ofs = DIJOFS_Z;
        else if ( !SDL_memcmp( &dev->guidType, &GUID_RxAxis, sizeof(dev->guidType) ) )
            in->ofs = DIJOFS_RX;
        else if ( !SDL_memcmp( &dev->guidType, &GUID_RyAxis, sizeof(dev->guidType) ) )
            in->ofs = DIJOFS_RY;
        else if ( !SDL_memcmp( &dev->guidType, &GUID_RzAxis, sizeof(dev->guidType) ) )
            in->ofs = DIJOFS_RZ;
        else if ( !SDL_memcmp( &dev->guidType, &GUID_Slider, sizeof(dev->guidType) ) )
        {
            in->ofs = DIJOFS_SLIDER( joystick->hwdata->NumSliders );
            ++joystick->hwdata->NumSliders;
        }
        else
        {
             return DIENUM_CONTINUE; /* not an axis we can grok */
        }

        diprg.diph.dwSize = sizeof(diprg);
        diprg.diph.dwHeaderSize = sizeof(diprg.diph);
        diprg.diph.dwObj = dev->dwType;
        diprg.diph.dwHow = DIPH_BYID;
        diprg.lMin = AXIS_MIN;
        diprg.lMax = AXIS_MAX;

        result =
            IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                            DIPROP_RANGE, &diprg.diph);
        if (FAILED(result)) {
            return DIENUM_CONTINUE;     /* don't use this axis */
        }

        /* Set dead zone to 0. */
        dilong.diph.dwSize = sizeof(dilong);
        dilong.diph.dwHeaderSize = sizeof(dilong.diph);
        dilong.diph.dwObj = dev->dwType;
        dilong.diph.dwHow = DIPH_BYID;
        dilong.dwData = 0;
        result =
            IDirectInputDevice8_SetProperty(joystick->hwdata->InputDevice,
                                            DIPROP_DEADZONE, &dilong.diph);
        if (FAILED(result)) {
            return DIENUM_CONTINUE;     /* don't use this axis */
        }

        joystick->naxes++;
    } else {
        /* not supported at this time */
        return DIENUM_CONTINUE;
    }

    joystick->hwdata->NumInputs++;

    if (joystick->hwdata->NumInputs == MAX_INPUTS) {
        return DIENUM_STOP;     /* too many */
    }

    return DIENUM_CONTINUE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate_Polled(SDL_Joystick * joystick)
{
    DIJOYSTATE2 state;
    HRESULT result;
    int i;

    result =
        IDirectInputDevice8_GetDeviceState(joystick->hwdata->InputDevice,
                                           sizeof(DIJOYSTATE2), &state);
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
        result =
            IDirectInputDevice8_GetDeviceState(joystick->hwdata->InputDevice,
                                               sizeof(DIJOYSTATE2), &state);
    }

    if ( result != DI_OK )
    {
        joystick->hwdata->send_remove_event = 1;
        joystick->hwdata->removed = 1;
        return;
    }

    /* Set each known axis, button and POV. */
    for (i = 0; i < joystick->hwdata->NumInputs; ++i) {
        const input_t *in = &joystick->hwdata->Inputs[i];

        switch (in->type) {
        case AXIS:
            switch (in->ofs) {
            case DIJOFS_X:
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.lX);
                break;
            case DIJOFS_Y:
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.lY);
                break;
            case DIJOFS_Z:
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.lZ);
                break;
            case DIJOFS_RX:
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.lRx);
                break;
            case DIJOFS_RY:
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.lRy);
                break;
            case DIJOFS_RZ:
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.lRz);
                break;
            case DIJOFS_SLIDER(0):
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.rglSlider[0]);
                break;
            case DIJOFS_SLIDER(1):
                SDL_PrivateJoystickAxis_Int(joystick, in->num,
                                            (Sint16) state.rglSlider[1]);
                break;
            }

            break;

        case BUTTON:
            SDL_PrivateJoystickButton_Int(joystick, in->num,
                                          (Uint8) (state.
                                                   rgbButtons[in->ofs -
                                                              DIJOFS_BUTTON0]
                                                   ? SDL_PRESSED :
                                                   SDL_RELEASED));
            break;
        case HAT:
            {
                Uint8 pos = TranslatePOV(state.rgdwPOV[in->ofs -
                                                       DIJOFS_POV(0)]);
                SDL_PrivateJoystickHat_Int(joystick, in->num, pos);
                break;
            }
        }
    }
}

void
SDL_SYS_JoystickUpdate_Buffered(SDL_Joystick * joystick)
{
    int i;
    HRESULT result;
    DWORD numevents;
    DIDEVICEOBJECTDATA evtbuf[INPUT_QSIZE];

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

    /* Handle the events or punt */
    if (FAILED(result))
    {
        joystick->hwdata->send_remove_event = 1;
        joystick->hwdata->removed = 1;
        return;
    }

    for (i = 0; i < (int) numevents; ++i) {
        int j;

        for (j = 0; j < joystick->hwdata->NumInputs; ++j) {
            const input_t *in = &joystick->hwdata->Inputs[j];

            if (evtbuf[i].dwOfs != in->ofs)
                continue;

            switch (in->type) {
            case AXIS:
                SDL_PrivateJoystickAxis(joystick, in->num,
                                        (Sint16) evtbuf[i].dwData);
                break;
            case BUTTON:
                SDL_PrivateJoystickButton(joystick, in->num,
                                          (Uint8) (evtbuf[i].
                                                   dwData ? SDL_PRESSED :
                                                   SDL_RELEASED));
                break;
            case HAT:
                {
                    Uint8 pos = TranslatePOV(evtbuf[i].dwData);
                    SDL_PrivateJoystickHat(joystick, in->num, pos);
                }
            }
        }
    }
}


/* Function to return > 0 if a bit array of buttons differs after applying a mask
*/
int ButtonChanged( int ButtonsNow, int ButtonsPrev, int ButtonMask )
{
    return ( ButtonsNow & ButtonMask ) != ( ButtonsPrev & ButtonMask );
}

/* Function to update the state of a XInput style joystick.
*/
void
SDL_SYS_JoystickUpdate_XInput(SDL_Joystick * joystick)
{
    HRESULT result;

    if ( !XINPUTGETSTATE )
        return;

    result = XINPUTGETSTATE( joystick->hwdata->userid, &joystick->hwdata->XInputState[joystick->hwdata->currentXInputSlot] );
    if ( result == ERROR_DEVICE_NOT_CONNECTED )
    {
        joystick->hwdata->send_remove_event = 1;
        joystick->hwdata->removed = 1;
        return;
    }

    /* only fire events if the data changed from last time */
    if ( joystick->hwdata->XInputState[joystick->hwdata->currentXInputSlot].dwPacketNumber != 0
        && joystick->hwdata->XInputState[joystick->hwdata->currentXInputSlot].dwPacketNumber != joystick->hwdata->XInputState[joystick->hwdata->currentXInputSlot^1].dwPacketNumber )
    {
        XINPUT_STATE_EX *pXInputState = &joystick->hwdata->XInputState[joystick->hwdata->currentXInputSlot];
        XINPUT_STATE_EX *pXInputStatePrev = &joystick->hwdata->XInputState[joystick->hwdata->currentXInputSlot ^ 1];

        /* !!! FIXME: why isn't this just using SDL_PrivateJoystickAxis_Int()? */
        SDL_PrivateJoystickAxis( joystick, 0, (Sint16)pXInputState->Gamepad.sThumbLX );
        SDL_PrivateJoystickAxis( joystick, 1, (Sint16)(-SDL_max(-32767, pXInputState->Gamepad.sThumbLY)) );
        SDL_PrivateJoystickAxis( joystick, 2, (Sint16)pXInputState->Gamepad.sThumbRX );
        SDL_PrivateJoystickAxis( joystick, 3, (Sint16)(-SDL_max(-32767, pXInputState->Gamepad.sThumbRY)) );
        SDL_PrivateJoystickAxis( joystick, 4, (Sint16)(((int)pXInputState->Gamepad.bLeftTrigger*65535/255) - 32768));
        SDL_PrivateJoystickAxis( joystick, 5, (Sint16)(((int)pXInputState->Gamepad.bRightTrigger*65535/255) - 32768));

        /* !!! FIXME: why isn't this just using SDL_PrivateJoystickButton_Int(), instead of keeping these two alternating state buffers? */
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_UP ) )
            SDL_PrivateJoystickButton(joystick, 0, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP ? SDL_PRESSED :  SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_DOWN ) )
            SDL_PrivateJoystickButton(joystick, 1, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN ? SDL_PRESSED :    SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_LEFT ) )
            SDL_PrivateJoystickButton(joystick, 2, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT ? SDL_PRESSED :    SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_RIGHT ) )
            SDL_PrivateJoystickButton(joystick, 3, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_START ) )
            SDL_PrivateJoystickButton(joystick, 4, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_START ? SDL_PRESSED :    SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_BACK ) )
            SDL_PrivateJoystickButton(joystick, 5, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? SDL_PRESSED : SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_THUMB ) )
            SDL_PrivateJoystickButton(joystick, 6, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_THUMB ) )
            SDL_PrivateJoystickButton(joystick, 7, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB ? SDL_PRESSED :  SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER ) )
            SDL_PrivateJoystickButton(joystick, 8, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? SDL_PRESSED :    SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER ) )
            SDL_PrivateJoystickButton(joystick, 9, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_A ) )
            SDL_PrivateJoystickButton(joystick, 10, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_A ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_B ) )
            SDL_PrivateJoystickButton(joystick, 11, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_B ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_X ) )
            SDL_PrivateJoystickButton(joystick, 12, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_X ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_Y ) )
            SDL_PrivateJoystickButton(joystick, 13, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_Y ? SDL_PRESSED :   SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons,  0x400 ) )
            SDL_PrivateJoystickButton(joystick, 14, pXInputState->Gamepad.wButtons & 0x400 ? SDL_PRESSED :  SDL_RELEASED ); /* 0x400 is the undocumented code for the guide button */

        joystick->hwdata->currentXInputSlot ^= 1;

    }
}


static Uint8
TranslatePOV(DWORD value)
{
    const int HAT_VALS[] = {
        SDL_HAT_UP,
        SDL_HAT_UP | SDL_HAT_RIGHT,
        SDL_HAT_RIGHT,
        SDL_HAT_DOWN | SDL_HAT_RIGHT,
        SDL_HAT_DOWN,
        SDL_HAT_DOWN | SDL_HAT_LEFT,
        SDL_HAT_LEFT,
        SDL_HAT_UP | SDL_HAT_LEFT
    };

    if (LOWORD(value) == 0xFFFF)
        return SDL_HAT_CENTERED;

    /* Round the value up: */
    value += 4500 / 2;
    value %= 36000;
    value /= 4500;

    if (value >= 8)
        return SDL_HAT_CENTERED;        /* shouldn't happen */

    return HAT_VALS[value];
}

/* SDL_PrivateJoystick* doesn't discard duplicate events, so we need to
 * do it. */
/* !!! FIXME: SDL_PrivateJoystickAxis _does_ discard duplicate events now. Ditch this code. */
static int
SDL_PrivateJoystickAxis_Int(SDL_Joystick * joystick, Uint8 axis, Sint16 value)
{
    if (joystick->axes[axis] != value)
        return SDL_PrivateJoystickAxis(joystick, axis, value);
    return 0;
}

static int
SDL_PrivateJoystickHat_Int(SDL_Joystick * joystick, Uint8 hat, Uint8 value)
{
    if (joystick->hats[hat] != value)
        return SDL_PrivateJoystickHat(joystick, hat, value);
    return 0;
}

static int
SDL_PrivateJoystickButton_Int(SDL_Joystick * joystick, Uint8 button,
                              Uint8 state)
{
    if (joystick->buttons[button] != state)
        return SDL_PrivateJoystickButton(joystick, button, state);
    return 0;
}

void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    HRESULT result;

    if ( joystick->closed || !joystick->hwdata )
        return;

    if (joystick->hwdata->bXInputDevice)
    {
        SDL_SYS_JoystickUpdate_XInput(joystick);
    }
    else
    {
        result = IDirectInputDevice8_Poll(joystick->hwdata->InputDevice);
        if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
            IDirectInputDevice8_Acquire(joystick->hwdata->InputDevice);
            IDirectInputDevice8_Poll(joystick->hwdata->InputDevice);
        }

        if (joystick->hwdata->buffered)
            SDL_SYS_JoystickUpdate_Buffered(joystick);
        else
            SDL_SYS_JoystickUpdate_Polled(joystick);
    }

    if ( joystick->hwdata->removed )
    {
        joystick->closed = 1;
        joystick->uncentered = 1;
    }
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    if (!joystick->hwdata->bXInputDevice) {
        IDirectInputDevice8_Unacquire(joystick->hwdata->InputDevice);
        IDirectInputDevice8_Release(joystick->hwdata->InputDevice);
    }

    /* free system specific hardware data */
    SDL_free(joystick->hwdata);

    joystick->closed = 1;
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    JoyStick_DeviceData *device = SYS_Joystick;

    while ( device )
    {
        JoyStick_DeviceData *device_next = device->pNext;
        SDL_free(device->joystickname);
        SDL_free(device);
        device = device_next;
    }
    SYS_Joystick = NULL;

    if ( s_threadJoystick )
    {
        SDL_LockMutex( s_mutexJoyStickEnum );
        s_bJoystickThreadQuit = SDL_TRUE;
        SDL_CondBroadcast( s_condJoystickThread ); /* signal the joystick thread to quit */
        SDL_UnlockMutex( s_mutexJoyStickEnum );
        SDL_WaitThread( s_threadJoystick, NULL ); /* wait for it to bugger off */

        SDL_DestroyMutex( s_mutexJoyStickEnum );
        SDL_DestroyCond( s_condJoystickThread );
        s_condJoystickThread= NULL;
        s_mutexJoyStickEnum = NULL;
        s_threadJoystick = NULL;
    }

    if (dinput != NULL) {
        IDirectInput8_Release(dinput);
        dinput = NULL;
    }

    if (coinitialized) {
        WIN_CoUninitialize();
        coinitialized = SDL_FALSE;
    }

    if (s_bXInputEnabled) {
        WIN_UnloadXInputDLL();
    }
}

/* return the stable device guid for this device index */
SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID( int device_index )
{
    JoyStick_DeviceData *device = SYS_Joystick;
    int index;

    for (index = device_index; index > 0; index--)
        device = device->pNext;

    return device->guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
    return joystick->hwdata->guid;
}

/* return SDL_TRUE if this device is using XInput */
SDL_bool SDL_SYS_IsXInputDeviceIndex(int device_index)
{
    JoyStick_DeviceData *device = SYS_Joystick;
    int index;

    for (index = device_index; index > 0; index--)
        device = device->pNext;

    return device->bXInputDevice;
}

/* return SDL_TRUE if this device was opened with XInput */
SDL_bool SDL_SYS_IsXInputJoystick(SDL_Joystick * joystick)
{
	return joystick->hwdata->bXInputDevice;
}

#endif /* SDL_JOYSTICK_DINPUT */

/* vi: set ts=4 sw=4 expandtab: */
