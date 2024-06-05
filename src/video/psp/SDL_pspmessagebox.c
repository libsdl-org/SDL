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

#ifdef SDL_VIDEO_DRIVER_PSP

#include "SDL_pspvideo.h"
#include "SDL_pspmessagebox.h"
#include <psputility.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspthreadman.h>
#include <pspkernel.h>

static void configure_dialog(pspUtilityMsgDialogParams *dialog, size_t dialog_size)
{
	/* clear structure and setup size */
	SDL_memset(dialog, 0, dialog_size);
	dialog->base.size = dialog_size;

	/* set language */
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &dialog->base.language);

	/* set X/O swap */
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &dialog->base.buttonSwap);

	/* set thread priorities */
	/* TODO: understand how these work */
	dialog->base.soundThread = 0x10;
	dialog->base.graphicsThread = 0x11;
	dialog->base.fontThread = 0x12;
	dialog->base.accessThread = 0x13;
}

int PSP_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
	unsigned char list[0x20000] __attribute__((aligned(64)));
	pspUtilityMsgDialogParams dialog;
	int status;

	/* check if it's possible to do a messagebox now */
	if (SDL_GetKeyboardFocus() == NULL)
		return SDL_SetError("No video context is available");

	/* configure dialog */
	configure_dialog(&dialog, sizeof(dialog));

	/* setup dialog options for text */
	dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_TEXT;

	/* copy the message in, 512 bytes max */
	SDL_snprintf(dialog.message, sizeof(dialog.message), "%s\r\n\r\n%s", messageboxdata->title, messageboxdata->message);

	/* too many buttons */
	if (messageboxdata->numbuttons > 2)
		return SDL_SetError("messageboxdata->numbuttons valid values are 0, 1, 2");

	/* we only have two options, "yes/no" or "ok" */
	if (messageboxdata->numbuttons == 2)
		dialog.options |= PSP_UTILITY_MSGDIALOG_OPTION_YESNO_BUTTONS | PSP_UTILITY_MSGDIALOG_OPTION_DEFAULT_NO;

	/* start dialog */
	if (sceUtilityMsgDialogInitStart(&dialog) != 0)
		return SDL_SetError("sceUtilityMsgDialogInitStart() failed for some reason");

	/* loop while the dialog is active */
	status = PSP_UTILITY_DIALOG_NONE;
	do
	{
		sceGuStart(GU_DIRECT, list);
		sceGuClearColor(0);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
		sceGuFinish();
		sceGuSync(0,0);

		status = sceUtilityMsgDialogGetStatus();

		switch (status)
		{
			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityMsgDialogUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityMsgDialogShutdownStart();
				break;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();

	} while (status != PSP_UTILITY_DIALOG_NONE);

	/* success */
	if (dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_YES)
		*buttonID = messageboxdata->buttons[0].buttonID;
	else if (dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_NO)
		*buttonID = messageboxdata->buttons[1].buttonID;
	else
		*buttonID = messageboxdata->buttons[0].buttonID;

	return 0;
}

#endif /* SDL_VIDEO_DRIVER_PSP */
