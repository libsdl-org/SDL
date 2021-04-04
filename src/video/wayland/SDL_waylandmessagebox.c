/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_WAYLAND

#include "SDL.h"
#include <stdlib.h> /* popen/pclose/fgets */

int
Wayland_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    #define ZENITY_CONST(name, str) \
        const char *name = str; \
        const size_t name##_len = SDL_strlen(name);
    ZENITY_CONST(zenity, "zenity --question --switch --no-wrap --icon-name=dialog-")
    ZENITY_CONST(title, "--title=")
    ZENITY_CONST(message, "--text=")
    ZENITY_CONST(extrabutton, "--extra-button=")
    ZENITY_CONST(icon_info, "information")
    ZENITY_CONST(icon_warn, "warning")
    ZENITY_CONST(icon_error, "error")
    #undef ZENITY_CONST

    char *command, *output;
    size_t command_len, output_len;
    const char *icon;
    char *tmp;
    FILE *process;
    int i;

    /* Start by calculating the lengths of the strings. These commands can get
     * pretty long, so we need to dynamically allocate this.
     */
    command_len = zenity_len;
    output_len = 0;
    switch (messageboxdata->flags)
    {
    case SDL_MESSAGEBOX_ERROR:
        icon = icon_error;
        command_len += icon_error_len;
        break;
    case SDL_MESSAGEBOX_WARNING:
        icon = icon_warn;
        command_len += icon_warn_len;
        break;
    case SDL_MESSAGEBOX_INFORMATION:
    default:
        icon = icon_info;
        command_len += icon_info_len;
        break;
    }
    #define ADD_ARGUMENT(arg, value) \
        command_len += arg + 3; /* Two " and a space */ \
        if (messageboxdata->value != NULL) { \
            command_len += SDL_strlen(messageboxdata->value); \
        }
    ADD_ARGUMENT(title_len, title)
    ADD_ARGUMENT(message_len, message)
    #undef ADD_ARGUMENT
    for (i = 0; i < messageboxdata->numbuttons; i += 1) {
        command_len += extrabutton_len + 3; /* Two " and a space */
        if (messageboxdata->buttons[i].text != NULL) {
            const size_t button_len = SDL_strlen(messageboxdata->buttons[i].text);
            command_len += button_len;
            if (button_len > output_len) {
                output_len = button_len;
            }
        }
    }

    /* Don't forget null terminators! */
    command_len += 1;
    output_len += 1;

    /* Now that we know the length of the command, allocate! */
    command = (char*) SDL_malloc(command_len + output_len);
    if (command == NULL) {
        return SDL_OutOfMemory();
    }
    output = command + command_len;
    command[0] = '\0';
    output[0] = '\0';

    /* Now we can build the command... */
    SDL_strlcpy(command, zenity, command_len);
    SDL_strlcat(command, icon, command_len);
    #define ADD_ARGUMENT(arg, value) \
        SDL_strlcat(command, " ", command_len); \
        SDL_strlcat(command, arg, command_len); \
        SDL_strlcat(command, "\"", command_len); \
        if (value != NULL) { \
            SDL_strlcat(command, value, command_len); \
        } \
        SDL_strlcat(command, "\"", command_len)
    ADD_ARGUMENT(title, messageboxdata->title);
    ADD_ARGUMENT(message, messageboxdata->message);
    for (i = 0; i < messageboxdata->numbuttons; i += 1) {
        ADD_ARGUMENT(extrabutton, messageboxdata->buttons[i].text);
    }
    #undef ADD_ARGUMENT

    /* ... then run it, finally. */
    process = popen(command, "r");
    if (process == NULL) {
        SDL_free(command);
        return SDL_SetError("zenity failed to run");
    }

    /* At this point, if no button ID is needed, we can just bail as soon as the
     * process has completed.
     */
    if (buttonid == NULL) {
        pclose(process);
        goto end;
    }
    *buttonid = -1;

    /* Read the result from stdout */
    tmp = fgets(output, output_len, process);
    pclose(process);
    if ((tmp == NULL) || (*tmp == '\0') || (*tmp == '\n')) {
        goto end; /* User simply closed the dialog */
    }

    /* It likes to add a newline... */
    tmp = SDL_strrchr(output, '\n');
    if (tmp != NULL) {
        *tmp = '\0';
    }

    /* Check which button got pressed */
    for (i = 0; i < messageboxdata->numbuttons; i += 1) {
        if (messageboxdata->buttons[i].text != NULL) {
            if (SDL_strcmp(output, messageboxdata->buttons[i].text) == 0) {
                *buttonid = i;
                break;
            }
        }
    }

end:
    SDL_free(command);
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */

/* vi: set ts=4 sw=4 expandtab: */
