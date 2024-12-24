#[=======================================================================[

FindSdlAndroidPlatform
----------------------

Locate the Android SDK platform.


Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` target(s):

<none>

Result variables
^^^^^^^^^^^^^^^^

This find module will set the following variables in your project:

`` SdlAndroidPlatform_FOUND
   if false, no Android platform has been found

`` SDL_ANDROID_PLATFORM_ROOT
   path of the Android SDK platform root directory if found

`` SDL_ANDROID_PLATFORM_ANDROID_JAR
   path of the Android SDK platform jar file if found

`` SDL_ANDROID_PLATFORM_VERSION
   the human-readable string containing the android platform version if found

Cache variables
^^^^^^^^^^^^^^^

These variables may optionally be set to help this module find the correct files:

``SDL_ANDROID_PLATFORM_ROOT``
  path of the Android SDK platform root directory


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

function(_sdl_is_valid_android_platform_root RESULT VERSION PLATFORM_ROOT)
    set(result FALSE)
    set(version -1)

    string(REGEX MATCH "/android-([0-9]+)$" root_match "${PLATFORM_ROOT}")
    if(root_match AND EXISTS "${PLATFORM_ROOT}/android.jar")
        set(result TRUE)
        set(version "${CMAKE_MATCH_1}")
    endif()

    set(${RESULT} ${result} PARENT_SCOPE)
    set(${VERSION} ${version} PARENT_SCOPE)
endfunction()

function(_sdl_find_android_platform_root ROOT)
  cmake_parse_arguments(sfapr "" "" "" ${ARGN})
  set(homes ${SDL_ANDROID_HOME} ${ANDROID_HOME} $ENV{ANDROID_HOME})
  set(root ${ROOT}-NOTFOUND)
  foreach(home IN LISTS homes)
    if(NOT IS_DIRECTORY "${home}")
      continue()
    endif()
    file(GLOB platform_roots LIST_DIRECTORIES true "${home}/platforms/*")
    set(max_platform_version -1)
    set(max_platform_root "")
    foreach(platform_root IN LISTS platform_roots)
      _sdl_is_valid_android_platform_root(is_valid platform_version "${platform_root}")
      if(is_valid AND platform_version GREATER max_platform_version)
        set(max_platform_version "${platform_version}")
        set(max_platform_root "${platform_root}")
      endif()
    endforeach()
    if(max_platform_version GREATER -1)
      set(root ${max_platform_root})
      break()
    endif()
  endforeach()
  set(${ROOT} ${root} PARENT_SCOPE)
endfunction()

set(SDL_ANDROID_PLATFORM_ANDROID_JAR "SDL_ANDROID_PLATFORM_ANDROID_JAR-NOTFOUND")

if(NOT DEFINED SDL_ANDROID_PLATFORM_ROOT)
  _sdl_find_android_platform_root(_new_sdl_android_platform_root)
  set(SDL_ANDROID_PLATFORM_ROOT "${_new_sdl_android_platform_root}" CACHE PATH "Path of Android platform")
  unset(_new_sdl_android_platform_root)
endif()
if(SDL_ANDROID_PLATFORM_ROOT)
  _sdl_is_valid_android_platform_root(_valid SDL_ANDROID_PLATFORM_VERSION "${SDL_ANDROID_PLATFORM_ROOT}")
  if(_valid)
    set(SDL_ANDROID_PLATFORM_ANDROID_JAR "${SDL_ANDROID_PLATFORM_ROOT}/android.jar")
  endif()
  unset(_valid)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SdlAndroidPlatform
  VERSION_VAR SDL_ANDROID_PLATFORM_VERSION
  REQUIRED_VARS SDL_ANDROID_PLATFORM_ROOT SDL_ANDROID_PLATFORM_ANDROID_JAR
)
