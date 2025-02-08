#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef SDL_PLATFORM_WINDOWS
#define EXE ".exe"
#else
#define EXE ""
#endif

/*
 * FIXME: Additional tests:
 * - stdin to stderr
 */

typedef struct {
    const char *childprocess_path;
} TestProcessData;

static TestProcessData parsed_args;

static void SDLCALL setUpProcess(void **arg) {
    *arg = &parsed_args;
}

static const char *options[] = {
    "/path/to/childprocess" EXE,
    NULL
};

static char **CreateArguments(int ignore, ...) {
    va_list ap;
    size_t count = 1;
    size_t i;
    char **result;

    va_start(ap, ignore);
    for (;;) {
        const char *keyN = va_arg(ap, const char *);
        if (!keyN) {
            break;
        }
        count += 1;
    }
    va_end(ap);

    result = SDL_calloc(count, sizeof(char *));

    i = 0;
    va_start(ap, ignore);
    for (;;) {
        const char *keyN = va_arg(ap, const char *);
        if (!keyN) {
            break;
        }
        result[i++] = SDL_strdup(keyN);
    }
    va_end(ap);

    return result;
}

static void DestroyStringArray(char **list) {
    char **current;

    if (!list) {
        return;
    }
    for (current = list; *current; current++) {
        SDL_free(*current);
    }
    SDL_free(list);
}

static int SDLCALL process_testArguments(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--print-arguments",
        "--",
        "",
        "  ",
        "a b c",
        "a\tb\tc\t",
        "\"a b\" c",
        "'a' 'b' 'c'",
        "%d%%%s",
        "\\t\\c",
        "evil\\",
        "a\\b\"c\\",
        "\"\\^&|<>%", /* characters with a special meaning */
        NULL
    };
    SDL_Process *process = NULL;
    char *buffer;
    int exit_code;
    int i;
    size_t total_read = 0;

    process = SDL_CreateProcess(process_args, true);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess()");
    if (!process) {
        goto failed;
    }

    exit_code = 0xdeadbeef;
    buffer = (char *)SDL_ReadProcess(process, &total_read, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (!buffer) {
        goto failed;
    }
    SDLTest_LogEscapedString("stdout of process: ", buffer, total_read);

    for (i = 3; process_args[i]; i++) {
        char line[64];
        SDL_snprintf(line, sizeof(line), "|%d=%s|", i - 3, process_args[i]);
        SDLTest_AssertCheck(!!SDL_strstr(buffer, line), "Check %s is in output", line);
    }
    SDL_free(buffer);

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;
failed:
    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

static int SDLCALL process_testexitCode(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    int i;
    int exit_codes[] = {
        0, 13, 31, 127, 255
    };

    for (i = 0; i < SDL_arraysize(exit_codes); i++) {
        bool wait_result;
        SDL_Process *process = NULL;
        char **process_args = NULL;
        char number_buffer[8];
        int exit_code;

        SDL_snprintf(number_buffer, sizeof(number_buffer), "%d", exit_codes[i]);

        process_args = CreateArguments(0, data->childprocess_path, "--exit-code", number_buffer, NULL);

        process = SDL_CreateProcess((const char * const *)process_args, false);
        SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess()");
        if (!process) {
            goto failed;
        }

        exit_code = 0xdeadbeef;
        SDLTest_AssertPass("About to wait on process (first time)");
        wait_result = SDL_WaitProcess(process, true, &exit_code);
        SDLTest_AssertCheck(wait_result == true, "SDL_WaitProcess(): Process should have closed immediately");
        SDLTest_AssertCheck(exit_code == exit_codes[i], "SDL_WaitProcess(): Exit code should be %d, is %d", exit_codes[i], exit_code);

        exit_code = 0xdeadbeef;
        SDLTest_AssertPass("About to wait on process (second time)");
        wait_result = SDL_WaitProcess(process, true, &exit_code);
        SDLTest_AssertCheck(wait_result == true, "SDL_WaitProcess(): Process should have closed immediately");
        SDLTest_AssertCheck(exit_code == exit_codes[i], "SDL_WaitProcess(): Exit code should be %d, is %d", exit_codes[i], exit_code);

        SDLTest_AssertPass("About to destroy process");
        SDL_DestroyProcess(process);
        DestroyStringArray(process_args);
        continue;
failed:
        SDL_DestroyProcess(process);
        DestroyStringArray(process_args);
        return TEST_ABORTED;
    }
    return TEST_COMPLETED;
#if 0
failed:
    SDL_DestroyProcess(process);
    DestroyStringArray(process_args);
    return TEST_ABORTED;
#endif
}

static int SDLCALL process_testInheritedEnv(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--print-environment",
        NULL,
    };
    SDL_PropertiesID props;
    SDL_Process *process = NULL;
    Sint64 pid;
    int exit_code;
    char random_env1[64];
    char random_env2[64];
    static const char *const TEST_ENV_KEY1 = "testprocess_inherited_var";
    static const char *const TEST_ENV_KEY2 = "testprocess_other_var";
    char *test_env_val1 = NULL;
    char *test_env_val2 = NULL;
    char *buffer = NULL;

    test_env_val1 = SDLTest_RandomAsciiStringOfSize(32);
    SDL_snprintf(random_env1, sizeof(random_env1), "%s=%s", TEST_ENV_KEY1, test_env_val1);
    SDLTest_AssertPass("Setting parent environment variable %s=%s", TEST_ENV_KEY1, test_env_val1);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), TEST_ENV_KEY1, test_env_val1, true);

    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), TEST_ENV_KEY2);

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcessWithProperties()");
    if (!process) {
        goto failed;
    }

    test_env_val2 = SDLTest_RandomAsciiStringOfSize(32);
    SDL_snprintf(random_env2, sizeof(random_env2), "%s=%s", TEST_ENV_KEY2, test_env_val2);
    SDLTest_AssertPass("Setting parent environment variable %s=%s", TEST_ENV_KEY2, test_env_val2);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(),TEST_ENV_KEY2, test_env_val2, true);
    SDLTest_AssertCheck(SDL_strcmp(test_env_val1, test_env_val2) != 0, "Sanity checking the 2 random environment variables are not identical");

    props = SDL_GetProcessProperties(process);
    SDLTest_AssertCheck(props != 0, "SDL_GetProcessProperties()");

    pid = SDL_GetNumberProperty(props, SDL_PROP_PROCESS_PID_NUMBER, 0);
    SDLTest_AssertCheck(pid != 0, "Checking process ID, expected non-zero, got %" SDL_PRIs64, pid);

    exit_code = 0xdeadbeef;
    buffer = (char *)SDL_ReadProcess(process, NULL, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);

    SDLTest_AssertCheck(SDL_strstr(buffer, random_env1) != NULL, "Environment of child should contain \"%s\"", test_env_val1);
    SDLTest_AssertCheck(SDL_strstr(buffer, random_env2) == NULL, "Environment of child should not contain \"%s\"", test_env_val2);

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    SDL_free(test_env_val1);
    SDL_free(test_env_val2);
    SDL_free(buffer);
    return TEST_COMPLETED;
