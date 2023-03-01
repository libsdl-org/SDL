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

set_and_check(_sdl3_prefix      "${CMAKE_CURRENT_LIST_DIR}/..")
set(_sdl3_include_dirs          "${_sdl3_prefix}/include;${_sdl3_prefix}/include/SDL3")
unset(_sdl3_prefix)

set(SDL3_LIBRARIES      SDL3::SDL3)
set(SDL3TEST_LIBRARY    SDL3::SDL3_test)


# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3-target.cmake files.

if(NOT TARGET SDL3::Headers)
    add_library(SDL3::Headers INTERFACE IMPORTED)
    set_target_properties(SDL3::SDL3
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3_include_dirs}"
    )
endif()
set(SDL3_Headers_FOUND TRUE)
unset(_sdl3_include_dirs)

set(_sdl3_library     "${SDL3_LIBDIR}/SDL3.lib")
set(_sdl3_dll_library "${SDL3_BINDIR}/SDL3.dll")
if(EXISTS "${_sdl3_library}" AND EXISTS "${_sdl3_dll_library}")
    if(NOT TARGET SDL3::SDL3-shared)
        add_library(SDL3::SDL3-shared SHARED IMPORTED)
        set_target_properties(SDL3::SDL3-shared
            PROPERTIES
                INTERFACE_LINK_LIBRARIES "SDL3::Headers"
                IMPORTED_IMPLIB "${_sdl3_library}"
                IMPORTED_LOCATION "${_sdl3_dll_library}"
                COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
                INTERFACE_SDL3_SHARED "ON"
                COMPATIBLE_INTERFACE_STRING "SDL_VERSION"
                INTERFACE_SDL_VERSION "SDL3"
        )
    endif()
    set(SDL3_SDL3-shared_FOUND TRUE)
else()
    set(SDL3_SDL3-shared_FOUND FALSE)
endif()
unset(_sdl3_library)
unset(_sdl3_dll_library)

set(SDL3_SDL3-static_FOUND FALSE)

set(_sdl3test_library "${SDL3_LIBDIR}/SDL3_test.lib")
if(EXISTS "${_sdl3test_library}")
    if(NOT TARGET SDL3::SDL3_test)
        add_library(SDL3::SDL3_test STATIC IMPORTED)
        set_target_properties(SDL3::SDL3_test
            PROPERTIES
                INTERFACE_LINK_LIBRARIES "SDL3::Headers"
                IMPORTED_LOCATION "${_sdl3test_library}"
                COMPATIBLE_INTERFACE_STRING "SDL_VERSION"
                INTERFACE_SDL_VERSION "SDL3"
        )
    endif()
    set(SDL3_SDL3_test_FOUND TRUE)
else()
    set(SDL3_SDL3_test_FOUND FALSE)
endif()
unset(_sdl3test_library)

if(SDL3_SDL3-shared_FOUND OR SDL3_SDL3-static_FOUND)
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
    else()
        _sdl_create_target_alias_compat(SDL3::SDL3 SDL3::SDL3-static)
    endif()
endif()

check_required_components(SDL3)
