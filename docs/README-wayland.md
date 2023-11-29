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
