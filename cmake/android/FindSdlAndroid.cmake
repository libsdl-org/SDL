#[=======================================================================[

FindSdlAndroid
----------------------

Locate various executables that are essential to creating an Android APK archive.
This find module uses the FindSdlAndroidBuildTools module to locate some Android utils.


Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` target(s):

`` SdlAndroid::aapt2 ``
   Imported executable for the "android package tool" v2

`` SdlAndroid::apksigner``
   Imported executable for the APK signer tool

`` SdlAndroid::d8 ``
   Imported executable for the dex compiler

`` SdlAndroid::zipalign ``
   Imported executable for the zipalign util

`` SdlAndroid::adb ``
   Imported executable for the "android debug bridge" tool

`` SdlAndroid::keytool ``
   Imported executable for the keytool, a key and certificate management utility

`` SdlAndroid::zip ``
   Imported executable for the zip, for packaging and compressing files

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

`` AAPT2_BIN ``
   Path of aapt2

`` APKSIGNER_BIN ``
   Path of apksigner

`` D8_BIN ``
   Path of d8

`` ZIPALIGN_BIN ``
   Path of zipalign

`` ADB_BIN ``
   Path of adb

`` KEYTOOL_BIN ``
   Path of keytool

`` ZIP_BIN ``
   Path of zip

#]=======================================================================]

cmake_minimum_required(VERSION 3.7)

if(NOT PROJECT_NAME MATCHES "^SDL.*")
  message(WARNING "This module is internal to SDL and is currently not supported.")
endif()

find_package(SdlAndroidBuildTools MODULE)

function(_sdl_android_find_create_imported_executable NAME)
  string(TOUPPER "${NAME}" NAME_UPPER)
  set(varname "${NAME_UPPER}_BIN")
  find_program("${varname}" NAMES "${NAME}" PATHS ${SDL_ANDROID_BUILD_TOOLS_ROOT})
  if(EXISTS "${${varname}}" AND NOT TARGET SdlAndroid::${NAME})
    add_executable(SdlAndroid::${NAME} IMPORTED)
    set_property(TARGET SdlAndroid::${NAME} PROPERTY IMPORTED_LOCATION "${${varname}}")
  endif()
endfunction()

if(SdlAndroidBuildTools_FOUND)
  _sdl_android_find_create_imported_executable(aapt2)
  _sdl_android_find_create_imported_executable(apksigner)
  _sdl_android_find_create_imported_executable(d8)
  _sdl_android_find_create_imported_executable(zipalign)
endif()

_sdl_android_find_create_imported_executable(adb)
_sdl_android_find_create_imported_executable(keytool)
_sdl_android_find_create_imported_executable(zip)
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SdlAndroid
  VERSION_VAR
  REQUIRED_VARS
    AAPT2_BIN
    APKSIGNER_BIN
    D8_BIN
    ZIPALIGN_BIN
    KEYTOOL_BIN
    ZIP_BIN
)
