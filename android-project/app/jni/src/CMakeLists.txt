cmake_minimum_required(VERSION 3.6)

project(my_app)

if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 CONFIG REQUIRED)
endif()

add_library(main SHARED
    YourSourceHere.c
)
target_link_libraries(main PRIVATE SDL3::SDL3)
