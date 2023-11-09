/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program:  Loop, watching keystrokes
   Note that you need to call SDL_PollEvent() or SDL_WaitEvent() to
   pump the event loop and catch keystrokes.
*/

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDLTest_CommonState *state;
static SDLTest_TextWindow *textwin;
static int done;

static void
print_string(char **text, size_t *maxlen, const char *fmt, ...)
{
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = SDL_vsnprintf(*text, *maxlen, fmt, ap);
    if (len > 0) {
        *text += len;
        if (((size_t)len) < *maxlen) {
            *maxlen -= (size_t)len;
        } else {
            *maxlen = 0;
        }
    }
    va_end(ap);
}

static void
print_modifiers(char **text, size_t *maxlen)
{
    int mod;
    print_string(text, maxlen, " modifiers:");
    mod = SDL_GetModState();
    if (!mod) {
        print_string(text, maxlen, " (none)");
        return;
    }
    if (mod & SDL_KMOD_LSHIFT) {
        print_string(text, maxlen, " LSHIFT");
    }
    if (mod & SDL_KMOD_RSHIFT) {
        print_string(text, maxlen, " RSHIFT");
    }
    if (mod & SDL_KMOD_LCTRL) {
        print_string(text, maxlen, " LCTRL");
    }
    if (mod & SDL_KMOD_RCTRL) {
        print_string(text, maxlen, " RCTRL");
    }
    if (mod & SDL_KMOD_LALT) {
        print_string(text, maxlen, " LALT");
    }
    if (mod & SDL_KMOD_RALT) {
        print_string(text, maxlen, " RALT");
    }
    if (mod & SDL_KMOD_LGUI) {
        print_string(text, maxlen, " LGUI");
    }
    if (mod & SDL_KMOD_RGUI) {
        print_string(text, maxlen, " RGUI");
    }
    if (mod & SDL_KMOD_NUM) {
        print_string(text, maxlen, " NUM");
    }
    if (mod & SDL_KMOD_CAPS) {
        print_string(text, maxlen, " CAPS");
    }
    if (mod & SDL_KMOD_MODE) {
        print_string(text, maxlen, " MODE");
    }
    if (mod & SDL_KMOD_SCROLL) {
        print_string(text, maxlen, " SCROLL");
    }
}

static void
PrintModifierState(void)
{
    char message[512];
    char *spot;
    size_t left;

    spot = message;
    left = sizeof(message);

    print_modifiers(&spot, &left);
    SDL_Log("Initial state:%s\n", message);
}

static void
PrintKey(SDL_Keysym *sym, SDL_bool pressed, SDL_bool repeat)
{
    char message[512];
    char *spot;
    size_t left;

    spot = message;
    left = sizeof(message);

    /* Print the keycode, name and state */
    if (sym->sym) {
        print_string(&spot, &left,
                     "Key %s:  scancode %d = %s, keycode 0x%08X = %s ",
                     pressed ? "pressed " : "released",
                     sym->scancode,
                     SDL_GetScancodeName(sym->scancode),
                     sym->sym, SDL_GetKeyName(sym->sym));
    } else {
        print_string(&spot, &left,
                     "Unknown Key (scancode %d = %s) %s ",
                     sym->scancode,
                     SDL_GetScancodeName(sym->scancode),
                     pressed ? "pressed " : "released");
    }
    print_modifiers(&spot, &left);
    if (repeat) {
        print_string(&spot, &left, " (repeat)");
    }
    SDL_Log("%s\n", message);
}

static void
PrintText(const char *eventtype, const char *text)
{
    const char *spot;
    char expanded[1024];

    expanded[0] = '\0';
    for (spot = text; *spot; ++spot) {
        size_t length = SDL_strlen(expanded);
        (void)SDL_snprintf(expanded + length, sizeof(expanded) - length, "\\x%.2x", (unsigned char)*spot);
    }
    SDL_Log("%s Text (%s): \"%s%s\"\n", eventtype, expanded, *text == '"' ? "\\" : "", text);
}

static void loop(void)
{
    SDL_Event event;
    int i;
    /* Check for events */
    /*SDL_WaitEvent(&event); emscripten does not like waiting*/

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            PrintKey(&event.key.keysym, (event.key.state == SDL_PRESSED), (event.key.repeat > 0));
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_BACKSPACE:
                    SDLTest_TextWindowAddText(textwin, "\b");
                    break;
                case SDLK_RETURN:
                    SDLTest_TextWindowAddText(textwin, "\n");
                    break;
                default:
                    break;
                }
            }
            break;
        case SDL_EVENT_TEXT_EDITING:
            PrintText("EDIT", event.edit.text);
            break;
        case SDL_EVENT_TEXT_INPUT:
            PrintText("INPUT", event.text.text);
            SDLTest_TextWindowAddText(textwin, "%s", event.text.text);
            break;
        case SDL_EVENT_FINGER_DOWN:
            if (SDL_TextInputActive()) {
                SDL_Log("Stopping text input\n");
                SDL_StopTextInput();
            } else {
                SDL_Log("Starting text input\n");
                SDL_StartTextInput();
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            /* Left button quits the app, other buttons toggles text input */
            if (event.button.button == SDL_BUTTON_LEFT) {
                done = 1;
            } else {
                if (SDL_TextInputActive()) {
                    SDL_Log("Stopping text input\n");
                    SDL_StopTextInput();
                } else {
                    SDL_Log("Starting text input\n");
                    SDL_StartTextInput();
                }
            }
            break;
        case SDL_EVENT_QUIT:
            done = 1;
            break;
        default:
            break;
        }
    }

    for (i = 0; i < state->num_windows; i++) {
        SDL_SetRenderDrawColor(state->renderers[i], 0, 0, 0, 255);
        SDL_RenderClear(state->renderers[i]);
        SDL_SetRenderDrawColor(state->renderers[i], 255, 255, 255, 255);
        SDLTest_TextWindowDisplay(textwin, state->renderers[i]);
        SDL_RenderPresent(state->renderers[i]);
    }

    /* Slow down framerate */
    SDL_Delay(100);

#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int w, h;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    state->window_title = "CheckKeys Test";

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Disable mouse emulation */
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

    /* Initialize SDL */
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GetWindowSize(state->windows[0], &w, &h);
    textwin = SDLTest_TextWindowCreate(0.f, 0.f, (float)w, (float)h);

#ifdef __IOS__
    {
        int i;
        /* Creating the context creates the view, which we need to show keyboard */
        for (i = 0; i < state->num_windows; i++) {
            SDL_GL_CreateContext(state->windows[i]);
        }
    }
#endif

    SDL_StartTextInput();

    /* Print initial modifier state */
    SDL_PumpEvents();
    PrintModifierState();

    /* Watch keystrokes */
    done = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    SDLTest_TextWindowDestroy(textwin);
    SDLTest_CleanupTextDrawing();
    SDLTest_CommonQuit(state);
    return 0;
}
