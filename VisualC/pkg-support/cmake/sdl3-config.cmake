# SDL CMake configuration file:
# This file is meant to be placed in a cmake subfolder of SDL3-devel-3.x.y-VC

cmake_minimum_required(VERSION 3.0)

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

if(CMAKE_SIZEOF_VOID_P STREQUAL "4")
    set(_sdl_arch_subdir "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL "8")
    set(_sdl_arch_subdir "x64")
else()
    set(SDL3_FOUND FALSE)
    return()
endif()

# For compatibility with autotools sdl3-config.cmake, provide SDL3_* variables.

set_and_check(SDL3_PREFIX       "${CMAKE_CURRENT_LIST_DIR}/..")
set_and_check(SDL3_EXEC_PREFIX  "${CMAKE_CURRENT_LIST_DIR}/..")
set_and_check(SDL3_INCLUDE_DIR  "${SDL3_PREFIX}/include")
set(SDL3_INCLUDE_DIRS           "${SDL3_INCLUDE_DIR}")
set_and_check(SDL3_BINDIR       "${SDL3_PREFIX}/lib/${_sdl_arch_subdir}")
set_and_check(SDL3_LIBDIR       "${SDL3_PREFIX}/lib/${_sdl_arch_subdir}")

set(SDL3_LIBRARIES      SDL3::SDL3_main SDL3::SDL3)
set(SDL3MAIN_LIBRARY    SDL3::SDL3_main)
set(SDL3TEST_LIBRARY    SDL3::SDL3_test)


# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3-target.cmake files.

set(_sdl3_library     "${SDL3_LIBDIR}/SDL3.lib")
set(_sdl3_dll_library "${SDL3_BINDIR}/SDL3.dll")
if(EXISTS "${_sdl3_library}" AND EXISTS "${_sdl3_dll_library}")
    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 SHARED IMPORTED)
        set_target_properties(SDL3::SDL3
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIRS}"
                IMPORTED_IMPLIB "${_sdl3_library}"
                IMPORTED_LOCATION "${_sdl3_dll_library}"
                COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
                INTERFACE_SDL3_SHARED "ON"
        )
    endif()
    set(SDL3_SDL3_FOUND TRUE)
else()
    set(SDL3_SDL3_FOUND FALSE)
endif()
unset(_sdl3_library)
unset(_sdl3_dll_library)

set(_sdl3main_library "${SDL3_LIBDIR}/SDL3_main.lib")
if(EXISTS "${_sdl3main_library}")
    if(NOT TARGET SDL3::SDL3_main)
        add_library(SDL3::SDL3_main STATIC IMPORTED)
        set_target_properties(SDL3::SDL3_main
        PROPERTIES
            IMPORTED_LOCATION "${_sdl3main_library}"
        )
    endif()
    set(SDL3_SDL3_main_FOUND TRUE)
else()
    set(SDL3_SDL3_FOUND FALSE)
endif()
unset(_sdl3main_library)

set(_sdl3test_library "${SDL3_LIBDIR}/SDL3_test.lib")
if(EXISTS "${_sdl3test_library}")
    if(NOT TARGET SDL3::SDL3_test)
        add_library(SDL3::SDL3_test STATIC IMPORTED)
        set_target_properties(SDL3::SDL3_test
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIRS}"
                IMPORTED_LOCATION "${_sdl3test_library}"
        )
    endif()
    set(SDL3_SDL3_test_FOUND TRUE)
else()
    set(SDL3_SDL3_FOUND FALSE)
endif()
unset(_sdl3test_library)

check_required_components(SDL3)
