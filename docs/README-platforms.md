# Platforms

## Supported Platforms

- [Android](README-android.md)
- [Emscripten](README-emscripten.md)
- [FreeBSD](README-bsd.md)
- [Haiku OS](README-haiku.md)
- [iOS](README-ios.md)
- [Linux](README-linux.md)
- [macOS](README-macos.md)
- [NetBSD](README-bsd.md)
- [Nintendo Switch](README-switch.md)
- [Nintendo 3DS](README-n3ds.md)
- [OpenBSD](README-bsd.md)
- [PlayStation 2](README-ps2.md)
- [PlayStation 4](README-ps4.md)
- [PlayStation 5](README-ps5.md)
- [PlayStation Portable](README-psp.md)
- [PlayStation Vita](README-vita.md)
- [RISC OS](README-riscos.md)
- [SteamOS](README-steamos.md)
- [tvOS](README-ios.md)
- [Windows](README-windows.md)
- [Windows GDK](README-gdk.md)
- [Xbox](README-gdk.md)

## Unsupported Platforms

If your favorite system is listed below, we aren't working on it. However, if you send reasonable patches and are willing to support the port in the long term, we are happy to take a look!

All of these still work with [SDL2](/SDL2), which is an incompatible API, but an option if you need to support these platforms still.

- Google Stadia
- NaCL
- Nokia N-Gage
- OS/2
- QNX
- WinPhone
- WinRT/UWP

## General notes for Unix platforms

Some aspects of SDL functionality are common to all Unix-based platforms.

### <a name=setuid></a>Privileged processes (setuid, setgid, setcap)

SDL is not designed to be used in programs with elevated privileges,
such as setuid (`chmod u+s`) or setgid (`chmod g+s`) executables,
or executables with file-based capabilities
(`setcap cap_sys_nice+ep` or similar).
It does not make any attempt to avoid trusting environment variables
or other aspects of the inherited execution environment.
Programs running with elevated privileges in an attacker-controlled
execution environment should not call SDL functions.
