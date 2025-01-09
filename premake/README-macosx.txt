Use the Xcode command files (located in the Xcode/build-scripts folder)
to conveniently generate a workspace for Xcode 3 or Xcode 4. It also
contains a cleaner script and a convenient script for automatically
running all the test suites.

If you use the script to automatically build the workspace file, you
need to open the workspace at least once after generating it, or it
will give errors that certain schema do not exist within the workspace.
Also, the script depends on Xcode command line tools being installed.

There are separate build files for building for i386 architecture
versus x86_64 architecture. There are separate build scripts for
Xcode 3 versus Xcode 4, but these just use the different toolchains.

There is a script for automatically running through all known supported
tests on that platform.

The Mac OS X projects currently have reliance on the following dependencies:

  -CoreVideo.framework
  -AudioToolbox.framework
  -AudioUnit.framework
  -Cocoa.framework
  -CoreAudio.framework
  -IOKit.framework
  -Carbon.framework
  -ForceFeedback.framework
  -CoreFoundation.framework

It will also link to OpenGL.framework, as the dependency function for OpenGL
assumes that OpenGL always exists on Mac OS X. However, this is defined in
a segmented way to allow the possibility of no OpenGL support on Mac OS X.

Run the clean script to clear out the directory of Xcode-related files
and binaries.