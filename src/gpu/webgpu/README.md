# SDL3 GPU WebGPU Backend
Status: Work In Progress

Author: Kyle Lukaszek

# Currently Native Only
Web demo will be back eventually.

# Building
Find your flavour of [wgpu-native](https://github.com/gfx-rs/wgpu-native/releases) and download then unzip the contents into a `wgpu/` directory.

To clone and build SDL WebGPU from source:
```
git clone https://github.com/klukaszek/SDL
mv wgpu/ SDL/wgpu/ 
cd SDL
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSDL_WEBGPU=ON -DSDL_RENDER_WEBGPU=ON
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
cmake .. -DSDL3_DIR="full/path/to/SDL/build" -DWEBGPU_PATH="full/path/to/your-wgpu-install/"
make
```

You can also provide the `-GNinja` flag to any of the `emcmake` commands to use Ninja instead of Make.

## More Info

- See checklist for a list of completed SDL3 API functions.
