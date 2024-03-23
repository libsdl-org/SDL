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

/* This is the joystick API for Simple DirectMedia Layer */

#include "SDL_sysjoystick.h"
#include "../SDL_hints_c.h"
#include "SDL_gamepad_c.h"
#include "SDL_joystick_c.h"
#include "SDL_steam_virtual_gamepad.h"

#include "../events/SDL_events_c.h"
#include "../video/SDL_sysvideo.h"
#include "../sensor/SDL_sensor_c.h"
#include "hidapi/SDL_hidapijoystick_c.h"

/* This is included in only one place because it has a large static list of controllers */
#include "controller_type.h"

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
/* Needed for checking for input remapping programs */
#include "../core/windows/SDL_windows.h"

#undef UNICODE /* We want ASCII functions */
#include <tlhelp32.h>
#endif

#ifdef SDL_JOYSTICK_VIRTUAL
#include "./virtual/SDL_virtualjoystick_c.h"
#endif

static SDL_JoystickDriver *SDL_joystick_drivers[] = {
#ifdef SDL_JOYSTICK_HIDAPI /* Highest priority driver for supported devices */
    &SDL_HIDAPI_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_GAMEINPUT /* Higher priority than other Windows drivers */
    &SDL_GAMEINPUT_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_RAWINPUT
    &SDL_RAWINPUT_JoystickDriver,
#endif
#if defined(SDL_JOYSTICK_DINPUT) || defined(SDL_JOYSTICK_XINPUT)
    &SDL_WINDOWS_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_WGI
    &SDL_WGI_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_WINMM
    &SDL_WINMM_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_LINUX
    &SDL_LINUX_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_IOKIT
    &SDL_DARWIN_JoystickDriver,
#endif
#if (defined(SDL_PLATFORM_MACOS) || defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_TVOS)) && !defined(SDL_JOYSTICK_DISABLED)
    &SDL_IOS_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_ANDROID
    &SDL_ANDROID_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_EMSCRIPTEN
    &SDL_EMSCRIPTEN_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_HAIKU
    &SDL_HAIKU_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_USBHID /* !!! FIXME: "USBHID" is a generic name, and doubly-confusing with HIDAPI next to it. This is the *BSD interface, rename this. */
    &SDL_BSD_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_PS2
    &SDL_PS2_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_PSP
    &SDL_PSP_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_VIRTUAL
    &SDL_VIRTUAL_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_VITA
    &SDL_VITA_JoystickDriver,
#endif
#ifdef SDL_JOYSTICK_N3DS
    &SDL_N3DS_JoystickDriver
#endif
#if defined(SDL_JOYSTICK_DUMMY) || defined(SDL_JOYSTICK_DISABLED)
        &SDL_DUMMY_JoystickDriver
#endif
};

#ifndef SDL_THREAD_SAFETY_ANALYSIS
static
#endif
SDL_Mutex *SDL_joystick_lock = NULL; /* This needs to support recursive locks */
static SDL_AtomicInt SDL_joystick_lock_pending;
static int SDL_joysticks_locked;
static SDL_bool SDL_joysticks_initialized;
static SDL_bool SDL_joysticks_quitting;
static SDL_bool SDL_joystick_being_added;
static SDL_Joystick *SDL_joysticks SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static int SDL_joystick_player_count SDL_GUARDED_BY(SDL_joystick_lock) = 0;
static SDL_JoystickID *SDL_joystick_players SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static SDL_bool SDL_joystick_allows_background_events = SDL_FALSE;
char SDL_joystick_magic;

static Uint32 initial_arcadestick_devices[] = {
    MAKE_VIDPID(0x0079, 0x181a), /* Venom Arcade Stick */
    MAKE_VIDPID(0x0079, 0x181b), /* Venom Arcade Stick */
    MAKE_VIDPID(0x0c12, 0x0ef6), /* Hitbox Arcade Stick */
    MAKE_VIDPID(0x0e6f, 0x0109), /* PDP Versus Fighting Pad */
    MAKE_VIDPID(0x0f0d, 0x0016), /* Hori Real Arcade Pro.EX */
    MAKE_VIDPID(0x0f0d, 0x001b), /* Hori Real Arcade Pro VX */
    MAKE_VIDPID(0x0f0d, 0x0063), /* Hori Real Arcade Pro Hayabusa (USA) Xbox One */
    MAKE_VIDPID(0x0f0d, 0x006a), /* Real Arcade Pro 4 */
    MAKE_VIDPID(0x0f0d, 0x0078), /* Hori Real Arcade Pro V Kai Xbox One */
    MAKE_VIDPID(0x0f0d, 0x008a), /* HORI Real Arcade Pro 4 */
    MAKE_VIDPID(0x0f0d, 0x008c), /* Hori Real Arcade Pro 4 */
    MAKE_VIDPID(0x0f0d, 0x00aa), /* HORI Real Arcade Pro V Hayabusa in Switch Mode */
    MAKE_VIDPID(0x0f0d, 0x00ed), /* Hori Fighting Stick mini 4 kai */
    MAKE_VIDPID(0x0f0d, 0x011c), /* Hori Fighting Stick α in PS4 Mode */
    MAKE_VIDPID(0x0f0d, 0x011e), /* Hori Fighting Stick α in PC Mode  */
    MAKE_VIDPID(0x0f0d, 0x0184), /* Hori Fighting Stick α in PS5 Mode */
    MAKE_VIDPID(0x146b, 0x0604), /* NACON Daija Arcade Stick */
    MAKE_VIDPID(0x1532, 0x0a00), /* Razer Atrox Arcade Stick */
    MAKE_VIDPID(0x1bad, 0xf03d), /* Street Fighter IV Arcade Stick TE - Chun Li */
    MAKE_VIDPID(0x1bad, 0xf502), /* Hori Real Arcade Pro.VX SA */
    MAKE_VIDPID(0x1bad, 0xf504), /* Hori Real Arcade Pro. EX */
    MAKE_VIDPID(0x1bad, 0xf506), /* Hori Real Arcade Pro.EX Premium VLX */
    MAKE_VIDPID(0x20d6, 0xa715), /* PowerA Nintendo Switch Fusion Arcade Stick */
    MAKE_VIDPID(0x24c6, 0x5000), /* Razer Atrox Arcade Stick */
    MAKE_VIDPID(0x24c6, 0x5501), /* Hori Real Arcade Pro VX-SA */
    MAKE_VIDPID(0x24c6, 0x550e), /* Hori Real Arcade Pro V Kai 360 */
    MAKE_VIDPID(0x2c22, 0x2300), /* Qanba Obsidian Arcade Joystick in PS4 Mode */
    MAKE_VIDPID(0x2c22, 0x2302), /* Qanba Obsidian Arcade Joystick in PS3 Mode */
    MAKE_VIDPID(0x2c22, 0x2303), /* Qanba Obsidian Arcade Joystick in PC Mode */
    MAKE_VIDPID(0x2c22, 0x2500), /* Qanba Dragon Arcade Joystick in PS4 Mode */
    MAKE_VIDPID(0x2c22, 0x2502), /* Qanba Dragon Arcade Joystick in PS3 Mode */
    MAKE_VIDPID(0x2c22, 0x2503), /* Qanba Dragon Arcade Joystick in PC Mode */
};
static SDL_vidpid_list arcadestick_devices = {
    SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES, 0, 0, NULL,
    SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_arcadestick_devices), initial_arcadestick_devices,
    SDL_FALSE
};

/* This list is taken from:
   https://raw.githubusercontent.com/denilsonsa/udev-joystick-blacklist/master/generate_rules.py
 */
static Uint32 initial_blacklist_devices[] = {
    /* Microsoft Microsoft Wireless Optical Desktop 2.10 */
    /* Microsoft Wireless Desktop - Comfort Edition */
    MAKE_VIDPID(0x045e, 0x009d),

    /* Microsoft Microsoft Digital Media Pro Keyboard */
    /* Microsoft Corp. Digital Media Pro Keyboard */
    MAKE_VIDPID(0x045e, 0x00b0),

    /* Microsoft Microsoft Digital Media Keyboard */
    /* Microsoft Corp. Digital Media Keyboard 1.0A */
    MAKE_VIDPID(0x045e, 0x00b4),

    /* Microsoft Microsoft Digital Media Keyboard 3000 */
    MAKE_VIDPID(0x045e, 0x0730),

    /* Microsoft Microsoft 2.4GHz Transceiver v6.0 */
    /* Microsoft Microsoft 2.4GHz Transceiver v8.0 */
    /* Microsoft Corp. Nano Transceiver v1.0 for Bluetooth */
    /* Microsoft Wireless Mobile Mouse 1000 */
    /* Microsoft Wireless Desktop 3000 */
    MAKE_VIDPID(0x045e, 0x0745),

    /* Microsoft SideWinder(TM) 2.4GHz Transceiver */
    MAKE_VIDPID(0x045e, 0x0748),

    /* Microsoft Corp. Wired Keyboard 600 */
    MAKE_VIDPID(0x045e, 0x0750),

    /* Microsoft Corp. Sidewinder X4 keyboard */
    MAKE_VIDPID(0x045e, 0x0768),

    /* Microsoft Corp. Arc Touch Mouse Transceiver */
    MAKE_VIDPID(0x045e, 0x0773),

    /* Microsoft 2.4GHz Transceiver v9.0 */
    /* Microsoft Nano Transceiver v2.1 */
    /* Microsoft Sculpt Ergonomic Keyboard (5KV-00001) */
    MAKE_VIDPID(0x045e, 0x07a5),

    /* Microsoft Nano Transceiver v1.0 */
    /* Microsoft Wireless Keyboard 800 */
    MAKE_VIDPID(0x045e, 0x07b2),

    /* Microsoft Nano Transceiver v2.0 */
    MAKE_VIDPID(0x045e, 0x0800),

    MAKE_VIDPID(0x046d, 0xc30a), /* Logitech, Inc. iTouch Composite keboard */

    MAKE_VIDPID(0x04d9, 0xa0df), /* Tek Syndicate Mouse (E-Signal USB Gaming Mouse) */

    /* List of Wacom devices at: http://linuxwacom.sourceforge.net/wiki/index.php/Device_IDs */
    MAKE_VIDPID(0x056a, 0x0010), /* Wacom ET-0405 Graphire */
    MAKE_VIDPID(0x056a, 0x0011), /* Wacom ET-0405A Graphire2 (4x5) */
    MAKE_VIDPID(0x056a, 0x0012), /* Wacom ET-0507A Graphire2 (5x7) */
    MAKE_VIDPID(0x056a, 0x0013), /* Wacom CTE-430 Graphire3 (4x5) */
    MAKE_VIDPID(0x056a, 0x0014), /* Wacom CTE-630 Graphire3 (6x8) */
    MAKE_VIDPID(0x056a, 0x0015), /* Wacom CTE-440 Graphire4 (4x5) */
    MAKE_VIDPID(0x056a, 0x0016), /* Wacom CTE-640 Graphire4 (6x8) */
    MAKE_VIDPID(0x056a, 0x0017), /* Wacom CTE-450 Bamboo Fun (4x5) */
    MAKE_VIDPID(0x056a, 0x0018), /* Wacom CTE-650 Bamboo Fun 6x8 */
    MAKE_VIDPID(0x056a, 0x0019), /* Wacom CTE-631 Bamboo One */
    MAKE_VIDPID(0x056a, 0x00d1), /* Wacom Bamboo Pen and Touch CTH-460 */
    MAKE_VIDPID(0x056a, 0x030e), /* Wacom Intuos Pen (S) CTL-480 */

    MAKE_VIDPID(0x09da, 0x054f), /* A4 Tech Co., G7 750 mouse */
    MAKE_VIDPID(0x09da, 0x1410), /* A4 Tech Co., Ltd Bloody AL9 mouse */
    MAKE_VIDPID(0x09da, 0x3043), /* A4 Tech Co., Ltd Bloody R8A Gaming Mouse */
    MAKE_VIDPID(0x09da, 0x31b5), /* A4 Tech Co., Ltd Bloody TL80 Terminator Laser Gaming Mouse */
    MAKE_VIDPID(0x09da, 0x3997), /* A4 Tech Co., Ltd Bloody RT7 Terminator Wireless */
    MAKE_VIDPID(0x09da, 0x3f8b), /* A4 Tech Co., Ltd Bloody V8 mouse */
    MAKE_VIDPID(0x09da, 0x51f4), /* Modecom MC-5006 Keyboard */
    MAKE_VIDPID(0x09da, 0x5589), /* A4 Tech Co., Ltd Terminator TL9 Laser Gaming Mouse */
    MAKE_VIDPID(0x09da, 0x7b22), /* A4 Tech Co., Ltd Bloody V5 */
    MAKE_VIDPID(0x09da, 0x7f2d), /* A4 Tech Co., Ltd Bloody R3 mouse */
    MAKE_VIDPID(0x09da, 0x8090), /* A4 Tech Co., Ltd X-718BK Oscar Optical Gaming Mouse */
    MAKE_VIDPID(0x09da, 0x9033), /* A4 Tech Co., X7 X-705K */
    MAKE_VIDPID(0x09da, 0x9066), /* A4 Tech Co., Sharkoon Fireglider Optical */
    MAKE_VIDPID(0x09da, 0x9090), /* A4 Tech Co., Ltd XL-730K / XL-750BK / XL-755BK Laser Mouse */
    MAKE_VIDPID(0x09da, 0x90c0), /* A4 Tech Co., Ltd X7 G800V keyboard */
    MAKE_VIDPID(0x09da, 0xf012), /* A4 Tech Co., Ltd Bloody V7 mouse */
    MAKE_VIDPID(0x09da, 0xf32a), /* A4 Tech Co., Ltd Bloody B540 keyboard */
    MAKE_VIDPID(0x09da, 0xf613), /* A4 Tech Co., Ltd Bloody V2 mouse */
    MAKE_VIDPID(0x09da, 0xf624), /* A4 Tech Co., Ltd Bloody B120 Keyboard */

    MAKE_VIDPID(0x1b1c, 0x1b3c), /* Corsair Harpoon RGB gaming mouse */

    MAKE_VIDPID(0x1d57, 0xad03), /* [T3] 2.4GHz and IR Air Mouse Remote Control */

    MAKE_VIDPID(0x1e7d, 0x2e4a), /* Roccat Tyon Mouse */

    MAKE_VIDPID(0x20a0, 0x422d), /* Winkeyless.kr Keyboards */

    MAKE_VIDPID(0x2516, 0x001f), /* Cooler Master Storm Mizar Mouse */
    MAKE_VIDPID(0x2516, 0x0028), /* Cooler Master Storm Alcor Mouse */

    /*****************************************************************/
    /* Additional entries                                            */
    /*****************************************************************/

    MAKE_VIDPID(0x04d9, 0x8008), /* OBINLB USB-HID Keyboard (Anne Pro II) */
    MAKE_VIDPID(0x04d9, 0x8009), /* OBINLB USB-HID Keyboard (Anne Pro II) */
    MAKE_VIDPID(0x04d9, 0xa292), /* OBINLB USB-HID Keyboard (Anne Pro II) */
    MAKE_VIDPID(0x04d9, 0xa293), /* OBINLB USB-HID Keyboard (Anne Pro II) */
    MAKE_VIDPID(0x1532, 0x0266), /* Razer Huntsman V2 Analog, non-functional DInput device */
    MAKE_VIDPID(0x1532, 0x0282), /* Razer Huntsman Mini Analog, non-functional DInput device */
    MAKE_VIDPID(0x26ce, 0x01a2), /* ASRock LED Controller */
    MAKE_VIDPID(0x20d6, 0x0002), /* PowerA Enhanced Wireless Controller for Nintendo Switch (charging port only) */
};
static SDL_vidpid_list blacklist_devices = {
    SDL_HINT_JOYSTICK_BLACKLIST_DEVICES, 0, 0, NULL,
    SDL_HINT_JOYSTICK_BLACKLIST_DEVICES_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_blacklist_devices), initial_blacklist_devices,
    SDL_FALSE
};

