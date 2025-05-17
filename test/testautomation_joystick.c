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
 * \sa SDL_AttachVirtualJoystick
 */
static int SDLCALL TestVirtualJoystick(void *arg)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Joystick *joystick = NULL;
    SDL_Gamepad *gamepad = NULL;
    SDL_JoystickID device_id;

    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMEPAD), "SDL_InitSubSystem(SDL_INIT_GAMEPAD)");

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.vendor_id = USB_VENDOR_NVIDIA;
    desc.product_id = USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V104;
    desc.name = "Virtual NVIDIA SHIELD Controller";
    device_id = SDL_AttachVirtualJoystick(&desc);
    SDLTest_AssertCheck(device_id > 0, "SDL_AttachVirtualJoystick() -> %" SDL_PRIs32 " (expected > 0)", device_id);
    SDLTest_AssertCheck(SDL_IsJoystickVirtual(device_id), "SDL_IsJoystickVirtual()");
    if (device_id > 0) {
        joystick = SDL_OpenJoystick(device_id);
        SDLTest_AssertCheck(joystick != NULL, "SDL_OpenJoystick()");
        if (joystick) {
            {
                const char *dname = SDL_GetJoystickName(joystick);
                SDLTest_AssertCheck(SDL_strcmp(dname, desc.name) == 0, "SDL_GetJoystickName() -> \"%s\" (expected \"%s\")", dname, desc.name);
            }
            {
                Uint16 vendor_id = SDL_GetJoystickVendor(joystick);
                SDLTest_AssertCheck(vendor_id == desc.vendor_id, "SDL_GetJoystickVendor() -> 0x%04x (expected 0x%04x)", vendor_id, desc.vendor_id);
            }
            {
                Uint16 product_id = SDL_GetJoystickProduct(joystick);
                SDLTest_AssertCheck(product_id == desc.product_id, "SDL_GetJoystickProduct() -> 0x%04x (expected 0x%04x)", product_id, desc.product_id);
            }
            {
                Uint16 product_version = SDL_GetJoystickProductVersion(joystick);
                SDLTest_AssertCheck(product_version == 0, "SDL_GetJoystickProductVersion() -> 0x%04x (expected 0x%04x)", product_version, 0);
            }
            {
                Uint16 firmware_Version = SDL_GetJoystickFirmwareVersion(joystick);
                SDLTest_AssertCheck(firmware_Version == 0, "SDL_GetJoystickFirmwareVersion() -> 0x%04x (expected 0x%04x)", firmware_Version, 0);
            }
            {
                const char *serial = SDL_GetJoystickSerial(joystick);
                SDLTest_AssertCheck(serial == NULL, "SDL_GetJoystickSerial() -> %s (expected %s)", serial, "(null)");
            }
            {
                SDL_JoystickType type = SDL_GetJoystickType(joystick);
                SDLTest_AssertCheck(type == desc.type, "SDL_GetJoystickType() -> %d (expected %d)", type, desc.type);
            }
            {
                Uint16 naxes = SDL_GetNumJoystickAxes(joystick);
                SDLTest_AssertCheck(naxes == desc.naxes, "SDL_GetNumJoystickAxes() -> 0x%04x (expected 0x%04x)", naxes, desc.naxes);
            }
            {
                int nballs = SDL_GetNumJoystickBalls(joystick);
                SDLTest_AssertCheck(nballs == 0, "SDL_GetNumJoystickBalls() -> %d (expected %d)", nballs, 0);
            }
            {
                int nhats = SDL_GetNumJoystickHats(joystick);
                SDLTest_AssertCheck(nhats == desc.nhats, "SDL_GetNumJoystickHats() -> %d (expected %d)", nhats, desc.nhats);
            }
            {
                int nbuttons = SDL_GetNumJoystickButtons(joystick);
                SDLTest_AssertCheck(nbuttons == desc.nbuttons, "SDL_GetNumJoystickButtons() -> %d (expected %d)", nbuttons, desc.nbuttons);
            }

            SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, true)");
            SDL_UpdateJoysticks();
            SDLTest_AssertCheck(SDL_GetJoystickButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH) == true, "SDL_GetJoystickButton(SDL_GAMEPAD_BUTTON_SOUTH) == true");

            SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, false)");
            SDL_UpdateJoysticks();
            SDLTest_AssertCheck(SDL_GetJoystickButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH) == false, "SDL_GetJoystickButton(SDL_GAMEPAD_BUTTON_SOUTH) == false");

            gamepad = SDL_OpenGamepad(SDL_GetJoystickID(joystick));
            SDLTest_AssertCheck(gamepad != NULL, "SDL_OpenGamepad() succeeded");
            if (gamepad) {
                {
                    const char *name = SDL_GetGamepadName(gamepad);
                    SDLTest_AssertCheck(name && SDL_strcmp(name, desc.name) == 0, "SDL_GetGamepadName() -> \"%s\" (expected \"%s\")", name, desc.name);
                }
                {
                    Uint16 vendor_id = SDL_GetGamepadVendor(gamepad);
                    SDLTest_AssertCheck(vendor_id == desc.vendor_id, "SDL_GetGamepadVendor() -> 0x%04x (expected 0x%04x)", vendor_id, desc.vendor_id);
                }
                {
                    Uint16 product_id = SDL_GetGamepadProduct(gamepad);
                    SDLTest_AssertCheck(product_id == desc.product_id, "SDL_GetGamepadProduct() -> 0x%04x (expected 0x%04x)", product_id, desc.product_id);
                }

                /* Set an explicit mapping with a different name */
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff0013db5669727475616c2043007601,Virtual Gamepad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
                {
                    const char *name = SDL_GetGamepadName(gamepad);
                    SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Gamepad") == 0, "SDL_GetGamepadName() ->\"%s\" (expected \"%s\")", name, "Virtual Gamepad");
                }
                {
                    SDL_GamepadButtonLabel label = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
                    SDLTest_AssertCheck(label == SDL_GAMEPAD_BUTTON_LABEL_A, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) -> %d (expected %d [%s])",
                                        label, SDL_GAMEPAD_BUTTON_LABEL_A, "SDL_GAMEPAD_BUTTON_LABEL_A");
                }
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_A, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_A");

                /* Set the south button and verify that the gamepad responds */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, true)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == true, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == true");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, false)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == false, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == false");

                /* Set an explicit mapping with legacy GameCube style buttons */
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff0013db5669727475616c2043007601,Virtual Nintendo GameCube,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,hint:SDL_GAMECONTROLLER_USE_GAMECUBE_LABELS:=1,");
                {
                    const char *name = SDL_GetGamepadName(gamepad);
                    SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Nintendo GameCube") == 0, "SDL_GetGamepadName() -> \"%s\" (expected \"%s\")", name, "Virtual Nintendo GameCube");
                }
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_EAST) == SDL_GAMEPAD_BUTTON_LABEL_X, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_EAST) == SDL_GAMEPAD_BUTTON_LABEL_X");

                /* Set the east button and verify that the gamepad responds, mapping "B" to the west button */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, true), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_EAST, true)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST) == true, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_WEST) == true");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, false), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_EAST, false)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST) == false, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_WEST) == false");

                /* Set an explicit mapping with legacy Nintendo style buttons */
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff0013db5669727475616c2043007601,Virtual Nintendo Gamepad,a:b1,b:b0,x:b3,y:b2,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1,");
                {
                    const char *name = SDL_GetGamepadName(gamepad);
                    SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Nintendo Gamepad") == 0, "SDL_GetGamepadName() -> \"%s\" (expected \"%s\")", name, "Virtual Nintendo Gamepad");
                }
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B");

                /* Set the south button and verify that the gamepad responds */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, true)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == true, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == true");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, false)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == false, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == false");

                /* Set an explicit mapping with PS4 style buttons */
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff0013db5669727475616c2043007601,Virtual PS4 Gamepad,type:ps4,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
                {
                    const char *name = SDL_GetGamepadName(gamepad);
                    SDLTest_AssertCheck(SDL_strcmp(name, "Virtual PS4 Gamepad") == 0, "SDL_GetGamepadName() -> \"%s\" (expected \"%s\")", name, "Virtual PS4 Gamepad");
                }
                SDLTest_AssertCheck(SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_CROSS, "SDL_GetGamepadButtonLabel(SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_CROSS");

                /* Set the south button and verify that the gamepad responds */
                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, true)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == true, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == true");

                SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false), "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_SOUTH, false)");
                SDL_UpdateJoysticks();
                SDLTest_AssertCheck(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == false, "SDL_GetGamepadButton(SDL_GAMEPAD_BUTTON_SOUTH) == false");

                SDL_CloseGamepad(gamepad);
            }

            SDL_CloseJoystick(joystick);
        }
        SDLTest_AssertCheck(SDL_DetachVirtualJoystick(device_id), "SDL_DetachVirtualJoystick()");
    }
    SDLTest_AssertCheck(!SDL_IsJoystickVirtual(device_id), "!SDL_IsJoystickVirtual()");

    SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Joystick routine test cases */
static const SDLTest_TestCaseReference joystickTest1 = {
    TestVirtualJoystick, "TestVirtualJoystick", "Test virtual joystick functionality", TEST_ENABLED
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
