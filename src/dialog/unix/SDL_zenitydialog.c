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

typedef enum
{
    ZENITY_MULTIPLE = 0x1,
    ZENITY_DIRECTORY = 0x2,
    ZENITY_SAVE = 0x4
} zenityFlags;

typedef struct
{
    SDL_DialogFileCallback callback;
    void* userdata;
    const char* filename;
    const SDL_DialogFileFilter *filters;
    int nfilters;
    Uint32 flags;
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
            /* Zenity doesn't support escaping with \ */
            *c = '/';
        }
    }

    return newname;
}

/* Exec call format:
 *
 *     /usr/bin/env zenity --file-selection --separator=\n [--multiple]
 *                         [--directory] [--save] [--filename FILENAME]
 *                         [--file-filter=Filter Name | *.filt *.fn ...]...
 */
static char** generate_args(const zenityArgs* info)
{
    int argc = 4;
    int nextarg = 0;
    char **argv = NULL;

    /* ARGC PASS */
    if (info->flags & ZENITY_MULTIPLE) {
        argc++;
    }

    if (info->flags & ZENITY_DIRECTORY) {
        argc++;
    }

    if (info->flags & ZENITY_SAVE) {
        argc++;
    }

    if (info->filename) {
        argc += 2;
    }

    if (info->filters) {
        argc += info->nfilters;
    }

    argv = SDL_malloc(sizeof(char *) * argc + 1);
    if (!argv) {
        return NULL;
    }

    argv[nextarg++] = SDL_strdup("/usr/bin/env");
    CHECK_OOM()
    argv[nextarg++] = SDL_strdup("zenity");
    CHECK_OOM()
    argv[nextarg++] = SDL_strdup("--file-selection");
    CHECK_OOM()
    argv[nextarg++] = SDL_strdup("--separator=\n");
    CHECK_OOM()

    /* ARGV PASS */
    if (info->flags & ZENITY_MULTIPLE) {
        argv[nextarg++] = SDL_strdup("--multiple");
        CHECK_OOM()
    }

    if (info->flags & ZENITY_DIRECTORY) {
        argv[nextarg++] = SDL_strdup("--directory");
        CHECK_OOM()
    }

    if (info->flags & ZENITY_SAVE) {
        argv[nextarg++] = SDL_strdup("--save");
        CHECK_OOM()
    }

    if (info->filename) {
        argv[nextarg++] = SDL_strdup("--filename");
        CHECK_OOM()

        argv[nextarg++] = SDL_strdup(info->filename);
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

/* TODO: Zenity survives termination of the parent */

static void run_zenity(zenityArgs* arg_struct)
{
    SDL_DialogFileCallback callback = arg_struct->callback;
    void* userdata = arg_struct->userdata;

    int out[2];
    pid_t process;
    int status = -1;

    if (pipe(out) < 0) {
        SDL_SetError("Could not create pipe: %s", strerror(errno));
        callback(userdata, NULL, -1);
        return;
    }

    /* Args are only needed in the forked process, but generating them early
       allows catching the error messages in the main process */
    char **args = generate_args(arg_struct);

    if (!args) {
        /* SDL_SetError will have been called already */
        callback(userdata, NULL, -1);
        return;
    }

    process = fork();

    if (process < 0) {
        SDL_SetError("Could not fork process: %s", strerror(errno));
        close(out[0]);
        close(out[1]);
        free_args(args);
        callback(userdata, NULL, -1);
        return;
    } else if (process == 0){
        dup2(out[1], STDOUT_FILENO);
        close(STDERR_FILENO); /* Hide errors from Zenity to stderr */
        close(out[0]);
        close(out[1]);

        /* Recent versions of Zenity have different exit codes, but picks up
          different codes from the environment */
        SDL_setenv("ZENITY_OK", "0", 1);
        SDL_setenv("ZENITY_CANCEL", "1", 1);
        SDL_setenv("ZENITY_ESC", "1", 1);
        SDL_setenv("ZENITY_EXTRA", "2", 1);
        SDL_setenv("ZENITY_ERROR", "2", 1);
        SDL_setenv("ZENITY_TIMEOUT", "2", 1);

        execv(args[0], args);

        exit(errno + 128);
    } else {
        char readbuffer[2048];
        size_t bytes_read = 0, bytes_last_read;
        char *container = NULL;
        close(out[1]);
        free_args(args);

        while ((bytes_last_read = read(out[0], readbuffer, sizeof(readbuffer)))) {
            char *new_container = SDL_realloc(container, bytes_read + bytes_last_read);
            if (!new_container) {
                SDL_free(container);
                close(out[0]);
                callback(userdata, NULL, -1);
                return;
            }
            container = new_container;
            SDL_memcpy(container + bytes_read, readbuffer, bytes_last_read);
            bytes_read += bytes_last_read;
        }
        close(out[0]);

        if (waitpid(process, &status, 0) == -1) {
            SDL_SetError("waitpid failed");
            SDL_free(container);
            callback(userdata, NULL, -1);
            return;
        }

        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
        }

        size_t narray = 1;
        char **array = (char **) SDL_malloc((narray + 1) * sizeof(char *));

        if (!array) {
            SDL_free(container);
            callback(userdata, NULL, -1);
            return;
        }

        array[0] = container;
        array[1] = NULL;

        for (int i = 0; i < bytes_read; i++) {
            if (container[i] == '\n') {
                container[i] = '\0';
                /* Reading from a process often leaves a trailing \n, so ignore the last one */
                if (i < bytes_read - 1) {
                    array[narray] = container + i + 1;
                    narray++;
                    char **new_array = (char **) SDL_realloc(array, (narray + 1) * sizeof(char *));
                    if (!new_array) {
                        SDL_free(container);
                        SDL_free(array);
                        callback(userdata, NULL, -1);
                        return;
                    }
                    array = new_array;
                    array[narray] = NULL;
                }
            }
        }

        /* 0 = the user chose one or more files, 1 = the user canceled the dialog */
        if (status == 0 || status == 1) {
            callback(userdata, (const char * const*) array, -1);
        } else {
            SDL_SetError("Could not run zenity: exit code %d (may be zenity or execv+128)", status);
            callback(userdata, NULL, -1);
        }

        SDL_free(array);
        SDL_free(container);
    }
}

static int run_zenity_thread(void* ptr)
{
    run_zenity(ptr);
    SDL_free(ptr);
    return 0;
}

void SDL_Zenity_ShowOpenFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, int nfilters, const char* default_location, SDL_bool allow_many)
{
    zenityArgs *args;
    SDL_Thread *thread;

    args = SDL_malloc(sizeof(*args));
    if (!args) {
        callback(userdata, NULL, -1);
        return;
    }

    args->callback = callback;
    args->userdata = userdata;
    args->filename = default_location;
    args->filters = filters;
    args->nfilters = nfilters;
    args->flags = (allow_many == SDL_TRUE) ? ZENITY_MULTIPLE : 0;

    thread = SDL_CreateThread(run_zenity_thread, "SDL_ShowOpenFileDialog", (void *) args);

    if (thread == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    SDL_DetachThread(thread);
}

void SDL_Zenity_ShowSaveFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, int nfilters, const char* default_location)
{
    zenityArgs *args;
    SDL_Thread *thread;

    args = SDL_malloc(sizeof(zenityArgs));
    if (args == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    args->callback = callback;
    args->userdata = userdata;
    args->filename = default_location;
    args->filters = filters;
    args->nfilters = nfilters;
    args->flags = ZENITY_SAVE;

    thread = SDL_CreateThread(run_zenity_thread, "SDL_ShowSaveFileDialog", (void *) args);

    if (thread == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    SDL_DetachThread(thread);
}

void SDL_Zenity_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const char* default_location, SDL_bool allow_many)
{
    zenityArgs *args;
    SDL_Thread *thread;

    args = SDL_malloc(sizeof(zenityArgs));
    if (args == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    args->callback = callback;
    args->userdata = userdata;
    args->filename = default_location;
    args->filters = NULL;
    args->nfilters = 0;
    args->flags = ((allow_many == SDL_TRUE) ? ZENITY_MULTIPLE : 0) | ZENITY_DIRECTORY;

    thread = SDL_CreateThread(run_zenity_thread, "SDL_ShowOpenFolderDialog", (void *) args);

    if (thread == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    SDL_DetachThread(thread);
}

int SDL_Zenity_detect(void)
{
    pid_t process;
    int status = -1;

    process = fork();

    if (process < 0) {
        SDL_SetError("Could not fork process: %s", strerror(errno));
        return 0;
    } else if (process == 0){
        /* Disable output */
        close(STDERR_FILENO);
        close(STDOUT_FILENO);
        execl("/usr/bin/env", "/usr/bin/env", "zenity", "--version", NULL);
        exit(errno + 128);
    } else {
        if (waitpid(process, &status, 0) == -1) {
            SDL_SetError("waitpid failed");
            return 0;
        }

        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
        }

        return !status;
    }
}
