#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#include <stdio.h>
#include <errno.h>

#ifdef SDL_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

int main(int argc, char *argv[]) {
    SDLTest_CommonState *state;
    int i;
    bool print_arguments = false;
    bool print_environment = false;
    bool stdin_to_stdout = false;
    bool read_stdin = false;
    bool stdin_to_stderr = false;
    int exit_code = 0;

    state = SDLTest_CommonCreateState(argv, 0);

    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--print-arguments") == 0) {
                print_arguments = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--print-environment") == 0) {
                print_environment = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--stdin-to-stdout") == 0) {
                stdin_to_stdout = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--stdin-to-stderr") == 0) {
                stdin_to_stderr = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--stdin") == 0) {
                read_stdin = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--stdout") == 0) {
                if (i + 1 < argc) {
                    fprintf(stdout, "%s", argv[i + 1]);
                    consumed = 2;
                }
            } else if (SDL_strcmp(argv[i], "--stderr") == 0) {
                if (i + 1 < argc) {
                    fprintf(stderr, "%s", argv[i + 1]);
                    consumed = 2;
                }
            } else if (SDL_strcmp(argv[i], "--exit-code") == 0) {
                if (i + 1 < argc) {
                    char *endptr = NULL;
                    exit_code = SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr && *endptr == '\0') {
                        consumed = 2;
                    }
                }
            } else if (SDL_strcmp(argv[i], "--version") == 0) {
                int version = SDL_GetVersion();
                fprintf(stdout, "SDL version %d.%d.%d",
                    SDL_VERSIONNUM_MAJOR(version),
                    SDL_VERSIONNUM_MINOR(version),
                    SDL_VERSIONNUM_MICRO(version));
                fprintf(stderr, "SDL version %d.%d.%d",
                    SDL_VERSIONNUM_MAJOR(version),
                    SDL_VERSIONNUM_MINOR(version),
                    SDL_VERSIONNUM_MICRO(version));
                consumed = 1;
                break;
            } else if (SDL_strcmp(argv[i], "--") == 0) {
                i++;
                break;
            }
        }
        if (consumed <= 0) {
            const char *args[] = {
                "[--print-arguments]",
                "[--print-environment]",
                "[--stdin]",
                "[--stdin-to-stdout]",
                "[--stdout TEXT]",
                "[--stdin-to-stderr]",
                "[--stderr TEXT]",
                "[--exit-code EXIT_CODE]",
                "[--] [ARG [ARG ...]]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], args);
            return 1;
        }
        i += consumed;
    }

    if (print_arguments) {
        int print_i;
        for (print_i = 0; i + print_i < argc; print_i++) {
            fprintf(stdout, "|%d=%s|\r\n", print_i, argv[i + print_i]);
        }
    }

    if (print_environment) {
        char **env = SDL_GetEnvironmentVariables(SDL_GetEnvironment());
        if (env) {
            for (i = 0; env[i]; ++i) {
                fprintf(stdout, "%s\n", env[i]);
            }
            SDL_free(env);
        }
    }

#ifdef SDL_PLATFORM_WINDOWS
    {
        DWORD mode;
        HANDLE stdout_handle = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(stdout_handle, &mode);
        SetConsoleMode(stdout_handle, mode & ~(ENABLE_LINE_INPUT));
    }
#else
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~(O_NONBLOCK));
#endif

    if (stdin_to_stdout || stdin_to_stderr || read_stdin) {
        for (;;) {
            char buffer[4 * 4096];
            size_t result;

            result = fread(buffer, 1, sizeof(buffer), stdin);
            if (result == 0) {
                if (errno == EAGAIN) {
                    clearerr(stdin);
                    SDL_Delay(20);
                    continue;
                }
                break;
            }
            if (stdin_to_stdout) {
                fwrite(buffer, 1, result, stdout);
                fflush(stdout);
            }
            if (stdin_to_stderr) {
                fwrite(buffer, 1, result, stderr);
            }
        }
    }

    SDLTest_CommonDestroyState(state);

    return exit_code;
}
