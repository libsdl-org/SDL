# Migrating to SDL 3.0

This guide provides useful information for migrating applications from SDL 2.0 to SDL 3.0.

We have provided a handy Python script to automate some of this work for you [link to script], and details on the changes are organized by SDL 2.0 header below.

## SDL_syswm.h

This header no longer includes platform specific headers and type definitions, instead allowing you to include the ones appropriate for your use case. You should define one or more of the following to enable the relevant platform-specific support:
* SDL_ENABLE_SYSWM_ANDROID
* SDL_ENABLE_SYSWM_COCOA
* SDL_ENABLE_SYSWM_KMSDRM
* SDL_ENABLE_SYSWM_UIKIT
* SDL_ENABLE_SYSWM_VIVANTE
* SDL_ENABLE_SYSWM_WAYLAND
* SDL_ENABLE_SYSWM_WINDOWS
* SDL_ENABLE_SYSWM_X11

The structures in this file are versioned separately from the rest of SDL, allowing better backwards compatibility and limited forwards compatibility with your application. Instead of calling `SDL_VERSION(&info.version)` before calling SDL_GetWindowWMInfo(), you pass the version in explicitly as `SDL_SYSWM_CURRENT_VERSION` so SDL knows what fields you expect to be filled out.

### SDL_GetWindowWMInfo

This function now returns a standard int result instead of SDL_bool, returning 0 if the function succeeds or a negative error code if there was an error. You should also pass `SDL_SYSWM_CURRENT_VERSION` as the new third version parameter. The version member of the info structure will be filled in with the version of data that is returned, the minimum of the version you requested and the version supported by the runtime SDL library.