failed:
    SDL_free(test_env_val1);
    SDL_free(test_env_val2);
    SDL_DestroyProcess(process);
    SDL_free(buffer);
    return TEST_ABORTED;
}

static int SDLCALL process_testNewEnv(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--print-environment",
        NULL,
    };
    SDL_Environment *process_env;
    SDL_PropertiesID props;
    SDL_Process *process = NULL;
    Sint64 pid;
    int exit_code;
    char random_env1[64];
    char random_env2[64];
    static const char *const TEST_ENV_KEY1 = "testprocess_inherited_var";
    static const char *const TEST_ENV_KEY2 = "testprocess_other_var";
    char *test_env_val1 = NULL;
    char *test_env_val2 = NULL;
    char *buffer = NULL;
    size_t total_read = 0;

    test_env_val1 = SDLTest_RandomAsciiStringOfSize(32);
    SDL_snprintf(random_env1, sizeof(random_env1), "%s=%s", TEST_ENV_KEY1, test_env_val1);
    SDLTest_AssertPass("Unsetting parent environment variable %s", TEST_ENV_KEY1);
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), TEST_ENV_KEY1);

    process_env = SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(process_env, "PATH", SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PATH"), true);
    SDL_SetEnvironmentVariable(process_env, "LD_LIBRARY_PATH", SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "LD_LIBRARY_PATH"), true);
    SDL_SetEnvironmentVariable(process_env, "DYLD_LIBRARY_PATH", SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "DYLD_LIBRARY_PATH"), true);
    SDL_SetEnvironmentVariable(process_env, TEST_ENV_KEY1, test_env_val1, true);

    test_env_val2 = SDLTest_RandomAsciiStringOfSize(32);
    SDL_snprintf(random_env2, sizeof(random_env2), "%s=%s", TEST_ENV_KEY2, test_env_val1);
    SDLTest_AssertPass("Setting parent environment variable %s=%s", TEST_ENV_KEY2, test_env_val2);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), TEST_ENV_KEY2, test_env_val2, true);
    SDLTest_AssertCheck(SDL_strcmp(test_env_val1, test_env_val2) != 0, "Sanity checking the 2 random environment variables are not identical");

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, process_env);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcessWithProperties()");
    if (!process) {
        goto failed;
    }

    props = SDL_GetProcessProperties(process);
    SDLTest_AssertCheck(props != 0, "SDL_GetProcessProperties()");

    pid = SDL_GetNumberProperty(props, SDL_PROP_PROCESS_PID_NUMBER, 0);
    SDLTest_AssertCheck(pid != 0, "Checking process ID, expected non-zero, got %" SDL_PRIs64, pid);

    exit_code = 0xdeadbeef;
    buffer = (char *)SDL_ReadProcess(process, &total_read, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    SDLTest_LogEscapedString("Text read from subprocess: ", buffer, total_read);

    SDLTest_AssertCheck(SDL_strstr(buffer, random_env1) != NULL, "Environment of child should contain \"%s\"", random_env1);
    SDLTest_AssertCheck(SDL_strstr(buffer, random_env2) == NULL, "Environment of child should not contain \"%s\"", random_env1);

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    SDL_DestroyEnvironment(process_env);
    SDL_free(test_env_val1);
    SDL_free(test_env_val2);
    SDL_free(buffer);
    return TEST_COMPLETED;

failed:
    SDL_DestroyProcess(process);
    SDL_DestroyEnvironment(process_env);
    SDL_free(test_env_val1);
    SDL_free(test_env_val2);
    SDL_free(buffer);
    return TEST_ABORTED;
}

