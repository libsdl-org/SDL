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

sudo apt-get install libudev-dev libasound2-dev

You also need the VideoCore binary stuff that ships in /opt/vc for EGL and 
OpenGL ES 2.x, it usually comes pre installed, but in any case:
    
sudo apt-get install libraspberrypi0 libraspberrypi-bin libraspberrypi-dev

================================================================================
 No HDMI Audio
================================================================================

If you notice that ALSA works but there's no audio over HDMI, try adding:
    
    hdmi_drive=2
    
to your config.txt file and reboot.

Reference: http://www.raspberrypi.org/phpBB3/viewtopic.php?t=5062

================================================================================
 Notes
================================================================================

* Building has only been tested natively (i.e. not cross compiled). Cross
  compilation might work though, feedback is welcome!
* No Text Input yet.