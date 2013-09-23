
#include <SDL_system.h>
#include "SDL_winrtapp_direct3d.h"
#include "SDL_winrtapp_xaml.h"

int (*WINRT_SDLAppEntryPoint)(int, char **) = NULL;

extern "C" DECLSPEC int
SDL_WinRTRunApp(int (*mainFunction)(int, char **), void * xamlBackgroundPanel)
{
    if (xamlBackgroundPanel) {
        return SDL_WinRTInitXAMLApp(mainFunction, xamlBackgroundPanel);
    } else {
        return SDL_WinRTInitNonXAMLApp(mainFunction);
    }
}
