include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(ALSASDL_PKGCONFIG_MODULESPEC alsa)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_ALSASDL "${ALSASDL_PKGCONFIG_MODULESPEC}")
endif()

find_path(ALSASDL_INCLUDE_PATH
  NAMES "alsa/asoundlib.h"
  PATHS ${PKG_ALSASDL_INCLUDE_DIRS}
)

find_library(ALSASDL_LIBRARY
  NAMES asound
  PATHS ${PKG_ALSASDL_LIBRARY_DIRS}
)

set(ALSASDL_COMPILE_FLAGS "${PKG_ALSASDL_CFLAGS}" CACHE STRING "Extra compile flags of ALSA")

set(ALSASDL_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of ALSA")

set(ALSASDL_LINK_FLAGS "${PKG_ALSASDL_LDFLAGS_OTHER}" CACHE STRING "Extra link flags of ALSA")

set(_alsa_required_vars )
if(NOT ALSASDL_FIND_COMPONENTS OR "LIBRARY" IN_LIST ALSASDL_FIND_COMPONENTS)
  list(APPEND _alsa_required_vars ALSASDL_LIBRARY)
endif()
list(APPEND _alsa_required_vars ALSASDL_INCLUDE_PATH)

find_package_handle_standard_args(ALSASDL
  REQUIRED_VARS ${_alsa_required_vars}
)

if(ALSASDL_FOUND)
  if(NOT TARGET ALSASDL::Headers)
    add_library(ALSASDL::Headers INTERFACE IMPORTED)
    set_target_properties(ALSASDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ALSASDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${ALSASDL_COMPILE_FLAGS}"
    )
  endif()
  if(NOT TARGET ALSASDL::ALSASDL)
    add_library(ALSASDL::ALSASDL SHARED IMPORTED)
    set_target_properties(ALSASDL::ALSASDL
      PROPERTIES
        IMPORTED_LOCATION "${ALSASDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ALSASDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${ALSASDL_COMPILE_FLAGS}"
        INTERFACE_LINK_LIBRARIES "${ALSASDL_LINK_LIBRARIES}"
        INTERFACE_LINK_FLAGS "${ALSASDL_LINK_FLAGS}"
    )
    sdl_add_soname_library(ALSASDL::ALSASDL)
  endif()
endif()
