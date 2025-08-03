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

#include "../SDL_gamepad_c.h"

#define SINPUT_REPORT_IDX_BUTTONS_0     3
#define SINPUT_REPORT_IDX_BUTTONS_1     4
#define SINPUT_REPORT_IDX_BUTTONS_2     5
#define SINPUT_REPORT_IDX_BUTTONS_3     6
#define SINPUT_REPORT_IDX_LEFT_X        7
#define SINPUT_REPORT_IDX_LEFT_Y        9
#define SINPUT_REPORT_IDX_RIGHT_X       11
#define SINPUT_REPORT_IDX_RIGHT_Y       13
#define SINPUT_REPORT_IDX_LEFT_TRIGGER  15
#define SINPUT_REPORT_IDX_RIGHT_TRIGGER 17
#define SINPUT_REPORT_IDX_IMU_TIMESTAMP 19
#define SINPUT_REPORT_IDX_IMU_ACCEL_X   23
#define SINPUT_REPORT_IDX_IMU_ACCEL_Y   25
#define SINPUT_REPORT_IDX_IMU_ACCEL_Z   27
#define SINPUT_REPORT_IDX_IMU_GYRO_X    29
#define SINPUT_REPORT_IDX_IMU_GYRO_Y    31
#define SINPUT_REPORT_IDX_IMU_GYRO_Z    33
#define SINPUT_REPORT_IDX_TOUCH1_X      35
#define SINPUT_REPORT_IDX_TOUCH1_Y      37
#define SINPUT_REPORT_IDX_TOUCH1_P      39
#define SINPUT_REPORT_IDX_TOUCH2_X      41
#define SINPUT_REPORT_IDX_TOUCH2_Y      43
#define SINPUT_REPORT_IDX_TOUCH2_P      45

#define SINPUT_BTN_IDX_EAST          0
#define SINPUT_BTN_IDX_SOUTH         1
#define SINPUT_BTN_IDX_NORTH         2
#define SINPUT_BTN_IDX_WEST          3
#define SINPUT_BTN_IDX_DPAD_UP       4
#define SINPUT_BTN_IDX_DPAD_DOWN     5
#define SINPUT_BTN_IDX_DPAD_LEFT     6
#define SINPUT_BTN_IDX_DPAD_RIGHT    7

#define SINPUT_BTN_IDX_LEFT_STICK    8
#define SINPUT_BTN_IDX_RIGHT_STICK   9
#define SINPUT_BTN_IDX_LEFT_BUMPER   10
#define SINPUT_BTN_IDX_RIGHT_BUMPER  11
#define SINPUT_BTN_IDX_LEFT_TRIGGER  12
#define SINPUT_BTN_IDX_RIGHT_TRIGGER 13
#define SINPUT_BTN_IDX_LEFT_PADDLE1  14
#define SINPUT_BTN_IDX_RIGHT_PADDLE1 15

#define SINPUT_BTN_IDX_START         16
#define SINPUT_BTN_IDX_BACK          17
#define SINPUT_BTN_IDX_GUIDE         18
#define SINPUT_BTN_IDX_CAPTURE       19
#define SINPUT_BTN_IDX_LEFT_PADDLE2  20
#define SINPUT_BTN_IDX_RIGHT_PADDLE2 21
#define SINPUT_BTN_IDX_TOUCHPAD1     22
#define SINPUT_BTN_IDX_TOUCHPAD2     23

#define SINPUT_BTN_IDX_POWER         24
#define SINPUT_BTN_IDX_MISC4         25
#define SINPUT_BTN_IDX_MISC5         26
#define SINPUT_BTN_IDX_MISC6         27
#define SINPUT_BTN_IDX_MISC7         28
#define SINPUT_BTN_IDX_MISC8         29
#define SINPUT_BTN_IDX_MISC9         30
#define SINPUT_BTN_IDX_MISC10        31

typedef enum
{
    k_eSInputanalog_None,
    k_eSInputanalog_One,
    k_eSInputanalog_Two,
    k_eSInputanalog_Max,
} ESInputanalogStyle;

