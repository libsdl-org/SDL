// Copyright (c) Microsoft Corporation.
//
// This file is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#ifndef __cplusplus
#error GameInput requires C++
#endif

#include <stdint.h>
#include <unknwn.h>

#pragma region Application Family or OneCore Family or Games Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)

#define GAMEINPUT_API_VERSION 3

namespace GameInput { namespace v3 {

enum GameInputKind
{
    GameInputKindUnknown          = 0x00000000,
    GameInputKindRawDeviceReport  = 0x00000001,
    GameInputKindControllerAxis   = 0x00000002,
    GameInputKindControllerButton = 0x00000004,
    GameInputKindControllerSwitch = 0x00000008,
    GameInputKindController       = 0x0000000E,
    GameInputKindKeyboard         = 0x00000010,
    GameInputKindMouse            = 0x00000020,
    GameInputKindSensors          = 0x00000040,
    GameInputKindArcadeStick      = 0x00010000,
    GameInputKindFlightStick      = 0x00020000,
    GameInputKindGamepad          = 0x00040000,
    GameInputKindRacingWheel      = 0x00080000,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputKind)

enum GameInputEnumerationKind
{
    GameInputNoEnumeration       = 0,
    GameInputAsyncEnumeration    = 1,
    GameInputBlockingEnumeration = 2
};

enum GameInputFocusPolicy
{
    GameInputDefaultFocusPolicy             = 0x00000000,
    GameInputExclusiveForegroundInput       = 0x00000002,
    GameInputExclusiveForegroundGuideButton = 0x00000008,
    GameInputExclusiveForegroundShareButton = 0x00000020,
    GameInputEnableBackgroundInput          = 0x00000040,
    GameInputEnableBackgroundGuideButton    = 0x00000080,
    GameInputEnableBackgroundShareButton    = 0x00000100
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputFocusPolicy)

enum GameInputSwitchKind
{
    GameInputUnknownSwitchKind = -1,
    GameInput2WaySwitch        =  0,
    GameInput4WaySwitch        =  1,
    GameInput8WaySwitch        =  2
};

enum GameInputSwitchPosition
{
    GameInputSwitchCenter    = 0,
    GameInputSwitchUp        = 1,
    GameInputSwitchUpRight   = 2,
    GameInputSwitchRight     = 3,
    GameInputSwitchDownRight = 4,
    GameInputSwitchDown      = 5,
    GameInputSwitchDownLeft  = 6,
    GameInputSwitchLeft      = 7,
    GameInputSwitchUpLeft    = 8
};

enum GameInputKeyboardKind
{
    GameInputUnknownKeyboard = -1,
    GameInputAnsiKeyboard    =  0,
    GameInputIsoKeyboard     =  1,
    GameInputKsKeyboard      =  2,
    GameInputAbntKeyboard    =  3,
    GameInputJisKeyboard     =  4
};

enum GameInputMouseButtons
{
    GameInputMouseNone           = 0x00000000,
    GameInputMouseLeftButton     = 0x00000001,
    GameInputMouseRightButton    = 0x00000002,
    GameInputMouseMiddleButton   = 0x00000004,
    GameInputMouseButton4        = 0x00000008,
    GameInputMouseButton5        = 0x00000010,
    GameInputMouseWheelTiltLeft  = 0x00000020,
    GameInputMouseWheelTiltRight = 0x00000040
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputMouseButtons)

enum GameInputMousePositions
{
    GameInputMouseNoPosition       = 0x00000000,
    GameInputMouseAbsolutePosition = 0x00000001,
    GameInputMouseRelativePosition = 0x00000002
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputMousePositions)

enum GameInputSensorsKind
{
    GameInputSensorsNone            = 0x00000000,
    GameInputSensorsAccelerometer   = 0x00000001,
    GameInputSensorsGyrometer       = 0x00000002,
    GameInputSensorsCompass         = 0x00000004,
    GameInputSensorsOrientation     = 0x00000008
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputSensorsKind)

enum GameInputSensorAccuracy
{
    GameInputSensorAccuracyUnknown      = 0x00000000,
    GameInputSensorAccuracyUnreliable   = 0x00000001,
    GameInputSensorAccuracyApproximate  = 0x00000002,
    GameInputSensorAccuracyHigh         = 0x00000003
};

enum GameInputArcadeStickButtons
{
    GameInputArcadeStickNone     = 0x00000000,
    GameInputArcadeStickMenu     = 0x00000001,
    GameInputArcadeStickView     = 0x00000002,
    GameInputArcadeStickUp       = 0x00000004,
    GameInputArcadeStickDown     = 0x00000008,
    GameInputArcadeStickLeft     = 0x00000010,
    GameInputArcadeStickRight    = 0x00000020,
    GameInputArcadeStickAction1  = 0x00000040,
    GameInputArcadeStickAction2  = 0x00000080,
    GameInputArcadeStickAction3  = 0x00000100,
    GameInputArcadeStickAction4  = 0x00000200,
    GameInputArcadeStickAction5  = 0x00000400,
    GameInputArcadeStickAction6  = 0x00000800,
    GameInputArcadeStickSpecial1 = 0x00001000,
    GameInputArcadeStickSpecial2 = 0x00002000
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputArcadeStickButtons)

enum GameInputFlightStickButtons
{
    GameInputFlightStickNone           = 0x00000000,
    GameInputFlightStickMenu           = 0x00000001,
    GameInputFlightStickView           = 0x00000002,
    GameInputFlightStickFirePrimary    = 0x00000004,
    GameInputFlightStickFireSecondary  = 0x00000008,
    GameInputFlightStickHatSwitchUp    = 0x00000010,
    GameInputFlightStickHatSwitchDown  = 0x00000020,
    GameInputFlightStickHatSwitchLeft  = 0x00000040,
    GameInputFlightStickHatSwitchRight = 0x00000080,
    GameInputFlightStickA              = 0x00000100,
    GameInputFlightStickB              = 0x00000200,
    GameInputFlightStickX              = 0x00000400,
    GameInputFlightStickY              = 0x00000800,
    GameInputFlightStickLeftShoulder   = 0x00001000,
    GameInputFlightStickRightShoulder  = 0x00002000,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputFlightStickButtons)

enum GameInputGamepadButtons
{
    GameInputGamepadNone                 = 0x00000000,
    GameInputGamepadMenu                 = 0x00000001,
    GameInputGamepadView                 = 0x00000002,
    GameInputGamepadA                    = 0x00000004,
    GameInputGamepadB                    = 0x00000008,
    GameInputGamepadC                    = 0x00004000,
    GameInputGamepadX                    = 0x00000010,
    GameInputGamepadY                    = 0x00000020,
    GameInputGamepadZ                    = 0x00008000,
    GameInputGamepadDPadUp               = 0x00000040,
    GameInputGamepadDPadDown             = 0x00000080,
    GameInputGamepadDPadLeft             = 0x00000100,
    GameInputGamepadDPadRight            = 0x00000200,
    GameInputGamepadLeftShoulder         = 0x00000400,
    GameInputGamepadRightShoulder        = 0x00000800,
    GameInputGamepadLeftTriggerButton    = 0x00010000,
    GameInputGamepadRightTriggerButton   = 0x00020000,
    GameInputGamepadLeftThumbstick       = 0x00001000,
    GameInputGamepadLeftThumbstickUp     = 0x00040000,
    GameInputGamepadLeftThumbstickDown   = 0x00080000,
    GameInputGamepadLeftThumbstickLeft   = 0x00100000,
    GameInputGamepadLeftThumbstickRight  = 0x00200000,
    GameInputGamepadRightThumbstick      = 0x00002000,
    GameInputGamepadRightThumbstickUp    = 0x00400000,
    GameInputGamepadRightThumbstickDown  = 0x00800000,
    GameInputGamepadRightThumbstickLeft  = 0x01000000,
    GameInputGamepadRightThumbstickRight = 0x02000000,
    GameInputGamepadPaddleLeft1          = 0x04000000,
    GameInputGamepadPaddleLeft2          = 0x08000000,
    GameInputGamepadPaddleRight1         = 0x10000000,
    GameInputGamepadPaddleRight2         = 0x20000000,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputGamepadButtons)

enum GameInputRawDeviceReportKind
{
    GameInputRawInputReport   = 0,
    GameInputRawOutputReport  = 1,
};

// Gamepad modules (Groupings of gamepad elements commonly found together)
const GameInputGamepadButtons GameInputGamepadModuleSystemDuo =
    GameInputGamepadMenu |
    GameInputGamepadView;

const GameInputGamepadButtons GameInputGamepadModuleDpad =
    GameInputGamepadDPadUp |
    GameInputGamepadDPadDown |
    GameInputGamepadDPadLeft |
    GameInputGamepadDPadRight;

const GameInputGamepadButtons GameInputGamepadModuleShoulders =
    GameInputGamepadLeftShoulder |
    GameInputGamepadRightShoulder;

const GameInputGamepadButtons GameInputGamepadModuleTriggers =
    GameInputGamepadLeftTriggerButton |
    GameInputGamepadRightTriggerButton;

const GameInputGamepadButtons GameInputGamepadModuleThumbsticks =
    GameInputGamepadLeftThumbstickUp |
    GameInputGamepadLeftThumbstickDown |
    GameInputGamepadLeftThumbstickLeft |
    GameInputGamepadLeftThumbstickRight |
    GameInputGamepadRightThumbstickUp |
    GameInputGamepadRightThumbstickDown |
    GameInputGamepadRightThumbstickLeft |
    GameInputGamepadRightThumbstickRight;

const GameInputGamepadButtons GameInputGamepadModulePaddles2 =
    GameInputGamepadPaddleLeft1 |
    GameInputGamepadPaddleRight1;

const GameInputGamepadButtons GameInputGamepadModulePaddles4 =
    GameInputGamepadPaddleLeft1 |
    GameInputGamepadPaddleLeft2 |
    GameInputGamepadPaddleRight1 |
    GameInputGamepadPaddleRight2;

// Commonly found gamepad layouts. Custom layouts are possible and encouraged.
const GameInputGamepadButtons GameInputGamepadLayoutBasic =
    GameInputGamepadModuleSystemDuo |
    GameInputGamepadModuleDpad |
    GameInputGamepadA |
    GameInputGamepadB;

const GameInputGamepadButtons GameInputGamepadLayoutButtons =
    GameInputGamepadLayoutBasic |
    GameInputGamepadX |
    GameInputGamepadY |
    GameInputGamepadModuleShoulders;

const GameInputGamepadButtons GameInputGamepadLayoutStandard =
    GameInputGamepadLayoutButtons |
    GameInputGamepadModuleTriggers |
    GameInputGamepadModuleThumbsticks |
    GameInputGamepadLeftThumbstick |
    GameInputGamepadRightThumbstick;

const GameInputGamepadButtons GameInputGamepadLayoutElite =
    GameInputGamepadLayoutStandard |
    GameInputGamepadModulePaddles4;

enum GameInputRacingWheelButtons
{
    GameInputRacingWheelNone            = 0x00000000,
    GameInputRacingWheelMenu            = 0x00000001,
    GameInputRacingWheelView            = 0x00000002,
    GameInputRacingWheelPreviousGear    = 0x00000004,
    GameInputRacingWheelNextGear        = 0x00000008,
    GameInputRacingWheelA               = 0x00000100,
    GameInputRacingWheelB               = 0x00000200,
    GameInputRacingWheelX               = 0x00000400,
    GameInputRacingWheelY               = 0x00000800,
    GameInputRacingWheelDpadUp          = 0x00000010,
    GameInputRacingWheelDpadDown        = 0x00000020,
    GameInputRacingWheelDpadLeft        = 0x00000040,
    GameInputRacingWheelDpadRight       = 0x00000080,
    GameInputRacingWheelLeftThumbstick  = 0x00001000,
    GameInputRacingWheelRightThumbstick = 0x00002000,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputRacingWheelButtons)

enum GameInputSystemButtons
{
    GameInputSystemButtonNone  = 0x00000000,
    GameInputSystemButtonGuide = 0x00000001,
    GameInputSystemButtonShare = 0x00000002
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputSystemButtons)

enum GameInputFlightStickAxes
{
    GameInputFlightStickAxesNone = 0x00000000,
    GameInputFlightStickRoll     = 0x00000010,
    GameInputFlightStickPitch    = 0x00000020,
    GameInputFlightStickYaw      = 0x00000040,
    GameInputFlightStickThrottle = 0x00000080,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputFlightStickAxes)

enum GameInputGamepadAxes
{
    GameInputGamepadAxesNone         = 0x00000000,
    GameInputGamepadLeftTrigger      = 0x00000001,
    GameInputGamepadRightTrigger     = 0x00000002,
    GameInputGamepadLeftThumbstickX  = 0x00000004,
    GameInputGamepadLeftThumbstickY  = 0x00000008,
    GameInputGamepadRightThumbstickX = 0x00000010,
    GameInputGamepadRightThumbstickY = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputGamepadAxes)

enum GameInputRacingWheelAxes
{
    GameInputRacingWheelAxesNone       = 0x00000000,
    GameInputRacingWheelSteering       = 0x00000100,
    GameInputRacingWheelThrottle       = 0x00000200,
    GameInputRacingWheelBrake          = 0x00000400,
    GameInputRacingWheelClutch         = 0x00000800,
    GameInputRacingWheelHandbrake      = 0x00001000,
    GameInputRacingWheelPatternShifter = 0x00002000,
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputRacingWheelAxes)

enum GameInputDeviceStatus
{
    GameInputDeviceNoStatus        = 0x00000000,
    GameInputDeviceConnected       = 0x00000001,
    GameInputDeviceHapticInfoReady = 0x00200000,
    GameInputDeviceAnyStatus       = 0xFFFFFFFF
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputDeviceStatus)

enum GameInputDeviceFamily
{
    GameInputFamilyVirtual   = -1,
    GameInputFamilyUnknown   =  0,
    GameInputFamilyXboxOne   =  1,
    GameInputFamilyXbox360   =  2,
    GameInputFamilyHid       =  3,
    GameInputFamilyI8042     =  4,
    GameInputFamilyAggregate =  5,
};

enum GameInputLabel
{
    GameInputLabelUnknown                  =  -1,
    GameInputLabelNone                     =   0,
    GameInputLabelXboxGuide                =   1,
    GameInputLabelXboxBack                 =   2,
    GameInputLabelXboxStart                =   3,
    GameInputLabelXboxMenu                 =   4,
    GameInputLabelXboxView                 =   5,
    GameInputLabelXboxA                    =   7,
    GameInputLabelXboxB                    =   8,
    GameInputLabelXboxX                    =   9,
    GameInputLabelXboxY                    =  10,
    GameInputLabelXboxDPadUp               =  11,
    GameInputLabelXboxDPadDown             =  12,
    GameInputLabelXboxDPadLeft             =  13,
    GameInputLabelXboxDPadRight            =  14,
    GameInputLabelXboxLeftShoulder         =  15,
    GameInputLabelXboxLeftTrigger          =  16,
    GameInputLabelXboxLeftStickButton      =  17,
    GameInputLabelXboxRightShoulder        =  18,
    GameInputLabelXboxRightTrigger         =  19,
    GameInputLabelXboxRightStickButton     =  20,
    GameInputLabelXboxPaddle1              =  21,
    GameInputLabelXboxPaddle2              =  22,
    GameInputLabelXboxPaddle3              =  23,
    GameInputLabelXboxPaddle4              =  24,
    GameInputLabelLetterA                  =  25,
    GameInputLabelLetterB                  =  26,
    GameInputLabelLetterC                  =  27,
    GameInputLabelLetterD                  =  28,
    GameInputLabelLetterE                  =  29,
    GameInputLabelLetterF                  =  30,
    GameInputLabelLetterG                  =  31,
    GameInputLabelLetterH                  =  32,
    GameInputLabelLetterI                  =  33,
    GameInputLabelLetterJ                  =  34,
    GameInputLabelLetterK                  =  35,
    GameInputLabelLetterL                  =  36,
    GameInputLabelLetterM                  =  37,
    GameInputLabelLetterN                  =  38,
    GameInputLabelLetterO                  =  39,
    GameInputLabelLetterP                  =  40,
    GameInputLabelLetterQ                  =  41,
    GameInputLabelLetterR                  =  42,
    GameInputLabelLetterS                  =  43,
    GameInputLabelLetterT                  =  44,
    GameInputLabelLetterU                  =  45,
    GameInputLabelLetterV                  =  46,
    GameInputLabelLetterW                  =  47,
    GameInputLabelLetterX                  =  48,
    GameInputLabelLetterY                  =  49,
    GameInputLabelLetterZ                  =  50,
    GameInputLabelNumber0                  =  51,
    GameInputLabelNumber1                  =  52,
    GameInputLabelNumber2                  =  53,
    GameInputLabelNumber3                  =  54,
    GameInputLabelNumber4                  =  55,
    GameInputLabelNumber5                  =  56,
    GameInputLabelNumber6                  =  57,
    GameInputLabelNumber7                  =  58,
    GameInputLabelNumber8                  =  59,
    GameInputLabelNumber9                  =  60,
    GameInputLabelArrowUp                  =  61,
    GameInputLabelArrowUpRight             =  62,
    GameInputLabelArrowRight               =  63,
    GameInputLabelArrowDownRight           =  64,
    GameInputLabelArrowDown                =  65,
    GameInputLabelArrowDownLLeft           =  66,
    GameInputLabelArrowLeft                =  67,
    GameInputLabelArrowUpLeft              =  68,
    GameInputLabelArrowUpDown              =  69,
    GameInputLabelArrowLeftRight           =  70,
    GameInputLabelArrowUpDownLeftRight     =  71,
    GameInputLabelArrowClockwise           =  72,
    GameInputLabelArrowCounterClockwise    =  73,
    GameInputLabelArrowReturn              =  74,
    GameInputLabelIconBranding             =  75,
    GameInputLabelIconHome                 =  76,
    GameInputLabelIconMenu                 =  77,
    GameInputLabelIconCross                =  78,
    GameInputLabelIconCircle               =  79,
    GameInputLabelIconSquare               =  80,
    GameInputLabelIconTriangle             =  81,
    GameInputLabelIconStar                 =  82,
    GameInputLabelIconDPadUp               =  83,
    GameInputLabelIconDPadDown             =  84,
    GameInputLabelIconDPadLeft             =  85,
    GameInputLabelIconDPadRight            =  86,
    GameInputLabelIconDialClockwise        =  87,
    GameInputLabelIconDialCounterClockwise =  88,
    GameInputLabelIconSliderLeftRight      =  89,
    GameInputLabelIconSliderUpDown         =  90,
    GameInputLabelIconWheelUpDown          =  91,
    GameInputLabelIconPlus                 =  92,
    GameInputLabelIconMinus                =  93,
    GameInputLabelIconSuspension           =  94,
    GameInputLabelHome                     =  95,
    GameInputLabelGuide                    =  96,
    GameInputLabelMode                     =  97,
    GameInputLabelSelect                   =  98,
    GameInputLabelMenu                     =  99,
    GameInputLabelView                     = 100,
    GameInputLabelBack                     = 101,
    GameInputLabelStart                    = 102,
    GameInputLabelOptions                  = 103,
    GameInputLabelShare                    = 104,
    GameInputLabelUp                       = 105,
    GameInputLabelDown                     = 106,
    GameInputLabelLeft                     = 107,
    GameInputLabelRight                    = 108,
    GameInputLabelLB                       = 109,
    GameInputLabelLT                       = 110,
    GameInputLabelLSB                      = 111,
    GameInputLabelL1                       = 112,
    GameInputLabelL2                       = 113,
    GameInputLabelL3                       = 114,
    GameInputLabelRB                       = 115,
    GameInputLabelRT                       = 116,
    GameInputLabelRSB                      = 117,
    GameInputLabelR1                       = 118,
    GameInputLabelR2                       = 119,
    GameInputLabelR3                       = 120,
    GameInputLabelPaddleLeft1              = 121,
    GameInputLabelPaddleLeft2              = 122,
    GameInputLabelPaddleRight1             = 123,
    GameInputLabelPaddleRight2             = 124,
};

enum GameInputFeedbackAxes
{
    GameInputFeedbackAxisNone     = 0x00000000,
    GameInputFeedbackAxisLinearX  = 0x00000001,
    GameInputFeedbackAxisLinearY  = 0x00000002,
    GameInputFeedbackAxisLinearZ  = 0x00000004,
    GameInputFeedbackAxisAngularX = 0x00000008,
    GameInputFeedbackAxisAngularY = 0x00000010,
    GameInputFeedbackAxisAngularZ = 0x00000020,
    GameInputFeedbackAxisNormal   = 0x00000040
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputFeedbackAxes)

enum GameInputFeedbackEffectState
{
    GameInputFeedbackStopped = 0,
    GameInputFeedbackRunning = 1,
    GameInputFeedbackPaused  = 2
};

enum GameInputForceFeedbackEffectKind
{
    GameInputForceFeedbackConstant         = 0,
    GameInputForceFeedbackRamp             = 1,
    GameInputForceFeedbackSineWave         = 2,
    GameInputForceFeedbackSquareWave       = 3,
    GameInputForceFeedbackTriangleWave     = 4,
    GameInputForceFeedbackSawtoothUpWave   = 5,
    GameInputForceFeedbackSawtoothDownWave = 6,
    GameInputForceFeedbackSpring           = 7,
    GameInputForceFeedbackFriction         = 8,
    GameInputForceFeedbackDamper           = 9,
    GameInputForceFeedbackInertia          = 10
};

enum GameInputRumbleMotors
{
    GameInputRumbleNone          = 0x00000000,
    GameInputRumbleLowFrequency  = 0x00000001,
    GameInputRumbleHighFrequency = 0x00000002,
    GameInputRumbleLeftTrigger   = 0x00000004,
    GameInputRumbleRightTrigger  = 0x00000008
};

DEFINE_ENUM_FLAG_OPERATORS(GameInputRumbleMotors)

interface IGameInput;
interface IGameInputReading;
interface IGameInputDevice;
interface IGameInputDispatcher;
interface IGameInputForceFeedbackEffect;
interface IGameInputMapper;

typedef uint64_t GameInputCallbackToken;
// start SDL modifications
#if 0
// end SDL modifications
constexpr GUID GAMEINPUT_HAPTIC_LOCATION_NONE          = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
constexpr GUID GAMEINPUT_HAPTIC_LOCATION_GRIP_LEFT     = { 0x08c707c2, 0x66bb, 0x406c, { 0xa8, 0x4a, 0xdf, 0xe0, 0x85, 0x12, 0x0a, 0x92 } };
constexpr GUID GAMEINPUT_HAPTIC_LOCATION_GRIP_RIGHT    = { 0x155a0b77, 0x8bb2, 0x40db, { 0x86, 0x90, 0xb6, 0xd4, 0x11, 0x26, 0xdf, 0xc1 } };
constexpr GUID GAMEINPUT_HAPTIC_LOCATION_TRIGGER_LEFT  = { 0x8de4d896, 0x5559, 0x4081, { 0x86, 0xe5, 0x17, 0x24, 0xcc, 0x07, 0xc6, 0xbc } };
constexpr GUID GAMEINPUT_HAPTIC_LOCATION_TRIGGER_RIGHT = { 0xff0cb557, 0x3af5, 0x406b, { 0x8b, 0x0f, 0x55, 0x5a, 0x2d, 0x92, 0xa2, 0x20 } };
// start SDL modifications
#endif
// end SDL modifications

const uint32_t GAMEINPUT_HAPTIC_MAX_LOCATIONS              = 8;
const uint32_t GAMEINPUT_HAPTIC_MAX_AUDIO_ENDPOINT_ID_SIZE = 256;

typedef void (CALLBACK* GameInputReadingCallback)(
    _In_ GameInputCallbackToken callbackToken,
    _In_ void* context,
    _In_ IGameInputReading* reading);

typedef void (CALLBACK* GameInputDeviceCallback)(
    _In_ GameInputCallbackToken callbackToken,
    _In_ void* context,
    _In_ IGameInputDevice* device,
    _In_ uint64_t timestamp,
    _In_ GameInputDeviceStatus currentStatus,
    _In_ GameInputDeviceStatus previousStatus);

typedef void (CALLBACK* GameInputSystemButtonCallback)(
    _In_ GameInputCallbackToken callbackToken,
    _In_ void* context,
    _In_ IGameInputDevice* device,
    _In_ uint64_t timestamp,
    _In_ GameInputSystemButtons currentButtons,
    _In_ GameInputSystemButtons previousButtons);

typedef void (CALLBACK* GameInputKeyboardLayoutCallback)(
    _In_ GameInputCallbackToken callbackToken,
    _In_ void* context,
    _In_ IGameInputDevice* device,
    _In_ uint64_t timestamp,
    _In_ uint32_t currentLayout,
    _In_ uint32_t previousLayout);

struct GameInputKeyState
{
    uint32_t scanCode;
    uint32_t codePoint;
    uint8_t  virtualKey;
    bool     isDeadKey;
};

struct GameInputMouseState
{
    GameInputMouseButtons   buttons;
    GameInputMousePositions positions;
    int64_t                 positionX;
    int64_t                 positionY;
    int64_t                 absolutePositionX;
    int64_t                 absolutePositionY;
    int64_t                 wheelX;
    int64_t                 wheelY;
};

struct GameInputVersion
{
    uint16_t major;
    uint16_t minor;
    uint16_t build;
    uint16_t revision;
};

struct GameInputSensorsState
{
    // GameInputSensorsAccelerometer
    float accelerationInGX;
    float accelerationInGY;
    float accelerationInGZ;

