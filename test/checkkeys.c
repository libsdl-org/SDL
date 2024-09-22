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

#define TEXT_WINDOW_OFFSET_X    2.0f
#define TEXT_WINDOW_OFFSET_Y    (2.0f + FONT_LINE_HEIGHT)

#define CURSOR_BLINK_INTERVAL_MS    500

typedef struct
{
    SDLTest_TextWindow *textwindow;
    char *edit_text;
    int edit_cursor;
    int edit_length;
} TextWindowState;

static SDLTest_CommonState *state;
static TextWindowState *windowstates;
static bool escape_pressed;
static bool cursor_visible;
static Uint64 last_cursor_change;
static int done;

static TextWindowState *GetTextWindowStateForWindowID(SDL_WindowID id)
{
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        if (id == SDL_GetWindowID(state->windows[i])) {
            return &windowstates[i];
        }
    }
    return NULL;
}

static SDLTest_TextWindow *GetTextWindowForWindowID(SDL_WindowID id)
{
    TextWindowState *windowstate = GetTextWindowStateForWindowID(id);
    if (windowstate) {
        return windowstate->textwindow;
    }
    return NULL;
}

static void UpdateTextWindowInputRect(SDL_WindowID id)
{
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        if (id == SDL_GetWindowID(state->windows[i])) {
            SDLTest_TextWindow *textwindow = windowstates[i].textwindow;
            int w, h;
            SDL_Rect rect;
            int cursor = 0;
            int current = textwindow->current;
            const char *current_line = textwindow->lines[current];

            SDL_GetWindowSize(state->windows[i], &w, &h);

            if (current_line) {
                cursor = (int)SDL_utf8strlen(current_line) * FONT_CHARACTER_SIZE;
            }

            rect.x = (int)TEXT_WINDOW_OFFSET_X;
            rect.y = (int)TEXT_WINDOW_OFFSET_Y + current * FONT_LINE_HEIGHT;
            rect.w = (int)(w - (2 * TEXT_WINDOW_OFFSET_X));
            rect.h = FONT_CHARACTER_SIZE;
            SDL_SetTextInputArea(state->windows[i], &rect, cursor);
            return;
        }
    }
}

static void print_string(char **text, size_t *maxlen, const char *fmt, ...)
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

