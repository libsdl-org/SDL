
#include <wrl.h>

/* The app's C-style main will be passed into SDL.dll as a function
   pointer, and called at the appropriate time.
*/
extern __declspec(dllimport) int SDL_WinRT_RunApplication(int (*)(int, char **));
extern "C" int SDL_main(int, char **);

/* Prevent MSVC++ from warning about threading models when defining our
   custom WinMain.  The threading model will instead be set via a direct
   call to Windows::Foundation::Initialize (rather than via an attributed
   function).

   To note, this warning (C4447) does not seem to come up unless this file
   is compiled with C++/CX enabled (via the /ZW compiler flag).
*/
#ifdef _MSC_VER
#pragma warning(disable:4447)
#endif

/* Make sure the function to initialize the Windows Runtime gets linked in. */
#ifdef _MSC_VER
#pragma comment(lib, "runtimeobject.lib")
#endif

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    if (FAILED(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED))) {
        return 1;
    }

    SDL_WinRT_RunApplication(SDL_main);
    return 0;
}
