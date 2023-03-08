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
 * \brief Check virtual joystick creation
 *
 * \sa SDL_AttachVirtualJoystickEx
 */
static int TestVirtualJoystick(void *arg)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Joystick *joystick = NULL;
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

            SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_A, SDL_PRESSED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_A, SDL_PRESSED)");
            SDL_UpdateJoysticks();
            SDLTest_AssertCheck(SDL_GetJoystickButton(joystick, SDL_GAMEPAD_BUTTON_A) == SDL_PRESSED, "SDL_GetJoystickButton(SDL_GAMEPAD_BUTTON_A) == SDL_PRESSED");
            SDLTest_AssertCheck(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_A, SDL_RELEASED) == 0, "SDL_SetJoystickVirtualButton(SDL_GAMEPAD_BUTTON_A, SDL_RELEASED)");

            SDL_UpdateJoysticks();
            SDLTest_AssertCheck(SDL_GetJoystickButton(joystick, SDL_GAMEPAD_BUTTON_A) == SDL_RELEASED, "SDL_GetJoystickButton(SDL_GAMEPAD_BUTTON_A) == SDL_RELEASED");

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
