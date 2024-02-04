/**
 * Joystick test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "../src/joystick/usb_ids.h"
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Test case functions */

/**
 * Check virtual joystick creation
 *
 * \sa SDL_AttachVirtualJoystickEx
 */
static int TestVirtualJoystick(void *arg)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Joystick *joystick = NULL;
    SDL_Gamepad *gamepad = NULL;
    SDL_JoystickID device_id;

    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMEPAD) == 0, "SDL_InitSubSystem(SDL_INIT_GAMEPAD)");

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_MAX;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_MAX;
    desc.vendor_id = USB_VENDOR_NVIDIA;
    desc.product_id = USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V104;
    desc.name = "Virtual NVIDIA SHIELD Controller";
    device_id = SDL_AttachVirtualJoystickEx(&desc);
    SDLTest_AssertCheck(device_id > 0, "SDL_AttachVirtualJoystickEx()");
    SDLTest_AssertCheck(SDL_IsJoystickVirtual(device_id), "SDL_IsJoystickVirtual()");
    if (device_id > 0) {
        joystick = SDL_OpenJoystick(device_id);
        SDLTest_AssertCheck(joystick != NULL, "SDL_OpenJoystick()");
        if (joystick) {
            SDLTest_AssertCheck(SDL_strcmp(SDL_GetJoystickName(joystick), desc.name) == 0, "SDL_GetJoystickName()");
            SDLTest_AssertCheck(SDL_GetJoystickVendor(joystick) == desc.vendor_id, "SDL_GetJoystickVendor()");
            SDLTest_AssertCheck(SDL_GetJoystickProduct(joystick) == desc.product_id, "SDL_GetJoystickProduct()");
            SDLTest_AssertCheck(SDL_GetJoystickProductVersion(joystick) == 0, "SDL_GetJoystickProductVersion()");
            SDLTest_AssertCheck(SDL_GetJoystickFirmwareVersion(joystick) == 0, "SDL_GetJoystickFirmwareVersion()");
            SDLTest_AssertCheck(SDL_GetJoystickSerial(joystick) == NULL, "SDL_GetJoystickSerial()");
            SDLTest_AssertCheck(SDL_GetJoystickType(joystick) == desc.type, "SDL_GetJoystickType()");
            SDLTest_AssertCheck(SDL_GetNumJoystickAxes(joystick) == desc.naxes, "SDL_GetNumJoystickAxes()");
            SDLTest_AssertCheck(SDL_GetNumJoystickHats(joystick) == desc.nhats, "SDL_GetNumJoystickHats()");
            SDLTest_AssertCheck(SDL_GetNumJoystickButtons(joystick) == desc.nbuttons, "SDL_GetNumJoystickButtons()");

            SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED)");
            SDL_UpdateJoysticks();
            SDLTest_AssertCheck(SDL_GetJoystickButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED, "SDL_GetJoystickButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED");

            SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED)");
            SDL_UpdateJoysticks();
            SDLTest_AssertCheck(SDL_GetJoystickButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED, "SDL_GetJoystickButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED");

            gamepad = SDL_OpenGamepad(SDL_GetJoystickInstanceID(joystick));
            SDLTest_AssertCheck(gamepad != NULL, "SDL_OpenGamepad() succeeded");
            if (gamepad) {
                SDLTest_AssertCheck(SDL_strcmp(SDL_GetGamepadName(gamepad), desc.name) == 0, "SDL_GetGamepadName()");
                SDLTest_AssertCheck(SDL_GetGamepadVendor(gamepad) == desc.vendor_id, "SDL_GetGamepadVendor()");
                SDLTest_AssertCheck(SDL_GetGamepadProduct(gamepad) == desc.product_id, "SDL_GetGamepadProduct()");

                /* Set an explicit mapping with a different name */
                SDL_SetGamepadMapping(SDL_GetJoystickInstanceID(joystick), "ff0013db5669727475616c2043007601,Virtual Gamepad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
                SDLTest_AssertCheck(SDL_strcmp(SDL_GetGamepadName(gamepad), "Virtual Gamepad") == 0, "SDL_GetGamepadName() == Virtual Gamepad");
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_A, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_A");

                /* Set the south button and verify that the gamepad responds */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED");

                /* Set an explicit mapping with legacy Nintendo style buttons */
                SDL_SetGamepadMapping(SDL_GetJoystickInstanceID(joystick), "ff0013db5669727475616c2043007601,Virtual Nintendo Gamepad,a:b1,b:b0,x:b3,y:b2,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1,");
                SDLTest_AssertCheck(SDL_strcmp(SDL_GetGamepadName(gamepad), "Virtual Nintendo Gamepad") == 0, "SDL_GetGamepadName() == Virtual Nintendo Gamepad");
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B");

                /* Set the south button and verify that the gamepad responds */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED");

                /* Set an explicit mapping with PS4 style buttons */
                SDL_SetGamepadMapping(SDL_GetJoystickInstanceID(joystick), "ff0013db5669727475616c2043007601,Virtual PS4 Gamepad,type:ps4,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
                SDLTest_AssertCheck(SDL_strcmp(SDL_GetGamepadName(gamepad), "Virtual PS4 Gamepad") == 0, "SDL_GetGamepadName() == Virtual PS4 Gamepad");
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_CROSS, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_CROSS");

                /* Set the south button and verify that the gamepad responds */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_PRESSED)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_PRESSED");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, SDL_RELEASED)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_RELEASED");

                SDL_CloseGamepad(gamepad);
            }

            SDL_CloseJoystick(joystick);
        }
        SDLTest_AssertCheck(SDL_DetachVirtualJoystick(device_id) == 0, "SDL_DetachVirtualJoystick()");
    }
    SDLTest_AssertCheck(!SDL_IsJoystickVirtual(device_id), "!SDL_IsJoystickVirtual()");

    SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Joystick routine test cases */
static const SDLTest_TestCaseReference joystickTest1 = {
    (SDLTest_TestCaseFp)TestVirtualJoystick, "TestVirtualJoystick", "Test virtual joystick functionality", TEST_ENABLED
};

/* Sequence of Joystick routine test cases */
static const SDLTest_TestCaseReference *joystickTests[] = {
    &joystickTest1,
    NULL
};

/* Joystick routine test suite (global) */
SDLTest_TestSuiteReference joystickTestSuite = {
    "Joystick",
    NULL,
    joystickTests,
    NULL
};
