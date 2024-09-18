Wayland
=======
Wayland is a replacement for the X11 window system protocol and architecture and is favored over X11 by default in SDL3
for communicating with desktop compositors. It works well for the majority of applications, however, applications may
encounter limitations or behavior that is different from other windowing systems.

## Common issues:

### Legacy, DPI-unaware applications are blurry

- Wayland handles high-DPI displays by scaling the desktop, which causes applications that are not designed to be
  DPI-aware to be automatically scaled by the window manager, which results in them being blurry. SDL can _attempt_ to
  scale these applications such that they will be output with a 1:1 pixel aspect, however this may be buggy, especially
  with odd-sized windows and/or scale factors that aren't quarter-increments (125%, 150%, etc...). To enable this, set
  the environment variable `SDL_VIDEO_WAYLAND_SCALE_TO_DISPLAY=1`

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

### Warping the mouse cursor to or from a point outside the window doesn't work

- The cursor can be warped only within the window with mouse focus, provided that the `zwp_pointer_confinement_v1`
  protocol is supported by the compositor.

### The application icon can't be set via ```SDL_SetWindowIcon()```

- Wayland requires compositor support for the `xdg-toplevel-icon-v1` protocol to set window icons programmatically.
  Otherwise, the launcher icon from the associated desktop entry file, aka a `.desktop` file, will typically be used.
  Please see the [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/latest/) for
  more information on the format of this file. Note that if your application manually sets the application ID via the
  `SDL_APP_ID` hint string, the desktop entry file name should match the application ID. For example, if your
  application ID is set to `org.my_org.sdl_app`, the desktop entry file should be named `org.my_org.sdl_app.desktop`.

## Using custom Wayland windowing protocols with SDL windows

Under normal operation, an `SDL_Window` corresponds to an XDG toplevel window, which provides a standard desktop window.
If an application wishes to use a different windowing protocol with an SDL window (e.g. wlr_layer_shell) while still
having SDL handle input and rendering, it needs to create a custom, roleless surface and attach that surface to its own
toplevel window.

This is done by using `SDL_CreateWindowWithProperties()` and setting the
`SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN` property to `true`. Once the window has been
successfully created, the `wl_display` and `wl_surface` objects can then be retrieved from the
`SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER` and `SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER` properties respectively.

Surfaces don't receive any size change notifications, so if an application changes the window size, it must inform SDL
that the surface size has changed by calling SDL_SetWindowSize() with the new dimensions.

Custom surfaces will automatically handle scaling internally if the window was created with the
`SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN` property set to `true`. In this case, applications should
not manually attach viewports or change the surface scale value, as SDL will handle this internally. Calls
to `SDL_SetWindowSize()` should use the logical size of the window, and `SDL_GetWindowSizeInPixels()` should be used to
query the size of the backbuffer surface in pixels. If this property is not set or is `false`, applications can
attach their own viewports or change the surface scale manually, and the SDL backend will not interfere or change any
values internally. In this case, calls to `SDL_SetWindowSize()` should pass the requested surface size in pixels, not
the logical window size, as no scaling calculations will be done internally.

All window functions that control window state aside from `SDL_SetWindowSize()` are no-ops with custom surfaces.

Please see the minimal example in `tests/testwaylandcustom.c` for an example of how to use a custom, roleless surface
and attach it to an application-managed toplevel window.

## Importing external surfaces into SDL windows

Wayland windows and surfaces are more intrinsically tied to the client library than other windowing systems, therefore,
when importing surfaces, it is necessary for both SDL and the application or toolkit to use the same `wl_display`
object. This can be set/queried via the global `SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER` property. To
import an external `wl_display`, set this property before initializing the SDL video subsystem, and read the value to
export the internal `wl_display` after the video subsystem has been initialized. Setting this property after the video
subsystem has been initialized has no effect, and reading it when the video subsystem is uninitialized will either
return the user provided value, if one was set while in the uninitialized state, or NULL.

