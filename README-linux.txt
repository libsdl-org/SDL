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


