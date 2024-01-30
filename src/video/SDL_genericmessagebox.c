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
#include "SDL_genericmessagebox.h"

#ifndef HAVE_NATIVE_MESSAGEBOXES_H
#ifdef HAVE_LIBDONNELL_H

#include <donnell.h>

void SDL_RenderGenericMessageBox(SDL_MessageBoxDataGeneric* data, SDL_bool first_time) 
{
	int i;
	
	if (first_time == SDL_FALSE) {
		Donnell_ImageBuffer_Clear(data->buffer, data->bg_color);
		Donnell_GraphicsPrimitives_DrawText(data->buffer, data->text_color, (char*)data->messageboxdata->message, MESSAGE_BOX_ICON_PADDING_AMOUNT + MESSAGE_BOX_ICON_SIZE + MESSAGE_BOX_TEXT_PADDING_AMOUNT_X, MESSAGE_BOX_TEXT_PADDING_AMOUNT_Y, MESSAGE_BOX_TEXT_SIZE, DONNELL_FONT_OPTIONS_SANS_SERIF);
		Donnell_GuiPrimitives_Icon_Draw(data->buffer, data->icon, data->icon_index, DONNELL_ICON_SIZE_32, MESSAGE_BOX_ICON_PADDING_AMOUNT, MESSAGE_BOX_ICON_PADDING_AMOUNT);		
	}
	
	for (i = 0; i < data->messageboxdata->numbuttons; i++) {
		Donnell_GraphicsPrimitives_DrawRectangle(data->buffer, data->bg_color, &data->buttons[i].button_rect, DONNELL_FALSE);
		Donnell_GuiPrimitives_DrawButton(data->buffer, (char*)data->messageboxdata->buttons[i].text, &data->buttons[i].button_rect, data->text_color, MESSAGE_BOX_BUTTON_TEXT_SIZE, DONNELL_FONT_OPTIONS_SANS_SERIF, data->buttons[i].button_state, DONNELL_FALSE);
	}
}

