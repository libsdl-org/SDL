================================================================================
Simple DirectMedia Layer for Linux
================================================================================

By default SDL will only link against glibc, the rest of the features will be
enabled dynamically at runtime depending on the available features on the target
system. So, for example if you built SDL with Xinerama support and the target
system does not have the Xinerama libraries installed, it will be disabled
at runtime, and you won't get a missing library error, at least with the 
default configuration parameters.


================================================================================
Build Dependencies
================================================================================
    
Ubuntu 13.04, all available features enabled:

sudo apt-get install build-essential mercurial make cmake autoconf automake \
libtool libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev \
libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev \
libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev \
libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev

NOTES:
- This includes all the audio targets except arts, because Ubuntu pulled the 
  artsc0-dev package, but in theory SDL still supports it.
- DirectFB isn't included because the configure script (currently) fails to find
  it at all. You can do "sudo apt-get install libdirectfb-dev" and fix the 
  configure script to include DirectFB support. Send patches.  :)


