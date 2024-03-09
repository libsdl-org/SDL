PS Vita
=======
SDL port for the Sony Playstation Vita and Sony Playstation TV

Credit to
* xerpi, cpasjuste and rsn8887 for initial (vita2d) port
* vitasdk/dolcesdk devs
* CBPS discord (Namely Graphene and SonicMastr)

Building
--------
To build for the PSVita, make sure you have vitasdk and cmake installed and run:
```
   cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   cmake --install build
```


Notes
-----
* gles1/gles2 support and renderers are disabled by default and can be enabled by configuring with `-DVIDEO_VITA_PVR=ON`
  These renderers support 720p and 1080i resolutions. These can be specified with:
  `SDL_setenv("VITA_RESOLUTION", "720", 1);` and `SDL_setenv("VITA_RESOLUTION", "1080", 1);`
* Desktop GL 1.X and 2.X support and renderers are also disabled by default and also can be enabled with `-DVIDEO_VITA_PVR=ON` as long as gl4es4vita is present in your SDK.
  They support the same resolutions as the gles1/gles2 backends and require specifying `SDL_setenv("VITA_PVR_OGL", "1", 1);`
  anytime before video subsystem initialization.
* gles2 support via PIB is disabled by default and can be enabled by configuring with `-DVIDEO_VITA_PIB=ON`
* By default SDL emits mouse events for touch events on every touchscreen.
  Vita has two touchscreens, so it's recommended to use `SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");` and handle touch events instead.
  Individual touchscreens can be disabled with:
  `SDL_setenv("VITA_DISABLE_TOUCH_FRONT", "1", 1);` and `SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);`
* Support for L2/R2/R3/R3 buttons, haptic feedback and gamepad led only available on PSTV, or when using external ds4 gamepad on vita.
