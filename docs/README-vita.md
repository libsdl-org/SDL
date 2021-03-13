PS Vita
=======
SDL port for the Sony Playstation Vita ans Sony Playstation TV

Credit to
* xerpi and rsn8887 for initial (vita2d) port
* vitasdk/dolcesdk devs
* CBPS discord (Namely Graphene and SonicMastr)

Building
--------
To build for the PSP, make sure you have vitasdk and cmake installed and run:
```
   cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   cmake --install build
```


Notes
-----
* There are two renderers: native gxm and pigs-in-a-blanket gles2.  
  By default gxm one is used. gles2 renderer is slow and only usable if you want to bind SDL_Texture to GL context.  
  You can create gles2 renderer by using hint or `1` as a renderer index in `SDL_CreateRenderer`.
* By default SDL emits mouse events for touch events on every touchscreen.  
  Vita has two touchscreens, so it's recommended to use `SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");` and handle touch events instead.
* Support for L2/R2/R3/R3 buttons, haptic feedback and gamepad led only available on PSTV, or when using external ds4 gamepad on vita.