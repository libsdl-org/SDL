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

#include "../SDL_dialog_utils.h"

#define X11_HANDLE_MAX_WIDTH 28
typedef struct
{
    SDL_DialogFileCallback callback;
    void *userdata;
    void *argv;

    /* Zenity only works with X11 handles apparently */
    char x11_window_handle[X11_HANDLE_MAX_WIDTH];
    /* These are part of argv, but are tracked separately for deallocation purposes */
    int nfilters;
    char **filters_slice;
    char *filename;
    char *title;
    char *accept;
    char *cancel;
} zenityArgs;

static char *zenity_clean_name(const char *name)
{
    char *newname = SDL_strdup(name);

    /* Filter out "|", which Zenity considers a special character. Let's hope
       there aren't others. TODO: find something better. */
    for (char *c = newname; *c; c++) {
        if (*c == '|') {
            // Zenity doesn't support escaping with '\'
            *c = '/';
        }
    }

    return newname;
}

static bool get_x11_window_handle(SDL_PropertiesID props, char *out)
{
    SDL_Window *window = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, NULL);
    if (!window) {
        return false;
    }
    SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
    if (!window_props) {
        return false;
    }
    Uint64 handle = (Uint64)SDL_GetNumberProperty(window_props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (!handle) {
        return false;
    }
    if (SDL_snprintf(out, X11_HANDLE_MAX_WIDTH, "0x%" SDL_PRIx64, handle) >= X11_HANDLE_MAX_WIDTH) {
        return false;
    };
    return true;
}

/* Exec call format:
 *
 *     zenity --file-selection --separator=\n [--multiple]
 *                         [--directory] [--save --confirm-overwrite]
 *                         [--filename FILENAME] [--modal --attach 0x11w1nd0w]
 *                         [--title TITLE] [--ok-label ACCEPT]
 *                         [--cancel-label CANCEL]
 *                         [--file-filter=Filter Name | *.filt *.fn ...]...
 */
static zenityArgs *create_zenity_args(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityArgs *args = SDL_calloc(1, sizeof(*args));
    if (!args) {
        return NULL;
    }
    args->callback = callback;
    args->userdata = userdata;
    args->nfilters = SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0);

    const char **argv = SDL_malloc(
        sizeof(*argv) * (3   /* zenity --file-selection --separator=\n */
                         + 1 /* --multiple */
                         + 2 /* --directory | --save --confirm-overwrite */
                         + 2 /* --filename [file] */
                         + 3 /* --modal --attach [handle] */
                         + 2 /* --title [title] */
                         + 2 /* --ok-label [label] */
                         + 2 /* --cancel-label [label] */
                         + args->nfilters + 1 /* NULL */));
    if (!argv) {
        goto cleanup;
    }
    args->argv = argv;

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
#define COPY_STRING_PROPERTY(dst, prop)                             \
    {                                                               \
        const char *str = SDL_GetStringProperty(props, prop, NULL); \
        if (str) {                                                  \
            dst = SDL_strdup(str);                                  \
            if (!dst) {                                             \
                goto cleanup;                                       \
            }                                                       \
        }                                                           \
    }

    COPY_STRING_PROPERTY(args->filename, SDL_PROP_FILE_DIALOG_LOCATION_STRING);
    COPY_STRING_PROPERTY(args->title, SDL_PROP_FILE_DIALOG_TITLE_STRING);
    COPY_STRING_PROPERTY(args->accept, SDL_PROP_FILE_DIALOG_ACCEPT_STRING);
    COPY_STRING_PROPERTY(args->cancel, SDL_PROP_FILE_DIALOG_CANCEL_STRING);
#undef COPY_STRING_PROPERTY

    // ARGV PASS
    int argc = 0;
    argv[argc++] = "zenity";
    argv[argc++] = "--file-selection";
    argv[argc++] = "--separator=\n";

    if (SDL_GetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false)) {
        argv[argc++] = "--multiple";
    }

    switch (type) {
    case SDL_FILEDIALOG_OPENFILE:
        break;

    case SDL_FILEDIALOG_SAVEFILE:
        argv[argc++] = "--save";
        /* Asking before overwriting while saving seems like a sane default */
        argv[argc++] = "--confirm-overwrite";
        break;

    case SDL_FILEDIALOG_OPENFOLDER:
        argv[argc++] = "--directory";
        break;
    };

    if (args->filename) {
        argv[argc++] = "--filename";
        argv[argc++] = args->filename;
    }

    if (get_x11_window_handle(props, args->x11_window_handle)) {
        argv[argc++] = "--modal";
        argv[argc++] = "--attach";
        argv[argc++] = args->x11_window_handle;
    }

    if (args->title) {
        argv[argc++] = "--title";
        argv[argc++] = args->title;
    }

    if (args->accept) {
        argv[argc++] = "--ok-label";
        argv[argc++] = args->accept;
    }

    if (args->cancel) {
        argv[argc++] = "--cancel-label";
        argv[argc++] = args->cancel;
    }

    const SDL_DialogFileFilter *filters = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, NULL);
    if (filters) {
        args->filters_slice = (char **)&argv[argc];
        for (int i = 0; i < args->nfilters; i++) {
            char *filter_str = convert_filter(filters[i],
                                              zenity_clean_name,
                                              "--file-filter=", " | ", "",
                                              "*.", " *.", "");

            if (!filter_str) {
                while (i--) {
                    SDL_free(args->filters_slice[i]);
                }
                goto cleanup;
            }

            args->filters_slice[i] = filter_str;
        }
        argc += args->nfilters;
    }

    argv[argc] = NULL;
    return args;

