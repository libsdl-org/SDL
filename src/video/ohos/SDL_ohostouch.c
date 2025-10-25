#include "SDL_ohostouch.h"
#include "../../events/SDL_touch_c.h"
#include "SDL_internal.h"

void OHOS_OnTouch(SDL_OHOSTouchEvent event)
{
    if (SDL_AddTouch(event.deviceId, SDL_TOUCH_DEVICE_DIRECT, "") < 0) {
        SDL_Log("Cannot add touch");
        return;
    }

    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
    {
        SDL_SendTouch(event.timestamp, event.deviceId, event.fingerId, NULL, SDL_EVENT_FINGER_DOWN, event.x, event.y, event.p);
        break;
    }
    case SDL_EVENT_FINGER_MOTION:
    {
        SDL_SendTouchMotion(event.timestamp, event.deviceId, event.fingerId, NULL, event.x, event.y, event.p);
        break;
    }
    case SDL_EVENT_FINGER_UP:
    {
        SDL_SendTouch(event.timestamp, event.deviceId, event.fingerId, NULL, SDL_EVENT_FINGER_UP, event.x, event.y, event.p);
        break;
    }
    case SDL_EVENT_FINGER_CANCELED:
    {
        SDL_SendTouch(event.timestamp, event.deviceId, event.fingerId, NULL, SDL_EVENT_FINGER_CANCELED, event.x, event.y, event.p);
        break;
    }
    }
}
