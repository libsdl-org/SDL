MinGW requires both the MinGW system and MSYS.

There is a script for generating a series of GNU makefiles targeted
at MinGW on Windows. These makefiles will build the SDL library and
test executables with static links to libgcc and the same features
as the Visual Studio builds. That is, they have full OpenGL support
and they have no dependency on MinGW.

After generating the scripts, simply navigate to the directory in
a MSYS terminal and execute:

    make

If you wish to clean the directory, you can use either the clean
batch file, or call:

    make clean

The former will remove the actual makefiles and the latter will
perform a typical clean operation. You can target specific
build configurations as such:

    make config=debug

Verbosity is initially set to off. All verbosity controls is
whether the resulting gcc and ar commands are printed to the
console. You can enable verbose output by setting verbose to any
value:

    make verbose=1

There is currently no install target, but that is intended
eventually.

Ben:
There is no DirectX support currently, but you can use the
command option '--directx' when generating the makefiles to
explicitly force the DirectX dependency on. This may have
undefined behavior, so use it cautiously.