cleanup:
    SDL_free(args->filename);
    SDL_free(args->title);
    SDL_free(args->accept);
    SDL_free(args->cancel);
    SDL_free(argv);
    SDL_free(args);
    return NULL;
}

// TODO: Zenity survives termination of the parent

static char *exec_zenity(void *argv, SDL_DialogResult* dialog_result, size_t* bytes)
{
    SDL_Process *process = NULL;
    SDL_Environment *env = NULL;
    int status = -1;
    char *output = NULL;

    *dialog_result = SDL_DIALOGRESULT_ERROR;

    env = SDL_CreateEnvironment(true);
    if (!env) {
        goto done;
    }

    /* Recent versions of Zenity have different exit codes, but picks up
      different codes from the environment */
    SDL_SetEnvironmentVariable(env, "ZENITY_OK", "0", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_CANCEL", "1", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_ESC", "1", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_EXTRA", "2", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_ERROR", "2", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_TIMEOUT", "2", true);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        goto done;
    }

    output = SDL_ReadProcess(process, bytes, &status);
    if (!output) {
        goto done;
    }

    // 0 = dialog completed, 1 = dialog canceled, other = error
    if (status == 0) {
        *dialog_result = SDL_DIALOGRESULT_SUCCESS;
    } else if (status == 1) {
        *dialog_result = SDL_DIALOGRESULT_CANCEL;
    } else {
        SDL_SetError("Could not run zenity: exit code %d", status);
        *dialog_result = SDL_DIALOGRESULT_ERROR;
    }

done:
    SDL_DestroyEnvironment(env);
    SDL_DestroyProcess(process);

    return output;
}

static void run_zenity(SDL_DialogFileCallback callback, void *userdata, void *argv)
{
    SDL_DialogResult dialog_result;
    size_t bytes_read = 0;
    char *container = NULL;
    size_t narray = 1;
    char **array = NULL;
    bool result = false;

    container = exec_zenity(argv, &dialog_result, &bytes_read);
    if (dialog_result == SDL_DIALOGRESULT_ERROR) {
        goto done;
    }

    array = (char **)SDL_malloc((narray + 1) * sizeof(char *));
    if (!array) {
        goto done;
    }
    array[0] = container;
    array[1] = NULL;

    for (int i = 0; i < bytes_read; i++) {
        if (container[i] == '\n') {
            container[i] = '\0';
            // Reading from a process often leaves a trailing \n, so ignore the last one
            if (i < bytes_read - 1) {
                array[narray] = container + i + 1;
                narray++;
                char **new_array = (char **)SDL_realloc(array, (narray + 1) * sizeof(char *));
                if (!new_array) {
                    goto done;
                }
                array = new_array;
                array[narray] = NULL;
            }
        }
    }

    callback(userdata, (const char *const *)array, -1);

    result = true;

done:
    SDL_free(array);
    SDL_free(container);

    if (!result) {
        callback(userdata, NULL, -1);
    }
}

static void free_zenity_args(zenityArgs *args)
{
    if (args->filters_slice) {
        for (int i = 0; i < args->nfilters; i++) {
            SDL_free(args->filters_slice[i]);
        }
    }
    SDL_free(args->filename);
    SDL_free(args->title);
    SDL_free(args->accept);
    SDL_free(args->cancel);
    SDL_free(args->argv);
    SDL_free(args);
}

static int run_zenity_thread(void *ptr)
{
    zenityArgs *args = ptr;
    run_zenity(args->callback, args->userdata, args->argv);
    free_zenity_args(args);
    return 0;
}

void SDL_Zenity_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityArgs *args = create_zenity_args(type, callback, userdata, props);
    if (!args) {
        callback(userdata, NULL, -1);
        return;
    }

    SDL_Thread *thread = SDL_CreateThread(run_zenity_thread, "SDL_ZenityFileDialog", (void *)args);

    if (!thread) {
        free_zenity_args(args);
        callback(userdata, NULL, -1);
        return;
    }

    SDL_DetachThread(thread);
}

