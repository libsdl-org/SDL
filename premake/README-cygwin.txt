There is a script in the Cygwin/build-scripts folder for generating a series of
GNU makefiles for building the SDL2 project and some parts of its test suite.
These work similarly to the MinGW makefiles, but the overall Cygwin project has
significant limitations.

The current project will not build correctly. It's experimental and has a lot of
tweaking needed to be built. It was built successfully once, but it has not been
maintained in any way.

The Cygwin project is limited in that it is not expected to be able to run
anything visual at all. It is not difficult to enable all of the visual tests
and support (such as X11 support or OpenGL), but it is not a goal for this
project. For the complexity of having a compatible desktop environment setup on
Cygwin, it's assumed that will not be the case for most users of the generated
Cygwin project. As a result, only the core tests and library are built for
Cygwin, focusing on things like thread support, file operations, and various
system queries and information gathering.

The Cygwin directory does have automated tests to run through the tests
supported by Cygwin. It also has separate build scripts for both debug and
release builds, though this is assuming the GNU make utility is located in the
user's PATH.

The Cygwin project has no outstanding dependencies, since it is designed to be
mostly minimalistic and just relied on the POSIX functionality provided by
Cygwin.

Like the other projects, you may cleanup the entire directory of any generated
or built files using the clean script located in Cygwin/build-scripts.