include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(SndIOSDL_PKGCONFIG_MODULESPEC sndio)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_SNDIO "${SndIOSDL_PKGCONFIG_MODULESPEC}")
endif()

find_path(SndIOSDL_INCLUDE_PATHS
  NAMES "sndio.h"
  PATHS ${PKG_SNDIO_INCLUDE_DIRS}
)

find_library(SndIOSDL_LIBRARY
  NAMES sndio
  PATHS ${PKG_SNDIO_LIBRARY_DIRS}
)

set(SndIOSDL_COMPILE_FLAGS "${PKG_SNDIO_CFLAGS}" CACHE STRING "Extra compile flags of SndIO")

set(SndIOSDL_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of SndIO")

set(SndIOSDL_LINK_FLAGS "${PKG_SNDIO_LDFLAGS_OTHER}" CACHE STRING "Extra link flags of SndIO")

set(_sndio_required_vars )

set(_sndio_library_required 0)
if(NOT SndIOSDL_FIND_COMPONENTS OR "LIBRARY" IN_LIST SndIOSDL_FIND_COMPONENTS)
  list(APPEND _sndio_required_vars SndIOSDL_LIBRARY)
  set(_sndio_library_required 1)
endif()

set(_sndio_headers_required 0)
if(NOT SndIOSDL_FIND_COMPONENTS OR "HEADERS" IN_LIST SndIOSDL_FIND_COMPONENTS)
  set(_sndio_headers_required 1)
endif()
list(APPEND _sndio_required_vars SndIOSDL_INCLUDE_PATHS)

find_package_handle_standard_args(SndIOSDL
  REQUIRED_VARS ${_sndio_required_vars}
)

if(SndIOSDL_FOUND)
  if(NOT TARGET SndIOSDL::Headers AND _sndio_headers_required)
    add_library(SndIOSDL::Headers INTERFACE IMPORTED)
    set_target_properties(SndIOSDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SndIOSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${SndIOSDL_COMPILE_FLAGS}"
    )
  endif()
  if(NOT TARGET SndIOSDL::SndIOSDL AND _sndio_library_required)
    add_library(SndIOSDL::SndIOSDL SHARED IMPORTED)
    set_target_properties(SndIOSDL::SndIOSDL
      PROPERTIES
        IMPORTED_LOCATION "${SndIOSDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SndIOSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${SndIOSDL_COMPILE_FLAGS}"
        INTERFACE_LINK_LIBRARIES "${SndIOSDL_LINK_LIBRARIES}"
        INTERFACE_LINK_FLAGS "${SndIOSDL_LINK_FLAGS}"
    )
    sdl_add_soname_library(SndIOSDL::SndIOSDL)
  endif()
endif()
