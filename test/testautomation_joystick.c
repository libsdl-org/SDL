/**
 * Joystick test suite
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "../src/joystick/usb_ids.h"
#include "../src/joystick/hidapi/SDL_hidapi_sinput.h"
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Fixture */

/* Create a 32-bit writable surface for blitting tests */
static void SDLCALL joystickSetUp(void **arg)
{
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
}

static void SDLCALL joystickTearDown(void *arg)
{
    SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}


/* Test case functions */

/**
 * Check virtual joystick creation
 *
 * \sa SDL_AttachVirtualJoystick
 */
static int SDLCALL joystick_testVirtual(void *arg)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Joystick *joystick = NULL;
    SDL_Gamepad *gamepad = NULL;
    SDL_JoystickID device_id;

    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.vendor_id = USB_VENDOR_NVIDIA;
    desc.product_id = USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V103;
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
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff008316550900001072000000007601,Virtual Gamepad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
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
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff008316550900001072000000007601,Virtual Nintendo GameCube,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,hint:SDL_GAMECONTROLLER_USE_GAMECUBE_LABELS:=1,");
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
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff008316550900001072000000007601,Virtual Nintendo Gamepad,a:b1,b:b0,x:b3,y:b2,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1,");
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
                SDL_SetGamepadMapping(SDL_GetJoystickID(joystick), "ff008316550900001072000000007601,Virtual PS4 Gamepad,type:ps4,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
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

    return TEST_COMPLETED;
}

/**
 * Check gamepad mappings
 */
static int SDLCALL joystick_testMappings(void *arg)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Gamepad *gamepad = NULL;
    SDL_JoystickID device_id;

    /* Add a mapping for the virtual controller in advance */
    SDL_AddGamepadMapping("ff000000550900001472000000007601,Virtual Gamepad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");

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

    gamepad = SDL_OpenGamepad(device_id);
    SDLTest_AssertCheck(gamepad != NULL, "SDL_OpenGamepad() succeeded");
    if (gamepad) {
        /* Verify that the gamepad picked up the predefined mapping */
        {
            const char *name = SDL_GetGamepadName(gamepad);
            SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Gamepad") == 0, "SDL_GetGamepadName() ->\"%s\" (expected \"%s\")", name, "Virtual Gamepad");
        }

        /* Verify that the gamepad picks up a new mapping with no CRC */
        SDL_AddGamepadMapping("ff000000550900001472000000007601,Virtual Gamepad V2,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
        {
            const char *name = SDL_GetGamepadName(gamepad);
            SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Gamepad V2") == 0, "SDL_GetGamepadName() ->\"%s\" (expected \"%s\")", name, "Virtual Gamepad V2");
        }

        /* Verify that the gamepad picks up a new mapping with valid CRC */
        SDL_AddGamepadMapping("ff008316550900001472000000007601,Virtual Gamepad V3,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,");
        {
            const char *name = SDL_GetGamepadName(gamepad);
            SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Gamepad V3") == 0, "SDL_GetGamepadName() ->\"%s\" (expected \"%s\")", name, "Virtual Gamepad V3");
        }

        SDL_CloseGamepad(gamepad);
    }
    SDLTest_AssertCheck(SDL_DetachVirtualJoystick(device_id), "SDL_DetachVirtualJoystick()");

    /* Try matching mappings with a new CRC */
    desc.name = "Virtual NVIDIA SHIELD Controller V2";
    device_id = SDL_AttachVirtualJoystick(&desc);
    SDLTest_AssertCheck(device_id > 0, "SDL_AttachVirtualJoystick() -> %" SDL_PRIs32 " (expected > 0)", device_id);
    SDLTest_AssertCheck(SDL_IsJoystickVirtual(device_id), "SDL_IsJoystickVirtual()");

    gamepad = SDL_OpenGamepad(device_id);
    SDLTest_AssertCheck(gamepad != NULL, "SDL_OpenGamepad() succeeded");
    if (gamepad) {
        {
            const char *name = SDL_GetGamepadName(gamepad);
            SDLTest_AssertCheck(name && SDL_strcmp(name, "Virtual Gamepad V2") == 0, "SDL_GetGamepadName() ->\"%s\" (expected \"%s\")", name, "Virtual Gamepad V2");
        }
        SDL_CloseGamepad(gamepad);
    }
    SDLTest_AssertCheck(SDL_DetachVirtualJoystick(device_id), "SDL_DetachVirtualJoystick()");

    return TEST_COMPLETED;
}