    // GameInputSensorsGyrometer
    float angularVelocityInRadPerSecX;
    float angularVelocityInRadPerSecY;
    float angularVelocityInRadPerSecZ;

    // GameInputSensorsCompass
    float headingInDegreesFromMagneticNorth;
    GameInputSensorAccuracy headingAccuracy;

    // GameInputSensorsOrientation
    float orientationW;
    float orientationX;
    float orientationY;
    float orientationZ;
};

struct GameInputArcadeStickState
{
    GameInputArcadeStickButtons buttons;
};

struct GameInputFlightStickState
{
    GameInputFlightStickButtons buttons;
    GameInputSwitchPosition     hatSwitch;
    float                       roll;
    float                       pitch;
    float                       yaw;
    float                       throttle;
};

struct GameInputGamepadState
{
    GameInputGamepadButtons buttons;
    float                   leftTrigger;
    float                   rightTrigger;
    float                   leftThumbstickX;
    float                   leftThumbstickY;
    float                   rightThumbstickX;
    float                   rightThumbstickY;
};

struct GameInputRacingWheelState
{
    GameInputRacingWheelButtons buttons;
    int32_t                     patternShifterGear;
    float                       wheel;
    float                       throttle;
    float                       brake;
    float                       clutch;
    float                       handbrake;
};

struct GameInputUsage
{
    uint16_t page;
    uint16_t id;
};

const uint32_t GAMEINPUT_MAX_SWITCH_STATES = 8;

struct GameInputControllerSwitchInfo
{
    GameInputLabel      labels[GAMEINPUT_MAX_SWITCH_STATES];
    GameInputSwitchKind kind;
};

struct GameInputControllerInfo
{
    uint32_t                                                                      controllerAxisCount;
    _Field_size_full_(controllerAxisCount) const GameInputLabel*                  controllerAxisLabels;
    uint32_t                                                                      controllerButtonCount;
    _Field_size_full_(controllerButtonCount) const GameInputLabel*                controllerButtonLabels;
    uint32_t                                                                      controllerSwitchCount;
    _Field_size_full_(controllerSwitchCount) const GameInputControllerSwitchInfo* controllerSwitchInfo;
};

struct GameInputKeyboardInfo
{
    GameInputKeyboardKind kind;
    uint32_t              layout;
    uint32_t              keyCount;
    uint32_t              functionKeyCount;
    uint32_t              maxSimultaneousKeys;
    uint32_t              platformType;
    uint32_t              platformSubtype;
};

struct GameInputMouseInfo
{
    GameInputMouseButtons supportedButtons;
    uint32_t              sampleRate;
    bool                  hasWheelX;
    bool                  hasWheelY;
};

struct GameInputSensorsInfo
{
    GameInputSensorsKind supportedSensors;
};

struct GameInputArcadeStickInfo
{
    GameInputLabel menuButtonLabel;
    GameInputLabel viewButtonLabel;
    GameInputLabel stickUpLabel;
    GameInputLabel stickDownLabel;
    GameInputLabel stickLeftLabel;
    GameInputLabel stickRightLabel;
    GameInputLabel actionButton1Label;
    GameInputLabel actionButton2Label;
    GameInputLabel actionButton3Label;
    GameInputLabel actionButton4Label;
    GameInputLabel actionButton5Label;
    GameInputLabel actionButton6Label;
    GameInputLabel specialButton1Label;
    GameInputLabel specialButton2Label;
    uint32_t       extraButtonCount;
    uint32_t       extraAxisCount;
};

struct GameInputFlightStickInfo
{
    GameInputLabel menuButtonLabel;
    GameInputLabel viewButtonLabel;
    GameInputLabel firePrimaryButtonLabel;
    GameInputLabel fireSecondaryButtonLabel;
    GameInputLabel hatSwitchUpLabel;
    GameInputLabel hatSwitchDownLabel;
    GameInputLabel hatSwitchLeftLabel;
    GameInputLabel hatSwitchRightLabel;
    GameInputLabel aButtonLabel;
    GameInputLabel bButtonLabel;
    GameInputLabel xButtonLabel;
    GameInputLabel yButtonLabel;
    GameInputLabel leftShoulderButtonLabel;
    GameInputLabel rightShoulderButtonLabel;
    uint32_t       extraButtonCount;
    uint32_t       extraAxisCount;
};

struct GameInputGamepadInfo
{
    GameInputGamepadButtons supportedLayout;
    GameInputLabel          menuButtonLabel;
    GameInputLabel          viewButtonLabel;
    GameInputLabel          aButtonLabel;
    GameInputLabel          bButtonLabel;
    GameInputLabel          cButtonLabel;
    GameInputLabel          xButtonLabel;
    GameInputLabel          yButtonLabel;
    GameInputLabel          zButtonLabel;
    GameInputLabel          dpadUpLabel;
    GameInputLabel          dpadDownLabel;
    GameInputLabel          dpadLeftLabel;
    GameInputLabel          dpadRightLabel;
    GameInputLabel          leftShoulderButtonLabel;
    GameInputLabel          rightShoulderButtonLabel;
    GameInputLabel          leftThumbstickButtonLabel;
    GameInputLabel          rightThumbstickButtonLabel;
    uint32_t                extraButtonCount;
    uint32_t                extraAxisCount;
};

struct GameInputRacingWheelInfo
{
    GameInputLabel menuButtonLabel;
    GameInputLabel viewButtonLabel;
    GameInputLabel previousGearButtonLabel;
    GameInputLabel nextGearButtonLabel;
    GameInputLabel dpadUpLabel;
    GameInputLabel dpadDownLabel;
    GameInputLabel dpadLeftLabel;
    GameInputLabel dpadRightLabel;
    GameInputLabel aButtonLabel;
    GameInputLabel bButtonLabel;
    GameInputLabel xButtonLabel;
    GameInputLabel yButtonLabel;
    GameInputLabel leftThumbstickButtonLabel;
    GameInputLabel rightThumbstickButtonLabel;
    bool           hasClutch;
    bool           hasHandbrake;
    bool           hasPatternShifter;
    int32_t        minPatternShifterGear;
    int32_t        maxPatternShifterGear;
    float          maxWheelAngle;
    uint32_t       extraButtonCount;
    uint32_t       extraAxisCount;
};

struct GameInputForceFeedbackMotorInfo
{
    GameInputFeedbackAxes supportedAxes;
    bool                  isConstantEffectSupported;
    bool                  isRampEffectSupported;
    bool                  isSineWaveEffectSupported;
    bool                  isSquareWaveEffectSupported;
    bool                  isTriangleWaveEffectSupported;
    bool                  isSawtoothUpWaveEffectSupported;
    bool                  isSawtoothDownWaveEffectSupported;
    bool                  isSpringEffectSupported;
    bool                  isFrictionEffectSupported;
    bool                  isDamperEffectSupported;
    bool                  isInertiaEffectSupported;
};

struct GameInputRawDeviceReportInfo
{
    GameInputRawDeviceReportKind kind;
    uint32_t                     id;
    uint32_t                     size;
};

struct GameInputDeviceInfo
{
    uint16_t               vendorId;
    uint16_t               productId;
    uint16_t               revisionNumber;
    GameInputUsage         usage;
    GameInputVersion       hardwareVersion;
    GameInputVersion       firmwareVersion;
    APP_LOCAL_DEVICE_ID    deviceId;
    APP_LOCAL_DEVICE_ID    deviceRootId;
    GameInputDeviceFamily  deviceFamily;
    GameInputKind          supportedInput;
    GameInputRumbleMotors  supportedRumbleMotors;
    GameInputSystemButtons supportedSystemButtons;
    GUID                   containerId;
    const char*            displayName;
    const char*            pnpPath;

