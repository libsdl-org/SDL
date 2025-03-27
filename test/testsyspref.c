#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    SDL_Color color;
    SDL_Event e;
    SDLTest_CommonState *state;
    int i;

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

#define LOG(val) SDL_Log(#val ": %d\n", SDL_GetSystemPreference(val))
    LOG(SDL_SYSTEM_PREFERENCE_REDUCED_MOTION);
    LOG(SDL_SYSTEM_PREFERENCE_REDUCED_TRANSPARENCY);
    LOG(SDL_SYSTEM_PREFERENCE_HIGH_CONTRAST);
    LOG(SDL_SYSTEM_PREFERENCE_COLORBLIND);
    LOG(SDL_SYSTEM_PREFERENCE_PERSIST_SCROLLBARS);
    LOG(SDL_SYSTEM_PREFERENCE_SCREEN_READER);
#undef LOG

    SDL_Log("Text scale: %f\n", SDL_GetSystemTextScale());
    SDL_Log("Cursor scale: %f\n", SDL_GetSystemCursorScale());

    if (SDL_GetSystemAccentColor(&color)) {
        SDL_Log("Accent color: %d %d %d %d\n", color.r, color.g, color.b, color.a);
    } else {
        SDL_Log("Could not get accent color: %s\n", SDL_GetError());
    }

    while (SDL_WaitEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            break;
        } else if (e.type == SDL_EVENT_SYSTEM_PREFERENCE_CHANGED) {
            switch (e.pref.pref) {
#define CHECK(val) case val: SDL_Log(#val " updated: %d\n", SDL_GetSystemPreference(val)); break;
            CHECK(SDL_SYSTEM_PREFERENCE_REDUCED_MOTION);
            CHECK(SDL_SYSTEM_PREFERENCE_REDUCED_TRANSPARENCY);
            CHECK(SDL_SYSTEM_PREFERENCE_HIGH_CONTRAST);
            CHECK(SDL_SYSTEM_PREFERENCE_COLORBLIND);
            CHECK(SDL_SYSTEM_PREFERENCE_PERSIST_SCROLLBARS);
            CHECK(SDL_SYSTEM_PREFERENCE_SCREEN_READER);
#undef CHECK
            default:
                SDL_Log("Unknown value '%d' updated: %d\n", e.pref.pref, SDL_GetSystemPreference(e.pref.pref));
            }
        } else if (e.type == SDL_EVENT_SYSTEM_TEXT_SCALE_CHANGED) {
            SDL_Log("Text scaling updated: %f\n", SDL_GetSystemTextScale());
        } else if (e.type == SDL_EVENT_SYSTEM_CURSOR_SCALE_CHANGED) {
            SDL_Log("Cursor scaling updated: %f\n", SDL_GetSystemCursorScale());
        } else if (e.type == SDL_EVENT_SYSTEM_ACCENT_COLOR_CHANGED) {
            if (SDL_GetSystemAccentColor(&color)) {
                SDL_Log("Accent color updated: %d %d %d %d\n", color.r, color.g, color.b, color.a);
            } else {
                SDL_Log("Accent color updated, could not get accent color: %s\n", SDL_GetError());
            }
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