static Uint32 initial_flightstick_devices[] = {
    MAKE_VIDPID(0x044f, 0x0402), /* HOTAS Warthog Joystick */
    MAKE_VIDPID(0x0738, 0x2221), /* Saitek Pro Flight X-56 Rhino Stick */
    MAKE_VIDPID(0x044f, 0xb10a), /* ThrustMaster, Inc. T.16000M Joystick */
    MAKE_VIDPID(0x046d, 0xc215), /* Logitech Extreme 3D */
    MAKE_VIDPID(0x231d, 0x0126), /* Gunfighter Mk.III ‘Space Combat Edition’ (right) */
    MAKE_VIDPID(0x231d, 0x0127), /* Gunfighter Mk.III ‘Space Combat Edition’ (left) */
};
static SDL_vidpid_list flightstick_devices = {
    SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES, 0, 0, NULL,
    SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_flightstick_devices), initial_flightstick_devices,
    SDL_FALSE
};

static Uint32 initial_gamecube_devices[] = {
    MAKE_VIDPID(0x0e6f, 0x0185), /* PDP Wired Fight Pad Pro for Nintendo Switch */
    MAKE_VIDPID(0x20d6, 0xa711), /* PowerA Wired Controller Nintendo GameCube Style */
};
static SDL_vidpid_list gamecube_devices = {
    SDL_HINT_JOYSTICK_GAMECUBE_DEVICES, 0, 0, NULL,
    SDL_HINT_JOYSTICK_GAMECUBE_DEVICES_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_gamecube_devices), initial_gamecube_devices,
    SDL_FALSE
};

static Uint32 initial_rog_gamepad_mice[] = {
    MAKE_VIDPID(0x0b05, 0x1906), /* ROG Pugio II */
    MAKE_VIDPID(0x0b05, 0x1958), /* ROG Chakram Core Mouse */
    MAKE_VIDPID(0x0b05, 0x18e3), /* ROG Chakram (wired) Mouse */
    MAKE_VIDPID(0x0b05, 0x18e5), /* ROG Chakram (wireless) Mouse */
    MAKE_VIDPID(0x0b05, 0x1a18), /* ROG Chakram X (wired) Mouse */
    MAKE_VIDPID(0x0b05, 0x1a1a), /* ROG Chakram X (wireless) Mouse */
    MAKE_VIDPID(0x0b05, 0x1a1c), /* ROG Chakram X (Bluetooth) Mouse */
};
static SDL_vidpid_list rog_gamepad_mice = {
    SDL_HINT_ROG_GAMEPAD_MICE, 0, 0, NULL,
    SDL_HINT_ROG_GAMEPAD_MICE_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_rog_gamepad_mice), initial_rog_gamepad_mice,
    SDL_FALSE
};

static Uint32 initial_throttle_devices[] = {
    MAKE_VIDPID(0x044f, 0x0404), /* HOTAS Warthog Throttle */
    MAKE_VIDPID(0x0738, 0xa221), /* Saitek Pro Flight X-56 Rhino Throttle */
};
static SDL_vidpid_list throttle_devices = {
    SDL_HINT_JOYSTICK_THROTTLE_DEVICES, 0, 0, NULL,
    SDL_HINT_JOYSTICK_THROTTLE_DEVICES_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_throttle_devices), initial_throttle_devices,
    SDL_FALSE
};

static Uint32 initial_wheel_devices[] = {
    MAKE_VIDPID(0x0079, 0x1864), /* DragonRise Inc. Wired Wheel (active mode) (also known as PXN V900 (PS3), Superdrive SV-750, or a Genesis Seaborg 400) */
    MAKE_VIDPID(0x046d, 0xc294), /* Logitech generic wheel */
    MAKE_VIDPID(0x046d, 0xc295), /* Logitech Momo Force */
    MAKE_VIDPID(0x046d, 0xc298), /* Logitech Driving Force Pro */
    MAKE_VIDPID(0x046d, 0xc299), /* Logitech G25 */
    MAKE_VIDPID(0x046d, 0xc29a), /* Logitech Driving Force GT */
    MAKE_VIDPID(0x046d, 0xc29b), /* Logitech G27 */
    MAKE_VIDPID(0x046d, 0xc24f), /* Logitech G29 (PS3) */
    MAKE_VIDPID(0x046d, 0xc260), /* Logitech G29 (PS4) */
    MAKE_VIDPID(0x046d, 0xc261), /* Logitech G920 (initial mode) */
    MAKE_VIDPID(0x046d, 0xc262), /* Logitech G920 (active mode) */
    MAKE_VIDPID(0x046d, 0xc268), /* Logitech PRO Racing Wheel (PC mode) */
    MAKE_VIDPID(0x046d, 0xc269), /* Logitech PRO Racing Wheel (PS4/PS5 mode) */
    MAKE_VIDPID(0x046d, 0xc272), /* Logitech PRO Racing Wheel for Xbox (PC mode) */
    MAKE_VIDPID(0x046d, 0xc26d), /* Logitech G923 (Xbox) */
    MAKE_VIDPID(0x046d, 0xc26e), /* Logitech G923 */
    MAKE_VIDPID(0x046d, 0xc266), /* Logitech G923 for Playstation 4 and PC (PC mode) */
    MAKE_VIDPID(0x046d, 0xc267), /* Logitech G923 for Playstation 4 and PC (PS4 mode)*/
    MAKE_VIDPID(0x046d, 0xca03), /* Logitech Momo Racing */
    MAKE_VIDPID(0x044f, 0xb65d), /* Thrustmaster Wheel FFB */
    MAKE_VIDPID(0x044f, 0xb66d), /* Thrustmaster Wheel FFB */
    MAKE_VIDPID(0x044f, 0xb677), /* Thrustmaster T150 */
    MAKE_VIDPID(0x044f, 0xb696), /* Thrustmaster T248 */
    MAKE_VIDPID(0x044f, 0xb66e), /* Thrustmaster T300RS (normal mode) */
    MAKE_VIDPID(0x044f, 0xb66f), /* Thrustmaster T300RS (advanced mode) */
    MAKE_VIDPID(0x044f, 0xb66d), /* Thrustmaster T300RS (PS4 mode) */
    MAKE_VIDPID(0x044f, 0xb65e), /* Thrustmaster T500RS */
    MAKE_VIDPID(0x044f, 0xb664), /* Thrustmaster TX (initial mode) */
    MAKE_VIDPID(0x044f, 0xb669), /* Thrustmaster TX (active mode) */
    MAKE_VIDPID(0x0483, 0x0522), /* Simagic Wheelbase (including M10, Alpha Mini, Alpha, Alpha U) */
    MAKE_VIDPID(0x0eb7, 0x0001), /* Fanatec ClubSport Wheel Base V2 */
    MAKE_VIDPID(0x0eb7, 0x0004), /* Fanatec ClubSport Wheel Base V2.5 */
    MAKE_VIDPID(0x0eb7, 0x0005), /* Fanatec CSL Elite Wheel Base+ (PS4) */
    MAKE_VIDPID(0x0eb7, 0x0006), /* Fanatec Podium Wheel Base DD1 */
    MAKE_VIDPID(0x0eb7, 0x0007), /* Fanatec Podium Wheel Base DD2 */
    MAKE_VIDPID(0x0eb7, 0x0011), /* Fanatec Forza Motorsport (CSR Wheel / CSR Elite Wheel) */
    MAKE_VIDPID(0x0eb7, 0x0020), /* Fanatec generic wheel / CSL DD / GT DD Pro */
    MAKE_VIDPID(0x0eb7, 0x0197), /* Fanatec Porsche Wheel (Turbo / GT3 RS / Turbo S / GT3 V2 / GT2) */
    MAKE_VIDPID(0x0eb7, 0x038e), /* Fanatec ClubSport Wheel Base V1 */
    MAKE_VIDPID(0x0eb7, 0x0e03), /* Fanatec CSL Elite Wheel Base */
    MAKE_VIDPID(0x11ff, 0x0511), /* DragonRise Inc. Wired Wheel (initial mode) (also known as PXN V900 (PS3), Superdrive SV-750, or a Genesis Seaborg 400) */
    MAKE_VIDPID(0x1209, 0xffb0), /* Generic FFBoard OpenFFBoard universal forcefeedback wheel */
    MAKE_VIDPID(0x2433, 0xf300), /* Asetek SimSports Invicta Wheelbase */
    MAKE_VIDPID(0x2433, 0xf301), /* Asetek SimSports Forte Wheelbase */
    MAKE_VIDPID(0x2433, 0xf303), /* Asetek SimSports La Prima Wheelbase */
    MAKE_VIDPID(0x2433, 0xf306), /* Asetek SimSports Tony Kannan Wheelbase */
};
static SDL_vidpid_list wheel_devices = {
    SDL_HINT_JOYSTICK_WHEEL_DEVICES, 0, 0, NULL,
    SDL_HINT_JOYSTICK_WHEEL_DEVICES_EXCLUDED, 0, 0, NULL,
    SDL_arraysize(initial_wheel_devices), initial_wheel_devices,
    SDL_FALSE
};

static Uint32 initial_zero_centered_devices[] = {
    MAKE_VIDPID(0x0e8f, 0x3013), /* HuiJia SNES USB adapter */
    MAKE_VIDPID(0x05a0, 0x3232), /* 8Bitdo Zero Gamepad */
};
static SDL_vidpid_list zero_centered_devices = {
    SDL_HINT_JOYSTICK_ZERO_CENTERED_DEVICES, 0, 0, NULL,
    NULL, 0, 0, NULL,
    SDL_arraysize(initial_zero_centered_devices), initial_zero_centered_devices,
    SDL_FALSE
};

#define CHECK_JOYSTICK_MAGIC(joystick, retval)             \
    if (!joystick || joystick->magic != &SDL_joystick_magic) { \
        SDL_InvalidParamError("joystick");                 \
        SDL_UnlockJoysticks();                             \
        return retval;                                     \
    }

SDL_bool SDL_JoysticksInitialized(void)
{
    return SDL_joysticks_initialized;
}

SDL_bool SDL_JoysticksQuitting(void)
{
    return SDL_joysticks_quitting;
}

void SDL_LockJoysticks(void)
{
    (void)SDL_AtomicIncRef(&SDL_joystick_lock_pending);
    SDL_LockMutex(SDL_joystick_lock);
    (void)SDL_AtomicDecRef(&SDL_joystick_lock_pending);

    ++SDL_joysticks_locked;
}

void SDL_UnlockJoysticks(void)
{
    SDL_bool last_unlock = SDL_FALSE;

    --SDL_joysticks_locked;

    if (!SDL_joysticks_initialized) {
        /* NOTE: There's a small window here where another thread could lock the mutex after we've checked for pending locks */
        if (!SDL_joysticks_locked && SDL_AtomicGet(&SDL_joystick_lock_pending) == 0) {
            last_unlock = SDL_TRUE;
        }
    }

    /* The last unlock after joysticks are uninitialized will cleanup the mutex,
     * allowing applications to lock joysticks while reinitializing the system.
     */
    if (last_unlock) {
        SDL_Mutex *joystick_lock = SDL_joystick_lock;

        SDL_LockMutex(joystick_lock);
        {
            SDL_UnlockMutex(SDL_joystick_lock);

            SDL_joystick_lock = NULL;
        }
        SDL_UnlockMutex(joystick_lock);
        SDL_DestroyMutex(joystick_lock);
    } else {
        SDL_UnlockMutex(SDL_joystick_lock);
    }
}

SDL_bool SDL_JoysticksLocked(void)
{
    return (SDL_joysticks_locked > 0);
}

void SDL_AssertJoysticksLocked(void)
{
    SDL_assert(SDL_JoysticksLocked());
}

/*
 * Get the driver and device index for a joystick instance ID
 * This should be called while the joystick lock is held, to prevent another thread from updating the list
 */
static SDL_bool SDL_GetDriverAndJoystickIndex(SDL_JoystickID instance_id, SDL_JoystickDriver **driver, int *driver_index)
{
    int i, num_joysticks, device_index;

    SDL_AssertJoysticksLocked();

    if (instance_id > 0) {
        for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
            num_joysticks = SDL_joystick_drivers[i]->GetCount();
            for (device_index = 0; device_index < num_joysticks; ++device_index) {
                SDL_JoystickID joystick_id = SDL_joystick_drivers[i]->GetDeviceInstanceID(device_index);
                if (joystick_id == instance_id) {
                    *driver = SDL_joystick_drivers[i];
                    *driver_index = device_index;
                    return SDL_TRUE;
                }
            }
        }
    }

    SDL_SetError("Joystick %" SDL_PRIu32 " not found", instance_id);
    return SDL_FALSE;
}

static int SDL_FindFreePlayerIndex(void)
{
    int player_index;

    SDL_AssertJoysticksLocked();

    for (player_index = 0; player_index < SDL_joystick_player_count; ++player_index) {
        if (SDL_joystick_players[player_index] == 0) {
            break;
        }
    }
    return player_index;
}

static int SDL_GetPlayerIndexForJoystickID(SDL_JoystickID instance_id)
{
    int player_index;

    SDL_AssertJoysticksLocked();

    for (player_index = 0; player_index < SDL_joystick_player_count; ++player_index) {
        if (instance_id == SDL_joystick_players[player_index]) {
            break;
        }
    }
    if (player_index == SDL_joystick_player_count) {
        player_index = -1;
    }
    return player_index;
}

static SDL_JoystickID SDL_GetJoystickIDForPlayerIndex(int player_index)
{
    SDL_AssertJoysticksLocked();

    if (player_index < 0 || player_index >= SDL_joystick_player_count) {
        return 0;
    }
    return SDL_joystick_players[player_index];
}

static SDL_bool SDL_SetJoystickIDForPlayerIndex(int player_index, SDL_JoystickID instance_id)
{
    SDL_JoystickID existing_instance = SDL_GetJoystickIDForPlayerIndex(player_index);
    SDL_JoystickDriver *driver;
    int device_index;
    int existing_player_index;

    SDL_AssertJoysticksLocked();

    if (player_index >= SDL_joystick_player_count) {
        SDL_JoystickID *new_players = (SDL_JoystickID *)SDL_realloc(SDL_joystick_players, (player_index + 1) * sizeof(*SDL_joystick_players));
        if (!new_players) {
            return SDL_FALSE;
        }

        SDL_joystick_players = new_players;
        SDL_memset(&SDL_joystick_players[SDL_joystick_player_count], 0, (player_index - SDL_joystick_player_count + 1) * sizeof(SDL_joystick_players[0]));
        SDL_joystick_player_count = player_index + 1;
    } else if (player_index >= 0 && SDL_joystick_players[player_index] == instance_id) {
        /* Joystick is already assigned the requested player index */
        return SDL_TRUE;
    }

    /* Clear the old player index */
    existing_player_index = SDL_GetPlayerIndexForJoystickID(instance_id);
    if (existing_player_index >= 0) {
        SDL_joystick_players[existing_player_index] = 0;
    }

    if (player_index >= 0) {
        SDL_joystick_players[player_index] = instance_id;
    }

    /* Update the driver with the new index */
    if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        driver->SetDevicePlayerIndex(device_index, player_index);
    }

    /* Move any existing joystick to another slot */
    if (existing_instance > 0) {
        SDL_SetJoystickIDForPlayerIndex(SDL_FindFreePlayerIndex(), existing_instance);
    }
    return SDL_TRUE;
}

