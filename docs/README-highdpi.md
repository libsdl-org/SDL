
SDL 3.0 has new support for high DPI displays

Displays now have a content display scale.

The display scale is the expected scale for content based on the DPI settings of the display. For example, a 4K display might have a 2.0 (200%) display scale, which means that the user expects UI elements to be twice as big on this display, to aid in readability.

The window size is now distinct from the window pixel size.

The window also has a display scale, which is the content display scale relative to the window pixel size.

For example, a 3840x2160 window displayed at 200% on Windows, and a 1920x1080 window on a 2x display on macOS will both have a pixel size of 3840x2160 and a display scale of 2.0.

You can query the window size using SDL_GetWindowSize(), and when this changes you get an SDL_EVENT_WINDOW_RESIZED event.

You can query the window pixel size using SDL_GetWindowSizeInPixels(), and when this changes you get an SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED event. You are guaranteed to get a SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED event when a window is created and resized, and you can use this event to create and resize your graphics context for the window.

You can query the window display scale using SDL_GetWindowDisplayScale(), and when this changes you get an SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED event.
