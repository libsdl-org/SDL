/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PROCESS_POSIX

#define SDLIPC_FILENO 3

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../SDL_sysprocess.h"
#include "../../io/SDL_iostream_c.h"
#include "../../video/SDL_surface_c.h"


#if defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP) && \
    !defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR)
#define HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR
#define posix_spawn_file_actions_addchdir posix_spawn_file_actions_addchdir_np
#endif

#define READ_END 0
#define WRITE_END 1

#define PARENT_END 0
#define CHILD_END 1

#define SDL_IPC_ENVVAR "_SDL_IPC"

#define _STR(VALUE) #VALUE

// Allows you to stringify the expanded value of a
// macro
#define STR(MACRO) _STR(MACRO)

struct SDL_IPC {
    int socket;
};

struct SDL_ProcessData {
    pid_t pid;
    SDL_IPC ipc;
};

typedef struct SDL_SurfaceData {
    int width;
    int height;
    SDL_PixelFormat format;
    int has_palette;
    int ncolors;
} SDL_SurfaceData;

struct SDL_SharedSurface {
    SDL_Surface *surface;
    int shared_memory_fd;
};

static int shm_open_anon(off_t length)
{
    // I'm surprised this isn't already POSIX...
    // everyone kinda does their own thing, with
    // BSDs having a special SHM_ANON arg for shm_open()
    // and Linux having a separate memfd_create() function
#define TEMPNAME_PREFIX "/sdl-shared-memory-"
#define RANDOM_SUFFIX_SIZE 6
    int random_suffix, fd;
    char tempname[sizeof(TEMPNAME_PREFIX) + RANDOM_SUFFIX_SIZE + 1];

    for (int i = 0; i < 100; i++) {
        random_suffix = rand();
        if (SDL_snprintf(tempname, sizeof(tempname), TEMPNAME_PREFIX "%*d", RANDOM_SUFFIX_SIZE, random_suffix) < 0)
            return -1;

        fd = shm_open(tempname, O_RDWR | O_CREAT | O_EXCL, 0600);
        const int error = fd < 0;
        if (error)
            continue;

        shm_unlink(tempname);

        if (ftruncate(fd, length) < 0) {
            SDL_SetError("Error resizing shared memory fd %d: %s", fd, strerror(errno));
            close(fd);
            return -1;
        }
        return fd;
    }
    // give up
    SDL_SetError("Error creating shared memory file descriptor: %s", strerror(errno));
    return -1;
#undef TEMPNAME_PREFIX
#undef RANDOM_SUFFIX_SIZE
}

static void CleanupStream(void *userdata, void *value)
{
    SDL_Process *process = (SDL_Process *)value;
    const char *property = (const char *)userdata;

    SDL_ClearProperty(process->props, property);
}

static bool SetupStream(SDL_Process *process, int fd, const char *mode, const char *property)
{
    // Set the file descriptor to non-blocking mode
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    SDL_IOStream *io = SDL_IOFromFD(fd, true);
    if (!io) {
        return false;
    }

    SDL_SetPointerPropertyWithCleanup(SDL_GetIOProperties(io), "SDL.internal.process", process, CleanupStream, (void *)property);
    SDL_SetPointerProperty(process->props, property, io);
    return true;
}

