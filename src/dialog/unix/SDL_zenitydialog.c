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
#include "../SDL_dialog_utils.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct
{
    SDL_DialogFileCallback callback;
    void* userdata;
    const char* filename;
    const SDL_DialogFileFilter *filters;
    int nfilters;
    bool allow_many;
    SDL_FileDialogType type;
    /* Zenity only works with X11 handles apparently */
    Uint64 x11_window_handle;
    const char *title;
    const char *accept;
    const char *cancel;
} zenityArgs;

#define CLEAR_AND_RETURN()                                                    \
    {                                                                         \
        while (--nextarg >= 0) {                                              \
            SDL_free(argv[nextarg]);                                          \
        }                                                                     \
        SDL_free(argv);                                                       \
        return NULL;                                                          \
    }

#define CHECK_OOM()                                                           \
    {                                                                         \
        if (!argv[nextarg - 1]) {                                             \
            CLEAR_AND_RETURN()                                                \
        }                                                                     \
                                                                              \
        if (nextarg > argc) {                                                 \
            SDL_SetError("Zenity dialog problem: argc (%d) < nextarg (%d)",   \
                         argc, nextarg);                                      \
            CLEAR_AND_RETURN()                                                \
        }                                                                     \
    }

char *zenity_clean_name(const char *name)
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

/* Exec call format:
 *
 *     /usr/bin/env zenity --file-selection --separator=\n [--multiple]
 *                         [--directory] [--save --confirm-overwrite]
 *                         [--filename FILENAME] [--modal --attach 0x11w1nd0w]
 *                         [--title TITLE] [--ok-label ACCEPT]
 *                         [--cancel-label CANCEL]
 *                         [--file-filter=Filter Name | *.filt *.fn ...]...
 */
static char** generate_args(const zenityArgs* info)
{
    int argc = 4;
    int nextarg = 0;
    char **argv = NULL;

    // ARGC PASS
    if (info->allow_many) {
        argc++;
    }

    switch (info->type) {
    case SDL_FILEDIALOG_OPENFILE:
        break;

    case SDL_FILEDIALOG_SAVEFILE:
        argc += 2;
        break;

    case SDL_FILEDIALOG_OPENFOLDER:
        argc++;
        break;
    };

    if (info->filename) {
        argc += 2;
    }

    if (info->x11_window_handle) {
        argc += 3;
    }

    if (info->title) {
        argc += 2;
    }

    if (info->accept) {
        argc += 2;
    }

    if (info->cancel) {
        argc += 2;
    }

    if (info->filters) {
        argc += info->nfilters;
    }

    argv = SDL_malloc(sizeof(char *) * argc + 1);
    if (!argv) {
        return NULL;
    }

    // ARGV PASS
    argv[nextarg++] = SDL_strdup("/usr/bin/env");
    CHECK_OOM()
    argv[nextarg++] = SDL_strdup("zenity");
    CHECK_OOM()
    argv[nextarg++] = SDL_strdup("--file-selection");
    CHECK_OOM()
    argv[nextarg++] = SDL_strdup("--separator=\n");
    CHECK_OOM()

    if (info->allow_many) {
        argv[nextarg++] = SDL_strdup("--multiple");
        CHECK_OOM()
    }

    switch (info->type) {
    case SDL_FILEDIALOG_OPENFILE:
        break;

    case SDL_FILEDIALOG_SAVEFILE:
        argv[nextarg++] = SDL_strdup("--save");
        /* Asking before overwriting while saving seems like a sane default */
        argv[nextarg++] = SDL_strdup("--confirm-overwrite");
        break;

    case SDL_FILEDIALOG_OPENFOLDER:
        argv[nextarg++] = SDL_strdup("--directory");
        break;
    };

    if (info->filename) {
        argv[nextarg++] = SDL_strdup("--filename");
        CHECK_OOM()

        argv[nextarg++] = SDL_strdup(info->filename);
        CHECK_OOM()
    }

    if (info->x11_window_handle) {
        argv[nextarg++] = SDL_strdup("--modal");
        CHECK_OOM()

        argv[nextarg++] = SDL_strdup("--attach");
        CHECK_OOM()

        argv[nextarg++] = SDL_malloc(64);
        CHECK_OOM()

        SDL_snprintf(argv[nextarg - 1], 64, "0x%" SDL_PRIx64, info->x11_window_handle);
    }

    if (info->title) {
        argv[nextarg++] = SDL_strdup("--title");
        CHECK_OOM()

        argv[nextarg++] = SDL_strdup(info->title);
        CHECK_OOM()
    }

    if (info->accept) {
        argv[nextarg++] = SDL_strdup("--ok-label");
        CHECK_OOM()

        argv[nextarg++] = SDL_strdup(info->accept);
        CHECK_OOM()
    }

    if (info->cancel) {
        argv[nextarg++] = SDL_strdup("--cancel-label");
        CHECK_OOM()

        argv[nextarg++] = SDL_strdup(info->cancel);
        CHECK_OOM()
    }

    if (info->filters) {
        for (int i = 0; i < info->nfilters; i++) {
            char *filter_str = convert_filter(info->filters[i],
                                              zenity_clean_name,
                                              "--file-filter=", " | ", "",
                                              "*.", " *.", "");

            if (!filter_str) {
                CLEAR_AND_RETURN()
            }

            argv[nextarg++] = filter_str;
            CHECK_OOM()
        }
    }

    argv[nextarg++] = NULL;

    return argv;
}

