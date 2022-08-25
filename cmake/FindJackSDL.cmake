include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(JackSDL_PKGCONFIG_MODULESPEC jack)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_JackSDL "${JackSDL_PKGCONFIG_MODULESPEC}")
endif()

find_path(JackSDL_INCLUDE_PATH
  NAMES "jack/jack.h"
  PATHS ${PKG_JackSDL_INCLUDE_DIRS}
)

find_library(JackSDL_LIBRARY
  NAMES jack
  PATHS ${PKG_JackSDL_LIBRARY_DIRS}
)

set(JackSDL_COMPILE_FLAGS "${PKG_JackSDL_CFLAGS}" CACHE STRING "Extra compile flags of Jack")

set(JackSDL_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of Jack")

set(JackSDL_LINK_FLAGS "${PKG_JackSDL_LDFLAGS_OTHER}" CACHE STRING "Extra link flags of Jack")

set(_jack_required_vars )
if(NOT JackSDL_FIND_COMPONENTS OR "LIBRARY" IN_LIST JackSDL_FIND_COMPONENTS)
  list(APPEND _jack_required_vars JackSDL_LIBRARY)
endif()
list(APPEND _jack_required_vars JackSDL_INCLUDE_PATH)

find_package_handle_standard_args(JackSDL
  REQUIRED_VARS ${_jack_required_vars}
)

if(JackSDL_FOUND)
  if(NOT TARGET JackSDL::Headers)
    add_library(JackSDL::Headers INTERFACE IMPORTED)
    set_target_properties(JackSDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${JackSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${JackSDL_COMPILE_FLAGS}"
    )
  endif()
  if(NOT TARGET JackSDL::JackSDL)
    add_library(JackSDL::JackSDL SHARED IMPORTED)
    set_target_properties(JackSDL::JackSDL
      PROPERTIES
        IMPORTED_LOCATION "${JackSDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${JackSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${JackSDL_COMPILE_FLAGS}"
        INTERFACE_LINK_LIBRARIES "${JackSDL_LINK_LIBRARIES}"
        INTERFACE_LINK_FLAGS "${JackSDL_LINK_FLAGS}"
    )
    sdl_add_soname_library(JackSDL::JackSDL)
  endif()
endif()
