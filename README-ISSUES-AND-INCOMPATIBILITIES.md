### Incompatibilities:
###### Note that this is not an exhaustive list. There are certainly things I've forgotten (or, more likely, things my brain has repressed to keep me sane)
- Read/Write textures are a joke. WebGPU only supports single channel R32Uint/Sint/Float textures for read/write.
    - I'm honestly considering just complaining to the WebGPU designers in hopes they'll fix this.
- Bindings in WebGPU were designed by the devil.
    - I'm not gonna go on another rant, but all you need to know is that the any resource bind's layout must first be defined at pipeline creation time (reasonable), 
      and that "layout" contains insane amounts of detail about the resource (unreasonable).
      The worst offender is the sampler resource. It needs to know what filtering type it's supposed to use. Sounds reasonable, right? Except for the fact that a texture in WGSL can be defined as a generic 
      "f32" type, which can be used for both filtering & unfilterable texture formats! And, if you were to use a non-filtering sampler on a filtering texture format (or vice versa), it'll break your legs for having the audacity!
      Or, you could just turn on a feature which 99.8% of all WebGPU devices support and not have to worry about this at all. Why is this not on by default? Because WebGPU hates me.
      Currently we're just using a very crude shader parser to extract the information needed, but even in concept that's not perfect. In execution it's even worse, since I'm bad at programming.
      Luckily it does seem as if shader reflection is something the WebGPU designers are interested in, so we'll maybe see that in about 85 years time?
- A texture's BPR must be 256-byte aligned (for some reason)
    - This means that 99% of the time, you'll need to change how you upload your textures.
- WebGPU doesn't expose the swapchain API. This means that things such as "maxFramesInFlight" do nothing, as it's already abstracted away and implementing our own swapchain system would just add more latency.
    - This also means that waiting for VBlank is really annoying.
- WebGPU only supports 1x or 4x multisampling. The backend will assert if debug mode is on and 2x or 8x multisampling is used.
- WebGPU doesn't have fences, and doing asynchronous operations in WebGPU through Futures is about as fun as being put in the gallows.
    - Currently the idea of a "fence" in the WebGPU backend is just a boolean which will be updated when the queue is finished. It sucks. 
