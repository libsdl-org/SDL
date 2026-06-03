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
set(CMAKE_C_FLAGS_INIT   "--sysroot=${SYSROOT} -O2")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${SYSROOT} -O2")
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
set(SDL_VULKAN OFF) # SDL still tries to compile with vulkan even though kindles do not support Vulkan
