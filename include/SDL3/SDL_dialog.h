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

#ifndef SDL_dialog_h_
#define SDL_dialog_h_

#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * An entry for filters for file dialogs.
 *
 * `name` is a user-readable label for the filter (for example, "Office document").
 *
 * `pattern` is a semicolon-separated list of file extensions (for example,
 * "doc;docx").
 *
 * \sa SDL_DialogFileCallback
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 */
typedef struct
{
    const char *name;
    const char *pattern;
} SDL_DialogFileFilter;

/**
 * Callback used by file dialog functions.
 *
 * The specific usage is described in each function.
 *
 * If filelist is...
 * - `NULL`, an error occured. Details can be obtained with SDL_GetError().
 * - A pointer to `NULL`, the user either didn't choose any file or canceled
 *   the dialog.
 * - A pointer to non-`NULL`, the user chose one or more files. The argument is
 *   a null-terminated list of pointers to C strings, each containing a path.
 *
 * The filelist argument does not need to be freed; it will automatically be
 * freed when the callback returns.
 *
 * The filter argument is the index of the filter that was selected, or one
 * more than the size of the list (therefore the index of the terminating NULL
 * entry) if no filter was selected, or -1 if the platform or method doesn't
 * support fetching the selected filter or if an error occured.
 *
 * \sa SDL_DialogFileFilter
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 */
typedef void(SDLCALL *SDL_DialogFileCallback)(void *userdata, const char * const *filelist, int filter);

/**
 * Displays a dialog that lets the user select a file on their filesystem.
 *
 * This function should only be invoked from the main thread.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with a null-terminated list of files the user
 * chose. The list will be empty if the user canceled the dialog, and it will
 * be NULL if an error occured.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * Depending on the platform, the user may be allowed to input paths that
 * don't yet exist.
 *
 * \param callback The function to be invoked when the user selects a file and
 *                 accepts, or the user cancels the dialog, or an error
 *                 occurs. The first argument is a null-terminated list of C
 *                 strings, representing the paths chosen by the user. The
 *                 list will be empty if the user canceled the dialog, and it
 *                 will be NULL if an error occured. If an error occured, it
 *                 can be fetched with SDL_GetError(). The second argument is
 *                 the userdata pointer passed to the function.
 * \param userdata An optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param window The window that the dialog should be modal for. May be NULL.
 *               Not all platforms support this option.
 * \param filters A null-terminated list of SDL_DialogFileFilter's. May be
 *                NULL. Not all platforms support this option, and platforms
 *                that do support it may allow the user to ignore the filters.
 * \param default_location The default folder or file to start the dialog at.
 *                         May be NULL. Not all platforms support this option.
 * \param allow_many If non-zero, the user will be allowed to select multiple
 *                   entries. Not all platforms support this option.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 */
extern DECLSPEC void SDLCALL SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, const char *default_location, SDL_bool allow_many);

/**
 * Displays a dialog that lets the user choose a new or existing file on their
 * filesystem.
 *
 * This function should only be invoked from the main thread.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with a null-terminated list of files the user
 * chose. The list will be empty if the user canceled the dialog, and it will
 * be NULL if an error occured.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * The chosen file may or may not already exist.
 *
 * \param callback The function to be invoked when the user selects a file and
 *                 accepts, or the user cancels the dialog, or an error
 *                 occurs. The first argument is a null-terminated list of C
 *                 strings, representing the paths chosen by the user. The
 *                 list will be empty if the user canceled the dialog, and it
 *                 will be NULL if an error occured. If an error occured, it
 *                 can be fetched with SDL_GetError(). The second argument is
 *                 the userdata pointer passed to the function.
 * \param userdata An optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param window The window that the dialog should be modal for. May be NULL.
 *               Not all platforms support this option.
 * \param filters A null-terminated list of SDL_DialogFileFilter's. May be
 *                NULL. Not all platforms support this option, and platforms
 *                that do support it may allow the user to ignore the filters.
 * \param default_location The default folder or file to start the dialog at.
 *                         May be NULL. Not all platforms support this option.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ShowOpenFileDialog
 */
extern DECLSPEC void SDLCALL SDL_ShowSaveFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, const char *default_location);

/**
 * Displays a dialog that lets the user select a folder on their filesystem.
 *
 * This function should only be invoked from the main thread.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with a null-terminated list of files the user
 * chose. The list will be empty if the user canceled the dialog, and it will
 * be NULL if an error occured.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * Depending on the platform, the user may be allowed to input paths that
 * don't yet exist.
 *
 * \param callback The function to be invoked when the user selects a folder
 *                 and accepts, or the user cancels the dialog, or an error
 *                 occurs. The first argument is a null-terminated list of C
 *                 strings, representing the paths chosen by the user. The
 *                 list will be empty if the user canceled the dialog, and it
 *                 will be NULL if an error occured. If an error occured, it
 *                 can be fetched with SDL_GetError(). The second argument is
 *                 the userdata pointer passed to the function.
 * \param userdata An optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param window The window that the dialog should be modal for. May be NULL.
 *               Not all platforms support this option.
 * \param default_location The default folder or file to start the dialog at.
 *                         May be NULL. Not all platforms support this option.
 * \param allow_many If non-zero, the user will be allowed to select multiple
 *                   entries. Not all platforms support this option.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ShowOpenFileDialog
 */
extern DECLSPEC void SDLCALL SDL_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const char *default_location, SDL_bool allow_many);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_joystick_h_ */