static void SDLCALL SDL_JoystickAllowBackgroundEventsChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    if (SDL_GetStringBoolean(hint, SDL_FALSE)) {
        SDL_joystick_allows_background_events = SDL_TRUE;
    } else {
        SDL_joystick_allows_background_events = SDL_FALSE;
    }
}

int SDL_InitJoysticks(void)
{
    int i, status;

    /* Create the joystick list lock */
    if (SDL_joystick_lock == NULL) {
        SDL_joystick_lock = SDL_CreateMutex();
    }

    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
        return -1;
    }

    SDL_LockJoysticks();

    SDL_joysticks_initialized = SDL_TRUE;

    SDL_InitGamepadMappings();

    SDL_LoadVIDPIDList(&arcadestick_devices);
    SDL_LoadVIDPIDList(&blacklist_devices);
    SDL_LoadVIDPIDList(&flightstick_devices);
    SDL_LoadVIDPIDList(&gamecube_devices);
    SDL_LoadVIDPIDList(&rog_gamepad_mice);
    SDL_LoadVIDPIDList(&throttle_devices);
    SDL_LoadVIDPIDList(&wheel_devices);
    SDL_LoadVIDPIDList(&zero_centered_devices);

    /* See if we should allow joystick events while in the background */
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
                        SDL_JoystickAllowBackgroundEventsChanged, NULL);

    SDL_InitSteamVirtualGamepadInfo();

    status = -1;
    for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
        if (SDL_joystick_drivers[i]->Init() >= 0) {
            status = 0;
        }
    }
    SDL_UnlockJoysticks();

    if (status < 0) {
        SDL_QuitJoysticks();
    }

    return status;
}

SDL_bool SDL_JoysticksOpened(void)
{
    SDL_bool opened;

    SDL_LockJoysticks();
    {
        if (SDL_joysticks != NULL) {
            opened = SDL_TRUE;
        } else {
            opened = SDL_FALSE;
        }
    }
    SDL_UnlockJoysticks();

    return opened;
}

SDL_bool SDL_JoystickHandledByAnotherDriver(struct SDL_JoystickDriver *driver, Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    int i;
    SDL_bool result = SDL_FALSE;

    SDL_LockJoysticks();
    {
        for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
            if (driver == SDL_joystick_drivers[i]) {
                /* Higher priority drivers do not have this device */
                break;
            }
            if (SDL_joystick_drivers[i]->IsDevicePresent(vendor_id, product_id, version, name)) {
                result = SDL_TRUE;
                break;
            }
        }
    }
    SDL_UnlockJoysticks();

    return result;
}

SDL_bool SDL_HasJoystick(void)
{
    int i;
    int total_joysticks = 0;

    SDL_LockJoysticks();
    {
        for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
            total_joysticks += SDL_joystick_drivers[i]->GetCount();
        }
    }
    SDL_UnlockJoysticks();

    if (total_joysticks > 0) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

SDL_JoystickID *SDL_GetJoysticks(int *count)
{
    int i, num_joysticks, device_index;
    int joystick_index = 0, total_joysticks = 0;
    SDL_JoystickID *joysticks;

    SDL_LockJoysticks();
    {
        for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
            total_joysticks += SDL_joystick_drivers[i]->GetCount();
        }

        joysticks = (SDL_JoystickID *)SDL_malloc((total_joysticks + 1) * sizeof(*joysticks));
        if (joysticks) {
            if (count) {
                *count = total_joysticks;
            }

            for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
                num_joysticks = SDL_joystick_drivers[i]->GetCount();
                for (device_index = 0; device_index < num_joysticks; ++device_index) {
                    SDL_assert(joystick_index < total_joysticks);
                    joysticks[joystick_index] = SDL_joystick_drivers[i]->GetDeviceInstanceID(device_index);
                    SDL_assert(joysticks[joystick_index] > 0);
                    ++joystick_index;
                }
            }
            SDL_assert(joystick_index == total_joysticks);
            joysticks[joystick_index] = 0;
        } else {
            if (count) {
                *count = 0;
            }
        }
    }
    SDL_UnlockJoysticks();

    return joysticks;
}

const SDL_SteamVirtualGamepadInfo *SDL_GetJoystickInstanceVirtualGamepadInfo(SDL_JoystickID instance_id)
{
    SDL_JoystickDriver *driver;
    int device_index;
    const SDL_SteamVirtualGamepadInfo *info = NULL;

    if (SDL_SteamVirtualGamepadEnabled() &&
        SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        info = SDL_GetSteamVirtualGamepadInfo(driver->GetDeviceSteamVirtualGamepadSlot(device_index));
    }
    return info;
}

/*
 * Get the implementation dependent name of a joystick
 */
const char *SDL_GetJoystickInstanceName(SDL_JoystickID instance_id)
{
    SDL_JoystickDriver *driver;
    int device_index;
    const char *name = NULL;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();
    info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
    if (info) {
        name = info->name;
    } else if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        name = driver->GetDeviceName(device_index);
    }
    SDL_UnlockJoysticks();

    /* FIXME: Really we should reference count this name so it doesn't go away after unlock */
    return name;
}

/*
 * Get the implementation dependent path of a joystick
 */
const char *SDL_GetJoystickInstancePath(SDL_JoystickID instance_id)
{
    SDL_JoystickDriver *driver;
    int device_index;
    const char *path = NULL;

    SDL_LockJoysticks();
    if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        path = driver->GetDevicePath(device_index);
    }
    SDL_UnlockJoysticks();

    /* FIXME: Really we should reference count this path so it doesn't go away after unlock */
    if (!path) {
        SDL_Unsupported();
    }
    return path;
}

/*
 *  Get the player index of a joystick, or -1 if it's not available
 */
int SDL_GetJoystickInstancePlayerIndex(SDL_JoystickID instance_id)
{
    int player_index;

    SDL_LockJoysticks();
    player_index = SDL_GetPlayerIndexForJoystickID(instance_id);
    SDL_UnlockJoysticks();

    return player_index;
}

/*
 * Return true if this joystick is known to have all axes centered at zero
 * This isn't generally needed unless the joystick never generates an initial axis value near zero,
 * e.g. it's emulating axes with digital buttons
 */
static SDL_bool SDL_JoystickAxesCenteredAtZero(SDL_Joystick *joystick)
{
#ifdef SDL_PLATFORM_WINRT
    return SDL_TRUE;
#else
    /*printf("JOYSTICK '%s' VID/PID 0x%.4x/0x%.4x AXES: %d\n", joystick->name, vendor, product, joystick->naxes);*/

    if (joystick->naxes == 2) {
        /* Assume D-pad or thumbstick style axes are centered at 0 */
        return SDL_TRUE;
    }

    return SDL_VIDPIDInList(SDL_GetJoystickVendor(joystick), SDL_GetJoystickProduct(joystick), &zero_centered_devices);
#endif /* SDL_PLATFORM_WINRT */
}

static SDL_bool IsROGAlly(SDL_Joystick *joystick)
{
    Uint16 vendor, product;
    SDL_JoystickGUID guid = SDL_GetJoystickGUID(joystick);

    /* The ROG Ally controller spoofs an Xbox 360 controller */
    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);
    if (vendor == USB_VENDOR_MICROSOFT && product == USB_PRODUCT_XBOX360_WIRED_CONTROLLER) {
        /* Check to see if this system has the expected sensors */
        SDL_bool has_ally_accel = SDL_FALSE;
        SDL_bool has_ally_gyro = SDL_FALSE;

        if (SDL_InitSubSystem(SDL_INIT_SENSOR) == 0) {
            SDL_SensorID *sensors = SDL_GetSensors(NULL);
            if (sensors) {
                int i;
                for (i = 0; sensors[i]; ++i) {
                    SDL_SensorID sensor = sensors[i];

                    if (!has_ally_accel && SDL_GetSensorInstanceType(sensor) == SDL_SENSOR_ACCEL) {
                        const char *sensor_name = SDL_GetSensorInstanceName(sensor);
                        if (sensor_name && SDL_strcmp(sensor_name, "Sensor BMI320 Acc") == 0) {
                            has_ally_accel = SDL_TRUE;
                        }
                    }
                    if (!has_ally_gyro && SDL_GetSensorInstanceType(sensor) == SDL_SENSOR_GYRO) {
                        const char *sensor_name = SDL_GetSensorInstanceName(sensor);
                        if (sensor_name && SDL_strcmp(sensor_name, "Sensor BMI320 Gyr") == 0) {
                            has_ally_gyro = SDL_TRUE;
                        }
                    }
                }
                SDL_free(sensors);
            }
            SDL_QuitSubSystem(SDL_INIT_SENSOR);
        }
        if (has_ally_accel && has_ally_gyro) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static SDL_bool ShouldAttemptSensorFusion(SDL_Joystick *joystick, SDL_bool *invert_sensors)
{
    const char *hint;
    int hint_value;

    SDL_AssertJoysticksLocked();

    *invert_sensors = SDL_FALSE;

    /* The SDL controller sensor API is only available for gamepads (at the moment) */
    if (!joystick->is_gamepad) {
        return SDL_FALSE;
    }

    /* If the controller already has sensors, use those */
    if (joystick->nsensors > 0) {
        return SDL_FALSE;
    }

    hint = SDL_GetHint(SDL_HINT_GAMECONTROLLER_SENSOR_FUSION);
    hint_value = SDL_GetStringInteger(hint, -1);
    if (hint_value > 0) {
        return SDL_TRUE;
    }
    if (hint_value == 0) {
        return SDL_FALSE;
    }

    if (hint) {
        SDL_vidpid_list gamepads;
        SDL_JoystickGUID guid;
        Uint16 vendor, product;
        SDL_bool enabled;
        SDL_zero(gamepads);

        /* See if the gamepad is in our list of devices to enable */
        guid = SDL_GetJoystickGUID(joystick);
        SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);
        SDL_LoadVIDPIDListFromHints(&gamepads, hint, NULL);
        enabled = SDL_VIDPIDInList(vendor, product, &gamepads);
        SDL_FreeVIDPIDList(&gamepads);
        if (enabled) {
            return SDL_TRUE;
        }
    }

    /* See if this is another known wraparound gamepad */
    if (joystick->name &&
        (SDL_strstr(joystick->name, "Backbone One") ||
         SDL_strstr(joystick->name, "Kishi"))) {
        return SDL_TRUE;
    }
    if (IsROGAlly(joystick)) {
        /* I'm not sure if this is a Windows thing, or a quirk for ROG Ally,
         * but we need to invert the sensor data on all axes.
         */
        *invert_sensors = SDL_TRUE;
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static void AttemptSensorFusion(SDL_Joystick *joystick, SDL_bool invert_sensors)
{
    SDL_SensorID *sensors;
    unsigned int i, j;

    SDL_AssertJoysticksLocked();

    if (SDL_InitSubSystem(SDL_INIT_SENSOR) < 0) {
        return;
    }

    sensors = SDL_GetSensors(NULL);
    if (sensors) {
        for (i = 0; sensors[i]; ++i) {
            SDL_SensorID sensor = sensors[i];

            if (!joystick->accel_sensor && SDL_GetSensorInstanceType(sensor) == SDL_SENSOR_ACCEL) {
                /* Increment the sensor subsystem reference count */
                SDL_InitSubSystem(SDL_INIT_SENSOR);

                joystick->accel_sensor = sensor;
                SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 0.0f);
            }
            if (!joystick->gyro_sensor && SDL_GetSensorInstanceType(sensor) == SDL_SENSOR_GYRO) {
                /* Increment the sensor subsystem reference count */
                SDL_InitSubSystem(SDL_INIT_SENSOR);

                joystick->gyro_sensor = sensor;
                SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 0.0f);
            }
        }
        SDL_free(sensors);
    }
    SDL_QuitSubSystem(SDL_INIT_SENSOR);

    /* SDL defines sensor orientation for phones relative to the natural
       orientation, and for gamepads relative to being held in front of you.
       When a phone is being used as a gamepad, its orientation changes,
       so adjust sensor axes to match.
     */
    if (SDL_GetNaturalDisplayOrientation(SDL_GetPrimaryDisplay()) == SDL_ORIENTATION_LANDSCAPE) {
        /* When a device in landscape orientation is laid flat, the axes change
           orientation as follows:
            -X to +X becomes -X to +X
            -Y to +Y becomes +Z to -Z
            -Z to +Z becomes -Y to +Y
        */
        joystick->sensor_transform[0][0] = 1.0f;
        joystick->sensor_transform[1][2] = 1.0f;
        joystick->sensor_transform[2][1] = -1.0f;
    } else {
        /* When a device in portrait orientation is rotated left and laid flat,
           the axes change orientation as follows:
            -X to +X becomes +Z to -Z
            -Y to +Y becomes +X to -X
            -Z to +Z becomes -Y to +Y
        */
        joystick->sensor_transform[0][1] = -1.0f;
        joystick->sensor_transform[1][2] = 1.0f;
        joystick->sensor_transform[2][0] = -1.0f;
    }

    if (invert_sensors) {
        for (i = 0; i < SDL_arraysize(joystick->sensor_transform); ++i) {
            for (j = 0; j < SDL_arraysize(joystick->sensor_transform[i]); ++j) {
                joystick->sensor_transform[i][j] *= -1.0f;
            }
        }
    }
}

static void CleanupSensorFusion(SDL_Joystick *joystick)
{
    SDL_AssertJoysticksLocked();

    if (joystick->accel_sensor || joystick->gyro_sensor) {
        if (joystick->accel_sensor) {
            if (joystick->accel) {
                SDL_CloseSensor(joystick->accel);
                joystick->accel = NULL;
            }
            joystick->accel_sensor = 0;

            /* Decrement the sensor subsystem reference count */
            SDL_QuitSubSystem(SDL_INIT_SENSOR);
        }
        if (joystick->gyro_sensor) {
            if (joystick->gyro) {
                SDL_CloseSensor(joystick->gyro);
                joystick->gyro = NULL;
            }
            joystick->gyro_sensor = 0;

            /* Decrement the sensor subsystem reference count */
            SDL_QuitSubSystem(SDL_INIT_SENSOR);
        }
    }
}

/*
 * Open a joystick for use - the index passed as an argument refers to
 * the N'th joystick on the system.  This index is the value which will
 * identify this joystick in future joystick events.
 *
 * This function returns a joystick identifier, or NULL if an error occurred.
 */