static int SDLCALL process_testKill(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--stdin",
        NULL,
    };
    SDL_Process *process = NULL;
    SDL_PropertiesID props;
    Sint64 pid;
    int result;
    int exit_code;

    SDLTest_AssertPass("About to call SDL_CreateProcess(true)");
    process = SDL_CreateProcess(process_args, true);
    if (!process) {
        goto failed;
    }

    props = SDL_GetProcessProperties(process);
    SDLTest_AssertCheck(props != 0, "SDL_GetProcessProperties()");

    pid = SDL_GetNumberProperty(props, SDL_PROP_PROCESS_PID_NUMBER, 0);
    SDLTest_AssertCheck(pid != 0, "Checking process ID, expected non-zero, got %" SDL_PRIs64, pid);

    exit_code = 0xdeadbeef;
    SDLTest_AssertPass("About to call SDL_WaitProcess(false)");
    result = SDL_WaitProcess(process, false, &exit_code);
    SDLTest_AssertCheck(result == false, "Process should not have exited yet");

    SDLTest_AssertPass("About to call SDL_KillProcess(false)");
    result = SDL_KillProcess(process, false);
    SDLTest_AssertCheck(result == true, "Process should have exited");

    exit_code = 0;
    SDLTest_AssertPass("About to call SDL_WaitProcess(true)");
    result = SDL_WaitProcess(process, true, &exit_code);
    SDLTest_AssertCheck(result == true, "Process should have exited");
    SDLTest_AssertCheck(exit_code != 0, "Exit code should be non-zero, is %d", exit_code);

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;

failed:
    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

