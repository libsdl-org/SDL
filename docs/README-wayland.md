Wayland
=======
Wayland is a replacement for the X11 window system protocol and architecture and is favored over X11 by default in SDL3
for communicating with desktop compositors. It works well for the majority of applications, however, applications may
encounter limitations or behavior that is different from other windowing systems.

## Common issues:

### Window decorations are missing, or the decorations look strange

- On some desktops (i.e. GNOME), Wayland applications use a library
  called [libdecor](https://gitlab.freedesktop.org/libdecor/libdecor) to provide window decorations. If this library is
  not installed, the decorations will be missing. This library uses plugins to generate different decoration styles, and
  if a plugin to generate native-looking decorations is not installed (i.e. the GTK plugin), the decorations will not
  appear to be 'native'.

### Windows do not appear immediately after creation

- Wayland requires that the application initially present a buffer before the window becomes visible. Additionally,
  applications _must_ have an event loop and processes messages on a regular basis, or the application can appear
  unresponsive to both the user and desktop compositor.

### ```SDL_SetWindowPosition()``` doesn't work on non-popup windows

- Wayland does not allow toplevel windows to position themselves programmatically.

### Retrieving the global mouse cursor position when the cursor is outside a window doesn't work

- Wayland only provides applications with the cursor position within the borders of the application windows. Querying
  the global position when an application window does not have mouse focus returns 0,0 as the actual cursor position is
  unknown. In most cases, applications don't actually need the global cursor position and should use the window-relative
  coordinates as provided by the mouse movement event or from ```SDL_GetMouseState()``` instead.

### Warping the global mouse cursor position via ```SDL_WarpMouseGlobal()``` doesn't work

- For security reasons, Wayland does not allow warping the global mouse cursor position.

### The application icon can't be set via ```SDL_SetWindowIcon()```

- Wayland doesn't support programmatically setting the application icon. To provide a custom icon for your application,
  you must create an associated desktop entry file, aka a `.desktop` file, that points to the icon image. Please see the
  [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/latest/) for more information
  on the format of this file. Note that if your application manually sets the application ID via the `SDL_APP_ID` hint
  string, the desktop entry file name should match the application ID. For example, if your application ID is set
  to `org.my_org.sdl_app`, the desktop entry file should be named `org.my_org.sdl_app.desktop`.

## Using custom Wayland windowing protocols with SDL windows

Under normal operation, an `SDL_Window` corresponds to an XDG toplevel window, which provides a standard desktop window.
If an application wishes to use a different windowing protocol with an SDL window (e.g. wlr_layer_shell) while still
having SDL handle input and rendering, it needs to create a custom, roleless surface and attach that surface to its own
toplevel window.

This is done by using `SDL_CreateWindowWithProperties()` and setting the
`SDL_PROPERTY_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN` property to `SDL_TRUE`. Once the window has been
successfully created, the `wl_display` and `wl_surface` objects can then be retrieved from the
`SDL_PROPERTY_WINDOW_WAYLAND_DISPLAY_POINTER` and `SDL_PROPERTY_WINDOW_WAYLAND_SURFACE_POINTER` properties respectively.

Surfaces don't receive any size change notifications, so if an application changes the window size, it must inform SDL
that the surface size has changed by calling SDL_SetWindowSize() with the new dimensions.

Custom surfaces will automatically handle scaling internally if the window was created with the `high-pixel-density`
property set to `SDL_TRUE`. In this case, applications should not manually attach viewports or change the surface scale
value, as SDL will handle this internally. Calls to `SDL_SetWindowSize()` should use the logical size of the window, and
`SDL_GetWindowSizeInPixels()` should be used to query the size of the backbuffer surface in pixels. If this property is
not set or is `SDL_FALSE`, applications can attach their own viewports or change the surface scale manually, and the SDL
backend will not interfere or change any values internally. In this case, calls to `SDL_SetWindowSize()` should pass the
requested surface size in pixels, not the logical window size, as no scaling calculations will be done internally.

All window functions that control window state aside from `SDL_SetWindowSize()` are no-ops with custom surfaces.

Please see the minimal example in tests/testwaylandcustom.c for an example of how to use a custom, roleless surface and
attach it to an application-managed toplevel window.