static void IgnoreSignal(int sig)
{
    struct sigaction action;

    sigaction(SIGPIPE, NULL, &action);
#ifdef HAVE_SA_SIGACTION
    if (action.sa_handler == SIG_DFL && (void (*)(int))action.sa_sigaction == SIG_DFL) {
#else
    if (action.sa_handler == SIG_DFL) {
#endif
        action.sa_handler = SIG_IGN;
        sigaction(sig, &action, NULL);
    }
}

static bool CreateSockets(int fds[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        return false;
    }

    fcntl(fds[PARENT_END], F_SETFD, fcntl(fds[PARENT_END], F_GETFD) | FD_CLOEXEC);
    fcntl(fds[CHILD_END], F_SETFD, fcntl(fds[CHILD_END], F_GETFD) | FD_CLOEXEC);

    return true;
}

static bool CreatePipe(int fds[2])
{
    if (pipe(fds) < 0) {
        return false;
    }

    // Make sure the pipe isn't accidentally inherited by another thread creating a process
    fcntl(fds[READ_END], F_SETFD, fcntl(fds[READ_END], F_GETFD) | FD_CLOEXEC);
    fcntl(fds[WRITE_END], F_SETFD, fcntl(fds[WRITE_END], F_GETFD) | FD_CLOEXEC);

    // Make sure we don't crash if we write when the pipe is closed
    IgnoreSignal(SIGPIPE);

    return true;
}

static bool GetStreamFD(SDL_PropertiesID props, const char *property, int *result)
{
    SDL_IOStream *io = (SDL_IOStream *)SDL_GetPointerProperty(props, property, NULL);
    if (!io) {
        SDL_SetError("%s is not set", property);
        return false;
    }

    int fd = (int)SDL_GetNumberProperty(SDL_GetIOProperties(io), SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, -1);
    if (fd < 0) {
        SDL_SetError("%s doesn't have SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER available", property);
        return false;
    }

    *result = fd;
    return true;
}

static bool AddFileDescriptorCloseActions(posix_spawn_file_actions_t *fa, bool has_ipc)
{
    DIR *dir = opendir("/proc/self/fd");
    const int keep_fileno = has_ipc ? SDLIPC_FILENO : STDERR_FILENO;
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            int fd = SDL_atoi(entry->d_name);
            if (fd <= keep_fileno) {
                continue;
            }

            int flags = fcntl(fd, F_GETFD);
            if (flags < 0 || (flags & FD_CLOEXEC)) {
                continue;
            }
            if (posix_spawn_file_actions_addclose(fa, fd) != 0) {
                closedir(dir);
                return SDL_SetError("posix_spawn_file_actions_addclose failed: %s", strerror(errno));
            }
        }
        closedir(dir);
    } else {
        for (int fd = (int)(sysconf(_SC_OPEN_MAX) - 1); fd > keep_fileno; --fd) {
            int flags = fcntl(fd, F_GETFD);
            if (flags < 0 || (flags & FD_CLOEXEC)) {
                continue;
            }
            if (posix_spawn_file_actions_addclose(fa, fd) != 0) {
                return SDL_SetError("posix_spawn_file_actions_addclose failed: %s", strerror(errno));
            }
        }
    }
    return true;
}

