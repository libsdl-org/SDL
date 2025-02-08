# SteamOS

SteamOS is literally a Linux system, and uses the same binaries you distribute to generic Linux Steam users, so generally speaking, all the other [Linux advice](README-linux.md) applies.

If you are shipping a Linux game on Steam, or explicitly targeting SteamOS, the system is guaranteed to provide SDL. The Steam Client will set up the dynamic loader path so that a known-good copy of SDL is available to any program that needs it before launching a game. Steam provides all major versions of SDL to date, in this manner, for both x86 and amd64, in addition to several add-on libraries like `SDL_image` and `SDL_mixer`. When shipping a Linux game on Steam, do not ship a build of SDL with your game. Link against SDL as normal, and expect it to be available on the player's system. This allows Valve to make fixes and improvements to their SDL and those fixes to flow on to your game.

We are obsessive about SDL3 having a backwards-compatible ABI. Whether you build your game using the Steam Runtime SDK or just about any other copy of SDL, it _should_ work with the one that ships with Steam.

In fact, it's not a bad idea to just copy the SDL build out of the Steam Runtime if you plan to ship a Linux game for non-Steam platforms, too, since you know it's definitely well-built.

