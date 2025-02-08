/*
 * This example code looks for joystick input in the event handler, and
 * reports any changes as a flood of info.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

/* Joysticks are low-level interfaces: there's something with a bunch of
   buttons, axes and hats, in no understood order or position. This is
   a flexible interface, but you'll need to build some sort of configuration
   UI to let people tell you what button, etc, does what. On top of this
   interface, SDL offers the "gamepad" API, which works with lots of devices,
   and knows how to map arbitrary buttons and such to look like an
   Xbox/PlayStation/etc gamepad. This is easier, and better, for many games,
   but isn't necessarily a good fit for complex apps and hardware. A flight
   simulator, a realistic racing game, etc, might want this interface instead
   of gamepads. */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Color colors[64];

#define MOTION_EVENT_COOLDOWN 40

typedef struct EventMessage
{
    char *str;
    SDL_Color color;
    Uint64 start_ticks;
    struct EventMessage *next;
} EventMessage;

static EventMessage messages;
static EventMessage *messages_tail = &messages;

static const char *hat_state_string(Uint8 state)
{
    switch (state) {
        case SDL_HAT_CENTERED: return "CENTERED";
        case SDL_HAT_UP: return "UP";
        case SDL_HAT_RIGHT: return "RIGHT";
        case SDL_HAT_DOWN: return "DOWN";
        case SDL_HAT_LEFT: return "LEFT";
        case SDL_HAT_RIGHTUP: return "RIGHT+UP";
        case SDL_HAT_RIGHTDOWN: return "RIGHT+DOWN";
        case SDL_HAT_LEFTUP: return "LEFT+UP";
        case SDL_HAT_LEFTDOWN: return "LEFT+DOWN";
        default: break;
    }
    return "UNKNOWN";
}

static const char *battery_state_string(SDL_PowerState state)
{
    switch (state) {
        case SDL_POWERSTATE_ERROR: return "ERROR";
        case SDL_POWERSTATE_UNKNOWN: return "UNKNOWN";
        case SDL_POWERSTATE_ON_BATTERY: return "ON BATTERY";
        case SDL_POWERSTATE_NO_BATTERY: return "NO BATTERY";
        case SDL_POWERSTATE_CHARGING: return "CHARGING";
        case SDL_POWERSTATE_CHARGED: return "CHARGED";
        default: break;
    }
    return "UNKNOWN";
}

static void add_message(SDL_JoystickID jid, const char *fmt, ...)
{
    const SDL_Color *color = &colors[((size_t) jid) % SDL_arraysize(colors)];
    EventMessage *msg = NULL;
    char *str = NULL;
    va_list ap;

    msg = (EventMessage *) SDL_calloc(1, sizeof (*msg));
    if (!msg) {
        return;  // oh well.
    }

    va_start(ap, fmt);
    SDL_vasprintf(&str, fmt, ap);
    va_end(ap);
    if (!str) {
        SDL_free(msg);
        return;  // oh well.
    }

    msg->str = str;
    SDL_copyp(&msg->color, color);
    msg->start_ticks = SDL_GetTicks();
    msg->next = NULL;

    messages_tail->next = msg;
    messages_tail = msg;
}