static int process_testStdinToStdout(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--stdin-to-stdout",
        NULL,
    };
    SDL_PropertiesID props;
    SDL_Process *process = NULL;
    Sint64 pid;
    SDL_IOStream *process_stdin = NULL;
    SDL_IOStream *process_stdout = NULL;
    SDL_IOStream *process_stderr = NULL;
    size_t text_in_size = 1 * 1024 * 1024;
    char *text_in = NULL;
    size_t total_written;
    size_t total_read;
    bool wait_result;
    int exit_code;
    SDL_IOStream *stdout_stream = NULL;
    char *stdout_stream_buf;
    int iteration_count = 0;

    text_in = SDLTest_RandomAsciiStringOfSize((int)text_in_size);
    /* Make sure text_in does not contain EOF */
    for (;;) {
        char *e = SDL_strstr(text_in, "EOF");
        if (!e) {
            break;
        }
        e[0] = 'N';
    }
    text_in[text_in_size - 3] = 'E';
    text_in[text_in_size - 2] = 'O';
    text_in[text_in_size - 1] = 'F';

    stdout_stream = SDL_IOFromDynamicMem();

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcessWithProperties()");
    if (!process) {
        goto failed;
    }

    props = SDL_GetProcessProperties(process);
    SDLTest_AssertCheck(props != 0, "SDL_GetProcessProperties()");

    pid = SDL_GetNumberProperty(props, SDL_PROP_PROCESS_PID_NUMBER, 0);
    SDLTest_AssertCheck(pid != 0, "Checking process ID, expected non-zero, got %" SDL_PRIs64, pid);

    process_stdin = SDL_GetProcessInput(process);
    SDLTest_AssertCheck(process_stdin != NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDIN_POINTER) returns a valid IO stream");
    process_stdout = SDL_GetProcessOutput(process);
    SDLTest_AssertCheck(process_stdout != NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDOUT_POINTER) returns a valid IO stream");
    process_stderr = (SDL_IOStream *)SDL_GetPointerProperty(props, SDL_PROP_PROCESS_STDERR_POINTER, NULL);
    SDLTest_AssertCheck(process_stderr == NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDERR_POINTER) returns NULL");
    if (!process_stdin || !process_stdout) {
        goto failed;
    }

    total_written = 0;
    total_read = 0;
    for (;;) {
        int log_this_iteration = (iteration_count % 32) == 32;
        char local_buffer[16 * 4094];
        size_t amount_read;
        SDL_IOStatus io_status;
        if (total_written != text_in_size) {
            size_t amount_written;
            if (log_this_iteration) {
                SDLTest_AssertPass("About to SDL_WriteIO (%dth time)", iteration_count);
            }
            amount_written = SDL_WriteIO(process_stdin, text_in + total_written, text_in_size - total_written);
            if (log_this_iteration) {
                SDLTest_Log("SDL_WriteIO() -> %u (%dth time)", (unsigned)amount_written, iteration_count);
            }
            if (amount_written == 0) {
                io_status = SDL_GetIOStatus(process_stdin);
                if (io_status != SDL_IO_STATUS_NOT_READY) {
                    SDLTest_Log("SDL_GetIOStatus(process_stdin) returns %d, breaking.", io_status);
                    break;
                }
            }
            total_written += amount_written;
            SDL_FlushIO(process_stdin);
        }

        /* FIXME: this needs a rate limit */
        if (log_this_iteration) {
            SDLTest_AssertPass("About to SDL_ReadIO (%dth time)", iteration_count);
        }
        amount_read = SDL_ReadIO(process_stdout, local_buffer, sizeof(local_buffer));
        if (log_this_iteration) {
            SDLTest_Log("SDL_ReadIO() -> %u (%dth time)", (unsigned)amount_read, iteration_count);
        }
        if (amount_read == 0) {
            io_status = SDL_GetIOStatus(process_stdout);
            if (io_status != SDL_IO_STATUS_NOT_READY) {
                SDLTest_Log("SDL_GetIOStatus(process_stdout) returned %d, breaking.", io_status);
                break;
            }
        } else {
            total_read += amount_read;
            SDL_WriteIO(stdout_stream, local_buffer, amount_read);
            stdout_stream_buf = SDL_GetPointerProperty(SDL_GetIOProperties(stdout_stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
            if (SDL_strstr(stdout_stream_buf, "EOF")) {
                SDLTest_Log("Found EOF in stdout");
                break;
            }
        }
        SDL_Delay(10);
    }
    SDLTest_Log("Wrote %" SDL_PRIu64 " bytes to process.stdin", (Uint64)total_written);
    SDLTest_Log("Read %" SDL_PRIu64 " bytes from process.stdout",(Uint64)total_read);

    stdout_stream_buf = SDL_GetPointerProperty(SDL_GetIOProperties(stdout_stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
    SDLTest_CompareMemory(stdout_stream_buf, total_written, text_in, text_in_size);

    exit_code = 0xdeadbeef;
    wait_result = SDL_WaitProcess(process, false, &exit_code);
    SDLTest_AssertCheck(wait_result == false, "Process should not have closed yet");

    SDLTest_AssertPass("About to close stdin");
    /* Closing stdin of `subprocessstdin --stdin-to-stdout` should close the process */
    SDL_CloseIO(process_stdin);

    process_stdin = SDL_GetProcessInput(process);
    SDLTest_AssertCheck(process_stdin == NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDIN_POINTER) is cleared after close");

    SDLTest_AssertPass("About to wait on process");
    exit_code = 0xdeadbeef;
    wait_result = SDL_WaitProcess(process, true, &exit_code);
    SDLTest_AssertCheck(wait_result == true, "Process should have closed when closing stdin");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (!wait_result) {
        bool killed;
        SDL_Log("About to kill process");
        killed = SDL_KillProcess(process, true);
        SDLTest_AssertCheck(killed, "SDL_KillProcess succeeded");
    }
    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    SDL_CloseIO(stdout_stream);
    SDL_free(text_in);
    return TEST_COMPLETED;
failed:

    SDL_DestroyProcess(process);
    SDL_CloseIO(stdout_stream);
    SDL_free(text_in);
    return TEST_ABORTED;
}

static int process_testStdinToStderr(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--stdin-to-stderr",
        NULL,
    };
    SDL_Process *process = NULL;
    SDL_IOStream *process_stdin = NULL;
    SDL_IOStream *process_stdout = NULL;
    SDL_IOStream *process_stderr = NULL;
    const char *text_in = "Tests whether we can write to stdin and read from stderr\r\n{'succes': true, 'message': 'Success!'}\r\nYippie ka yee\r\nEOF";
    size_t result;
    int exit_code;
    SDL_PropertiesID props;
    char buffer[256];
    size_t amount_read;

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_APP);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcessWithProperties()");
    if (!process) {
        goto failed;
    }

    SDLTest_AssertPass("About to write to process");
    process_stdin = SDL_GetProcessInput(process);
    SDLTest_AssertCheck(process_stdin != NULL, "SDL_GetProcessInput()");
    result = SDL_WriteIO(process_stdin, text_in, SDL_strlen(text_in));
    SDLTest_AssertCheck(result == SDL_strlen(text_in), "SDL_WriteIO() wrote %d, expected %d", (int)result, (int)SDL_strlen(text_in));
    SDL_CloseIO(process_stdin);

    process_stdout = SDL_GetProcessOutput(process);
    SDLTest_AssertCheck(process_stdout == NULL, "Process has no stdout");

    process_stderr = SDL_GetPointerProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_STDERR_POINTER, NULL);
    SDLTest_AssertCheck(process_stderr != NULL, "Process has stderr");

    exit_code = 0xdeadbeef;
    result = SDL_WaitProcess(process, true, &exit_code);
    SDLTest_AssertCheck(result == true, "Process should have finished");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);

    amount_read = SDL_ReadIO(process_stderr, buffer, sizeof(buffer));
    SDLTest_CompareMemory(buffer, amount_read, text_in, SDL_strlen(text_in));

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;