/* Pin behaviour of GamepadType and button labels; mapping shouldn't be drift except intentionally. */

/* Attach a fake joystick with the given VID/PID so that SDL itself parses the
 * mapping and we can query the result through the public gamepad API. */
static SDL_Gamepad *OpenVirtualGamepadWithMapping(Uint16 vendor, Uint16 product, const char *mapping, SDL_JoystickID *device_id)
{
    SDL_VirtualJoystickDesc desc;
    SDL_Gamepad *gamepad = NULL;

    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.nhats = 1;
    desc.vendor_id = vendor;
    desc.product_id = product;
    desc.name = "Test Gamepad";
    *device_id = SDL_AttachVirtualJoystick(&desc);
    if (*device_id > 0 && SDL_SetGamepadMapping(*device_id, mapping)) {
        gamepad = SDL_OpenGamepad(*device_id);
    }
    return gamepad;
}

static void CloseVirtualGamepad(SDL_Gamepad *gamepad, SDL_JoystickID device_id)
{
    if (gamepad) {
        SDL_CloseGamepad(gamepad);
    }
    if (device_id > 0) {
        SDL_DetachVirtualJoystick(device_id);
    }
}

/* Get the label of the gamepad button that `jbutton` maps to */
static bool GetLabelForJoystickButton(SDL_Gamepad *gamepad, int jbutton, SDL_GamepadButtonLabel *label)
{
    int i, count = 0;
    SDL_GamepadBinding **bindings = SDL_GetGamepadBindings(gamepad, &count);
    bool found = false;

    if (bindings) {
        for (i = 0; i < count; ++i) {
            if (bindings[i]->input_type == SDL_GAMEPAD_BINDTYPE_BUTTON &&
                bindings[i]->input.button == jbutton &&
                bindings[i]->output_type == SDL_GAMEPAD_BINDTYPE_BUTTON) {
                *label = SDL_GetGamepadButtonLabel(gamepad, bindings[i]->output.button);
                found = true;
                break;
            }
        }
        SDL_free(bindings);
    }
    return found;
}

/**
 * Test gamepad type detection from VID/PID/name
 */
