# Release checklist

## New feature release

* Update `WhatsNew.txt`

* Bump version number to 2.EVEN.0 in all these locations:

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
        * `SDL_BINARY_AGE += 1`
        * set `SDL_INTERFACE_AGE` to 0
        * if backwards compatibility has been broken,
            set both `SDL_BINARY_AGE` and `SDL_INTERFACE_AGE` to 0
    * `Xcode/SDL/SDL.xcodeproj/project.pbxproj`: `DYLIB_CURRENT_VERSION`,
        `DYLIB_COMPATIBILITY_VERSION`
        * increment first number in `DYLIB_CURRENT_VERSION`
        * set second number in `DYLIB_CURRENT_VERSION` to 0
        * if backwards compatibility has been broken,
            increase `DYLIB_COMPATIBILITY_VERSION` (?)

* Regenerate `configure`

* Do the release

## New bugfix release

* Check that no new API/ABI was added

    * If it was, do a new feature release (see above) instead

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is even)

    * Same places as listed above

* Bump ABI version information

    * `configure.ac`: `CMakeLists.txt`: `SDL_INTERFACE_AGE`, `SDL_BINARY_AGE`
        * `SDL_INTERFACE_AGE += 1`
        * `SDL_BINARY_AGE += 1`
    * `Xcode/SDL/SDL.xcodeproj/project.pbxproj`: `DYLIB_CURRENT_VERSION`,
        `DYLIB_COMPATIBILITY_VERSION`
        * increment second number in `DYLIB_CURRENT_VERSION`

* Regenerate `configure`

* Do the release

## After a feature release

* Create a branch like `release-2.24.x`

* Bump version number to 2.ODD.0 for next development branch

    * Same places as listed above

* Bump ABI version information

    * Same places as listed above
    * Assume that the next feature release will contain new API/ABI

## New development prerelease

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is odd)

    * Same places as listed above

* Bump ABI version information

    * Same places as listed above
    * Assume that the next feature release will contain new API/ABI

* Regenerate `configure`

* Do the release
