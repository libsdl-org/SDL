This is an **experimental** WebGPU SDLGPU backend.
It has multiple known issues which I've just chosen to ignore for now:

- Uploading textures is a pain.
    - WebGPU requires a texture's bytes per row to be 256 byte aligned. You'll need to pad each row to fit within that alignment.
    - This also means that you'll have to create a much larger transfer buffer to store that data.
- RGBA32 textures are horrible to use.
    - Due to issues with WebGPU's *excellent* design choices regarding resource binds, RGBA32 textures are, by default, very tricky to use.
      In order to use an RGBA32 texture you must either:
        - Include the line "//!SDLGPU_COMPAT_F32_UNFILTERABLE" somewhere in your WGSL source code 
          (This is a hacky solution that has numerous issues, but 50% of the time it works all of the time)
        - Pass the "SDL_PROP_GPU_DEVICE_CREATE_WEBGPU_FLOAT32_FILTERABLE" property to SDL_CreateGPUDeviceWithProperties.
          This just adds the feature that's needed. Why is this not just part of WebGPU to begin with? Who knows.
- You know what, textures in general just suck.
    - Don't ever use a read-write storage texture. Just... no. (Anything that isn't R32 is unsupported by WebGPU)
    - WebGPU only supports multisampling with 1x or 4x.
- The backend currently doesn't fully follow SDLGPU conventions. (mostly due to my inability to read)
    - Pushing data to a uniform buffer instantly updates it. This breaks anything that requires uploading to a uniform buffer multiple times in one render pass.
    - You want to cancel a command buffer? No you don't.
- Multiple things just don't work.
    - No mipmap generation. (Unimplemented)
    - Stencils don't work. (Implemented, but just badly since I'm stupid)
    - I don't think download buffers work and I'm too afraid to test it since this backend is about as stable as a demon core.
    - There are a decent amount of memory leaks at initialization.
    - I'm pretty sure there's a heap overflow somewhere. Still works though.

#### How on god's green earth do I build this?
I'm very bad at CMake, and you will be punished for it.
The CMake setup for WebGPU is currently in a state of "If I pretend it doesn't exist, it can't hurt me"

```bash
cmake -S . -B build -DSDL_WGPU=ON -DSDL_WGPU_LIB="dawn or wgpu-native" -DSDL_WGPU_LIB_STATIC=ON (if using Dawn)
cmake --build build
```
You will have to provide your own compiled WebGPU library. Drop the library into the SDL root folder, and just pray it works.

#### Emscripten (Or: How to get WebGPU on the Web)

In order to build with Emscripten, you'll need to do a few things.
- You will have to define SDL_WGPU_LIB as "Dawn". (Damn you, me a month ago!) 
    - You will also have to enable SDL_WGPU_LIB_STATIC.
- You will need to pass these arguments to emcc:
    - "-sASYNCIFY=1"
    - "-sALLOW_MEMORY_GROWTH=1" (Not strictly required, but heavily encouraged since otherwise it'll probably run out of memory really quick)
    - "--use-port=emdawnwebgpu" (This tells Emscripten to provide the WebGPU library itself. Highly recommended, although you can link emdawnwebgpu yourself if you want to.)
- This is not strictly required, but it is heavily recommended you turn on SDL_WGPU_LIB_BYO. This tells SDL that you'll provide your own WebGPU library when the time comes. This makes the build process *much* easier, and is overall less prone to error.

#### Hey, it broke! What should I do?
No it didn't. Crashing your PC is a new feature in SDLGPU.

Seriously though, it breaking should be expected. If you run into an issue, please make an issue on the Github repo.

This first initial release is pretty much just me throwing a grenade into a crowd to check if it's a dud.
It's only ever been tested on my desktop, my laptop, or on the web. Again: If you find any issues, please inform me.
Otherwise, have fun! (Or don't; I'm not your dad.)

@TheeStickmahn (thestickmahn@proton.me)