SDL_MessageBoxDataGeneric* SDL_CreateGenericMessageBoxData(const SDL_MessageBoxData *messageboxdata, unsigned int scale) 
{
	SDL_MessageBoxDataGeneric* data;
	DonnellSize text_size;
	int calc_width;
	int calc_height;
	int calc_button_width;
	int calc_text_height;
	int i;
	
	if (scale < 1) {
		scale = 1;
	}
	
	data = SDL_malloc(sizeof(SDL_MessageBoxDataGeneric));
	if(!data) {
		return NULL;
	}
	Donnell_Init();
	
    data->messageboxdata = messageboxdata;
	
	Donnell_GraphicsPrimitives_MeasureText(&text_size, (char*)messageboxdata->message, MESSAGE_BOX_TEXT_SIZE, MESSAGE_BOX_TEXT_FONT, 1);
	calc_width = MESSAGE_BOX_ICON_PADDING_AMOUNT + MESSAGE_BOX_ICON_SIZE + MESSAGE_BOX_TEXT_PADDING_AMOUNT_X*2 + text_size.w;
	calc_height = MESSAGE_BOX_ICON_PADDING_AMOUNT + MESSAGE_BOX_ICON_SIZE + MESSAGE_BOX_BUTTON_PADDING_AMOUNT_Y + MESSAGE_BOX_BUTTON_SIZE_H + MESSAGE_BOX_BUTTON_PADDING_AMOUNT_YM;
	calc_text_height = MESSAGE_BOX_TEXT_PADDING_AMOUNT_Y + text_size.h + MESSAGE_BOX_BUTTON_PADDING_AMOUNT_Y + MESSAGE_BOX_BUTTON_SIZE_H + MESSAGE_BOX_BUTTON_PADDING_AMOUNT_YM;
	calc_button_width = 0;
	
    for (i = 0; i < messageboxdata->numbuttons; i++) {
		Donnell_GraphicsPrimitives_MeasureTextLine(&text_size, (char*)messageboxdata->buttons[i].text, MESSAGE_BOX_BUTTON_TEXT_SIZE, MESSAGE_BOX_TEXT_FONT, 1);
		data->buttons[i].button_rect.w = MESSAGE_BOX_BUTTON_TEXT_PADDING_AMOUNT*2 + text_size.w;
		if (MIN_MESSAGE_BOX_BUTTON_SIZE_W > data->buttons[i].button_rect.w) {
			data->buttons[i].button_rect.w = MIN_MESSAGE_BOX_BUTTON_SIZE_W;
		}
		calc_button_width += data->buttons[i].button_rect.w + MESSAGE_BOX_BUTTON_PADDING_AMOUNT_X;
		data->buttons[i].button_rect.h = MESSAGE_BOX_BUTTON_SIZE_H;
		data->buttons[i].button_state = DONNELL_BUTTON_STATE_NORMAL;
    }
    
    if (calc_button_width > calc_width) {
		calc_width = calc_button_width;
	}
	
	if (calc_width < MIN_MESSAGE_BOX_SIZE_W) {
		calc_width = MIN_MESSAGE_BOX_SIZE_W;
	}
	
    if (calc_text_height > calc_height) {
		calc_height = calc_text_height;
	}
	
	if (calc_height < MIN_MESSAGE_BOX_SIZE_H) {
		calc_height = MIN_MESSAGE_BOX_SIZE_H;
	}
	
    for (i = 0; i < messageboxdata->numbuttons; i++) {
		int j;
		int other_widths;
		
		other_widths = 0;
		
		for (j = i+1; j < messageboxdata->numbuttons; j++) {
			other_widths += data->buttons[j].button_rect.w + (MESSAGE_BOX_BUTTON_PADDING_AMOUNT_X/2);
		}
		
		data->buttons[i].button_rect.x = calc_width - data->buttons[i].button_rect.w - MESSAGE_BOX_BUTTON_PADDING_AMOUNT_X - other_widths;
		data->buttons[i].button_rect.y = calc_height - MESSAGE_BOX_BUTTON_SIZE_H - MESSAGE_BOX_BUTTON_PADDING_AMOUNT_Y + MESSAGE_BOX_BUTTON_PADDING_AMOUNT_YM;
    }

	
	data->buffer = Donnell_ImageBuffer_Create(calc_width, calc_height, scale);
	data->bg_color = Donnell_Pixel_CreateEasy(MESSAGE_BOX_BG_COLOR, MESSAGE_BOX_BG_COLOR, MESSAGE_BOX_BG_COLOR, 255);
	data->text_color = Donnell_Pixel_CreateEasy(MESSAGE_BOX_TEXT_COLOR, MESSAGE_BOX_TEXT_COLOR, MESSAGE_BOX_TEXT_COLOR, 255);
		
	if(messageboxdata->flags & SDL_MESSAGEBOX_WARNING) {
		data->icon = Donnell_GuiPrimitives_StockIcons_Load(DONNELL_STOCK_ICON_WARNING);	
	} else if(messageboxdata->flags & SDL_MESSAGEBOX_INFORMATION) {
		data->icon = Donnell_GuiPrimitives_StockIcons_Load(DONNELL_STOCK_ICON_INFO);		
	} else {
		data->icon = Donnell_GuiPrimitives_StockIcons_Load(DONNELL_STOCK_ICON_ERROR);			
	}
	
	data->icon_index = Donnell_GuiPrimitives_Icon_GetBestForSize(data->icon, MESSAGE_BOX_ICON_SIZE, scale);

	return data;
} 

void SDL_DestroyGenericMessageBoxData(SDL_MessageBoxDataGeneric* data) {
	Donnell_ImageBuffer_Free(data->buffer);
	Donnell_GuiPrimitives_Icon_Free(data->icon);
	Donnell_Pixel_Free(data->text_color);
	Donnell_Pixel_Free(data->bg_color);
	SDL_free(data);
	Donnell_Cleanup();
}

#else

