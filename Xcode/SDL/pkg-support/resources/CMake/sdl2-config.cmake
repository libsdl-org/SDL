# SDL2 CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL2 framework

cmake_minimum_required(VERSION 3.0)

set(_sdl2_known_comps SDL2)
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

set(SDL2_INCLUDE_DIR    "${CMAKE_CURRENT_LIST_DIR}/../../Headers")
set(SDL2_INCLUDE_DIRS   "${SDL2_INCLUDE_DIR}")
set(SDL2_LIBRARY        "${CMAKE_CURRENT_LIST_DIR}/../../SDL2")


# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL2-target.cmake files.

if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2
        PROPERTIES
            FRAMEWORK 1
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
endif()
