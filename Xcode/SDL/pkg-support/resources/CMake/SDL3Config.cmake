# SDL3 CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL3 framework for macOS,
# or in the CMake directory of a SDL3 framework for iOS / tvOS / visionOS.

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12...4.0)

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

# Compute the installation prefix relative to this file:
# search upwards for the .framework directory
set(_current_path "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_current_path "${_current_path}" REALPATH)
set(_sdl3_framework_path "")

while(NOT _sdl3_framework_path)
    if(IS_DIRECTORY "${_current_path}" AND "${_current_path}" MATCHES "/SDL3\\.framework$")
        set(_sdl3_framework_path "${_current_path}")
        break()
    endif()
    get_filename_component(_next_current_path "${_current_path}" DIRECTORY)
    if("${_current_path}" STREQUAL "${_next_current_path}")
        break()
    endif()
    set(_current_path "${_next_current_path}")
endwhile()
unset(_current_path)
unset(_next_current_path)

if(NOT _sdl3_framework_path)
    message(FATAL_ERROR "Could not find SDL3.framework root from ${CMAKE_CURRENT_LIST_DIR}")
endif()

get_filename_component(_sdl3_framework_parent_path "${_sdl3_framework_path}" PATH)

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3-target.cmake files.

if(NOT TARGET SDL3::Headers)
    add_library(SDL3::Headers INTERFACE IMPORTED)
    set_target_properties(SDL3::Headers
        PROPERTIES
            INTERFACE_COMPILE_OPTIONS "SHELL:-F \"${_sdl3_framework_parent_path}\""
    )
endif()
set(SDL3_Headers_FOUND TRUE)

if(NOT TARGET SDL3::SDL3-shared)
    add_library(SDL3::SDL3-shared SHARED IMPORTED)
    set_target_properties(SDL3::SDL3-shared
        PROPERTIES
            FRAMEWORK "TRUE"
            IMPORTED_LOCATION "${_sdl3_framework_path}/SDL3"
            INTERFACE_LINK_LIBRARIES "SDL3::Headers"
            COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
            INTERFACE_SDL3_SHARED "ON"
            COMPATIBLE_INTERFACE_STRING "SDL_VERSION"
            INTERFACE_SDL_VERSION "SDL3"
    )
endif()
set(SDL3_SDL3-shared_FOUND TRUE)

set(SDL3_SDL3-static FALSE)

set(SDL3_SDL3_test FALSE)

unset(_sdl3_framework_parent_path)
unset(_sdl3_framework_path)

if(SDL3_SDL3-shared_FOUND)
    set(SDL3_SDL3_FOUND TRUE)
endif()

function(_sdl_create_target_alias_compat NEW_TARGET TARGET)
    if(CMAKE_VERSION VERSION_LESS "3.18")
        # Aliasing local targets is not supported on CMake < 3.18, so make it global.
        add_library(${NEW_TARGET} INTERFACE IMPORTED)
        set_target_properties(${NEW_TARGET} PROPERTIES INTERFACE_LINK_LIBRARIES "${TARGET}")
    else()
        add_library(${NEW_TARGET} ALIAS ${TARGET})
    endif()
endfunction()

# Make sure SDL3::SDL3 always exists
if(NOT TARGET SDL3::SDL3)
    if(TARGET SDL3::SDL3-shared)
        _sdl_create_target_alias_compat(SDL3::SDL3 SDL3::SDL3-shared)
    endif()
endif()

check_required_components(SDL3)

set(SDL3_LIBRARIES SDL3::SDL3)
set(SDL3_STATIC_LIBRARIES SDL3::SDL3-static)
set(SDL3_STATIC_PRIVATE_LIBS)

set(SDL3TEST_LIBRARY SDL3::SDL3_test)
