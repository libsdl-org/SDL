/*
  Copyright (C) Valve Corporation

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

#ifndef CONTROLLER_TYPE_H
#define CONTROLLER_TYPE_H
#ifdef _WIN32
#pragma once
#endif

#ifndef __cplusplus
#define inline SDL_INLINE
#endif

//-----------------------------------------------------------------------------
// Purpose: Steam Controller models 
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN A DATABASE
//-----------------------------------------------------------------------------
typedef enum
{
	k_eControllerType_None = -1,
	k_eControllerType_Unknown = 0,

	// Steam Controllers
	k_eControllerType_UnknownSteamController = 1,
	k_eControllerType_SteamController = 2,
	k_eControllerType_SteamControllerV2 = 3,

	// Other Controllers
	k_eControllerType_UnknownNonSteamController = 30,
	k_eControllerType_XBox360Controller = 31,
	k_eControllerType_XBoxOneController = 32,
	k_eControllerType_PS3Controller = 33,
	k_eControllerType_PS4Controller = 34,
	k_eControllerType_WiiController = 35,
	k_eControllerType_AppleController = 36,
	k_eControllerType_AndroidController = 37,
	k_eControllerType_SwitchProController = 38,
	k_eControllerType_SwitchJoyConLeft = 39,
	k_eControllerType_SwitchJoyConRight = 40,
	k_eControllerType_SwitchJoyConPair = 41,
    k_eControllerType_SwitchInputOnlyController = 42,
	k_eControllerType_MobileTouch = 43,
	k_eControllerType_LastController,			// Don't add game controllers below this enumeration - this enumeration can change value

	// Keyboards and Mice
	k_eControllertype_GenericKeyboard = 400,
	k_eControllertype_GenericMouse = 800,
} EControllerType;

static inline SDL_bool BIsSteamController( EControllerType eType )
{
	return ( eType == k_eControllerType_SteamController || eType == k_eControllerType_SteamControllerV2 );
}

#define MAKE_CONTROLLER_ID( nVID, nPID )	(unsigned int)( nVID << 16 | nPID )
typedef struct
{
	unsigned int m_unDeviceID;
	EControllerType m_eControllerType;
} ControllerDescription_t;

static const ControllerDescription_t arrControllers[] = {
	{ MAKE_CONTROLLER_ID( 0x0079, 0x18d4 ), k_eControllerType_XBox360Controller },	// GPD Win 2 X-Box Controller
	{ MAKE_CONTROLLER_ID( 0x044f, 0xb326 ), k_eControllerType_XBox360Controller },	// Thrustmaster Gamepad GP XID
	{ MAKE_CONTROLLER_ID( 0x045e, 0x028e ), k_eControllerType_XBox360Controller },	// Microsoft X-Box 360 pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x028f ), k_eControllerType_XBox360Controller },	// Microsoft X-Box 360 pad v2
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0291 ), k_eControllerType_XBox360Controller },	// Xbox 360 Wireless Receiver (XBOX)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a0 ), k_eControllerType_XBox360Controller },	// Microsoft X-Box 360 Big Button IR
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a1 ), k_eControllerType_XBox360Controller },	// Microsoft X-Box 360 pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02d1 ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02dd ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One pad (Firmware 2015)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02e0 ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One S pad (Bluetooth)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02e3 ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One Elite pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02ea ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One S pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02fd ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One S pad (Bluetooth)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02ff ), k_eControllerType_XBoxOneController },	// Microsoft X-Box One Elite pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0719 ), k_eControllerType_XBox360Controller },	// Xbox 360 Wireless Receiver
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc21d ), k_eControllerType_XBox360Controller },	// Logitech Gamepad F310
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc21e ), k_eControllerType_XBox360Controller },	// Logitech Gamepad F510
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc21f ), k_eControllerType_XBox360Controller },	// Logitech Gamepad F710
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc242 ), k_eControllerType_XBox360Controller },	// Logitech Chillstream Controller

	{ MAKE_CONTROLLER_ID( 0x054c, 0x0268 ), k_eControllerType_PS3Controller },		// Sony PS3 Controller
	{ MAKE_CONTROLLER_ID( 0x0925, 0x0005 ), k_eControllerType_PS3Controller },		// Sony PS3 Controller
	{ MAKE_CONTROLLER_ID( 0x8888, 0x0308 ), k_eControllerType_PS3Controller },		// Sony PS3 Controller
	{ MAKE_CONTROLLER_ID( 0x1a34, 0x0836 ), k_eControllerType_PS3Controller },		// Afterglow ps3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x006e ), k_eControllerType_PS3Controller },		// HORI horipad4 ps3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0066 ), k_eControllerType_PS3Controller },		// HORI horipad4 ps4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x005f ), k_eControllerType_PS3Controller },		// HORI Fighting commander ps3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x005e ), k_eControllerType_PS3Controller },		// HORI Fighting commander ps4
	//{ MAKE_CONTROLLER_ID( 0x0738, 0x3250 ), k_eControllerType_PS3Controller },		// madcats fightpad pro ps3 already in ps4 list.. does this work??
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8250 ), k_eControllerType_PS3Controller },		// madcats fightpad pro ps4
	{ MAKE_CONTROLLER_ID( 0x0079, 0x181a ), k_eControllerType_PS3Controller },		// Venom Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x0079, 0x0006 ), k_eControllerType_PS3Controller },		// PC Twin Shock Controller - looks like a DS3 but the face buttons are 1-4 instead of symbols
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1844 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x8888, 0x0308 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x2563, 0x0575 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x0810, 0x0001 ), k_eControllerType_PS3Controller },		// actually ps2 - maybe break out later
	{ MAKE_CONTROLLER_ID( 0x0810, 0x0003 ), k_eControllerType_PS3Controller },		// actually ps2 - maybe break out later
	{ MAKE_CONTROLLER_ID( 0x2563, 0x0523 ), k_eControllerType_PS3Controller },		// Digiflip GP006
	{ MAKE_CONTROLLER_ID( 0x11ff, 0x3331 ), k_eControllerType_PS3Controller },		// SRXJ-PH2400
	{ MAKE_CONTROLLER_ID( 0x20bc, 0x5500 ), k_eControllerType_PS3Controller },		// ShanWan PS3
	{ MAKE_CONTROLLER_ID( 0x05b8, 0x1004 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0603 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x044f, 0xb315 ), k_eControllerType_PS3Controller },		// Firestorm Dual Analog 3
	{ MAKE_CONTROLLER_ID( 0x0925, 0x8888 ), k_eControllerType_PS3Controller },		// Actually ps2 -maybe break out later Lakeview Research WiseGroup Ltd, MP-8866 Dual Joypad
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x004d ), k_eControllerType_PS3Controller },		// Horipad 3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0009 ), k_eControllerType_PS3Controller },		// HORI BDA GP1
	{ MAKE_CONTROLLER_ID( 0x0e8f, 0x0008 ), k_eControllerType_PS3Controller },		// Green Asia
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x006a ), k_eControllerType_PS3Controller },		// Real Arcade Pro 4
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x011e ), k_eControllerType_PS3Controller },		// Rock Candy PS4
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0214 ), k_eControllerType_PS3Controller },		// afterglow ps3
	{ MAKE_CONTROLLER_ID( 0x0925, 0x8866 ), k_eControllerType_PS3Controller },		// PS2 maybe break out later
	{ MAKE_CONTROLLER_ID( 0x0e8f, 0x310d ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2003 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x056e, 0x2013 ), k_eControllerType_PS3Controller },		// JC-U4113SBK
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8838 ), k_eControllerType_PS3Controller },		// Madcatz Fightstick Pro
	{ MAKE_CONTROLLER_ID( 0x1a34, 0x0836 ), k_eControllerType_PS3Controller },		// Afterglow PS3
	{ MAKE_CONTROLLER_ID( 0x0f30, 0x1100 ), k_eControllerType_PS3Controller },		// Quanba Q1 fight stick
	{ MAKE_CONTROLLER_ID( 0x1345, 0x6005 ), k_eControllerType_PS3Controller },		// ps2 maybe break out later
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0087 ), k_eControllerType_PS3Controller },		// HORI fighting mini stick
	{ MAKE_CONTROLLER_ID( 0x146b, 0x5500 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xca6d ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x25f0, 0xc121 ), k_eControllerType_PS3Controller },		//
	{ MAKE_CONTROLLER_ID( 0x8380, 0x0003 ), k_eControllerType_PS3Controller },		// BTP 2163
	{ MAKE_CONTROLLER_ID( 0x1345, 0x1000 ), k_eControllerType_PS3Controller },		// PS2 ACME GA-D5
	{ MAKE_CONTROLLER_ID( 0x0e8f, 0x3075 ), k_eControllerType_PS3Controller },		// SpeedLink Strike FX
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0128 ), k_eControllerType_PS3Controller },		// Rock Candy PS3
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2000 ), k_eControllerType_PS3Controller },		// Quanba Drone
	{ MAKE_CONTROLLER_ID( 0x06a3, 0xf622 ), k_eControllerType_PS3Controller },		// Cyborg V3
	{ MAKE_CONTROLLER_ID( 0x044f, 0xd007 ), k_eControllerType_PS3Controller },		// Thrustmaster wireless 3-1
	{ MAKE_CONTROLLER_ID( 0x25f0, 0x83c3 ), k_eControllerType_PS3Controller },		// gioteck vx2
	{ MAKE_CONTROLLER_ID( 0x05b8, 0x1006 ), k_eControllerType_PS3Controller },		// JC-U3412SBK
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x576d ), k_eControllerType_PS3Controller },		// Power A PS3
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x6302 ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x056e, 0x200f ), k_eControllerType_PS3Controller },		// From SDL
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x1314 ), k_eControllerType_PS3Controller },		// PDP Afterglow Wireless PS3 controller

	{ MAKE_CONTROLLER_ID( 0x054c, 0x05c4 ), k_eControllerType_PS4Controller },		// Sony PS4 Controller
	{ MAKE_CONTROLLER_ID( 0x054c, 0x09cc ), k_eControllerType_PS4Controller },		// Sony PS4 Slim Controller
	{ MAKE_CONTROLLER_ID( 0x054c, 0x0ba0 ), k_eControllerType_PS4Controller },		// Sony PS4 Controller (Wireless dongle)
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x008a ), k_eControllerType_PS4Controller },		// HORI Real Arcade Pro 4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0055 ), k_eControllerType_PS4Controller },		// HORIPAD 4 FPS
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0066 ), k_eControllerType_PS4Controller },		// HORIPAD 4 FPS Plus 
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8384 ), k_eControllerType_PS4Controller },		// Mad Catz FightStick TE S+ PS4
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8250 ), k_eControllerType_PS4Controller },		// Mad Catz FightPad Pro PS4
	{ MAKE_CONTROLLER_ID( 0x0C12, 0x0E10 ), k_eControllerType_PS4Controller },		// Armor Armor 3 Pad PS4
	{ MAKE_CONTROLLER_ID( 0x0C12, 0x1CF6 ), k_eControllerType_PS4Controller },		// EMIO PS4 Elite Controller
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1000 ), k_eControllerType_PS4Controller },		// Razer Raiju PS4 Controller
	{ MAKE_CONTROLLER_ID( 0x1532, 0X0401 ), k_eControllerType_PS4Controller },		// Razer Panthera PS4 Controller
	{ MAKE_CONTROLLER_ID( 0x054c, 0x05c5 ), k_eControllerType_PS4Controller },		// STRIKEPAD PS4 Grip Add-on
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d01 ), k_eControllerType_PS4Controller },		// Nacon Revolution Pro Controller - has gyro
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d02 ), k_eControllerType_PS4Controller },		// Nacon Revolution Pro Controller v2 - has gyro
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00a0 ), k_eControllerType_PS4Controller },		// HORI TAC4 mousething
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x009c ), k_eControllerType_PS4Controller },		// HORI TAC PRO mousething
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0ef6 ), k_eControllerType_PS4Controller },		// Hitbox Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x0079, 0x181b ), k_eControllerType_PS4Controller },		// Venom Arcade Stick - XXX:this may not work and may need to be called a ps3 controller
	{ MAKE_CONTROLLER_ID( 0x0738, 0x3250 ), k_eControllerType_PS4Controller },		// Mad Catz FightPad PRO - controller shaped with 6 face buttons
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00ee ), k_eControllerType_PS4Controller },		// Hori mini wired https://www.playstation.com/en-us/explore/accessories/gaming-controllers/mini-wired-gamepad/
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8481 ), k_eControllerType_PS4Controller },		// Mad Catz FightStick TE 2+ PS4
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8480 ), k_eControllerType_PS4Controller },		// Mad Catz FightStick TE 2 PS4
	{ MAKE_CONTROLLER_ID( 0x7545, 0x0104 ), k_eControllerType_PS4Controller },		// Armor 3 or Level Up Cobra - At least one variant has gyro
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0e15 ), k_eControllerType_PS4Controller },		// Game:Pad 4
	{ MAKE_CONTROLLER_ID( 0x11c0, 0x4001 ), k_eControllerType_PS4Controller },		// "PS4 Fun Controller" added from user log

	{ MAKE_CONTROLLER_ID( 0x1532, 0x1007 ), k_eControllerType_PS4Controller },		// Razer Raiju 2 Tournament edition USB- untested and added for razer
	{ MAKE_CONTROLLER_ID( 0x1532, 0x100A ), k_eControllerType_PS4Controller },		// Razer Raiju 2 Tournament edition BT - untested and added for razer
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1004 ), k_eControllerType_PS4Controller },		// Razer Raiju 2 Ultimate USB - untested and added for razer
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1009 ), k_eControllerType_PS4Controller },		// Razer Raiju 2 Ultimate BT - untested and added for razer
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1008 ), k_eControllerType_PS4Controller },		// Razer Panthera Evo Fightstick - untested and added for razer

	{ MAKE_CONTROLLER_ID( 0x056e, 0x2004 ), k_eControllerType_XBox360Controller },	// Elecom JC-U3613M
	{ MAKE_CONTROLLER_ID( 0x06a3, 0xf51a ), k_eControllerType_XBox360Controller },	// Saitek P3600
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4716 ), k_eControllerType_XBox360Controller },	// Mad Catz Wired Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4718 ), k_eControllerType_XBox360Controller },	// Mad Catz Street Fighter IV FightStick SE
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4726 ), k_eControllerType_XBox360Controller },	// Mad Catz Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4728 ), k_eControllerType_XBox360Controller },	// Mad Catz Street Fighter IV FightPad
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4736 ), k_eControllerType_XBox360Controller },	// Mad Catz MicroCon Gamepad
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4738 ), k_eControllerType_XBox360Controller },	// Mad Catz Wired Xbox 360 Controller (SFIV)
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4740 ), k_eControllerType_XBox360Controller },	// Mad Catz Beat Pad
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4a01 ), k_eControllerType_XBoxOneController },	// Mad Catz FightStick TE 2
	{ MAKE_CONTROLLER_ID( 0x0738, 0xb726 ), k_eControllerType_XBox360Controller },	// Mad Catz Xbox controller - MW2
	{ MAKE_CONTROLLER_ID( 0x0738, 0xbeef ), k_eControllerType_XBox360Controller },	// Mad Catz JOYTECH NEO SE Advanced GamePad
	{ MAKE_CONTROLLER_ID( 0x0738, 0xcb02 ), k_eControllerType_XBox360Controller },	// Saitek Cyborg Rumble Pad - PC/Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0738, 0xcb03 ), k_eControllerType_XBox360Controller },	// Saitek P3200 Rumble Pad - PC/Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0738, 0xf738 ), k_eControllerType_XBox360Controller },	// Super SFIV FightStick TE S
	{ MAKE_CONTROLLER_ID( 0x0955, 0xb400 ), k_eControllerType_XBox360Controller },	// NVIDIA Shield streaming controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0105 ), k_eControllerType_XBox360Controller },	// HSM3 Xbox360 dancepad
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0113 ), k_eControllerType_XBox360Controller },	// Afterglow AX.1 Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x011f ), k_eControllerType_XBox360Controller },	// Rock Candy Gamepad Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0131 ), k_eControllerType_XBox360Controller },	// PDP EA Sports Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0133 ), k_eControllerType_XBox360Controller },	// Xbox 360 Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0139 ), k_eControllerType_XBoxOneController },	// Afterglow Prismatic Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x013a ), k_eControllerType_XBoxOneController },	// PDP Xbox One Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0146 ), k_eControllerType_XBoxOneController },	// Rock Candy Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0147 ), k_eControllerType_XBoxOneController },	// PDP Marvel Xbox One Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x015c ), k_eControllerType_XBoxOneController },	// PDP Xbox One Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0161 ), k_eControllerType_XBoxOneController },	// PDP Xbox One Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0162 ), k_eControllerType_XBoxOneController },	// PDP Xbox One Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0163 ), k_eControllerType_XBoxOneController },	// PDP Xbox One Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0164 ), k_eControllerType_XBoxOneController },	// PDP Battlefield One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0165 ), k_eControllerType_XBoxOneController },	// PDP Titanfall 2
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0201 ), k_eControllerType_XBox360Controller },	// Pelican PL-3601 'TSZ' Wired Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0213 ), k_eControllerType_XBox360Controller },	// Afterglow Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x021f ), k_eControllerType_XBox360Controller },	// Rock Candy Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0246 ), k_eControllerType_XBoxOneController },	// Rock Candy Gamepad for Xbox One 2015
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a0 ), k_eControllerType_XBox360Controller },	// Counterfeit 360Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0301 ), k_eControllerType_XBox360Controller },	// Logic3 Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0346 ), k_eControllerType_XBoxOneController },	// Rock Candy Gamepad for Xbox One 2016
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0401 ), k_eControllerType_XBox360Controller },	// Logic3 Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0413 ), k_eControllerType_XBox360Controller },	// Afterglow AX.1 Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0501 ), k_eControllerType_XBox360Controller },	// PDP Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0xf501 ), k_eControllerType_XBox360Controller },	// Counterfeit 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0xf900 ), k_eControllerType_XBox360Controller },	// PDP Afterglow AX.1
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x000a ), k_eControllerType_XBox360Controller },	// Hori Co. DOA4 FightStick
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x000c ), k_eControllerType_XBox360Controller },	// Hori PadEX Turbo
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x000d ), k_eControllerType_XBox360Controller },	// Hori Fighting Stick EX2
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0016 ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro.EX
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x001b ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro VX
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0063 ), k_eControllerType_XBoxOneController },	// Hori Real Arcade Pro Hayabusa (USA) Xbox One
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0067 ), k_eControllerType_XBoxOneController },	// HORIPAD ONE
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0078 ), k_eControllerType_XBoxOneController },	// Hori Real Arcade Pro V Kai Xbox One
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x008c ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro 4
	{ MAKE_CONTROLLER_ID( 0x11c9, 0x55f0 ), k_eControllerType_XBox360Controller },	// Nacon GC-100XF
	{ MAKE_CONTROLLER_ID( 0x12ab, 0x0004 ), k_eControllerType_XBox360Controller },	// Honey Bee Xbox360 dancepad
	{ MAKE_CONTROLLER_ID( 0x12ab, 0x0301 ), k_eControllerType_XBox360Controller },	// PDP AFTERGLOW AX.1
	{ MAKE_CONTROLLER_ID( 0x12ab, 0x0303 ), k_eControllerType_XBox360Controller },	// Mortal Kombat Klassic FightStick
	{ MAKE_CONTROLLER_ID( 0x1430, 0x02a0 ), k_eControllerType_XBox360Controller },	// RedOctane Controller Adapter
	{ MAKE_CONTROLLER_ID( 0x1430, 0x4748 ), k_eControllerType_XBox360Controller },	// RedOctane Guitar Hero X-plorer
	{ MAKE_CONTROLLER_ID( 0x1430, 0xf801 ), k_eControllerType_XBox360Controller },	// RedOctane Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0601 ), k_eControllerType_XBox360Controller },	// BigBen Interactive XBOX 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0037 ), k_eControllerType_XBox360Controller },	// Razer Sabertooth
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0a00 ), k_eControllerType_XBoxOneController },	// Razer Atrox Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0a03 ), k_eControllerType_XBoxOneController },	// Razer Wildcat
	{ MAKE_CONTROLLER_ID( 0x15e4, 0x3f00 ), k_eControllerType_XBox360Controller },	// Power A Mini Pro Elite
	{ MAKE_CONTROLLER_ID( 0x15e4, 0x3f0a ), k_eControllerType_XBox360Controller },	// Xbox Airflo wired controller
	{ MAKE_CONTROLLER_ID( 0x15e4, 0x3f10 ), k_eControllerType_XBox360Controller },	// Batarang Xbox 360 controller
	{ MAKE_CONTROLLER_ID( 0x162e, 0xbeef ), k_eControllerType_XBox360Controller },	// Joytech Neo-Se Take2
	{ MAKE_CONTROLLER_ID( 0x1689, 0xfd00 ), k_eControllerType_XBox360Controller },	// Razer Onza Tournament Edition
	{ MAKE_CONTROLLER_ID( 0x1689, 0xfd01 ), k_eControllerType_XBox360Controller },	// Razer Onza Classic Edition
	{ MAKE_CONTROLLER_ID( 0x1689, 0xfe00 ), k_eControllerType_XBox360Controller },	// Razer Sabertooth
	{ MAKE_CONTROLLER_ID( 0x1bad, 0x0002 ), k_eControllerType_XBox360Controller },	// Harmonix Rock Band Guitar
	{ MAKE_CONTROLLER_ID( 0x1bad, 0x0003 ), k_eControllerType_XBox360Controller },	// Harmonix Rock Band Drumkit
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf016 ), k_eControllerType_XBox360Controller },	// Mad Catz Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf018 ), k_eControllerType_XBox360Controller },	// Mad Catz Street Fighter IV SE Fighting Stick
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf019 ), k_eControllerType_XBox360Controller },	// Mad Catz Brawlstick for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf021 ), k_eControllerType_XBox360Controller },	// Mad Cats Ghost Recon FS GamePad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf023 ), k_eControllerType_XBox360Controller },	// MLG Pro Circuit Controller (Xbox)
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf025 ), k_eControllerType_XBox360Controller },	// Mad Catz Call Of Duty
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf027 ), k_eControllerType_XBox360Controller },	// Mad Catz FPS Pro
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf028 ), k_eControllerType_XBox360Controller },	// Street Fighter IV FightPad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf02e ), k_eControllerType_XBox360Controller },	// Mad Catz Fightpad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf036 ), k_eControllerType_XBox360Controller },	// Mad Catz MicroCon GamePad Pro
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf038 ), k_eControllerType_XBox360Controller },	// Street Fighter IV FightStick TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf039 ), k_eControllerType_XBox360Controller },	// Mad Catz MvC2 TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03a ), k_eControllerType_XBox360Controller },	// Mad Catz SFxT Fightstick Pro
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03d ), k_eControllerType_XBox360Controller },	// Street Fighter IV Arcade Stick TE - Chun Li
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03e ), k_eControllerType_XBox360Controller },	// Mad Catz MLG FightStick TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03f ), k_eControllerType_XBox360Controller },	// Mad Catz FightStick SoulCaliber
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf042 ), k_eControllerType_XBox360Controller },	// Mad Catz FightStick TES+
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf080 ), k_eControllerType_XBox360Controller },	// Mad Catz FightStick TE2
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf501 ), k_eControllerType_XBox360Controller },	// HoriPad EX2 Turbo
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf502 ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro.VX SA
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf503 ), k_eControllerType_XBox360Controller },	// Hori Fighting Stick VX
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf504 ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro. EX
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf505 ), k_eControllerType_XBox360Controller },	// Hori Fighting Stick EX2B
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf506 ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro.EX Premium VLX
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf900 ), k_eControllerType_XBox360Controller },	// Harmonix Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf901 ), k_eControllerType_XBox360Controller },	// Gamestop Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf902 ), k_eControllerType_XBox360Controller },	// Mad Catz Gamepad2
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf903 ), k_eControllerType_XBox360Controller },	// Tron Xbox 360 controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf904 ), k_eControllerType_XBox360Controller },	// PDP Versus Fighting Pad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf906 ), k_eControllerType_XBox360Controller },	// MortalKombat FightStick
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xfa01 ), k_eControllerType_XBox360Controller },	// MadCatz GamePad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xfd00 ), k_eControllerType_XBox360Controller },	// Razer Onza TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xfd01 ), k_eControllerType_XBox360Controller },	// Razer Onza
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5000 ), k_eControllerType_XBox360Controller },	// Razer Atrox Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5300 ), k_eControllerType_XBox360Controller },	// PowerA MINI PROEX Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5303 ), k_eControllerType_XBox360Controller },	// Xbox Airflo wired controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x530a ), k_eControllerType_XBox360Controller },	// Xbox 360 Pro EX Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x531a ), k_eControllerType_XBox360Controller },	// PowerA Pro Ex
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5397 ), k_eControllerType_XBox360Controller },	// FUS1ON Tournament Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x541a ), k_eControllerType_XBoxOneController },	// PowerA Xbox One Mini Wired Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x542a ), k_eControllerType_XBoxOneController },	// Xbox ONE spectra
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x543a ), k_eControllerType_XBoxOneController },	// PowerA Xbox One wired controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5500 ), k_eControllerType_XBox360Controller },	// Hori XBOX 360 EX 2 with Turbo
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5501 ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro VX-SA
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5502 ), k_eControllerType_XBox360Controller },	// Hori Fighting Stick VX Alt
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5503 ), k_eControllerType_XBox360Controller },	// Hori Fighting Edge
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5506 ), k_eControllerType_XBox360Controller },	// Hori SOULCALIBUR V Stick
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5510 ), k_eControllerType_XBox360Controller },	// Hori Fighting Commander ONE
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x550d ), k_eControllerType_XBox360Controller },	// Hori GEM Xbox controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x550e ), k_eControllerType_XBox360Controller },	// Hori Real Arcade Pro V Kai 360
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x551a ), k_eControllerType_XBoxOneController },	// PowerA FUSION Pro Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x561a ), k_eControllerType_XBoxOneController },	// PowerA FUSION Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5b00 ), k_eControllerType_XBox360Controller },	// ThrustMaster Ferrari Italia 458 Racing Wheel
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5b02 ), k_eControllerType_XBox360Controller },	// Thrustmaster, Inc. GPX Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5b03 ), k_eControllerType_XBox360Controller },	// Thrustmaster Ferrari 458 Racing Wheel
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5d04 ), k_eControllerType_XBox360Controller },	// Razer Sabertooth
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafa ), k_eControllerType_XBox360Controller },	// Aplay Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafb ), k_eControllerType_XBox360Controller },	// Aplay Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafc ), k_eControllerType_XBox360Controller },	// Afterglow Gamepad 1
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafe ), k_eControllerType_XBox360Controller },	// Rock Candy Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafd ), k_eControllerType_XBox360Controller },	// Afterglow Gamepad 3
	
	// These have been added via Minidump for unrecognized Xinput controller assert
	{ MAKE_CONTROLLER_ID( 0x0000, 0x0000 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a2 ), k_eControllerType_XBox360Controller },	// Unknown Controller - Microsoft VID
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x1414 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x1314 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0159 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfaff ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0086 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x006d ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00a4 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1832 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x187f ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1883 ), k_eControllerType_XBox360Controller },	// Unknown Controller	
	{ MAKE_CONTROLLER_ID( 0x03eb, 0xff01 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2303 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0ef8 ), k_eControllerType_XBox360Controller },	// Homemade fightstick based on brook pcb (with XInput driver??)
	{ MAKE_CONTROLLER_ID( 0x046d, 0x1000 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1345, 0x6006 ), k_eControllerType_XBox360Controller },	// Unknown Controller

	{ MAKE_CONTROLLER_ID( 0x056e, 0x2012 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0602 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00ae ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0603 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x056e, 0x2013 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x046d, 0x0401 ), k_eControllerType_XBox360Controller },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0x0301 ), k_eControllerType_XBox360Controller },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0xcaa3 ), k_eControllerType_XBox360Controller },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc261 ), k_eControllerType_XBox360Controller },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0x0291 ), k_eControllerType_XBox360Controller },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x0079, 0x18d3 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00b1 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0001, 0x0001 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1345, 0x6005 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x188e ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x18d4 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2003 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00b1 ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x187c ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x189c ), k_eControllerType_XBox360Controller },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1874 ), k_eControllerType_XBox360Controller },	// Unknown Controller

	{ MAKE_CONTROLLER_ID( 0x1038, 0xb360 ), k_eControllerType_XBox360Controller },	// SteelSeries Nimbus/Stratus XL

																					
	//{ MAKE_CONTROLLER_ID( 0x1949, 0x0402 ), /*android*/ },	// Unknown Controller

	{ MAKE_CONTROLLER_ID( 0x05ac, 0x0001 ), k_eControllerType_AppleController },	// MFI Extended Gamepad (generic entry for iOS/tvOS)
	{ MAKE_CONTROLLER_ID( 0x05ac, 0x0002 ), k_eControllerType_AppleController },	// MFI Standard Gamepad (generic entry for iOS/tvOS)

    // We currently don't support using a pair of Switch Joy-Con's as a single
    // controller and we don't want to support using them individually for the
    // time being, so these should be disabled until one of the above is true
    // { MAKE_CONTROLLER_ID( 0x057e, 0x2006 ), k_eControllerType_SwitchJoyConLeft },    // Nintendo Switch Joy-Con (Left)
    // { MAKE_CONTROLLER_ID( 0x057e, 0x2007 ), k_eControllerType_SwitchJoyConRight },   // Nintendo Switch Joy-Con (Right)

    // This same controller ID is spoofed by many 3rd-party Switch controllers.
    // The ones we currently know of are:
    // * Any 8bitdo controller with Switch support
    // * ORTZ Gaming Wireless Pro Controller
    // * ZhiXu Gamepad Wireless
    // * Sunwaytek Wireless Motion Controller for Nintendo Switch
	{ MAKE_CONTROLLER_ID( 0x057e, 0x2009 ), k_eControllerType_SwitchProController },        // Nintendo Switch Pro Controller
    
    { MAKE_CONTROLLER_ID( 0x0f0d, 0x00c1 ), k_eControllerType_SwitchInputOnlyController },  // HORIPAD for Nintendo Switch
    { MAKE_CONTROLLER_ID( 0x20d6, 0xa711 ), k_eControllerType_SwitchInputOnlyController },  // PowerA Wired Controller Plus
    { MAKE_CONTROLLER_ID( 0x0f0d, 0x0092 ), k_eControllerType_SwitchInputOnlyController },  // HORI Pokken Tournament DX Pro Pad


	// Valve products - don't add to public list
    { MAKE_CONTROLLER_ID( 0x0000, 0x11fb ), k_eControllerType_MobileTouch },		// Streaming mobile touch virtual controls
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1101 ), k_eControllerType_SteamController },	// Valve Legacy Steam Controller (CHELL)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1102 ), k_eControllerType_SteamController },	// Valve wired Steam Controller (D0G)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1105 ), k_eControllerType_SteamControllerV2 },	// Valve Bluetooth Steam Controller (D0G)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1106 ), k_eControllerType_SteamControllerV2 },	// Valve Bluetooth Steam Controller (D0G)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1142 ), k_eControllerType_SteamController },	// Valve wireless Steam Controller
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1201 ), k_eControllerType_SteamController },	// Valve wired Steam Controller (HEADCRAB)
};


#if 0  /* these are currently unused, so #if 0'd out to prevent compiler warnings for now */
static inline const ControllerDescription_t * GetControllerArray( int* nLength /* Out */)
{
	*nLength = sizeof( arrControllers ) / sizeof( arrControllers[0] );
	return arrControllers;
}
#endif

static inline EControllerType GuessControllerType( int nVID, int nPID )
{
	unsigned int unDeviceID = MAKE_CONTROLLER_ID( nVID, nPID );
	int iIndex;
	for ( iIndex = 0; iIndex < sizeof( arrControllers ) / sizeof( arrControllers[0] ); ++iIndex )
	{
		if ( unDeviceID == arrControllers[ iIndex ].m_unDeviceID )
		{
			return arrControllers[ iIndex ].m_eControllerType;
		}
	}
#undef MAKE_CONTROLLER_ID

	return k_eControllerType_UnknownNonSteamController;
}

#endif // CONSTANTS_H