bool SDL_Zenity_detect(void)
{
    const char *args[] = {
        "zenity", "--version", NULL
    };
    int status = -1;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_Process *process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (process) {
        SDL_WaitProcess(process, true, &status);
        SDL_DestroyProcess(process);
    }
    return (status == 0);
}

static void run_input_zenity(SDL_DialogInputCallback callback, void *userdata, void *argv)
{
    SDL_DialogResult dialog_result;
    size_t bytes_read = 0;
    char *input = NULL;
    bool result = false;

    input = exec_zenity(argv, &dialog_result, &bytes_read);
    if (!input) {
        goto done;
    }

    for (int i = 0; i < bytes_read; i++) {
        if (input[i] == '\n') {
            input[i] = '\0';
            break;
        }
    }

    callback(userdata, input, dialog_result);
    result = true;

done:
    SDL_free(input);

    if (!result) {
        callback(userdata, NULL, SDL_DIALOGRESULT_ERROR);
    }
}

typedef struct
{
    SDL_DialogInputCallback callback;
    void *userdata;
    void *argv;

    char *title;
    char *prompt;
    char *default_val;
    char *accept;
    char *cancel;
} zenityInputArgs;

static void free_zenity_input_args(zenityInputArgs *args)
{
    SDL_free(args->title);
    SDL_free(args->prompt);
    SDL_free(args->default_val);
    SDL_free(args->accept);
    SDL_free(args->cancel);
    SDL_free(args->argv);
    SDL_free(args);
}

static zenityInputArgs *create_zenity_input_args(SDL_DialogInputCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityInputArgs *args = SDL_calloc(1, sizeof(zenityInputArgs));
    if (!args) {
        return NULL;
    }

    const char **argv = SDL_calloc(sizeof(*argv),
                                   1 /* zenity */
                                   + 1 /* --entry | --password */
                                   + 2 /* --title [title] */
                                   + 2 /* --text [prompt] */
                                   + 2 /* --entry-text [default_val] */
                                   + 2 /* --ok-label [label] */
                                   + 2 /* --cancel-label [label] */
                                   + 1 /* NULL */);

    if (!argv) {
        goto cleanup;
    }

    args->argv = argv;
    args->callback = callback;
    args->userdata = userdata;

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
#define COPY_STRING_PROPERTY(dst, prop)                             \
    {                                                               \
        const char *str = SDL_GetStringProperty(props, prop, NULL); \
        if (str) {                                                  \
            dst = SDL_strdup(str);                                  \
            if (!dst) {                                             \
                goto cleanup;                                       \
            }                                                       \
        }                                                           \
    }

    COPY_STRING_PROPERTY(args->title, SDL_PROP_INPUT_DIALOG_TITLE_STRING);
    COPY_STRING_PROPERTY(args->prompt, SDL_PROP_INPUT_DIALOG_PROMPT_STRING);
    COPY_STRING_PROPERTY(args->default_val, SDL_PROP_INPUT_DIALOG_DEFAULT_STRING);
    COPY_STRING_PROPERTY(args->accept, SDL_PROP_INPUT_DIALOG_ACCEPT_STRING);
    COPY_STRING_PROPERTY(args->cancel, SDL_PROP_INPUT_DIALOG_CANCEL_STRING);
#undef COPY_STRING_PROPERTY

    int argc = 0;
    argv[argc++] = "zenity";

    if (SDL_GetBooleanProperty(props, SDL_PROP_INPUT_DIALOG_PASSWORD_BOOLEAN, false)) {
        argv[argc++] = "--password";
    } else {
        argv[argc++] = "--entry";
    }

    if (args->title) {
        argv[argc++] = "--title";
        argv[argc++] = args->title;
    }

    if (args->prompt) {
        argv[argc++] = "--text";
        argv[argc++] = args->prompt;
    }

    if (args->default_val) {
        argv[argc++] = "--entry-text";
        argv[argc++] = args->default_val;
    }

    if (args->accept) {
        argv[argc++] = "--ok-label";
        argv[argc++] = args->accept;
    }

    if (args->cancel) {
        argv[argc++] = "--cancel-label";
        argv[argc++] = args->cancel;
    }

    argv[argc++] = NULL;
    return args;

cleanup:
    free_zenity_input_args(args);
    return NULL;
}

static int run_zenity_input_thread(void *ptr)
{
    zenityInputArgs *args = ptr;
    run_input_zenity(args->callback, args->userdata, args->argv);
    free_zenity_input_args(args);
    return 0;
}

void SDL_SYS_ShowInputDialogWithProperties(SDL_DialogInputCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityInputArgs *args = create_zenity_input_args(callback, userdata, props);
    if (!args) {
        callback(userdata, NULL, SDL_DIALOGRESULT_ERROR);
        return;
    }

    SDL_Thread *thread = SDL_CreateThread(run_zenity_input_thread, "SDL_ZenityInputDialog", (void *)args);

    if (!thread) {
        free_zenity_input_args(args);
        callback(userdata, NULL, SDL_DIALOGRESULT_ERROR);
        return;
    }

    SDL_DetachThread(thread);
}

