# SDL3 WebGPU Support

# Native support
To support WebGPU natively (mostly for debugging purposes), you'll have to fetch your flavour of WebGPU from:

- [wgpu-native](https://github.com/gfx-rs/wgpu-native) ([prebuilt](https://github.com/gfx-rs/wgpu-native/releases/))
- [Dawn](https://dawn.googlesource.com/dawn) (Source)

Once you have your libraries compiled copy the resulting library into `./lib` and the provided or preduced headers into `./include/webgpu/`.

Once this has been done you can compile with the following instructions:

```
cmake .. -DCMAKE_BUILD_TYPE=Release -DSDL_WEBGPU=ON -DSDL_RENDER_WEBGPU=ON
cmake --build . --config Release --parallel
sudo cmake --install . --config Release
```

# Browser support
The current Emscripten version of webgpu.h is behind both wgpu-native and Dawn which conflicts with callbacks and how strings are passed to WebGPU resources.
For now I've put browser support on the backburner until I've tested using wgpu-native and Dawn.

# webgpu.h
The goal is to eventually just provide the [webgpu.h](https://github.com/webgpu-native/webgpu-headers/blob/main/webgpu.h) and then have the developer provide their library of choice, or use emcc to target the browser.
