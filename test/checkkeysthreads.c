/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

static int done;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

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
PrintKey(SDL_KeyboardEvent *event)
{
    char message[512];
    char *spot;
    size_t left;

    spot = message;
    left = sizeof(message);

    /* Print the keycode, name and state */
    if (event->key) {
        print_string(&spot, &left,
                     "Key %s:  scancode %d = %s, keycode 0x%08X = %s ",
                     event->state ? "pressed " : "released",
                     event->scancode,
                     SDL_GetScancodeName(event->scancode),
                     event->key, SDL_GetKeyName(event->key));
    } else {
        print_string(&spot, &left,
                     "Unknown Key (scancode %d = %s) %s ",
                     event->scancode,
                     SDL_GetScancodeName(event->scancode),
                     event->state ? "pressed " : "released");
    }
    print_modifiers(&spot, &left);
    if (event->repeat) {
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
    /* Check for events */
    /*SDL_WaitEvent(&event); emscripten does not like waiting*/

    SDL_Log("starting loop\n");
    while (!done && SDL_WaitEvent(&event)) {
        SDL_Log("Got event type: %" SDL_PRIu32 "\n", event.type);
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            PrintKey(&event.key);
            break;
        case SDL_EVENT_TEXT_EDITING:
            PrintText("EDIT", event.text.text);
            break;
        case SDL_EVENT_TEXT_INPUT:
            PrintText("INPUT", event.text.text);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            SDL_Window *window = SDL_GetWindowFromID(event.button.windowID);

            /* Left button quits the app, other buttons toggles text input */
            SDL_Log("mouse button down button: %d (LEFT=%d)\n", event.button.button, SDL_BUTTON_LEFT);
            if (event.button.button == SDL_BUTTON_LEFT) {
                done = 1;
            } else {
                if (SDL_TextInputActive(window)) {
                    SDL_Log("Stopping text input\n");
                    SDL_StopTextInput(window);
                } else {
                    SDL_Log("Starting text input\n");
                    SDL_StartTextInput(window);
                }
            }
            break;
        }
        case SDL_EVENT_QUIT:
            done = 1;
            break;
        default:
            break;
        }
        SDL_Log("waiting new event\n");
    }
    SDL_Log("exiting event loop\n");
#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

/* Very simple thread - counts 0 to 9 delaying 50ms between increments */
static int SDLCALL ping_thread(void *ptr)
{
    int cnt;
    SDL_Event sdlevent;
    SDL_zero(sdlevent);
    for (cnt = 0; cnt < 10; ++cnt) {
        SDL_Log("sending event (%d/%d) from thread.\n", cnt + 1, 10);
        sdlevent.type = SDL_EVENT_KEY_DOWN;
        sdlevent.key.key = SDLK_1;
        SDL_PushEvent(&sdlevent);
        SDL_Delay(1000 + SDL_rand(1000));
    }
    return cnt;
}

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Thread *thread;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Set 640x480 video mode */
    window = SDL_CreateWindow("CheckKeys Test", 640, 480, 0);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create 640x480 window: %s\n",
                     SDL_GetError());
        quit(2);
    }

    /* On wayland, no window will actually show until something has
       actually been displayed.
    */
    renderer = SDL_CreateRenderer(window, NULL);
    SDL_RenderPresent(renderer);

#ifdef SDL_PLATFORM_IOS
    /* Creating the context creates the view, which we need to show keyboard */
    SDL_GL_CreateContext(window);
#endif

    SDL_StartTextInput(window);

    /* Print initial modifier state */
    SDL_PumpEvents();
    PrintModifierState();

    /* Watch keystrokes */
    done = 0;

    thread = SDL_CreateThread(ping_thread, "PingThread", NULL);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_WaitThread(thread, NULL);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
