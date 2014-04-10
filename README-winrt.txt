================================================================================
Simple DirectMedia Layer for WinRT
================================================================================

SDL/WinRT layer allows SDL2-based applications to run on many of Microsoft's
platforms that utilize the "Windows Runtime" (aka "WinRT") APIs.  WinRT apps
are currently always full-screen apps, run in what Microsoft calls their
"Modern" environment (aka. "Metro"), and are distributed via Microsoft-run
online stores.  Some of the operating systems that support such apps include:

* Windows 8.x
* Windows RT 8.x (aka. Windows 8.x for ARM processors)
* Windows Phone 8.x

To note, WinRT applications that run on Windows 8.x and/or Windows RT are often
called "Windows Store" apps.


--------------------------------------------------------------------------------
Requirements
--------------------------------------------------------------------------------
- Microsoft Visual C++ 2012 -- Free, "Express" editions may be used, so long
  as they include support for either "Windows Store" or "Windows Phone" apps.
  (NOTE: MSVC 2013 support is pending.  2012 projects may be converted to 2013
  projects by MSVC, in the meantime.)
- A valid Microsoft account -- This requirement is not imposed by SDL, but
  rather by Microsoft's Visual C++ toolchain.


--------------------------------------------------------------------------------
TODO
--------------------------------------------------------------------------------
- Finish adding support for MSVC 2013, and "Universal" WinRT apps, which
  support Windows 8.1, Windows Phone 8.1, and in the future, Xbox One and
  Windows Desktop.
- Finish adding support for the SDL satellite libraries (SDL_image, SDL_mixer,
  SDL_ttf, etc.)
- Create templates for both MSVC 2012 and MSVC 2013, and have the corresponding
  VSIX packages either include pre-built copies of SDL, or reference binaries
  available via MSVC's NuGet servers
- Write setup instructions that use MSVC 201x templates
- Write setup instructions that don't use MSVC 201x templates, and use
  MSVC project-to-project references, rather than pre-built binaries
- Write a list of caveats found in SDL/WinRT, such as APIs that don't work due
  to platform restrictions, or things that need further work
