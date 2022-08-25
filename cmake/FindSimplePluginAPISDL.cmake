include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

cmake_minimum_required(VERSION 3.3)

set(SimplePluginAPISDL_PKGCONFIG_MODULESPEC libspa-0.2)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PKG_SimplePluginAPISDL "${SimplePluginAPISDL_PKGCONFIG_MODULESPEC}")
endif()

find_path(SimplePluginAPISDL_INCLUDE_PATH
  NAMES "spa/support/plugin.h"
  PATHS ${PKG_SimplePluginAPISDL_INCLUDE_DIRS}
)

set(SimplePluginAPISDL_COMPILE_FLAGS "${PKG_SimplePluginAPISDL_CFLAGS}" CACHE STRING "Extra compile flags of Simple Plugin API")

set(_sapi_required_vars SimplePluginAPISDL_INCLUDE_PATH)

find_package_handle_standard_args(SimplePluginAPISDL
  REQUIRED_VARS ${_sapi_required_vars}
)

if(SimplePluginAPISDL_FOUND)
  if(NOT TARGET SimplePluginAPISDL::Headers)
    add_library(SimplePluginAPISDL::Headers INTERFACE IMPORTED)
    set_target_properties(SimplePluginAPISDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SimplePluginAPISDL_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${SimplePluginAPISDL_COMPILE_FLAGS}"
    )
  endif()
endif()
