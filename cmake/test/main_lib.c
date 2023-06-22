#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED /* don't drag in header-only SDL_main implementation */
#include <SDL3/SDL_main.h>

#include EXPORT_HEADER

#ifdef _WIN32
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    return TRUE;
}
#endif

int MYLIBRARY_EXPORT mylibrary_init(void);
void MYLIBRARY_EXPORT mylibrary_quit(void);
int MYLIBRARY_EXPORT mylibrary_work(void);

int mylibrary_init(void) {
    SDL_SetMainReady();
    if (SDL_Init(0) < 0) {
        SDL_Log("Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    return 0;
}

void mylibrary_quit(void) {
    SDL_Quit();
}

int mylibrary_work(void) {
    SDL_Delay(100);
    return 0;
}
