# DJGPP platform overrides for DOS
#
# CMake's built-in Platform/DOS.cmake assumes OpenWatcom naming conventions
# (no prefix, .lib suffix, CMAKE_LINK_LIBRARY_SUFFIX=".lib").  DJGPP uses
# standard Unix/GCC conventions for its system libraries (lib prefix, .a
# suffix — e.g. libm.a).
#
# This file is loaded via CMAKE_USER_MAKE_RULES_OVERRIDE in the toolchain
# file, which runs *after* the platform module has set its defaults, giving
# us the final say on these variables.

set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_LINK_LIBRARY_SUFFIX "")
set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".lib")
set(CMAKE_EXECUTABLE_SUFFIX ".exe")