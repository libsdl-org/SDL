
# Introduction to SDL with Emscripten

First, you should have the Emscripten SDK installed from:

https://emscripten.org/docs/getting_started/downloads.html

We'll start by creating a simple project to build and run [hello.c](hello.c)

## Building SDL

Once you have a command line interface with the Emscripten SDK set up and you've changed directory to the SDL directory, you can build SDL like this:

```sh
mkdir hello
cd hello
emcmake cmake ..
emmake make
```

## Building your app

In this case we'll just run a simple command to compile our source with the SDL library we just built:
```sh
emcc -o index.html ../docs/hello.c -I../include -L. -lSDL3
```

## Running your app

You can now run your app by pointing a webserver at your build directory and connecting a web browser to it.

## More information

A more complete example is available at:

https://github.com/Ravbug/sdl3-sample

Additional information and troubleshooting is available in [README-emscripten.md](README-emscripten.md)