typedef enum
{
    k_eSInputMiscTriggerStyle_None,
    k_eSInputMiscTriggerStyle_Analog,
    k_eSInputMiscTriggerStyle_Digital,
    k_eSInputMiscTriggerStyle_AnalogBumpers,
    k_eSInputMiscTriggerStyle_DigitalBumpers,
    k_eSInputMiscTriggerStyle_Max,
} ESInputTriggerStyle;

typedef enum
{
    k_eSInputPaddleStyle_None,
    k_eSInputPaddleStyle_Two,
    k_eSInputPaddleStyle_Four,
    k_eSInputPaddleStyle_Max,
} ESInputPaddleStyle;

typedef enum
{
    k_eSInputHasButtonStyle_No,
    k_eSInputHasButtonStyle_Yes,
    k_eSInputHasButtonStyle_Max,
} ESInputHasButtonStyle;

typedef enum
{
    k_eSInputMiscButtonStyle_None = 0,
    k_eSInputMiscButtonStyle_One = 1,
    k_eSInputMiscButtonStyle_Two,
    k_eSInputMiscButtonStyle_Three,
    k_eSInputMiscButtonStyle_Four,
    k_eSInputMiscButtonStyle_Five,
    k_eSInputMiscButtonStyle_Six,
    k_eSInputMiscButtonStyle_Seven,
    k_eSInputMiscButtonStyle_Max,
} ESInputMiscButtonStyle;

typedef enum
{
    k_eSInputControllerType_Dynamic = 0x00,
    k_eSInputControllerType_HHL_PROGCC = 0xffff0100,
    k_eSInputControllerType_HHL_GCCULT = 0xffff0101,
    k_eSInputControllerType_LoadFirmware = 0xffffffff,
} ESinputControllerType;

struct SDL_SInputFeatures
{
    ESinputControllerType controller_type;
    SDL_GamepadFaceStyle face_style;

    ESInputanalogStyle analog_style;
    ESInputTriggerStyle trigger_style;
    ESInputHasButtonStyle guide_style;
    ESInputPaddleStyle paddle_style;
    ESInputHasButtonStyle click_style;
    ESInputMiscButtonStyle misc_button_style;
};

