# SDL2 CMake configuration file:
# This file is meant to be placed in a cmake subfolder of SDL2-devel-2.x.y-VC

cmake_minimum_required(VERSION 3.0)

set(_sdl2_known_comps SDL2 SDL2main SDL2test)
if(NOT SDL2_FIND_COMPONENTS)
    set(SDL2_FIND_COMPONENTS ${_sdl2_known_comps})
endif()

# Fail early when an unknown component is requested
list(REMOVE_DUPLICATES SDL2_FIND_COMPONENTS)
list(REMOVE_ITEM SDL2_FIND_COMPONENTS ${_sdl2_known_comps})
if(SDL2_FIND_COMPONENTS)
    set(missing_required_comps)
    foreach(missingcomp ${SDL2_FIND_COMPONENTS})
        if(SDL2_FIND_REQUIRED_${missingcomp})
            list(APPEND missing_required_comps ${missingcomp})
        endif()
    endforeach()
    if(missing_required_comps)
        if(NOT SDL2_FIND_QUIETLY)
            message(WARNING "SDL2: following component(s) are not available: ${missing_required_comps}")
        endif()
        set(SDL2_FOUND FALSE)
        return()
    endif()
endif()

set(SDL2_FOUND TRUE)

if(CMAKE_SIZEOF_VOID_P STREQUAL "4")
    set(_sdl_arch_subdir "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL "8")
    set(_sdl_arch_subdir "x64")
else()
    set(SDL2_FOUND FALSE)
    return()
endif()

set(SDL2_INCLUDE_DIR    "${CMAKE_CURRENT_LIST_DIR}/../include")
set(SDL2_INCLUDE_DIRS   "${SDL2_INCLUDE_DIR}")
set(SDL2_LIBRARY        "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL2.lib")
set(SDL2_DLL_LIBRARY    "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL2.dll")
set(SDL2main_LIBRARY    "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL2main.lib")
set(SDL2test_LIBRARY    "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL2test.lib")


# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL2-target.cmake files.

if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
            IMPORTED_IMPLIB "${SDL2_LIBRARY}"
            IMPORTED_LOCATION "${SDL2_DLL_LIBRARY}"
    )
endif()

if(NOT TARGET SDL2::SDL2main)
    add_library(SDL2::SDL2main STATIC IMPORTED)
    set_target_properties(SDL2::SDL2main
        PROPERTIES
            IMPORTED_LOCATION "${SDL2main_LIBRARY}"
    )
endif()

if(NOT TARGET SDL2::SDL2test)
    add_library(SDL2::SDL2test STATIC IMPORTED)
    set_target_properties(SDL2::SDL2test
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
            IMPORTED_LOCATION "${SDL2test_LIBRARY}"
    )
endif()