void free_args(char **argv)
{
    char **ptr = argv;

    while (*ptr) {
        SDL_free(*ptr);
        ptr++;
    }

    SDL_free(argv);
}

// TODO: Zenity survives termination of the parent

static void run_zenity(zenityArgs* arg_struct)
{
    SDL_DialogFileCallback callback = arg_struct->callback;
    void* userdata = arg_struct->userdata;
    SDL_Process *process = NULL;
    char **args = NULL;
    SDL_Environment *env = NULL;
    int status = -1;
    size_t bytes_read = 0;
    char *container = NULL;
    size_t narray = 1;
    char **array = NULL;
    bool result = false;

    args = generate_args(arg_struct);
    if (!args) {
        goto done;
    }

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
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        goto done;
    }

    container = SDL_ReadProcess(process, &bytes_read, &status);
    if (!container) {
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
                char **new_array = (char **) SDL_realloc(array, (narray + 1) * sizeof(char *));
                if (!new_array) {
                    goto done;
                }
                array = new_array;
                array[narray] = NULL;
            }
        }
    }

    // 0 = the user chose one or more files, 1 = the user canceled the dialog
    if (status == 0 || status == 1) {
        callback(userdata, (const char * const*)array, -1);
    } else {
        SDL_SetError("Could not run zenity: exit code %d", status);
        callback(userdata, NULL, -1);
    }

    result = true;

done:
    SDL_free(array);
    SDL_free(container);
    free_args(args);
    SDL_DestroyEnvironment(env);
    SDL_DestroyProcess(process);

    if (!result) {
        callback(userdata, NULL, -1);
    }
}

static int run_zenity_thread(void* ptr)
{
    run_zenity(ptr);
    SDL_free(ptr);
    return 0;
}

void SDL_Zenity_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityArgs *args;
    SDL_Thread *thread;

    args = SDL_malloc(sizeof(*args));
    if (!args) {
        callback(userdata, NULL, -1);
        return;
    }

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
    args->callback = callback;
    args->userdata = userdata;
    args->filename = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, NULL);
    args->filters = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, NULL);
    args->nfilters = SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0);
    args->allow_many = SDL_GetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
    args->type = type;
    args->x11_window_handle = 0;
    args->title = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, NULL);
    args->accept = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, NULL);
    args->cancel = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_CANCEL_STRING, NULL);

    SDL_Window *window = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, NULL);
    if (window) {
        SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
        if (window_props) {
            args->x11_window_handle = (Uint64) SDL_GetNumberProperty(window_props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        }
    }

    thread = SDL_CreateThread(run_zenity_thread, "SDL_ZenityFileDialog", (void *) args);

    if (thread == NULL) {
        SDL_free(args);
        callback(userdata, NULL, -1);
        return;
    }

    SDL_DetachThread(thread);
}

bool SDL_Zenity_detect(void)
{
    const char *args[] = {
        "/usr/bin/env", "zenity", "--version", NULL
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
