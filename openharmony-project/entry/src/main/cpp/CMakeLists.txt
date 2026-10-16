# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(loopwave)

set(NATIVEAPP_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

add_subdirectory("SDL")

include_directories(${NATIVEAPP_ROOT_PATH}
                    ${NATIVEAPP_ROOT_PATH}/include
                    ${NATIVEAPP_ROOT_PATH}/SDL/include)

add_library(main SHARED myapp.c)
target_link_libraries(main PUBLIC SDL3::SDL3)