SDL_Joystick *SDL_OpenJoystick(SDL_JoystickID instance_id)
{
    SDL_JoystickDriver *driver;
    int device_index;
    SDL_Joystick *joystick;
    SDL_Joystick *joysticklist;
    const char *joystickname = NULL;
    const char *joystickpath = NULL;
    SDL_JoystickPowerLevel initial_power_level;
    SDL_bool invert_sensors = SDL_FALSE;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();

    if (!SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        SDL_UnlockJoysticks();
        return NULL;
    }

    joysticklist = SDL_joysticks;
    /* If the joystick is already open, return it
     * it is important that we have a single joystick for each instance id
     */
    while (joysticklist) {
        if (instance_id == joysticklist->instance_id) {
            joystick = joysticklist;
            ++joystick->ref_count;
            SDL_UnlockJoysticks();
            return joystick;
        }
        joysticklist = joysticklist->next;
    }

    /* Create and initialize the joystick */
    joystick = (SDL_Joystick *)SDL_calloc(sizeof(*joystick), 1);
    if (!joystick) {
        SDL_UnlockJoysticks();
        return NULL;
    }
    joystick->magic = &SDL_joystick_magic;
    joystick->driver = driver;
    joystick->instance_id = instance_id;
    joystick->attached = SDL_TRUE;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_UNKNOWN;
    joystick->led_expiration = SDL_GetTicks();

    if (driver->Open(joystick, device_index) < 0) {
        SDL_free(joystick);
        SDL_UnlockJoysticks();
        return NULL;
    }

    joystickname = driver->GetDeviceName(device_index);
    if (joystickname) {
        joystick->name = SDL_strdup(joystickname);
    }

    joystickpath = driver->GetDevicePath(device_index);
    if (joystickpath) {
        joystick->path = SDL_strdup(joystickpath);
    }

    joystick->guid = driver->GetDeviceGUID(device_index);

    if (joystick->naxes > 0) {
        joystick->axes = (SDL_JoystickAxisInfo *)SDL_calloc(joystick->naxes, sizeof(*joystick->axes));
    }
    if (joystick->nballs > 0) {
        joystick->balls = (SDL_JoystickBallData *)SDL_calloc(joystick->nballs, sizeof(*joystick->balls));
    }
    if (joystick->nhats > 0) {
        joystick->hats = (Uint8 *)SDL_calloc(joystick->nhats, sizeof(*joystick->hats));
    }
    if (joystick->nbuttons > 0) {
        joystick->buttons = (Uint8 *)SDL_calloc(joystick->nbuttons, sizeof(*joystick->buttons));
    }
    if (((joystick->naxes > 0) && !joystick->axes) ||
        ((joystick->nballs > 0) && !joystick->balls) ||
        ((joystick->nhats > 0) && !joystick->hats) ||
        ((joystick->nbuttons > 0) && !joystick->buttons)) {
        SDL_CloseJoystick(joystick);
        SDL_UnlockJoysticks();
        return NULL;
    }

    /* If this joystick is known to have all zero centered axes, skip the auto-centering code */
    if (SDL_JoystickAxesCenteredAtZero(joystick)) {
        int i;

        for (i = 0; i < joystick->naxes; ++i) {
            joystick->axes[i].has_initial_value = SDL_TRUE;
        }
    }

    joystick->is_gamepad = SDL_IsGamepad(instance_id);

    /* Get the Steam Input API handle */
    info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
    if (info) {
        joystick->steam_handle = info->handle;
    }

    /* Use system gyro and accelerometer if the gamepad doesn't have built-in sensors */
    if (ShouldAttemptSensorFusion(joystick, &invert_sensors)) {
        AttemptSensorFusion(joystick, invert_sensors);
    }

    /* Add joystick to list */
    ++joystick->ref_count;
    /* Link the joystick in the list */
    joystick->next = SDL_joysticks;
    SDL_joysticks = joystick;

    /* send initial battery event */
    initial_power_level = joystick->epowerlevel;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_UNKNOWN;
    SDL_SendJoystickBatteryLevel(joystick, initial_power_level);

    driver->Update(joystick);

    SDL_UnlockJoysticks();

    return joystick;
}

SDL_JoystickID SDL_AttachVirtualJoystick(SDL_JoystickType type, int naxes, int nbuttons, int nhats)
{
    SDL_VirtualJoystickDesc desc;

    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = (Uint16)type;
    desc.naxes = (Uint16)naxes;
    desc.nbuttons = (Uint16)nbuttons;
    desc.nhats = (Uint16)nhats;
    return SDL_AttachVirtualJoystickEx(&desc);
}

SDL_JoystickID SDL_AttachVirtualJoystickEx(const SDL_VirtualJoystickDesc *desc)
{
#ifdef SDL_JOYSTICK_VIRTUAL
    SDL_JoystickID retval;

    SDL_LockJoysticks();
    retval = SDL_JoystickAttachVirtualInner(desc);
    SDL_UnlockJoysticks();
    return retval;
#else
    return SDL_SetError("SDL not built with virtual-joystick support");
#endif
}

int SDL_DetachVirtualJoystick(SDL_JoystickID instance_id)
{
#ifdef SDL_JOYSTICK_VIRTUAL
    int retval;

    SDL_LockJoysticks();
    retval = SDL_JoystickDetachVirtualInner(instance_id);
    SDL_UnlockJoysticks();
    return retval;
#else
    return SDL_SetError("SDL not built with virtual-joystick support");
#endif
}

SDL_bool SDL_IsJoystickVirtual(SDL_JoystickID instance_id)
{
#ifdef SDL_JOYSTICK_VIRTUAL
    SDL_JoystickDriver *driver;
    int device_index;
    SDL_bool is_virtual = SDL_FALSE;

    SDL_LockJoysticks();
    if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        if (driver == &SDL_VIRTUAL_JoystickDriver) {
            is_virtual = SDL_TRUE;
        }
    }
    SDL_UnlockJoysticks();

    return is_virtual;
#else
    return SDL_FALSE;
#endif
}

int SDL_SetJoystickVirtualAxis(SDL_Joystick *joystick, int axis, Sint16 value)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

#ifdef SDL_JOYSTICK_VIRTUAL
        retval = SDL_SetJoystickVirtualAxisInner(joystick, axis, value);
#else
        retval = SDL_SetError("SDL not built with virtual-joystick support");
#endif
    }
    SDL_UnlockJoysticks();

    return retval;
}

int SDL_SetJoystickVirtualButton(SDL_Joystick *joystick, int button, Uint8 value)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

#ifdef SDL_JOYSTICK_VIRTUAL
        retval = SDL_SetJoystickVirtualButtonInner(joystick, button, value);
#else
        retval = SDL_SetError("SDL not built with virtual-joystick support");
#endif
    }
    SDL_UnlockJoysticks();

    return retval;
}

int SDL_SetJoystickVirtualHat(SDL_Joystick *joystick, int hat, Uint8 value)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

#ifdef SDL_JOYSTICK_VIRTUAL
        retval = SDL_SetJoystickVirtualHatInner(joystick, hat, value);
#else
        retval = SDL_SetError("SDL not built with virtual-joystick support");
#endif
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Checks to make sure the joystick is valid.
 */
SDL_bool SDL_IsJoystickValid(SDL_Joystick *joystick)
{
    SDL_AssertJoysticksLocked();
    return (joystick && joystick->magic == &SDL_joystick_magic);
}

SDL_bool SDL_PrivateJoystickGetAutoGamepadMapping(SDL_JoystickID instance_id, SDL_GamepadMapping *out)
{
    SDL_JoystickDriver *driver;
    int device_index;
    SDL_bool is_ok = SDL_FALSE;

    SDL_LockJoysticks();
    if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        is_ok = driver->GetGamepadMapping(device_index, out);
    }
    SDL_UnlockJoysticks();

    return is_ok;
}

/*
 * Get the number of multi-dimensional axis controls on a joystick
 */
int SDL_GetNumJoystickAxes(SDL_Joystick *joystick)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        retval = joystick->naxes;
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the number of hats on a joystick
 */
int SDL_GetNumJoystickHats(SDL_Joystick *joystick)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        retval = joystick->nhats;
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the number of trackballs on a joystick
 */
int SDL_GetNumJoystickBalls(SDL_Joystick *joystick)
{
    CHECK_JOYSTICK_MAGIC(joystick, -1);

    return joystick->nballs;
}

/*
 * Get the number of buttons on a joystick
 */
int SDL_GetNumJoystickButtons(SDL_Joystick *joystick)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        retval = joystick->nbuttons;
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the current state of an axis control on a joystick
 */
Sint16 SDL_GetJoystickAxis(SDL_Joystick *joystick, int axis)
{
    Sint16 state;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        if (axis < joystick->naxes) {
            state = joystick->axes[axis].value;
        } else {
            SDL_SetError("Joystick only has %d axes", joystick->naxes);
            state = 0;
        }
    }
    SDL_UnlockJoysticks();

    return state;
}

/*
 * Get the initial state of an axis control on a joystick
 */
SDL_bool SDL_GetJoystickAxisInitialState(SDL_Joystick *joystick, int axis, Sint16 *state)
{
    SDL_bool retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, SDL_FALSE);

        if (axis >= joystick->naxes) {
            SDL_SetError("Joystick only has %d axes", joystick->naxes);
            retval = SDL_FALSE;
        } else {
            if (state) {
                *state = joystick->axes[axis].initial_value;
            }
            retval = joystick->axes[axis].has_initial_value;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the current state of a hat on a joystick
 */
Uint8 SDL_GetJoystickHat(SDL_Joystick *joystick, int hat)
{
    Uint8 state;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        if (hat < joystick->nhats) {
            state = joystick->hats[hat];
        } else {
            SDL_SetError("Joystick only has %d hats", joystick->nhats);
            state = 0;
        }
    }
    SDL_UnlockJoysticks();

    return state;
}

/*
 * Get the ball axis change since the last poll
 */
int SDL_GetJoystickBall(SDL_Joystick *joystick, int ball, int *dx, int *dy)
{
    int retval;

    CHECK_JOYSTICK_MAGIC(joystick, -1);

    retval = 0;
    if (ball < joystick->nballs) {
        if (dx) {
            *dx = joystick->balls[ball].dx;
        }
        if (dy) {
            *dy = joystick->balls[ball].dy;
        }
        joystick->balls[ball].dx = 0;
        joystick->balls[ball].dy = 0;
    } else {
        return SDL_SetError("Joystick only has %d balls", joystick->nballs);
    }
    return retval;
}

/*
 * Get the current state of a button on a joystick
 */
Uint8 SDL_GetJoystickButton(SDL_Joystick *joystick, int button)
{
    Uint8 state;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        if (button < joystick->nbuttons) {
            state = joystick->buttons[button];
        } else {
            SDL_SetError("Joystick only has %d buttons", joystick->nbuttons);
            state = 0;
        }
    }
    SDL_UnlockJoysticks();

    return state;
}

/*
 * Return if the joystick in question is currently attached to the system,
 *  \return SDL_FALSE if not plugged in, SDL_TRUE if still present.
 */
SDL_bool SDL_JoystickConnected(SDL_Joystick *joystick)
{
    SDL_bool retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, SDL_FALSE);

        retval = joystick->attached;
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the instance id for this opened joystick
 */
SDL_JoystickID SDL_GetJoystickInstanceID(SDL_Joystick *joystick)
{
    SDL_JoystickID retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        retval = joystick->instance_id;
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Return the SDL_Joystick associated with an instance id.
 */
SDL_Joystick *SDL_GetJoystickFromInstanceID(SDL_JoystickID instance_id)
{
    SDL_Joystick *joystick;

    SDL_LockJoysticks();
    for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
        if (joystick->instance_id == instance_id) {
            break;
        }
    }
    SDL_UnlockJoysticks();
    return joystick;
}

/**
 * Return the SDL_Joystick associated with a player index.
 */
SDL_Joystick *SDL_GetJoystickFromPlayerIndex(int player_index)
{
    SDL_JoystickID instance_id;
    SDL_Joystick *joystick;

    SDL_LockJoysticks();
    instance_id = SDL_GetJoystickIDForPlayerIndex(player_index);
    for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
        if (joystick->instance_id == instance_id) {
            break;
        }
    }
    SDL_UnlockJoysticks();
    return joystick;
}

/*
 * Get the properties associated with a joystick
 */
SDL_PropertiesID SDL_GetJoystickProperties(SDL_Joystick *joystick)
{
    SDL_PropertiesID retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        if (joystick->props == 0) {
            joystick->props = SDL_CreateProperties();
        }
        retval = joystick->props;
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the friendly name of this joystick
 */
const char *SDL_GetJoystickName(SDL_Joystick *joystick)
{
    const char *retval;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, NULL);

        info = SDL_GetJoystickInstanceVirtualGamepadInfo(joystick->instance_id);
        if (info) {
            retval = info->name;
        } else {
            retval = joystick->name;
        }
    }
    SDL_UnlockJoysticks();

    /* FIXME: Really we should reference count this name so it doesn't go away after unlock */
    return retval;
}

/*
 * Get the implementation dependent path of this joystick
 */
const char *SDL_GetJoystickPath(SDL_Joystick *joystick)
{
    const char *retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, NULL);

        if (joystick->path) {
            retval = joystick->path;
        } else {
            SDL_Unsupported();
            retval = NULL;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Get the player index of an opened joystick, or -1 if it's not available
 */
int SDL_GetJoystickPlayerIndex(SDL_Joystick *joystick)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        retval = SDL_GetPlayerIndexForJoystickID(joystick->instance_id);
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Set the player index of an opened joystick
 */
int SDL_SetJoystickPlayerIndex(SDL_Joystick *joystick, int player_index)
{
    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        SDL_SetJoystickIDForPlayerIndex(player_index, joystick->instance_id);
    }
    SDL_UnlockJoysticks();
    return 0;
}

