include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)

cmake_minimum_required(VERSION 3.3)

set(OpenSoundSystemSDL_INCLUDE_PATH )

find_path(OpenSoundSystemSDL_SYS_SOUNDCARD_H_INCLUDE_PATH
  NAMES "sys/soundcard.h"
)
if(OpenSoundSystemSDL_SYS_SOUNDCARD_H_INCLUDE_PATH)
  set(OpenSoundSystemSDL_INCLUDE_PATH ${OpenSoundSystemSDL_SYS_SOUNDCARD_H_INCLUDE_PATH})
else()
  find_path(OpenSoundSystemSDL_SOUNDCARD_H_INCLUDE_PATH
    NAMES "soundcard.h"
  )
  if(OpenSoundSystemSDL_SOUNDCARD_H_INCLUDE_PATH)
    set(OpenSoundSystemSDL_INCLUDE_PATH ${OpenSoundSystemSDL_SOUNDCARD_H_INCLUDE_PATH})
  endif()
endif()

set(_opensoundsystem_needs_library 0)
if(_opensoundsystem_needs_library)
  set(_opensoundsystem_needs_library 1)
  find_library(OpenSoundSystemSDL_LIBRARY
    NAMES ossaudio
  )
endif()

set(_opensoundsystem_required_vars )
if(_opensoundsystem_needs_library)
  list(APPEND _opensoundsystem_required_vars OpenSoundSystemSDL_LIBRARY)
endif()
list(APPEND _opensoundsystem_required_vars OpenSoundSystemSDL_INCLUDE_PATH)

find_package_handle_standard_args(OpenSoundSystemSDL
  REQUIRED_VARS ${_opensoundsystem_required_vars}
)

if(OpenSoundSystemSDL_FOUND)
  if(NOT TARGET OpenSoundSystemSDL::Headers)
    add_library(OpenSoundSystemSDL::Headers INTERFACE IMPORTED)
    set_target_properties(OpenSoundSystemSDL::Headers
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OpenSoundSystemSDL_INCLUDE_PATH}"
    )
  endif()
  if(_opensoundsystem_needs_library AND OpenSoundSystemSDL_LIBRARY)
    if(NOT TARGET OpenSoundSystemSDL::OpenSoundSystemSDL)
        add_library(OpenSoundSystemSDL::OpenSoundSystemSDL SHARED IMPORTED)
        set_target_properties(OpenSoundSystemSDL::OpenSoundSystemSDL
          PROPERTIES
            IMPORTED_LOCATION "${OpenSoundSystemSDL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenSoundSystemSDL_INCLUDE_PATH}"
        )
        sdl_add_soname_library(OpenSoundSystemSDL::OpenSoundSystemSDL)
    endif()
  endif()
endif()
