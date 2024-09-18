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
 * - stdin to stdout
 * - stdin to stderr
 * - read env, using env set by parent process
 * - exit codes
 * - kill process
 * - waiting twice on process
 * - executing a non-existing program
 * - executing a process linking to a shared library not in the search paths
 * - piping processes
 * - forwarding SDL_IOFromFile stream to process
 * - forwarding process to SDL_IOFromFile stream
 */

typedef struct {
    const char *childprocess_path;
} TestProcessData;

static TestProcessData parsed_args;

static void SDLCALL setUpProcess(void **arg) {
    *arg = &parsed_args;
}

static const char *options[] = { "/path/to/childprocess" EXE, NULL };

static SDL_Environment *DuplicateEnvironment(const char *key0, ...)
{
    va_list ap;
    const char *keyN;
    SDL_Environment *env = SDL_GetEnvironment();
    SDL_Environment *new_env = SDL_CreateEnvironment(false);

    if (key0) {
        char *sep = SDL_strchr(key0, '=');
        if (sep) {
            *sep = '\0';
            SDL_SetEnvironmentVariable(new_env, key0, sep + 1, true);
            *sep = '=';
            SDL_SetEnvironmentVariable(new_env, key0, sep, true);
        } else {
            SDL_SetEnvironmentVariable(new_env, key0, SDL_GetEnvironmentVariable(env, key0), true);
        }
        va_start(ap, key0);
        for (;;) {
            keyN = va_arg(ap, const char *);
            if (keyN) {
                sep = SDL_strchr(keyN, '=');
                if (sep) {
                    *sep = '\0';
                    SDL_SetEnvironmentVariable(new_env, keyN, sep + 1, true);
                    *sep = '=';
                } else {
                    SDL_SetEnvironmentVariable(new_env, keyN, SDL_GetEnvironmentVariable(env, keyN), true);
                }
            } else {
                break;
            }
        }
        va_end(ap);
    }

    return new_env;
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

    process = SDL_CreateProcess(process_args, true);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess()");
    if (!process) {
        goto failed;
    }

    exit_code = 0xdeadbeef;
    buffer = (char *)SDL_ReadProcess(process, NULL, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (!buffer) {
        goto failed;
    }

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

static int SDLCALL process_testInheritedEnv(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--print-environment",
        "--expect-env", NULL,
        NULL,
    };
    SDL_PropertiesID props;
    SDL_Process *process = NULL;
    Sint64 pid;
    SDL_IOStream *process_stdout = NULL;
    char buffer[256];
    bool wait_result;
    int exit_code;
    static const char *const TEST_ENV_KEY = "testprocess_environment";
    char *test_env_val = NULL;

    test_env_val = SDLTest_RandomAsciiStringOfSize(32);
    SDLTest_AssertPass("Setting parent environment variable %s=%s", TEST_ENV_KEY, test_env_val);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), TEST_ENV_KEY, test_env_val, true);
    SDL_snprintf(buffer, sizeof(buffer), "%s=%s", TEST_ENV_KEY, test_env_val);
    process_args[3] = buffer;

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)process_args);
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

    process_stdout = SDL_GetProcessOutput(process);
    SDLTest_AssertCheck(process_stdout != NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDOUT_POINTER) returns a valid IO stream");
    if (!process_stdout) {
        goto failed;
    }

    for (;;) {
        size_t amount_read;

        amount_read = SDL_ReadIO(process_stdout, buffer, sizeof(buffer) - 1);
        if (amount_read > 0) {
            buffer[amount_read] = '\0';
            SDLTest_Log("READ: %s", buffer);
        } else if (SDL_GetIOStatus(process_stdout) != SDL_IO_STATUS_NOT_READY) {
            break;
        }
        SDL_Delay(10);
    }

    SDLTest_AssertPass("About to wait on process");
    exit_code = 0xdeadbeef;
    wait_result = SDL_WaitProcess(process, true, &exit_code);
    SDLTest_AssertCheck(wait_result == true, "Process should have closed when closing stdin");
    SDLTest_AssertPass("exit_code will be != 0 when environment variable was not set");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    SDL_free(test_env_val);
    return TEST_COMPLETED;
failed:
    SDL_free(test_env_val);
    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

