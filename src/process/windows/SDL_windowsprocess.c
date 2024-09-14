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

#ifdef SDL_PROCESS_WINDOWS

#include "../../core/windows/SDL_windows.h"
#include "../SDL_sysprocess.h"
#include "../../file/SDL_iostream_c.h"

#define READ_END 0
#define WRITE_END 1

struct SDL_ProcessData {
    PROCESS_INFORMATION process_information;
};

static void CleanupStream(void *userdata, void *value)
{
    SDL_Process *process = (SDL_Process *)value;
    const char *property = (const char *)userdata;

    SDL_ClearProperty(process->props, property);
}

static bool SetupStream(SDL_Process *process, HANDLE handle, const char *mode, const char *property)
{
    SDL_IOStream *io = SDL_IOFromHandle(handle, mode, true);
    if (!io) {
        return false;
    }

    SDL_SetPointerPropertyWithCleanup(SDL_GetIOProperties(io), "SDL.internal.process", process, CleanupStream, (void *)property);
    SDL_SetPointerProperty(process->props, property, io);
    return true;
}

static bool SetupRedirect(SDL_PropertiesID props, const char *property, HANDLE *result)
{
    SDL_IOStream *io = (SDL_IOStream *)SDL_GetPointerProperty(props, property, NULL);
    if (!io) {
        SDL_SetError("%s is not set", property);
        return false;
    }

    HANDLE handle = (HANDLE)SDL_GetPointerProperty(SDL_GetIOProperties(io), SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER, INVALID_HANDLE_VALUE);
    if (handle == INVALID_HANDLE_VALUE) {
        SDL_SetError("%s doesn't have SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER available", property);
        return false;
    }

    if (!DuplicateHandle(GetCurrentProcess(), handle,
                         GetCurrentProcess(), result,
                         0, TRUE, DUPLICATE_SAME_ACCESS)) {
        WIN_SetError("DuplicateHandle()");
        return false;
    }
    return true;
}

static bool join_arguments(const char * const *args, char **args_out)
{
    size_t len;
    int i;
    int i_out;
    char *result;

    len = 0;
    for (i = 0; args[i]; i++) {
        const char *a = args[i];

        /* two double quotes to surround an argument with */
        len += 2;

        for (; *a; a++) {
            switch (*a) {
            case '"':
                len += 2;
                break;
            default:
                len += 1;
                break;
            }
        }
        /* space separator or final '\0' */
        len += 1;
    }

    result = SDL_malloc(len);
    if (!result) {
        *args_out = NULL;
        return false;
    }

    i_out = 0;
    for (i = 0; args[i]; i++) {
        const char *a = args[i];

        result[i_out++] = '"';
        for (; *a; a++) {
            switch (*a) {
            case '"':
                result[i_out++] = '\\';
                result[i_out++] = *a;
                break;
            default:
                result[i_out++] = *a;
                break;
            }
        }
        result[i_out++] = '"';
        result[i_out++] = ' ';
    }
    SDL_assert(i_out == len);
    result[len - 1] = '\0';
    *args_out = result;
    return true;
}

static bool join_env(const char * const *env, char **environment_out)
{
    size_t len;
    const char * const *var;
    char *result;

    if (!env) {
        *environment_out = NULL;
        return true;
    }

    len = 0;
    for (var = env; *var; var++) {
        len += SDL_strlen(*var) + 1;
    }
    result = SDL_malloc(len + 1);
    if (!result) {
        return false;
    }

    len = 0;
    for (var = env; *var; var++) {
        size_t l = SDL_strlen(*var);
        SDL_memcpy(result + len, *var, l);
        result[len + l] = '\0';
        len += l + 1;
    }
    result[len] = '\0';

    *environment_out = result;
    return true;
}