failed:
    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

static int process_testSimpleStdinToStdout(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--stdin-to-stdout",
        NULL,
    };
    SDL_Process *process = NULL;
    SDL_IOStream *input = NULL;
    const char *text_in = "Tests whether we can write to stdin and read from stdout\r\n{'succes': true, 'message': 'Success!'}\r\nYippie ka yee\r\nEOF";
    char *buffer;
    size_t result;
    int exit_code;
    size_t total_read = 0;

    process = SDL_CreateProcess(process_args, true);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess()");
    if (!process) {
        goto failed;
    }

    SDLTest_AssertPass("About to write to process");
    input = SDL_GetProcessInput(process);
    SDLTest_AssertCheck(input != NULL, "SDL_GetProcessInput()");
    result = SDL_WriteIO(input, text_in, SDL_strlen(text_in));
    SDLTest_AssertCheck(result == SDL_strlen(text_in), "SDL_WriteIO() wrote %d, expected %d", (int)result, (int)SDL_strlen(text_in));
    SDL_CloseIO(input);

    input = SDL_GetProcessInput(process);
    SDLTest_AssertCheck(input == NULL, "SDL_GetProcessInput() after close");

    exit_code = 0xdeadbeef;
    buffer = (char *)SDL_ReadProcess(process, &total_read, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (!buffer) {
        goto failed;
    }

    SDLTest_LogEscapedString("Expected text read from subprocess: %s", text_in, SDL_strlen(text_in));
    SDLTest_LogEscapedString("Actual text read from subprocess: %s", buffer, total_read);
    SDLTest_AssertCheck(total_read == SDL_strlen(text_in), "Expected to read %u bytes, actually read %u bytes", (unsigned)SDL_strlen(text_in), (unsigned)total_read);
    SDLTest_AssertCheck(SDL_strcmp(buffer, text_in) == 0, "Subprocess stdout should match text written to stdin");
    SDL_free(buffer);

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;

failed:
    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

static int process_testMultiprocessStdinToStdout(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--stdin-to-stdout",
        "--log-stdin",
        NULL,
        NULL,
    };
    SDL_Process *process1 = NULL;
    SDL_Process *process2 = NULL;
    SDL_PropertiesID props;
    SDL_IOStream *input = NULL;
    const char *text_in = "Tests whether we can write to stdin and read from stdout\r\n{'succes': true, 'message': 'Success!'}\r\nYippie ka yee\r\nEOF";
    char *buffer;
    size_t result;
    int exit_code;
    size_t total_read = 0;
    bool finished;

    process_args[3] = "child1-stdin.txt";
    process1 = SDL_CreateProcess(process_args, true);
    SDLTest_AssertCheck(process1 != NULL, "SDL_CreateProcess()");
    if (!process1) {
        goto failed;
    }

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_POINTER, SDL_GetPointerProperty(SDL_GetProcessProperties(process1), SDL_PROP_PROCESS_STDOUT_POINTER, NULL));
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDLTest_AssertPass("About to call SDL_CreateProcessWithProperties");
    process_args[3] = "child2-stdin.txt";
    process2 = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(process2 != NULL, "SDL_CreateProcess()");
    if (!process2) {
        goto failed;
    }

    SDLTest_AssertPass("About to write to process");
    input = SDL_GetProcessInput(process1);
    SDLTest_AssertCheck(input != NULL, "SDL_GetProcessInput()");
    result = SDL_WriteIO(input, text_in, SDL_strlen(text_in));
    SDLTest_AssertCheck(result == SDL_strlen(text_in), "SDL_WriteIO() wrote %d, expected %d", (int)result, (int)SDL_strlen(text_in));
    SDL_CloseIO(input);

    exit_code = 0xdeadbeef;
    finished = SDL_WaitProcess(process1, true, &exit_code);
    SDLTest_AssertCheck(finished == true, "process 1 should have finished");
    SDLTest_AssertCheck(exit_code == 0, "Exit code of process 1 should be 0, is %d", exit_code);

    exit_code = 0xdeadbeef;
    buffer = (char *)SDL_ReadProcess(process2, &total_read, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code of process 2 should be 0, is %d", exit_code);
    if (!buffer) {
        goto failed;
    }

    SDLTest_LogEscapedString("Expected text read from subprocess: ", text_in, SDL_strlen(text_in));
    SDLTest_LogEscapedString("Actual text read from subprocess: ", buffer, total_read);
    SDLTest_AssertCheck(total_read == SDL_strlen(text_in), "Expected to read %u bytes, actually read %u bytes", (unsigned)SDL_strlen(text_in), (unsigned)total_read);
    SDLTest_AssertCheck(SDL_strcmp(buffer, text_in) == 0, "Subprocess stdout should match text written to stdin");
    SDL_free(buffer);
    SDLTest_AssertPass("About to destroy processes");
    SDL_DestroyProcess(process1);
    SDL_DestroyProcess(process2);
    return TEST_COMPLETED;

failed:
    SDL_DestroyProcess(process1);
    SDL_DestroyProcess(process2);
    return TEST_ABORTED;
}