struct SDL_ProgressDialog
{
    SDL_Process* proc;

    // SDL_ProgressDialog will be returned before the SDL_Process is created;
    // those variables handle the deferring of updating the progress value.
    enum {
        SDL_ZENITY_NOTSTARTED = 0,
        SDL_ZENITY_STARTING,
        SDL_ZENITY_RUNNING
    } status;
    bool cancel;
    // The mutex is used for:
    // - Controlling early calls to SDL_UpdateProgressDialog while the process
    //   is launching; and
    // - Controlling when the thread and the caller wish to release this struct
    //   at the same time.
    SDL_Mutex* mutex;
    bool destroyed_by_caller; // When the thread wishes to destroy this struct,
                              // it will set `proc` to NULL.
    float deferred_set_progress;
    char *deferred_set_prompt;
};

SDL_ProgressDialog* create_zenity_progress_info(void)
{
    SDL_ProgressDialog* dialog = SDL_calloc(1, sizeof(SDL_ProgressDialog));

    if (!dialog) {
        return NULL;
    }

    dialog->mutex = SDL_CreateMutex();

    if (!dialog->mutex) {
        SDL_free(dialog);
        return NULL;
    }

    return dialog;
}

void free_zenity_progress_info(SDL_ProgressDialog* dialog)
{
    SDL_DestroyMutex(dialog->mutex);

    if (dialog->proc) {
        SDL_KillProcess(dialog->proc, true);
        SDL_DestroyProcess(dialog->proc);
    }

    SDL_free(dialog);
}

void zenity_set_dialog_status(SDL_ProgressDialog* dialog, float progress, const char *new_prompt)
{
    int size;
    char *str = NULL;
    int written;

    if (!dialog || !dialog->proc || progress < 0.0f || progress > 1.0f) {
        return;
    }

    // "# " + new_prompt + "\n" + "100\n\0"
    size = (new_prompt ? SDL_strlen(new_prompt) + 3 : 0) + 5;
    str = SDL_malloc(size);

    if (!str) {
        goto done;
    }

    SDL_IOStream* in = SDL_GetProcessInput(dialog->proc);

    if (!in) {
        goto done;
    }

    if (new_prompt) {
        written = SDL_snprintf(str, size, "# %s\n%d\n", new_prompt, (int) SDL_roundf(progress * 100.0f));
    } else {
        written = SDL_snprintf(str, size, "%d\n", (int) SDL_roundf(progress * 100.0f));
    }

    if (written < size - 3 || written > size - 1) {
        goto done;
    }

    SDL_WriteIO(in, str, written);
    // FIXME: Can't flush stdin?
    //SDL_FlushIO(in);

done:
    SDL_free(str);
}

static void run_progress_zenity(SDL_DialogProgressCallback callback, void *userdata, SDL_ProgressDialog* dialog, void *argv)
{
    SDL_Environment *env = NULL;
    SDL_Process *process = NULL;
    int exitcode = -1;

    env = SDL_CreateEnvironment(true);
    if (!env) {
        goto done;
    }

    /* Recent versions of Zenity have different exit codes, but picks up
      different codes from the environment */
    SDL_SetEnvironmentVariable(env, "ZENITY_OK", "0", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_CANCEL", "1", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_ESC", "1", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_EXTRA", "2", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_ERROR", "2", true);
    SDL_SetEnvironmentVariable(env, "ZENITY_TIMEOUT", "2", true);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        goto done;
    }

    dialog->proc = process;

    SDL_LockMutex(dialog->mutex);
    dialog->status = SDL_ZENITY_STARTING;

    if (dialog->destroyed_by_caller) {
        // Force because Zenity ignores signals otherwise
        SDL_KillProcess(process, true);
    } else {
        zenity_set_dialog_status(dialog, dialog->deferred_set_progress, dialog->deferred_set_prompt);
    }
    SDL_free(dialog->deferred_set_prompt);

    dialog->status = SDL_ZENITY_RUNNING;
    SDL_UnlockMutex(dialog->mutex);

    SDL_WaitProcess(dialog->proc, true, &exitcode);

    if (dialog->cancel || exitcode == 1) {
        callback(userdata, SDL_DIALOGRESULT_CANCEL);
    } else if (exitcode) {
        callback(userdata, SDL_DIALOGRESULT_ERROR);
    } else {
        callback(userdata, SDL_DIALOGRESULT_SUCCESS);
    }

done:
    // SDL_ProcessDialog* can be destroyed either by this thread or by the
    // SDL_DestroyProgressDialog function, whichever happens last.
    SDL_LockMutex(dialog->mutex);
    SDL_DestroyProcess(process);
    dialog->proc = NULL;
    if (dialog->destroyed_by_caller) {
        // The caller has already finished with the dialog, so it's safe to
        // unlock it before we're done.
        SDL_UnlockMutex(dialog->mutex);
        free_zenity_progress_info(dialog);
    } else {
        // In the condition above, the mutex will have been destroyed, so it's
        // no longer safe to unlock it.
        SDL_UnlockMutex(dialog->mutex);
    }

    SDL_DestroyEnvironment(env);
}

