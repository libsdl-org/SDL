# --- armhf-toolchain.cmake ---
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)   # armv7hf

# Compilers
set(TARGET arm-kindlehf-linux-gnueabihf)
set(TOOLCHAIN $ENV{HOME}/x-tools/arm-kindlehf-linux-gnueabihf)
set(SYSROOT   ${TOOLCHAIN}/${TARGET}/sysroot)

set(CMAKE_C_COMPILER   ${TOOLCHAIN}/bin/${TARGET}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN}/bin/${TARGET}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN}/bin/${TARGET}-gcc)

# Sysroot + search
set(CMAKE_SYSROOT ${SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Sensible flags
set(CMAKE_C_FLAGS_INIT   "--sysroot=${SYSROOT} -O2 -idirafter /usr/include")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${SYSROOT} -O2 -idirafter /usr/include")
set(CMAKE_EXE_LINKER_FLAGS_INIT   "--sysroot=${SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${SYSROOT}")

# Help Curses find the wide library on Linux targets
set(CURSES_NEED_WIDE TRUE CACHE BOOL "")

# Make pkg-config discover target .pc files inside the sysroot
# (honored by CMake's FindPkgConfig)
if(DEFINED ENV{PKG_CONFIG})
  set(PKG_CONFIG_EXECUTABLE "$ENV{PKG_CONFIG}" CACHE FILEPATH "")
endif()
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}      "${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig")

# SDL Specific
set(SDL_X11 ON CACHE BOOL "" FORCE)
set(SDL_X11_XINPUT ON CACHE BOOL "" FORCE)
set(SDL_OPENGL ON CACHE BOOL "" FORCE)
set(SDL_OPENGL_EGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)

set(HAVE_OPENGL     TRUE)
set(HAVE_OPENGL_GLX TRUE)

set(SDL_VENDOR_INFO ON CACHE BOOL "" FORCE)

set(SDL_ALSA ON CACHE BOOL "" FORCE)
set(SDL_OSS OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)

set(SDL_LIBTHAI OFF CACHE BOOL "" FORCE)
set(SDL_LIBSAMPLERATE OFF CACHE BOOL "" FORCE)
set(SDL_TEST ON CACHE BOOL "" FORCE)