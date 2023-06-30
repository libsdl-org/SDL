/**
 * Joystick test suite
 */

#include "SDL.h"
#include "SDL_test.h"
#include "../src/joystick/usb_ids.h"

/* ================= Test Case Implementation ================== */

/* Test case functions */

/**
 * @brief Check virtual joystick creation
 *
 * @sa SDL_JoystickAttachVirtualEx
 */
static int
TestVirtualJoystick(void *arg)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Joystick *joystick = NULL;
    int device_index;

    SDLTest_AssertCheck(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0, "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)");

    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.vendor_id = USB_VENDOR_NVIDIA;
    desc.product_id = USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V104;
    desc.name = "Virtual NVIDIA SHIELD Controller";
    device_index = SDL_JoystickAttachVirtualEx(&desc);
    SDLTest_AssertCheck(device_index >= 0, "SDL_JoystickAttachVirtualEx()");
    SDLTest_AssertCheck(SDL_JoystickIsVirtual(device_index), "SDL_JoystickIsVirtual()");
    if (device_index >= 0) {
        joystick = SDL_JoystickOpen(device_index);
        SDLTest_AssertCheck(joystick != NULL, "SDL_JoystickOpen()");
        if (joystick) {
            SDLTest_AssertCheck(SDL_strcmp(SDL_JoystickName(joystick), desc.name) == 0, "SDL_JoystickName()");
            SDLTest_AssertCheck(SDL_JoystickGetVendor(joystick) == desc.vendor_id, "SDL_JoystickGetVendor()");
            SDLTest_AssertCheck(SDL_JoystickGetProduct(joystick) == desc.product_id, "SDL_JoystickGetProduct()");
            SDLTest_AssertCheck(SDL_JoystickGetProductVersion(joystick) == 0, "SDL_JoystickGetProductVersion()");
            SDLTest_AssertCheck(SDL_JoystickGetFirmwareVersion(joystick) == 0, "SDL_JoystickGetFirmwareVersion()");
            SDLTest_AssertCheck(SDL_JoystickGetSerial(joystick) == NULL, "SDL_JoystickGetSerial()");
            SDLTest_AssertCheck(SDL_JoystickGetType(joystick) == desc.type, "SDL_JoystickGetType()");
            SDLTest_AssertCheck(SDL_JoystickNumAxes(joystick) == desc.naxes, "SDL_JoystickNumAxes()");
            SDLTest_AssertCheck(SDL_JoystickNumBalls(joystick) == 0, "SDL_JoystickNumBalls()");
            SDLTest_AssertCheck(SDL_JoystickNumHats(joystick) == desc.nhats, "SDL_JoystickNumHats()");
            SDLTest_AssertCheck(SDL_JoystickNumButtons(joystick) == desc.nbuttons, "SDL_JoystickNumButtons()");

            SDLTest_AssertCheck(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, SDL_PRESSED) == 0, "SDL_JoystickSetVirtualButton(SDL_CONTROLLER_BUTTON_A, SDL_PRESSED)");
            SDL_JoystickUpdate();
            SDLTest_AssertCheck(SDL_JoystickGetButton(joystick, SDL_CONTROLLER_BUTTON_A) == SDL_PRESSED, "SDL_JoystickGetButton(SDL_CONTROLLER_BUTTON_A) == SDL_PRESSED");
            SDLTest_AssertCheck(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, SDL_RELEASED) == 0, "SDL_JoystickSetVirtualButton(SDL_CONTROLLER_BUTTON_A, SDL_RELEASED)");
            SDL_JoystickUpdate();
            SDLTest_AssertCheck(SDL_JoystickGetButton(joystick, SDL_CONTROLLER_BUTTON_A) == SDL_RELEASED, "SDL_JoystickGetButton(SDL_CONTROLLER_BUTTON_A) == SDL_RELEASED");

            SDL_JoystickClose(joystick);
        }
        SDLTest_AssertCheck(SDL_JoystickDetachVirtual(device_index) == 0, "SDL_JoystickDetachVirtual()");
    }
    SDLTest_AssertCheck(!SDL_JoystickIsVirtual(device_index), "!SDL_JoystickIsVirtual()");

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);

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
