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

#ifdef SDL_VIDEO_DRIVER_X11

#include "SDL_x11messagebox.h"
#include "SDL_x11toolkit.h"

#ifndef SDL_FORK_MESSAGEBOX
#define SDL_FORK_MESSAGEBOX 1
#endif

#if SDL_FORK_MESSAGEBOX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

typedef struct SDL_MessageBoxCallbackDataX11
{
    int *buttonID;
    SDL_ToolkitWindowX11 *window;
} SDL_MessageBoxCallbackDataX11;

static void X11_MessageBoxButtonCallback(SDL_ToolkitControlX11 *control, void *data)
{
    SDL_MessageBoxCallbackDataX11 *cbdata;
    
    cbdata = (SDL_MessageBoxCallbackDataX11 *)data;
    *cbdata->buttonID = X11Toolkit_GetButtonControlData(control)->buttonID;
    X11Toolkit_SignalWindowClose(cbdata->window);
}

static bool X11_ShowMessageBoxImpl(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    SDL_ToolkitWindowX11 *window;
    SDL_ToolkitControlX11 *icon;
    SDL_ToolkitControlX11 fake_icon;
    SDL_ToolkitControlX11 *message;
    SDL_ToolkitControlX11 **buttons;
    const SDL_MessageBoxColor *colorhints;
    SDL_MessageBoxCallbackDataX11 data;
    int max_button_w;
    int max_button_h;
    int total_button_w;
    int total_text_and_icon_w;
    int w;
    int h;
    int i;
    int t;
    
    /* Init vars */
    max_button_w = 50;
    max_button_h = 0;
    w = h = 2;
    i = t = total_button_w = total_text_and_icon_w = 0;
    
    /* Color scheme */
    if (messageboxdata->colorScheme) {
        colorhints = messageboxdata->colorScheme->colors;
    } else {
        colorhints = NULL;
    }
    
    /* Create window */
    window = X11Toolkit_CreateWindowStruct(messageboxdata->window, colorhints);
    if (!window) {
        return false;
    }
    
    /* Create controls */
    buttons = SDL_calloc(messageboxdata->numbuttons, sizeof(SDL_ToolkitControlX11 *));
    icon = X11Toolkit_CreateIconControl(window, messageboxdata->flags);
    message = X11Toolkit_CreateLabelControl(window, (char *)messageboxdata->message);
    data.buttonID = buttonID;
    data.window = window;
    for (i = 0; i < messageboxdata->numbuttons; i++) {
        buttons[i] = X11Toolkit_CreateButtonControl(window, &messageboxdata->buttons[i]);
        X11Toolkit_RegisterCallbackForButtonControl(buttons[i], &data, X11_MessageBoxButtonCallback);
        max_button_w = SDL_max(max_button_w, buttons[i]->rect.w);
        max_button_h = SDL_max(max_button_h, buttons[i]->rect.h);
        buttons[i]->rect.x = 0;
    }
    if (icon) {
        icon->rect.x = icon->rect.y = 0;
    }
        
    /* Positioning and sizing */
    if (icon) {
        message->rect.x = SDL_TOOLKIT_X11_ELEMENT_PADDING_2 + icon->rect.x + icon->rect.w;
        message->rect.y = X11Toolkit_GetIconControlCharY(icon);
    } else {
        message->rect.x = 0;
        message->rect.y = -2;
        icon = &fake_icon;
        icon->rect.w = 0;
        icon->rect.h = 0;
        icon->rect.x = 0;
        icon->rect.y = 0;    
    }
    if (messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
        for (i = messageboxdata->numbuttons; i != -1; i--) {
            buttons[i]->rect.w = max_button_w;
            buttons[i]->rect.h = max_button_h;
            X11Toolkit_NotifyUpsize(buttons[i]);    
            
            if (icon->rect.h > message->rect.h) {
                buttons[i]->rect.y = icon->rect.h + SDL_TOOLKIT_X11_ELEMENT_PADDING_2;
            } else {
                buttons[i]->rect.y = message->rect.h + SDL_TOOLKIT_X11_ELEMENT_PADDING_2;                
            }        
            
            if (i) {
                buttons[i]->rect.x = buttons[i-1]->rect.x + buttons[i-1]->rect.w + SDL_TOOLKIT_X11_ELEMENT_PADDING_3;
            }
        }        
    } else {
        for (i = 0; i < messageboxdata->numbuttons; i++) {
            buttons[i]->rect.w = max_button_w;
            buttons[i]->rect.h = max_button_h;
            X11Toolkit_NotifyUpsize(buttons[i]);    
            
            if (icon->rect.h > message->rect.h) {
                buttons[i]->rect.y = icon->rect.h + SDL_TOOLKIT_X11_ELEMENT_PADDING_2;
            } else {
                buttons[i]->rect.y = message->rect.h + SDL_TOOLKIT_X11_ELEMENT_PADDING_2;                
            }        
            
            if (i) {
                buttons[i]->rect.x = buttons[i-1]->rect.x + buttons[i-1]->rect.w + SDL_TOOLKIT_X11_ELEMENT_PADDING_3;
            }
        }        
    }
    total_button_w = buttons[messageboxdata->numbuttons-1]->rect.x + buttons[messageboxdata->numbuttons-1]->rect.w;
    total_text_and_icon_w =  message->rect.x + message->rect.w;
    if (total_button_w > total_text_and_icon_w) {
        w = total_button_w;
    } else {
        w = total_text_and_icon_w;
    }
    w += SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * 2;
    if (message->rect.h > icon->rect.h) {
        h = message->rect.h;
    } else {
        h = icon->rect.h;
    }
    h += max_button_h + SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * 3;
    t = (w - total_text_and_icon_w) / 2;
    icon->rect.x += t;
    message->rect.x += t;
    icon->rect.y += SDL_TOOLKIT_X11_ELEMENT_PADDING_2;
    message->rect.y += SDL_TOOLKIT_X11_ELEMENT_PADDING_2;
    t = (w - total_button_w) / 2;
    for (i = 0; i < messageboxdata->numbuttons; i++) {
        buttons[i]->rect.x += t;
        buttons[i]->rect.y += SDL_TOOLKIT_X11_ELEMENT_PADDING_2;
    }
    if (!messageboxdata->message) {
        icon->rect.x = (w - icon->rect.w)/2;
    }
    
    /* Actually create window, do event loop, cleanup */
    X11Toolkit_CreateWindowRes(window, w, h, (char *)messageboxdata->title);
    X11Toolkit_DoWindowEventLoop(window);
    X11Toolkit_DestroyWindow(window);
    if (buttons) {
        SDL_free(buttons);
    }
    return true;
}

