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

/**
 * # CategoryDialog
 *
 * File dialog support.
 *
 * SDL offers file dialogs, to let users select files with native GUI
 * interfaces. There are "open" dialogs, "save" dialogs, and folder selection
 * dialogs. The app can control some details, such as filtering to specific
 * files, or whether multiple files can be selected by the user.
 *
 * Note that launching a file dialog is a non-blocking operation; control
 * returns to the app immediately, and a callback is called later (possibly in
 * another thread) when the user makes a choice.
 */

#ifndef SDL_dialog_h_
#define SDL_dialog_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * An entry for filters for file dialogs.
 *
 * `name` is a user-readable label for the filter (for example, "Office
 * document").
 *
 * `pattern` is a semicolon-separated list of file extensions (for example,
 * "doc;docx"). File extensions may only contain alphanumeric characters,
 * hyphens, underscores and periods. Alternatively, the whole string can be a
 * single asterisk ("*"), which serves as an "All files" filter.
 *
 * \since This struct is available since SDL 3.2.0.
 *
 * \sa SDL_DialogFileCallback
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 * \sa SDL_ShowFileDialogWithProperties
 */
typedef struct SDL_DialogFileFilter
{
    const char *name;
    const char *pattern;
} SDL_DialogFileFilter;

/**
 * Callback used by file dialog functions.
 *
 * The specific usage is described in each function.
 *
 * If `filelist` is:
 *
 * - NULL, an error occurred. Details can be obtained with SDL_GetError().
 * - A pointer to NULL, the user either didn't choose any file or canceled the
 *   dialog.
 * - A pointer to non-`NULL`, the user chose one or more files. The argument
 *   is a null-terminated array of pointers to UTF-8 encoded strings, each
 *   containing a path.
 *
 * The filelist argument should not be freed; it will automatically be freed
 * when the callback returns.
 *
 * The filter argument is the index of the filter that was selected, or -1 if
 * no filter was selected or if the platform or method doesn't support
 * fetching the selected filter.
 *
 * In Android, the `filelist` are `content://` URIs. They should be opened
 * using SDL_IOFromFile() with appropriate modes. This applies both to open
 * and save file dialog.
 *
 * \param userdata an app-provided pointer, for the callback's use.
 * \param filelist the file(s) chosen by the user.
 * \param filter index of the selected filter.
 *
 * \since This datatype is available since SDL 3.2.0.
 *
 * \sa SDL_DialogFileFilter
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 * \sa SDL_ShowFileDialogWithProperties
 */
typedef void (SDLCALL *SDL_DialogFileCallback)(void *userdata, const char * const *filelist, int filter);

/**
 * Displays a dialog that lets the user select a file on their filesystem.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with a null-terminated list of files the user
 * chose. The list will be empty if the user canceled the dialog, and it will
 * be NULL if an error occurred.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * Depending on the platform, the user may be allowed to input paths that
 * don't yet exist.
 *
 * On Linux, dialogs may require XDG Portals, which requires DBus, which
 * requires an event-handling loop. Apps that do not use SDL to handle events
 * should add a call to SDL_PumpEvents in their main loop.
 *
 * \param callback a function pointer to be invoked when the user selects a
 *                 file and accepts, or cancels the dialog, or an error
 *                 occurs.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param window the window that the dialog should be modal for, may be NULL.
 *               Not all platforms support this option.
 * \param filters a list of filters, may be NULL. See the
 *                [`SDL_DialogFileFilter` documentation for
 *                examples](SDL_DialogFileFilter#code-examples). Not all
 *                platforms support this option, and platforms that do support
 *                it may allow the user to ignore the filters. If non-NULL, it
 *                must remain valid at least until the callback is invoked.
 * \param nfilters the number of filters. Ignored if filters is NULL.
 * \param default_location the default folder or file to start the dialog at,
 *                         may be NULL. Not all platforms support this option.
 * \param allow_many if non-zero, the user will be allowed to select multiple
 *                   entries. Not all platforms support this option.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.2.0.
 *
 * \sa SDL_DialogFileCallback
 * \sa SDL_DialogFileFilter
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 * \sa SDL_ShowFileDialogWithProperties
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location, bool allow_many);

/**
 * Displays a dialog that lets the user choose a new or existing file on their
 * filesystem.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with a null-terminated list of files the user
 * chose. The list will be empty if the user canceled the dialog, and it will
 * be NULL if an error occurred.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * The chosen file may or may not already exist.
 *
 * On Linux, dialogs may require XDG Portals, which requires DBus, which
 * requires an event-handling loop. Apps that do not use SDL to handle events
 * should add a call to SDL_PumpEvents in their main loop.
 *
 * \param callback a function pointer to be invoked when the user selects a
 *                 file and accepts, or cancels the dialog, or an error
 *                 occurs.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param window the window that the dialog should be modal for, may be NULL.
 *               Not all platforms support this option.
 * \param filters a list of filters, may be NULL. Not all platforms support
 *                this option, and platforms that do support it may allow the
 *                user to ignore the filters. If non-NULL, it must remain
 *                valid at least until the callback is invoked.
 * \param nfilters the number of filters. Ignored if filters is NULL.
 * \param default_location the default folder or file to start the dialog at,
 *                         may be NULL. Not all platforms support this option.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.2.0.
 *
 * \sa SDL_DialogFileCallback
 * \sa SDL_DialogFileFilter
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowOpenFolderDialog
 * \sa SDL_ShowFileDialogWithProperties
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowSaveFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location);

/**
 * Displays a dialog that lets the user select a folder on their filesystem.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with a null-terminated list of files the user
 * chose. The list will be empty if the user canceled the dialog, and it will
 * be NULL if an error occurred.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * Depending on the platform, the user may be allowed to input paths that
 * don't yet exist.
 *
 * On Linux, dialogs may require XDG Portals, which requires DBus, which
 * requires an event-handling loop. Apps that do not use SDL to handle events
 * should add a call to SDL_PumpEvents in their main loop.
 *
 * \param callback a function pointer to be invoked when the user selects a
 *                 file and accepts, or cancels the dialog, or an error
 *                 occurs.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param window the window that the dialog should be modal for, may be NULL.
 *               Not all platforms support this option.
 * \param default_location the default folder or file to start the dialog at,
 *                         may be NULL. Not all platforms support this option.
 * \param allow_many if non-zero, the user will be allowed to select multiple
 *                   entries. Not all platforms support this option.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.2.0.
 *
 * \sa SDL_DialogFileCallback
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowFileDialogWithProperties
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const char *default_location, bool allow_many);

/**
 * Various types of file dialogs.
 *
 * This is used by SDL_ShowFileDialogWithProperties() to decide what kind of
 * dialog to present to the user.
 *
 * \since This enum is available since SDL 3.2.0.
 *
 * \sa SDL_ShowFileDialogWithProperties
 */
typedef enum SDL_FileDialogType
{
    SDL_FILEDIALOG_OPENFILE,
    SDL_FILEDIALOG_SAVEFILE,
    SDL_FILEDIALOG_OPENFOLDER
} SDL_FileDialogType;

/**
 * Create and launch a file dialog with the specified properties.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_FILE_DIALOG_FILTERS_POINTER`: a pointer to a list of
 *   SDL_DialogFileFilter structs, which will be used as filters for
 *   file-based selections. Ignored if the dialog is an "Open Folder" dialog.
 *   If non-NULL, the array of filters must remain valid at least until the
 *   callback is invoked.
 * - `SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER`: the number of filters in the
 *   array of filters, if it exists.
 * - `SDL_PROP_FILE_DIALOG_WINDOW_POINTER`: the window that the dialog should
 *   be modal for.
 * - `SDL_PROP_FILE_DIALOG_LOCATION_STRING`: the default folder or file to
 *   start the dialog at.
 * - `SDL_PROP_FILE_DIALOG_MANY_BOOLEAN`: true to allow the user to select
 *   more than one entry.
 * - `SDL_PROP_FILE_DIALOG_TITLE_STRING`: the title for the dialog.
 * - `SDL_PROP_FILE_DIALOG_ACCEPT_STRING`: the label that the accept button
 *   should have.
 * - `SDL_PROP_FILE_DIALOG_CANCEL_STRING`: the label that the cancel button
 *   should have.
 *
 * Note that each platform may or may not support any of the properties.
 *
 * \param type the type of file dialog.
 * \param callback a function pointer to be invoked when the user selects a
 *                 file and accepts, or cancels the dialog, or an error
 *                 occurs.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param props the properties to use.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.2.0.
 *
 * \sa SDL_FileDialogType
 * \sa SDL_DialogFileCallback
 * \sa SDL_DialogFileFilter
 * \sa SDL_ShowOpenFileDialog
 * \sa SDL_ShowSaveFileDialog
 * \sa SDL_ShowOpenFolderDialog
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props);

#define SDL_PROP_FILE_DIALOG_FILTERS_POINTER     "SDL.filedialog.filters"
#define SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER     "SDL.filedialog.nfilters"
#define SDL_PROP_FILE_DIALOG_WINDOW_POINTER      "SDL.filedialog.window"
#define SDL_PROP_FILE_DIALOG_LOCATION_STRING     "SDL.filedialog.location"
#define SDL_PROP_FILE_DIALOG_MANY_BOOLEAN        "SDL.filedialog.many"
#define SDL_PROP_FILE_DIALOG_TITLE_STRING        "SDL.filedialog.title"
#define SDL_PROP_FILE_DIALOG_ACCEPT_STRING       "SDL.filedialog.accept"
#define SDL_PROP_FILE_DIALOG_CANCEL_STRING       "SDL.filedialog.cancel"

/**
 * Result of dialogs.
 *
 * This is passed as an argument for the callback of many dialog-related
 * functions.
 *
 * \since This enum is available since SDL 3.4.0.
 */
typedef enum SDL_DialogResult
{
    /** An error occured. The dialog may or may not have been shown to the user
        before the error happened, but the chosen input couldn't be obtained. */
    SDL_DIALOGRESULT_ERROR,
    /** The user has canceled the dialog, or the program aborted the dialog. */
    SDL_DIALOGRESULT_CANCEL,
    /** The user finished the input and confirmed the result. */
    SDL_DIALOGRESULT_SUCCESS
} SDL_DialogResult;

/**
 * Callback used by input dialog functions.
 *
 * \param userdata an app-provided pointer, for the callback's use.
 * \param input the text input by the user.
 * \param result the success status of the dialog.
 *
 * \since This datatype is available since SDL 3.4.0.
 *
 * \sa SDL_DialogResult
 * \sa SDL_ShowInputDialogWithProperties
 */
typedef void (SDLCALL *SDL_DialogInputCallback)(void *userdata, const char *input, SDL_DialogResult result);

/**
 * Create and launch a text input dialog with the specified properties.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with the string input by the user, an empty
 * string if the user canceled the dialog, and NULL if an error occured.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_INPUT_DIALOG_WINDOW_POINTER`: the window that the dialog should
 *   be modal for.
 * - `SDL_PROP_INPUT_DIALOG_TITLE_STRING`: the title for the dialog.
 * - `SDL_PROP_INPUT_DIALOG_PROMPT_STRING`: the prompt for the dialog.
 * - `SDL_PROP_INPUT_DIALOG_DEFAULT_STRING`: the default value to fill in the
 *   dialog.
 * - `SDL_PROP_INPUT_DIALOG_ACCEPT_STRING`: the label that the accept button
 *   should have.
 * - `SDL_PROP_INPUT_DIALOG_CANCEL_STRING`: the label that the cancel button
 *   should have.
 * - `SDL_PROP_INPUT_DIALOG_PASSWORD_BOOLEAN`: if true, the text field will
 *   treat its contents as a password, which usually involves displaying one
 *   character (such as an asterisk) in place of all characters in the field.
 *   Defaults to false.
 *
 * Note that each platform may or may not support any of the properties.
 *
 * \param callback a function pointer to be invoked when the user finishes
 *                 entering the prompt.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param props the properties to use.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.4.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowInputDialogWithProperties(SDL_DialogInputCallback callback, void *userdata, SDL_PropertiesID props);

#define SDL_PROP_INPUT_DIALOG_WINDOW_POINTER      "SDL.inputdialog.window"
#define SDL_PROP_INPUT_DIALOG_TITLE_STRING        "SDL.inputdialog.title"
#define SDL_PROP_INPUT_DIALOG_PROMPT_STRING       "SDL.inputdialog.prompt"
#define SDL_PROP_INPUT_DIALOG_DEFAULT_STRING      "SDL.inputdialog.default"
#define SDL_PROP_INPUT_DIALOG_ACCEPT_STRING       "SDL.inputdialog.accept"
#define SDL_PROP_INPUT_DIALOG_CANCEL_STRING       "SDL.inputdialog.cancel"
#define SDL_PROP_INPUT_DIALOG_PASSWORD_BOOLEAN    "SDL.inputdialog.password"

/**
 * A progress dialog.
 *
 * \since This datatype is available since SDL 3.4.0.
 *
 * \sa SDL_ShowProgressDialogWithProperties
 */
typedef struct SDL_ProgressDialog SDL_ProgressDialog;

/**
 * Callback used by progress dialog functions.
 *
 * \param userdata an app-provided pointer, for the callback's use.
 * \param result the success status of the dialog.
 *
 * \since This datatype is available since SDL 3.4.0.
 *
 * \sa SDL_ShowProgressDialogWithProperties
 */
typedef void (SDLCALL *SDL_DialogProgressCallback)(void *userdata, SDL_DialogResult result);

/**
 * Create and launch a text input dialog with the specified properties.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_PROGRESS_DIALOG_WINDOW_POINTER`: the window that the dialog
 *   should be modal for.
 * - `SDL_PROP_PROGRESS_DIALOG_TITLE_STRING`: the title for the dialog.
 * - `SDL_PROP_PROGRESS_DIALOG_PROMPT_STRING`: the prompt for the dialog.
 * - `SDL_PROP_PROGRESS_DIALOG_ACCEPT_STRING`: the label that the accept button
 *   should have.
 * - `SDL_PROP_PROGRESS_DIALOG_CANCEL_STRING`: the label that the cancel button
 *   should have.
 * - `SDL_PROP_PROGRESS_DIALOG_CAN_CANCEL_BOOLEAN`: if true, the user can cancel
 *   the dialog anytime with a "Cancel" button. It may be possible to cancel the
 *   dialog even if this property is set to false. Defaults to true.
 * - `SDL_PROP_PROGRESS_DIALOG_PROGRESS_NUMBER`: a float betweeon 0 and 1
 *   representing the value of the progress bar. Ignored if the property
 *   `SDL_PROP_PROGRESS_DIALOG_INDEFINITE_BOOLEAN` is set to true.
 * - `SDL_PROP_PROGRESS_DIALOG_INDEFINITE_BOOLEAN`: If true, the progress bar
 *   won't show any meaningful value and will adopt a generic progress-less
 *   loading behavior. Defaults to false.
 * - `SDL_PROP_PROGRESS_DIALOG_SHOW_TIME_REMAINING_BOOLEAN`: If true, the dialog
 *   will show an estimate of the remaining time for the progress bar to finish.
 *   Defaults to false.
 * - `SDL_PROP_PROGRESS_DIALOG_AUTO_CLOSE_BOOLEAN`: If true, the dialog will be
 *   closed automatically when the progress bar reaches 100%. Defaults to false.
 *
 * Note that each platform may or may not support any of the properties.
 *
 * \param callback a function pointer to be invoked when the progress finishes,
 *                 successfully or not.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param props the properties to use.
 *
 * \returns a progress dialog handle.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.4.0.
 *
 * \sa SDL_DialogProgressCallback
 * \sa SDL_UpdateProgressDialog
 * \sa SDL_DestroyProgressDialog
 */
extern SDL_DECLSPEC SDL_ProgressDialog* SDLCALL SDL_ShowProgressDialogWithProperties(SDL_DialogProgressCallback callback, void *userdata, SDL_PropertiesID props);

#define SDL_PROP_PROGRESS_DIALOG_WINDOW_POINTER                 "SDL.progressdialog.window"
#define SDL_PROP_PROGRESS_DIALOG_TITLE_STRING                   "SDL.progressdialog.title"
#define SDL_PROP_PROGRESS_DIALOG_PROMPT_STRING                  "SDL.progressdialog.prompt"
#define SDL_PROP_PROGRESS_DIALOG_ACCEPT_STRING                  "SDL.progressdialog.accept"
#define SDL_PROP_PROGRESS_DIALOG_CANCEL_STRING                  "SDL.progressdialog.cancel"
#define SDL_PROP_PROGRESS_DIALOG_CAN_CANCEL_BOOLEAN             "SDL.progressdialog.can_cancel"
#define SDL_PROP_PROGRESS_DIALOG_INDEFINITE_BOOLEAN             "SDL.progressdialog.indefinite"
#define SDL_PROP_PROGRESS_DIALOG_SHOW_TIME_REMAINING_BOOLEAN    "SDL.progressdialog.show_time_remaining"
#define SDL_PROP_PROGRESS_DIALOG_AUTO_CLOSE_BOOLEAN             "SDL.progressdialog.auto_close"

/**
 * Update a progress dialog to a new progress status.
 *
 * For auto-closing progress dialogs, calling this function with a progress of 1
 * (corresponding to 100%) will close the dialog and invoke the callback with
 * success status. To close the dialog with error status, use
 * SDL_CloseProgressDialog() instead.
 *
 * For indefinite progress dialogs, this function has no effect unless progress
 * is 1, in which case it will report a successful completion.
 *
 * \param dialog the progress dialog.
 * \param progress the new progress for the dialog, which is a float between
 *                 0 and 1 inclusively.
 * \param new_prompt a new prompt, or NULL to keep the same prompt.
 *
 * \threadsafety This function is not thread-safe.
 *
 * \since This function is available since SDL 3.4.0.
 *
 * \sa SDL_ShowProgressDialogWithProperties
 * \sa SDL_DestroyProgressDialog
 */
extern SDL_DECLSPEC void SDLCALL SDL_UpdateProgressDialog(SDL_ProgressDialog* dialog, float progress, const char *new_prompt);

/**
 * Close and destroy a progress dialog, invoking the callback with error
 * status if it hasn't yet been called.
 *
 * It is safe to call this function while the dialog is still open; it will
 * abort the dialog if it hasn't already completed.
 *
 * \param dialog the dialog to be destroyed.
 *
 * \threadsafety This function is not thread-safe.
 *
 * \since This function is available since SDL 3.4.0.
 *
 * \sa SDL_ShowProgressDialogWithProperties
 * \sa SDL_UpdateProgressDialog
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyProgressDialog(SDL_ProgressDialog* dialog);

/**
 * Callback used by color dialog functions.
 *
 * \param userdata an app-provided pointer, for the callback's use.
 * \param color the color chosen by the user.
 * \param result the success status of the dialog.
 *
 * \since This datatype is available since SDL 3.4.0.
 *
 * \sa SDL_DialogResult
 * \sa SDL_ShowColorPickerDialogWithProperties
 */
typedef void (SDLCALL *SDL_DialogColorCallback)(void *userdata, SDL_Color color, SDL_DialogResult result);

/**
 * Create and launch a color picker dialog with the specified properties.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with the color chosen by the user, or black if
 * the dialog was canceled or an error occured.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_COLOR_DIALOG_WINDOW_POINTER`: the window that the dialog should
 *   be modal for.
 * - `SDL_PROP_COLOR_DIALOG_TITLE_STRING`: the title for the dialog.
 * - `SDL_PROP_COLOR_DIALOG_PROMPT_STRING`: the prompt for the dialog.
 * - `SDL_PROP_COLOR_DIALOG_DEFAULT_POINTER`: the default value for the dialog.
 *   Must be a pointer to SDL_Color; NULL has the same effect as not setting
 *   this property.
 * - `SDL_PROP_COLOR_DIALOG_ACCEPT_STRING`: the label that the accept button
 *   should have.
 * - `SDL_PROP_COLOR_DIALOG_CANCEL_STRING`: the label that the cancel button
 *   should have.
 *
 * Note that each platform may or may not support any of the properties.
 *
 * If the platform's native color picker does not support selecting an alpha
 * value, and a non-NULL default color is provided, the resulting color will
 * have the same alpha value as the default color. Otherwise, it will be opaque.
 *
 * \param callback a function pointer to be invoked when the user finishes
 *                 entering the prompt.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param props the properties to use.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.4.0.
 *
 * \sa SDL_Color
 * \sa SDL_DialogColorCallback
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowColorPickerDialogWithProperties(SDL_DialogColorCallback callback, void *userdata, SDL_PropertiesID props);

#define SDL_PROP_COLOR_DIALOG_WINDOW_POINTER      "SDL.colordialog.window"
#define SDL_PROP_COLOR_DIALOG_TITLE_STRING        "SDL.colordialog.title"
#define SDL_PROP_COLOR_DIALOG_PROMPT_STRING       "SDL.colordialog.prompt"
#define SDL_PROP_COLOR_DIALOG_DEFAULT_POINTER     "SDL.colordialog.default"
#define SDL_PROP_COLOR_DIALOG_ACCEPT_STRING       "SDL.colordialog.accept"
#define SDL_PROP_COLOR_DIALOG_CANCEL_STRING       "SDL.colordialog.cancel"

typedef struct SDL_Date
{
    Uint16 y;
    Uint8 m;
    Uint8 d;
} SDL_Date;

/**
 * Callback used by date picker dialog functions.
 *
 * \param userdata an app-provided pointer, for the callback's use.
 * \param date the date chosen by the user.
 * \param result the success status of the dialog.
 *
 * \since This datatype is available since SDL 3.4.0.
 *
 * \sa SDL_DialogResult
 * \sa SDL_ShowDatePickerDialogWithProperties
 */
typedef void (SDLCALL *SDL_DialogDateCallback)(void *userdata, SDL_Date date, SDL_DialogResult result);

/**
 * Create and launch a date picker dialog with the specified properties.
 *
 * This function only supports dates from the Gregorian calendar, with years
 * 1-9999, months 1-12 and days 1-31. Field can be set to 0 to retain the
 * default value.
 *
 * This is an asynchronous function; it will return immediately, and the
 * result will be passed to the callback.
 *
 * The callback will be invoked with the date chosen by the user, or { 0, 0, 0 }
 * if the dialog was canceled or an error occured.
 *
 * Note that the callback may be called from a different thread than the one
 * the function was invoked on.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_DATE_DIALOG_WINDOW_POINTER`: the window that the dialog should
 *   be modal for.
 * - `SDL_PROP_DATE_DIALOG_TITLE_STRING`: the title for the dialog.
 * - `SDL_PROP_DATE_DIALOG_PROMPT_STRING`: the prompt for the dialog.
 * - `SDL_PROP_DATE_DIALOG_DEFAULT_POINTER`: the default value for the dialog.
 *   Should be a pointer to SDL_Date. Fields can be set to 0 to keep the default
 *   value.
 * - `SDL_PROP_DATE_DIALOG_ACCEPT_STRING`: the label that the accept button
 *   should have.
 * - `SDL_PROP_DATE_DIALOG_CANCEL_STRING`: the label that the cancel button
 *   should have.
 *
 * Note that each platform may or may not support any of the properties.
 *
 * \param callback a function pointer to be invoked when the user finishes
 *                 entering the prompt.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param props the properties to use.
 *
 * \threadsafety This function should be called only from the main thread. The
 *               callback may be invoked from the same thread or from a
 *               different one, depending on the OS's constraints.
 *
 * \since This function is available since SDL 3.4.0.
 *
 * \sa SDL_Date
 * \sa SDL_DialogDateCallback
 */
extern SDL_DECLSPEC void SDLCALL SDL_ShowDatePickerDialogWithProperties(SDL_DialogDateCallback callback, void *userdata, SDL_PropertiesID props);

#define SDL_PROP_DATE_DIALOG_WINDOW_POINTER      "SDL.datedialog.window"
#define SDL_PROP_DATE_DIALOG_TITLE_STRING        "SDL.datedialog.title"
#define SDL_PROP_DATE_DIALOG_PROMPT_STRING       "SDL.datedialog.prompt"
#define SDL_PROP_DATE_DIALOG_DEFAULT_POINTER     "SDL.datedialog.default"
#define SDL_PROP_DATE_DIALOG_ACCEPT_STRING       "SDL.datedialog.accept"
#define SDL_PROP_DATE_DIALOG_CANCEL_STRING       "SDL.datedialog.cancel"

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_dialog_h_ */
