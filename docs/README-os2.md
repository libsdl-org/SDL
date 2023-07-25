Simple DirectMedia Layer 2 for OS/2 & eComStation
================================================================================
SDL port for OS/2, authored by Andrey Vasilkin <digi@os2.snc.ru>, 2016


OpenGL not supported by this port.

Additional optional environment variables:

SDL_AUDIO_SHARE
  Values: 0 or 1, default is 0
  Initializes the device as shareable or exclusively acquired.

SDL_VIDEODRIVER
  Values: DIVE or VMAN, default is DIVE
  Use video subsystem: Direct interface video extensions (DIVE) or
  Video Manager (VMAN).

You may significantly increase video output speed with OS4 kernel and patched
files vman.dll and dive.dll or with latest versions of ACPI support and video
driver Panorama.

Latest versions of OS/4 kernel:
  http://gus.biysk.ru/os4/
 (Info: https://www.os2world.com/wiki/index.php/Phoenix_OS/4)

Patched files vman.dll and dive.dll:
  http://gus.biysk.ru/os4/test/pached_dll/PATCHED_DLL.RAR


Compiling:
----------

Open Watcom 1.9 or newer is tested. For the new Open Watcom V2 fork, see:
https://github.com/open-watcom/ and https://open-watcom.github.io
WATCOM environment variable must to be set to the Open Watcom install
directory. To compile, run: wmake -f Makefile.os2


Installing:
-----------

- eComStation:

  If you have previously installed SDL2, make a Backup copy of SDL2.dll
  located in D:\ecs\dll (where D: is disk on which installed eComStation).
  Stop all programs running with SDL2. Copy SDL2.dll to D:\ecs\dll

- OS/2:

  Copy SDL2.dll to any directory on your LIBPATH.  If you have a previous
  version installed, close all SDL2 applications before replacing the old
  copy.  Also make sure that any other older versions of DLLs are removed
  from your system.


Joysticks in SDL2:
------------------

The joystick code in SDL2 is a direct forward-port from the SDL-1.2 version.
Here is the original documentation from SDL-1.2:

The Joystick detection only works for standard joysticks (2 buttons, 2 axes
and the like). Therefore, if you use a non-standard joystick, you should
specify its features in the SDL_OS2_JOYSTICK environment variable in a batch
file or CONFIG.SYS, so SDL applications can provide full capability to your
device. The syntax is:

SET SDL_OS2_JOYSTICK=[JOYSTICK_NAME] [AXES] [BUTTONS] [HATS] [BALLS]

So, it you have a Gravis GamePad with 4 axes, 2 buttons, 2 hats and 0 balls,
the line should be:

SET SDL_OS2_JOYSTICK=Gravis_GamePad 4 2 2 0

If you want to add spaces in your joystick name, just surround it with
quotes or double-quotes:

SET SDL_OS2_JOYSTICK='Gravis GamePad' 4 2 2 0

or

SET SDL_OS2_JOYSTICK="Gravis GamePad" 4 2 2 0

   Note however that Balls and Hats are not supported under OS/2, and the
value will be ignored... but it is wise to define these correctly because
in the future those can be supported.

   Also the number of buttons is limited to 2 when using two joysticks,
4 when using one joystick with 4 axes, 6 when using a joystick with 3 axes
and 8 when using a joystick with 2 axes. Notice however these are limitations
of the Joystick Port hardware, not OS/2.
