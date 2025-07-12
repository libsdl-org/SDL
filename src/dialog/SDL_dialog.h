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

void SDL_SYS_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props);
void SDL_SYS_ShowInputDialogWithProperties(SDL_DialogInputCallback callback, void *userdata, SDL_PropertiesID props);
SDL_ProgressDialog* SDL_SYS_ShowProgressDialogWithProperties(SDL_DialogProgressCallback callback, void *userdata, SDL_PropertiesID props);
void SDL_SYS_UpdateProgressDialog(SDL_ProgressDialog* dialog, float progress, const char *new_prompt);
void SDL_SYS_DestroyProgressDialog(SDL_ProgressDialog* dialog);
void SDL_SYS_ShowColorPickerDialogWithProperties(SDL_DialogColorCallback callback, void *userdata, SDL_PropertiesID props);
void SDL_SYS_ShowDatePickerDialogWithProperties(SDL_DialogDateCallback callback, void *userdata, SDL_PropertiesID props);
