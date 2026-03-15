# Ubuntu Touch / Lomiri

Ubuntu Touch being similar to Ubuntu desktop, most features should be supported
out-of-the-box with SDL.

## Developing apps

Ubuntu Touch apps are developed using [Clickable](https://clickable-ut.dev/).

Clickable provides an SDL template. It is highly recommended to use the template
as a starting point for both new and existing apps.

## Considerations

Ubuntu Touch is similar to the desktop version of Ubuntu, but presents some
differences in behavior. Developers should be wary of the following:

### SDL_GetPrefPath

The only allowed writable folder is `~/.local/share/<appname>/`.
`SDL_GetPrefPath` ignores its arguments and will always return that path on
Ubuntu Touch. No changes are needed in apps.

### Video driver

Currently, [a bug](https://github.com/libsdl-org/SDL/issues/12247) forces SDL to
use the Wayland driver on Ubuntu Touch. No changes are needed in apps.

### Extra functions

SDL provides `SDL_IsUbuntuTouch()` to differentiate between Ubuntu Touch and
regular Unix, which can help if certain platform-specific tweaks are needed.