typedef struct
{
    SDL_DialogProgressCallback callback;
    void *userdata;
    void *argv;

    char *title;
    char *prompt;
    char *default_val;
    char *accept;
    char *cancel;

    SDL_ProgressDialog* dialog_info;
} zenityProgressArgs;

static void free_zenity_progress_args(zenityProgressArgs *args)
{
    SDL_free(args->title);
    SDL_free(args->prompt);
    SDL_free(args->default_val);
    SDL_free(args->accept);
    SDL_free(args->cancel);
    SDL_free(args->argv);
    // Don't free args->dialog_info; this will be freed by the caller
    SDL_free(args);
}

static zenityProgressArgs *create_zenity_progress_args(SDL_DialogProgressCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityProgressArgs *args = SDL_calloc(1, sizeof(zenityProgressArgs));
    if (!args) {
        return NULL;
    }

    const char **argv = SDL_calloc(sizeof(*argv),
                                   2 /* zenity --progress */
                                   + 2 /* --title [title] */
                                   + 2 /* --text [prompt] */
                                   + 2 /* --ok-label [label] */
                                   + 2 /* --cancel-label [label] */
                                   + 4 /* --no-cancel --pulsate --time-remaining --auto-close */
                                   + 1 /* NULL */);

    if (!argv) {
        goto cleanup;
    }

    args->argv = argv;
    args->callback = callback;
    args->userdata = userdata;

    args->dialog_info = create_zenity_progress_info();
    if (!args->dialog_info) {
        goto cleanup;
    }

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
#define COPY_STRING_PROPERTY(dst, prop)                             \
    {                                                               \
        const char *str = SDL_GetStringProperty(props, prop, NULL); \
        if (str) {                                                  \
            dst = SDL_strdup(str);                                  \
            if (!dst) {                                             \
                goto cleanup;                                       \
            }                                                       \
        }                                                           \
    }

    COPY_STRING_PROPERTY(args->title, SDL_PROP_INPUT_DIALOG_TITLE_STRING);
    COPY_STRING_PROPERTY(args->prompt, SDL_PROP_INPUT_DIALOG_PROMPT_STRING);
    COPY_STRING_PROPERTY(args->default_val, SDL_PROP_INPUT_DIALOG_DEFAULT_STRING);
    COPY_STRING_PROPERTY(args->accept, SDL_PROP_INPUT_DIALOG_ACCEPT_STRING);
    COPY_STRING_PROPERTY(args->cancel, SDL_PROP_INPUT_DIALOG_CANCEL_STRING);
#undef COPY_STRING_PROPERTY

    int argc = 0;
    argv[argc++] = "zenity";
    argv[argc++] = "--progress";

    if (args->title) {
        argv[argc++] = "--title";
        argv[argc++] = args->title;
    }

    if (args->prompt) {
        argv[argc++] = "--text";
        argv[argc++] = args->prompt;
    }

    if (args->accept) {
        argv[argc++] = "--ok-label";
        argv[argc++] = args->accept;
    }

    if (args->cancel) {
        argv[argc++] = "--cancel-label";
        argv[argc++] = args->cancel;
    }

    if (!SDL_GetBooleanProperty(props, SDL_PROP_PROGRESS_DIALOG_CAN_CANCEL_BOOLEAN, true)) {
        argv[argc++] = "--no-cancel";
    }

    if (SDL_GetBooleanProperty(props, SDL_PROP_PROGRESS_DIALOG_INDEFINITE_BOOLEAN, false)) {
        argv[argc++] = "--pulsate";
    }

    if (SDL_GetBooleanProperty(props, SDL_PROP_PROGRESS_DIALOG_SHOW_TIME_REMAINING_BOOLEAN, false)) {
        argv[argc++] = "--time-remaining";
    }

    if (SDL_GetBooleanProperty(props, SDL_PROP_PROGRESS_DIALOG_AUTO_CLOSE_BOOLEAN, false)) {
        argv[argc++] = "--auto-close";
    }

    argv[argc++] = NULL;
    return args;

cleanup:
    free_zenity_progress_args(args);
    return NULL;
}

static int run_zenity_progress_thread(void *ptr)
{
    zenityProgressArgs *args = ptr;
    run_progress_zenity(args->callback, args->userdata, args->dialog_info, args->argv);
    free_zenity_progress_args(args);
    return 0;
}

