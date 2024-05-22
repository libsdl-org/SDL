# SDL3 CMake configuration file:
# This file is meant to be placed in share/cmake/SDL3, next to SDL3.xcframework

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

macro(_check_target_is_simulator)
    include(CheckCSourceCompiles)
    check_c_source_compiles([===[
    #include <TargetConditionals.h>
    #if defined(TARGET_OS_SIMULATOR)
    int target_is_simulator;
    #endif
    int main(int argc, char *argv[]) { return target_is_simulator; }
    ]===] SDL_TARGET_IS_SIMULATOR)
endmacro()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    _check_target_is_simulator()
    if(SDL_TARGET_IS_SIMULATOR)
        set(_xcfw_target_subdir "ios-arm64_x86_64-simulator")
    else()
        set(_xcfw_target_subdir "ios-arm64")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    _check_target_is_simulator()
    if(SDL_TARGET_IS_SIMULATOR)
        set(_xcfw_target_subdir "tvos-arm64_x86_64-simulator")
    else()
        set(_xcfw_target_subdir "tvos-arm64")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_xcfw_target_subdir "macos-arm64_x86_64")
else()
    message(WARNING "Unsupported Apple platform (${CMAKE_SYSTEM_NAME}) and broken sdl3-config-version.cmake")
    set(SDL3_FOUND FALSE)
    return()
endif()

# Compute the installation prefix relative to this file.
get_filename_component(_sdl3_xcframework_parent_path "${CMAKE_CURRENT_LIST_DIR}" REALPATH)              # /share/cmake/SDL3/
get_filename_component(_sdl3_xcframework_parent_path "${_sdl3_xcframework_parent_path}" REALPATH)       # /share/cmake/SDL3/
get_filename_component(_sdl3_xcframework_parent_path "${_sdl3_xcframework_parent_path}" PATH)           # /share/cmake
get_filename_component(_sdl3_xcframework_parent_path "${_sdl3_xcframework_parent_path}" PATH)           # /share
get_filename_component(_sdl3_xcframework_parent_path "${_sdl3_xcframework_parent_path}" PATH)           # /
set_and_check(_sdl3_xcframework_path "${_sdl3_xcframework_parent_path}/SDL3.xcframework")               # /SDL3.xcframework
set_and_check(_sdl3_framework_parent_path "${_sdl3_xcframework_path}/${_xcfw_target_subdir}")           # /SDL3.xcframework/macos-arm64_x86_64
set_and_check(_sdl3_framework_path "${_sdl3_framework_parent_path}/SDL3.framework")                     # /SDL3.xcframework/macos-arm64_x86_64/SDL3.framework


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
    if(CMAKE_VERSION GREATER_EQUAL "3.28")
        set_target_properties(SDL3::SDL3-shared
            PROPERTIES
                FRAMEWORK "TRUE"
                IMPORTED_LOCATION "${_sdl3_xcframework_path}"
                INTERFACE_LINK_LIBRARIES "SDL3::Headers"
        )
    else()
        set_target_properties(SDL3::SDL3-shared
            PROPERTIES
                FRAMEWORK "TRUE"
                IMPORTED_LOCATION "${_sdl3_framework_path}/SDL3"
                INTERFACE_LINK_LIBRARIES "SDL3::Headers"
        )
    endif()
    set_target_properties(SDL3::SDL3-shared
        PROPERTIES
            COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
            INTERFACE_SDL3_SHARED "ON"
            COMPATIBLE_INTERFACE_STRING "SDL_VERSION"
            INTERFACE_SDL_VERSION "SDL3"
    )
endif()
set(SDL3_SDL3-shared_FOUND TRUE)

set(SDL3_SDL3-static FALSE)

set(SDL3_SDL3_test FALSE)

unset(_sdl3_xcframework_parent_path)
unset(_sdl3_xcframework_path)
unset(_sdl3_framework_parent_path)
unset(_sdl3_framework_path)
unset(_sdl3_include_dirs)

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
