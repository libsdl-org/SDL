Use the Xcode command files (located in the Xcode-iOS/build-scripts folder)
to conveniently generate a workspace for Xcode 3 or Xcode 4. It also
contains a cleaner script and a convenient script for automatically
running all the test suites.

The iOS project will be referencing all files related to the top-level iOS
project. The core library will use the top-level include and src directories,
just like the other generated projects, but it will build projects for each of
the Demos in the top-level Xcode-iOS folder. These projects will have any
resources they need copied to be copied over and included as resources. They
will also reference the Info.plist file in Xcode-iOS/Demos.

iOS support is currently experimental, but it should work just fine for any and
all applications. All of the demos that work from the manually-created Xcode
projects also work for the generated projects. There are a few minor things that
need improving, but nothing major.

The iOS projects have no major dependencies other than the ones in the manual
Xcode-iOS project. Those are:

  -AudioToolbox.framework
  -QuartzCore.framework
  -OpenGLES.framework
  -CoreGraphics.framework
  -UIKit.framework
  -Foundation.framework
  -CoreAudio.framework
  -CoreMotion.framework

All of these frameworks are part of the iOS SDK, not part of the core OS X
system.

Run the clean script to clear out the directory of Xcode-related files
and binaries.
