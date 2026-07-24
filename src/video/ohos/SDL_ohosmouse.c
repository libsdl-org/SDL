#include "SDL_internal.h"
#include "events/SDL_mouse_c.h"
#include "SDL_ohosmouse.h"

void OHOS_OnMouse(SDL_OHOSMouseEvent event)
{
    SDL_SendMouseMotion(event.timestamp, NULL, SDL_DEFAULT_MOUSE_ID, false, event.x, event.y);
    if (!event.motion) {
        SDL_SendMouseButton(event.timestamp, NULL, SDL_DEFAULT_MOUSE_ID, event.button, event.down);
    }
}