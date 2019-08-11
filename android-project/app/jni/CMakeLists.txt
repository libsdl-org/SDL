cmake_minimum_required(VERSION 3.6)

project(GAME)

# Settings
set(HIDAPI OFF CACHE BOOL "" FORCE)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()


# SDL sources are in a subfolder named "SDL"
add_subdirectory(SDL)

# Your game and its CMakeLists.txt are in a subfolder named "src"
add_subdirectory(src)