bool SDL_SYS_CreateProcessWithProperties(SDL_Process *process, SDL_PropertiesID props)
{
    const char * const *args = SDL_GetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, NULL);
    const char * const *env = SDL_GetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, NULL);
    SDL_ProcessIO stdin_option = (SDL_ProcessIO)SDL_GetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_ProcessIO stdout_option = (SDL_ProcessIO)SDL_GetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_INHERITED);
    SDL_ProcessIO stderr_option = (SDL_ProcessIO)SDL_GetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);
    bool redirect_stderr = SDL_GetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, false) &&
                           !SDL_HasProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER);
    char *createprocess_cmdline = NULL;
    char *createprocess_env = NULL;
    STARTUPINFOA startup_info;
    DWORD creation_flags;
    char *create_process_cwd;
    SECURITY_ATTRIBUTES security_attributes;
    HANDLE stdin_pipe[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    HANDLE stdout_pipe[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    HANDLE stderr_pipe[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    char **env_copy = NULL;
    bool result = false;

    // Keep the malloc() before exec() so that an OOM won't run a process at all
    SDL_ProcessData *data = SDL_calloc(1, sizeof(*data));
    if (!data) {
        return false;
    }
    process->internal = data;

    if (!join_arguments(args, &createprocess_cmdline)) {
        goto done;
    }

    if (!env) {
        env_copy = SDL_GetEnvironmentVariables(SDL_GetEnvironment());
        env = (const char * const *)env_copy;
    }
    if (!join_env(env, &createprocess_env)) {
        goto done;
    }

    creation_flags = 0;

    SDL_zero(startup_info);
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags |= STARTF_USESTDHANDLES;
    startup_info.hStdInput = INVALID_HANDLE_VALUE;
    startup_info.hStdOutput = INVALID_HANDLE_VALUE;
    startup_info.hStdError = INVALID_HANDLE_VALUE;

    SDL_zero(security_attributes);
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;
    security_attributes.lpSecurityDescriptor = NULL;

    switch (stdin_option) {
    case SDL_PROCESS_STDIO_REDIRECT:
        if (!SetupRedirect(props, SDL_PROP_PROCESS_CREATE_STDIN_POINTER, &startup_info.hStdInput)) {
            goto done;
        }
        break;
    case SDL_PROCESS_STDIO_APP:
        if (!CreatePipe(&stdin_pipe[READ_END], &stdin_pipe[WRITE_END], &security_attributes, 0)) {
            stdin_pipe[READ_END] = INVALID_HANDLE_VALUE;
            stdin_pipe[WRITE_END] = INVALID_HANDLE_VALUE;
            goto done;
        }
        if (!SetHandleInformation(stdin_pipe[WRITE_END], HANDLE_FLAG_INHERIT, 0) ) {
            WIN_SetError("SetHandleInformation()");
            goto done;
        }
        startup_info.hStdInput = stdin_pipe[READ_END];
        break;
    case SDL_PROCESS_STDIO_NULL:
        startup_info.hStdInput = CreateFile(TEXT("\\\\.\\NUL"), GENERIC_ALL, 0, &security_attributes, OPEN_EXISTING, 0, NULL);
        break;
    case SDL_PROCESS_STDIO_INHERITED:
    default:
        if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE),
                             GetCurrentProcess(), &startup_info.hStdInput,
                             0, TRUE, DUPLICATE_SAME_ACCESS)) {
            startup_info.hStdInput = INVALID_HANDLE_VALUE;
            WIN_SetError("DuplicateHandle()");
            goto done;
        }
        break;
    }

    switch (stdout_option) {
    case SDL_PROCESS_STDIO_REDIRECT:
        if (!SetupRedirect(props, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, &startup_info.hStdOutput)) {
            goto done;
        }
        break;
    case SDL_PROCESS_STDIO_APP:
        if (!CreatePipe(&stdout_pipe[READ_END], &stdout_pipe[WRITE_END], &security_attributes, 0)) {
            stdout_pipe[READ_END] = INVALID_HANDLE_VALUE;
            stdout_pipe[WRITE_END] = INVALID_HANDLE_VALUE;
            goto done;
        }
        if (!SetHandleInformation(stdout_pipe[READ_END], HANDLE_FLAG_INHERIT, 0) ) {
            WIN_SetError("SetHandleInformation()");
            goto done;
        }
        startup_info.hStdOutput = stdout_pipe[WRITE_END];
        break;
    case SDL_PROCESS_STDIO_NULL:
        startup_info.hStdOutput = CreateFile(TEXT("\\\\.\\NUL"), GENERIC_ALL, 0, &security_attributes, OPEN_EXISTING, 0, NULL);
        break;
    case SDL_PROCESS_STDIO_INHERITED:
    default:
        if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE),
                             GetCurrentProcess(), &startup_info.hStdOutput,
                             0, TRUE, DUPLICATE_SAME_ACCESS)) {
            startup_info.hStdOutput = INVALID_HANDLE_VALUE;
            WIN_SetError("DuplicateHandle()");
            goto done;
        }
        break;
    }

    if (redirect_stderr) {
        if (!DuplicateHandle(GetCurrentProcess(), startup_info.hStdOutput,
                             GetCurrentProcess(), &startup_info.hStdError,
                             0, TRUE, DUPLICATE_SAME_ACCESS)) {
            startup_info.hStdError = INVALID_HANDLE_VALUE;
            WIN_SetError("DuplicateHandle()");
            goto done;
        }
    } else {
        switch (stderr_option) {
        case SDL_PROCESS_STDIO_REDIRECT:
            if (!SetupRedirect(props, SDL_PROP_PROCESS_CREATE_STDERR_POINTER, &startup_info.hStdError)) {
                goto done;
            }
            break;
        case SDL_PROCESS_STDIO_APP:
            if (!CreatePipe(&stderr_pipe[READ_END], &stderr_pipe[WRITE_END], &security_attributes, 0)) {
                stderr_pipe[READ_END] = INVALID_HANDLE_VALUE;
                stderr_pipe[WRITE_END] = INVALID_HANDLE_VALUE;
                goto done;
            }
            if (!SetHandleInformation(stderr_pipe[READ_END], HANDLE_FLAG_INHERIT, 0) ) {
                WIN_SetError("SetHandleInformation()");
                goto done;
            }
            startup_info.hStdError = stderr_pipe[WRITE_END];
            break;
        case SDL_PROCESS_STDIO_NULL:
            startup_info.hStdError = CreateFile(TEXT("\\\\.\\NUL"), GENERIC_ALL, 0, &security_attributes, OPEN_EXISTING, 0, NULL);
            break;
        case SDL_PROCESS_STDIO_INHERITED:
        default:
            if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE),
                                 GetCurrentProcess(), &startup_info.hStdError,
                                 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                startup_info.hStdError = INVALID_HANDLE_VALUE;
                WIN_SetError("DuplicateHandle()");
                goto done;
            }
            break;
        }
    }

    // FIXME: This should use CreateProcessW()
    // FIXME: current directory as extended option? SDL_CreatProcessWithProperties
    create_process_cwd = NULL;
    if (!CreateProcessA(NULL, createprocess_cmdline, NULL, NULL, TRUE, creation_flags, createprocess_env, create_process_cwd, &startup_info, &data->process_information)) {
        WIN_SetError("CreateProcessA");
        goto done;
    }

    SDL_SetNumberProperty(process->props, SDL_PROP_PROCESS_PID_NUMBER, data->process_information.dwProcessId);

    if (stdin_option == SDL_PROCESS_STDIO_APP) {
        if (!SetupStream(process, stdin_pipe[WRITE_END], "wb", SDL_PROP_PROCESS_STDIN_POINTER)) {
            CloseHandle(stdin_pipe[WRITE_END]);
            stdin_pipe[WRITE_END] = INVALID_HANDLE_VALUE;
        }
    }
    if (stdout_option == SDL_PROCESS_STDIO_APP) {
        if (!SetupStream(process, stdout_pipe[READ_END], "rb", SDL_PROP_PROCESS_STDOUT_POINTER)) {
            CloseHandle(stdout_pipe[READ_END]);
            stdout_pipe[READ_END] = INVALID_HANDLE_VALUE;
        }
    }
    if (stderr_option == SDL_PROCESS_STDIO_APP) {
        if (!SetupStream(process, stderr_pipe[READ_END], "rb", SDL_PROP_PROCESS_STDERR_POINTER)) {
            CloseHandle(stderr_pipe[READ_END]);
            stderr_pipe[READ_END] = INVALID_HANDLE_VALUE;
        }
    }

    result = true;

