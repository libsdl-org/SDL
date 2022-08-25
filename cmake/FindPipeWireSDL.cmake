include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(PipeWireSDL_PKGCONFIG_MODULESPEC libpipewire-0.3>=0.3.20)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_PipeWireSDL "${PipeWireSDL_PKGCONFIG_MODULESPEC}")
endif()

find_package(SimplePluginAPISDL)

find_path(PipeWireSDL_INCLUDE_PATH
  NAMES "pipewire/extensions/metadata.h"
  PATHS ${PKG_PipeWireSDL_INCLUDE_DIRS}
)

find_library(PipeWireSDL_LIBRARY
  NAMES pipewire-0.3
  PATHS ${PKG_PipeWireSDL_LIBRARY_DIRS}
)

set(PipeWireSDL_COMPILE_FLAGS "${PKG_PipeWireSDL_CFLAGS}" CACHE STRING "Extra compile flags of PipeWire")

set(PipeWireSDL_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of PipeWire")

set(PipeWireSDL_LINK_FLAGS "${PKG_PipeWireSDL_LDFLAGS_OTHER}" CACHE STRING "Extra link flags of PipeWire")

set(_pipewire_required_vars )
if(NOT PipeWireSDL_FIND_COMPONENTS OR "LIBRARY" IN_LIST PipeWireSDL_FIND_COMPONENTS)
  list(APPEND _pipewire_required_vars PipeWireSDL_LIBRARY)
endif()
list(APPEND _pipewire_required_vars PipeWireSDL_INCLUDE_PATH SimplePluginAPISDL_FOUND)

find_package_handle_standard_args(PipeWireSDL
  REQUIRED_VARS ${_pipewire_required_vars}
)

if(PipeWireSDL_FOUND)
  if(NOT TARGET PipeWireSDL::Headers)
    add_library(PipeWireSDL::Headers INTERFACE IMPORTED)
    set_target_properties(PipeWireSDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PipeWireSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${PipeWireSDL_COMPILE_FLAGS}"
    )
  endif()
  if(NOT TARGET PipeWireSDL::PipeWireSDL)
    add_library(PipeWireSDL::PipeWireSDL SHARED IMPORTED)
    set_target_properties(PipeWireSDL::PipeWireSDL
      PROPERTIES
        IMPORTED_LOCATION "${PipeWireSDL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PipeWireSDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${PipeWireSDL_COMPILE_FLAGS}"
        INTERFACE_LINK_LIBRARIES "${PipeWireSDL_LINK_LIBRARIES}"
        INTERFACE_LINK_FLAGS "${PipeWireSDL_LINK_FLAGS}"
    )
    sdl_add_soname_library(PipeWireSDL::PipeWireSDL)
  endif()
endif()