static inline Uint16 HIDAPI_DriverSInput_DeriveVersion(Uint8 *buttons, bool left_analog, bool right_analog, bool left_trigger, bool right_trigger)
{
    Uint16 version = 0;

    // Analog sticks
    if (right_analog) {
        version += k_eSInputanalog_Two;
    } else if (left_analog) {
        version += k_eSInputanalog_One;
    }
    version *= k_eSInputanalog_Max;

    // Trigger style
    bool analogTriggers = left_trigger || right_trigger;
    bool digitalTriggers = (buttons[SINPUT_BTN_IDX_LEFT_TRIGGER >> 3] & (1 << (SINPUT_BTN_IDX_LEFT_TRIGGER & 0x07))) ||
                          (buttons[SINPUT_BTN_IDX_RIGHT_TRIGGER >> 3] & (1 << (SINPUT_BTN_IDX_RIGHT_TRIGGER & 0x07)));
    bool bumpers = (buttons[SINPUT_BTN_IDX_LEFT_BUMPER >> 3] & (1 << (SINPUT_BTN_IDX_LEFT_BUMPER & 0x07))) ||
                        (buttons[SINPUT_BTN_IDX_RIGHT_BUMPER >> 3] & (1 << (SINPUT_BTN_IDX_RIGHT_BUMPER & 0x07)));
    if (analogTriggers && bumpers) {
        version += k_eSInputMiscTriggerStyle_AnalogBumpers;
    } else if (digitalTriggers && bumpers) {
        version += k_eSInputMiscTriggerStyle_DigitalBumpers;
    } else if (analogTriggers) {
        version += k_eSInputMiscTriggerStyle_Analog;
    } else if (digitalTriggers) {
        version += k_eSInputMiscTriggerStyle_Digital;
    } else {
        version += k_eSInputMiscTriggerStyle_None;
    }
    version *= k_eSInputMiscTriggerStyle_Max;

    // Guide button style
    if ((buttons[SINPUT_BTN_IDX_GUIDE >> 3] & (1 << (SINPUT_BTN_IDX_GUIDE & 0x07))) != 0)
        version += k_eSInputHasButtonStyle_Yes;
    version *= k_eSInputHasButtonStyle_Max;

    // Paddles
    bool hasTopPaddles = (buttons[SINPUT_BTN_IDX_LEFT_PADDLE1 >> 3] & (1 << (SINPUT_BTN_IDX_LEFT_PADDLE1 & 0x07))) != 0 ||
                        (buttons[SINPUT_BTN_IDX_RIGHT_PADDLE1 >> 3] & (1 << (SINPUT_BTN_IDX_RIGHT_PADDLE1 & 0x07))) != 0;
    bool hasBottomPaddles = (buttons[SINPUT_BTN_IDX_LEFT_PADDLE2 >> 3] & (1 << (SINPUT_BTN_IDX_LEFT_PADDLE2 & 0x07))) != 0 ||
                            (buttons[SINPUT_BTN_IDX_RIGHT_PADDLE2 >> 3] & (1 << (SINPUT_BTN_IDX_RIGHT_PADDLE2 & 0x07))) != 0;
    if (hasTopPaddles && hasBottomPaddles) {
        version += k_eSInputPaddleStyle_Four;
    } else if (hasTopPaddles) {
        version += k_eSInputPaddleStyle_Two;
    } else {
        version += k_eSInputPaddleStyle_None;
    }
    version *= k_eSInputPaddleStyle_Max;

    // Touchpad click style
    if ((buttons[SINPUT_BTN_IDX_TOUCHPAD1 >> 3] & (1 << (SINPUT_BTN_IDX_TOUCHPAD1 & 0x07))) != 0)
        version += k_eSInputHasButtonStyle_Yes;
    version *= k_eSInputHasButtonStyle_Max;

    // Misc button style
    Uint8 miscButtonCount = 0;
    if ((buttons[SINPUT_BTN_IDX_CAPTURE >> 3] & (1 << (SINPUT_BTN_IDX_CAPTURE & 0x07))) != 0)
        miscButtonCount++;
    for (int i = SINPUT_BTN_IDX_TOUCHPAD2; i <= SINPUT_BTN_IDX_MISC10; ++i) {
        if (buttons[i >> 3] & (1 << (i & 0x07))) {
            ++miscButtonCount;
        }
    }

    return version;
}

static inline void HIDAPI_DriverSInput_GetControllerType(
    Uint16 vendor, Uint16 product, Uint16 version, Uint8 subtype, struct SDL_SInputFeatures *features)
{
    features->face_style = (subtype & 0xE0) >> 5;
    features->controller_type = k_eSInputControllerType_Dynamic;

    if (vendor == USB_VENDOR_RASPBERRYPI && product == USB_PRODUCT_HANDHELDLEGEND_PROGCC) {
        features->controller_type = k_eSInputControllerType_HHL_PROGCC;
    } else if (vendor == USB_VENDOR_RASPBERRYPI && product == USB_PRODUCT_HANDHELDLEGEND_GCULTIMATE) {
        features->controller_type = k_eSInputControllerType_HHL_GCCULT;
    }

    // Decode dynamic features
    features->analog_style = version % k_eSInputanalog_Max;
    version /= k_eSInputanalog_Max;
    features->trigger_style = version % k_eSInputMiscTriggerStyle_Max;
    version /= k_eSInputMiscTriggerStyle_Max;
    features->guide_style = version % k_eSInputHasButtonStyle_Max;
    version /= k_eSInputHasButtonStyle_Max;
    features->paddle_style = version % k_eSInputPaddleStyle_Max;
    version /= k_eSInputPaddleStyle_Max;
    features->click_style = version % k_eSInputHasButtonStyle_Max;
    // version /= k_eSInputHasButtonStyle_Max;
    features->misc_button_style = version % k_eSInputMiscButtonStyle_Max;
    version /= k_eSInputMiscButtonStyle_Max;
}