// Display an x11 message box.
bool X11_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
#if SDL_FORK_MESSAGEBOX
    // Use a child process to protect against setlocale(). Annoying.
    pid_t pid;
    int fds[2];
    int status = 0;
    bool result = true;

    if (pipe(fds) == -1) {
        return X11_ShowMessageBoxImpl(messageboxdata, buttonID); // oh well.
    }

    pid = fork();
    if (pid == -1) { // failed
        close(fds[0]);
        close(fds[1]);
        return X11_ShowMessageBoxImpl(messageboxdata, buttonID); // oh well.
    } else if (pid == 0) {                                       // we're the child
        int exitcode = 0;
        close(fds[0]);
        result = X11_ShowMessageBoxImpl(messageboxdata, buttonID);
        if (write(fds[1], &result, sizeof(result)) != sizeof(result)) {
            exitcode = 1;
        } else if (write(fds[1], buttonID, sizeof(*buttonID)) != sizeof(*buttonID)) {
            exitcode = 1;
        }
        close(fds[1]);
        _exit(exitcode); // don't run atexit() stuff, static destructors, etc.
    } else {             // we're the parent
        pid_t rc;
        close(fds[1]);
        do {
            rc = waitpid(pid, &status, 0);
        } while ((rc == -1) && (errno == EINTR));

        SDL_assert(rc == pid); // not sure what to do if this fails.

        if ((rc == -1) || (!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
            result = SDL_SetError("msgbox child process failed");
        } else if ((read(fds[0], &result, sizeof(result)) != sizeof(result)) ||
                   (read(fds[0], buttonID, sizeof(*buttonID)) != sizeof(*buttonID))) {
            result = SDL_SetError("read from msgbox child process failed");
            *buttonID = 0;
        }
        close(fds[0]);

        return result;
    }
#else
    return X11_ShowMessageBoxImpl(messageboxdata, buttonID);
#endif
}
#endif // SDL_VIDEO_DRIVER_X11
