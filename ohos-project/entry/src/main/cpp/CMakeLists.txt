# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(MyApplication10)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../SDL_port)
add_subdirectory(../../../../../SDL_port external/sdlbin)
else()
	message(FATAL_ERROR "没有找到 SDL 主项目，需要将 OpenMinecraft-Dev/SDL clone 到与本项目同位置的文件夹中")
endif()

# add_subdirectory(../../../../../openal-soft external/openalbin)

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
add_definitions(-DVK_USE_PLATFORM_OHOS=1)
include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
	            ${SDL3_SOURCE_DIR}/include)

add_library(entry SHARED napi_init.cpp)
add_dependencies(entry SDL3::SDL3-shared)
target_link_libraries(entry PUBLIC libace_napi.z.so libSDL3.so libvulkan.so libGLESv2.so libohaudio.so)