done:
    if (startup_info.hStdInput != INVALID_HANDLE_VALUE &&
        startup_info.hStdInput != stdin_pipe[READ_END]) {
        CloseHandle(startup_info.hStdInput);
    }
    if (startup_info.hStdOutput != INVALID_HANDLE_VALUE &&
        startup_info.hStdOutput != stdout_pipe[WRITE_END]) {
        CloseHandle(startup_info.hStdOutput);
    }
    if (startup_info.hStdError != INVALID_HANDLE_VALUE &&
        startup_info.hStdError != stderr_pipe[WRITE_END]) {
        CloseHandle(startup_info.hStdError);
    }
    if (stdin_pipe[READ_END] != INVALID_HANDLE_VALUE) {
        CloseHandle(stdin_pipe[READ_END]);
    }
    if (stdout_pipe[WRITE_END] != INVALID_HANDLE_VALUE) {
        CloseHandle(stdout_pipe[WRITE_END]);
    }
    if (stderr_pipe[WRITE_END] != INVALID_HANDLE_VALUE) {
        CloseHandle(stderr_pipe[WRITE_END]);
    }
    SDL_free(createprocess_cmdline);
    SDL_free(createprocess_env);
    SDL_free(env_copy);

    if (!result) {
        if (stdin_pipe[WRITE_END] != INVALID_HANDLE_VALUE) {
            CloseHandle(stdin_pipe[WRITE_END]);
        }
        if (stdout_pipe[READ_END] != INVALID_HANDLE_VALUE) {
            CloseHandle(stdout_pipe[READ_END]);
        }
        if (stderr_pipe[READ_END] != INVALID_HANDLE_VALUE) {
            CloseHandle(stderr_pipe[READ_END]);
        }
    }
    return result;
}