static int process_testWriteToFinishedProcess(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        NULL,
    };
    SDL_Process *process = NULL;
    bool result;
    int exit_code;
    SDL_IOStream *process_stdin;
    const char *text_in = "text_in";

    SDLTest_AssertPass("About to call SDL_CreateProcess");
    process = SDL_CreateProcess(process_args, true);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess()");
    if (!process) {
        goto failed;
    }

    exit_code = 0xdeadbeef;
    SDLTest_AssertPass("About to call SDL_WaitProcess");
    result = SDL_WaitProcess(process, true, &exit_code);
    SDLTest_AssertCheck(result, "SDL_WaitProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);

    process_stdin = SDL_GetProcessInput(process);
    SDLTest_AssertCheck(process_stdin != NULL, "SDL_GetProcessInput returns non-Null SDL_IOStream");
    SDLTest_AssertPass("About to call SDL_WriteIO on dead child process");
    SDL_WriteIO(process_stdin, text_in, SDL_strlen(text_in));

    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;

failed:
    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

static int process_testNonExistingExecutable(void *arg)
{
    static const int STEM_LENGTH = 16;
    char **process_args;
    char *random_stem;
    char *random_path;
    SDL_Process *process = NULL;

    random_stem = SDLTest_RandomAsciiStringOfSize(STEM_LENGTH);
    random_path = SDL_malloc(STEM_LENGTH + SDL_strlen(EXE) + 1);
    SDL_snprintf(random_path, STEM_LENGTH + SDL_strlen(EXE) + 1, "%s%s", random_stem, EXE);
    SDL_free(random_stem);
    SDLTest_AssertCheck(!SDL_GetPathInfo(random_path, NULL), "%s does not exist", random_path);

    process_args = CreateArguments(0, random_path, NULL);
    SDL_free(random_path);

    SDLTest_AssertPass("About to call SDL_CreateProcess");
    process = SDL_CreateProcess((const char * const *)process_args, false);
    SDLTest_AssertCheck(process == NULL, "SDL_CreateProcess() should have failed (%s)", SDL_GetError());

    DestroyStringArray(process_args);
    return TEST_COMPLETED;
}

static int process_testBatBadButVulnerability(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    char *inject_arg = NULL;
    char **process_args = NULL;
    char *text_out = NULL;
    size_t len_text_out;
    int exitcode;
    SDL_Process *process = NULL;
    SDL_IOStream *child_bat;
    char buffer[256];

#ifndef SDL_PLATFORM_WINDOWS
    SDLTest_AssertPass("The BatBadBut vulnerability only applied to Windows");
    return TEST_SKIPPED;
#endif
    /* FIXME: remove child.bat at end of loop and/or create in temporary directory */
    child_bat = SDL_IOFromFile("child_batbadbut.bat", "w");
    SDL_IOprintf(child_bat, "@echo off\necho Hello from child_batbadbut.bat\necho \"|bat1=%%1|\"\n");
    SDL_CloseIO(child_bat);

    inject_arg = SDL_malloc(SDL_strlen(data->childprocess_path) + 100);
    SDL_snprintf(inject_arg, SDL_strlen(data->childprocess_path) + 100, "\"&%s --version  --print-arguments --stdout OWNEDSTDOUT\"", data->childprocess_path);
    process_args = CreateArguments(0, "child_batbadbut.bat", inject_arg, NULL);

    SDLTest_AssertPass("About to call SDL_CreateProcess");
    process = SDL_CreateProcess((const char * const*)process_args, true);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess");
    if (!process) {
        goto cleanup;
    }
    text_out = SDL_ReadProcess(process, &len_text_out, &exitcode);
    SDLTest_AssertCheck(exitcode == 0, "process exited with exitcode 0, was %d", exitcode);
    SDLTest_AssertCheck(text_out != NULL, "SDL_ReadProcess returned data");
    SDLTest_LogEscapedString("Output: ", text_out, len_text_out);
    if (!text_out) {
        goto cleanup;
    }

    SDLTest_AssertCheck(SDL_strstr(text_out, "Hello from child_batbadbut") != NULL, "stdout contains 'Hello from child'");
    SDLTest_AssertCheck(SDL_strstr(text_out, "SDL version") == NULL, "stdout should not contain SDL version");
    SDL_snprintf(buffer, sizeof(buffer), "|bat1=\"\"\"&%s\"\"|", process_args[1] + 2);
    SDLTest_LogEscapedString("stdout should contain: ", buffer, SDL_strlen(buffer));
    SDLTest_AssertCheck(SDL_strstr(text_out, buffer) != NULL, "Verify first argument");

cleanup:
    SDL_free(text_out);
    SDL_DestroyProcess(process);
    SDL_free(inject_arg);
    DestroyStringArray(process_args);
    return TEST_COMPLETED;
}

static int process_testFileRedirection(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    SDL_PropertiesID props = 0;
    const char * process_args[] = {
        data->childprocess_path,
        "--stdin-to-stdout",
        "--stdin-to-stderr",
        NULL,
    };
    const char TEXT_REF[] = "This is input for the child process";
    static const char *PATH_STDIN = "test_redirection_stdin.txt";
    static const char *PATH_STDOUT = "test_redirection_stdout.txt";
    static const char *PATH_STDERR = "test_redirection_stderr.txt";
    char *text_out = NULL;
    size_t len_text_out;
    int exitcode;
    bool result;
    SDL_Process *process = NULL;
    SDL_IOStream *stream;
    SDL_IOStream *input_stream = NULL;
    SDL_IOStream *output_stream = NULL;
    SDL_IOStream *error_stream = NULL;

    stream = SDL_IOFromFile(PATH_STDIN, "w");
    SDLTest_AssertCheck(stream != NULL, "SDL_IOFromFile(\"%s\", \"w\")", PATH_STDIN);
    if (!stream) {
        goto cleanup;
    }
    SDL_WriteIO(stream, TEXT_REF, sizeof(TEXT_REF));
    SDL_CloseIO(stream);

    input_stream = SDL_IOFromFile(PATH_STDIN, "r");
    SDLTest_AssertCheck(input_stream != NULL, "SDL_IOFromFile(\"%s\", \"r\")", PATH_STDIN);
    if (!input_stream) {
        goto cleanup;
    }

    output_stream = SDL_IOFromFile(PATH_STDOUT, "w");
    SDLTest_AssertCheck(output_stream != NULL, "SDL_IOFromFile(\"%s\", \"w\")", PATH_STDOUT);
    if (!output_stream) {
        goto cleanup;
    }

    error_stream = SDL_IOFromFile(PATH_STDERR, "w");
    SDLTest_AssertCheck(error_stream != NULL, "SDL_IOFromFile(\"%s\", \"w\")", PATH_STDERR);
    if (!error_stream) {
        goto cleanup;
    }

    props =  SDL_CreateProperties();
    SDLTest_AssertCheck(props != 0, "SDL_CreateProperties()");
    if (!props) {
        goto cleanup;
    }
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_POINTER, (void *)input_stream);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, (void *)output_stream);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_POINTER, (void *)error_stream);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcessWithProperties (%s)", SDL_GetError());
    if (!process) {
        goto cleanup;
    }

    exitcode = 0xdeadbeef;
    text_out = SDL_ReadProcess(process, &len_text_out, &exitcode);
    SDLTest_AssertCheck(text_out == NULL, "SDL_ReadProcess should not be able to close a redirected process (%s)", SDL_GetError());
    SDLTest_AssertCheck(len_text_out == 0, "length written by SDL_ReadProcess should be 0");
    SDL_free(text_out);
    text_out = NULL;

    exitcode = 0xdeadbeef;
    result = SDL_WaitProcess(process, true, &exitcode);
    SDLTest_AssertCheck(result, "process must have exited");
    SDLTest_AssertCheck(exitcode == 0, "process exited with exitcode 0, was %d", exitcode);

    SDL_CloseIO(input_stream);
    input_stream = NULL;
    SDL_CloseIO(output_stream);
    output_stream = NULL;
    SDL_CloseIO(error_stream);
    error_stream = NULL;

    text_out = SDL_LoadFile(PATH_STDOUT, &len_text_out);
    SDLTest_AssertCheck(text_out != NULL, "SDL_LoadFile(\"%s\") succeeded (%s)", PATH_STDOUT, SDL_GetError());
    SDLTest_AssertPass("Comparing stdout with reference");
    SDLTest_CompareMemory(text_out, len_text_out, TEXT_REF, sizeof(TEXT_REF));
    SDL_free(text_out);

    text_out = SDL_LoadFile(PATH_STDERR, &len_text_out);
    SDLTest_AssertCheck(text_out != NULL, "SDL_LoadFile(\"%s\") succeeded (%s)", PATH_STDERR, SDL_GetError());
    SDLTest_AssertPass("Comparing stderr with reference");
    SDLTest_CompareMemory(text_out, len_text_out, TEXT_REF, sizeof(TEXT_REF));
    SDL_free(text_out);

