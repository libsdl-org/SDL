# SDL2 CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL2 framework

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12)

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

string(REGEX REPLACE "SDL2\\.framework.*" "SDL2.framework" SDL2_FRAMEWORK_PATH "${CMAKE_CURRENT_LIST_DIR}")
string(REGEX REPLACE "SDL2\\.framework.*" "" SDL2_FRAMEWORK_PARENT_PATH "${CMAKE_CURRENT_LIST_DIR}")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL2-target.cmake files.

if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    set_target_properties(SDL2::SDL2
        PROPERTIES
            INTERFACE_COMPILE_OPTIONS "-F;${SDL2_FRAMEWORK_PARENT_PATH}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_FRAMEWORK_PATH}/Headers"
            INTERFACE_LINK_OPTIONS "-F;${SDL2_FRAMEWORK_PARENT_PATH};-framework;SDL2"
    )
endif()