int SDL_RumbleJoystick(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        if (low_frequency_rumble == joystick->low_frequency_rumble &&
            high_frequency_rumble == joystick->high_frequency_rumble) {
            /* Just update the expiration */
            retval = 0;
        } else {
            retval = joystick->driver->Rumble(joystick, low_frequency_rumble, high_frequency_rumble);
            joystick->rumble_resend = SDL_GetTicks() + SDL_RUMBLE_RESEND_MS;
        }

        if (retval == 0) {
            joystick->low_frequency_rumble = low_frequency_rumble;
            joystick->high_frequency_rumble = high_frequency_rumble;

            if ((low_frequency_rumble || high_frequency_rumble) && duration_ms) {
                joystick->rumble_expiration = SDL_GetTicks() + SDL_min(duration_ms, SDL_MAX_RUMBLE_DURATION_MS);
                if (!joystick->rumble_expiration) {
                    joystick->rumble_expiration = 1;
                }
            } else {
                joystick->rumble_expiration = 0;
                joystick->rumble_resend = 0;
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

int SDL_RumbleJoystickTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble, Uint32 duration_ms)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        if (left_rumble == joystick->left_trigger_rumble && right_rumble == joystick->right_trigger_rumble) {
            /* Just update the expiration */
            retval = 0;
        } else {
            retval = joystick->driver->RumbleTriggers(joystick, left_rumble, right_rumble);
        }

        if (retval == 0) {
            joystick->left_trigger_rumble = left_rumble;
            joystick->right_trigger_rumble = right_rumble;

            if ((left_rumble || right_rumble) && duration_ms) {
                joystick->trigger_rumble_expiration = SDL_GetTicks() + SDL_min(duration_ms, SDL_MAX_RUMBLE_DURATION_MS);
            } else {
                joystick->trigger_rumble_expiration = 0;
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

int SDL_SetJoystickLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    int retval;
    SDL_bool isfreshvalue;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        isfreshvalue = red != joystick->led_red ||
                       green != joystick->led_green ||
                       blue != joystick->led_blue;

        if (isfreshvalue || SDL_GetTicks() >= joystick->led_expiration) {
            retval = joystick->driver->SetLED(joystick, red, green, blue);
            joystick->led_expiration = SDL_GetTicks() + SDL_LED_MIN_REPEAT_MS;
        } else {
            /* Avoid spamming the driver */
            retval = 0;
        }

        /* Save the LED value regardless of success, so we don't spam the driver */
        joystick->led_red = red;
        joystick->led_green = green;
        joystick->led_blue = blue;
    }
    SDL_UnlockJoysticks();

    return retval;
}

int SDL_SendJoystickEffect(SDL_Joystick *joystick, const void *data, int size)
{
    int retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, -1);

        retval = joystick->driver->SendEffect(joystick, data, size);
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Close a joystick previously opened with SDL_OpenJoystick()
 */
void SDL_CloseJoystick(SDL_Joystick *joystick)
{
    SDL_Joystick *joysticklist;
    SDL_Joystick *joysticklistprev;
    int i;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick,);

        /* First decrement ref count */
        if (--joystick->ref_count > 0) {
            SDL_UnlockJoysticks();
            return;
        }

        SDL_DestroyProperties(joystick->props);

        if (joystick->rumble_expiration) {
            SDL_RumbleJoystick(joystick, 0, 0, 0);
        }
        if (joystick->trigger_rumble_expiration) {
            SDL_RumbleJoystickTriggers(joystick, 0, 0, 0);
        }

        CleanupSensorFusion(joystick);

        joystick->driver->Close(joystick);
        joystick->hwdata = NULL;
        joystick->magic = NULL;

        joysticklist = SDL_joysticks;
        joysticklistprev = NULL;
        while (joysticklist) {
            if (joystick == joysticklist) {
                if (joysticklistprev) {
                    /* unlink this entry */
                    joysticklistprev->next = joysticklist->next;
                } else {
                    SDL_joysticks = joystick->next;
                }
                break;
            }
            joysticklistprev = joysticklist;
            joysticklist = joysticklist->next;
        }

        /* Free the data associated with this joystick */
        SDL_free(joystick->name);
        SDL_free(joystick->path);
        SDL_free(joystick->serial);
        SDL_free(joystick->axes);
        SDL_free(joystick->balls);
        SDL_free(joystick->hats);
        SDL_free(joystick->buttons);
        for (i = 0; i < joystick->ntouchpads; i++) {
            SDL_JoystickTouchpadInfo *touchpad = &joystick->touchpads[i];
            SDL_free(touchpad->fingers);
        }
        SDL_free(joystick->touchpads);
        SDL_free(joystick->sensors);
        SDL_free(joystick);
    }
    SDL_UnlockJoysticks();
}

void SDL_QuitJoysticks(void)
{
    int i;
    SDL_JoystickID *joysticks;

    SDL_LockJoysticks();

    SDL_joysticks_quitting = SDL_TRUE;

    joysticks = SDL_GetJoysticks(NULL);
    if (joysticks) {
        for (i = 0; joysticks[i]; ++i) {
            SDL_PrivateJoystickRemoved(joysticks[i]);
        }
        SDL_free(joysticks);
    }

    while (SDL_joysticks) {
        SDL_joysticks->ref_count = 1;
        SDL_CloseJoystick(SDL_joysticks);
    }

    /* Quit drivers in reverse order to avoid breaking dependencies between drivers */
    for (i = SDL_arraysize(SDL_joystick_drivers) - 1; i >= 0; --i) {
        SDL_joystick_drivers[i]->Quit();
    }

    if (SDL_joystick_players) {
        SDL_free(SDL_joystick_players);
        SDL_joystick_players = NULL;
        SDL_joystick_player_count = 0;
    }

    SDL_QuitSubSystem(SDL_INIT_EVENTS);

    SDL_QuitSteamVirtualGamepadInfo();

    SDL_DelHintCallback(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
                        SDL_JoystickAllowBackgroundEventsChanged, NULL);

    SDL_FreeVIDPIDList(&arcadestick_devices);
    SDL_FreeVIDPIDList(&blacklist_devices);
    SDL_FreeVIDPIDList(&flightstick_devices);
    SDL_FreeVIDPIDList(&gamecube_devices);
    SDL_FreeVIDPIDList(&rog_gamepad_mice);
    SDL_FreeVIDPIDList(&throttle_devices);
    SDL_FreeVIDPIDList(&wheel_devices);
    SDL_FreeVIDPIDList(&zero_centered_devices);

    SDL_QuitGamepadMappings();

    SDL_joysticks_quitting = SDL_FALSE;
    SDL_joysticks_initialized = SDL_FALSE;

    SDL_UnlockJoysticks();
}

static SDL_bool SDL_PrivateJoystickShouldIgnoreEvent(void)
{
    if (SDL_joystick_allows_background_events) {
        return SDL_FALSE;
    }

    if (SDL_HasWindows() && SDL_GetKeyboardFocus() == NULL) {
        /* We have windows but we don't have focus, ignore the event. */
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

/* These are global for SDL_sysjoystick.c and SDL_events.c */

void SDL_PrivateJoystickAddTouchpad(SDL_Joystick *joystick, int nfingers)
{
    int ntouchpads;
    SDL_JoystickTouchpadInfo *touchpads;

    SDL_AssertJoysticksLocked();

    ntouchpads = joystick->ntouchpads + 1;
    touchpads = (SDL_JoystickTouchpadInfo *)SDL_realloc(joystick->touchpads, (ntouchpads * sizeof(SDL_JoystickTouchpadInfo)));
    if (touchpads) {
        SDL_JoystickTouchpadInfo *touchpad = &touchpads[ntouchpads - 1];
        SDL_JoystickTouchpadFingerInfo *fingers = (SDL_JoystickTouchpadFingerInfo *)SDL_calloc(nfingers, sizeof(SDL_JoystickTouchpadFingerInfo));

        if (fingers) {
            touchpad->nfingers = nfingers;
            touchpad->fingers = fingers;
        } else {
            /* Out of memory, this touchpad won't be active */
            touchpad->nfingers = 0;
            touchpad->fingers = NULL;
        }

        joystick->ntouchpads = ntouchpads;
        joystick->touchpads = touchpads;
    }
}

void SDL_PrivateJoystickAddSensor(SDL_Joystick *joystick, SDL_SensorType type, float rate)
{
    int nsensors;
    SDL_JoystickSensorInfo *sensors;

    SDL_AssertJoysticksLocked();

    nsensors = joystick->nsensors + 1;
    sensors = (SDL_JoystickSensorInfo *)SDL_realloc(joystick->sensors, (nsensors * sizeof(SDL_JoystickSensorInfo)));
    if (sensors) {
        SDL_JoystickSensorInfo *sensor = &sensors[nsensors - 1];

        SDL_zerop(sensor);
        sensor->type = type;
        sensor->rate = rate;

        joystick->nsensors = nsensors;
        joystick->sensors = sensors;
    }
}

void SDL_PrivateJoystickAdded(SDL_JoystickID instance_id)
{
    SDL_JoystickDriver *driver;
    int device_index;
    int player_index = -1;

    SDL_AssertJoysticksLocked();

    if (SDL_JoysticksQuitting()) {
        return;
    }

    SDL_joystick_being_added = SDL_TRUE;

    if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        player_index = driver->GetDeviceSteamVirtualGamepadSlot(device_index);
        if (player_index < 0) {
            player_index = driver->GetDevicePlayerIndex(device_index);
        }
    }
    if (player_index < 0 && SDL_IsGamepad(instance_id)) {
        player_index = SDL_FindFreePlayerIndex();
    }
    if (player_index >= 0) {
        SDL_SetJoystickIDForPlayerIndex(player_index, instance_id);
    }

    {
        SDL_Event event;

        event.type = SDL_EVENT_JOYSTICK_ADDED;
        event.common.timestamp = 0;

        if (SDL_EventEnabled(event.type)) {
            event.jdevice.which = instance_id;
            SDL_PushEvent(&event);
        }
    }

    SDL_joystick_being_added = SDL_FALSE;

    if (SDL_IsGamepad(instance_id)) {
        SDL_PrivateGamepadAdded(instance_id);
    }
}

SDL_bool SDL_IsJoystickBeingAdded(void)
{
    return SDL_joystick_being_added;
}

void SDL_PrivateJoystickForceRecentering(SDL_Joystick *joystick)
{
    Uint8 i, j;
    Uint64 timestamp = SDL_GetTicksNS();

    SDL_AssertJoysticksLocked();

    /* Tell the app that everything is centered/unpressed... */
    for (i = 0; i < joystick->naxes; i++) {
        if (joystick->axes[i].has_initial_value) {
            SDL_SendJoystickAxis(timestamp, joystick, i, joystick->axes[i].zero);
        }
    }

    for (i = 0; i < joystick->nbuttons; i++) {
        SDL_SendJoystickButton(timestamp, joystick, i, SDL_RELEASED);
    }

    for (i = 0; i < joystick->nhats; i++) {
        SDL_SendJoystickHat(timestamp, joystick, i, SDL_HAT_CENTERED);
    }

    for (i = 0; i < joystick->ntouchpads; i++) {
        SDL_JoystickTouchpadInfo *touchpad = &joystick->touchpads[i];

        for (j = 0; j < touchpad->nfingers; ++j) {
            SDL_SendJoystickTouchpad(timestamp, joystick, i, j, SDL_RELEASED, 0.0f, 0.0f, 0.0f);
        }
    }
}

void SDL_PrivateJoystickRemoved(SDL_JoystickID instance_id)
{
    SDL_Joystick *joystick = NULL;
    int player_index;
    SDL_Event event;

    SDL_AssertJoysticksLocked();

    /* Find this joystick... */
    for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
        if (joystick->instance_id == instance_id) {
            SDL_PrivateJoystickForceRecentering(joystick);
            joystick->attached = SDL_FALSE;
            break;
        }
    }

    /* FIXME: The driver no longer provides the name and GUID at this point, so we
     *        don't know whether this was a gamepad. For now always send the event.
     */
    if (SDL_TRUE /*SDL_IsGamepad(instance_id)*/) {
        SDL_PrivateGamepadRemoved(instance_id);
    }

    event.type = SDL_EVENT_JOYSTICK_REMOVED;
    event.common.timestamp = 0;

    if (SDL_EventEnabled(event.type)) {
        event.jdevice.which = instance_id;
        SDL_PushEvent(&event);
    }

    player_index = SDL_GetPlayerIndexForJoystickID(instance_id);
    if (player_index >= 0) {
        SDL_joystick_players[player_index] = 0;
    }
}

int SDL_SendJoystickAxis(Uint64 timestamp, SDL_Joystick *joystick, Uint8 axis, Sint16 value)
{
    int posted;
    SDL_JoystickAxisInfo *info;

    SDL_AssertJoysticksLocked();

    /* Make sure we're not getting garbage or duplicate events */
    if (axis >= joystick->naxes) {
        return 0;
    }

    info = &joystick->axes[axis];
    if (!info->has_initial_value ||
        (!info->has_second_value && (info->initial_value <= -32767 || info->initial_value == 32767) && SDL_abs(value) < (SDL_JOYSTICK_AXIS_MAX / 4))) {
        info->initial_value = value;
        info->value = value;
        info->zero = value;
        info->has_initial_value = SDL_TRUE;
    } else if (value == info->value && !info->sending_initial_value) {
        return 0;
    } else {
        info->has_second_value = SDL_TRUE;
    }
    if (!info->sent_initial_value) {
        /* Make sure we don't send motion until there's real activity on this axis */
        const int MAX_ALLOWED_JITTER = SDL_JOYSTICK_AXIS_MAX / 80; /* ShanWan PS3 controller needed 96 */
        if (SDL_abs(value - info->value) <= MAX_ALLOWED_JITTER &&
            !SDL_IsJoystickVIRTUAL(joystick->guid)) {
            return 0;
        }
        info->sent_initial_value = SDL_TRUE;
        info->sending_initial_value = SDL_TRUE;
        SDL_SendJoystickAxis(timestamp, joystick, axis, info->initial_value);
        info->sending_initial_value = SDL_FALSE;
    }

    /* We ignore events if we don't have keyboard focus, except for centering
     * events.
     */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if (info->sending_initial_value ||
            (value > info->zero && value >= info->value) ||
            (value < info->zero && value <= info->value)) {
            return 0;
        }
    }

    /* Update internal joystick state */
    SDL_assert(timestamp != 0);
    info->value = value;
    joystick->update_complete = timestamp;

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_AXIS_MOTION)) {
        SDL_Event event;
        event.type = SDL_EVENT_JOYSTICK_AXIS_MOTION;
        event.common.timestamp = timestamp;
        event.jaxis.which = joystick->instance_id;
        event.jaxis.axis = axis;
        event.jaxis.value = value;
        posted = SDL_PushEvent(&event) == 1;
    }
    return posted;
}

int SDL_SendJoystickBall(Uint64 timestamp, SDL_Joystick *joystick, Uint8 ball, Sint16 xrel, Sint16 yrel)
{
    int posted;

    SDL_AssertJoysticksLocked();

    /* Make sure we're not getting garbage events */
    if (ball >= joystick->nballs) {
        return 0;
    }

    /* We ignore events if we don't have keyboard focus. */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        return 0;
    }

    /* Update internal mouse state */
    joystick->balls[ball].dx += xrel;
    joystick->balls[ball].dy += yrel;

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_BALL_MOTION)) {
        SDL_Event event;
        event.type = SDL_EVENT_JOYSTICK_BALL_MOTION;
        event.common.timestamp = timestamp;
        event.jball.which = joystick->instance_id;
        event.jball.ball = ball;
        event.jball.xrel = xrel;
        event.jball.yrel = yrel;
        posted = SDL_PushEvent(&event) == 1;
    }
    return posted;
}

int SDL_SendJoystickHat(Uint64 timestamp, SDL_Joystick *joystick, Uint8 hat, Uint8 value)
{
    int posted;

    SDL_AssertJoysticksLocked();

    /* Make sure we're not getting garbage or duplicate events */
    if (hat >= joystick->nhats) {
        return 0;
    }
    if (value == joystick->hats[hat]) {
        return 0;
    }

    /* We ignore events if we don't have keyboard focus, except for centering
     * events.
     */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if (value != SDL_HAT_CENTERED) {
            return 0;
        }
    }

    /* Update internal joystick state */
    SDL_assert(timestamp != 0);
    joystick->hats[hat] = value;
    joystick->update_complete = timestamp;

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_HAT_MOTION)) {
        SDL_Event event;
        event.type = SDL_EVENT_JOYSTICK_HAT_MOTION;
        event.common.timestamp = timestamp;
        event.jhat.which = joystick->instance_id;
        event.jhat.hat = hat;
        event.jhat.value = value;
        posted = SDL_PushEvent(&event) == 1;
    }
    return posted;
}

