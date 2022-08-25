include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(PulseAudioSDL_PKGCONFIG_MODULESPEC libpulse-simple)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_PULSEAUDIO "${PulseAudioSDL_PKGCONFIG_MODULESPEC}")
endif()

find_path(PulseAudioSDL_INCLUDE_PATHS
  NAMES "pulse/pulseaudio.h"
  PATHS ${PKG_PULSEAUDIO_INCLUDE_DIRS}
)

find_library(PulseAudioSDL_LIBRARY
  NAMES pulse-simple
  PATHS ${PKG_PULSEAUDIO_LIBRARY_DIRS}
)

set(PulseAudioSDL_COMPILE_FLAGS "${PKG_PULSEAUDIO_CFLAGS}" CACHE STRING "Extra compile flags of PulseAudio")

set(PulseAudioSDL_LINK_LIBRARIES "pulse" CACHE STRING "Extra link libraries of PulseAudio")

set(PulseAudioSDL_LINK_FLAGS "${PKG_PULSEAUDIO_LDFLAGS}" CACHE STRING "Extra link flags of PulseAudio")

set(_pulseaudio_required_vars )

set(_pulseaudio_library_required 0)
if(NOT PulseAudioSDL_FIND_COMPONENTS OR "LIBRARY" IN_LIST PulseAudioSDL_FIND_COMPONENTS)
  list(APPEND _pulseaudio_required_vars PulseAudioSDL_LIBRARY)
  set(_pulseaudio_library_required 1)
endif()

set(_pulseaudio_headers_required 0)
if(NOT PulseAudioSDL_FIND_COMPONENTS OR "HEADERS" IN_LIST PulseAudioSDL_FIND_COMPONENTS)
  set(_pulseaudio_headers_required 1)
endif()
list(APPEND _pulseaudio_required_vars PulseAudioSDL_INCLUDE_PATHS)

find_package_handle_standard_args(PulseAudioSDL
  REQUIRED_VARS ${_pulseaudio_required_vars}
)

if(PulseAudioSDL_FOUND)
  if(NOT TARGET PulseAudioSDL::Headers AND _pulseaudio_headers_required)
    add_library(PulseAudioSDL::Headers INTERFACE IMPORTED)
    set_target_properties(PulseAudioSDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PulseAudioSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${PulseAudioSDL_COMPILE_FLAGS}"
    )
  endif()
  if(NOT TARGET PulseAudioSDL::PulseAudioSDL AND _pulseaudio_library_required)
    add_library(PulseAudioSDL::PulseAudioSDL SHARED IMPORTED)
    set_target_properties(PulseAudioSDL::PulseAudioSDL
      PROPERTIES
        IMPORTED_LOCATION "${PulseAudioSDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PulseAudioSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${PulseAudioSDL_COMPILE_FLAGS}"
        INTERFACE_LINK_LIBRARIES "${PulseAudioSDL_LINK_LIBRARIES}"
        INTERFACE_LINK_FLAGS "${PulseAudioSDL_LINK_FLAGS}"
    )
    sdl_add_soname_library(PulseAudioSDL::PulseAudioSDL)
  endif()
endif()
