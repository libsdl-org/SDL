#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#include <stdio.h>
#include <errno.h>


int main(int argc, char *argv[]) {
    SDLTest_CommonState *state;
    int i;
    const char *expect_environment = NULL;
    bool expect_environment_match = false;
    bool print_arguments = false;
    bool print_environment = false;
    bool stdin_to_stdout = false;
    bool stdin_to_stderr = false;
    int exit_code = 0;

    state = SDLTest_CommonCreateState(argv, 0);

    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (SDL_strcmp(argv[i], "--print-arguments") == 0) {
            print_arguments = true;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--print-environment") == 0) {
            print_environment = true;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--expect-env") == 0) {
            if (i + 1 < argc) {
                expect_environment = argv[i + 1];
                consumed = 2;
            }
        } else if (SDL_strcmp(argv[i], "--stdin-to-stdout") == 0) {
            stdin_to_stdout = true;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--stdin-to-stderr") == 0) {
            stdin_to_stderr = true;
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
        } else if (SDL_strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (consumed <= 0) {
            const char *args[] = {
                "[--print-arguments]",
                "[--print-environment]",
                "[--expect-env KEY=VAL]",
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

    if (print_environment || expect_environment) {
        char **env = SDL_GetEnvironmentVariables(SDL_GetEnvironment());
        if (env) {
            for (i = 0; env[i]; ++i) {
                if (print_environment) {
                    fprintf(stdout, "%s\n", env[i]);
                }
                if (expect_environment) {
                    expect_environment_match |= SDL_strcmp(env[i], expect_environment) == 0;
                }
            }
            SDL_free(env);
        }
    }

    if (stdin_to_stdout || stdin_to_stderr) {

        for (;;) {
            int c;
            c = fgetc(stdin);
            if (c == EOF) {
                if (errno == EAGAIN) {
                    clearerr(stdin);
                    SDL_Delay(10);
                    continue;
                }
                break;
            }
            if (stdin_to_stdout) {
                fputc(c, stdout);
                fflush(stdout);
            }
            if (stdin_to_stderr) {
                fputc(c, stderr);
            }
        }
    }

    SDLTest_CommonDestroyState(state);

    if (expect_environment && !expect_environment_match) {
        exit_code |= 0x1;
    }
    return exit_code;
}
