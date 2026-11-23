
SDL 3.0 has new support for high DPI displays. Interfaces provided by SDL uses the platform's native coordinates unless otherwise specified.

## Explanation

Displays now have a content display scale, which is the expected scale for content based on the DPI settings of the display. For example, a 4K display might have a 2.0 (200%) display scale, which means that the user expects UI elements to be twice as big on this display, to aid in readability. You can query the display content scale using `SDL_GetDisplayContentScale()`, and when this changes you get an `SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED` event.

The window size is now distinct from the window pixel size, and the ratio between the two is the window pixel density. If the window is created with the `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag, SDL will try to match the native pixel density for the display, otherwise it will try to have the pixel size match the window size. You can query the window pixel density using `SDL_GetWindowPixelDensity()`. You can query the window pixel size using `SDL_GetWindowSizeInPixels()`, and when this changes you get an `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` event. You are guaranteed to get a `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` event when a window is created and resized, and you can use this event to create and resize your graphics context for the window. Do not attempt to calculate the pixel size manually by using the value returned by `SDL_GetWindowPixelDensity()` and multiplying it by the window dimensions. Doing so can result in subtle rounding errors, as SDL's video backends use platform-specific calculations to guarantee that the dimensions returned by `SDL_GetWindowSizeInPixels()` are exactly what desktop compositors require for pixel-perfect output.

The window has a display scale, which is the scale from the pixel resolution to the desired content size, e.g. the combination of the pixel density and the content scale. For example, a 3840x2160 window displayed at 200% on Windows, and a 1920x1080 window with the high density flag on a 2x display on macOS will both have a pixel size of 3840x2160 and a display scale of 2.0.  You can query the window display scale using `SDL_GetWindowDisplayScale()`, and when this changes you get an `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` event.

## Numeric example

Given a window spanning a 3840x2160 monitor set to 2x display or 200% scaling, the following tabulates the effect of creating a window with or without `SDL_WINDOW_HIGH_PIXEL_DENSITY` on macOS and Windows:

| Value                          | macOS (Default) | macOS (HD) | Windows (Default & HD) |
|--------------------------------|-----------------|------------|------------------------|
| `SDL_GetWindowSize()`          | 1920x1080       | 1920x1080  | 3840x2160              |
| `SDL_GetWindowSizeInPixels()`  | 1920x1080       | 3840x2160  | 3840x2160              |
| `SDL_GetDisplayContentScale()` | 1.0             | 1.0        | 2.0                    |
| `SDL_GetWindowDisplayScale()`  | 1.0             | 2.0        | 2.0                    |
| `SDL_GetWindowPixelDensity()`  | 1.0             | 2.0        | 1.0                    |

Observe the difference between the approaches taken by macOS and Windows:
- The Windows and Android coordinate system always deals in physical device pixels, high DPI support is achieved by providing a content scale that tells the developer to draw objects larger. Ignoring this scale factor results in graphics appearing tiny.
- The macOS and iOS coordinate system always deals in window coordinates, high DPI support is achieved by providing an optional flag for the developer to request more pixels. Omitting this flag results in graphics having low detail.
- On Linux, X11 uses a similar approach to Windows and Wayland uses a similar approach to macOS.

## Fullscreen windows

Non-exclusive fullscreen windows are simply windows that cover the entire display, and are handled the same as a non-fullscreen window.

In exclusive fullscreen mode, windows are not affected by the `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag. Instead, the pixel density of the window is tied to the pixel density of the selected display mode. The native mode for a display is retrieved by calling `SDL_GetFullscreenDisplayModes()` and choosing the first mode returned in the list. Note that this mode may not match the desktop mode, particularly in scaled environments.

Modes with a pixel density of 1.0 have a 1:1 correlation between window and pixel coordinates, and most platforms don't expose modes with a pixel density above 1.0. The exception is macOS, which may expose scaled modes with a pixel density above 1.0. When a scaled mode is chosen, applications must ensure that they call `SDL_GetWindowSizeInPixels()` to retrieve the window's drawable dimensions when rendering, even if the window wasn't created with the `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag. Applications unable to handle high pixel density rendering should not use these modes.

`SDL_GetWindowDisplayScale()` may still be used in exclusive fullscreen mode to retrieve the preferred scale factor for UI elements.

## Solution

Proper high DPI support takes into account both the content scale and the pixel density.

First, you'd create your window with the `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag, assuming you want the highest detail possible. Then you'd get the window display scale to see how much your UI elements should be enlarged to be readable.

If you're using the SDL 2D renderer, SDL provides the function `SDL_ConvertEventToRenderCoordinates()` to convert mouse coordinates between window coordinates and rendering coordinates, and the more general functions `SDL_RenderCoordinatesFromWindow()` and `SDL_RenderCoordinatesToWindow()` to do other conversion between them.

If you're not using the 2D renderer, you can implement this yourself using `SDL_GetWindowPixelDensity()` as scale factor to convert from window coordinates to pixels.

Finally you'll want to test on both Windows or X11, and macOS or Wayland, if possible, to make sure your high DPI support works in all environments.