bool SDL_SYS_CreateProcessWithProperties(SDL_Process *process, SDL_PropertiesID props)
{
    char * const *args = SDL_GetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, NULL);
    SDL_Environment *env = SDL_GetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, SDL_GetEnvironment());
    char **envp = NULL;
    const char *working_directory = SDL_GetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, NULL);
    SDL_ProcessIO stdin_option = (SDL_ProcessIO)SDL_GetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_ProcessIO stdout_option = (SDL_ProcessIO)SDL_GetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_INHERITED);
    SDL_ProcessIO stderr_option = (SDL_ProcessIO)SDL_GetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);
    bool redirect_stderr = SDL_GetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, false) &&
                           !SDL_HasProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER);
    bool sdl_ipc = SDL_GetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_SDL_IPC, false);
    int stdin_pipe[2] = { -1, -1 };
    int stdout_pipe[2] = { -1, -1 };
    int stderr_pipe[2] = { -1, -1 };
    int sockets[2] = { -1, -1 };
    int fd = -1;

    if (sdl_ipc) {
        // allows children who have already opened a new file descriptor
        // to distinguish between "fd 3 came from SDL" vs. "fd 3 is the fd I just opened"
        //
        // also gives us the option to change fd logic in the future without breaking
        // clients
        SDL_SetEnvironmentVariable(env, SDL_IPC_ENVVAR, STR(SDLIPC_FILENO), true);
    }

    // Keep the malloc() before exec() so that an OOM won't run a process at all
    envp = SDL_GetEnvironmentVariables(env);
    if (!envp) {
        return false;
    }

    SDL_ProcessData *data = SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_free(envp);
        return false;
    }
    process->internal = data;

    posix_spawnattr_t attr;
    posix_spawn_file_actions_t fa;

    if (posix_spawnattr_init(&attr) != 0) {
        SDL_SetError("posix_spawnattr_init failed: %s", strerror(errno));
        goto posix_spawn_fail_none;
    }

    if (posix_spawn_file_actions_init(&fa) != 0) {
        SDL_SetError("posix_spawn_file_actions_init failed: %s", strerror(errno));
        goto posix_spawn_fail_attr;
    }

    if (working_directory) {
#ifdef HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR
#ifdef SDL_PLATFORM_APPLE
        if (__builtin_available(macOS 10.15, *)) {
            if (posix_spawn_file_actions_addchdir_np(&fa, working_directory) != 0) {
                SDL_SetError("posix_spawn_file_actions_addchdir failed: %s", strerror(errno));
                goto posix_spawn_fail_all;
            }
        } else {
            SDL_SetError("Setting the working directory is only supported on macOS 10.15 and newer");
            goto posix_spawn_fail_all;
        }
#else
        if (posix_spawn_file_actions_addchdir(&fa, working_directory) != 0) {
            SDL_SetError("posix_spawn_file_actions_addchdir failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
#endif // SDL_PLATFORM_APPLE
#else
        SDL_SetError("Setting the working directory is not supported");
        goto posix_spawn_fail_all;
#endif
    }

    // Background processes don't have access to the terminal
    if (process->background) {
        if (stdin_option == SDL_PROCESS_STDIO_INHERITED) {
            stdin_option = SDL_PROCESS_STDIO_NULL;
        }
        if (stdout_option == SDL_PROCESS_STDIO_INHERITED) {
            stdout_option = SDL_PROCESS_STDIO_NULL;
        }
        if (stderr_option == SDL_PROCESS_STDIO_INHERITED) {
            stderr_option = SDL_PROCESS_STDIO_NULL;
        }
    }

    switch (stdin_option) {
    case SDL_PROCESS_STDIO_REDIRECT:
        if (!GetStreamFD(props, SDL_PROP_PROCESS_CREATE_STDIN_POINTER, &fd)) {
            goto posix_spawn_fail_all;
        }
        if (posix_spawn_file_actions_adddup2(&fa, fd, STDIN_FILENO) != 0) {
            SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
        break;
    case SDL_PROCESS_STDIO_APP:
        if (!CreatePipe(stdin_pipe)) {
            goto posix_spawn_fail_all;
        }
        if (posix_spawn_file_actions_adddup2(&fa, stdin_pipe[READ_END], STDIN_FILENO) != 0) {
            SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
        break;
    case SDL_PROCESS_STDIO_NULL:
        if (posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0) != 0) {
            SDL_SetError("posix_spawn_file_actions_addopen failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
        break;
    case SDL_PROCESS_STDIO_INHERITED:
    default:
        break;
    }

    switch (stdout_option) {
    case SDL_PROCESS_STDIO_REDIRECT:
        if (!GetStreamFD(props, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, &fd)) {
            goto posix_spawn_fail_all;
        }
        if (posix_spawn_file_actions_adddup2(&fa, fd, STDOUT_FILENO) != 0) {
            SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
        break;
    case SDL_PROCESS_STDIO_APP:
        if (!CreatePipe(stdout_pipe)) {
            goto posix_spawn_fail_all;
        }
        if (posix_spawn_file_actions_adddup2(&fa, stdout_pipe[WRITE_END], STDOUT_FILENO) != 0) {
            SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
        break;
    case SDL_PROCESS_STDIO_NULL:
        if (posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0644) != 0) {
            SDL_SetError("posix_spawn_file_actions_addopen failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
        break;
    case SDL_PROCESS_STDIO_INHERITED:
    default:
        break;
    }

    if (sdl_ipc) {
        if (!CreateSockets(sockets)) {
            goto posix_spawn_fail_all;
        }
        if (posix_spawn_file_actions_adddup2(&fa, sockets[CHILD_END], SDLIPC_FILENO) != 0) {
            SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
    }

    if (redirect_stderr) {
        if (posix_spawn_file_actions_adddup2(&fa, STDOUT_FILENO, STDERR_FILENO) != 0) {
            SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
    } else {
        switch (stderr_option) {
        case SDL_PROCESS_STDIO_REDIRECT:
            if (!GetStreamFD(props, SDL_PROP_PROCESS_CREATE_STDERR_POINTER, &fd)) {
                goto posix_spawn_fail_all;
            }
            if (posix_spawn_file_actions_adddup2(&fa, fd, STDERR_FILENO) != 0) {
                SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
                goto posix_spawn_fail_all;
            }
            break;
        case SDL_PROCESS_STDIO_APP:
            if (!CreatePipe(stderr_pipe)) {
                goto posix_spawn_fail_all;
            }
            if (posix_spawn_file_actions_adddup2(&fa, stderr_pipe[WRITE_END], STDERR_FILENO) != 0) {
                SDL_SetError("posix_spawn_file_actions_adddup2 failed: %s", strerror(errno));
                goto posix_spawn_fail_all;
            }
            break;
        case SDL_PROCESS_STDIO_NULL:
            if (posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0644) != 0) {
                SDL_SetError("posix_spawn_file_actions_addopen failed: %s", strerror(errno));
                goto posix_spawn_fail_all;
            }
            break;
        case SDL_PROCESS_STDIO_INHERITED:
        default:
            break;
        }
    }

    if (!AddFileDescriptorCloseActions(&fa, sdl_ipc)) {
        goto posix_spawn_fail_all;
    }

    // Spawn the new process
    if (process->background) {
        int status = -1;
        #ifdef SDL_PLATFORM_APPLE  // Apple has vfork marked as deprecated and (as of macOS 10.12) is almost identical to calling fork() anyhow.
        const pid_t pid = fork();
        const char *forkname = "fork";
        #else
        const pid_t pid = vfork();
        const char *forkname = "vfork";
        #endif
        switch (pid) {
        case -1:
            SDL_SetError("%s() failed: %s", forkname, strerror(errno));
            goto posix_spawn_fail_all;

        case 0:
            // Detach from the terminal and launch the process
            setsid();
            if (posix_spawnp(&data->pid, args[0], &fa, &attr, args, envp) != 0) {
                _exit(errno);
            }
            _exit(0);

        default:
            if (waitpid(pid, &status, 0) < 0) {
                SDL_SetError("waitpid() failed: %s", strerror(errno));
                goto posix_spawn_fail_all;
            }
            if (status != 0) {
                SDL_SetError("posix_spawn() failed: %s", strerror(status));
                goto posix_spawn_fail_all;
            }
            break;
        }
    } else {
        if (posix_spawnp(&data->pid, args[0], &fa, &attr, args, envp) != 0) {
            SDL_SetError("posix_spawn() failed: %s", strerror(errno));
            goto posix_spawn_fail_all;
        }
    }
    SDL_SetNumberProperty(process->props, SDL_PROP_PROCESS_PID_NUMBER, data->pid);

    if (stdin_option == SDL_PROCESS_STDIO_APP) {
        if (!SetupStream(process, stdin_pipe[WRITE_END], "wb", SDL_PROP_PROCESS_STDIN_POINTER)) {
            close(stdin_pipe[WRITE_END]);
        }
        close(stdin_pipe[READ_END]);
    }

    if (stdout_option == SDL_PROCESS_STDIO_APP) {
        if (!SetupStream(process, stdout_pipe[READ_END], "rb", SDL_PROP_PROCESS_STDOUT_POINTER)) {
            close(stdout_pipe[READ_END]);
        }
        close(stdout_pipe[WRITE_END]);
    }

    if (stderr_option == SDL_PROCESS_STDIO_APP) {
        if (!SetupStream(process, stderr_pipe[READ_END], "rb", SDL_PROP_PROCESS_STDERR_POINTER)) {
            close(stderr_pipe[READ_END]);
        }
        close(stderr_pipe[WRITE_END]);
    }

    if (sdl_ipc) {
        data->ipc.socket = sockets[PARENT_END];
        close(sockets[CHILD_END]);
    }
    SDL_SetBooleanProperty(process->props, SDL_PROP_PROCESS_SDL_IPC, sdl_ipc);

    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);
    SDL_free(envp);

    return true;

    /* --------------------------------------------------------------------- */

posix_spawn_fail_all:
    posix_spawn_file_actions_destroy(&fa);

posix_spawn_fail_attr:
    posix_spawnattr_destroy(&attr);

posix_spawn_fail_none:
    if (stdin_pipe[READ_END] >= 0) {
        close(stdin_pipe[READ_END]);
    }
    if (stdin_pipe[WRITE_END] >= 0) {
        close(stdin_pipe[WRITE_END]);
    }
    if (stdout_pipe[READ_END] >= 0) {
        close(stdout_pipe[READ_END]);
    }
    if (stdout_pipe[WRITE_END] >= 0) {
        close(stdout_pipe[WRITE_END]);
    }
    if (stderr_pipe[READ_END] >= 0) {
        close(stderr_pipe[READ_END]);
    }
    if (stderr_pipe[WRITE_END] >= 0) {
        close(stderr_pipe[WRITE_END]);
    }
    if (sockets[PARENT_END] >= 0) {
        close(sockets[PARENT_END]);
    }
    if (sockets[CHILD_END] >= 0) {
        close(sockets[CHILD_END]);
    }
    SDL_free(envp);
    return false;
}

bool SDL_SYS_KillProcess(SDL_Process *process, bool force)
{
    int ret = kill(process->internal->pid, force ? SIGKILL : SIGTERM);
    if (ret == 0) {
        return true;
    } else {
        return SDL_SetError("Could not kill(): %s", strerror(errno));
    }
}

bool SDL_SYS_WaitProcess(SDL_Process *process, bool block, int *exitcode)
{
    int wstatus = 0;
    int ret;
    pid_t pid = process->internal->pid;

    if (process->background) {
        // We can't wait on the status, so we'll poll to see if it's alive
        if (block) {
            while (kill(pid, 0) == 0) {
                SDL_Delay(10);
            }
        } else {
            if (kill(pid, 0) == 0) {
                return false;
            }
        }
        *exitcode = 0;
        return true;
    } else {
        ret = waitpid(pid, &wstatus, block ? 0 : WNOHANG);
        if (ret < 0) {
            return SDL_SetError("Could not waitpid(): %s", strerror(errno));
        }

        if (ret == 0) {
            SDL_ClearError();
            return false;
        }

        if (WIFEXITED(wstatus)) {
            *exitcode = WEXITSTATUS(wstatus);
        } else if (WIFSIGNALED(wstatus)) {
            *exitcode = -WTERMSIG(wstatus);
        } else {
            *exitcode = -255;
        }

        return true;
    }
}

void SDL_SYS_DestroyProcess(SDL_Process *process)
{
    SDL_IOStream *io;

    io = (SDL_IOStream *)SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDIN_POINTER, NULL);
    if (io) {
        SDL_CloseIO(io);
    }

    io = (SDL_IOStream *)SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDOUT_POINTER, NULL);
    if (io) {
        SDL_CloseIO(io);
    }

    io = (SDL_IOStream *)SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDERR_POINTER, NULL);
    if (io) {
        SDL_CloseIO(io);
    }

    SDL_free(process->internal);
}

SDL_IPC * SDL_SYS_GetProcessIPC(SDL_Process *process)
{
    SDL_ProcessData *data = (SDL_ProcessData*)process->internal;
    return &data->ipc;
}

SDL_IPC * SDL_SYS_GetParentIPC(void)
{
    const char *SDL_GetEnvironmentVariable(SDL_Environment *env, const char *name);

    // hmmmm
    static SDL_IPC ipc = {
        .socket = -1,
    };

    SDL_Environment *env = NULL;

    if (ipc.socket == -1) {
        env = SDL_GetEnvironment();
        if (!env) {
            goto return_null;
        }

        const char *env_fd = SDL_GetEnvironmentVariable(env, SDL_IPC_ENVVAR);
        if (!env_fd) {
            goto destroy_environment;
        }

        char *end = NULL;
        long result = SDL_strtol(env_fd, &end, 10);

        const bool error = end == NULL
            || end == env_fd
            || result > INT_MAX
            || result < 0;

        if (error) {
            goto destroy_environment;
        }

        ipc.socket = (int)result;
    }

    return &ipc;

destroy_environment:
    SDL_DestroyEnvironment(env);
return_null:
    return NULL;
}

static SDL_SharedSurface *SDL_SYS_CreateSharedSurfaceFrom(int shared_memory_fd, size_t size, size_t pitch, int width, int height, SDL_PixelFormat format)
{
    void *pixels;
    SDL_SharedSurface *result = (SDL_SharedSurface *)SDL_malloc(sizeof(*result));
    if (!result) {
        return NULL;
    }

    result->shared_memory_fd = shared_memory_fd;

    pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if (pixels == MAP_FAILED) {
        SDL_SetError("Failed to memory map shared memory: %s", strerror(errno));
        goto error_mmap_failed;
    }

    result->surface = SDL_CreateSurfaceFrom(width, height, format, pixels, (int)pitch);
    if (result->surface == NULL) {
        // SDL_CreateSurfaceFrom already does SDL_SetError()
        goto error_create_surface_failed;
    }

    return result;

error_create_surface_failed:
    munmap(pixels, size);
error_mmap_failed:
    close(shared_memory_fd);
    SDL_free(result);
    return NULL;
}

static SDL_SharedResource SDL_SYS_ReceiveSharedSurface(SDL_IPC *ipc, struct msghdr hdr)
{
    static const SDL_SharedResource error = {
        .type = SDL_SHARED_RESOURCE_ERROR,
        .surface = NULL,
    };

    size_t size, pitch;
    int shared_resource_fd = -1;
    ssize_t amount_read = -1;
    SDL_SharedSurface *result = NULL;
    SDL_SurfaceData network_data = { 0 };
    struct iovec vec = {
        .iov_base = &network_data,
        .iov_len = sizeof(network_data),
    };
    struct msghdr datahdr = {
        .msg_iov = &vec,
        .msg_iovlen = 1,
        .msg_control = NULL,
        .msg_controllen = 0,
    };
    SDL_Palette *palette = NULL;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&hdr);

    // There should never be a reason this branch is taken
    if (!(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS))
        return error;

    SDL_memcpy(&shared_resource_fd, CMSG_DATA(cmsg), sizeof(shared_resource_fd));

    amount_read = recvmsg(ipc->socket, &datahdr, 0);
    if (amount_read < 0)
        return error;

    if (network_data.has_palette) {
        // this is starting to get unwieldy
        palette = SDL_CreatePalette(network_data.ncolors);
        if (!palette)
            return error;

        amount_read = read(
            ipc->socket,
            palette->colors,
            (size_t)network_data.ncolors * sizeof(*palette->colors)
        );
        if (amount_read < 0) {
            SDL_DestroyPalette(palette);
            return error;
        }
    }

    if (!SDL_CalculateSurfaceSize(network_data.format, network_data.width, network_data.height, &size, &pitch, false)) {
        // We should never really end up here
        SDL_DestroyPalette(palette);
        return error;
    }
    result = SDL_SYS_CreateSharedSurfaceFrom(shared_resource_fd, size, pitch, network_data.width, network_data.height, network_data.format);

    if (result == NULL) {
        SDL_DestroyPalette(palette);
        return error;
    }

    if (palette) {
        if (!SDL_SetSurfacePalette(result->surface, palette)) {
            SDL_DestroyPalette(palette);
            SDL_SYS_DestroySharedSurface(result);
            return error;
        }

        // surface takes ownership of the palette
        SDL_DestroyPalette(palette);
    }

    return (SDL_SharedResource) {
        .type = SDL_SHARED_SURFACE,
        .surface = result,
    };
}

SDL_SharedResource SDL_SYS_ReceiveSharedResource(SDL_IPC *ipc)
{
    static const SDL_SharedResource error = {
        .type = SDL_SHARED_RESOURCE_ERROR,
        .surface = NULL,
    };
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } cmsgbuf;
    SDL_SHARED_RESOURCE_TYPE type = SDL_SHARED_RESOURCE_ERROR;
    ssize_t amount_read = -1;
    struct iovec vec = {
        .iov_base = &type,
        .iov_len = sizeof(type),
    };
    struct msghdr hdr = {
        .msg_iov = &vec,
        .msg_iovlen = 1,
        .msg_control = &cmsgbuf.buf,
        .msg_controllen = sizeof(cmsgbuf.buf),
    };

    amount_read = recvmsg(ipc->socket, &hdr, 0);

    if (amount_read < 0)
        return error;

    switch (type) {
        case SDL_SHARED_SURFACE:
            return SDL_SYS_ReceiveSharedSurface(ipc, hdr);
        default:
            return error;
    }
}

SDL_SharedSurface *SDL_SYS_CreateSharedSurface(int width, int height, SDL_PixelFormat format)
{
    size_t pitch, size;
    int shared_memory_fd;

    CHECK_PARAM(width < 0) {
        SDL_InvalidParamError("width");
        return NULL;
    }

    CHECK_PARAM(height < 0) {
        SDL_InvalidParamError("height");
        return NULL;
    }

    CHECK_PARAM(format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_InvalidParamError("format");
        return NULL;
    }

    if (!SDL_CalculateSurfaceSize(format, width, height, &size, &pitch, false /* not minimal pitch */)) {
        // overflow...
        return NULL;
    }

    shared_memory_fd = shm_open_anon((off_t)size);
    if (shared_memory_fd < 0) {
        return NULL;
    }

    return SDL_SYS_CreateSharedSurfaceFrom(shared_memory_fd, size, pitch, width, height, format);
}

void SDL_SYS_DestroySharedSurface(SDL_SharedSurface *surface)
{
    if (!surface)
        return;

    size_t size;

    if (!SDL_CalculateSurfaceSize(surface->surface->format, surface->surface->w, surface->surface->h, &size, NULL, false)) {
        // How did we even get here?
        // How do we even recover without leaking?
    }

    munmap(surface->surface->pixels, size);
    SDL_DestroySurface(surface->surface);
    close(surface->shared_memory_fd);
    SDL_free(surface);
}

bool SDL_SYS_SendSharedSurface(SDL_IPC *ipc, SDL_SharedSurface *surface)
{
    static const SDL_SHARED_RESOURCE_TYPE type = SDL_SHARED_SURFACE;
    ssize_t sent;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    SDL_Palette *palette = SDL_GetSurfacePalette(surface->surface);

    CHECK_PARAM(ipc == NULL) {
        SDL_InvalidParamError("ipc");
        return false;
    }

    CHECK_PARAM(surface == NULL) {
        SDL_InvalidParamError("surface");
        return false;
    }

    struct SDL_SurfaceData surface_data = {
        .width = surface->surface->w,
        .height = surface->surface->h,
        .format = surface->surface->format,
        .has_palette = !!palette,
        .ncolors = palette ? palette->ncolors : 0
    };

    struct iovec data[] = {
        {
            .iov_base = (void*)&type,
            .iov_len = sizeof(type),
        },
        {
            .iov_base = &surface_data,
            .iov_len = sizeof(surface_data),
        },
        {
            .iov_base = palette ? palette->colors : NULL,
            .iov_len = palette ? (size_t)palette->ncolors * sizeof(*palette->colors) : 0,
        },
    };

    // ripped straight from cmsg(3)
    union {
        char buf[CMSG_SPACE(sizeof(surface->shared_memory_fd))];
        struct cmsghdr align;
    } cmsgbuf;

    msg = (struct msghdr) {
        .msg_iov = data,
        .msg_iovlen = SDL_arraysize(data),
        .msg_control = cmsgbuf.buf,
        .msg_controllen = sizeof(cmsgbuf.buf),
    };

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(surface->shared_memory_fd));
    SDL_memcpy(CMSG_DATA(cmsg), &surface->shared_memory_fd, sizeof(surface->shared_memory_fd));

    sent = sendmsg(ipc->socket, &msg, 0);
    const size_t expected = data[0].iov_len + data[1].iov_len + data[2].iov_len;
    return sent == (ssize_t)expected;
}

SDL_Surface *SDL_SYS_GetSurfaceFromSharedSurface(SDL_SharedSurface *surface)
{
    return surface->surface;
}

#endif // SDL_PROCESS_POSIX