SDL_ProgressDialog* SDL_SYS_ShowProgressDialogWithProperties(SDL_DialogProgressCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityProgressArgs *args = create_zenity_progress_args(callback, userdata, props);
    if (!args) {
        callback(userdata, SDL_DIALOGRESULT_ERROR);
        return NULL;
    }

    SDL_Thread *thread = SDL_CreateThread(run_zenity_progress_thread, "SDL_ZenityProgressDialog", (void *)args);

    if (!thread) {
        free_zenity_progress_args(args);
        callback(userdata, SDL_DIALOGRESULT_ERROR);
        return NULL;
    }

    SDL_DetachThread(thread);

    return args->dialog_info;
}

void SDL_SYS_UpdateProgressDialog(SDL_ProgressDialog* dialog, float progress, const char *new_prompt)
{
    if (!dialog->proc) {
        // The dialog has already finished.
        return;
    }

    // Ordering is important to avoid race conditions
    dialog->deferred_set_progress = progress;
    if (new_prompt) {
        dialog->deferred_set_prompt = SDL_strdup(new_prompt);
    }

    // If Zenity hasn't yet started, the value will be picked up when it will
    // have started.
    if (dialog->status == SDL_ZENITY_STARTING)
    {
        SDL_LockMutex(dialog->mutex);
        zenity_set_dialog_status(dialog, progress, new_prompt);
        SDL_UnlockMutex(dialog->mutex);
    }
    else if (dialog->status == SDL_ZENITY_RUNNING)
    {
        zenity_set_dialog_status(dialog, progress, new_prompt);
    }
}

void SDL_SYS_DestroyProgressDialog(SDL_ProgressDialog* dialog)
{
    dialog->cancel = true;

    // SDL_ProcessDialog* can be destroyed either by the thread or by this
    // function, whichever happens last.
    SDL_LockMutex(dialog->mutex);
    dialog->destroyed_by_caller = true;

    if (dialog->status == SDL_ZENITY_NOTSTARTED) {
        // The program will never be started
        SDL_UnlockMutex(dialog->mutex);
    } else if (dialog->proc) {
        // Force-kill, because Zenity seemingly ignores killing otherwise
        SDL_KillProcess(dialog->proc, true);
        SDL_UnlockMutex(dialog->mutex);
    } else {
        // The thread has already finished with the dialog, so it's safe to
        // unlock it before we're done.
        SDL_UnlockMutex(dialog->mutex);
        free_zenity_progress_info(dialog);
    }
}

