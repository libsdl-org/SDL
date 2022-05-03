# Release checklist

* Update `WhatsNew.txt`

* Bump version number to 2.0.EVEN for stable release

    * `configure.ac`, `CMakeLists.txt`: `SDL_*_VERSION`
    * `Xcode/SDL/Info-Framework.plist`: `CFBundleShortVersionString`,
        `CFBundleVersion`
    * `Makefile.os2`: `VERSION`
    * `build-scripts/winrtbuild.ps1`: `$SDLVersion`
    * `include/SDL_version.h`: `SDL_*_VERSION`, `SDL_PATCHLEVEL`
    * `src/main/windows/version.rc`: `FILEVERSION`, `PRODUCTVERSION`,
        `FileVersion`, `ProductVersion`

* Bump ABI version information

    * `configure.ac`: `CMakeLists.txt`: `SDL_INTERFACE_AGE`, `SDL_BINARY_AGE`
        * `SDL_INTERFACE_AGE += 1`
        * `SDL_BINARY_AGE += 1`
        * if any functions have been added, set `SDL_INTERFACE_AGE` to 0
        * if backwards compatibility has been broken,
            set both `SDL_BINARY_AGE` and `SDL_INTERFACE_AGE` to 0
    * `Xcode/SDL/SDL.xcodeproj/project.pbxproj`: `DYLIB_CURRENT_VERSION`,
        `DYLIB_COMPATIBILITY_VERSION`
        * increment second number in `DYLIB_CURRENT_VERSION`
        * if any functions have been added, increment first number in
            `DYLIB_CURRENT_VERSION` and set second number to 0
        * if backwards compatibility has been broken,
            increase `DYLIB_COMPATIBILITY_VERSION` (?)

* Regenerate `configure`

* Do the release

* Bump version number to 2.0.ODD for next development version

    * Same places as listed above

* Bump ABI version information

    * Same places as listed above
        * initially assume that there is no new ABI
