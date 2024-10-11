
SDL 3.0 has new support for high DPI displays. Interfaces provided by SDL uses the platform's native coordinates unless otherwise specified.

To reconcile platform differences in their approach to high-density scaling, SDL provides the following interfaces:
- `SDL_GetWindowSize()`          retrieves the window dimensions in native coordinates.
- `SDL_GetWindowSizeInPixels()`  retrieves the window dimensions in pixels-addressable.
- `SDL_GetDisplayContentScale()` retrieves the suggested amplification factor when drawing in native coordinates.
- `SDL_GetWindowDisplayScale()`  retrieves the suggested amplification factor when drawing in pixels-addressable.
- `SDL_GetWindowPixelDensity()`  retrieves how many addressable pixels correspond to one unit of native coordinates.
- `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`    is emitted when the value retrievable from `SDL_GetWindowSizeInPixels()` changes.
- `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` is emitted when the value retrievable from `SDL_GetWindowDisplayScale()` changes.
- Windows created with `SDL_WINDOW_HIGH_PIXEL_DENSITY` will ask the platform to display addressable pixels at their natural scale.

## Numeric example

Given a fullscreen window spanning a 3840x2160 monitor set to 2x display or 200% scaling, the following tabulates the effect of creating a window with or without `SDL_WINDOW_HIGH_PIXEL_DENSITY` on MacOS and Win32:

| Value                          | MacOS (Default) | MacOS (HD) | Win32 (Default & HD) |
|--------------------------------|-----------------|------------|----------------------|
| `SDL_GetWindowSize()`          | 1920x1080       | 1920x1080  | 3840x2160            |
| `SDL_GetWindowSizeInPixels()`  | 1920x1080       | 3840x2160  | 3840x2160            |
| `SDL_GetDisplayContentScale()` | 1.0             | 1.0        | 2.0                  |
| `SDL_GetWindowDisplayScale()`  | 1.0             | 2.0        | 2.0                  |
| `SDL_GetWindowPixelDensity()`  | 1.0             | 2.0        | 1.0                  |

Observe the philosophical difference between the approaches taken by MacOS and Win32:
- Win32 coordinate system always deals in physical device pixels, high DPI support is achieved by providing an advisory hint for the developer to enlarge drawn objects. Ignoring the advisory scale factor results in graphics appearing tiny.
- MacOS coordinate system always deals in physical content sizes, high DPI support is achieved by providing an optional flag for the developer to request finer granularity. Omitting the granularity request results in graphics appearing coarse.

## Explanation

Displays now have a content display scale, which is the expected scale for content based on the DPI settings of the display. For example, a 4K display might have a 2.0 (200%) display scale, which means that the user expects UI elements to be twice as big on this display, to aid in readability. You can query the display content scale using `SDL_GetDisplayContentScale()`, and when this changes you get an `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` event.

The window size is now distinct from the window pixel size, and the ratio between the two is the window pixel density. If the window is created with the `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag, SDL will try to match the native pixel density for the display, otherwise it will try to have the pixel size match the window size. You can query the window pixel density using `SDL_GetWindowPixelDensity()`. You can query the window pixel size using `SDL_GetWindowSizeInPixels()`, and when this changes you get an `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` event. You are guaranteed to get a `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` event when a window is created and resized, and you can use this event to create and resize your graphics context for the window.

The window has a display scale, which is the scale from the pixel resolution to the desired content size, e.g. the combination of the pixel density and the content scale. For example, a 3840x2160 window displayed at 200% on Windows, and a 1920x1080 window with the high density flag on a 2x display on macOS will both have a pixel size of 3840x2160 and a display scale of 2.0.  You can query the window display scale using `SDL_GetWindowDisplayScale()`, and when this changes you get an `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` event.
