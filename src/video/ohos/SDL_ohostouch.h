#ifndef SDL_OHOSTOUCH_H
#define SDL_OHOSTOUCH_H

typedef struct SDL_OHOSTouchEvent {
    int64_t deviceId;
    int32_t fingerId;
    int type;
    float x;
    float y;
    float p;
    float area;
    int64_t timestamp;
} SDL_OHOSTouchEvent;

void OHOS_OnTouch(SDL_OHOSTouchEvent event);

#endif