cleanup:
    SDL_CloseIO(input_stream);
    SDL_CloseIO(output_stream);
    SDL_CloseIO(error_stream);
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;
}

static const SDLTest_TestCaseReference processTestArguments = {
    process_testArguments, "process_testArguments", "Test passing arguments to child process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestExitCode = {
    process_testexitCode, "process_testExitCode", "Test exit codes", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestInheritedEnv = {
    process_testInheritedEnv, "process_testInheritedEnv", "Test inheriting environment from parent process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestNewEnv = {
    process_testNewEnv, "process_testNewEnv", "Test creating new environment for child process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestKill = {
    process_testKill, "process_testKill", "Test Killing a child process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestStdinToStdout = {
    process_testStdinToStdout, "process_testStdinToStdout", "Test writing to stdin and reading from stdout", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestStdinToStderr = {
    process_testStdinToStderr, "process_testStdinToStderr", "Test writing to stdin and reading from stderr", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestSimpleStdinToStdout = {
    process_testSimpleStdinToStdout, "process_testSimpleStdinToStdout", "Test writing to stdin and reading from stdout using the simplified API", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestMultiprocessStdinToStdout = {
    process_testMultiprocessStdinToStdout, "process_testMultiprocessStdinToStdout", "Test writing to stdin and reading from stdout using the simplified API", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestWriteToFinishedProcess = {
    process_testWriteToFinishedProcess, "process_testWriteToFinishedProcess", "Test writing to stdin of terminated process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestNonExistingExecutable = {
    process_testNonExistingExecutable, "process_testNonExistingExecutable", "Test running a non-existing executable", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestBatBadButVulnerability = {
    process_testBatBadButVulnerability, "process_testBatBadButVulnerability", "Test BatBadBut vulnerability: command injection through cmd.exe", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestFileRedirection = {
    process_testFileRedirection, "process_testFileRedirection", "Test redirection from/to files", TEST_ENABLED
};

static const SDLTest_TestCaseReference *processTests[] = {
    &processTestArguments,
    &processTestExitCode,
    &processTestInheritedEnv,
    &processTestNewEnv,
    &processTestKill,
    &processTestStdinToStdout,
    &processTestStdinToStderr,
    &processTestSimpleStdinToStdout,
    &processTestMultiprocessStdinToStdout,
    &processTestWriteToFinishedProcess,
    &processTestNonExistingExecutable,
    &processTestBatBadButVulnerability,
    &processTestFileRedirection,
    NULL
};

static SDLTest_TestSuiteReference processTestSuite = {
    "Process",
    setUpProcess,
    processTests,
    NULL
};

static SDLTest_TestSuiteReference *testSuites[] = {
    &processTestSuite,
    NULL
};

int main(int argc, char *argv[])
{
    int i;
    int result;
    SDLTest_CommonState *state;
    SDLTest_TestSuiteRunner *runner;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    runner = SDLTest_CreateTestSuiteRunner(state, testSuites);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!parsed_args.childprocess_path) {
                parsed_args.childprocess_path = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!parsed_args.childprocess_path) {
        SDLTest_CommonLogUsage(state, argv[0], options);
        return 1;
    }

    result = SDLTest_ExecuteTestSuiteRunner(runner);

    SDL_Quit();
    SDLTest_DestroyTestSuiteRunner(runner);
    SDLTest_CommonDestroyState(state);
    return result;
}