int SDL_SendJoystickButton(Uint64 timestamp, SDL_Joystick *joystick, Uint8 button, Uint8 state)
{
    int posted;
    SDL_Event event;

    SDL_AssertJoysticksLocked();

    switch (state) {
    case SDL_PRESSED:
        event.type = SDL_EVENT_JOYSTICK_BUTTON_DOWN;
        break;
    case SDL_RELEASED:
        event.type = SDL_EVENT_JOYSTICK_BUTTON_UP;
        break;
    default:
        /* Invalid state -- bail */
        return 0;
    }

    SDL_AssertJoysticksLocked();

    /* Make sure we're not getting garbage or duplicate events */
    if (button >= joystick->nbuttons) {
        return 0;
    }
    if (state == joystick->buttons[button]) {
        return 0;
    }

    /* We ignore events if we don't have keyboard focus, except for button
     * release. */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if (state == SDL_PRESSED) {
            return 0;
        }
    }

    /* Update internal joystick state */
    SDL_assert(timestamp != 0);
    joystick->buttons[button] = state;
    joystick->update_complete = timestamp;

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(event.type)) {
        event.common.timestamp = timestamp;
        event.jbutton.which = joystick->instance_id;
        event.jbutton.button = button;
        event.jbutton.state = state;
        posted = SDL_PushEvent(&event) == 1;
    }
    return posted;
}

static void SendSteamHandleUpdateEvents(void)
{
    SDL_Joystick *joystick;
    const SDL_SteamVirtualGamepadInfo *info;

    /* Check to see if any Steam handles changed */
    for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
        SDL_bool changed = SDL_FALSE;

        if (!joystick->is_gamepad) {
            continue;
        }

        info = SDL_GetJoystickInstanceVirtualGamepadInfo(joystick->instance_id);
        if (info) {
            if (joystick->steam_handle != info->handle) {
                joystick->steam_handle = info->handle;
                changed = SDL_TRUE;
            }
        } else {
            if (joystick->steam_handle != 0) {
                joystick->steam_handle = 0;
                changed = SDL_TRUE;
            }
        }
        if (changed) {
            SDL_Event event;

            SDL_zero(event);
            event.type = SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED;
            event.common.timestamp = 0;
            event.gdevice.which = joystick->instance_id;
            SDL_PushEvent(&event);
        }
    }
}

void SDL_UpdateJoysticks(void)
{
    int i;
    Uint64 now;
    SDL_Joystick *joystick;

    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        return;
    }

    SDL_LockJoysticks();

    if (SDL_UpdateSteamVirtualGamepadInfo()) {
        SendSteamHandleUpdateEvents();
    }

#ifdef SDL_JOYSTICK_HIDAPI
    /* Special function for HIDAPI devices, as a single device can provide multiple SDL_Joysticks */
    HIDAPI_UpdateDevices();
#endif /* SDL_JOYSTICK_HIDAPI */

    for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
        if (joystick->attached) {
            joystick->driver->Update(joystick);

            if (joystick->delayed_guide_button) {
                SDL_GamepadHandleDelayedGuideButton(joystick);
            }
        }

        now = SDL_GetTicks();
        if (joystick->rumble_expiration && now >= joystick->rumble_expiration) {
            SDL_RumbleJoystick(joystick, 0, 0, 0);
            joystick->rumble_resend = 0;
        }

        if (joystick->rumble_resend && now >= joystick->rumble_resend) {
            joystick->driver->Rumble(joystick, joystick->low_frequency_rumble, joystick->high_frequency_rumble);
            joystick->rumble_resend = now + SDL_RUMBLE_RESEND_MS;
            if (joystick->rumble_resend == 0) {
                joystick->rumble_resend = 1;
            }
        }

        if (joystick->trigger_rumble_expiration && now >= joystick->trigger_rumble_expiration) {
            SDL_RumbleJoystickTriggers(joystick, 0, 0, 0);
        }
    }

    if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_UPDATE_COMPLETE)) {
        for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
            if (joystick->update_complete) {
                SDL_Event event;

                event.type = SDL_EVENT_JOYSTICK_UPDATE_COMPLETE;
                event.common.timestamp = joystick->update_complete;
                event.jdevice.which = joystick->instance_id;
                SDL_PushEvent(&event);

                joystick->update_complete = 0;
            }
        }
    }

    /* this needs to happen AFTER walking the joystick list above, so that any
       dangling hardware data from removed devices can be free'd
     */
    for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
        SDL_joystick_drivers[i]->Detect();
    }

    SDL_UnlockJoysticks();
}

static const Uint32 SDL_joystick_event_list[] = {
    SDL_EVENT_JOYSTICK_AXIS_MOTION,
    SDL_EVENT_JOYSTICK_BALL_MOTION,
    SDL_EVENT_JOYSTICK_HAT_MOTION,
    SDL_EVENT_JOYSTICK_BUTTON_DOWN,
    SDL_EVENT_JOYSTICK_BUTTON_UP,
    SDL_EVENT_JOYSTICK_ADDED,
    SDL_EVENT_JOYSTICK_REMOVED,
    SDL_EVENT_JOYSTICK_BATTERY_UPDATED
};

void SDL_SetJoystickEventsEnabled(SDL_bool enabled)
{
    unsigned int i;

    for (i = 0; i < SDL_arraysize(SDL_joystick_event_list); ++i) {
        SDL_SetEventEnabled(SDL_joystick_event_list[i], enabled);
    }
}

SDL_bool SDL_JoystickEventsEnabled(void)
{
    SDL_bool enabled = SDL_FALSE;
    unsigned int i;

    for (i = 0; i < SDL_arraysize(SDL_joystick_event_list); ++i) {
        enabled = SDL_EventEnabled(SDL_joystick_event_list[i]);
        if (enabled) {
            break;
        }
    }
    return enabled;
}

void SDL_GetJoystickGUIDInfo(SDL_JoystickGUID guid, Uint16 *vendor, Uint16 *product, Uint16 *version, Uint16 *crc16)
{
    Uint16 *guid16 = (Uint16 *)guid.data;
    Uint16 bus = SDL_SwapLE16(guid16[0]);

    if ((bus < ' ' || bus == SDL_HARDWARE_BUS_VIRTUAL) && guid16[3] == 0x0000 && guid16[5] == 0x0000) {
        /* This GUID fits the standard form:
         * 16-bit bus
         * 16-bit CRC16 of the joystick name (can be zero)
         * 16-bit vendor ID
         * 16-bit zero
         * 16-bit product ID
         * 16-bit zero
         * 16-bit version
         * 8-bit driver identifier ('h' for HIDAPI, 'x' for XInput, etc.)
         * 8-bit driver-dependent type info
         */
        if (vendor) {
            *vendor = SDL_SwapLE16(guid16[2]);
        }
        if (product) {
            *product = SDL_SwapLE16(guid16[4]);
        }
        if (version) {
            *version = SDL_SwapLE16(guid16[6]);
        }
        if (crc16) {
            *crc16 = SDL_SwapLE16(guid16[1]);
        }
    } else if (bus < ' ' || bus == SDL_HARDWARE_BUS_VIRTUAL) {
        /* This GUID fits the unknown VID/PID form:
         * 16-bit bus
         * 16-bit CRC16 of the joystick name (can be zero)
         * 11 characters of the joystick name, null terminated
         */
        if (vendor) {
            *vendor = 0;
        }
        if (product) {
            *product = 0;
        }
        if (version) {
            *version = 0;
        }
        if (crc16) {
            *crc16 = SDL_SwapLE16(guid16[1]);
        }
    } else {
        if (vendor) {
            *vendor = 0;
        }
        if (product) {
            *product = 0;
        }
        if (version) {
            *version = 0;
        }
        if (crc16) {
            *crc16 = 0;
        }
    }
}

static int PrefixMatch(const char *a, const char *b)
{
    int matchlen = 0;
    while (*a && *b) {
        if (SDL_tolower((unsigned char)*a++) == SDL_tolower((unsigned char)*b++)) {
            ++matchlen;
        } else {
            break;
        }
    }
    return matchlen;
}

char *SDL_CreateJoystickName(Uint16 vendor, Uint16 product, const char *vendor_name, const char *product_name)
{
    static struct
    {
        const char *prefix;
        const char *replacement;
    } replacements[] = {
        { "ASTRO Gaming", "ASTRO" },
        { "Bensussen Deutsch & Associates,Inc.(BDA)", "BDA" },
        { "Guangzhou Chicken Run Network Technology Co., Ltd.", "GameSir" },
        { "HORI CO.,LTD", "HORI" },
        { "HORI CO.,LTD.", "HORI" },
        { "Mad Catz Inc.", "Mad Catz" },
        { "Nintendo Co., Ltd.", "Nintendo" },
        { "NVIDIA Corporation ", "" },
        { "Performance Designed Products", "PDP" },
        { "QANBA USA, LLC", "Qanba" },
        { "QANBA USA,LLC", "Qanba" },
        { "Unknown ", "" },
    };
    const char *custom_name;
    char *name;
    size_t i, len;

    custom_name = GuessControllerName(vendor, product);
    if (custom_name) {
        return SDL_strdup(custom_name);
    }

    if (!vendor_name) {
        vendor_name = "";
    }
    if (!product_name) {
        product_name = "";
    }

    while (*vendor_name == ' ') {
        ++vendor_name;
    }
    while (*product_name == ' ') {
        ++product_name;
    }

    if (*vendor_name && *product_name) {
        len = (SDL_strlen(vendor_name) + 1 + SDL_strlen(product_name) + 1);
        name = (char *)SDL_malloc(len);
        if (name) {
            (void)SDL_snprintf(name, len, "%s %s", vendor_name, product_name);
        }
    } else if (*product_name) {
        name = SDL_strdup(product_name);
    } else if (vendor || product) {
        /* Couldn't find a controller name, try to give it one based on device type */
        switch (SDL_GetGamepadTypeFromVIDPID(vendor, product, NULL, SDL_TRUE)) {
        case SDL_GAMEPAD_TYPE_XBOX360:
            name = SDL_strdup("Xbox 360 Controller");
            break;
        case SDL_GAMEPAD_TYPE_XBOXONE:
            name = SDL_strdup("Xbox One Controller");
            break;
        case SDL_GAMEPAD_TYPE_PS3:
            name = SDL_strdup("PS3 Controller");
            break;
        case SDL_GAMEPAD_TYPE_PS4:
            name = SDL_strdup("PS4 Controller");
            break;
        case SDL_GAMEPAD_TYPE_PS5:
            name = SDL_strdup("DualSense Wireless Controller");
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
            name = SDL_strdup("Nintendo Switch Pro Controller");
            break;
        default:
            len = (6 + 1 + 6 + 1);
            name = (char *)SDL_malloc(len);
            if (name) {
                (void)SDL_snprintf(name, len, "0x%.4x/0x%.4x", vendor, product);
            }
            break;
        }
    } else {
        name = SDL_strdup("Controller");
    }

    if (!name) {
        return NULL;
    }

    /* Trim trailing whitespace */
    for (len = SDL_strlen(name); (len > 0 && name[len - 1] == ' '); --len) {
        /* continue */
    }
    name[len] = '\0';

    /* Compress duplicate spaces */
    for (i = 0; i < (len - 1);) {
        if (name[i] == ' ' && name[i + 1] == ' ') {
            SDL_memmove(&name[i], &name[i + 1], (len - i));
            --len;
        } else {
            ++i;
        }
    }

    /* Perform any manufacturer replacements */
    for (i = 0; i < SDL_arraysize(replacements); ++i) {
        size_t prefixlen = SDL_strlen(replacements[i].prefix);
        if (SDL_strncasecmp(name, replacements[i].prefix, prefixlen) == 0) {
            size_t replacementlen = SDL_strlen(replacements[i].replacement);
            if (replacementlen <= prefixlen) {
                SDL_memcpy(name, replacements[i].replacement, replacementlen);
                SDL_memmove(name + replacementlen, name + prefixlen, (len - prefixlen) + 1);
                len -= (prefixlen - replacementlen);
            } else {
                /* FIXME: Need to handle the expand case by reallocating the string */
            }
            break;
        }
    }

    /* Remove duplicate manufacturer or product in the name
     * e.g. Razer Razer Raiju Tournament Edition Wired
     */
    for (i = 1; i < (len - 1); ++i) {
        int matchlen = PrefixMatch(name, &name[i]);
        while (matchlen > 0) {
            if (name[matchlen] == ' ' || name[matchlen] == '-') {
                SDL_memmove(name, name + matchlen + 1, len - matchlen);
                break;
            }
            --matchlen;
        }
        if (matchlen > 0) {
            /* We matched the manufacturer's name and removed it */
            break;
        }
    }

    return name;
}

SDL_JoystickGUID SDL_CreateJoystickGUID(Uint16 bus, Uint16 vendor, Uint16 product, Uint16 version, const char *vendor_name, const char *product_name, Uint8 driver_signature, Uint8 driver_data)
{
    SDL_JoystickGUID guid;
    Uint16 *guid16 = (Uint16 *)guid.data;
    Uint16 crc = 0;

    SDL_zero(guid);

    if (vendor_name && *vendor_name && product_name && *product_name) {
        crc = SDL_crc16(crc, vendor_name, SDL_strlen(vendor_name));
        crc = SDL_crc16(crc, " ", 1);
        crc = SDL_crc16(crc, product_name, SDL_strlen(product_name));
    } else if (product_name) {
        crc = SDL_crc16(crc, product_name, SDL_strlen(product_name));
    }

    /* We only need 16 bits for each of these; space them out to fill 128. */
    /* Byteswap so devices get same GUID on little/big endian platforms. */
    *guid16++ = SDL_SwapLE16(bus);
    *guid16++ = SDL_SwapLE16(crc);

    if (vendor) {
        *guid16++ = SDL_SwapLE16(vendor);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(product);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(version);
        guid.data[14] = driver_signature;
        guid.data[15] = driver_data;
    } else {
        size_t available_space = sizeof(guid.data) - 4;

        if (driver_signature) {
            available_space -= 2;
            guid.data[14] = driver_signature;
            guid.data[15] = driver_data;
        }
        if (product_name) {
            SDL_strlcpy((char *)guid16, product_name, available_space);
        }
    }
    return guid;
}

SDL_JoystickGUID SDL_CreateJoystickGUIDForName(const char *name)
{
    return SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_UNKNOWN, 0, 0, 0, NULL, name, 0, 0);
}

void SDL_SetJoystickGUIDVendor(SDL_JoystickGUID *guid, Uint16 vendor)
{
    Uint16 *guid16 = (Uint16 *)guid->data;

    guid16[2] = SDL_SwapLE16(vendor);
}

void SDL_SetJoystickGUIDProduct(SDL_JoystickGUID *guid, Uint16 product)
{
    Uint16 *guid16 = (Uint16 *)guid->data;

    guid16[4] = SDL_SwapLE16(product);
}

void SDL_SetJoystickGUIDVersion(SDL_JoystickGUID *guid, Uint16 version)
{
    Uint16 *guid16 = (Uint16 *)guid->data;

    guid16[6] = SDL_SwapLE16(version);
}

