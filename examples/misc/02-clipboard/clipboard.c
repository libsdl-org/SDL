/*
 * This example code lets the user copy and paste with the system clipboard.
 *
 * This only handles text, but SDL supports other data types, too.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static const char *copybuttonstr = "Click here to copy!";
static const char *pastebuttonstr = "Click here to paste!";
static SDL_FRect currenttimerect;
static SDL_FRect copybuttonrect;
static SDL_FRect pastetextrect;
static SDL_FRect pastebuttonrect;
static bool copy_pressed = false;
static bool paste_pressed = false;
static char current_time[64];
static char *pasted_str = NULL;

static void CalculateCurrentTimeString(void)
{
    SDL_Time ticks = 0;
    SDL_DateTime dt;
    if (!SDL_GetCurrentTime(&ticks) || !SDL_TimeToDateTime(ticks, &dt, true)) {
        SDL_snprintf(current_time, sizeof (current_time), "(Don't know the current time, sorry.)");
    } else {
        static const char *month[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
        static const char *day[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        SDL_snprintf(current_time, sizeof (current_time), "%s, %s %d, %d   %02d:%02d:%02d", day[dt.day_of_week], month[dt.month-1], dt.day, dt.year, dt.hour, dt.minute, dt.second);
    }
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Example Misc Clipboard", "1.0", "com.example.misc-clipboard");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/misc/clipboard", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    CalculateCurrentTimeString();

    /* set up the locations where we'll draw stuff. */
    currenttimerect.x = 30;
    currenttimerect.y = 10;
    currenttimerect.w = 390;
    currenttimerect.h = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 10;

    copybuttonrect.x = currenttimerect.x + currenttimerect.w + 30;
    copybuttonrect.y = currenttimerect.y;
    copybuttonrect.w = (float) ((SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(copybuttonstr)) + 10);
    copybuttonrect.h = currenttimerect.h;

    pastetextrect.x = 10;
    pastetextrect.y = currenttimerect.y + currenttimerect.h + 10;
    pastetextrect.w = 620;
    pastetextrect.h = ((480 - pastetextrect.y) - copybuttonrect.h) - 20;

    pastebuttonrect.w = (float) ((SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(pastebuttonstr)) + 10);
    pastebuttonrect.x = (640 - pastebuttonrect.w) / 2.0f;
    pastebuttonrect.y = pastetextrect.y + pastetextrect.h + 10;
    pastebuttonrect.h = copybuttonrect.h;

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    SDL_ConvertEventToRenderCoordinates(renderer, event);
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            const SDL_FPoint p = { event->button.x, event->button.y };
            copy_pressed = SDL_PointInRectFloat(&p, &copybuttonrect);
            paste_pressed = SDL_PointInRectFloat(&p, &pastebuttonrect);
        }
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            const SDL_FPoint p = { event->button.x, event->button.y };
            if (copy_pressed && SDL_PointInRectFloat(&p, &copybuttonrect)) {
                SDL_SetClipboardText(current_time);
            } else if (paste_pressed && SDL_PointInRectFloat(&p, &pastebuttonrect)) {
                SDL_free(pasted_str);
                pasted_str = SDL_GetClipboardText();
            }
            copy_pressed = paste_pressed = false;
        }
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

static void RenderPastedText(void)
{
    char *str = pasted_str;
    if (str) {
        float x = pastetextrect.x + 5;
        float y = pastetextrect.y + 5;
        const float w = pastetextrect.w - 10;
        const float h = pastetextrect.h;
        const size_t max_chars_per_line = (size_t) (w / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
        char *newline;
        size_t slen;
        char ch;

        /* this doesn't wordwrap, or deal with Unicode....this is just a simple example app! */
        while ((newline = SDL_strchr(str, '\n')) != NULL) {
            const bool ignore_cr = ((newline > str) && (newline[-1] == '\r'));

            if (ignore_cr) {
                newline[-1] = '\0';
            }
            *newline = '\0';

            slen = SDL_strlen(str);  /* length to end of line. */
            slen = SDL_min(slen, max_chars_per_line);
            ch = str[slen];
            str[slen] = '\0';
            SDL_RenderDebugText(renderer, x, y, str);
            str[slen] = ch;

            if (ignore_cr) {
                newline[-1] = '\r';
            }
            *newline = '\n';

            str = newline + 1;
            y += (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2);
            if ((h - y) < SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) {
                break;  // no space for another line, stop here.
            }
        }

        /* last text after newline, if there's room. */
        if ((h - y) >= SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) {
            slen = SDL_strlen(str);  /* length to end of line. */
            slen = SDL_min(slen, max_chars_per_line);
            ch = str[slen];
            str[slen] = '\0';
            SDL_RenderDebugText(renderer, x, y, str);
            str[slen] = ch;
        }
    }
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    float x, y;

    CalculateCurrentTimeString();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  /* black */
    SDL_RenderClear(renderer);

    /* draw a frame around the current time. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(renderer, &currenttimerect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &currenttimerect);

    /* draw the current time inside the frame. */
    x = currenttimerect.x + ((currenttimerect.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(current_time))) / 2.0f);
    y = currenttimerect.y + 5;
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    SDL_RenderDebugText(renderer, x, y, current_time);

    /* draw a frame for the "copy the current time to the clipboard" button. */
    if (copy_pressed) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    }
    SDL_RenderFillRect(renderer, &copybuttonrect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &copybuttonrect);

    /* draw the "copy this text" button string. */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, copybuttonrect.x + 5, copybuttonrect.y + 5, copybuttonstr);

    /* draw a frame for the pasted text area. */
    SDL_SetRenderDrawColor(renderer, 0, 53, 25, 255);
    SDL_RenderFillRect(renderer, &pastetextrect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &pastetextrect);

    /* draw pasted text. */
    SDL_SetRenderDrawColor(renderer, 0, 219, 107, 255);
    RenderPastedText();

    /* draw a frame for the "paste from the clipboard" button. */
    if (paste_pressed) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    }
    SDL_RenderFillRect(renderer, &pastebuttonrect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &pastebuttonrect);

    /* draw the "paste some text" button string. */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, pastebuttonrect.x + 5, pastebuttonrect.y + 5, pastebuttonstr);

    /* put the new rendering on the screen. */
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_free(pasted_str);
    /* SDL will clean up the window/renderer for us. */
}

