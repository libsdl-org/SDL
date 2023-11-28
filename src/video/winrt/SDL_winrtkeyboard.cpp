/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_WINRT

/* Windows-specific includes */
#include <Windows.h>
#include <agile.h>

/* SDL-specific includes */
#include "SDL_winrtevents_c.h"

extern "C" {
#include "../../events/scancodes_windows.h"
#include "../../events/SDL_keyboard_c.h"
}

static SDL_Scancode WINRT_TranslateKeycode(Windows::System::VirtualKey virtualKey, const Windows::UI::Core::CorePhysicalKeyStatus& keyStatus)
{
    SDL_Scancode code;
    Uint8 index;
    Uint16 scanCode = keyStatus.ScanCode | (keyStatus.IsExtendedKey ? 0xe000 : 0);

    /* Pause/Break key have a special scan code with 0xe1 prefix
     * that is not properly reported under UWP.
     * Use Pause scan code that is used in Win32. */
    if (virtualKey == Windows::System::VirtualKey::Pause) {
        scanCode = 0xe046;
    }

    /* Pack scan code into one byte to make the index. */
    index = LOBYTE(scanCode) | (HIBYTE(scanCode) ? 0x80 : 0x00);
    code = windows_scancode_table[index];

    return code;
}

void WINRT_ProcessAcceleratorKeyActivated(Windows::UI::Core::AcceleratorKeyEventArgs ^ args)
{
    using namespace Windows::UI::Core;

    Uint8 state;
    SDL_Scancode code;

    switch (args->EventType) {
    case CoreAcceleratorKeyEventType::SystemKeyDown:
    case CoreAcceleratorKeyEventType::KeyDown:
        state = SDL_PRESSED;
        break;
    case CoreAcceleratorKeyEventType::SystemKeyUp:
    case CoreAcceleratorKeyEventType::KeyUp:
        state = SDL_RELEASED;
        break;
    default:
        return;
    }

    code = WINRT_TranslateKeycode(args->VirtualKey, args->KeyStatus);
    SDL_SendKeyboardKey(0, state, code);
}

void WINRT_ProcessCharacterReceivedEvent(Windows::UI::Core::CharacterReceivedEventArgs ^ args)
{
    wchar_t src_ucs2[2];
    char dest_utf8[16];
    int result;

    /* Setup src */
    src_ucs2[0] = args->KeyCode;
    src_ucs2[1] = L'\0';

    /* Convert the text, then send an SDL_EVENT_TEXT_INPUT event. */
    result = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)&src_ucs2, -1, (LPSTR)dest_utf8, sizeof(dest_utf8), NULL, NULL);
    if (result > 0) {
        SDL_SendKeyboardText(dest_utf8);
    }
}

#if NTDDI_VERSION >= NTDDI_WIN10

static bool WINRT_InputPaneVisible = false;

void WINTRT_OnInputPaneShowing(Windows::UI::ViewManagement::InputPane ^ sender, Windows::UI::ViewManagement::InputPaneVisibilityEventArgs ^ args)
{
    WINRT_InputPaneVisible = true;
}

void WINTRT_OnInputPaneHiding(Windows::UI::ViewManagement::InputPane ^ sender, Windows::UI::ViewManagement::InputPaneVisibilityEventArgs ^ args)
{
    WINRT_InputPaneVisible = false;
}

void WINTRT_InitialiseInputPaneEvents(SDL_VideoDevice *_this)
{
    using namespace Windows::UI::ViewManagement;
    InputPane ^ inputPane = InputPane::GetForCurrentView();
    if (inputPane) {
        inputPane->Showing += ref new Windows::Foundation::TypedEventHandler<Windows::UI::ViewManagement::InputPane ^,
                                                                             Windows::UI::ViewManagement::InputPaneVisibilityEventArgs ^>(&WINTRT_OnInputPaneShowing);
        inputPane->Hiding += ref new Windows::Foundation::TypedEventHandler<Windows::UI::ViewManagement::InputPane ^,
                                                                            Windows::UI::ViewManagement::InputPaneVisibilityEventArgs ^>(&WINTRT_OnInputPaneHiding);
    }
}

SDL_bool WINRT_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return SDL_TRUE;
}

void WINRT_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    using namespace Windows::UI::ViewManagement;
    InputPane ^ inputPane = InputPane::GetForCurrentView();
    if (inputPane) {
        inputPane->TryShow();
    }
}

void WINRT_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    using namespace Windows::UI::ViewManagement;
    InputPane ^ inputPane = InputPane::GetForCurrentView();
    if (inputPane) {
        inputPane->TryHide();
    }
}

SDL_bool WINRT_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    using namespace Windows::UI::ViewManagement;
    InputPane ^ inputPane = InputPane::GetForCurrentView();
    if (inputPane) {
        switch (SDL_WinRTGetDeviceFamily()) {
        case SDL_WINRT_DEVICEFAMILY_XBOX:
            // Documentation recommends using inputPane->Visible
            // https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.inputpane.visible?view=winrt-22621
            // This does not seem to work on latest UWP/Xbox.
            // Workaround: Listen to Showing/Hiding events
            if (WINRT_InputPaneVisible) {
                return SDL_TRUE;
            }
            break;
        default:
            // OccludedRect is recommend on universal apps per docs
            // https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.inputpane.visible?view=winrt-22621
            Windows::Foundation::Rect rect = inputPane->OccludedRect;
            if (rect.Width > 0 && rect.Height > 0) {
                return SDL_TRUE;
            }
            break;
        }
    }
    return SDL_FALSE;
}

#endif // NTDDI_VERSION >= ...

#endif // SDL_VIDEO_DRIVER_WINRT
