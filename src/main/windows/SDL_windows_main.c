/*
    SDL_windows_main.c, placed in the public domain by Sam Lantinga  4/13/98

    The WinMain function -- calls your program's main() function
*/
#include "SDL_config.h"

#ifdef __WIN32__

/* Include this so we define UNICODE properly */
#include "../../core/windows/SDL_windows.h"

/* Include the SDL main definition header */
#include "SDL.h"
#include "SDL_main.h"

#ifdef main
#  undef main
#endif /* main */

static void
UnEscapeQuotes(char *arg)
{
    char *last = NULL;

    while (*arg) {
        if (*arg == '"' && (last != NULL && *last == '\\')) {
            char *c_curr = arg;
            char *c_last = last;

            while (*c_curr) {
                *c_last = *c_curr;
                c_last = c_curr;
                c_curr++;
            }
            *c_last = '\0';
        }
        last = arg;
        arg++;
    }
}

/* Parse a command line buffer into arguments */
static int
ParseCommandLine(char *cmdline, char **argv)
{
    char *bufp;
    char *lastp = NULL;
    int argc, last_argc;

    argc = last_argc = 0;
    for (bufp = cmdline; *bufp;) {
        /* Skip leading whitespace */
        while (*bufp == ' ' || *bufp == '\t') {
            ++bufp;
        }
        /* Skip over argument */
        if (*bufp == '"') {
            ++bufp;
            if (*bufp) {
                if (argv) {
                    argv[argc] = bufp;
                }
                ++argc;
            }
            /* Skip over word */
            lastp = bufp;
            while (*bufp && (*bufp != '"' || *lastp == '\\')) {
                lastp = bufp;
                ++bufp;
            }
        } else {
            if (*bufp) {
                if (argv) {
                    argv[argc] = bufp;
                }
                ++argc;
            }
            /* Skip over word */
            while (*bufp && (*bufp != ' ' && *bufp != '\t')) {
                ++bufp;
            }
        }
        if (*bufp) {
            if (argv) {
                *bufp = '\0';
            }
            ++bufp;
        }

        /* Strip out \ from \" sequences */
        if (argv && last_argc != argc) {
            UnEscapeQuotes(argv[last_argc]);
        }
        last_argc = argc;
    }
    if (argv) {
        argv[argc] = NULL;
    }
    return (argc);
}

/* Pop up an out of memory message, returns to Windows */
static BOOL
OutOfMemory(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Out of memory - aborting", NULL);
    return FALSE;
}

#if defined(_MSC_VER)
/* The VC++ compiler needs main/wmain defined */
# define console_ansi_main main
# if UNICODE
#  define console_wmain wmain
# endif
#endif

/* Gets the arguments with GetCommandLine, converts them to argc and argv
   and calls SDL_main */
static int
main_getcmdline()
{
    char **argv;
    int argc;
    char *cmdline = NULL;
    int retval = 0;
    int cmdalloc = 0;
    const TCHAR *text = GetCommandLine();
    const TCHAR *ptr;
    int argc_guess = 2;  /* space for NULL and initial argument. */
    int rc;

    /* make a rough guess of command line arguments. Overestimates if there
       are quoted things. */
    for (ptr = text; *ptr; ptr++) {
        if ((*ptr == ' ') || (*ptr == '\t')) {
            argc_guess++;
        }
    }

#if UNICODE
    rc = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (rc > 0) {
        cmdalloc = rc + (sizeof (char *) * argc_guess);
        argv = (char **) VirtualAlloc(NULL, cmdalloc, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        if (argv) {
            int rc2;
            cmdline = (char *) (argv + argc_guess);
            rc2 = WideCharToMultiByte(CP_UTF8, 0, text, -1, cmdline, rc, NULL, NULL);
            SDL_assert(rc2 == rc);
        }
    }
#else
    /* !!! FIXME: are these in the system codepage? We need to convert to UTF-8. */
    rc = ((int) SDL_strlen(text)) + 1;
    cmdalloc = rc + (sizeof (char *) * argc_guess);
    argv = (char **) VirtualAlloc(NULL, cmdalloc, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (argv) {
        cmdline = (char *) (argv + argc_guess);
        SDL_strcpy(cmdline, text);
    }
#endif
    if (cmdline == NULL) {
        return OutOfMemory();
    }

    /* Parse it into argv and argc */
    SDL_assert(ParseCommandLine(cmdline, NULL) <= argc_guess);
    argc = ParseCommandLine(cmdline, argv);

    SDL_SetMainReady();

    /* Run the application main() code */
    retval = SDL_main(argc, argv);

    VirtualFree(argv, cmdalloc, MEM_DECOMMIT);
    VirtualFree(argv, 0, MEM_RELEASE);

    return retval;
}

/* This is where execution begins [console apps, ansi] */
int
console_ansi_main(int argc, char *argv[])
{
    return main_getcmdline();
}


#if UNICODE
/* This is where execution begins [console apps, unicode] */
int
console_wmain(int argc, wchar_t *wargv[], wchar_t *wenvp)
{
    return main_getcmdline();
}
#endif

/* This is where execution begins [windowed apps] */
int WINAPI
WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    return main_getcmdline();
}

#endif /* __WIN32__ */

/* vi: set ts=4 sw=4 expandtab: */