#include <stdlib.h>   /* fgets */
#include <stdio.h>    /* FILE, STDOUT_FILENO, fdopen, fclose */
#include <unistd.h>   /* pid_t, pipe, fork, close, dup2, execvp, _exit */
#include <sys/wait.h> /* waitpid, WIFEXITED, WEXITSTATUS */
#include <string.h>   /* strerr */
#include <errno.h>

#define ZENITY_VERSION_LEN 32 /* Number of bytes to read from zenity --version (including NUL)*/

static int run_zenity(const char **args, int fd_pipe[2])
{
    int status;
    pid_t pid1;

    pid1 = fork();
    if (pid1 == 0) { /* child process */
        close(fd_pipe[0]); /* no reading from pipe */
        /* write stdout in pipe */
        if (dup2(fd_pipe[1], STDOUT_FILENO) == -1) {
            _exit(128);
        }
        close(fd_pipe[1]);

        /* const casting argv is fine:
         * https://pubs.opengroup.org/onlinepubs/9699919799/functions/fexecve.html -> rational
         */
        execvp("zenity", (char **)args);
        _exit(129);
    } else if (pid1 < 0) { /* fork() failed */
        return SDL_SetError("fork() failed: %s", strerror(errno));
    } else { /* parent process */
        if (waitpid(pid1, &status, 0) != pid1) {
            return SDL_SetError("Waiting on zenity failed: %s", strerror(errno));
        }

        if (!WIFEXITED(status)) {
            return SDL_SetError("zenity failed for some reason");
        }

        if (WEXITSTATUS(status) >= 128) {
            return SDL_SetError("zenity reported error or failed to launch: %d", WEXITSTATUS(status));
        }

        return 0; /* success! */
    }
}

static int get_zenity_version(int *major, int *minor)
{
    int fd_pipe[2]; /* fd_pipe[0]: read end of pipe, fd_pipe[1]: write end of pipe */
    const char *argv[] = { "zenity", "--version", NULL };

    if (pipe(fd_pipe) != 0) { /* create a pipe */
        return SDL_SetError("pipe() failed: %s", strerror(errno));
    }

    if (run_zenity(argv, fd_pipe) == 0) {
        FILE *outputfp = NULL;
        char version_str[ZENITY_VERSION_LEN];
        char *version_ptr = NULL, *end_ptr = NULL;
        int tmp;

        close(fd_pipe[1]);
        outputfp = fdopen(fd_pipe[0], "r");
        if (!outputfp) {
            close(fd_pipe[0]);
            return SDL_SetError("failed to open pipe for reading: %s", strerror(errno));
        }

        version_ptr = fgets(version_str, ZENITY_VERSION_LEN, outputfp);
        (void)fclose(outputfp); /* will close underlying fd */

        if (!version_ptr) {
            return SDL_SetError("failed to read zenity version string");
        }

        /* we expect the version string is in the form of MAJOR.MINOR.MICRO
         * as described in meson.build. We'll ignore everything after that.
         */
        tmp = (int) SDL_strtol(version_ptr, &end_ptr, 10);
        if (tmp == 0 && end_ptr == version_ptr) {
            return SDL_SetError("failed to get zenity major version number");
        }
        *major = tmp;

        if (*end_ptr == '.') {
            version_ptr = end_ptr + 1; /* skip the dot */
            tmp = (int) SDL_strtol(version_ptr, &end_ptr, 10);
            if (tmp == 0 && end_ptr == version_ptr) {
                return SDL_SetError("failed to get zenity minor version number");
            }
            *minor = tmp;
        } else {
            *minor = 0;
        }

        return 0; /* success */
    }

    close(fd_pipe[0]);
    close(fd_pipe[1]);
    return -1; /* run_zenity should've called SDL_SetError() */
}

