# SDL CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL3 framework

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12)

include(FeatureSummary)
set_package_properties(SDL3 PROPERTIES
    URL "https://www.libsdl.org/"
    DESCRIPTION "low level access to audio, keyboard, mouse, joystick, and graphics hardware"
)

# Copied from `configure_package_config_file`
macro(set_and_check _var _file)
    set(${_var} "${_file}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
    endif()
endmacro()

# Copied from `configure_package_config_file`
macro(check_required_components _NAME)
    foreach(comp ${${_NAME}_FIND_COMPONENTS})
        if(NOT ${_NAME}_${comp}_FOUND)
            if(${_NAME}_FIND_REQUIRED_${comp})
                set(${_NAME}_FOUND FALSE)
            endif()
        endif()
    endforeach()
endmacro()

set(SDL3_FOUND TRUE)

string(REGEX REPLACE "SDL3\\.framework.*" "SDL3.framework" SDL3_FRAMEWORK_PATH "${CMAKE_CURRENT_LIST_DIR}")
string(REGEX REPLACE "SDL3\\.framework.*" "" SDL3_FRAMEWORK_PARENT_PATH "${CMAKE_CURRENT_LIST_DIR}")

# For compatibility with autotools sdl3-config.cmake, provide SDL3_* variables.

set_and_check(SDL3_PREFIX       "${SDL3_FRAMEWORK_PATH}")
set_and_check(SDL3_EXEC_PREFIX  "${SDL3_FRAMEWORK_PATH}")
set_and_check(SDL3_INCLUDE_DIR  "${SDL3_FRAMEWORK_PATH}/Headers")
set(SDL3_INCLUDE_DIRS           "${SDL3_INCLUDE_DIR};${SDL3_FRAMEWORK_PATH}")
set_and_check(SDL3_BINDIR       "${SDL3_FRAMEWORK_PATH}")
set_and_check(SDL3_LIBDIR       "${SDL3_FRAMEWORK_PATH}")

set(SDL3_LIBRARIES "SDL3::SDL3")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3-target.cmake files.

if(NOT TARGET SDL3::SDL3)
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    set_target_properties(SDL3::SDL3
        PROPERTIES
            INTERFACE_COMPILE_OPTIONS "SHELL:-F \"${SDL3_FRAMEWORK_PARENT_PATH}\""
            INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIRS}"
            INTERFACE_LINK_OPTIONS "SHELL:-F \"${SDL3_FRAMEWORK_PARENT_PATH}\";SHELL:-framework SDL3"
            COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
            INTERFACE_SDL3_SHARED "ON"
    )
endif()
set(SDL3_SDL3_FOUND TRUE)

check_required_components(SDL3)
