/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_PS3

#include <io/kb.h>

#include "SDL_PS3keyboard_c.h"
#include "SDL_PS3video.h"

static void unicodeToUtf8(Uint16 w, char *utf8buf)
{
    unsigned char *utf8s = (unsigned char *) utf8buf;

    if ( w < 0x0080 ) {
        utf8s[0] = ( unsigned char ) w;
        utf8s[1] = 0;
    }
    else if ( w < 0x0800 ) {
        utf8s[0] = 0xc0 | (( w ) >> 6 );
        utf8s[1] = 0x80 | (( w ) & 0x3f );
        utf8s[2] = 0;
    }
    else {
        utf8s[0] = 0xe0 | (( w ) >> 12 );
        utf8s[1] = 0x80 | (( ( w ) >> 6 ) & 0x3f );
        utf8s[2] = 0x80 | (( w ) & 0x3f );
        utf8s[3] = 0;
    }
}

static void updateKeymap(SDL_VideoDevice *_this)
{
    SDL_VideoData *data =
        (SDL_VideoData *) _this->internal;

    SDL_Scancode scancode;
    SDL_Keycode keymap[SDL_SCANCODE_COUNT];
    KbConfig kbConfig;
    KbMkey kbMkey;
    KbLed kbLed;
    Uint16 unicode;

    // TODO: fix me
    // SDL_GetDefaultKeymap(keymap);

    ioKbGetConfiguration(0, &kbConfig);

    data->_keyboardMapping = kbConfig.mapping;

    kbMkey._KbMkeyU.mkeys = 0;
    kbLed._KbLedU.leds = 1; // Num lock

    // Update SDL keycodes according to the keymap
    for (scancode = 0; scancode < SDL_SCANCODE_COUNT; ++scancode) {

        // Make sure this scancode is a valid character scancode
        if (scancode == SDL_SCANCODE_UNKNOWN ||
            scancode == SDL_SCANCODE_ESCAPE ||
            scancode == SDL_SCANCODE_RETURN ||
            (keymap[scancode] & SDLK_SCANCODE_MASK)) {
            continue;
        }

        unicode = ioKbCnvRawCode(data->_keyboardMapping, kbMkey, kbLed, scancode);

        // Ignore Keypad flag
        unicode &= ~KB_KEYPAD;

        // Exclude raw keys
        if (unicode != 0 && unicode < KB_RAWDAT) {
            keymap[scancode] = unicode;
        }
    }
    // TODO: fix me
    // SDL_SetKeymap(keymap, SDL_NUM_SCANCODES);
}

static void checkKeyboardConnected(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->internal;

    KbInfo kbInfo;
    ioKbGetInfo(&kbInfo);

    if (kbInfo.status[0] == 1 && !data->_keyboardConnected) // Connected
    {
        data->_keyboardConnected = true;

        // Old events in the queue are discarded
        ioKbClearBuf(0);

        // Set raw keyboard code types to get scan codes
        ioKbSetCodeType(0, KB_CODETYPE_RAW);
        ioKbSetReadMode(0, KB_RMODE_INPUTCHAR);

        updateKeymap(_this);
    }
    else if (kbInfo.status[0] != 1 && data->_keyboardConnected) // Disconnected
    {
        data->_keyboardConnected = false;

        SDL_ResetKeyboard();
    }
}

static void updateModifierKey(bool oldState, bool newState, SDL_Scancode scancode)
{
    if (!oldState ^ !newState) {
        // SDL_SendKeyboardKey(newState ? true : false, scancode);
        // SDL_SendKeyboardKey(0, SDL_DEFAULT_KEYBOARD_ID, keycode, TranslateKeycode(keycode), true);
    }
}

static void updateModifiers(SDL_VideoDevice *_this, const KbData *Keys)
{
    SDL_Keymod modstate = SDL_GetModState();

    updateModifierKey(modstate & SDL_KMOD_LSHIFT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_shift, SDL_SCANCODE_LSHIFT);
    updateModifierKey(modstate & SDL_KMOD_RSHIFT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_shift, SDL_SCANCODE_RSHIFT);
    updateModifierKey(modstate & SDL_KMOD_LCTRL, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_ctrl, SDL_SCANCODE_LCTRL);
    updateModifierKey(modstate & SDL_KMOD_RCTRL, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_ctrl, SDL_SCANCODE_RCTRL);
    updateModifierKey(modstate & SDL_KMOD_LALT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_alt, SDL_SCANCODE_LALT);
    updateModifierKey(modstate & SDL_KMOD_RALT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_alt, SDL_SCANCODE_RALT);
    updateModifierKey(modstate & SDL_KMOD_LGUI, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_win, SDL_SCANCODE_LGUI);
    updateModifierKey(modstate & SDL_KMOD_RGUI, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_win, SDL_SCANCODE_RGUI);
}

static void updateKeys(SDL_VideoDevice *_this, const KbData *Keys)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->internal;

    int x = 0;
    int numKeys = 0;
    Uint8 newkeystate[SDL_SCANCODE_COUNT];
    const bool * keystate = SDL_GetKeyboardState(&numKeys);
    Uint16 unicode;
    SDL_Scancode scancode;

    for (scancode = 0; scancode < SDL_SCANCODE_COUNT; ++scancode) {
        newkeystate[scancode] = false; // SDL_RELEASED
    }

    // for (x = 0; x < Keys->led; x++) {
    for (x = 0; x < MAX_KEYCODES; x++) {
        if (Keys->keycode[0] != 0)
            newkeystate[Keys->keycode[x]] = true;
    }

    for (scancode = 0; scancode < SDL_SCANCODE_COUNT; ++scancode) {
        if ((newkeystate[scancode] != keystate[scancode])
                && (scancode < SDL_SCANCODE_LCTRL || scancode > SDL_SCANCODE_RGUI)) {

            // TODO: fix me
            // Send new key state
            // SDL_SendKeyboardKey(newkeystate[scancode], scancode);
            // SDL_SendKeyboardKey(newkeystate[scancode], SDL_DEFAULT_KEYBOARD_ID, scancode, TranslateKeycode(scancode), true);

            // Send the text corresponding to the keypress
            if (newkeystate[scancode] == true) {
                // Convert scancode
                unicode = ioKbCnvRawCode(data->_keyboardMapping, Keys->mkey, Keys->led, scancode);

                // Ignore Keypad flag
                unicode &= ~KB_RAWDAT;

                // Exclude raw keys
                if (unicode != 0 && unicode < KB_RAWDAT) {
                    char utf8[SDL_TEXTINPUT_TYPE_TEXT_NAME];

                    // Convert from Unicode to UTF-8
                    unicodeToUtf8(unicode, utf8);
                    // TODO: replace with correct function
                    // SDL_SendKeyboardText(utf8);
                }
            }
        }
    }
}

void PS3_PumpKeyboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->internal;

    checkKeyboardConnected(_this);

    if (data->_keyboardConnected) {
        // KbData Keys;

        // TODO: fix me.
        // Read data from the keyboard buffer
        // if (ioKbRead(0, &Keys) == 0 && Keys.led > 0) {
        //     updateModifiers(_this, &Keys);
        //     updateKeys(_this, &Keys);
        // }
    }
}

void PS3_InitKeyboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->internal;

    // Init the PS3 Keyboard
    ioKbInit(1);

    data->_keyboardConnected = false;
}

void PS3_QuitKeyboard(SDL_VideoDevice *_this)
{
    ioKbEnd();
}

#endif // SDL_VIDEO_DRIVER_PS3

/* vi: set ts=4 sw=4 expandtab: */