int SDL_ShowGenericMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    int fd_pipe[2]; /* fd_pipe[0]: read end of pipe, fd_pipe[1]: write end of pipe */
    int zenity_major = 0, zenity_minor = 0, output_len = 0;
    int argc = 5, i;
    const char *argv[5 + 2 /* icon name */ + 2 /* title */ + 2 /* message */ + 2 * MAX_BUTTONS + 1 /* NULL */] = {
        "zenity", "--question", "--switch", "--no-wrap", "--no-markup"
    };

    if (messageboxdata->numbuttons > MAX_BUTTONS) {
        return SDL_SetError("Too many buttons (%d max allowed)", MAX_BUTTONS);
    }

    /* get zenity version so we know which arg to use */
    if (get_zenity_version(&zenity_major, &zenity_minor) != 0) {
        return -1; /* get_zenity_version() calls SDL_SetError(), so message is already set */
    }

    if (pipe(fd_pipe) != 0) { /* create a pipe */
        return SDL_SetError("pipe() failed: %s", strerror(errno));
    }

    /* https://gitlab.gnome.org/GNOME/zenity/-/commit/c686bdb1b45e95acf010efd9ca0c75527fbb4dea
     * This commit removed --icon-name without adding a deprecation notice.
     * We need to handle it gracefully, otherwise no message box will be shown.
     */
    argv[argc++] = zenity_major > 3 || (zenity_major == 3 && zenity_minor >= 90) ? "--icon" : "--icon-name";
    switch (messageboxdata->flags) {
    case SDL_MESSAGEBOX_ERROR:
        argv[argc++] = "dialog-error";
        break;
    case SDL_MESSAGEBOX_WARNING:
        argv[argc++] = "dialog-warning";
        break;
    case SDL_MESSAGEBOX_INFORMATION:
    default:
        argv[argc++] = "dialog-information";
        break;
    }

    if (messageboxdata->title && messageboxdata->title[0]) {
        argv[argc++] = "--title";
        argv[argc++] = messageboxdata->title;
    } else {
        argv[argc++] = "--title=\"\"";
    }

    if (messageboxdata->message && messageboxdata->message[0]) {
        argv[argc++] = "--text";
        argv[argc++] = messageboxdata->message;
    } else {
        argv[argc++] = "--text=\"\"";
    }

    for (i = 0; i < messageboxdata->numbuttons; ++i) {
        if (messageboxdata->buttons[i].text && messageboxdata->buttons[i].text[0]) {
            int len = SDL_strlen(messageboxdata->buttons[i].text);
            if (len > output_len) {
                output_len = len;
            }

            argv[argc++] = "--extra-button";
            argv[argc++] = messageboxdata->buttons[i].text;
        } else {
            argv[argc++] = "--extra-button=\"\"";
        }
    }
    argv[argc] = NULL;

    if (run_zenity(argv, fd_pipe) == 0) {
        FILE *outputfp = NULL;
        char *output = NULL;
        char *tmp = NULL;

        if (!buttonid) {
            /* if we don't need buttonid, we can return immediately */
            close(fd_pipe[0]);
            return 0; /* success */
        }
        *buttonid = -1;

        output = SDL_malloc(output_len + 1);
        if (!output) {
            close(fd_pipe[0]);
            return -1;
        }
        output[0] = '\0';

        outputfp = fdopen(fd_pipe[0], "r");
        if (!outputfp) {
            SDL_free(output);
            close(fd_pipe[0]);
            return SDL_SetError("Couldn't open pipe for reading: %s", strerror(errno));
        }
        tmp = fgets(output, output_len + 1, outputfp);
        (void)fclose(outputfp);

        if ((!tmp) || (*tmp == '\0') || (*tmp == '\n')) {
            SDL_free(output);
            return 0; /* User simply closed the dialog */
        }

        /* It likes to add a newline... */
        tmp = SDL_strrchr(output, '\n');
        if (tmp) {
            *tmp = '\0';
        }

        /* Check which button got pressed */
        for (i = 0; i < messageboxdata->numbuttons; i += 1) {
            if (messageboxdata->buttons[i].text) {
                if (SDL_strcmp(output, messageboxdata->buttons[i].text) == 0) {
                    *buttonid = messageboxdata->buttons[i].buttonid;
                    break;
                }
            }
        }

        SDL_free(output);
        return 0; /* success! */
    }

    close(fd_pipe[0]);
    close(fd_pipe[1]);
    return -1; /* run_zenity() calls SDL_SetError(), so message is already set */
}
#endif
#endif

