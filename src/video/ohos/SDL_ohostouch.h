#ifndef SDL_OHOSTOUCH_H
#define SDL_OHOSTOUCH_H

typedef struct SDL_OHOSTouchEvent {
    long long deviceId;
    int fingerId;
    int type;
    float x;
    float y;
    float p;
    float area;
    long long timestamp;
} SDL_OHOSTouchEvent;

void OHOS_OnTouch(SDL_OHOSTouchEvent event);

#endif
