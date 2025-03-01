Linux
================================================================================

By default SDL will only link against glibc, the rest of the features will be
enabled dynamically at runtime depending on the available features on the target
system. So, for example if you built SDL with XRandR support and the target
system does not have the XRandR libraries installed, it will be disabled
at runtime, and you won't get a missing library error, at least with the
default configuration parameters.


Build Dependencies
--------------------------------------------------------------------------------

Ubuntu 18.04, all available features enabled:

    sudo apt-get install build-essential git make \
    pkg-config cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev

Ubuntu 22.04+ can also add `libpipewire-0.3-dev libwayland-dev libdecor-0-dev liburing-dev` to that command line.

Fedora 35, all available features enabled:

    sudo yum install gcc git-core make cmake \
    alsa-lib-devel pulseaudio-libs-devel nas-devel pipewire-devel \
    libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXfixes-devel \
    libXi-devel libXScrnSaver-devel dbus-devel ibus-devel \
    systemd-devel mesa-libGL-devel libxkbcommon-devel mesa-libGLES-devel \
    mesa-libEGL-devel vulkan-devel wayland-devel wayland-protocols-devel \
    libdrm-devel mesa-libgbm-devel libusb-devel libdecor-devel \
    pipewire-jack-audio-connection-kit-devel \

Fedora 39+ can also add `liburing-devel` to that command line.

NOTES:
- The sndio audio target is unavailable on Fedora (but probably not what you
  should want to use anyhow).

openSUSE Tumbleweed:

    sudo zypper in libunwind-devel libusb-1_0-devel Mesa-libGL-devel libxkbcommon-devel libdrm-devel \
    libgbm-devel pipewire-devel libpulse-devel sndio-devel Mesa-libEGL-devel

Arch:
    sudo pacman -S alsa-lib cmake hidapi ibus jack libdecor libgl libpulse libusb libx11 libxcursor libxext libxinerama libxkbcommon libxrandr libxrender libxss mesa ninja pipewire sndio vulkan-driver vulkan-headers wayland wayland-protocols


Joystick does not work
--------------------------------------------------------------------------------

If you compiled or are using a version of SDL with udev support (and you should!)
there's a few issues that may cause SDL to fail to detect your joystick. To
debug this, start by installing the evtest utility. On Ubuntu/Debian:

    sudo apt-get install evtest

Then run:

    sudo evtest

You'll hopefully see your joystick listed along with a name like "/dev/input/eventXX"
Now run:

    cat /dev/input/event/XX

If you get a permission error, you need to set a udev rule to change the mode of
your device (see below)

Also, try:

    sudo udevadm info --query=all --name=input/eventXX

If you see a line stating ID_INPUT_JOYSTICK=1, great, if you don't see it,
you need to set up an udev rule to force this variable.

A combined rule for the Saitek Pro Flight Rudder Pedals to fix both issues looks
like:

    SUBSYSTEM=="input", ATTRS{idProduct}=="0763", ATTRS{idVendor}=="06a3", MODE="0666", ENV{ID_INPUT_JOYSTICK}="1"
    SUBSYSTEM=="input", ATTRS{idProduct}=="0764", ATTRS{idVendor}=="06a3", MODE="0666", ENV{ID_INPUT_JOYSTICK}="1"

You can set up similar rules for your device by changing the values listed in
idProduct and idVendor. To obtain these values, try:

    sudo udevadm info -a --name=input/eventXX | grep idVendor
    sudo udevadm info -a --name=input/eventXX | grep idProduct

If multiple values come up for each of these, the one you want is the first one of each.

On other systems which ship with an older udev (such as CentOS), you may need
to set up a rule such as:

    SUBSYSTEM=="input", ENV{ID_CLASS}=="joystick", ENV{ID_INPUT_JOYSTICK}="1"

