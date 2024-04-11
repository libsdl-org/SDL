/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
cmake_minimum_required(VERSION 3.4.1)
project(entry)

set(env ${CMAKE_CURRENT_SOURCE_DIR})
set(SDL_HOME_PATH ohos_sdl2)
set(NATIVE_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${SDL_HOME_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/${SDL_HOME_PATH}/src/video/khronos/GLES2)
set(NATIVE_ENTRY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/application)
set(NATIVE_TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDL/src/test)


add_subdirectory(${SDL_HOME_PATH})
include_directories(${NATIVE_INCLUDE_PATH})

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

file(GLOB my_files "${NATIVE_ENTRY_PATH}/*.c" "${NATIVE_ENTRY_PATH}/*.h" "${NATIVE_TEST_PATH}/*.c")

add_library(entry SHARED ${my_files})

add_dependencies(entry SDL2)

target_link_libraries(entry PUBLIC libSDL2d.so)