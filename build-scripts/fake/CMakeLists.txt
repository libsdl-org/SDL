cmake_minimum_required(VERSION 3.16)
project(fake-SDL2 LANGUAGES C VERSION "2.31.0")

get_filename_component(SDL2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
message(STATUS "SDL2 root directory = ${SDL2_ROOT}")

include(CheckCCompilerFlag)
include(CheckLinkerFlag)
include(CMakePushCheckState)
include(GNUInstallDirs)

add_library(SDL2-Headers INTERFACE)
target_include_directories(SDL2-Headers INTERFACE "$<BUILD_INTERFACE:${SDL2_ROOT}/include>")
target_include_directories(SDL2-Headers INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(SDL2-Headers INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>")
set_property(TARGET SDL2-Headers PROPERTY EXPORT_NAME "Headers")

add_library(SDL2-fake SHARED
  "fake.c"
  "${SDL2_ROOT}/src/main/windows/version.rc"
)
target_link_libraries(SDL2-fake PUBLIC SDL2-Headers)
target_include_directories(SDL2-fake PRIVATE "$<BUILD_INTERFACE:${SDL2_ROOT}/src>")
set_property(TARGET SDL2-fake PROPERTY OUTPUT_NAME "SDL2")
set_property(TARGET SDL2-fake PROPERTY DEFINE_SYMBOL "BUILD_DLL")
set_property(TARGET SDL2-fake PROPERTY EXPORT_NAME "SDL2")

add_library(SDL2main STATIC)
target_link_libraries(SDL2main PRIVATE SDL2-Headers)
if(WIN32)
  target_sources(SDL2main PRIVATE "${SDL2_ROOT}/src/main/windows/SDL_windows_main.c")
else()
  target_sources(SDL2main PRIVATE "${SDL2_ROOT}/src/main/dummy/SDL_windows_main.c")
endif()

# Increment this if there is an incompatible change - but if that happens,
# we should rename the library from SDL2 to SDL4, at which point this would
# reset to 0 anyway.
set(SDL_SO_VERSION_MAJOR "0")
set(SDL_SO_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(SDL_SO_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(SDL_SO_VERSION "${SDL_SO_VERSION_MAJOR}.${SDL_SO_VERSION_MINOR}.${SDL_SO_VERSION_PATCH}")

if(PROJECT_VERSION_MINOR MATCHES "[02468]$")
  math(EXPR SDL_DYLIB_COMPAT_VERSION_MAJOR "100 * ${PROJECT_VERSION_MINOR} + 1")
  set(SDL_DYLIB_COMPAT_VERSION_MINOR "0")
  math(EXPR SDL_DYLIB_CURRENT_VERSION_MAJOR "${SDL_DYLIB_COMPAT_VERSION_MAJOR}")
  set(SDL_DYLIB_CURRENT_VERSION_MINOR "${PROJECT_VERSION_PATCH}")
else()
  math(EXPR SDL_DYLIB_COMPAT_VERSION_MAJOR "100 * ${PROJECT_VERSION_MINOR} + ${PROJECT_VERSION_PATCH} + 1")
  set(SDL_DYLIB_COMPAT_VERSION_MINOR "0")
  math(EXPR SDL_DYLIB_CURRENT_VERSION_MAJOR "${SDL_DYLIB_COMPAT_VERSION_MAJOR}")
  set(SDL_DYLIB_CURRENT_VERSION_MINOR "0")
endif()
set(SDL_DYLIB_CURRENT_VERSION_PATCH "0")
set(SDL_DYLIB_COMPAT_VERSION_PATCH "0")

set(SDL_DYLIB_CURRENT_VERSION "${SDL_DYLIB_CURRENT_VERSION_MAJOR}.${SDL_DYLIB_CURRENT_VERSION_MINOR}.${SDL_DYLIB_CURRENT_VERSION_PATCH}")
set(SDL_DYLIB_COMPAT_VERSION "${SDL_DYLIB_COMPAT_VERSION_MAJOR}.${SDL_DYLIB_COMPAT_VERSION_MINOR}.${SDL_DYLIB_COMPAT_VERSION_PATCH}")

message(DEBUG "SDL_SO_VERSION=${SDL_SO_VERSION} SDL_DYLIB_CURRENT_VERSION=${SDL_DYLIB_CURRENT_VERSION} SDL_DYLIB_COMPAT_VERSION=${SDL_DYLIB_COMPAT_VERSION}")

if(APPLE)
  cmake_minimum_required(VERSION 3.17)
  set_target_properties(SDL2-fake PROPERTIES
    MACOSX_RPATH TRUE
    FRAMEWORK "${SDL_FRAMEWORK}"
    SOVERSION "${SDL_SO_VERSION_MAJOR}"
    MACHO_COMPATIBILITY_VERSION "${SDL_DYLIB_COMPAT_VERSION}"
    MACHO_CURRENT_VERSION "${SDL_DYLIB_CURRENT_VERSION}"
  )
  if(SDL_FRAMEWORK)
    set_target_properties(SDL2-fake PROPERTIES
      PUBLIC_HEADER "${SDL2_INCLUDE_FILES}"
      FRAMEWORK_VERSION "${SDL_FRAMEWORK_VERSION}"
      MACOSX_FRAMEWORK_IDENTIFIER "org.libsdl.SDL2"
      RESOURCE "${SDL_FRAMEWORK_RESOURCES}"
    )
  endif()
elseif(UNIX AND NOT ANDROID)
  set_target_properties(SDL2-fake PROPERTIES
    VERSION "${SDL_SO_VERSION}"
    SOVERSION "${SDL_SO_VERSION_MAJOR}"
  )
elseif(WIN32 OR CYGWIN)
  set_target_properties(SDL2-fake PROPERTIES
    PREFIX ""
  )
endif()

if(MSVC)
  set(CMAKE_INSTALLDIR "cmake")
else()
  set(CMAKE_INSTALLDIR "${CMAKE_INSTALL_LIBDIR}/cmake/SDL2")
endif()


file(GLOB SDL2_HEADERS "${SDL2_ROOT}/include/*.h")
install(FILES ${SDL2_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL2")
install(TARGETS SDL2-fake SDL2-Headers SDL2main EXPORT SDL2-fake-exports
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(EXPORT SDL2-fake-exports DESTINATION "${CMAKE_INSTALLDIR}" FILE "SDL2Config.cmake")

set(prefix "\${pcfiledir}/../..")
set(exec_prefix "\${pcfiledir}/../..")
set(libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
if(MINGW)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_libraries(SDL2main PUBLIC "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-Wl,--undefined=_WinMain@16>")
  else()
    target_link_libraries(SDL2main PUBLIC "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-Wl,--undefined=WinMain>")
  endif()
endif()
if(WIN32)
  set(SDL_LIBS "-lmingw32 -lSDL2main -lSDL2")
else()
  set(SDL_LIBS "-lSDL2")
endif()
configure_file("${SDL2_ROOT}/sdl2.pc.in" "sdl2.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sdl2.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

set(bin_prefix_relpath "..")
set(ENABLE_SHARED_TRUE "")
set(ENABLE_SHARED_FALSE "#")
set(ENABLE_STATIC_TRUE "#")
set(ENABLE_STATIC_FALSE "")
configure_file("${SDL2_ROOT}/sdl2-config.in" "sdl2-config")
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/sdl2-config" DESTINATION "${CMAKE_INSTALL_BINDIR}")