static int SDLCALL joystick_testGamepadTypeForVIDPID(void *arg)
{
    static const struct {
        Uint16 vendor;
        Uint16 product;
        const char *name;
        SDL_GamepadType expected_type;
    } vectors[] = {
        /* (Some) official Xbox controllers */
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX360_XUSB_CONTROLLER, "Xbox 360 Controller", SDL_GAMEPAD_TYPE_XBOX360 },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX360_WIRED_CONTROLLER, "Xbox 360 Controller", SDL_GAMEPAD_TYPE_XBOX360 },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX360_WIRELESS_RECEIVER, "Xbox 360 Wireless Controller", SDL_GAMEPAD_TYPE_XBOX360 },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_ONE_ADAPTIVE_BLUETOOTH, "Xbox Adaptive Controller", SDL_GAMEPAD_TYPE_XBOXONE },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_ONE_ELITE_SERIES_1, "Xbox One Elite Controller", SDL_GAMEPAD_TYPE_XBOXONE },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLE, "Xbox One Elite 2 Controller", SDL_GAMEPAD_TYPE_XBOXONE },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_ONE_S, "Xbox One S Controller", SDL_GAMEPAD_TYPE_XBOXONE },
        { USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_ONE_XBOXGIP_CONTROLLER, "Xbox One Controller", SDL_GAMEPAD_TYPE_XBOXONE },
        
        /* Official PlayStation controllers */
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS3, "PS3 Controller", SDL_GAMEPAD_TYPE_PS3 },
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS4, "PS4 Controller", SDL_GAMEPAD_TYPE_PS4 },
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS4_DONGLE, "PS4 Controller", SDL_GAMEPAD_TYPE_PS4 },
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS4_SLIM, "PS4 Controller", SDL_GAMEPAD_TYPE_PS4 },
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS4_STRIKEPAD, "PS4 Controller", SDL_GAMEPAD_TYPE_PS4 },
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS5, "DualSense Wireless Controller", SDL_GAMEPAD_TYPE_PS5 },
        { USB_VENDOR_SONY, USB_PRODUCT_SONY_DS5_EDGE, "DualSense Edge Wireless Controller", SDL_GAMEPAD_TYPE_PS5 },
        
        /* Official Nintendo controllers */
        /* TODO -- SNES, NES, Wii-U */
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_N64_CONTROLLER, "Nintendo N64 Controller", SDL_GAMEPAD_TYPE_N64 },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_GAMECUBE_ADAPTER, "Nintendo GameCube Adapter", SDL_GAMEPAD_TYPE_GAMECUBE },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_WII_REMOTE, "Wii Remote", SDL_GAMEPAD_TYPE_WII },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_WII_REMOTE2, "Wii Remote Plus", SDL_GAMEPAD_TYPE_WII },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_LEFT, "Joy-Con (L)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_RIGHT, "Joy-Con (R)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_PAIR, "Joy-Con (L/R)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_GRIP, "Joy-Con (L)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_GRIP, "Joy-Con L+R", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_GRIP, "Joy-Con (R)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_PRO, "Nintendo Switch Pro Controller", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SEGA_GENESIS_CONTROLLER, "Nintendo SEGA Genesis Controller", SDL_GAMEPAD_TYPE_SEGA_GENESIS },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_JOYCON_LEFT, "Joy-Con 2 (L)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_JOYCON_RIGHT, "Joy-Con 2 (R)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_JOYCON_PAIR, "Joy-Con 2 (L/R)", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_PRO, "Nintendo Switch 2 Pro Controller", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO },
        { USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER, "Nintendo GameCube Controller", SDL_GAMEPAD_TYPE_GAMECUBE },
        
        /* Misc. */
        { 0, 0, "Nintendo Wireless Gamepad", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO },
        { USB_VENDOR_8BITDO, USB_PRODUCT_8BITDO_SF30_PRO, "8BitDo SF30 Pro", SDL_GAMEPAD_TYPE_STANDARD },
        { USB_VENDOR_8BITDO, 0x2002, "8BitDo Ultimate Wired Controller for Xbox", SDL_GAMEPAD_TYPE_XBOXONE },
        { 0x28de, 0x1205, "Steam Deck", SDL_GAMEPAD_TYPE_STEAM },
    };

    for (int i = 0; i < (int)SDL_arraysize(vectors); ++i) {
        SDL_VirtualJoystickDesc desc;
        SDL_JoystickID device_id;
        SDL_GamepadType real_type, type;

        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.vendor_id = vectors[i].vendor;
        desc.product_id = vectors[i].product;
        desc.name = vectors[i].name;
        device_id = SDL_AttachVirtualJoystick(&desc);
        SDLTest_AssertCheck(device_id > 0, "SDL_AttachVirtualJoystick() -> %" SDL_PRIs32 " (expected > 0)", device_id);
        if (device_id == 0) {
            continue;
        }

        real_type = SDL_GetRealGamepadTypeForID(device_id);
        SDLTest_AssertCheck(real_type == vectors[i].expected_type,
            "SDL_GetRealGamepadTypeForID() for %04x:%04x \"%s\" -> %d (expected %d)",
            vectors[i].vendor, vectors[i].product, vectors[i].name, real_type, vectors[i].expected_type);

        type = SDL_GetGamepadTypeForID(device_id);
        SDLTest_AssertCheck(type == vectors[i].expected_type,
            "SDL_GetGamepadTypeForID() for %04x:%04x \"%s\" -> %d (expected %d)",
            vectors[i].vendor, vectors[i].product, vectors[i].name, type, vectors[i].expected_type);

        SDL_DetachVirtualJoystick(device_id);
    }

    return TEST_COMPLETED;
}

