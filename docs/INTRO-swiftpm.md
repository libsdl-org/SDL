
# Introduction to SDL with Swift Package Manager

The easiest way to use SDL is to add it as a package dependency of your project.

We'll start by creating a simple project to build and run [hello.c](hello.c)

## From the command line on macOS

- Create a directory for the project, with a `Sources/hello` directory inside it
- Copy [hello.c](hello.c) into `Sources/hello`
- Create the file Package.swift in the project directory:

```swift
// swift-tools-version: 6.4

import PackageDescription

let package = Package(
    name: "hello",
    platforms: [
        .macOS(.v13),
    ],
    dependencies: [
        .package(url: "https://github.com/libsdl-org/SDL", branch: "main"),
    ],
    targets: [
        .executableTarget(
            name: "hello",
            dependencies: [
                .product(name: "SimpleDirectMediaLayer", package: "SDL"),
            ]
        ),
    ]
)
```

- Build and run:

```sh
swift run hello
```

Additional information and troubleshooting is available in [README-swiftpm.md](README-swiftpm.md)