void SDL_SetJoystickGUIDCRC(SDL_JoystickGUID *guid, Uint16 crc)
{
    Uint16 *guid16 = (Uint16 *)guid->data;

    guid16[1] = SDL_SwapLE16(crc);
}

SDL_GamepadType SDL_GetGamepadTypeFromVIDPID(Uint16 vendor, Uint16 product, const char *name, SDL_bool forUI)
{
    SDL_GamepadType type = SDL_GAMEPAD_TYPE_STANDARD;

    if (vendor == 0x0000 && product == 0x0000) {
        /* Some devices are only identifiable by their name */
        if (name &&
            (SDL_strcmp(name, "Lic Pro Controller") == 0 ||
             SDL_strcmp(name, "Nintendo Wireless Gamepad") == 0 ||
             SDL_strcmp(name, "Wireless Gamepad") == 0)) {
            /* HORI or PowerA Switch Pro Controller clone */
            type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
        }

    } else if (vendor == 0x0001 && product == 0x0001) {
        type = SDL_GAMEPAD_TYPE_STANDARD;

    } else if (vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_LEFT) {
        type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT;

    } else if (vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_RIGHT) {
        if (name && SDL_strstr(name, "NES Controller") != NULL) {
            /* We don't have a type for the Nintendo Online NES Controller */
            type = SDL_GAMEPAD_TYPE_STANDARD;
        } else {
            type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT;
        }

    } else if (vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_GRIP) {
        if (name && SDL_strstr(name, "(L)") != NULL) {
            type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT;
        } else {
            type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT;
        }

    } else if (vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_PAIR) {
        type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;

    } else if (forUI && SDL_IsJoystickGameCube(vendor, product)) {
        /* We don't have a type for the Nintendo GameCube controller */
        type = SDL_GAMEPAD_TYPE_STANDARD;

    } else {
        switch (GuessControllerType(vendor, product)) {
        case k_eControllerType_XBox360Controller:
            type = SDL_GAMEPAD_TYPE_XBOX360;
            break;
        case k_eControllerType_XBoxOneController:
            type = SDL_GAMEPAD_TYPE_XBOXONE;
            break;
        case k_eControllerType_PS3Controller:
            type = SDL_GAMEPAD_TYPE_PS3;
            break;
        case k_eControllerType_PS4Controller:
            type = SDL_GAMEPAD_TYPE_PS4;
            break;
        case k_eControllerType_PS5Controller:
            type = SDL_GAMEPAD_TYPE_PS5;
            break;
        case k_eControllerType_XInputPS4Controller:
            if (forUI) {
                type = SDL_GAMEPAD_TYPE_PS4;
            } else {
                type = SDL_GAMEPAD_TYPE_STANDARD;
            }
            break;
        case k_eControllerType_SwitchProController:
        case k_eControllerType_SwitchInputOnlyController:
            type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
            break;
        case k_eControllerType_XInputSwitchController:
            if (forUI) {
                type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
            } else {
                type = SDL_GAMEPAD_TYPE_STANDARD;
            }
            break;
        default:
            break;
        }
    }
    return type;
}

SDL_GamepadType SDL_GetGamepadTypeFromGUID(SDL_JoystickGUID guid, const char *name)
{
    SDL_GamepadType type;
    Uint16 vendor, product;

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);
    type = SDL_GetGamepadTypeFromVIDPID(vendor, product, name, SDL_TRUE);
    if (type == SDL_GAMEPAD_TYPE_STANDARD) {
        if (SDL_IsJoystickXInput(guid)) {
            /* This is probably an Xbox One controller */
            return SDL_GAMEPAD_TYPE_XBOXONE;
        }
#ifdef SDL_JOYSTICK_HIDAPI
        if (SDL_IsJoystickHIDAPI(guid)) {
            return HIDAPI_GetGamepadTypeFromGUID(guid);
        }
#endif /* SDL_JOYSTICK_HIDAPI */
    }
    return type;
}

SDL_bool SDL_JoystickGUIDUsesVersion(SDL_JoystickGUID guid)
{
    Uint16 vendor, product;

    if (SDL_IsJoystickMFI(guid)) {
        /* The version bits are used as button capability mask */
        return SDL_FALSE;
    }

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);
    if (vendor && product) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

SDL_bool SDL_IsJoystickXboxOne(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_XBoxOneController;
}

SDL_bool SDL_IsJoystickXboxOneElite(Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_1 ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2 ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLE) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

SDL_bool SDL_IsJoystickXboxSeriesX(Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_SERIES_X ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_BLE) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_PDP) {
        if (product_id == USB_PRODUCT_XBOX_SERIES_X_VICTRIX_GAMBIT ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_PDP_BLUE ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_PDP_AFTERGLOW) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_POWERA_ALT) {
        if ((product_id >= 0x2001 && product_id <= 0x201a) ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_POWERA_FUSION_PRO2 ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_POWERA_MOGA_XP_ULTRA ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_POWERA_SPECTRA) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_HORI) {
        if (product_id == USB_PRODUCT_HORI_FIGHTING_COMMANDER_OCTA_SERIES_X ||
            product_id == USB_PRODUCT_HORI_HORIPAD_PRO_SERIES_X) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_HP) {
        if (product_id == USB_PRODUCT_XBOX_SERIES_X_HP_HYPERX ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_HP_HYPERX_RGB) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_RAZER) {
        if (product_id == USB_PRODUCT_RAZER_WOLVERINE_V2 ||
            product_id == USB_PRODUCT_RAZER_WOLVERINE_V2_CHROMA) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_THRUSTMASTER) {
        if (product_id == USB_PRODUCT_THRUSTMASTER_ESWAPX_PRO) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_TURTLE_BEACH) {
        if (product_id == USB_PRODUCT_TURTLE_BEACH_SERIES_X_REACT_R ||
            product_id == USB_PRODUCT_TURTLE_BEACH_SERIES_X_RECON) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_8BITDO) {
        if (product_id == USB_PRODUCT_8BITDO_XBOX_CONTROLLER1 ||
            product_id == USB_PRODUCT_8BITDO_XBOX_CONTROLLER2) {
            return SDL_TRUE;
        }
    }
    if (vendor_id == USB_VENDOR_GAMESIR) {
        if (product_id == USB_PRODUCT_GAMESIR_G7) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

SDL_bool SDL_IsJoystickBluetoothXboxOne(Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_ADAPTIVE_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_ADAPTIVE_BLE ||
            product_id == USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_S_REV2_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_S_REV2_BLE ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLE ||
            product_id == USB_PRODUCT_XBOX_SERIES_X_BLE) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

SDL_bool SDL_IsJoystickPS4(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_PS4Controller;
}

SDL_bool SDL_IsJoystickPS5(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_PS5Controller;
}

SDL_bool SDL_IsJoystickDualSenseEdge(Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == USB_VENDOR_SONY) {
        if (product_id == USB_PRODUCT_SONY_DS5_EDGE) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

SDL_bool SDL_IsJoystickNintendoSwitchPro(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SwitchProController || eType == k_eControllerType_SwitchInputOnlyController;
}

SDL_bool SDL_IsJoystickNintendoSwitchProInputOnly(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SwitchInputOnlyController;
}

SDL_bool SDL_IsJoystickNintendoSwitchJoyCon(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SwitchJoyConLeft || eType == k_eControllerType_SwitchJoyConRight;
}

SDL_bool SDL_IsJoystickNintendoSwitchJoyConLeft(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SwitchJoyConLeft;
}

SDL_bool SDL_IsJoystickNintendoSwitchJoyConRight(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SwitchJoyConRight;
}

SDL_bool SDL_IsJoystickNintendoSwitchJoyConGrip(Uint16 vendor_id, Uint16 product_id)
{
    return vendor_id == USB_VENDOR_NINTENDO && product_id == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_GRIP;
}

SDL_bool SDL_IsJoystickNintendoSwitchJoyConPair(Uint16 vendor_id, Uint16 product_id)
{
    return vendor_id == USB_VENDOR_NINTENDO && product_id == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_PAIR;
}

SDL_bool SDL_IsJoystickGameCube(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_VIDPIDInList(vendor_id, product_id, &gamecube_devices);
}

SDL_bool SDL_IsJoystickAmazonLunaController(Uint16 vendor_id, Uint16 product_id)
{
    return ((vendor_id == USB_VENDOR_AMAZON && product_id == USB_PRODUCT_AMAZON_LUNA_CONTROLLER) ||
            (vendor_id == BLUETOOTH_VENDOR_AMAZON && product_id == BLUETOOTH_PRODUCT_LUNA_CONTROLLER));
}

SDL_bool SDL_IsJoystickGoogleStadiaController(Uint16 vendor_id, Uint16 product_id)
{
    return vendor_id == USB_VENDOR_GOOGLE && product_id == USB_PRODUCT_GOOGLE_STADIA_CONTROLLER;
}

SDL_bool SDL_IsJoystickNVIDIASHIELDController(Uint16 vendor_id, Uint16 product_id)
{
    return (vendor_id == USB_VENDOR_NVIDIA &&
            (product_id == USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V103 ||
             product_id == USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V104));
}

SDL_bool SDL_IsJoystickSteamController(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SteamController || eType == k_eControllerType_SteamControllerV2;
}

SDL_bool SDL_IsJoystickSteamDeck(Uint16 vendor_id, Uint16 product_id)
{
    EControllerType eType = GuessControllerType(vendor_id, product_id);
    return eType == k_eControllerType_SteamControllerNeptune;
}

SDL_bool SDL_IsJoystickXInput(SDL_JoystickGUID guid)
{
    return (guid.data[14] == 'x') ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IsJoystickWGI(SDL_JoystickGUID guid)
{
    return (guid.data[14] == 'w') ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IsJoystickHIDAPI(SDL_JoystickGUID guid)
{
    return (guid.data[14] == 'h') ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IsJoystickMFI(SDL_JoystickGUID guid)
{
    return (guid.data[14] == 'm') ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IsJoystickRAWINPUT(SDL_JoystickGUID guid)
{
    return (guid.data[14] == 'r') ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IsJoystickVIRTUAL(SDL_JoystickGUID guid)
{
    return (guid.data[14] == 'v') ? SDL_TRUE : SDL_FALSE;
}

static SDL_bool SDL_IsJoystickWheel(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_VIDPIDInList(vendor_id, product_id, &wheel_devices);
}

static SDL_bool SDL_IsJoystickArcadeStick(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_VIDPIDInList(vendor_id, product_id, &arcadestick_devices);
}

static SDL_bool SDL_IsJoystickFlightStick(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_VIDPIDInList(vendor_id, product_id, &flightstick_devices);
}

static SDL_bool SDL_IsJoystickThrottle(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_VIDPIDInList(vendor_id, product_id, &throttle_devices);
}

static SDL_JoystickType SDL_GetJoystickGUIDType(SDL_JoystickGUID guid)
{
    Uint16 vendor;
    Uint16 product;

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);

    if (SDL_IsJoystickWheel(vendor, product)) {
        return SDL_JOYSTICK_TYPE_WHEEL;
    }

    if (SDL_IsJoystickArcadeStick(vendor, product)) {
        return SDL_JOYSTICK_TYPE_ARCADE_STICK;
    }

    if (SDL_IsJoystickFlightStick(vendor, product)) {
        return SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    }

    if (SDL_IsJoystickThrottle(vendor, product)) {
        return SDL_JOYSTICK_TYPE_THROTTLE;
    }

    if (SDL_IsJoystickXInput(guid)) {
        /* XInput GUID, get the type based on the XInput device subtype */
        switch (guid.data[15]) {
        case 0x01: /* XINPUT_DEVSUBTYPE_GAMEPAD */
            return SDL_JOYSTICK_TYPE_GAMEPAD;
        case 0x02: /* XINPUT_DEVSUBTYPE_WHEEL */
            return SDL_JOYSTICK_TYPE_WHEEL;
        case 0x03: /* XINPUT_DEVSUBTYPE_ARCADE_STICK */
            return SDL_JOYSTICK_TYPE_ARCADE_STICK;
        case 0x04: /* XINPUT_DEVSUBTYPE_FLIGHT_STICK */
            return SDL_JOYSTICK_TYPE_FLIGHT_STICK;
        case 0x05: /* XINPUT_DEVSUBTYPE_DANCE_PAD */
            return SDL_JOYSTICK_TYPE_DANCE_PAD;
        case 0x06: /* XINPUT_DEVSUBTYPE_GUITAR */
        case 0x07: /* XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE */
        case 0x0B: /* XINPUT_DEVSUBTYPE_GUITAR_BASS */
            return SDL_JOYSTICK_TYPE_GUITAR;
        case 0x08: /* XINPUT_DEVSUBTYPE_DRUM_KIT */
            return SDL_JOYSTICK_TYPE_DRUM_KIT;
        case 0x13: /* XINPUT_DEVSUBTYPE_ARCADE_PAD */
            return SDL_JOYSTICK_TYPE_ARCADE_PAD;
        default:
            return SDL_JOYSTICK_TYPE_UNKNOWN;
        }
    }

    if (SDL_IsJoystickWGI(guid)) {
        return (SDL_JoystickType)guid.data[15];
    }

    if (SDL_IsJoystickVIRTUAL(guid)) {
        return (SDL_JoystickType)guid.data[15];
    }

#ifdef SDL_JOYSTICK_HIDAPI
    if (SDL_IsJoystickHIDAPI(guid)) {
        return HIDAPI_GetJoystickTypeFromGUID(guid);
    }
#endif /* SDL_JOYSTICK_HIDAPI */

    if (GuessControllerType(vendor, product) != k_eControllerType_UnknownNonSteamController) {
        return SDL_JOYSTICK_TYPE_GAMEPAD;
    }

    return SDL_JOYSTICK_TYPE_UNKNOWN;
}

SDL_bool SDL_ShouldIgnoreJoystick(const char *name, SDL_JoystickGUID guid)
{
    Uint16 vendor;
    Uint16 product;

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);

    /* Check the joystick blacklist */
    if (SDL_VIDPIDInList(vendor, product, &blacklist_devices)) {
        return SDL_TRUE;
    }
    if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_ROG_CHAKRAM, SDL_FALSE)) {
        if (SDL_VIDPIDInList(vendor, product, &rog_gamepad_mice)) {
            return SDL_TRUE;
        }
    }

    if (SDL_ShouldIgnoreGamepad(name, guid)) {
        return SDL_TRUE;
    }

    return SDL_FALSE;
}

/* return the guid for this index */
SDL_JoystickGUID SDL_GetJoystickInstanceGUID(SDL_JoystickID instance_id)
{
    SDL_JoystickDriver *driver;
    int device_index;
    SDL_JoystickGUID guid;

    SDL_LockJoysticks();
    if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
        guid = driver->GetDeviceGUID(device_index);
    } else {
        SDL_zero(guid);
    }
    SDL_UnlockJoysticks();

    return guid;
}

Uint16 SDL_GetJoystickInstanceVendor(SDL_JoystickID instance_id)
{
    Uint16 vendor;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();
    info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
    if (info) {
        vendor = info->vendor_id;
    } else {
        SDL_JoystickGUID guid = SDL_GetJoystickInstanceGUID(instance_id);

        SDL_GetJoystickGUIDInfo(guid, &vendor, NULL, NULL, NULL);
    }
    SDL_UnlockJoysticks();

    return vendor;
}

Uint16 SDL_GetJoystickInstanceProduct(SDL_JoystickID instance_id)
{
    Uint16 product;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();
    info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
    if (info) {
        product = info->product_id;
    } else {
        SDL_JoystickGUID guid = SDL_GetJoystickInstanceGUID(instance_id);

        SDL_GetJoystickGUIDInfo(guid, NULL, &product, NULL, NULL);
    }
    SDL_UnlockJoysticks();

    return product;
}

Uint16 SDL_GetJoystickInstanceProductVersion(SDL_JoystickID instance_id)
{
    Uint16 version;
    SDL_JoystickGUID guid = SDL_GetJoystickInstanceGUID(instance_id);

    SDL_GetJoystickGUIDInfo(guid, NULL, NULL, &version, NULL);
    return version;
}

SDL_JoystickType SDL_GetJoystickInstanceType(SDL_JoystickID instance_id)
{
    SDL_JoystickType type;
    SDL_JoystickGUID guid = SDL_GetJoystickInstanceGUID(instance_id);

    type = SDL_GetJoystickGUIDType(guid);
    if (type == SDL_JOYSTICK_TYPE_UNKNOWN) {
        if (SDL_IsGamepad(instance_id)) {
            type = SDL_JOYSTICK_TYPE_GAMEPAD;
        }
    }
    return type;
}

SDL_JoystickGUID SDL_GetJoystickGUID(SDL_Joystick *joystick)
{
    SDL_JoystickGUID retval;

    SDL_LockJoysticks();
    {
        static SDL_JoystickGUID emptyGUID;

        CHECK_JOYSTICK_MAGIC(joystick, emptyGUID);

        retval = joystick->guid;
    }
    SDL_UnlockJoysticks();

    return retval;
}

Uint16 SDL_GetJoystickVendor(SDL_Joystick *joystick)
{
    Uint16 vendor;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        info = SDL_GetJoystickInstanceVirtualGamepadInfo(joystick->instance_id);
        if (info) {
            vendor = info->vendor_id;
        } else {
            SDL_JoystickGUID guid = SDL_GetJoystickGUID(joystick);

            SDL_GetJoystickGUIDInfo(guid, &vendor, NULL, NULL, NULL);
        }
    }
    SDL_UnlockJoysticks();

    return vendor;
}

Uint16 SDL_GetJoystickProduct(SDL_Joystick *joystick)
{
    Uint16 product;
    const SDL_SteamVirtualGamepadInfo *info;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        info = SDL_GetJoystickInstanceVirtualGamepadInfo(joystick->instance_id);
        if (info) {
            product = info->product_id;
        } else {
            SDL_JoystickGUID guid = SDL_GetJoystickGUID(joystick);

            SDL_GetJoystickGUIDInfo(guid, NULL, &product, NULL, NULL);
        }
    }
    SDL_UnlockJoysticks();

    return product;
}

