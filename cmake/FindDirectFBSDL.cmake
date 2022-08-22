include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(DirectFBSDL_PKGCONFIG_MODULESPEC directfb>=1.0.0)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_DIRECTFB "${DirectFBSDL_PKGCONFIG_MODULESPEC}")
endif()

find_path(DirectFBSDL_INCLUDE_PATHS
  NAMES "directfb.h"
  PATHS ${PKG_DIRECTFB_INCLUDE_DIRS}
  PATH_SUFFIXES directfb
)

find_library(DirectFBSDL_LIBRARY
  NAMES directfb
  PATHS ${PKG_DIRECTFB_LIBRARY_DIRS}
)

set(DirectFBSDL_COMPILE_FLAGS "${PKG_DIRECTFB_CFLAGS}" CACHE STRING "Extra compile flags of DirectFB")

set(DirectFBSDL_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of DirectFB")

set(DirectFBSDL_LINK_FLAGS "${PKG_DIRECTFB_LDFLAGS}" CACHE STRING "Extra link flags of DirectFB")

set(_directfb_required_vars )

set(_directfb_library_required 0)
if(NOT DirectFBSDL_FIND_COMPONENTS OR "LIBRARY" IN_LIST DirectFBSDL_FIND_COMPONENTS)
  list(APPEND _directfb_required_vars DirectFBSDL_LIBRARY)
  set(_directfb_library_required 1)
endif()

set(_directfb_headers_required 0)
if(NOT DirectFBSDL_FIND_COMPONENTS OR "HEADERS" IN_LIST DirectFBSDL_FIND_COMPONENTS)
  set(_directfb_headers_required 1)
endif()
list(APPEND _directfb_required_vars DirectFBSDL_INCLUDE_PATHS)

find_package_handle_standard_args(DirectFBSDL
  REQUIRED_VARS ${_directfb_required_vars}
)

if(DirectFBSDL_FOUND)
  if(NOT TARGET DirectFBSDL::Headers AND _directfb_headers_required)
    add_library(DirectFBSDL::Headers INTERFACE IMPORTED)
    set_target_properties(DirectFBSDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DirectFBSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${DirectFBSDL_COMPILE_FLAGS}"
    )
  endif()
  if(NOT TARGET DirectFBSDL::DirectFBSDL AND _directfb_library_required)
    add_library(DirectFBSDL::DirectFBSDL SHARED IMPORTED)
    set_target_properties(DirectFBSDL::DirectFBSDL
      PROPERTIES
        IMPORTED_LOCATION "${DirectFBSDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DirectFBSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${DirectFBSDL_COMPILE_FLAGS}"
        INTERFACE_LINK_LIBRARIES "${DirectFBSDL_LINK_LIBRARIES}"
        INTERFACE_LINK_FLAGS "${DirectFBSDL_LINK_FLAGS}"
    )
    sdl_add_soname_library(DirectFBSDL::DirectFBSDL)
  endif()
endif()
