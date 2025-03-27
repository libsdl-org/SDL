#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    int i;
    char *file;
    char *folder;
    SDL_IOStream *s;
    char buf[7];

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);

        if (consumed <= 0) {
            static const char *options[] = { NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    file = SDL_CreateUnsafeTempFile();

    if (file) {
        SDL_Log("FILE: %s\n", file);
        SDL_RemovePath(file);
        SDL_free(file);
    } else {
        SDL_Log("No temp file: %s\n", SDL_GetError());
    }

    folder = SDL_CreateTempFolder();

    if (folder) {
        SDL_Log("FOLDER: %s\n", folder);
        SDL_RemovePath(folder);
        SDL_free(folder);
    } else {
        SDL_Log("No temp folder: %s\n", SDL_GetError());
    }

    s = SDL_CreateSafeTempFile();

    if (s) {
        SDL_WriteIO(s, "Hello!", 6);
        SDL_SeekIO(s, 0, SDL_IO_SEEK_SET);
        SDL_ReadIO(s, buf, 6);
        SDL_CloseIO(s);

        /* The file should be deleted by now. */

        buf[6] = '\0';
        SDL_Log("SECURE FILE: '%s'\n", buf);
    } else {
        SDL_Log("No secure temp file: %s\n", SDL_GetError());
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
