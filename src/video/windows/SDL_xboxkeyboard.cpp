/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_WINDOWS && (defined(__XBOXONE__) || defined(__XBOXSERIES__))

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <XAsync.h>
#include <XGameUI.h>
#include <XGameRuntime.h>

extern "C" {
#include "../../events/SDL_keyboard_c.h"
#include "SDL_windowsvideo.h"
}

/* Max length passed to XGameUiShowTextEntryAsync */
#define SDL_XBOX_VIRTUAL_KEYBOARD_MAX_TEXT_LENGTH 1024


SDL_bool
WIN_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return SDL_TRUE;
}

void
WIN_StartTextInput(SDL_VideoDevice *_this)
{
    XAsyncBlock* asyncBlock = new XAsyncBlock;
    asyncBlock->context = nullptr;
    asyncBlock->queue = NULL;
    asyncBlock->callback = [](XAsyncBlock* async)
    {
        async->context;
        uint32_t textBufSize;
        HRESULT hr = XGameUiShowTextEntryResultSize(async, &textBufSize);

        if (FAILED(hr))
        {
            SDL_Log("XGameUiShowTextEntryResultSize failed: 0x%08X", hr);
            return;
        }

        if (textBufSize == 0)
        {
            return;
        }

        char* textBuf = new char[textBufSize + 1];
        if (textBuf == nullptr)
        {
            SDL_Log("Allocating text buffer with size: XGameUiShowTextEntryResultSize(%ul) failed!", textBufSize);
            return;
        }

        hr = XGameUiShowTextEntryResult(async, textBufSize, textBuf, nullptr);

        if (FAILED(hr))
        {
            SDL_Log("XGameUiShowTextEntryResult failed: 0x%08X", hr);
            delete[] textBuf;
            return;
        }

        SDL_SendKeyboardText(textBuf);

        // Use the text buffer
       delete[] textBuf;
    };

    // This can be further improved, title, description, InputScope can be exposed for the user to set.
    HRESULT hr = XGameUiShowTextEntryAsync(
        asyncBlock,
        "Enter text",
        "",
        "",
        XGameUiTextEntryInputScope::Default,
        SDL_XBOX_VIRTUAL_KEYBOARD_MAX_TEXT_LENGTH
    );
}

void
WIN_StopTextInput(SDL_VideoDevice *_this)
{
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
