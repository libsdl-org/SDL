#ifndef SDL_OHOSMOUSE_H
#define SDL_OHOSMOUSE_H

typedef struct SDL_OHOSMouseEvent {
    float x;
    float y;
    long long timestamp;
    int button;
    bool motion;
    bool down;
} SDL_OHOSMouseEvent;

void OHOS_OnMouse(SDL_OHOSMouseEvent event);

#endif