    _Field_size_full_opt_(1) const GameInputKeyboardInfo*    keyboardInfo;
    _Field_size_full_opt_(1) const GameInputMouseInfo*       mouseInfo;
    _Field_size_full_opt_(1) const GameInputSensorsInfo*     sensorsInfo;
    _Field_size_full_opt_(1) const GameInputControllerInfo*  controllerInfo;
    _Field_size_full_opt_(1) const GameInputArcadeStickInfo* arcadeStickInfo;
    _Field_size_full_opt_(1) const GameInputFlightStickInfo* flightStickInfo;
    _Field_size_full_opt_(1) const GameInputGamepadInfo*     gamepadInfo;
    _Field_size_full_opt_(1) const GameInputRacingWheelInfo* racingWheelInfo;

    uint32_t forceFeedbackMotorCount;
    _Field_size_full_(forceFeedbackMotorCount) const GameInputForceFeedbackMotorInfo* forceFeedbackMotorInfo;

    uint32_t inputReportCount;
    _Field_size_full_opt_(inputReportCount) const GameInputRawDeviceReportInfo* inputReportInfo;

    uint32_t outputReportCount;
    _Field_size_full_opt_(outputReportCount) const GameInputRawDeviceReportInfo* outputReportInfo;
};

struct GameInputHapticInfo
{
    _Field_z_ wchar_t                                         audioEndpointId[GAMEINPUT_HAPTIC_MAX_AUDIO_ENDPOINT_ID_SIZE];
    _Field_range_(1, GAMEINPUT_HAPTIC_MAX_LOCATIONS) uint32_t locationCount;
    _Field_size_full_(locationCount) GUID                     locations[GAMEINPUT_HAPTIC_MAX_LOCATIONS];
};

struct GameInputForceFeedbackEnvelope
{
    uint64_t attackDuration;
    uint64_t sustainDuration;
    uint64_t releaseDuration;
    float    attackGain;
    float    sustainGain;
    float    releaseGain;
    uint32_t playCount;
    uint64_t repeatDelay;
};

struct GameInputForceFeedbackMagnitude
{
    float linearX;
    float linearY;
    float linearZ;
    float angularX;
    float angularY;
    float angularZ;
    float normal;
};

struct GameInputForceFeedbackConditionParams
{
    GameInputForceFeedbackMagnitude magnitude;
    float                           positiveCoefficient;
    float                           negativeCoefficient;
    float                           maxPositiveMagnitude;
    float                           maxNegativeMagnitude;
    float                           deadZone;
    float                           bias;
};

struct GameInputForceFeedbackConstantParams
{
    GameInputForceFeedbackEnvelope  envelope;
    GameInputForceFeedbackMagnitude magnitude;
};

struct GameInputForceFeedbackPeriodicParams
{
    GameInputForceFeedbackEnvelope  envelope;
    GameInputForceFeedbackMagnitude magnitude;
    float                           frequency;
    float                           phase;
    float                           bias;
};

struct GameInputForceFeedbackRampParams
{
    GameInputForceFeedbackEnvelope  envelope;
    GameInputForceFeedbackMagnitude startMagnitude;
    GameInputForceFeedbackMagnitude endMagnitude;
};

struct GameInputForceFeedbackParams
{
    GameInputForceFeedbackEffectKind kind;
    union
    {
        GameInputForceFeedbackConstantParams  constant;
        GameInputForceFeedbackRampParams      ramp;
        GameInputForceFeedbackPeriodicParams  sineWave;
        GameInputForceFeedbackPeriodicParams  squareWave;
        GameInputForceFeedbackPeriodicParams  triangleWave;
        GameInputForceFeedbackPeriodicParams  sawtoothUpWave;
        GameInputForceFeedbackPeriodicParams  sawtoothDownWave;
        GameInputForceFeedbackConditionParams spring;
        GameInputForceFeedbackConditionParams friction;
        GameInputForceFeedbackConditionParams damper;
        GameInputForceFeedbackConditionParams inertia;
    } data;
};

struct GameInputRumbleParams
{
    float lowFrequency;
    float highFrequency;
    float leftTrigger;
    float rightTrigger;
};

enum GameInputElementKind
{
    GameInputElementKindNone   = 0,
    GameInputElementKindAxis   = 1,
    GameInputElementKindButton = 2,
    GameInputElementKindSwitch = 3
};

struct GameInputAxisMapping
{
    GameInputElementKind controllerElementKind;
    uint32_t             controllerIndex;

