# Swift Package Manager (experimental!)

SDL can be built and used as a Swift package. The package builds the same sources as the CMake build, and its traits map onto the CMake options.

## Requirements

- A Swift 6.4 toolchain on macOS.

## Supported platforms

- iOS 15 or later
- Mac Catalyst 15 or later
- macOS 13 or later
- tvOS 15 or later
- visionOS 26 or later

(Note: watchOS is not supported by SDL.)


## Products

- `SimpleDirectMediaLayer`: the SDL library. SwiftPM or Xcode decides whether to link it statically or dynamically.
- `SimpleDirectMediaLayerStatic`: the SDL library, always linked statically.
- `SimpleDirectMediaLayerDynamic`: the SDL library, always built as a dynamic library. It exports the same symbols as the CMake build.
- `SDL3_test`: the static test support library, the same as the CMake `SDL3_test` library.


## Including SDL in your project

SDL's release tags (such as `release-3.4.8`) aren't semantic versions (yet!), so SwiftPM can't use them with `from:` or `exact:`. Depend on a branch or a revision instead.

In a package, add SDL to your dependencies and use one of its products in your target:

```swift
dependencies: [
    .package(url: "https://github.com/libsdl-org/SDL", branch: "main"),
],
targets: [
    .executableTarget(
        name: "MyGame",
        dependencies: [
            .product(name: "SimpleDirectMediaLayer", package: "SDL"),
        ]
    ),
]
```

Then use SDL from Swift with `import SimpleDirectMediaLayer`, or from C and Objective-C with `#include <SDL3/SDL.h>`.

In an Xcode project, add the package with File > Add Package Dependencies.

SDL uses a build tool plugin, `BuildSDLRevisionHeaderPlugin`, to generate its revision header. Xcode asks you to enable the plugin the first time you build. When building with `xcodebuild`, pass `-skipPackagePluginValidation`.

Your project's `Package.resolved` only pins SDL. The packages that SDL itself depends on are only used by its own tests.


## Configuring SDL with traits

The package uses package traits matching CMake options (and a few more). Each trait's description names the CMake option it corresponds to.

By default, the `EnableDefault…` traits are enabled. Each one turns on a subsystem with the drivers that are on by default for each platform. See the trait descriptions in `Package.swift` for the full list.

To add traits to the defaults, list `.defaults` along with the traits you want. To build only the traits you choose, leave `.defaults` out.

```swift
.package(url: "https://github.com/libsdl-org/SDL", branch: "main", traits: [.defaults, "EnableLeanAndMean"]),
```

In an Xcode project, choose the traits in the project editor's Package Dependencies tab by selecting the SDL package.

Some traits need more than SDL:

- `EnableSteamStorage` is only available on macOS and requires Steam's API library at run time.
- `EnableHIDAPILibUSB` and `EnableHIDAPILibUSBShared` need libusb. The package doesn't locate it, so its headers and library need to be on the search paths.
- `EnableTestFFmpeg` needs FFmpeg 5.1 or later, or FFmpeg 6.1 or later with `EnableVideoVulkan`. As with libusb, its headers and libraries need to be on the search paths.


## Version information

`SDL_GetRevision()` reports the same revision as the CMake build. If the source tree has a `REVISION.txt` file, the revision comes from it. Otherwise it comes from the SDL version and `git describe`.

Git is optional. Without it, the build still succeeds, and the revision ends in `GIT-NOTFOUND` or `HEAD-HASH-NOTFOUND`.


## Building and running the tests and examples

The test and example programs run on macOS only. Run them from the SDL directory with `swift run`:

```sh
swift run testsprite
```

`swift test` runs a test suite that launches a selection of the test programs. It also runs on macOS only.

When building SDL itself, `swift build --traits` replaces the default traits unless `default` is included in the list:

```sh
swift build --traits default,EnableVideoVulkan
```


## Notes for contributors

The manifest finds SDL's source files by reading its directories. SwiftPM caches the result, so after adding or removing source files, run `touch Package.swift` to make SwiftPM read the manifest again.

The mapping from traits to SDL's configuration defines is in `swift/Sources/include/build_config/SDL_build_config.h`.

The package's own sources, tests and build plugin are in the `swift` directory.