Uint16 SDL_GetJoystickProductVersion(SDL_Joystick *joystick)
{
    Uint16 version;
    SDL_JoystickGUID guid = SDL_GetJoystickGUID(joystick);

    SDL_GetJoystickGUIDInfo(guid, NULL, NULL, &version, NULL);
    return version;
}

Uint16 SDL_GetJoystickFirmwareVersion(SDL_Joystick *joystick)
{
    Uint16 retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, 0);

        retval = joystick->firmware_version;
    }
    SDL_UnlockJoysticks();

    return retval;
}

const char *SDL_GetJoystickSerial(SDL_Joystick *joystick)
{
    const char *retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, NULL);

        retval = joystick->serial;
    }
    SDL_UnlockJoysticks();

    return retval;
}

SDL_JoystickType SDL_GetJoystickType(SDL_Joystick *joystick)
{
    SDL_JoystickType type;
    SDL_JoystickGUID guid = SDL_GetJoystickGUID(joystick);

    type = SDL_GetJoystickGUIDType(guid);
    if (type == SDL_JOYSTICK_TYPE_UNKNOWN) {
        SDL_LockJoysticks();
        {
            CHECK_JOYSTICK_MAGIC(joystick, SDL_JOYSTICK_TYPE_UNKNOWN);

            if (joystick->is_gamepad) {
                type = SDL_JOYSTICK_TYPE_GAMEPAD;
            }
        }
        SDL_UnlockJoysticks();
    }
    return type;
}

/* convert the guid to a printable string */
int SDL_GetJoystickGUIDString(SDL_JoystickGUID guid, char *pszGUID, int cbGUID)
{
    return SDL_GUIDToString(guid, pszGUID, cbGUID);
}

/* convert the string version of a joystick guid to the struct */
SDL_JoystickGUID SDL_GetJoystickGUIDFromString(const char *pchGUID)
{
    return SDL_GUIDFromString(pchGUID);
}

/* update the power level for this joystick */
void SDL_SendJoystickBatteryLevel(SDL_Joystick *joystick, SDL_JoystickPowerLevel ePowerLevel)
{
    SDL_AssertJoysticksLocked();

    SDL_assert(joystick->ref_count); /* make sure we are calling this only for update, not for initialization */
    if (ePowerLevel != joystick->epowerlevel) {
        if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_BATTERY_UPDATED)) {
            SDL_Event event;
            event.type = SDL_EVENT_JOYSTICK_BATTERY_UPDATED;
            event.common.timestamp = 0;
            event.jbattery.which = joystick->instance_id;
            event.jbattery.level = ePowerLevel;
            SDL_PushEvent(&event);
        }
        joystick->epowerlevel = ePowerLevel;
    }
}

/* return its power level */
SDL_JoystickPowerLevel SDL_GetJoystickPowerLevel(SDL_Joystick *joystick)
{
    SDL_JoystickPowerLevel retval;

    SDL_LockJoysticks();
    {
        CHECK_JOYSTICK_MAGIC(joystick, SDL_JOYSTICK_POWER_UNKNOWN);

        retval = joystick->epowerlevel;
    }
    SDL_UnlockJoysticks();

    return retval;
}

int SDL_SendJoystickTouchpad(Uint64 timestamp, SDL_Joystick *joystick, int touchpad, int finger, Uint8 state, float x, float y, float pressure)
{
    SDL_JoystickTouchpadInfo *touchpad_info;
    SDL_JoystickTouchpadFingerInfo *finger_info;
    int posted;
    Uint32 event_type;

    SDL_AssertJoysticksLocked();

    if (touchpad < 0 || touchpad >= joystick->ntouchpads) {
        return 0;
    }

    touchpad_info = &joystick->touchpads[touchpad];
    if (finger < 0 || finger >= touchpad_info->nfingers) {
        return 0;
    }

    finger_info = &touchpad_info->fingers[finger];

    if (!state) {
        if (x == 0.0f && y == 0.0f) {
            x = finger_info->x;
            y = finger_info->y;
        }
        pressure = 0.0f;
    }

    if (x < 0.0f) {
        x = 0.0f;
    } else if (x > 1.0f) {
        x = 1.0f;
    }
    if (y < 0.0f) {
        y = 0.0f;
    } else if (y > 1.0f) {
        y = 1.0f;
    }
    if (pressure < 0.0f) {
        pressure = 0.0f;
    } else if (pressure > 1.0f) {
        pressure = 1.0f;
    }

    if (state == finger_info->state) {
        if (!state ||
            (x == finger_info->x && y == finger_info->y && pressure == finger_info->pressure)) {
            return 0;
        }
    }

    if (state == finger_info->state) {
        event_type = SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION;
    } else if (state) {
        event_type = SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN;
    } else {
        event_type = SDL_EVENT_GAMEPAD_TOUCHPAD_UP;
    }

    /* We ignore events if we don't have keyboard focus, except for touch release */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if (event_type != SDL_EVENT_GAMEPAD_TOUCHPAD_UP) {
            return 0;
        }
    }

    /* Update internal joystick state */
    SDL_assert(timestamp != 0);
    finger_info->state = state;
    finger_info->x = x;
    finger_info->y = y;
    finger_info->pressure = pressure;
    joystick->update_complete = timestamp;

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(event_type)) {
        SDL_Event event;
        event.type = event_type;
        event.common.timestamp = timestamp;
        event.gtouchpad.which = joystick->instance_id;
        event.gtouchpad.touchpad = touchpad;
        event.gtouchpad.finger = finger;
        event.gtouchpad.x = x;
        event.gtouchpad.y = y;
        event.gtouchpad.pressure = pressure;
        posted = SDL_PushEvent(&event) == 1;
    }
    return posted;
}

int SDL_SendJoystickSensor(Uint64 timestamp, SDL_Joystick *joystick, SDL_SensorType type, Uint64 sensor_timestamp, const float *data, int num_values)
{
    int i;
    int posted = 0;

    SDL_AssertJoysticksLocked();

    /* We ignore events if we don't have keyboard focus */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        return 0;
    }

    for (i = 0; i < joystick->nsensors; ++i) {
        SDL_JoystickSensorInfo *sensor = &joystick->sensors[i];

        if (sensor->type == type) {
            if (sensor->enabled) {
                num_values = SDL_min(num_values, SDL_arraysize(sensor->data));

                /* Update internal sensor state */
                SDL_memcpy(sensor->data, data, num_values * sizeof(*data));
                joystick->update_complete = timestamp;

                /* Post the event, if desired */
                if (SDL_EventEnabled(SDL_EVENT_GAMEPAD_SENSOR_UPDATE)) {
                    SDL_Event event;
                    event.type = SDL_EVENT_GAMEPAD_SENSOR_UPDATE;
                    event.common.timestamp = timestamp;
                    event.gsensor.which = joystick->instance_id;
                    event.gsensor.sensor = type;
                    num_values = SDL_min(num_values,
                                         SDL_arraysize(event.gsensor.data));
                    SDL_memset(event.gsensor.data, 0,
                               sizeof(event.gsensor.data));
                    SDL_memcpy(event.gsensor.data, data,
                               num_values * sizeof(*data));
                    event.gsensor.sensor_timestamp = sensor_timestamp;
                    posted = SDL_PushEvent(&event) == 1;
                }
            }
            break;
        }
    }
    return posted;
}

static void SDL_LoadVIDPIDListFromHint(const char *hint, int *num_entries, int *max_entries, Uint32 **entries)
{
    Uint32 entry;
    char *spot;
    char *file = NULL;

    if (hint && *hint == '@') {
        spot = file = (char *)SDL_LoadFile(hint + 1, NULL);
    } else {
        spot = (char *)hint;
    }

    if (!spot) {
        return;
    }

    while ((spot = SDL_strstr(spot, "0x")) != NULL) {
        entry = (Uint16)SDL_strtol(spot, &spot, 0);
        entry <<= 16;
        spot = SDL_strstr(spot, "0x");
        if (!spot) {
            break;
        }
        entry |= (Uint16)SDL_strtol(spot, &spot, 0);

        if (*num_entries == *max_entries) {
            int new_max_entries = *max_entries + 16;
            Uint32 *new_entries = (Uint32 *)SDL_realloc(*entries, new_max_entries * sizeof(**entries));
            if (!new_entries) {
                /* Out of memory, go with what we have already */
                break;
            }
            *entries = new_entries;
            *max_entries = new_max_entries;
        }
        (*entries)[(*num_entries)++] = entry;
    }

    if (file) {
        SDL_free(file);
    }
}

void SDL_LoadVIDPIDListFromHints(SDL_vidpid_list *list, const char *included_list, const char *excluded_list)
{
    /* Empty the list */
    list->num_included_entries = 0;
    list->num_excluded_entries = 0;

    /* Add the initial entries */
    if (list->num_initial_entries > 0) {
        if (list->num_included_entries < list->num_initial_entries) {
            Uint32 *entries = (Uint32 *)SDL_malloc(list->num_initial_entries * sizeof(*entries));
            if (entries) {
                SDL_memcpy(entries, list->initial_entries, list->num_initial_entries * sizeof(*entries));
                list->included_entries = entries;
                list->num_included_entries = list->num_initial_entries;
                list->max_included_entries = list->num_initial_entries;
            }
        }
    }

    /* Add the included entries from the hint */
    SDL_LoadVIDPIDListFromHint(included_list, &list->num_included_entries, &list->max_included_entries, &list->included_entries);

    /* Add the excluded entries from the hint */
    SDL_LoadVIDPIDListFromHint(excluded_list, &list->num_excluded_entries, &list->max_excluded_entries, &list->excluded_entries);
}

static void SDLCALL SDL_VIDPIDIncludedHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_vidpid_list *list = (SDL_vidpid_list *)userdata;
    const char *included_list = hint;
    const char *excluded_list = NULL;

    if (!list->initialized) {
        return;
    }

    if (list->excluded_hint_name) {
        excluded_list = SDL_GetHint(list->excluded_hint_name);
    }
    SDL_LoadVIDPIDListFromHints(list, included_list, excluded_list);
}

static void SDLCALL SDL_VIDPIDExcludedHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_vidpid_list *list = (SDL_vidpid_list *)userdata;
    const char *included_list = NULL;
    const char *excluded_list = hint;

    if (!list->initialized) {
        return;
    }

    if (list->included_hint_name) {
        included_list = SDL_GetHint(list->included_hint_name);
    }
    SDL_LoadVIDPIDListFromHints(list, included_list, excluded_list);
}

void SDL_LoadVIDPIDList(SDL_vidpid_list *list)
{
    const char *included_list = NULL;
    const char *excluded_list = NULL;

    if (list->included_hint_name) {
        SDL_AddHintCallback(list->included_hint_name, SDL_VIDPIDIncludedHintChanged, list);
    }

    if (list->excluded_hint_name) {
        SDL_AddHintCallback(list->excluded_hint_name, SDL_VIDPIDExcludedHintChanged, list);
    }

    list->initialized = SDL_TRUE;

    if (list->included_hint_name) {
        included_list = SDL_GetHint(list->included_hint_name);
    }
    if (list->excluded_hint_name) {
        excluded_list = SDL_GetHint(list->excluded_hint_name);
    }
    SDL_LoadVIDPIDListFromHints(list, included_list, excluded_list);
}

SDL_bool SDL_VIDPIDInList(Uint16 vendor_id, Uint16 product_id, const SDL_vidpid_list *list)
{
    int i;
    Uint32 vidpid = MAKE_VIDPID(vendor_id, product_id);

    for (i = 0; i < list->num_excluded_entries; ++i) {
        if (vidpid == list->excluded_entries[i]) {
            return SDL_FALSE;
        }
    }
    for (i = 0; i < list->num_included_entries; ++i) {
        if (vidpid == list->included_entries[i]) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

void SDL_FreeVIDPIDList(SDL_vidpid_list *list)
{
    if (list->included_hint_name) {
        SDL_DelHintCallback(list->included_hint_name, SDL_VIDPIDIncludedHintChanged, list);
    }

    if (list->excluded_hint_name) {
        SDL_DelHintCallback(list->excluded_hint_name, SDL_VIDPIDExcludedHintChanged, list);
    }

    if (list->included_entries) {
        SDL_free(list->included_entries);
        list->included_entries = NULL;
        list->num_included_entries = 0;
        list->max_included_entries = 0;
    }

    if (list->excluded_entries) {
        SDL_free(list->excluded_entries);
        list->excluded_entries = NULL;
        list->num_excluded_entries = 0;
        list->max_excluded_entries = 0;
    }

    list->initialized = SDL_FALSE;
}
