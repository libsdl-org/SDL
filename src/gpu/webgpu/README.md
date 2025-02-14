# SDL3 GPU WebGPU Backend
Status: Work In Progress

Author: Kyle Lukaszek

## Compiling with Emscripten
**Emscripten version 3.1.69+ is required to make use of SDL3 WebGPU.**
**There is an issue with Emscripten versions (>=3.1.65 & < 3.1.69) that breaks buffer copying**

To clone and build SDL WebGPU from source:
```
git clone https://github.com/klukaszek/SDL
cd SDL
mkdir build
cd build
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release -DSDL_WEBGPU=ON -DSDL_RENDER_WEBGPU=ON
cmake --build . --config Release --parallel
sudo cmake --install . --config Release
```

To clone and build examples:
```
git clone https://github.com/klukaszek/SDL3-WebGPU-Examples
cd SDL3-WebGPU-Examples
git submodule update --init
git submodule update --remote
mkdir build
cd build
emcmake cmake .. -DSDL3_DIR="full/path/to/SDL/build"
make
```

You can also provide the `-GNinja` flag to any of the `emcmake` commands to use Ninja instead of Make.

## More Info

- See checklist for a list of completed SDL3 API functions.
