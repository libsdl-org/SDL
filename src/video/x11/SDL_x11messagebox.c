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

typedef struct SDL_MessageBoxControlsX11
{
    SDL_ToolkitWindowX11 *window;
    SDL_ToolkitControlX11 *icon;
    SDL_ToolkitControlX11 fake_icon;
    SDL_ToolkitControlX11 *message;
    SDL_ToolkitControlX11 **buttons;
    const SDL_MessageBoxData *messageboxdata;
} SDL_MessageBoxControlsX11;

static void X11_MessageBoxButtonCallback(SDL_ToolkitControlX11 *control, void *data)
{
    SDL_MessageBoxCallbackDataX11 *cbdata;
    
    cbdata = (SDL_MessageBoxCallbackDataX11 *)data;
    *cbdata->buttonID = X11Toolkit_GetButtonControlData(control)->buttonID;
    X11Toolkit_SignalWindowClose(cbdata->window);
}

static void X11_PositionMessageBox(SDL_MessageBoxControlsX11 *controls, int *wp, int *hp) {
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
    max_button_w *= controls->window->iscale;
   
    
    /* Positioning and sizing */
    for (i = 0; i < controls->messageboxdata->numbuttons; i++) {
        max_button_w = SDL_max(max_button_w, controls->buttons[i]->rect.w);
        max_button_h = SDL_max(max_button_h, controls->buttons[i]->rect.h);
        controls->buttons[i]->rect.x = 0;
    }
    
    if (controls->icon) {
        controls->icon->rect.x = controls->icon->rect.y = 0;
    }
        
    if (controls->icon) {
        controls->message->rect.x = (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale) + controls->icon->rect.x + controls->icon->rect.w;
        controls->message->rect.y = X11Toolkit_GetIconControlCharY(controls->icon);
    } else {
        controls->message->rect.x = 0;
        controls->message->rect.y = -2;
        controls->icon = &controls->fake_icon;
        controls->icon->rect.w = 0;
        controls->icon->rect.h = 0;
        controls->icon->rect.x = 0;
        controls->icon->rect.y = 0;    
    }
    if (controls->messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
        for (i = controls->messageboxdata->numbuttons; i != -1; i--) {
            controls->buttons[i]->rect.w = max_button_w;
            controls->buttons[i]->rect.h = max_button_h;
            X11Toolkit_NotifyControlOfSizeChange(controls->buttons[i]);    
            
            if (controls->icon->rect.h > controls->message->rect.h) {
                controls->buttons[i]->rect.y = controls->icon->rect.h + (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 *controls-> window->iscale);
            } else {
                controls->buttons[i]->rect.y = controls->message->rect.h + (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale);                
            }        
            
            if (i) {
                controls->buttons[i]->rect.x = controls->buttons[i-1]->rect.x + controls->buttons[i-1]->rect.w + (SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * controls->window->iscale);
            }
        }        
    } else {
        for (i = 0; i < controls->messageboxdata->numbuttons; i++) {
            controls->buttons[i]->rect.w = max_button_w;
            controls->buttons[i]->rect.h = max_button_h;
            X11Toolkit_NotifyControlOfSizeChange(controls->buttons[i]);    
            
            if (controls->icon->rect.h > controls->message->rect.h) {
                controls->buttons[i]->rect.y = controls->icon->rect.h + (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale);
            } else {
                controls->buttons[i]->rect.y = controls->message->rect.h + (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale);                
            }        
            
            if (i) {
                controls->buttons[i]->rect.x = controls->buttons[i-1]->rect.x + controls->buttons[i-1]->rect.w + (SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * controls->window->iscale);
            }
        }        
    }
    total_button_w = controls->buttons[controls->messageboxdata->numbuttons-1]->rect.x + controls->buttons[controls->messageboxdata->numbuttons-1]->rect.w;
    total_text_and_icon_w =  controls->message->rect.x + controls->message->rect.w;
    if (total_button_w > total_text_and_icon_w) {
        w = total_button_w;
    } else {
        w = total_text_and_icon_w;
    }
    w += (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale) * 2;
    if (controls->message->rect.h > controls->icon->rect.h) {
        h = controls->message->rect.h;
    } else {
        h = controls->icon->rect.h;
    }
    h += max_button_h + (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale) * 3;
    t = (w - total_text_and_icon_w) / 2;
    controls->icon->rect.x += t;
    controls->message->rect.x += t;
    controls->icon->rect.y += (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale);
    controls->message->rect.y += (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale);
    t = (w - total_button_w) / 2;
    for (i = 0; i < controls->messageboxdata->numbuttons; i++) {
        controls->buttons[i]->rect.x += t;
        controls->buttons[i]->rect.y += (SDL_TOOLKIT_X11_ELEMENT_PADDING_2 * controls->window->iscale);
    }
    if (!controls->messageboxdata->message) {
        controls->icon->rect.x = (w - controls->icon->rect.w)/2;
    }  
    
    *wp = w;
    *hp = h;
}

static void X11_OnMessageBoxScaleChange(SDL_ToolkitWindowX11 *window, void *data) {
    SDL_MessageBoxControlsX11 *controls;
    int w;
    int h;
        
    controls = data;
    X11_PositionMessageBox(controls, &w, &h);
    X11Toolkit_ResizeWindow(window, w, h);
}

static bool X11_ShowMessageBoxImpl(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    SDL_MessageBoxControlsX11 controls;
    SDL_MessageBoxCallbackDataX11 data;
    const SDL_MessageBoxColor *colorhints;
    int i;
    int w;
    int h;
    
    controls.messageboxdata = messageboxdata;
    
    /* Color scheme */
    if (messageboxdata->colorScheme) {
        colorhints = messageboxdata->colorScheme->colors;
    } else {
        colorhints = NULL;
    }
    
    /* Create window */
    controls.window = X11Toolkit_CreateWindowStruct(messageboxdata->window, colorhints);
    controls.window->cb_data = &controls;
    controls.window->cb_on_scale_change = X11_OnMessageBoxScaleChange;
    if (!controls.window) {
        return false;
    }
    
    /* Create controls */
    controls.buttons = SDL_calloc(messageboxdata->numbuttons, sizeof(SDL_ToolkitControlX11 *));
    controls.icon = X11Toolkit_CreateIconControl(controls.window, messageboxdata->flags);
    controls.message = X11Toolkit_CreateLabelControl(controls.window, (char *)messageboxdata->message);
    data.buttonID = buttonID;
    data.window = controls.window;
    for (i = 0; i < messageboxdata->numbuttons; i++) {
        controls.buttons[i] = X11Toolkit_CreateButtonControl(controls.window, &messageboxdata->buttons[i]);
        X11Toolkit_RegisterCallbackForButtonControl(controls.buttons[i], &data, X11_MessageBoxButtonCallback);
    }

    /* Positioning */
    X11_PositionMessageBox(&controls, &w, &h);
    
    /* Actually create window, do event loop, cleanup */
    X11Toolkit_CreateWindowRes(controls.window, w, h, (char *)messageboxdata->title);
    X11Toolkit_DoWindowEventLoop(controls.window);
    X11Toolkit_DestroyWindow(controls.window);
    if (controls.buttons) {
        SDL_free(controls.buttons);
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