Once this is done, and the application has created or obtained the `wl_surface` to be wrapped in an `SDL_Window`, the
window is created with `SDL_CreateWindowWithProperties()` with the
`SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER` property to set to the `wl_surface` object that is to be
imported by SDL.

SDL receives no notification regarding size changes on external surfaces or toplevel windows, so if the external surface
needs to be resized, SDL must be informed by calling SDL_SetWindowSize() with the new dimensions.

If desired, SDL can automatically handle the scaling for the surface by setting the
`SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN` property to `true`, however, if the surface being imported
already has, or will have, a viewport/fractional scale manager attached to it by the application or an external toolkit,
a protocol violation will result. Avoid setting this property if importing surfaces from toolkits such as Qt or GTK.

If the window is flagged as high pixel density, calls to `SDL_SetWindowSize()` should pass the logical size of the
window and `SDL_GetWindowSizeInPixels()` should be used to retrieve the backbuffer size in pixels. Otherwise, calls to
`SDL_SetWindowSize()` should pass the requested surface size in pixels, not the logical window size, as no scaling
calculations will be done internally.

All window functions that control window state aside from `SDL_SetWindowSize()` are no-ops with external surfaces.

An example of how to use external surfaces with a `wl_display` owned by SDL can be seen in `tests/testnativewayland.c`,
and the following is a minimal example of interoperation with Qt 6, with Qt owning the `wl_display`:

```c++
#include <QApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <SDL3/SDL.h>

int main(int argc, char *argv[])
{
    int ret = -1;
    int done = 0;
    SDL_PropertiesID props;
    SDL_Event e;
    SDL_Window *sdlWindow = NULL;
    SDL_Renderer *sdlRenderer = NULL;
    struct wl_display *display = NULL;
    struct wl_surface *surface = NULL;

    /* Initialize Qt */
    QApplication qtApp(argc, argv);
    QWindow qtWindow;

    /* The windowing system must be Wayland. */
    if (QApplication::platformName() != "wayland") {
        goto exit;
    }

    {
        /* Get the wl_display object from Qt */
        QNativeInterface::QWaylandApplication *qtWlApp = qtApp.nativeInterface<QNativeInterface::QWaylandApplication>();
        display = qtWlApp->display();

        if (!display) {
            goto exit;
        }
    }

    /* Set SDL to use the existing wl_display object from Qt and initialize. */
    SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, display);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    /* Create a basic, frameless QWindow */
    qtWindow.setFlags(Qt::FramelessWindowHint);
    qtWindow.setGeometry(0, 0, 640, 480);
    qtWindow.show();

    {
        /* Get the native wl_surface backing resource for the window */
        QPlatformNativeInterface *qtNative = qtApp.platformNativeInterface();
        surface = (struct wl_surface *)qtNative->nativeResourceForWindow("surface", &qtWindow);

        if (!surface) {
            goto exit;
        }
    }

    /* Create a window that wraps the wl_surface from the QWindow.
     * Qt objects should not be flagged as DPI-aware or protocol violations will result.
     */
    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, surface);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 640);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 480);
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!sdlWindow) {
        goto exit;
    }

    /* Create a renderer */
    sdlRenderer = SDL_CreateRenderer(sdlWindow, NULL);
    if (!sdlRenderer) {
        goto exit;
    }

    /* Draw a blue screen for the window until ESC is pressed or the window is no longer visible. */
    while (!done) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                done = 1;
            }
        }

        qtApp.processEvents();

        /* Update the backbuffer size if the window scale changed. */
        qreal scale = qtWindow.devicePixelRatio();
        SDL_SetWindowSize(sdlWindow, SDL_lround(640. * scale), SDL_lround(480. * scale));

        if (qtWindow.isVisible()) {
            SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 255, 255);
            SDL_RenderClear(sdlRenderer);
            SDL_RenderPresent(sdlRenderer);
        } else {
            done = 1;
        }
    }

    ret = 0;

exit:
    /* Cleanup */
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
    }

    SDL_Quit();
    return ret;
}
```

