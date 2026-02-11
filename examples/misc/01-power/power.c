/*
 * This example code reports power status (plugged in, battery level, etc).
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Example Misc Power", "1.0", "com.example.misc-power");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/misc/power", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const SDL_FRect frame = { 100, 200, 440, 80 };  /* the percentage bar dimensions. */

    /* Query for battery info */
    int seconds = 0, percent = 0;
    const SDL_PowerState state = SDL_GetPowerInfo(&seconds, &percent);

    /* We set up different drawing details for each power state, then
       run it all through the same drawing code. */
    int clearr = 0, clearg = 0, clearb = 0;  /* clear window to this color. */
    int textr = 255, textg = 255, textb = 255;  /* draw messages in this color. */
    int framer = 255, frameg = 255, frameb = 255;  /* draw a percentage bar frame in this color. */
    int barr = 0, barg = 0, barb = 0;  /* draw a percentage bar in this color. */
    const char *msg = NULL;
    const char *msg2 = NULL;

    switch (state) {
        case SDL_POWERSTATE_ERROR:
            msg2 = "ERROR GETTING POWER STATE";
            msg = SDL_GetError();
            clearr = 255;  /* red background */
            break;

        default:  /* in case this does something unexpected later, treat it as unknown. */
        case SDL_POWERSTATE_UNKNOWN:
            msg = "Power state is unknown.";
            clearr = clearb = clearg = 50;  /* grey background */
            break;

        case SDL_POWERSTATE_ON_BATTERY:
            msg = "Running on battery.";
            barr = 255;  /* draw in red */
            break;

        case SDL_POWERSTATE_NO_BATTERY:
            msg = "Plugged in, no battery available.";
            clearg = 50;  /* green background */
            break;

        case SDL_POWERSTATE_CHARGING:
            msg = "Charging.";
            barb = barg = 255;  /* draw in cyan */
            break;

        case SDL_POWERSTATE_CHARGED:
            msg = "Charged.";
            barg = 255;  /* draw in green */
            break;
    }

    SDL_SetRenderDrawColor(renderer, clearr, clearg, clearb, 255);
    SDL_RenderClear(renderer);

    if (percent >= 0) {
        float x, y;
        SDL_FRect pctrect;
        char remainstr[64];
        char msgbuf[128];
        
        SDL_copyp(&pctrect, &frame);
        pctrect.w *= percent / 100.0f;

        if (seconds < 0) {
            SDL_strlcpy(remainstr, "unknown time", sizeof (remainstr));
        } else {
            int hours, minutes;
            hours = seconds / (60 * 60);
            seconds -= hours * (60 * 60);
            minutes = seconds / 60;
            seconds -= minutes * 60;
            SDL_snprintf(remainstr, sizeof (remainstr), "%02d:%02d:%02d", hours, minutes, seconds);
        }

        SDL_snprintf(msgbuf, sizeof (msgbuf), "Battery: %3d percent, %s remaining", percent, remainstr);
        x  = frame.x + ((frame.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(msgbuf))) / 2.0f);
        y = frame.y + frame.h + SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

        SDL_SetRenderDrawColor(renderer, barr, barg, barb, 255);  /* draw percent bar. */
        SDL_RenderFillRect(renderer, &pctrect);
        SDL_SetRenderDrawColor(renderer, framer, frameg, frameb, 255);  /* draw frame on top of bar. */
        SDL_RenderRect(renderer, &frame);
        SDL_SetRenderDrawColor(renderer, textr, textg, textb, 255);
        SDL_RenderDebugText(renderer, x, y, msgbuf);  /* draw text about battery level */
    }

    if (msg) {
        const float x = frame.x + ((frame.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(msg))) / 2.0f);
        const float y = frame.y - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2);
        SDL_SetRenderDrawColor(renderer, textr, textg, textb, 255);
        SDL_RenderDebugText(renderer, x, y, msg);
    }

    if (msg2) {
        const float x = frame.x + ((frame.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(msg2))) / 2.0f);
        const float y = frame.y - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 4);
        SDL_SetRenderDrawColor(renderer, textr, textg, textb, 255);
        SDL_RenderDebugText(renderer, x, y, msg2);
    }

    /* put the new rendering on the screen. */
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}

