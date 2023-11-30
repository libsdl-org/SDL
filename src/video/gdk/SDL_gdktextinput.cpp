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
/*
  Screen keyboard and text input backend
  for GDK platforms.
*/
#include "SDL_internal.h"
#include "SDL_gdktextinput.h"

#ifdef SDL_GDK_TEXTINPUT

/* GDK headers are weird here */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <XGameUI.h>
#include <XUser.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../../events/SDL_keyboard_c.h"
#include "../windows/SDL_windowsvideo.h"

/* TODO: Have a separate task queue for text input perhaps? */
static XTaskQueueHandle g_TextTaskQueue = NULL;
/* Global because there can be only one text entry shown at once. */
static XAsyncBlock *g_TextBlock = NULL;

/* Creation parameters */
static SDL_bool g_DidRegisterHints = SDL_FALSE;
static char *g_TitleText = NULL;
static char *g_DescriptionText = NULL;
static char *g_DefaultText = NULL;
static const Sint32 g_DefaultTextInputScope = (Sint32)XGameUiTextEntryInputScope::Default;
static Sint32 g_TextInputScope = g_DefaultTextInputScope;
static const Sint32 g_DefaultMaxTextLength = 1024; /* as per doc: maximum allowed amount on consoles */
static Sint32 g_MaxTextLength = g_DefaultMaxTextLength;

static void SDLCALL GDK_InternalHintCallback(
    void *userdata,
    const char *name,
    const char *oldValue,
    const char *newValue)
{
    if (!userdata) {
        return;
    }

    /* oldValue is ignored because we store it ourselves. */
    /* name is ignored because we deduce it from userdata */

    if (userdata == &g_TextInputScope || userdata == &g_MaxTextLength) {
        /* int32 hint */
        Sint32 intValue = (!newValue || newValue[0] == '\0') ? 0 : SDL_atoi(newValue);
        if (userdata == &g_MaxTextLength && intValue <= 0) {
            intValue = g_DefaultMaxTextLength;
        } else if (userdata == &g_TextInputScope && intValue < 0) {
            intValue = g_DefaultTextInputScope;
        }

        *(Sint32 *)userdata = intValue;
    } else {
        /* string hint */
        if (!newValue || newValue[0] == '\0') {
            /* treat empty or NULL strings as just NULL for this impl */
            SDL_free(*(char **)userdata);
            *(char **)userdata = NULL;
        } else {
            char *newString = SDL_strdup(newValue);
            if (newString) {
                /* free previous value and write the new one */
                SDL_free(*(char **)userdata);
                *(char **)userdata = newString;
            }
        }
    }
}

static int GDK_InternalEnsureTaskQueue(void)
{
    if (!g_TextTaskQueue) {
        if (SDL_GDKGetTaskQueue(&g_TextTaskQueue) < 0) {
            /* SetError will be done for us. */
            return -1;
        }
    }

    return 0;
}

static void CALLBACK GDK_InternalTextEntryCallback(XAsyncBlock *asyncBlock)
{
    HRESULT hR = S_OK;
    Uint32 resultSize = 0;
    Uint32 resultUsed = 0;
    char *resultBuffer = NULL;

    /* The keyboard will be already hidden when we reach this code */

    if (FAILED(hR = XGameUiShowTextEntryResultSize(
                   asyncBlock,
                   &resultSize))) {
        SDL_SetError("XGameUiShowTextEntryResultSize failure with HRESULT of %08X", hR);
    } else if (resultSize > 0) {
        /* +1 to be super sure that the buffer will be null terminated */
        resultBuffer = (char *)SDL_calloc(sizeof(*resultBuffer), 1 + (size_t)resultSize);
        if (resultBuffer) {
            /* still pass the original size that we got from ResultSize */
            if (FAILED(hR = XGameUiShowTextEntryResult(
                           asyncBlock,
                           resultSize,
                           resultBuffer,
                           &resultUsed))) {
                SDL_SetError("XGameUiShowTextEntryResult failure with HRESULT of %08X", hR);
            }
            /* check that we have some text and that we weren't cancelled */
            else if (resultUsed > 0 && resultBuffer[0] != '\0') {
                /* it's null terminated so it's fine */
                SDL_SendKeyboardText(resultBuffer);
            }
            /* we're done with the buffer */
            SDL_free(resultBuffer);
            resultBuffer = NULL;
        }
    }

    /* free the async block after we're done */
    SDL_free(asyncBlock);
    asyncBlock = NULL;
    g_TextBlock = NULL; /* once we do this we're fully done with the keyboard */
}

