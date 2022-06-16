# Release checklist

## New feature release

* Update `WhatsNew.txt`

* Bump version number to 2.EVEN.0 in all these locations:

    * `configure.ac`, `CMakeLists.txt`: `SDL_*_VERSION`
    * `Xcode/SDL/Info-Framework.plist`: `CFBundleShortVersionString`,
        `CFBundleVersion`
    * `Makefile.os2`: `VERSION`
    * `Makefile.w32`: `*_VERSION`
    * `build-scripts/winrtbuild.ps1`: `$SDLVersion`
    * `include/SDL_version.h`: `SDL_*_VERSION`, `SDL_PATCHLEVEL`
    * `src/main/windows/version.rc`: `FILEVERSION`, `PRODUCTVERSION`,
        `FileVersion`, `ProductVersion`

* Bump ABI version information

    * `CMakeLists.txt`, `Xcode/SDL/SDL.xcodeproj/project.pbxproj`:
        `DYLIB_CURRENT_VERSION`, `DYLIB_COMPATIBILITY_VERSION`
        * set first number in `DYLIB_CURRENT_VERSION` to
            (100 * *minor*) + 1
        * set second number in `DYLIB_CURRENT_VERSION` to 0
        * if backwards compatibility has been broken,
            increase `DYLIB_COMPATIBILITY_VERSION` (?)

* Run test/versioning.sh to verify that everything is consistent

* Regenerate `configure`

* Do the release

## New bugfix release

* Check that no new API/ABI was added

    * If it was, do a new feature release (see above) instead

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is even)

    * Same places as listed above

* Bump ABI version information

    * `CMakeLists.txt`, `Xcode/SDL/SDL.xcodeproj/project.pbxproj`:
        `DYLIB_CURRENT_VERSION`, `DYLIB_COMPATIBILITY_VERSION`
        * set second number in `DYLIB_CURRENT_VERSION` to *patchlevel*

* Run test/versioning.sh to verify that everything is consistent

* Regenerate `configure`

* Do the release

## After a feature release

* Create a branch like `release-2.24.x`

* Bump version number to 2.ODD.0 for next development branch

    * Same places as listed above

* Bump ABI version information

    * Same places as listed above
    * Assume that the next feature release will contain new API/ABI

* Run test/versioning.sh to verify that everything is consistent

## New development prerelease

* Bump version number from 2.Y.Z to 2.Y.(Z+1) (Y is odd)

    * Same places as listed above

* Bump ABI version information

    * `CMakeLists.txt`, `Xcode/SDL/SDL.xcodeproj/project.pbxproj`:
        `DYLIB_CURRENT_VERSION`, `DYLIB_COMPATIBILITY_VERSION`
        * set first number in `DYLIB_CURRENT_VERSION` to
            (100 * *minor*) + *patchlevel* + 1
        * set second number in `DYLIB_CURRENT_VERSION` to 0
        * if backwards compatibility has been broken,
            increase `DYLIB_COMPATIBILITY_VERSION` (?)

* Run test/versioning.sh to verify that everything is consistent

* Regenerate `configure`

* Do the release
