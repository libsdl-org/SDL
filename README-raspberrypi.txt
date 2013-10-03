================================================================================
SDL2 for Raspberry Pi
================================================================================

Requirements:

Raspbian (other Linux distros may work as well).

================================================================================
 Features
================================================================================

* Works without X11
* Hardware accelerated OpenGL ES 2.x
* Sound via ALSA
* Input (mouse/keyboard/joystick) via EVDEV
* Hotplugging of input devices via UDEV

================================================================================
 Raspbian Build Dependencies
================================================================================

sudo apt-get install libudev-dev libasound2-dev libdbus-1-dev

You also need the VideoCore binary stuff that ships in /opt/vc for EGL and 
OpenGL ES 2.x, it usually comes pre installed, but in any case:
    
sudo apt-get install libraspberrypi0 libraspberrypi-bin libraspberrypi-dev

================================================================================
 No input
================================================================================

Make sure you belong to the "input" group.

    sudo usermod -aG input `whoami`

================================================================================
 No HDMI Audio
================================================================================

If you notice that ALSA works but there's no audio over HDMI, try adding:
    
    hdmi_drive=2
    
to your config.txt file and reboot.

Reference: http://www.raspberrypi.org/phpBB3/viewtopic.php?t=5062

================================================================================
 Text Input API support
================================================================================

The Text Input API is supported, with translation of scan codes done via the
kernel symbol tables. For this to work, SDL needs access to a valid console.
If you notice there's no SDL_TEXTINPUT message being emmited, double check that
your app has read access to one of the following:
    
* /proc/self/fd/0
* /dev/tty
* /dev/tty[0...6]
* /dev/vc/0
* /dev/console

This is usually not a problem if you run from the physical terminal (as opposed
to running from a pseudo terminal, such as via SSH). If running from a PTS, a 
quick workaround is to run your app as root or add yourself to the tty group,
then re login to the system.

   sudo usermod -aG tty `whoami`
    
The keyboard layout used by SDL is the same as the one the kernel uses.
To configure the layout on Raspbian:
    
    sudo dpkg-reconfigure keyboard-configuration
    
To configure the locale, which controls which keys are interpreted as letters,
this determining the CAPS LOCK behavior:

    sudo dpkg-reconfigure locales

================================================================================
 Notes
================================================================================

* Building has only been tested natively (i.e. not cross compiled). Cross
  compilation might work though, feedback is welcome!