void GDK_EnsureHints(void)
{
    if (g_DidRegisterHints == SDL_FALSE) {
        SDL_AddHintCallback(
            SDL_HINT_GDK_TEXTINPUT_TITLE,
            GDK_InternalHintCallback,
            &g_TitleText);
        SDL_AddHintCallback(
            SDL_HINT_GDK_TEXTINPUT_DESCRIPTION,
            GDK_InternalHintCallback,
            &g_DescriptionText);
        SDL_AddHintCallback(
            SDL_HINT_GDK_TEXTINPUT_DEFAULT,
            GDK_InternalHintCallback,
            &g_DefaultText);
        SDL_AddHintCallback(
            SDL_HINT_GDK_TEXTINPUT_SCOPE,
            GDK_InternalHintCallback,
            &g_TextInputScope);
        SDL_AddHintCallback(
            SDL_HINT_GDK_TEXTINPUT_MAX_LENGTH,
            GDK_InternalHintCallback,
            &g_MaxTextLength);
        g_DidRegisterHints = SDL_TRUE;
    }
}

void GDK_StartTextInput(SDL_VideoDevice *_this)
{
    /*
     * Currently a stub, since all input is handled by the virtual keyboard,
     * but perhaps when implementing XGameUiTextEntryOpen in the future
     * you will need this.
     *
     * Also XGameUiTextEntryOpen docs say that it is
     * "not implemented on desktop" so... no thanks.
     *
     * Right now this function isn't implemented on Desktop
     * and seems to be present only in the docs? So I didn't bother.
     */
}

void GDK_StopTextInput(SDL_VideoDevice *_this)
{
    /* See notice in GDK_StartTextInput */
}

int GDK_SetTextInputRect(SDL_VideoDevice *_this, const SDL_Rect *rect)
{
    /*
     * XGameUiShowTextEntryAsync does not allow you to set
     * the position of the virtual keyboard window.
     *
     * However, XGameUiTextEntryOpen seems to allow that,
     * but again, see notice in GDK_StartTextInput.
     *
     * Right now it's a stub which may be useful later.
     */
    return 0;
}

void GDK_ClearComposition(SDL_VideoDevice *_this)
{
    /* See notice in GDK_StartTextInput */
}

SDL_bool GDK_IsTextInputShown(SDL_VideoDevice *_this)
{
    /*
     * The XGameUiShowTextEntryAsync window
     * does specify potential input candidates
     * just below the text box, so technically
     * this is true whenever the window is shown.
     */
    return (g_TextBlock != NULL);
}

SDL_bool GDK_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    /* Currently always true for this input method */
    return SDL_TRUE;
}

void GDK_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    /*
     * There is XGameUiTextEntryOpen but it's only in online docs,
     * My October Update 1 GDKX installation does not have this function defined
     * and as such I decided not to use it at all, since some folks might use even older GDKs.
     *
     * That means the only text input option for us is a simple virtual keyboard widget.
     */

    HRESULT hR = S_OK;

    if (g_TextBlock) {
        /* already showing the keyboard */
        return;
    }

    if (GDK_InternalEnsureTaskQueue() < 0) {
        /* unable to obtain the SDL GDK queue */
        return;
    }

    g_TextBlock = (XAsyncBlock *)SDL_calloc(1, sizeof(*g_TextBlock));
    if (!g_TextBlock) {
        return;
    }

    g_TextBlock->queue = g_TextTaskQueue;
    g_TextBlock->context = _this;
    g_TextBlock->callback = GDK_InternalTextEntryCallback;
    if (FAILED(hR = XGameUiShowTextEntryAsync(
                   g_TextBlock,
                   g_TitleText,
                   g_DescriptionText,
                   g_DefaultText,
                   (XGameUiTextEntryInputScope)g_TextInputScope,
                   (uint32_t)g_MaxTextLength))) {
        SDL_free(g_TextBlock);
        g_TextBlock = NULL;
        SDL_SetError("XGameUiShowTextEntryAsync failure with HRESULT of %08X", hR);
    }
}

void GDK_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (g_TextBlock) {
        XAsyncCancel(g_TextBlock);
        /* the completion callback will free the block */
    }
}

SDL_bool GDK_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* See notice in GDK_IsTextInputShown */
    return GDK_IsTextInputShown(_this);
}

#ifdef __cplusplus
}
#endif

#endif
