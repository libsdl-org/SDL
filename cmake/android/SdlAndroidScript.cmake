#[=======================================================================[

This CMake script is meant to be used in CMake script mode (cmake -P).
It wraps commands that communicate with an actual Android device.
Because

#]=======================================================================]

cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_SCRIPT_MODE_FILE)
  message(FATAL_ERROR "This file can only be used in CMake script mode")
endif()
if(NOT ADB)
  set(ADB "adb")
endif()

if(NOT ACTION)
  message(FATAL_ERROR "Missing ACTION argument")
endif()

if(ACTION STREQUAL "uninstall")
  # The uninstall action attempts to uninstall all packages. All failures are ignored.
  foreach(package IN LISTS PACKAGES)
    message("Uninstalling ${package} ...")
    execute_process(
        COMMAND ${ADB} uninstall ${package}
        RESULT_VARIABLE res
    )
    message("... result=${res}")
  endforeach()
elseif(ACTION STREQUAL "install")
  # The install actions attempts to install APK's to an Android device using adb. Failures are ignored.
  set(failed_apks "")
  foreach(apk IN LISTS APKS)
    message("Installing ${apk} ...")
    execute_process(
        COMMAND ${ADB} install -d -r --streaming ${apk}
        RESULT_VARIABLE res
    )
    message("... result=${res}")
    if(NOT res EQUAL 0)
      list(APPEND failed_apks ${apk})
    endif()
  endforeach()
  if(failed_apks)
    message(FATAL_ERROR "Failed to install ${failed_apks}")
  endif()
elseif(ACTION STREQUAL "build-install-run")
  if(NOT EXECUTABLES)
    message(FATAL_ERROR "Missing EXECUTABLES (don't know what executables to build/install and start")
  endif()
  if(NOT BUILD_FOLDER)
    message(FATAL_ERROR "Missing BUILD_FOLDER (don't know where to build the APK's")
  endif()
  set(install_targets "")
  foreach(executable IN LISTS EXECUTABLES)
    list(APPEND install_targets "install-${executable}")
  endforeach()
  execute_process(
      COMMAND ${CMAKE_COMMAND} --build "${BUILD_FOLDER}" --target ${install_targets}
      RESULT_VARIABLE res
  )
  if(NOT res EQUAL 0)
    message(FATAL_ERROR "Failed to install APK(s) for ${EXECUTABLES}")
  endif()
  list(GET EXECUTABLES 0 start_executable)
  execute_process(
      COMMAND ${CMAKE_COMMAND} --build "${BUILD_FOLDER}" --target start-${start_executable}
      RESULT_VARIABLE res
  )
else()
  message(FATAL_ERROR "Unknown ACTION=${ACTION}")
endif()
