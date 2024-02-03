
# Simple DirectMedia Layer (SDL) Version 3.0

https://www.libsdl.org/

Simple DirectMedia Layer is a cross-platform development library designed
to provide low level access to audio, keyboard, mouse, joystick, and graphics
hardware via OpenGL and Direct3D. It is used by video playback software,
emulators, and popular games including Valve's award winning catalog
and many Humble Bundle games.

More extensive documentation is available in the docs directory, starting
with [README.md](docs/README.md). If you are migrating to SDL 3.0 from SDL 2.0,
the changes are extensively documented in [README-migration.md](docs/README-migration.md).

Enjoy!

Sam Lantinga (slouken@libsdl.org)

## 変更点
- イベントキューオブジェクトのポインタ型を追加
    * typedef struct SDL_EventEntry *SDL_EventQueueElement;
- イベントキューを操作する関数を追加
    * void SDL_LockEventQueue();
    * void SDL_UnlockEventQueue();
    * SDL_bool SDL_IsEventQueueActive();
    * SDL_EventQueueElement SDL_EventQueueBegin();
    * SDL_EventQueueElement SDL_EventQueueEnd();
    * int SDL_NumOfEvent();
    * SDL_EventQueueElement SDL_ForwardElement(SDL_EventQueueElement element, SDL_bool remove);
    * SDL_Event* SDL_GetEvent(SDL_EventQueueElement element);