    // When axis is mapped from a axis
    bool isInverted;

    // When the axis is mapped from a button
    bool     fromTwoButtons;
    uint32_t buttonMinIndexValue;

    // When the axis is mapped from a switch
    GameInputSwitchPosition referenceDirection;
};

struct GameInputButtonMapping
{
    GameInputElementKind controllerElementKind;
    uint32_t             controllerIndex;

    // When the button is mapped from an axis
    bool isInverted;

    // Button mapped from button only needs the index

    // When the button is mapped from a switch
    GameInputSwitchPosition switchPosition;
};

const IID IID_IGameInput = {0x20efc1c7, 0x5d9a, 0x43ba, {0xb2, 0x6f, 0xb8, 0x07, 0xfa, 0x48, 0x60, 0x9c}};

DECLARE_INTERFACE_IID_(IGameInput, IUnknown, "20EFC1C7-5D9A-43BA-B26F-B807FA48609C")
{
    IFACEMETHOD_(uint64_t, GetCurrentTimestamp)() PURE;

    IFACEMETHOD(GetCurrentReading)(
        _In_ GameInputKind inputKind,
        _In_opt_ IGameInputDevice* device,
        _COM_Outptr_ IGameInputReading** reading) PURE;

    IFACEMETHOD(GetNextReading)(
        _In_ IGameInputReading* referenceReading,
        _In_ GameInputKind inputKind,
        _In_opt_ IGameInputDevice* device,
        _COM_Outptr_ IGameInputReading** reading) PURE;

    IFACEMETHOD(GetPreviousReading)(
        _In_ IGameInputReading* referenceReading,
        _In_ GameInputKind inputKind,
        _In_opt_ IGameInputDevice* device,
        _COM_Outptr_ IGameInputReading** reading) PURE;

    IFACEMETHOD(RegisterReadingCallback)(
        _In_opt_ IGameInputDevice* device,
        _In_ GameInputKind inputKind,
        _In_opt_ void* context,
        _In_ GameInputReadingCallback callbackFunc,
        _Out_opt_ _Result_zeroonfailure_ GameInputCallbackToken* callbackToken) PURE;

    IFACEMETHOD(RegisterDeviceCallback)(
        _In_opt_ IGameInputDevice* device,
        _In_ GameInputKind inputKind,
        _In_ GameInputDeviceStatus statusFilter,
        _In_ GameInputEnumerationKind enumerationKind,
        _In_opt_ void* context,
        _In_ GameInputDeviceCallback callbackFunc,
        _Out_opt_ _Result_zeroonfailure_ GameInputCallbackToken* callbackToken) PURE;

    IFACEMETHOD(RegisterSystemButtonCallback)(
        _In_opt_ IGameInputDevice* device,
        _In_ GameInputSystemButtons buttonFilter,
        _In_opt_ void* context,
        _In_ GameInputSystemButtonCallback callbackFunc,
        _Out_opt_ _Result_zeroonfailure_ GameInputCallbackToken* callbackToken) PURE;

    IFACEMETHOD(RegisterKeyboardLayoutCallback)(
        _In_opt_ IGameInputDevice* device,
        _In_opt_ void* context,
        _In_ GameInputKeyboardLayoutCallback callbackFunc,
        _Out_opt_ _Result_zeroonfailure_ GameInputCallbackToken* callbackToken) PURE;

    IFACEMETHOD_(void, StopCallback)(
        _In_ GameInputCallbackToken callbackToken) PURE;

    IFACEMETHOD_(bool, UnregisterCallback)(
        _In_ GameInputCallbackToken callbackToken) PURE;

    IFACEMETHOD(CreateDispatcher)(
        _COM_Outptr_ IGameInputDispatcher** dispatcher) PURE;

    IFACEMETHOD(FindDeviceFromId)(
        _In_ const APP_LOCAL_DEVICE_ID* value,
        _COM_Outptr_ IGameInputDevice** device) PURE;

    IFACEMETHOD(FindDeviceFromPlatformString)(
        _In_ LPCWSTR value,
        _COM_Outptr_ IGameInputDevice** device) PURE;

    IFACEMETHOD_(void, SetFocusPolicy)(
        _In_ GameInputFocusPolicy policy) PURE;

    IFACEMETHOD(CreateAggregateDevice)(
        _In_ GameInputKind inputKind,
        _Out_ APP_LOCAL_DEVICE_ID* deviceId) PURE;

    IFACEMETHOD(DisableAggregateDevice)(
        _In_ const APP_LOCAL_DEVICE_ID* deviceId) PURE;
};

DECLARE_INTERFACE_IID_(IGameInputRawDeviceReport, IUnknown, "05A42D89-2CB6-45A3-874D-E635723587AB")
{
    IFACEMETHOD_(void, GetDevice)(
        _Outptr_ IGameInputDevice** device) PURE;

    IFACEMETHOD_(void, GetReportInfo)(
        _Out_ GameInputRawDeviceReportInfo* reportInfo) PURE;

    IFACEMETHOD_(size_t, GetRawDataSize)() PURE;

    IFACEMETHOD_(size_t, GetRawData)(
        _In_ size_t bufferSize,
        _Out_writes_(bufferSize) void* buffer) PURE;

    IFACEMETHOD_(bool, SetRawData)(
        _In_ size_t bufferSize,
        _In_reads_(bufferSize) const void* buffer) PURE;
};

DECLARE_INTERFACE_IID_(IGameInputReading, IUnknown, "C81C4CDE-ED1A-4631-A30F-C556A6241A1F")
{
    IFACEMETHOD_(GameInputKind, GetInputKind)() PURE;

    IFACEMETHOD_(uint64_t, GetTimestamp)() PURE;

    IFACEMETHOD_(void, GetDevice)(
        _Outptr_ IGameInputDevice** device) PURE;

    IFACEMETHOD_(uint32_t, GetControllerAxisCount)() PURE;

    IFACEMETHOD_(uint32_t, GetControllerAxisState)(
        _In_ uint32_t stateArrayCount,
        _Out_writes_(stateArrayCount) float* stateArray) PURE;

    IFACEMETHOD_(uint32_t, GetControllerButtonCount)() PURE;

    IFACEMETHOD_(uint32_t, GetControllerButtonState)(
        _In_ uint32_t stateArrayCount,
        _Out_writes_(stateArrayCount) bool* stateArray) PURE;

    IFACEMETHOD_(uint32_t, GetControllerSwitchCount)() PURE;

    IFACEMETHOD_(uint32_t, GetControllerSwitchState)(
        _In_ uint32_t stateArrayCount,
        _Out_writes_(stateArrayCount) GameInputSwitchPosition* stateArray) PURE;

    IFACEMETHOD_(uint32_t, GetKeyCount)() PURE;

    IFACEMETHOD_(uint32_t, GetKeyState)(
        _In_ uint32_t stateArrayCount,
        _Out_writes_(stateArrayCount) GameInputKeyState* stateArray) PURE;

    IFACEMETHOD_(bool, GetMouseState)(
        _Out_ GameInputMouseState* state) PURE;

    IFACEMETHOD_(bool, GetSensorsState)(
        _Out_ GameInputSensorsState* state) PURE;

    IFACEMETHOD_(bool, GetArcadeStickState)(
        _Out_ GameInputArcadeStickState* state) PURE;

    IFACEMETHOD_(bool, GetFlightStickState)(
        _Out_ GameInputFlightStickState* state) PURE;

    IFACEMETHOD_(bool, GetGamepadState)(
        _Out_ GameInputGamepadState* state) PURE;

    IFACEMETHOD_(bool, GetRacingWheelState)(
        _Out_ GameInputRacingWheelState* state) PURE;

    IFACEMETHOD_(bool, GetRawReport)(
        _Outptr_result_maybenull_ IGameInputRawDeviceReport** report) PURE;
};

DECLARE_INTERFACE_IID_(IGameInputDevice, IUnknown, "63E2F38B-A399-4275-8AE7-D4C6E524D12A")
{
    IFACEMETHOD(GetDeviceInfo)(
        _Outptr_ const GameInputDeviceInfo** info) PURE;

    IFACEMETHOD(GetHapticInfo)(
        _Out_ GameInputHapticInfo* info) PURE;

    IFACEMETHOD_(GameInputDeviceStatus, GetDeviceStatus)() PURE;

    IFACEMETHOD(CreateForceFeedbackEffect)(
        _In_ uint32_t motorIndex,
        _In_ const GameInputForceFeedbackParams* params,
        _COM_Outptr_ IGameInputForceFeedbackEffect** effect) PURE;

    IFACEMETHOD_(bool, IsForceFeedbackMotorPoweredOn)(
        _In_ uint32_t motorIndex) PURE;

    IFACEMETHOD_(void, SetForceFeedbackMotorGain)(
        _In_ uint32_t motorIndex,
        _In_ float masterGain) PURE;

    IFACEMETHOD_(void, SetRumbleState)(
        _In_opt_ const GameInputRumbleParams* params) PURE;

    IFACEMETHOD(DirectInputEscape)(
        _In_ uint32_t command,
        _In_reads_bytes_(bufferInSize) const void* bufferIn,
        _In_ uint32_t bufferInSize,
        _Out_writes_bytes_(bufferOutSize) void* bufferOut,
        _In_ uint32_t bufferOutSize,
        _Out_opt_ uint32_t* bufferOutSizeWritten) PURE;

    IFACEMETHOD(CreateInputMapper)(
        _COM_Outptr_ IGameInputMapper** inputMapper) PURE;

    IFACEMETHOD(GetExtraAxisCount)(
        _In_ GameInputKind inputKind,
        _Out_ uint32_t* extraAxisCount) PURE;

    IFACEMETHOD(GetExtraButtonCount)(
        _In_ GameInputKind inputKind,
        _Out_ uint32_t* extraButtonCount) PURE;

    IFACEMETHOD(GetExtraAxisIndexes)(
        _In_ GameInputKind inputKind,
        _In_ uint32_t extraAxisCount,
        _Out_writes_(extraAxisCount) uint8_t* extraAxisIndexes) PURE;

    IFACEMETHOD(GetExtraButtonIndexes)(
        _In_ GameInputKind inputKind,
        _In_ uint32_t extraButtonCount,
        _Out_writes_(extraButtonCount) uint8_t* extraButtonIndexes) PURE;

    IFACEMETHOD(CreateRawDeviceReport)(
        _In_ uint32_t reportId,
        _In_ GameInputRawDeviceReportKind reportKind,
        _COM_Outptr_ IGameInputRawDeviceReport** report) PURE;

    IFACEMETHOD(SendRawDeviceOutput)(
        _In_ IGameInputRawDeviceReport* report) PURE;
};

DECLARE_INTERFACE_IID_(IGameInputDispatcher, IUnknown, "415EED2E-98CB-42C2-8F28-B94601074E31")
{
    IFACEMETHOD_(bool, Dispatch)(
        _In_ uint64_t quotaInMicroseconds) PURE;

    IFACEMETHOD(OpenWaitHandle)(
        _Outptr_result_nullonfailure_ HANDLE* waitHandle) PURE;
};

DECLARE_INTERFACE_IID_(IGameInputForceFeedbackEffect, IUnknown, "FF61096A-3373-4093-A1DF-6D31846B3511")
{
    IFACEMETHOD_(void, GetDevice)(
        _Outptr_ IGameInputDevice** device) PURE;

    IFACEMETHOD_(uint32_t, GetMotorIndex)() PURE;

    IFACEMETHOD_(float, GetGain)() PURE;

    IFACEMETHOD_(void, SetGain)(
        _In_ float gain) PURE;

    IFACEMETHOD_(void, GetParams)(
        _Out_ GameInputForceFeedbackParams* params) PURE;

    IFACEMETHOD_(bool, SetParams)(
        _In_ const GameInputForceFeedbackParams* params) PURE;

    IFACEMETHOD_(GameInputFeedbackEffectState, GetState)() PURE;

    IFACEMETHOD_(void, SetState)(
        _In_ GameInputFeedbackEffectState state) PURE;
};

DECLARE_INTERFACE_IID_(IGameInputMapper, IUnknown, "3C600700-F16C-49CE-9BE6-6A2EF752ED5E")
{
    IFACEMETHOD_(bool, GetArcadeStickButtonMappingInfo)(
        _In_ GameInputArcadeStickButtons buttonElement,
		_Out_ GameInputButtonMapping* mapping) PURE;

    IFACEMETHOD_(bool, GetFlightStickAxisMappingInfo)(
        _In_ GameInputFlightStickAxes axisElement,
		_Out_ GameInputAxisMapping* mapping) PURE;

    IFACEMETHOD_(bool, GetFlightStickButtonMappingInfo)(
        _In_ GameInputFlightStickButtons buttonElement,
		_Out_ GameInputButtonMapping* mapping) PURE;

    IFACEMETHOD_(bool, GetGamepadAxisMappingInfo)(
        _In_ GameInputGamepadAxes axisElement,
		_Out_ GameInputAxisMapping* mapping) PURE;

    IFACEMETHOD_(bool, GetGamepadButtonMappingInfo)(
        _In_ GameInputGamepadButtons buttonElement,
		_Out_ GameInputButtonMapping* mapping) PURE;

    IFACEMETHOD_(bool, GetRacingWheelAxisMappingInfo)(
        _In_ GameInputRacingWheelAxes axisElement,
		_Out_ GameInputAxisMapping* mapping) PURE;

    IFACEMETHOD_(bool, GetRacingWheelButtonMappingInfo)(
        _In_ GameInputRacingWheelButtons buttonElement,
        _Out_ GameInputButtonMapping* mapping) PURE;
};

STDAPI GameInputInitialize(
    _In_ REFIID riid,
    _COM_Outptr_ LPVOID* ppv);

inline HRESULT GameInputCreate(
    _COM_Outptr_ IGameInput** gameInput)
{
    return GameInputInitialize(
        IID_IGameInput,
        reinterpret_cast<void**>(gameInput));
}

const LONG GAMEINPUT_FACILITY = 0x38A;

const HRESULT GAMEINPUT_E_DEVICE_DISCONNECTED               = _HRESULT_TYPEDEF_(0x838A0001L);
const HRESULT GAMEINPUT_E_DEVICE_NOT_FOUND                  = _HRESULT_TYPEDEF_(0x838A0002L);
const HRESULT GAMEINPUT_E_READING_NOT_FOUND                 = _HRESULT_TYPEDEF_(0x838A0003L);
const HRESULT GAMEINPUT_E_REFERENCE_READING_TOO_OLD         = _HRESULT_TYPEDEF_(0x838A0004L);
const HRESULT GAMEINPUT_E_FEEDBACK_NOT_SUPPORTED            = _HRESULT_TYPEDEF_(0x838A0007L);
const HRESULT GAMEINPUT_E_OBJECT_NO_LONGER_EXISTS           = _HRESULT_TYPEDEF_(0x838A0008L);
const HRESULT GAMEINPUT_E_CALLBACK_NOT_FOUND                = _HRESULT_TYPEDEF_(0x838A0009L);
const HRESULT GAMEINPUT_E_HAPTIC_INFO_NOT_FOUND             = _HRESULT_TYPEDEF_(0x838A000AL);
const HRESULT GAMEINPUT_E_AGGREGATE_OPERATION_NOT_SUPPORTED = _HRESULT_TYPEDEF_(0x838A000BL);
const HRESULT GAMEINPUT_E_INPUT_KIND_NOT_PRESENT            = _HRESULT_TYPEDEF_(0x838A000CL);

}} // namespace GameInput::v3

#endif // #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
#pragma endregion // Application Family or OneCore Family or Games Family
