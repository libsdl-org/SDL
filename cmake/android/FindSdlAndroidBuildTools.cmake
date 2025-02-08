#[=======================================================================[

FindSdlAndroidBuildTools
----------------------

Locate the Android build tools directory.


Imported targets
^^^^^^^^^^^^^^^^

This find module defines the following :prop_tgt:`IMPORTED` target(s):

<none>

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

`` SdlAndroidBuildTools_FOUND
   if false, no Android build tools have been found

`` SDL_ANDROID_BUILD_TOOLS_ROOT
   path of the Android build tools root directory if found

`` SDL_ANDROID_BUILD_TOOLS_VERSION
   the human-readable string containing the android build tools version if found

Cache variables
^^^^^^^^^^^^^^^

These variables may optionally be set to help this module find the correct files:

``SDL_ANDROID_BUILD_TOOLS_ROOT``
  path of the Android build tools root directory


Variables for locating Android platform
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This module responds to the flags:

``SDL_ANDROID_HOME
    First, this module will look for platforms in this CMake variable.

``ANDROID_HOME
    If no platform was found in `SDL_ANDROID_HOME`, then try `ANDROID_HOME`.

``$ENV{ANDROID_HOME}
    If no platform was found in neither `SDL_ANDROID_HOME` or `ANDROID_HOME`, then try `ANDROID_HOME}`

#]=======================================================================]

cmake_minimum_required(VERSION 3.7...3.28)

if(NOT PROJECT_NAME MATCHES "^SDL.*")
  message(WARNING "This module is internal to SDL and is currently not supported.")
endif()

function(_sdl_is_valid_android_build_tools_root RESULT VERSION BUILD_TOOLS_ROOT)
  set(result TRUE)
  set(version -1)

  string(REGEX MATCH "/([0-9.]+)$" root_match "${BUILD_TOOLS_ROOT}")
  if(root_match
      AND EXISTS "${BUILD_TOOLS_ROOT}/aapt2"
      AND EXISTS "${BUILD_TOOLS_ROOT}/apksigner"
      AND EXISTS "${BUILD_TOOLS_ROOT}/d8"
      AND EXISTS "${BUILD_TOOLS_ROOT}/zipalign")
    set(result "${BUILD_TOOLS_ROOT}")
    set(version "${CMAKE_MATCH_1}")
  endif()

  set(${RESULT} ${result} PARENT_SCOPE)
  set(${VERSION} ${version} PARENT_SCOPE)
endfunction()

function(_find_sdl_android_build_tools_root ROOT)
  cmake_parse_arguments(fsabtr "" "" "" ${ARGN})
  set(homes ${SDL_ANDROID_HOME} ${ANDROID_HOME} $ENV{ANDROID_HOME})
  set(root ${ROOT}-NOTFOUND)
  foreach(home IN LISTS homes)
    if(NOT IS_DIRECTORY "${home}")
      continue()
    endif()
    file(GLOB build_tools_roots LIST_DIRECTORIES true "${home}/build-tools/*")
    set(max_build_tools_version -1)
    set(max_build_tools_root "")
    foreach(build_tools_root IN LISTS build_tools_roots)
      _sdl_is_valid_android_build_tools_root(is_valid build_tools_version "${build_tools_root}")
      if(is_valid AND build_tools_version GREATER max_build_tools_version)
        set(max_build_tools_version "${build_tools_version}")
        set(max_build_tools_root "${build_tools_root}")
      endif()
    endforeach()
    if(max_build_tools_version GREATER -1)
      set(root ${max_build_tools_root})
      break()
    endif()
  endforeach()
  set(${ROOT} ${root} PARENT_SCOPE)
endfunction()

if(NOT DEFINED SDL_ANDROID_BUILD_TOOLS_ROOT)
  _find_sdl_android_build_tools_root(SDL_ANDROID_BUILD_TOOLS_ROOT)
  set(SDL_ANDROID_BUILD_TOOLS_ROOT "${SDL_ANDROID_BUILD_TOOLS_ROOT}" CACHE PATH "Path of Android build tools")
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SdlAndroidBuildTools
  VERSION_VAR SDL_ANDROID_BUILD_TOOLS_VERSION
  REQUIRED_VARS SDL_ANDROID_BUILD_TOOLS_ROOT
)