static void print_modifiers(char **text, size_t *maxlen, SDL_Keymod mod)
{
    print_string(text, maxlen, " modifiers:");
    if (mod == SDL_KMOD_NONE) {
        print_string(text, maxlen, " (none)");
        return;
    }
    if ((mod & SDL_KMOD_SHIFT) == SDL_KMOD_SHIFT) {
        print_string(text, maxlen, " SHIFT");
    } else {
        if (mod & SDL_KMOD_LSHIFT) {
            print_string(text, maxlen, " LSHIFT");
        }
        if (mod & SDL_KMOD_RSHIFT) {
            print_string(text, maxlen, " RSHIFT");
        }
    }
    if ((mod & SDL_KMOD_CTRL) == SDL_KMOD_CTRL) {
        print_string(text, maxlen, " CTRL");
    } else {
        if (mod & SDL_KMOD_LCTRL) {
            print_string(text, maxlen, " LCTRL");
        }
        if (mod & SDL_KMOD_RCTRL) {
            print_string(text, maxlen, " RCTRL");
        }
    }
    if ((mod & SDL_KMOD_ALT) == SDL_KMOD_ALT) {
        print_string(text, maxlen, " ALT");
    } else {
        if (mod & SDL_KMOD_LALT) {
            print_string(text, maxlen, " LALT");
        }
        if (mod & SDL_KMOD_RALT) {
            print_string(text, maxlen, " RALT");
        }
    }
    if ((mod & SDL_KMOD_GUI) == SDL_KMOD_GUI) {
        print_string(text, maxlen, " GUI");
    } else {
        if (mod & SDL_KMOD_LGUI) {
            print_string(text, maxlen, " LGUI");
        }
        if (mod & SDL_KMOD_RGUI) {
            print_string(text, maxlen, " RGUI");
        }
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

static void PrintModifierState(void)
{
    char message[512];
    char *spot;
    size_t left;

    spot = message;
    left = sizeof(message);

    print_modifiers(&spot, &left, SDL_GetModState());
    SDL_Log("Initial state:%s\n", message);
}

static void PrintKey(SDL_KeyboardEvent *event)
{
    char message[512];
    char *spot;
    size_t left;

    spot = message;
    left = sizeof(message);

    /* Print the keycode, name and state */
    if (event->key) {
        print_string(&spot, &left,
                     "Key %s:  raw 0x%.2x, scancode %d = %s, keycode 0x%08X = %s ",
                     event->down ? "pressed " : "released",
                     event->raw,
                     event->scancode,
                     event->scancode == SDL_SCANCODE_UNKNOWN ? "UNKNOWN" : SDL_GetScancodeName(event->scancode),
                     event->key, SDL_GetKeyName(event->key));
    } else {
        print_string(&spot, &left,
                     "Unknown Key (raw 0x%.2x, scancode %d = %s) %s ",
                     event->raw,
                     event->scancode,
                     event->scancode == SDL_SCANCODE_UNKNOWN ? "UNKNOWN" : SDL_GetScancodeName(event->scancode),
                     event->down ? "pressed " : "released");
    }
    print_modifiers(&spot, &left, event->mod);
    if (event->repeat) {
        print_string(&spot, &left, " (repeat)");
    }
    SDL_Log("%s\n", message);
}

static void PrintText(const char *eventtype, const char *text)
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

static void CountKeysDown(void)
{
    int i, count = 0, max_keys = 0;
    const bool *keystate = SDL_GetKeyboardState(&max_keys);

    for (i = 0; i < max_keys; ++i) {
        if (keystate[i]) {
            ++count;
        }
    }
    SDL_Log("Keys down: %d\n", count);
}

static void DrawCursor(int i)
{
    SDL_FRect rect;
    TextWindowState *windowstate = &windowstates[i];
    SDLTest_TextWindow *textwindow = windowstate->textwindow;
    int current = textwindow->current;
    const char *current_line = textwindow->lines[current];

    rect.x = TEXT_WINDOW_OFFSET_X;
    if (current_line) {
        rect.x += SDL_utf8strlen(current_line) * FONT_CHARACTER_SIZE;
    }
    if (windowstate->edit_cursor > 0) {
        rect.x += (float)windowstate->edit_cursor * FONT_CHARACTER_SIZE;
    }
    rect.y = TEXT_WINDOW_OFFSET_Y + current * FONT_LINE_HEIGHT;
    rect.w = FONT_CHARACTER_SIZE * 0.75f;
    rect.h = (float)FONT_CHARACTER_SIZE;

    SDL_SetRenderDrawColor(state->renderers[i], 0xAA, 0xAA, 0xAA, 255);
    SDL_RenderFillRect(state->renderers[i], &rect);
}

static void DrawEditText(int i)
{
    SDL_FRect rect;
    TextWindowState *windowstate = &windowstates[i];
    SDLTest_TextWindow *textwindow = windowstate->textwindow;
    int current = textwindow->current;
    const char *current_line = textwindow->lines[current];

    if (windowstate->edit_text == NULL) {
        return;
    }

    /* Draw the highlight under the selected text */
    if (windowstate->edit_length > 0) {
        rect.x = TEXT_WINDOW_OFFSET_X;
        if (current_line) {
            rect.x += SDL_utf8strlen(current_line) * FONT_CHARACTER_SIZE;
        }
        if (windowstate->edit_cursor > 0) {
            rect.x += (float)windowstate->edit_cursor * FONT_CHARACTER_SIZE;
        }
        rect.y = TEXT_WINDOW_OFFSET_Y + current * FONT_LINE_HEIGHT;
        rect.w = (float)windowstate->edit_length * FONT_CHARACTER_SIZE;
        rect.h = (float)FONT_CHARACTER_SIZE;

        SDL_SetRenderDrawColor(state->renderers[i], 0xAA, 0xAA, 0xAA, 255);
        SDL_RenderFillRect(state->renderers[i], &rect);
    }

    /* Draw the edit text */
    rect.x = TEXT_WINDOW_OFFSET_X;
    if (current_line) {
        rect.x += SDL_utf8strlen(current_line) * FONT_CHARACTER_SIZE;
    }
    rect.y = TEXT_WINDOW_OFFSET_Y + current * FONT_LINE_HEIGHT;
    SDL_SetRenderDrawColor(state->renderers[i], 255, 255, 0, 255);
    SDLTest_DrawString(state->renderers[i], rect.x, rect.y, windowstate->edit_text);
}

static void loop(void)
{
    SDL_Event event;
    Uint64 now;
    int i;
    char line[1024];

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            PrintKey(&event.key);
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                case SDLK_BACKSPACE:
                    SDLTest_TextWindowAddText(GetTextWindowForWindowID(event.key.windowID), "\b");
                    UpdateTextWindowInputRect(event.key.windowID);
                    break;
                case SDLK_RETURN:
                    SDLTest_TextWindowAddText(GetTextWindowForWindowID(event.key.windowID), "\n");
                    UpdateTextWindowInputRect(event.key.windowID);
                    break;
                default:
                    break;
                }
                if (event.key.key == SDLK_ESCAPE) {
                    /* Pressing escape twice will stop the application */
                    if (escape_pressed) {
                        done = 1;
                    } else {
                        escape_pressed = true;
                    }
                } else {
                    escape_pressed = true;
                }
            }
            CountKeysDown();
            break;
        case SDL_EVENT_TEXT_EDITING:
        {
            TextWindowState *windowstate = GetTextWindowStateForWindowID(event.edit.windowID);
            if (windowstate->edit_text) {
                SDL_free(windowstate->edit_text);
                windowstate->edit_text = NULL;
            }
            if (event.edit.text && *event.edit.text) {
                windowstate->edit_text = SDL_strdup(event.edit.text);
            }
            windowstate->edit_cursor = event.edit.start;
            windowstate->edit_length = event.edit.length;

            SDL_snprintf(line, sizeof(line), "EDIT %" SDL_PRIs32 ":%" SDL_PRIs32, event.edit.start, event.edit.length);
            PrintText(line, event.edit.text);
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            PrintText("INPUT", event.text.text);
            SDLTest_TextWindowAddText(GetTextWindowForWindowID(event.text.windowID), "%s", event.text.text);
            UpdateTextWindowInputRect(event.text.windowID);
            break;
        case SDL_EVENT_FINGER_DOWN:
        {
            SDL_Window *window = SDL_GetWindowFromEvent(&event);
            if (SDL_TextInputActive(window)) {
                SDL_Log("Stopping text input for window %" SDL_PRIu32 "\n", event.tfinger.windowID);
                SDL_StopTextInput(window);
            } else {
                SDL_Log("Starting text input for window %" SDL_PRIu32 "\n", event.tfinger.windowID);
                SDL_StartTextInput(window);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_RIGHT) {
                SDL_Window *window = SDL_GetWindowFromEvent(&event);
                if (SDL_TextInputActive(window)) {
                    SDL_Log("Stopping text input for window %" SDL_PRIu32 "\n", event.button.windowID);
                    SDL_StopTextInput(window);
                } else {
                    SDL_Log("Starting text input for window %" SDL_PRIu32 "\n", event.button.windowID);
                    SDL_StartTextInput(window);
                }
            }
            break;
        case SDL_EVENT_KEYMAP_CHANGED:
            SDL_Log("Keymap changed!\n");
            break;
        case SDL_EVENT_QUIT:
            done = 1;
            break;
        default:
            break;
        }
    }

    now = SDL_GetTicks();
    for (i = 0; i < state->num_windows; i++) {
        char caption[1024];

        /* Clear the window */
        SDL_SetRenderDrawColor(state->renderers[i], 0, 0, 0, 255);
        SDL_RenderClear(state->renderers[i]);

        /* Draw the text */
        SDL_SetRenderDrawColor(state->renderers[i], 255, 255, 255, 255);
        SDL_snprintf(caption, sizeof(caption), "Text input %s (click right mouse button to toggle)\n", SDL_TextInputActive(state->windows[i]) ? "enabled" : "disabled");
        SDLTest_DrawString(state->renderers[i], TEXT_WINDOW_OFFSET_X, TEXT_WINDOW_OFFSET_X, caption);
        SDLTest_TextWindowDisplay(windowstates[i].textwindow, state->renderers[i]);

        /* Draw the cursor */
        if ((now - last_cursor_change) >= CURSOR_BLINK_INTERVAL_MS) {
            cursor_visible = !cursor_visible;
            last_cursor_change = now;
        }
        if (cursor_visible) {
            DrawCursor(i);
        }

        /* Draw the composition text */
        DrawEditText(i);

        SDL_RenderPresent(state->renderers[i]);
    }

    /* Slow down framerate */
    SDL_Delay(100);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;

    SDL_SetHint(SDL_HINT_WINDOWS_RAW_KEYBOARD, "1");

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    state->window_title = "CheckKeys Test";

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

    windowstates = (TextWindowState *)SDL_calloc(state->num_windows, sizeof(*windowstates));
    if (!windowstates) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't allocate text windows: %s\n", SDL_GetError());
        goto done;
    }

    for (i = 0; i < state->num_windows; ++i) {
        int w, h;
        SDL_FRect rect;

        SDL_GetWindowSize(state->windows[i], &w, &h);
        rect.x = TEXT_WINDOW_OFFSET_X;
        rect.y = TEXT_WINDOW_OFFSET_Y;
        rect.w = w - (2 * TEXT_WINDOW_OFFSET_X);
        rect.h = h - TEXT_WINDOW_OFFSET_Y;
        windowstates[i].textwindow = SDLTest_TextWindowCreate(rect.x, rect.y, rect.w, rect.h);
    }

#ifdef SDL_PLATFORM_IOS
    {
        /* Creating the context creates the view, which we need to show keyboard */
        for (i = 0; i < state->num_windows; i++) {
            SDL_GL_CreateContext(state->windows[i]);
        }
    }
#endif

    for (i = 0; i < state->num_windows; ++i) {
        UpdateTextWindowInputRect(SDL_GetWindowID(state->windows[i]));

        SDL_StartTextInput(state->windows[i]);
    }

    /* Print initial state */
    SDL_PumpEvents();
    PrintModifierState();

    /* Watch keystrokes */
    done = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

done:
    if (windowstates) {
        for (i = 0; i < state->num_windows; ++i) {
            SDLTest_TextWindowDestroy(windowstates[i].textwindow);
        }
        SDL_free(windowstates);
    }
    SDLTest_CleanupTextDrawing();
    SDLTest_CommonQuit(state);
    return 0;
}
