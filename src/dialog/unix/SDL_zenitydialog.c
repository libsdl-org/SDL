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
    char x11_window_handle[64];
    const char *title;
    const char *accept;
    const char *cancel;
} zenityArgs;

typedef struct
{
    const char *const *argv;
    int argc;

    char *const *filters_slice;
    int nfilters;
} Args;

#define NULL_ARGS \
    (Args) { NULL }

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

/* Exec call format:
 *
 *     zenity --file-selection --separator=\n [--multiple]
 *                         [--directory] [--save --confirm-overwrite]
 *                         [--filename FILENAME] [--modal --attach 0x11w1nd0w]
 *                         [--title TITLE] [--ok-label ACCEPT]
 *                         [--cancel-label CANCEL]
 *                         [--file-filter=Filter Name | *.filt *.fn ...]...
 */
static Args generate_args(const zenityArgs *info)
{
    int argc = 0;
    const char **argv = SDL_malloc(
        sizeof(*argv) * (3   /* zenity --file-selection --separator=\n */
                         + 1 /* --multiple */
                         + 1 /* --directory */
                         + 2 /* --save --confirm-overwrite */
                         + 2 /* --filename [file] */
                         + 3 /* --modal --attach [handle] */
                         + 2 /* --title [title] */
                         + 2 /* --ok-label [label] */
                         + 2 /* --cancel-label [label] */
                         + info->nfilters + 1 /* NULL */));
    if (!argv) {
        return NULL_ARGS;
    }

    // ARGV PASS
    argv[argc++] = "zenity";
    argv[argc++] = "--file-selection";
    argv[argc++] = "--separator=\n";

    if (info->allow_many) {
        argv[argc++] = "--multiple";
    }

    switch (info->type) {
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

    if (info->filename) {
        argv[argc++] = "--filename";
        argv[argc++] = info->filename;
    }

    if (info->x11_window_handle[0]) {
        argv[argc++] = "--modal";
        argv[argc++] = "--attach";
        argv[argc++] = info->x11_window_handle;
    }

    if (info->title) {
        argv[argc++] = "--title";
        argv[argc++] = info->title;
    }

    if (info->accept) {
        argv[argc++] = "--ok-label";
        argv[argc++] = info->accept;
    }

    if (info->cancel) {
        argv[argc++] = "--cancel-label";
        argv[argc++] = info->cancel;
    }

    char **filters_slice = (char **)&argv[argc];
    if (info->filters) {
        for (int i = 0; i < info->nfilters; i++) {
            char *filter_str = convert_filter(info->filters[i],
                                              zenity_clean_name,
                                              "--file-filter=", " | ", "",
                                              "*.", " *.", "");

            if (!filter_str) {
                while (i--) {
                    SDL_free(filters_slice[i]);
                }
                SDL_free(argv);
                return NULL_ARGS;
            }

            filters_slice[i] = filter_str;
        }
    }

    argc += info->nfilters;
    argv[argc] = NULL;

    return (Args){
        .argv = argv,
        .argc = argc,

        .filters_slice = filters_slice,
        .nfilters = info->nfilters
    };
}

static void free_args(Args args)
{
    if (!args.argv) {
        return;
    }
    for (int i = 0; i < args.nfilters; i++) {
        SDL_free(args.filters_slice[i]);
    }

    SDL_free((void *)args.argv);
}

// TODO: Zenity survives termination of the parent

static void run_zenity(zenityArgs* arg_struct)
{
    SDL_DialogFileCallback callback = arg_struct->callback;
    void* userdata = arg_struct->userdata;
    SDL_Process *process = NULL;
    SDL_Environment *env = NULL;
    int status = -1;
    size_t bytes_read = 0;
    char *container = NULL;
    size_t narray = 1;
    char **array = NULL;
    bool result = false;

    Args args = generate_args(arg_struct);
    if (!args.argv) {
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
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args.argv);
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

static void free_zenity_args(zenityArgs *args)
{
    if (args->filters) {
        for (int i = 0; i < args->nfilters; i++) {
            SDL_free((void *)args->filters[i].name);
            SDL_free((void *)args->filters[i].pattern);
        }
        SDL_free((void *)args->filters);
    }
    if (args->filename) {
        SDL_free((void *)args->filename);
    }
    if (args->title) {
        SDL_free((void *)args->title);
    }
    if (args->accept) {
        SDL_free((void *)args->title);
    }
    if (args->cancel) {
        SDL_free((void *)args->title);
    }

    SDL_free(args);
}

static SDL_DialogFileFilter *deep_copy_filters(int nfilters, const SDL_DialogFileFilter *filters)
{
    if (nfilters == 0 || filters == NULL) {
        return NULL;
    }

    SDL_DialogFileFilter *new_filters = SDL_malloc(sizeof(SDL_DialogFileFilter) * nfilters);
    if (!new_filters) {
        return NULL;
    }

    for (int i = 0; i < nfilters; i++) {
        new_filters[i].name = SDL_strdup(filters[i].name);
        if (!new_filters[i].name) {
            nfilters = i;
            goto cleanup;
        }

        new_filters[i].pattern = SDL_strdup(filters[i].pattern);
        if (!new_filters[i].pattern) {
            SDL_free((void *)new_filters[i].name);
            nfilters = i;
            goto cleanup;
        }
    }

    return new_filters;

cleanup:
    while (nfilters--) {
        SDL_free((void *)new_filters[nfilters].name);
        SDL_free((void *)new_filters[nfilters].pattern);
    }
    SDL_free(new_filters);
    return NULL;
}

static int run_zenity_thread(void *ptr)
{
    run_zenity(ptr);
    free_zenity_args(ptr);
    return 0;
}

void SDL_Zenity_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    zenityArgs *args;
    SDL_Thread *thread = NULL;

    args = SDL_malloc(sizeof(*args));
    if (!args) {
        callback(userdata, NULL, -1);
        return;
    }

#define COPY_STRING_PROPERTY(dst, prop)                             \
    {                                                               \
        const char *str = SDL_GetStringProperty(props, prop, NULL); \
        if (str) {                                                  \
            dst = SDL_strdup(str);                                  \
            if (!dst) {                                             \
                goto done;                                          \
            }                                                       \
        } else {                                                    \
            dst = NULL;                                             \
        }                                                           \
    }

    /* Properties can be destroyed as soon as the function returns; copy over what we need. */
    args->callback = callback;
    args->userdata = userdata;
    COPY_STRING_PROPERTY(args->filename, SDL_PROP_FILE_DIALOG_LOCATION_STRING);
    args->nfilters = SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0);
    args->filters = deep_copy_filters(args->nfilters, SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, NULL));
    if (args->nfilters != 0 && !args->filters) {
        goto done;
    }
    args->allow_many = SDL_GetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
    args->type = type;
    args->x11_window_handle[0] = 0;
    COPY_STRING_PROPERTY(args->title, SDL_PROP_FILE_DIALOG_TITLE_STRING);
    COPY_STRING_PROPERTY(args->accept, SDL_PROP_FILE_DIALOG_ACCEPT_STRING);
    COPY_STRING_PROPERTY(args->cancel, SDL_PROP_FILE_DIALOG_CANCEL_STRING);

    SDL_Window *window = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, NULL);
    if (window) {
        SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
        if (window_props) {
            Uint64 handle = (Uint64)SDL_GetNumberProperty(window_props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
            if (handle) {
                SDL_snprintf(args->x11_window_handle, 64, "0x%" SDL_PRIx64, handle);
            }
        }
    }

    thread = SDL_CreateThread(run_zenity_thread, "SDL_ZenityFileDialog", (void *)args);

done:
    if (thread == NULL) {
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