static void run_color_zenity(SDL_DialogColorCallback callback, void *userdata, void *argv)
{
    SDL_DialogResult dialog_result;
    size_t bytes_read = 0;
    char *input = NULL;
    bool result = false;
    SDL_Color color;
    int r, g, b;
    float a;

    color.r = 0;
    color.g = 0;
    color.b = 0;
    color.a = 0;

    input = exec_zenity(argv, &dialog_result, &bytes_read);

    if (!input) {
        goto done;
    }

    if (dialog_result == SDL_DIALOGRESULT_SUCCESS) {
        if (SDL_sscanf(input, "rgba(%d,%d,%d,%f)", &r, &g, &b, &a) == 4) {
            color.r = r;
            color.g = g;
            color.b = b;
            color.a = (Uint8) (a * 255.0f);
        } else if (SDL_sscanf(input, "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
            color.r = r;
            color.g = g;
            color.b = b;
            color.a = SDL_ALPHA_OPAQUE;
        } else {
            SDL_SetError("Unexpected rgb format from Zenity: %s", input);
        }
    }

    callback(userdata, color, dialog_result);
    result = true;

done:
    SDL_free(input);

    if (!result) {
        callback(userdata, color, SDL_DIALOGRESULT_ERROR);
    }
}

typedef struct
{
    SDL_DialogColorCallback callback;
    void *userdata;
    void *argv;

    char *title;
    char *prompt;
    char *default_val;
    char *accept;
    char *cancel;
} zenityColorArgs;

static void free_zenity_color_args(zenityColorArgs *args)
{
    SDL_free(args->title);
    SDL_free(args->prompt);
    SDL_free(args->default_val);
    SDL_free(args->accept);
    SDL_free(args->cancel);
    SDL_free(args->argv);
    SDL_free(args);
}

static zenityColorArgs *create_zenity_color_args(SDL_DialogColorCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityColorArgs *args = SDL_calloc(1, sizeof(zenityColorArgs));
    if (!args) {
        return NULL;
    }

    const char **argv = SDL_calloc(sizeof(*argv),
                                   2 /* zenity --color-selection */
                                   + 2 /* --title [title] */
                                   + 2 /* --text [prompt] */
                                   + 2 /* --color [default_val] */
                                   + 2 /* --ok-label [label] */
                                   + 2 /* --cancel-label [label] */
                                   + 1 /* NULL */);

    if (!argv) {
        goto cleanup;
    }

    args->argv = argv;
    args->callback = callback;
    args->userdata = userdata;

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
#define COPY_STRING_PROPERTY(dst, prop)                             \
    {                                                               \
        const char *str = SDL_GetStringProperty(props, prop, NULL); \
        if (str) {                                                  \
            dst = SDL_strdup(str);                                  \
            if (!dst) {                                             \
                goto cleanup;                                       \
            }                                                       \
        }                                                           \
    }

    COPY_STRING_PROPERTY(args->title, SDL_PROP_COLOR_DIALOG_TITLE_STRING);
    COPY_STRING_PROPERTY(args->prompt, SDL_PROP_COLOR_DIALOG_PROMPT_STRING);
    COPY_STRING_PROPERTY(args->accept, SDL_PROP_COLOR_DIALOG_ACCEPT_STRING);
    COPY_STRING_PROPERTY(args->cancel, SDL_PROP_COLOR_DIALOG_CANCEL_STRING);
#undef COPY_STRING_PROPERTY

    int argc = 0;
    argv[argc++] = "zenity";
    argv[argc++] = "--color-selection";

    if (args->title) {
        argv[argc++] = "--title";
        argv[argc++] = args->title;
    }

    if (args->prompt) {
        argv[argc++] = "--text";
        argv[argc++] = args->prompt;
    }

    SDL_Color *col = SDL_GetPointerProperty(props, SDL_PROP_COLOR_DIALOG_DEFAULT_POINTER, NULL);
    if (col) {
        args->default_val = SDL_malloc(32 * sizeof(char)); // "rgba(255,255,255,0.99999999999)" + '\0'

        if (!args->default_val) {
            goto cleanup;
        }

        int written = SDL_snprintf(args->default_val, 32 * sizeof(char), "rgba(%d,%d,%d,%f)", col->r, col->g, col->b, ((float) col->a) / 255.0f);

        if (written < 13 || written > 32) {
            goto cleanup;
        }

        argv[argc++] = "--color";
        argv[argc++] = args->default_val;
    }

    if (args->accept) {
        argv[argc++] = "--ok-label";
        argv[argc++] = args->accept;
    }

    if (args->cancel) {
        argv[argc++] = "--cancel-label";
        argv[argc++] = args->cancel;
    }

    argv[argc++] = NULL;
    return args;

cleanup:
    free_zenity_color_args(args);
    return NULL;
}

static int run_zenity_color_thread(void *ptr)
{
    zenityColorArgs *args = ptr;
    run_color_zenity(args->callback, args->userdata, args->argv);
    free_zenity_color_args(args);
    return 0;
}

void SDL_SYS_ShowColorPickerDialogWithProperties(SDL_DialogColorCallback callback, void *userdata, SDL_PropertiesID props)
{
    SDL_Color dummy;
    dummy.r = 0;
    dummy.g = 0;
    dummy.b = 0;
    dummy.a = 0;

    zenityColorArgs *args = create_zenity_color_args(callback, userdata, props);
    if (!args) {
        callback(userdata, dummy, SDL_DIALOGRESULT_ERROR);
        return;
    }

    SDL_Thread *thread = SDL_CreateThread(run_zenity_color_thread, "SDL_ZenityColorDialog", (void *)args);

    if (!thread) {
        free_zenity_color_args(args);
        callback(userdata, dummy, SDL_DIALOGRESULT_ERROR);
        return;
    }

    SDL_DetachThread(thread);
}

static void run_date_zenity(SDL_DialogDateCallback callback, void *userdata, void *argv)
{
    SDL_DialogResult dialog_result;
    size_t bytes_read = 0;
    char *input = NULL;
    bool result = false;
    SDL_Date date;
    int y, m, d;

    date.y = 0;
    date.m = 0;
    date.d = 0;

    input = exec_zenity(argv, &dialog_result, &bytes_read);

    if (!input) {
        goto done;
    }

    if (dialog_result == SDL_DIALOGRESULT_SUCCESS) {
        if (SDL_sscanf(input, "%d-%d-%d", &y, &m, &d) == 3) {
            date.y = y;
            date.m = m;
            date.d = d;
        } else {
            SDL_SetError("Unexpected date format from Zenity: %s", input);
        }
    }

    callback(userdata, date, dialog_result);
    result = true;

done:
    SDL_free(input);

    if (!result) {
        callback(userdata, date, SDL_DIALOGRESULT_ERROR);
    }
}

typedef struct
{
    SDL_DialogDateCallback callback;
    void *userdata;
    void *argv;

    char *title;
    char *prompt;
    char *default_day;
    char *default_month;
    char *default_year;
    char *accept;
    char *cancel;
} zenityDateArgs;

static void free_zenity_date_args(zenityDateArgs *args)
{
    SDL_free(args->title);
    SDL_free(args->prompt);
    SDL_free(args->default_day);
    SDL_free(args->default_month);
    SDL_free(args->default_year);
    SDL_free(args->accept);
    SDL_free(args->cancel);
    SDL_free(args->argv);
    SDL_free(args);
}

static zenityDateArgs *create_zenity_date_args(SDL_DialogDateCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityDateArgs *args = SDL_calloc(1, sizeof(zenityDateArgs));
    if (!args) {
        return NULL;
    }

    const char **argv = SDL_calloc(sizeof(*argv),
                                   2 /* zenity --color-selection */
                                   + 2 /* --title [title] */
                                   + 2 /* --text [prompt] */
                                   + 6 /* --year [y] --month [m] --day [d] */
                                   + 2 /* --ok-label [label] */
                                   + 2 /* --cancel-label [label] */
                                   + 1 /* NULL */);

    if (!argv) {
        goto cleanup;
    }

    args->argv = argv;
    args->callback = callback;
    args->userdata = userdata;

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
#define COPY_STRING_PROPERTY(dst, prop)                             \
    {                                                               \
        const char *str = SDL_GetStringProperty(props, prop, NULL); \
        if (str) {                                                  \
            dst = SDL_strdup(str);                                  \
            if (!dst) {                                             \
                goto cleanup;                                       \
            }                                                       \
        }                                                           \
    }

    COPY_STRING_PROPERTY(args->title, SDL_PROP_DATE_DIALOG_TITLE_STRING);
    COPY_STRING_PROPERTY(args->prompt, SDL_PROP_DATE_DIALOG_PROMPT_STRING);
    COPY_STRING_PROPERTY(args->accept, SDL_PROP_DATE_DIALOG_ACCEPT_STRING);
    COPY_STRING_PROPERTY(args->cancel, SDL_PROP_DATE_DIALOG_CANCEL_STRING);
#undef COPY_STRING_PROPERTY

    int argc = 0;
    argv[argc++] = "zenity";
    argv[argc++] = "--calendar";

    if (args->title) {
        argv[argc++] = "--title";
        argv[argc++] = args->title;
    }

    if (args->prompt) {
        argv[argc++] = "--text";
        argv[argc++] = args->prompt;
    }

    SDL_Date *date = SDL_GetPointerProperty(props, SDL_PROP_DATE_DIALOG_DEFAULT_POINTER, NULL);
    if (date) {
        if (date->y != 0) {
            args->default_year = SDL_malloc(5 * sizeof(char)); // "9999" + '\0' (Zenity doesn't support years past 9999)

            if (!args->default_year) {
                goto cleanup;
            }

            int written = SDL_snprintf(args->default_year, 5 * sizeof(char), "%d", date->y);

            if (written < 1 || written > 4) {
                goto cleanup;
            }

            argv[argc++] = "--year";
            argv[argc++] = args->default_year;
        }

        if (date->m != 0) {
            args->default_month = SDL_malloc(3 * sizeof(char)); // "12" + '\0'

            if (!args->default_month) {
                goto cleanup;
            }

            int written = SDL_snprintf(args->default_month, 3 * sizeof(char), "%d", date->m);

            if (written < 1 || written > 2) {
                goto cleanup;
            }

            argv[argc++] = "--month";
            argv[argc++] = args->default_month;
        }

        if (date->d != 0) {
            args->default_day = SDL_malloc(3 * sizeof(char)); // "31" + '\0'

            if (!args->default_day) {
                goto cleanup;
            }

            int written = SDL_snprintf(args->default_day, 3 * sizeof(char), "%d", date->d);

            if (written < 1 || written > 2) {
                goto cleanup;
            }

            argv[argc++] = "--day";
            argv[argc++] = args->default_day;
        }
    }

    if (args->accept) {
        argv[argc++] = "--ok-label";
        argv[argc++] = args->accept;
    }

    if (args->cancel) {
        argv[argc++] = "--cancel-label";
        argv[argc++] = args->cancel;
    }

    argv[argc++] = NULL;
    return args;

cleanup:
    free_zenity_date_args(args);
    return NULL;
}

static int run_zenity_date_thread(void *ptr)
{
    zenityDateArgs *args = ptr;
    run_date_zenity(args->callback, args->userdata, args->argv);
    free_zenity_date_args(args);
    return 0;
}

void SDL_SYS_ShowDatePickerDialogWithProperties(SDL_DialogDateCallback callback, void *userdata, SDL_PropertiesID props)
{
    SDL_Date dummy;
    dummy.y = 0;
    dummy.m = 0;
    dummy.d = 0;

    zenityDateArgs *args = create_zenity_date_args(callback, userdata, props);
    if (!args) {
        callback(userdata, dummy, SDL_DIALOGRESULT_ERROR);
        return;
    }

    SDL_Thread *thread = SDL_CreateThread(run_zenity_date_thread, "SDL_ZenityDateDialog", (void *)args);

    if (!thread) {
        free_zenity_date_args(args);
        callback(userdata, dummy, SDL_DIALOGRESULT_ERROR);
        return;
    }

    SDL_DetachThread(thread);
}
