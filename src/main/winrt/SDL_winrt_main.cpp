
//#include "pch.h"

// The app's C-style main will be passed into SDL.dll as a function
// pointer, and called at the appropriate time.
typedef int (*SDLmain_MainFunction)(int, char **);
extern __declspec(dllimport) int SDL_WinRT_RunApplication(SDLmain_MainFunction mainFunction);
extern "C" int SDL_main(int, char **);

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
    return SDL_WinRT_RunApplication(SDL_main);
}