static int SDLCALL process_testNewEnv(void *arg)
{
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->childprocess_path,
        "--print-environment",
        "--expect-env", NULL,
        NULL,
    };
    SDL_Environment *process_env;
    SDL_PropertiesID props;
    SDL_Process *process = NULL;
    Sint64 pid;
    SDL_IOStream *process_stdout = NULL;
    char buffer[256];
    bool wait_result;
    int exit_code;
    static const char *const TEST_ENV_KEY = "testprocess_environment";
    char *test_env_val = NULL;

    test_env_val = SDLTest_RandomAsciiStringOfSize(32);
    SDL_snprintf(buffer, sizeof(buffer), "%s=%s", TEST_ENV_KEY, test_env_val);
    process_args[3] = buffer;

    process_env = DuplicateEnvironment("PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", buffer, NULL);

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

    process_stdout = SDL_GetProcessOutput(process);
    SDLTest_AssertCheck(process_stdout != NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDOUT_POINTER) returns a valid IO stream");
    if (!process_stdout) {
        goto failed;
    }

    for (;;) {
        size_t amount_read;

        amount_read = SDL_ReadIO(process_stdout, buffer, sizeof(buffer) - 1);
        if (amount_read > 0) {
            buffer[amount_read] = '\0';
            SDLTest_Log("READ: %s", buffer);
        } else if (SDL_GetIOStatus(process_stdout) != SDL_IO_STATUS_NOT_READY) {
            break;
        }
        SDL_Delay(10);
    }

    SDLTest_AssertPass("About to wait on process");
    exit_code = 0xdeadbeef;
    wait_result = SDL_WaitProcess(process, true, &exit_code);
    SDLTest_AssertCheck(wait_result == true, "Process should have closed when closing stdin");
    SDLTest_AssertPass("exit_code will be != 0 when environment variable was not set");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    SDLTest_AssertPass("About to destroy process");
    SDL_free(test_env_val);
    SDL_DestroyProcess(process);
    SDL_DestroyEnvironment(process_env);
    return TEST_COMPLETED;

failed:
    SDL_free(test_env_val);
    SDL_DestroyProcess(process);
    SDL_DestroyEnvironment(process_env);
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
    const char *text_in = "Tests whether we can write to stdin and read from stdout\r\n{'succes': true, 'message': 'Success!'}\r\nYippie ka yee\r\nEOF";
    size_t amount_written;
    size_t amount_to_write;
    char buffer[128];
    size_t total_read;
    bool wait_result;
    int exit_code;

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
    if (!process_stdin || !process_stdout) {
        goto failed;
    }
    SDLTest_AssertPass("About to write to process");
    amount_to_write = SDL_strlen(text_in);
    amount_written = SDL_WriteIO(process_stdin, text_in, amount_to_write);
    SDLTest_AssertCheck(amount_written == amount_to_write, "SDL_WriteIO(subprocess.stdin) wrote %" SDL_PRIu64 " bytes, expected %" SDL_PRIu64, (Uint64)amount_written, (Uint64)amount_to_write);
    if (amount_to_write != amount_written) {
        goto failed;
    }
    SDL_FlushIO(process_stdin);

    total_read = 0;
    buffer[0] = '\0';
    for (;;) {
        size_t amount_read;
        if (total_read >= sizeof(buffer) - 1) {
            SDLTest_AssertCheck(0, "Buffer is too small for input data.");
            goto failed;
        }

        SDLTest_AssertPass("About to read from process");
        amount_read = SDL_ReadIO(process_stdout, buffer + total_read, sizeof(buffer) - total_read - 1);
        if (amount_read == 0 && SDL_GetIOStatus(process_stdout) != SDL_IO_STATUS_NOT_READY) {
            break;
        }
        total_read += amount_read;
        buffer[total_read] = '\0';
        if (total_read >= sizeof(buffer) - 1 || SDL_strstr(buffer, "EOF")) {
            break;
        }
        SDL_Delay(10);
    }
    SDLTest_Log("Text read from subprocess: %s", buffer);
    SDLTest_AssertCheck(SDL_strcmp(buffer, text_in) == 0, "Subprocess stdout should match text written to stdin");

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
    buffer = (char *)SDL_ReadProcess(process, NULL, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (!buffer) {
        goto failed;
    }

    SDLTest_Log("Text read from subprocess: %s", buffer);
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
    buffer = (char *)SDL_ReadProcess(process2, NULL, &exit_code);
    SDLTest_AssertCheck(buffer != NULL, "SDL_ReadProcess()");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (!buffer) {
        goto failed;
    }

    SDLTest_Log("Text read from subprocess: %s", buffer);
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

static const SDLTest_TestCaseReference processTestArguments = {
    process_testArguments, "process_testArguments", "Test passing arguments to child process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestIneritedEnv = {
    process_testInheritedEnv, "process_testInheritedEnv", "Test inheriting environment from parent process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestNewEnv = {
    process_testNewEnv, "process_testNewEnv", "Test creating new environment for child process", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestStdinToStdout = {
    process_testStdinToStdout, "process_testStdinToStdout", "Test writing to stdin and reading from stdout", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestSimpleStdinToStdout = {
    process_testSimpleStdinToStdout, "process_testSimpleStdinToStdout", "Test writing to stdin and reading from stdout using the simplified API", TEST_ENABLED
};

static const SDLTest_TestCaseReference processTestMultiprocessStdinToStdout = {
    process_testMultiprocessStdinToStdout, "process_testMultiprocessStdinToStdout", "Test writing to stdin and reading from stdout using the simplified API", TEST_ENABLED
};

static const SDLTest_TestCaseReference *processTests[] = {
    &processTestArguments,
    &processTestIneritedEnv,
    &processTestNewEnv,
    &processTestStdinToStdout,
    &processTestSimpleStdinToStdout,
    &processTestMultiprocessStdinToStdout,
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