bool SDL_SYS_KillProcess(SDL_Process *process, SDL_bool force)
{
    if (!TerminateProcess(process->internal->process_information.hProcess, 1)) {
        return WIN_SetError("TerminateProcess failed");
    }
    return true;
}

bool SDL_SYS_WaitProcess(SDL_Process *process, SDL_bool block, int *exitcode)
{
    DWORD result;

    result = WaitForSingleObject(process->internal->process_information.hProcess, block ? INFINITE : 0);

    if (result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT) {
        DWORD rc;
        if (!GetExitCodeProcess(process->internal->process_information.hProcess, &rc)) {
            return WIN_SetError("GetExitCodeProcess");
        }
        if (exitcode) {
            *exitcode = (int)rc;
        }
        return true;
    } else if (result == WAIT_FAILED) {
        return WIN_SetError("WaitForSingleObject(hProcess) returned WAIT_FAILED");
    } else {
        SDL_ClearError();
        return false;
    }
}

void SDL_SYS_DestroyProcess(SDL_Process *process)
{
    SDL_ProcessData *data = process->internal;
    SDL_IOStream *io;

    io = (SDL_IOStream *)SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDIN_POINTER, NULL);
    if (io) {
        SDL_CloseIO(io);
    }
    io = (SDL_IOStream *)SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDERR_POINTER, NULL);
    if (io) {
        SDL_CloseIO(io);
    }
    io = (SDL_IOStream *)SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDOUT_POINTER, NULL);
    if (io) {
        SDL_CloseIO(io);
    }
    CloseHandle(data->process_information.hThread);
    CloseHandle(data->process_information.hProcess);
    SDL_free(data);
}

#endif // SDL_PROCESS_WINDOWS