/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int i;

    SDL_SetAppMetadata("Example Input Joystick Events", "1.0", "com.example.input-joystick-events");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/input/joystick-events", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    colors[0].r = colors[0].g = colors[0].b = colors[0].a = 255;
    for (i = 1; i < SDL_arraysize(colors); i++) {
        colors[i].r = SDL_rand(255);
        colors[i].g = SDL_rand(255);
        colors[i].b = SDL_rand(255);
        colors[i].a = 255;
    }

    add_message(0, "Please plug in a joystick.");

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_JOYSTICK_ADDED) {
        /* this event is sent for each hotplugged stick, but also each already-connected joystick during SDL_Init(). */
        const SDL_JoystickID which = event->jdevice.which;
        SDL_Joystick *joystick = SDL_OpenJoystick(which);
        if (!joystick) {
            add_message(which, "Joystick #%u add, but not opened: %s", (unsigned int) which, SDL_GetError());
        } else {
            add_message(which, "Joystick #%u ('%s') added", (unsigned int) which, SDL_GetJoystickName(joystick));
        }
    } else if (event->type == SDL_EVENT_JOYSTICK_REMOVED) {
        const SDL_JoystickID which = event->jdevice.which;
        SDL_Joystick *joystick = SDL_GetJoystickFromID(which);
        if (joystick) {
            SDL_CloseJoystick(joystick);  /* the joystick was unplugged. */
        }
        add_message(which, "Joystick #%u removed", (unsigned int) which);
    } else if (event->type == SDL_EVENT_JOYSTICK_AXIS_MOTION) {
        static Uint64 axis_motion_cooldown_time = 0;  /* these are spammy, only show every X milliseconds. */
        const Uint64 now = SDL_GetTicks();
        if (now >= axis_motion_cooldown_time) {
            const SDL_JoystickID which = event->jaxis.which;
            axis_motion_cooldown_time = now + MOTION_EVENT_COOLDOWN;
            add_message(which, "Joystick #%u axis %d -> %d", (unsigned int) which, (int) event->jaxis.axis, (int) event->jaxis.value);
        }
    } else if (event->type == SDL_EVENT_JOYSTICK_BALL_MOTION) {
        static Uint64 ball_motion_cooldown_time = 0;  /* these are spammy, only show every X milliseconds. */
        const Uint64 now = SDL_GetTicks();
        if (now >= ball_motion_cooldown_time) {
            const SDL_JoystickID which = event->jball.which;
            ball_motion_cooldown_time = now + MOTION_EVENT_COOLDOWN;
            add_message(which, "Joystick #%u ball %d -> %d, %d", (unsigned int) which, (int) event->jball.ball, (int) event->jball.xrel, (int) event->jball.yrel);
        }
    } else if (event->type == SDL_EVENT_JOYSTICK_HAT_MOTION) {
        const SDL_JoystickID which = event->jhat.which;
        add_message(which, "Joystick #%u hat %d -> %s", (unsigned int) which, (int) event->jhat.hat, hat_state_string(event->jhat.value));
    } else if ((event->type == SDL_EVENT_JOYSTICK_BUTTON_UP) || (event->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN)) {
        const SDL_JoystickID which = event->jbutton.which;
        add_message(which, "Joystick #%u button %d -> %s", (unsigned int) which, (int) event->jbutton.button, event->jbutton.down ? "PRESSED" : "RELEASED");
    } else if (event->type == SDL_EVENT_JOYSTICK_BATTERY_UPDATED) {
        const SDL_JoystickID which = event->jbattery.which;
        add_message(which, "Joystick #%u battery -> %s - %d%%", (unsigned int) which, battery_state_string(event->jbattery.state), event->jbattery.percent);
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 now = SDL_GetTicks();
    const float msg_lifetime = 3500.0f;  /* milliseconds a message lives for. */
    EventMessage *msg = messages.next;
    float prev_y = 0.0f;
    int winw = 640, winh = 480;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &winw, &winh);

    while (msg) {
        float x, y;
        const float life_percent = ((float) (now - msg->start_ticks)) / msg_lifetime;
        if (life_percent >= 1.0f) {  /* msg is done. */
            messages.next = msg->next;
            if (messages_tail == msg) {
                messages_tail = &messages;
            }
            SDL_free(msg->str);
            SDL_free(msg);
            msg = messages.next;
            continue;
        }
        x = (((float) winw) - (SDL_strlen(msg->str) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
        y = ((float) winh) * life_percent;
        if ((prev_y != 0.0f) && ((prev_y - y) < ((float) SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE))) {
            msg->start_ticks = now;
            break;  // wait for the previous message to tick up a little.
        }

        SDL_SetRenderDrawColor(renderer, msg->color.r, msg->color.g, msg->color.b, (Uint8) (((float) msg->color.a) * (1.0f - life_percent)));
        SDL_RenderDebugText(renderer, x, y, msg->str);

        prev_y = y;
        msg = msg->next;
    }
    
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. We let the joysticks leak. */
}