#define SINPUT_TEST_VERSION SINPUT_PACK_VERSION( \
    SINPUT_MISCSTYLE_NONE,        \
    SINPUT_TOUCHSTYLE_NONE,       \
    SINPUT_METASTYLE_BACKGUIDE,   \
    SINPUT_PADDLESTYLE_NONE,      \
    SINPUT_TRIGGERSTYLE_ANALOG,   \
    SINPUT_BUMPERSTYLE_TWO,       \
    SINPUT_ANALOGSTYLE_LEFTRIGHT)

#define USB_VENDOR_RASPBERRYPI_TEST                0x2e8a
#define USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST 0x10c6

/**
 * Check gamepad types and button labels for various guids.
 */
static int SDLCALL joystick_testGamepadMappingForCraftedGUID(void *arg)
{
    static const struct {
        const char *desc;
        Uint16 bus;
        Uint16 vendor;
        Uint16 product;
        Uint16 version;
        Uint8 driver_signature;
        Uint8 driver_data;
        SDL_GamepadType expected_type;
        int jbutton;
        SDL_GamepadButtonLabel expected_label;
    } vectors[] = {
        /* HIDAPI / Nintendo (see SDL_hidapi_nintendo.h) */
        { "GameCube adapter", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_GAMECUBE_ADAPTER, 0, 'h', 0, SDL_GAMEPAD_TYPE_GAMECUBE, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "GameCube adapter", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_GAMECUBE_ADAPTER, 0, 'h', 0, SDL_GAMEPAD_TYPE_GAMECUBE, 2, SDL_GAMEPAD_BUTTON_LABEL_X },
        { "GameCube adapter", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_GAMECUBE_ADAPTER, 0, 'h', 0, SDL_GAMEPAD_TYPE_GAMECUBE, 1, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "Switch 2 GameCube", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER, 0, 'h', 0, SDL_GAMEPAD_TYPE_GAMECUBE, 1, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "Switch 2 Pro", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_PRO, 0, 'h', 0, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 0, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "Switch 2 Joy-Con pair", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH2_JOYCON_PAIR, 0, 'h', 0, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR, 0, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "NSO N64", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_N64_CONTROLLER, 0, 'h', 12 /* N64 */, SDL_GAMEPAD_TYPE_N64, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "NSO N64", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_N64_CONTROLLER, 0, 'h', 12 /* N64 */, SDL_GAMEPAD_TYPE_N64, 1, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "NSO SEGA Genesis", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SEGA_GENESIS_CONTROLLER, 0, 'h', 13 /* Genesis */, SDL_GAMEPAD_TYPE_SEGA_GENESIS, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "Joy-Con (L) mini gamepad mode", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_JOYCON_LEFT, 0, 'h', 1 /* JoyConLeft */, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT, 0, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "Wii Remote", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_WII_REMOTE, 0, 'h', 128 /* WiiExtension None */, SDL_GAMEPAD_TYPE_WII, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "Switch Pro", 0x03, USB_VENDOR_NINTENDO, USB_PRODUCT_NINTENDO_SWITCH_PRO, 0, 'h', 3 /* ProController */, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 0, SDL_GAMEPAD_BUTTON_LABEL_B },
        
        /* HIDAPI / Misc. */
        { "8BitDo SF30 Pro (USB)", 0x03, USB_VENDOR_8BITDO, USB_PRODUCT_8BITDO_SF30_PRO, 0, 'h', 0, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 1, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "8BitDo SF30 Pro (USB)", 0x03, USB_VENDOR_8BITDO, USB_PRODUCT_8BITDO_SF30_PRO, 0, 'h', 0, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 3, SDL_GAMEPAD_BUTTON_LABEL_Y },
        
        /* HIDAPI / Valve */
        { "Steam Deck", 0x03, 0x28de, 0x1205, 0, 'h', 0, SDL_GAMEPAD_TYPE_STEAM, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        
        /* HIDAPI / SInput */
        { "SInput abxy face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 1 << 5, SDL_GAMEPAD_TYPE_STANDARD, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "SInput abxy face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 1 << 5, SDL_GAMEPAD_TYPE_STANDARD, 1, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "SInput abxy face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 1 << 5, SDL_GAMEPAD_TYPE_STANDARD, 2, SDL_GAMEPAD_BUTTON_LABEL_X },
        { "SInput abxy face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 1 << 5, SDL_GAMEPAD_TYPE_STANDARD, 3, SDL_GAMEPAD_BUTTON_LABEL_Y },
        { "SInput axby face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 2 << 5, SDL_GAMEPAD_TYPE_GAMECUBE, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "SInput axby face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 2 << 5, SDL_GAMEPAD_TYPE_GAMECUBE, 1, SDL_GAMEPAD_BUTTON_LABEL_X },
        { "SInput axby face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 2 << 5, SDL_GAMEPAD_TYPE_GAMECUBE, 2, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "SInput axby face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 2 << 5, SDL_GAMEPAD_TYPE_GAMECUBE, 3, SDL_GAMEPAD_BUTTON_LABEL_Y },
        { "SInput bayx face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 3 << 5, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 0, SDL_GAMEPAD_BUTTON_LABEL_B },
        { "SInput bayx face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 3 << 5, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 1, SDL_GAMEPAD_BUTTON_LABEL_A },
        { "SInput bayx face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 3 << 5, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 2, SDL_GAMEPAD_BUTTON_LABEL_Y },
        { "SInput bayx face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 3 << 5, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, 3, SDL_GAMEPAD_BUTTON_LABEL_X },
        { "SInput sony face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 4 << 5, SDL_GAMEPAD_TYPE_PS4, 0, SDL_GAMEPAD_BUTTON_LABEL_CROSS },
        { "SInput sony face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 4 << 5, SDL_GAMEPAD_TYPE_PS4, 1, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE },
        { "SInput sony face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 4 << 5, SDL_GAMEPAD_TYPE_PS4, 2, SDL_GAMEPAD_BUTTON_LABEL_SQUARE },
        { "SInput sony face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 4 << 5, SDL_GAMEPAD_TYPE_PS4, 3, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE },
        { "SInput dflt face style", 0x03, USB_VENDOR_RASPBERRYPI_TEST, USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC_TEST, SINPUT_TEST_VERSION, 'h', 0 << 5, SDL_GAMEPAD_TYPE_STANDARD, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        
        /* RawInput */
        { "RawInput gamepad", 0x03, USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_SERIES_X, 0, 'r', 0, SDL_GAMEPAD_TYPE_XBOXONE, 0, SDL_GAMEPAD_BUTTON_LABEL_A },
        
        /* WGI */
        { "WGI gamepad", 0x03, USB_VENDOR_MICROSOFT, USB_PRODUCT_XBOX_SERIES_X, 0, 'w', SDL_JOYSTICK_TYPE_GAMEPAD, SDL_GAMEPAD_TYPE_XBOXONE, 1, SDL_GAMEPAD_BUTTON_LABEL_B },
    };
    
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, "0");

    for (int i = 0; i < (int)SDL_arraysize(vectors); ++i) {
        /* Make a guid for the given test-case gamepad. (Carbon copy of what SDL_CreateJoystickGUID()
         * produces for a nonzero vendor, modulo some fields we don't care about.) */
        SDL_GUID guid;
        Uint16 *guid16 = (Uint16 *)guid.data;
        SDL_JoystickID device_id = 0;
        SDL_Gamepad *gamepad;

        SDL_zero(guid);
        /* Byteswap so devices get the same GUID on little/big endian platforms */
        *guid16++ = SDL_Swap16LE(vectors[i].bus);
        *guid16++ = 0; /* (the name CRC would go here) */
        *guid16++ = SDL_Swap16LE(vectors[i].vendor);
        *guid16++ = 0;
        *guid16++ = SDL_Swap16LE(vectors[i].product);
        *guid16++ = 0;
        *guid16++ = SDL_Swap16LE(vectors[i].version);
        guid.data[14] = vectors[i].driver_signature;
        guid.data[15] = vectors[i].driver_data;

        /* ...get its mapping... */
        char *mapping = SDL_GetGamepadMappingForGUID(guid);
        SDLTest_AssertCheck(mapping != NULL, "SDL_GetGamepadMappingForGUID() for %s", vectors[i].desc);
        if (!mapping) {
            continue;
        }

        /* ...make a fake gamepad with this mapping... */
        gamepad = OpenVirtualGamepadWithMapping(vectors[i].vendor, vectors[i].product, mapping, &device_id);
        SDLTest_AssertCheck(gamepad != NULL, "Open a virtual gamepad with the mapping for %s", vectors[i].desc);
        if (gamepad) {
            SDL_GamepadType type = SDL_GetGamepadType(gamepad);
            SDL_GamepadButtonLabel label = SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
            bool found;

            /* ...does it end up with the right gamepad-type? */
            SDLTest_AssertCheck(type == vectors[i].expected_type,
                "SDL_GetGamepadType() for %s -> %d (expected %d)",
                vectors[i].desc, type, vectors[i].expected_type);

            found = GetLabelForJoystickButton(gamepad, vectors[i].jbutton, &label);
            
            /* ...is there a gamepad button for this joystick button at all? */
            SDLTest_AssertCheck(found, "Joystick button %d of %s is bound to a gamepad button", vectors[i].jbutton, vectors[i].desc);
            if (found) {
                /* ...and is it bound to the expected gamepad button? */
                SDLTest_AssertCheck(
                    label == vectors[i].expected_label,
                    "Label for joystick button %d of %s -> %d (expected %d)",
                    vectors[i].jbutton, vectors[i].desc, label, vectors[i].expected_label);
            }
        }
        CloseVirtualGamepad(gamepad, device_id);
        SDL_free(mapping);
    }

    SDL_ResetHint(SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Joystick routine test cases */
static const SDLTest_TestCaseReference joystickTest1 = {
    joystick_testVirtual, "joystick_testVirtual", "Test virtual joystick functionality", TEST_ENABLED
};
static const SDLTest_TestCaseReference joystickTest2 = {
    joystick_testMappings, "joystick_testMappings", "Test gamepad mapping functionality", TEST_ENABLED
};
static const SDLTest_TestCaseReference joystickTest3 = {
    joystick_testGamepadTypeForVIDPID, "joystick_testGamepadTypeForVIDPID", "Test gamepad type detection from vendor/product/name", TEST_ENABLED
};
static const SDLTest_TestCaseReference joystickTest4 = {
    joystick_testGamepadMappingForCraftedGUID, "joystick_testGamepadMappingForCraftedGUID", "Test gamepad types and button labels in generated driver mappings", TEST_ENABLED
};

/* Sequence of Joystick routine test cases */
static const SDLTest_TestCaseReference *joystickTests[] = {
    &joystickTest1,
    &joystickTest2,
    &joystickTest3,
    &joystickTest4,
    NULL
};

/* Joystick routine test suite (global) */
SDLTest_TestSuiteReference joystickTestSuite = {
    "Joystick",
    joystickSetUp,
    joystickTests,
    joystickTearDown
};
