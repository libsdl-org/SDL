The Simple DirectMedia Layer (SDL for short) is a cross-platform 
library designed to make it easy to write multi-media software, 
such as games and emulators.

The Simple DirectMedia Layer library source code is available from: 
http://www.libsdl.org/

This library is distributed under the terms of the zlib license: 
http://zlib.net/zlib_license.html


This packages contains the SDL framework for macOS. 
Conforming with Apple guidelines, this framework 
contains both the SDL runtime component and development header files.


To Install:
Copy the SDL3.framework to /Library/Frameworks

You may alternatively install it in <Your home directory>/Library/Frameworks 
if your access privileges are not high enough.


Use in CMake projects:
SDL3.framework can be used in CMake projects using the following pattern:
```
find_package(SDL3 REQUIRED COMPONENTS SDL3)
add_executable(my_game ${MY_SOURCES})
target_link_libraries(my_game PRIVATE SDL3::SDL3)
```
If SDL3.framework is installed in a non-standard location,
please refer to the following link for ways to configure CMake:
https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure


Additional References:

 - Screencast tutorials for getting started with OpenSceneGraph/macOS are 
 	available at:
	http://www.openscenegraph.org/projects/osg/wiki/Support/Tutorials/MacOSXTips
	Though these are OpenSceneGraph centric, the same exact concepts apply to 
	SDL, thus the videos are recommended for everybody getting started with
	developing on macOS. (You can skim over the PlugIns stuff since SDL
	doesn't have any PlugIns to worry about.)
