cmake_minimum_required(VERSION 3.16)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  message(FATAL_ERROR "Prevented in-tree build. Please create a build directory outside of the SDL source code and run \"cmake -S ${CMAKE_SOURCE_DIR} -B .\" from there")
endif()

# See docs/release_checklist.md
project(SDL3 LANGUAGES C VERSION "3.1.2")

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SDL3_SUBPROJECT OFF)
else()
  set(SDL3_SUBPROJECT ON)
endif()

# CMake 3.0 expands the "if(${A})" in "set(OFF 1);set(A OFF);if(${A})" to "if(1)"
# CMake 3.24+ emits a warning when not set.
unset(OFF)
unset(ON)
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckLanguage)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckStructHasMember)
include(CMakeDependentOption)
include(CMakeParseArguments)
include(CMakePushCheckState)
include(GNUInstallDirs)

if(NOT DEFINED OpenGL_GL_PREFERENCE)
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

find_package(PkgConfig)

list(APPEND CMAKE_MODULE_PATH "${SDL3_SOURCE_DIR}/cmake")
include("${SDL3_SOURCE_DIR}/cmake/macros.cmake")
include("${SDL3_SOURCE_DIR}/cmake/sdlchecks.cmake")
include("${SDL3_SOURCE_DIR}/cmake/sdlcompilers.cmake")
include("${SDL3_SOURCE_DIR}/cmake/sdlcpu.cmake")
include("${SDL3_SOURCE_DIR}/cmake/sdlmanpages.cmake")
include("${SDL3_SOURCE_DIR}/cmake/sdlplatform.cmake")
include("${SDL3_SOURCE_DIR}/cmake/sdltargets.cmake")
include("${SDL3_SOURCE_DIR}/cmake/GetGitRevisionDescription.cmake")
include("${SDL3_SOURCE_DIR}/cmake/3rdparty.cmake")
include("${SDL3_SOURCE_DIR}/cmake/PreseedMSVCCache.cmake")
include("${SDL3_SOURCE_DIR}/cmake/PreseedEmscriptenCache.cmake")

SDL_DetectCompiler()
SDL_DetectTargetCPUArchitectures(SDL_CPUS)

# Increment this if there is an incompatible change - but if that happens,
# we should rename the library from SDL3 to SDL4, at which point this would
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

set(SDL_FRAMEWORK_VERSION "A")

set(SDL_CHECK_REQUIRED_INCLUDES "" CACHE STRING "Extra includes (for CMAKE_REQUIRED_INCLUDES)")
set(SDL_CHECK_REQUIRED_LINK_OPTIONS "" CACHE STRING "Extra link options (for CMAKE_REQUIRED_LINK_OPTIONS)")
mark_as_advanced(SDL_CHECK_REQUIRED_INCLUDES SDL_CHECK_REQUIRED_LINK_OPTIONS)

string(APPEND CMAKE_REQUIRED_FLAGS " -D_GNU_SOURCE=1")
list(APPEND CMAKE_REQUIRED_INCLUDES ${SDL_CHECK_REQUIRED_INCLUDES})
list(APPEND CMAKE_REQUIRED_LINK_OPTIONS ${SDL_CHECK_REQUIRED_LINK_OPTIONS})

# Get the platform
SDL_DetectCMakePlatform()

# Don't mistake macOS for unix
if(UNIX AND NOT ANDROID AND NOT APPLE AND NOT RISCOS)
  set(UNIX_SYS ON)
else()
  set(UNIX_SYS OFF)
endif()

if(UNIX OR APPLE)
  set(UNIX_OR_MAC_SYS ON)
else()
  set(UNIX_OR_MAC_SYS OFF)
endif()

# Emscripten pthreads work, but you need to have a non-pthread fallback build
#  for systems without support. It's not currently enough to not use
#  pthread functions in a pthread-build; it won't start up on unsupported
#  browsers. As such, you have to explicitly enable it on Emscripten builds
#  for the time being. This default will change to ON once this becomes
#  commonly supported in browsers or the Emscripten team makes a single
#  binary work everywhere.
if (UNIX_OR_MAC_SYS AND NOT EMSCRIPTEN)
  set(SDL_PTHREADS_DEFAULT ON)
else()
  set(SDL_PTHREADS_DEFAULT OFF)
endif()

if(UNIX_SYS OR ANDROID)
  set(SDL_CLOCK_GETTIME_DEFAULT ON)
else()
  set(SDL_CLOCK_GETTIME_DEFAULT OFF)
endif()

# The hidraw support doesn't catch Xbox, PS4 and Nintendo controllers,
#  so we'll just use libusb when it's available. libusb does not support iOS,
#  so we default to yes on iOS.
if(IOS OR TVOS OR VISIONOS OR WATCHOS OR ANDROID)
  set(SDL_HIDAPI_LIBUSB_AVAILABLE FALSE)
else()
  set(SDL_HIDAPI_LIBUSB_AVAILABLE TRUE)
endif()

set(SDL_ASSEMBLY_DEFAULT OFF)
if(USE_CLANG OR USE_GCC OR USE_INTELCC OR MSVC_VERSION GREATER 1400)
  set(SDL_ASSEMBLY_DEFAULT ON)
endif()

set(SDL_GCC_ATOMICS_DEFAULT OFF)
if(USE_GCC OR USE_CLANG OR USE_INTELCC OR USE_QCC)
  set(SDL_GCC_ATOMICS_DEFAULT ON)
endif()

# Default option knobs
set(SDL_LIBC_DEFAULT ON)
set(SDL_SYSTEM_ICONV_DEFAULT ON)
if(WINDOWS OR MACOS OR IOS OR TVOS OR VISIONOS OR WATCHOS)
  set(SDL_SYSTEM_ICONV_DEFAULT OFF)
endif()

if(MSVC)
  if(NOT SDL_LIBC)
    # Make sure /RTC1 is disabled, otherwise it will use functions from the CRT
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      string(REGEX REPLACE "/RTC(su|[1su])" "" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
  endif()

  if(MSVC_CLANG)
    # clang-cl treats /W4 as '-Wall -Wextra' -- we don't need -Wextra
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      string(REGEX REPLACE "/W4" "/W3" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
  endif()
endif()

set(SDL_SHARED_DEFAULT ON)
set(SDL_STATIC_DEFAULT ON)

set(SDL_SHARED_AVAILABLE ON)
set(SDL_STATIC_AVAILABLE ON)

# All these *_DEFAULT vars will default to ON if not specified,
# so you only need to override them if they need to be disabled.
if(EMSCRIPTEN)
  # Set up default values for the currently supported set of subsystems:
  # Emscripten/Javascript does not have assembly support, a dynamic library
  # loading architecture, or low-level CPU inspection.
  set(SDL_ASSEMBLY_DEFAULT OFF)
  set(SDL_SHARED_AVAILABLE OFF)
endif()

if(VITA OR PSP OR PS2 OR N3DS OR RISCOS)
  set(SDL_SHARED_AVAILABLE OFF)
endif()

if((RISCOS OR UNIX_SYS) AND NOT (LINUX OR NETBSD OR OPENBSD))
  set(SDL_OSS_DEFAULT ON)
else()
  set(SDL_OSS_DEFAULT OFF)
endif()

if(SDL_SHARED_DEFAULT AND SDL_STATIC_DEFAULT AND SDL_SHARED_AVAILABLE)
  if(DEFINED BUILD_SHARED_LIBS)
    # When defined, use BUILD_SHARED_LIBS as default
    if(BUILD_SHARED_LIBS)
      set(SDL_STATIC_DEFAULT OFF)
    else()
      set(SDL_SHARED_DEFAULT OFF)
    endif()
  else()
    # Default to just building the shared library
    set(SDL_STATIC_DEFAULT OFF)
  endif()
endif()

set(SDL_SUBSYSTEMS )

macro(define_sdl_subsystem _name)
  cmake_parse_arguments("_ds" "" "" "DEPS" ${ARGN})
  string(TOUPPER ${_name} _uname)
  if(NOT DEFINED SDL_${_uname}_DEFAULT)
    set(SDL_${_uname}_DEFAULT ON)
  endif()
  if(_ds_DEPS)
    cmake_dependent_option(SDL_${_uname} "Enable the ${_name} subsystem" "${SDL_${_uname}_DEFAULT}" "${_ds_DEPS}" OFF)
  else()
    option(SDL_${_uname} "Enable the ${_name} subsystem" "${SDL_${_uname}_DEFAULT}")
  endif()
  list(APPEND SDL_SUBSYSTEMS "${_name}")
endmacro()

define_sdl_subsystem(Audio)
define_sdl_subsystem(Video)
define_sdl_subsystem(GPU DEPS SDL_VIDEO)
define_sdl_subsystem(Render DEPS SDL_VIDEO)
define_sdl_subsystem(Camera DEPS SDL_VIDEO)
define_sdl_subsystem(Joystick)
define_sdl_subsystem(Haptic)
define_sdl_subsystem(Hidapi)
define_sdl_subsystem(Power)
define_sdl_subsystem(Sensor)
define_sdl_subsystem(Dialog)

cmake_dependent_option(SDL_FRAMEWORK "Build SDL libraries as Apple Framework" OFF "APPLE" OFF)
if(SDL_FRAMEWORK)
  set(SDL_STATIC_AVAILABLE FALSE)
endif()

if(UNIX AND NOT ANDROID AND NOT RISCOS AND NOT SDL_FRAMEWORK)
  set(SDL_RPATH_DEFAULT ON)
else()
  set(SDL_RPATH_DEFAULT OFF)
endif()

set(SDL_PRESEED_AVAILABLE OFF)
if(COMMAND SDL_Preseed_CMakeCache)
  set(SDL_PRESEED_AVAILABLE ON)
endif()

# Allow some projects to be built conditionally.
set_option(SDL_DISABLE_INSTALL    "Disable installation of SDL3" ${SDL3_SUBPROJECT})
cmake_dependent_option(SDL_DISABLE_INSTALL_CPACK "Create binary SDL3 archive using CPack" ${SDL3_SUBPROJECT} "NOT SDL_DISABLE_INSTALL" ON)
cmake_dependent_option(SDL_DISABLE_INSTALL_DOCS "Install docs for SDL3" ON "NOT SDL_DISABLE_INSTALL;NOT SDL_FRAMEWORK" ON)
set_option(SDL_DISABLE_UNINSTALL  "Disable uninstallation of SDL3" OFF)
cmake_dependent_option(SDL_PRESEED "Preseed CMake cache to speed up configuration" ON "${SDL_PRESEED_AVAILABLE}" OFF)

cmake_dependent_option(SDL_DISABLE_ANDROID_JAR  "Disable creation of SDL3.jar" ${SDL3_SUBPROJECT} "ANDROID" ON)

option_string(SDL_ASSERTIONS "Enable internal sanity checks (auto/disabled/release/enabled/paranoid)" "auto")
set_option(SDL_ASSEMBLY            "Enable assembly routines" ${SDL_ASSEMBLY_DEFAULT})
dep_option(SDL_AVX                 "Use AVX assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_AVX2                "Use AVX2 assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_AVX512F             "Use AVX512F assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_SSE                 "Use SSE assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_SSE2                "Use SSE2 assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_SSE3                "Use SSE3 assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_SSE4_1              "Use SSE4.1 assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_SSE4_2              "Use SSE4.2 assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_MMX                 "Use MMX assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_X86 OR SDL_CPU_X64" OFF)
dep_option(SDL_ALTIVEC             "Use Altivec assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_POWERPC32 OR SDL_CPU_POWERPC64" OFF)
dep_option(SDL_ARMSIMD             "Use SIMD assembly blitters on ARM" OFF "SDL_ASSEMBLY;SDL_CPU_ARM32" OFF)
dep_option(SDL_ARMNEON             "Use NEON assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_ARM32 OR SDL_CPU_ARM64" OFF)
dep_option(SDL_LSX                 "Use LSX assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_LOONGARCH64" OFF)
dep_option(SDL_LASX                "Use LASX assembly routines" ON "SDL_ASSEMBLY;SDL_CPU_LOONGARCH64" OFF)

set_option(SDL_LIBC                "Use the system C library" ${SDL_LIBC_DEFAULT})
set_option(SDL_SYSTEM_ICONV        "Use iconv() from system-installed libraries" ${SDL_SYSTEM_ICONV_DEFAULT})
set_option(SDL_LIBICONV            "Prefer iconv() from libiconv, if available, over libc version" OFF)
set_option(SDL_GCC_ATOMICS         "Use gcc builtin atomics" ${SDL_GCC_ATOMICS_DEFAULT})
dep_option(SDL_DBUS                "Enable D-Bus support" ON "${UNIX_SYS}" OFF)
dep_option(SDL_DISKAUDIO           "Support the disk writer audio driver" ON "SDL_AUDIO" OFF)
dep_option(SDL_DUMMYAUDIO          "Support the dummy audio driver" ON "SDL_AUDIO" OFF)
dep_option(SDL_DUMMYVIDEO          "Use dummy video driver" ON "SDL_VIDEO" OFF)
dep_option(SDL_IBUS                "Enable IBus support" ON "${UNIX_SYS}" OFF)
dep_option(SDL_OPENGL              "Include OpenGL support" ON "SDL_VIDEO;NOT IOS;NOT VISIONOS;NOT TVOS;NOT WATCHOS" OFF)
dep_option(SDL_OPENGLES            "Include OpenGL ES support" ON "SDL_VIDEO;NOT VISIONOS;NOT TVOS;NOT WATCHOS" OFF)
set_option(SDL_PTHREADS            "Use POSIX threads for multi-threading" ${SDL_PTHREADS_DEFAULT})
dep_option(SDL_PTHREADS_SEM        "Use pthread semaphores" ON "SDL_PTHREADS" OFF)
dep_option(SDL_OSS                 "Support the OSS audio API" ${SDL_OSS_DEFAULT} "UNIX_SYS OR RISCOS;SDL_AUDIO" OFF)
dep_option(SDL_ALSA                "Support the ALSA audio API" ${UNIX_SYS} "SDL_AUDIO" OFF)
dep_option(SDL_ALSA_SHARED         "Dynamically load ALSA audio support" ON "SDL_ALSA" OFF)
dep_option(SDL_JACK                "Support the JACK audio API" ${UNIX_SYS} "SDL_AUDIO" OFF)
dep_option(SDL_JACK_SHARED         "Dynamically load JACK audio support" ON "SDL_JACK" OFF)
set_option(SDL_PIPEWIRE            "Use Pipewire audio" ${UNIX_SYS})
dep_option(SDL_PIPEWIRE_SHARED     "Dynamically load Pipewire support" ON "SDL_PIPEWIRE" OFF)
dep_option(SDL_PULSEAUDIO          "Use PulseAudio" ${UNIX_SYS} "SDL_AUDIO" OFF)
dep_option(SDL_PULSEAUDIO_SHARED   "Dynamically load PulseAudio support" ON "SDL_PULSEAUDIO" OFF)
dep_option(SDL_SNDIO               "Support the sndio audio API" ${UNIX_SYS} "SDL_AUDIO" OFF)
dep_option(SDL_SNDIO_SHARED        "Dynamically load the sndio audio API" ON "SDL_SNDIO" OFF)
set_option(SDL_RPATH               "Use an rpath when linking SDL" ${SDL_RPATH_DEFAULT})
set_option(SDL_CLOCK_GETTIME       "Use clock_gettime() instead of gettimeofday()" ${SDL_CLOCK_GETTIME_DEFAULT})
dep_option(SDL_X11                 "Use X11 video driver" ${UNIX_SYS} "SDL_VIDEO" OFF)
dep_option(SDL_X11_SHARED          "Dynamically load X11 support" ON "SDL_X11" OFF)
set(SDL_X11_OPTIONS Xcursor Xdbe XInput Xfixes Xrandr Xscrnsaver XShape)
foreach(_SUB ${SDL_X11_OPTIONS})
  string(TOUPPER "SDL_X11_${_SUB}" _OPT)
  dep_option(${_OPT}               "Enable ${_SUB} support" ON "SDL_X11" OFF)
endforeach()
dep_option(SDL_WAYLAND             "Use Wayland video driver" ${UNIX_SYS} "SDL_VIDEO" OFF)
dep_option(SDL_WAYLAND_SHARED      "Dynamically load Wayland support" ON "SDL_WAYLAND" OFF)
dep_option(SDL_WAYLAND_LIBDECOR    "Use client-side window decorations on Wayland" ON "SDL_WAYLAND" OFF)
dep_option(SDL_WAYLAND_LIBDECOR_SHARED     "Dynamically load libdecor support" ON "SDL_WAYLAND_LIBDECOR;SDL_WAYLAND_SHARED" OFF)
dep_option(SDL_RPI                 "Use Raspberry Pi video driver" ON "SDL_VIDEO;UNIX_SYS;SDL_CPU_ARM32 OR SDL_CPU_ARM64" OFF)
dep_option(SDL_ROCKCHIP            "Use ROCKCHIP Hardware Acceleration video driver" ON "SDL_VIDEO;UNIX_SYS;SDL_CPU_ARM32 OR SDL_CPU_ARM64" OFF)
dep_option(SDL_COCOA               "Use Cocoa video driver" ON "APPLE" OFF)
dep_option(SDL_DIRECTX             "Use DirectX for Windows audio/video" ON "SDL_AUDIO OR SDL_VIDEO;WINDOWS" OFF)
dep_option(SDL_XINPUT              "Use Xinput for Windows" ON "WINDOWS" OFF)
dep_option(SDL_WASAPI              "Use the Windows WASAPI audio driver" ON "WINDOWS;SDL_AUDIO" OFF)
dep_option(SDL_RENDER_D3D          "Enable the Direct3D 9 render driver" ON "SDL_RENDER;SDL_DIRECTX" OFF)
dep_option(SDL_RENDER_D3D11        "Enable the Direct3D 11 render driver" ON "SDL_RENDER;SDL_DIRECTX" OFF)
dep_option(SDL_RENDER_D3D12        "Enable the Direct3D 12 render driver" ON "SDL_RENDER;SDL_DIRECTX" OFF)
dep_option(SDL_RENDER_METAL        "Enable the Metal render driver" ON "SDL_RENDER;${APPLE}" OFF)
dep_option(SDL_RENDER_GPU          "Enable the SDL_GPU render driver" ON "SDL_RENDER;SDL_GPU" OFF)
dep_option(SDL_VIVANTE             "Use Vivante EGL video driver" ON "${UNIX_SYS};SDL_CPU_ARM32" OFF)
dep_option(SDL_VULKAN              "Enable Vulkan support" ON "SDL_VIDEO;ANDROID OR APPLE OR LINUX OR FREEBSD OR WINDOWS" OFF)
dep_option(SDL_RENDER_VULKAN       "Enable the Vulkan render driver" ON "SDL_RENDER;SDL_VULKAN" OFF)
dep_option(SDL_METAL               "Enable Metal support" ON "APPLE" OFF)
dep_option(SDL_KMSDRM              "Use KMS DRM video driver" ${UNIX_SYS} "SDL_VIDEO" OFF)
dep_option(SDL_KMSDRM_SHARED       "Dynamically load KMS DRM support" ON "SDL_KMSDRM" OFF)
set_option(SDL_OFFSCREEN           "Use offscreen video driver" ON)
dep_option(SDL_DUMMYCAMERA         "Support the dummy camera driver" ON SDL_CAMERA OFF)
option_string(SDL_BACKGROUNDING_SIGNAL "number to use for magic backgrounding signal or 'OFF'" OFF)
option_string(SDL_FOREGROUNDING_SIGNAL "number to use for magic foregrounding signal or 'OFF'" OFF)
dep_option(SDL_HIDAPI              "Enable the HIDAPI subsystem" ON "NOT VISIONOS" OFF)
dep_option(SDL_HIDAPI_LIBUSB       "Use libusb for low level joystick drivers" ON SDL_HIDAPI_LIBUSB_AVAILABLE OFF)
dep_option(SDL_HIDAPI_LIBUSB_SHARED "Dynamically load libusb support" ON SDL_HIDAPI_LIBUSB OFF)
dep_option(SDL_HIDAPI_JOYSTICK     "Use HIDAPI for low level joystick drivers" ON SDL_HIDAPI OFF)
dep_option(SDL_VIRTUAL_JOYSTICK    "Enable the virtual-joystick driver" ON SDL_HIDAPI OFF)
set_option(SDL_LIBUDEV             "Enable libudev support" ON)
set_option(SDL_ASAN                "Use AddressSanitizer to detect memory errors" OFF)
set_option(SDL_CCACHE              "Use Ccache to speed up build" OFF)
set_option(SDL_CLANG_TIDY          "Run clang-tidy static analysis" OFF)
set_option(SDL_GPU_DXVK            "Build SDL_GPU with DXVK support" OFF)

set(SDL_VENDOR_INFO "" CACHE STRING "Vendor name and/or version to add to SDL_REVISION")

cmake_dependent_option(SDL_SHARED "Build a shared version of the library" ${SDL_SHARED_DEFAULT} ${SDL_SHARED_AVAILABLE} OFF)
cmake_dependent_option(SDL_STATIC "Build a static version of the library" ${SDL_STATIC_DEFAULT} ${SDL_STATIC_AVAILABLE} OFF)
option(SDL_TEST_LIBRARY   "Build the SDL3_test library" ON)

dep_option(SDL_TESTS           "Build the test directory" OFF SDL_TEST_LIBRARY OFF)
dep_option(SDL_INSTALL_TESTS   "Install test-cases" OFF "NOT SDL_DISABLE_INSTALL;NOT SDL_FRAMEWORK" OFF)
dep_option(SDL_TESTS_LINK_SHARED "link tests to shared SDL library" "${SDL_SHARED}" "SDL_SHARED;SDL_STATIC" "${SDL_SHARED}")
set(SDL_TESTS_TIMEOUT_MULTIPLIER "1" CACHE STRING "Timeout multiplier to account for really slow machines")

set_option(SDL_EXAMPLES           "Build the examples directory")
dep_option(SDL_EXAMPLES_LINK_SHARED "link examples to shared SDL library" "${SDL_SHARED}" "SDL_SHARED;SDL_STATIC" "${SDL_SHARED}")

if(VITA)
  set_option(VIDEO_VITA_PIB  "Build with PSVita piglet gles2 support" OFF)
  set_option(VIDEO_VITA_PVR  "Build with PSVita PVR gles/gles2 support" OFF)
endif()

if(NOT (SDL_SHARED OR SDL_STATIC))
  message(FATAL_ERROR "SDL_SHARED and SDL_STATIC cannot both be disabled")
endif()

if(SDL_PRESEED)
  SDL_Preseed_CMakeCache()
endif()

if(SDL_SHARED)
  add_library(SDL3-shared SHARED)
  add_library(SDL3::SDL3-shared ALIAS SDL3-shared)
  SDL_AddCommonCompilerFlags(SDL3-shared)
  if ("c_std_99" IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(SDL3-shared PRIVATE c_std_99)
  else()
    message(WARNING "target_compile_features does not know c_std_99 for C compiler")
  endif()
endif()

if(SDL_STATIC)
  add_library(SDL3-static STATIC)
  add_library(SDL3::SDL3-static ALIAS SDL3-static)
  SDL_AddCommonCompilerFlags(SDL3-static)
  if ("c_std_99" IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(SDL3-static PRIVATE c_std_99)
  else()
    message(WARNING "target_compile_features does not know c_std_99 for C compiler")
  endif()
endif()

if(SDL_TEST_LIBRARY)
  add_library(SDL3_test STATIC)
  add_library(SDL3::SDL3_test ALIAS SDL3_test)
  SDL_AddCommonCompilerFlags(SDL3_test)
endif()

# Make sure SDL3::SDL3 always exists
if(TARGET SDL3::SDL3-shared)
  add_library(SDL3::SDL3 ALIAS SDL3-shared)
else()
  add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()

sdl_pc_link_options("-lSDL3")

# Enable large file support on 32-bit glibc, so that we can access files
# with large inode numbers
check_symbol_exists("__GLIBC__" "stdlib.h" LIBC_IS_GLIBC)
if (LIBC_IS_GLIBC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
  # Enable large file support on 32-bit glibc, so that we can access files with large inode numbers
  sdl_compile_definitions(PRIVATE "_FILE_OFFSET_BITS=64")
  # Enable 64-bit time_t on 32-bit glibc, so that time stamps remain correct beyond January 2038
  sdl_compile_definitions(PRIVATE "_TIME_BITS=64")
endif()

check_linker_supports_version_file(HAVE_WL_VERSION_SCRIPT)
if(HAVE_WL_VERSION_SCRIPT)
  sdl_shared_link_options("-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/dynapi/SDL_dynapi.sym")
else()
  if((LINUX AND LIBC_IS_GLIBC) OR ANDROID)
    message(FATAL_ERROR "Linker does not support '-Wl,--version-script=xxx.sym'. This is required on the current host platform (${SDL_CMAKE_PLATFORM}).")
  endif()
endif()

if(CYGWIN)
  # We build SDL on cygwin without the UNIX emulation layer
  sdl_include_directories(PUBLIC SYSTEM "/usr/include/mingw")
  cmake_push_check_state()
  string(APPEND CMAKE_REQUIRED_FLAGS " -mno-cygwin")
  check_c_source_compiles("int main(int argc, char **argv) { return 0; }"
    HAVE_GCC_NO_CYGWIN)
  cmake_pop_check_state()
  if(HAVE_GCC_NO_CYGWIN)
    sdl_shared_link_options("-mno-cygwin")
  endif()
endif()

# General includes
sdl_compile_definitions(PRIVATE "USING_GENERATED_CONFIG_H")
sdl_include_directories(
  PRIVATE
    "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/build_config"
    "${SDL3_BINARY_DIR}/include"
    "${SDL3_SOURCE_DIR}/include"
)
# Note: The clang toolset for Visual Studio does not support the '-idirafter' option.
if(USE_GCC OR USE_INTELCC OR (USE_CLANG AND NOT MSVC_CLANG))
  sdl_compile_options(NO_EXPORT PRIVATE "$<BUILD_INTERFACE:-idirafter${SDL3_SOURCE_DIR}/src/video/khronos>")
else()
  sdl_include_directories(NO_EXPORT SYSTEM PRIVATE "$<BUILD_INTERFACE:${SDL3_SOURCE_DIR}/src/video/khronos>")
endif()

if(MSVC AND TARGET SDL3-shared)
  if(SDL_CPU_X64)
    enable_language(ASM_MASM)
    set(asm_src "${SDL3_SOURCE_DIR}/src/stdlib/SDL_mslibc_x64.masm")
    target_compile_options(SDL3-shared PRIVATE "$<$<COMPILE_LANGUAGE:ASM_MASM>:/nologo>")
    set_property(SOURCE "${asm_src}" PROPERTY LANGUAGE "ASM_MASM")
    target_sources(SDL3-shared PRIVATE "${asm_src}")
  elseif(SDL_CPU_ARM32 OR SDL_CPU_ARM64)
    # FIXME: ARM assembler (armasm.exe/armasm64.exe) is NOT ASM_MASM, and does currently not work with CMake
    # (https://gitlab.kitware.com/cmake/cmake/-/issues/18912)
  endif()
endif()

if(USE_INTELCC)
  # warning #39: division by zero
  # warning #239: floating point underflow
  # warning #264: floating-point value does not fit in required floating-point type
  set_property(SOURCE "${SDL3_SOURCE_DIR}/src/libm/e_exp.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -wd239 -wd264")
  set_property(SOURCE "${SDL3_SOURCE_DIR}/src/libm/e_log.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -wd39")
  set_property(SOURCE "${SDL3_SOURCE_DIR}/src/libm/e_log10.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -wd39")
  set_property(SOURCE
    "${SDL3_SOURCE_DIR}/src/libm/e_exp.c"
    "${SDL3_SOURCE_DIR}/src/libm/e_log.c"
    "${SDL3_SOURCE_DIR}/src/libm/e_log10.c"
    PROPERTY SKIP_PRECOMPILE_HEADERS 1)
endif()

set(SDL_DEFAULT_ASSERT_LEVEL_CONFIGURED 1)
if(SDL_ASSERTIONS MATCHES "^(auto|)$")
  # Do nada - use optimization settings to determine the assertion level
  set(SDL_DEFAULT_ASSERT_LEVEL )
  set(SDL_DEFAULT_ASSERT_LEVEL_CONFIGURED 0)
elseif(SDL_ASSERTIONS MATCHES "^(disabled|0)$")
  set(SDL_DEFAULT_ASSERT_LEVEL 0)
elseif(SDL_ASSERTIONS MATCHES "^(release|1)$")
  set(SDL_DEFAULT_ASSERT_LEVEL 1)
elseif(SDL_ASSERTIONS MATCHES "^(enabled|2)$")
  set(SDL_DEFAULT_ASSERT_LEVEL 2)
elseif(SDL_ASSERTIONS MATCHES "^(paranoid|3)$")
  set(SDL_DEFAULT_ASSERT_LEVEL 3)
else()
  message(FATAL_ERROR "unknown assertion level")
endif()
set(HAVE_ASSERTIONS ${SDL_ASSERTIONS})

if(NOT SDL_BACKGROUNDING_SIGNAL STREQUAL "OFF")
  sdl_compile_definitions(PRIVATE "SDL_BACKGROUNDING_SIGNAL=${SDL_BACKGROUNDING_SIGNAL}")
endif()

if(NOT SDL_FOREGROUNDING_SIGNAL STREQUAL "OFF")
  sdl_compile_definitions(PRIVATE "SDL_FOREGROUNDING_SIGNAL=${SDL_FOREGROUNDING_SIGNAL}")
endif()

# Compiler option evaluation
if(USE_GCC OR USE_CLANG OR USE_INTELCC OR USE_QCC)
  if(SDL_GCC_ATOMICS)
    check_c_source_compiles("int main(int argc, char **argv) {
        int a;
        void *x, *y, *z;
        __sync_lock_test_and_set(&a, 4);
        __sync_lock_test_and_set(&x, y);
        __sync_fetch_and_add(&a, 1);
        __sync_bool_compare_and_swap(&a, 5, 10);
        __sync_bool_compare_and_swap(&x, y, z);
        return 0; }" COMPILER_SUPPORTS_GCC_ATOMICS)
    set(HAVE_GCC_ATOMICS ${COMPILER_SUPPORTS_GCC_ATOMICS})
    if(NOT HAVE_GCC_ATOMICS)
      check_c_source_compiles("int main(int argc, char **argv) {
          int a;
          __sync_lock_test_and_set(&a, 1);
          __sync_lock_release(&a);
          return 0; }" COMPILER_SUPPORTS_SYNC_LOCK_TEST_AND_SET)
      set(HAVE_GCC_SYNC_LOCK_TEST_AND_SET ${COMPILER_SUPPORTS_SYNC_LOCK_TEST_AND_SET})
    endif()
  endif()

  cmake_push_check_state()
  string(APPEND CMAKE_REQUIRED_FLAGS " -fvisibility=hidden -Werror")
  check_c_source_compiles("
      #if !defined(__GNUC__) || __GNUC__ < 4
      #error SDL only uses visibility attributes in GCC 4 or newer
      #endif
      __attribute__((visibility(\"default\"))) int foo(void);
      __attribute__((visibility(\"hidden\")))  int bar(void);
      int foo(void) { return 0; }
      int bar(void) { return 1; }
      int main(void) { return 0; }" HAVE_GCC_FVISIBILITY)
  cmake_pop_check_state()

  if(APPLE)
    check_c_compiler_flag(-Wno-error=deprecated-declarations COMPILER_SUPPORTS_WNO_ERROR_DEPRECATED_DECLARATIONS)
    if(COMPILER_SUPPORTS_WNO_ERROR_DEPRECATED_DECLARATIONS)
      sdl_compile_options(PRIVATE "-Wno-error=deprecated-declarations")
    endif()
  endif()

  if(APPLE)
    check_linker_flag(C "-Wl,-undefined,error" LINKER_SUPPORTS_WL_UNDEFINED_ERROR)
    if(LINKER_SUPPORTS_WL_UNDEFINED_ERROR)
      sdl_shared_link_options("-Wl,-undefined,error")
    endif()
  elseif(NOT OPENBSD)
    cmake_push_check_state()
    check_linker_flag(C "-Wl,--no-undefined" LINKER_SUPPORTS_WL_NO_UNDEFINED)
    #FIXME: originally this if had an additional "AND NOT (USE_CLANG AND WINDOWS)"
    if(LINKER_SUPPORTS_WL_NO_UNDEFINED)
      sdl_shared_link_options("-Wl,--no-undefined")
    endif()
  endif()
endif()

if(MSVC)
  sdl_compile_definitions(
    PRIVATE
      "_CRT_SECURE_NO_DEPRECATE"
      "_CRT_NONSTDC_NO_DEPRECATE"
      "_CRT_SECURE_NO_WARNINGS"
  )

  # CET support was added in VS 2019 16.7
  if(MSVC_VERSION GREATER 1926 AND CMAKE_GENERATOR_PLATFORM MATCHES "Win32|x64")
    # Mark SDL3.dll as compatible with Control-flow Enforcement Technology (CET)
    sdl_shared_link_options("-CETCOMPAT")
  endif()
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  # Due to a limitation of Microsoft's LTO implementation, LTO must be disabled for memcpy and memset.
  # The same applies to various functions normally belonging in the C library (for x86 architecture).
  set_property(SOURCE "${SDL3_SOURCE_DIR}/src/stdlib/SDL_mslibc.c" APPEND_STRING PROPERTY COMPILE_FLAGS " /GL-")
endif()

if(SDL_ASSEMBLY)
  set(HAVE_ASSEMBLY TRUE)

  if(SDL_MMX)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -mmmx")
    endif()
    check_c_source_compiles("
      #include <mmintrin.h>
      void ints_add(int *dest, int *a, int *b, unsigned size) {
        for (; size >= 2; size -= 2, dest += 2, a += 2, b += 2) {
          *(__m64*)dest = _mm_add_pi32(*(__m64*)a, *(__m64*)b);
        }
      }
      int main(int argc, char *argv[]) {
        ints_add((int*)0, (int*)0, (int*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_MMX)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_MMX)
      set(HAVE_MMX TRUE)
    endif()
  endif()
  if(SDL_SSE)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -msse")
    endif()
    check_c_source_compiles("
      #include <xmmintrin.h>
      void floats_add(float *dest, float *a, float *b, unsigned size) {
        for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
          _mm_storeu_ps(dest, _mm_add_ps(_mm_loadu_ps(a), _mm_loadu_ps (b)));
        }
      }
      int main(int argc, char **argv) {
        floats_add((float*)0, (float*)0, (float*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_SSE)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_SSE)
      set(HAVE_SSE TRUE)
    endif()
  endif()
  if(SDL_SSE2)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -msse2")
    endif()
    check_c_source_compiles("
      #include <emmintrin.h>
      void doubles_add(double *dest, double *a, double *b, unsigned size) {
        for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
          _mm_store_pd(dest, _mm_add_pd(_mm_loadu_pd(a), _mm_loadu_pd(b)));
        }
      }
      int main(int argc, char **argv) {
        doubles_add((double*)0, (double*)0, (double*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_SSE2)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_SSE2)
      set(HAVE_SSE2 TRUE)
    endif()
  endif()
  if(SDL_SSE3)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -msse3")
    endif()
    check_c_source_compiles("
      #include <pmmintrin.h>
      void ints_add(int *dest, int *a, int *b, unsigned size) {
        for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
          _mm_storeu_si128((__m128i*)dest, _mm_add_epi32(_mm_lddqu_si128((__m128i*)a), _mm_lddqu_si128((__m128i*)b)));
        }
      }
      int main(int argc, char **argv) {
        ints_add((int*)0, (int*)0, (int*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_SSE3)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_SSE3)
      set(HAVE_SSE3 TRUE)
    endif()
  endif()
  if(SDL_SSE4_1)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -msse4.1")
    endif()
    check_c_source_compiles("
      #include <smmintrin.h>
      void ints_mul(int *dest, int *a, int *b, unsigned size) {
        for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
          _mm_storeu_si128((__m128i*)dest, _mm_mullo_epi32(_mm_lddqu_si128((__m128i*)a), _mm_lddqu_si128((__m128i*)b)));
        }
      }
      int main(int argc, char **argv) {
        ints_mul((int*)0, (int*)0, (int*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_SSE4_1)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_SSE4_1)
      set(HAVE_SSE4_1 TRUE)
    endif()
  endif()
  if(SDL_SSE4_2)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -msse4.2")
    endif()
    check_c_source_compiles("
      #include <nmmintrin.h>
      unsigned calc_crc32c(const char *text, unsigned len) {
          unsigned crc32c = ~0;
          for (; len >= 4; len -= 4, text += 4) {
            crc32c = (unsigned)_mm_crc32_u32(crc32c, *(unsigned*)text);
          }
          return crc32c;
      }
      int main(int argc, char **argv) {
        calc_crc32c(\"SDL_SSE4\",8);
        return 0;
      }" COMPILER_SUPPORTS_SSE4_2)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_SSE4_2)
      set(HAVE_SSE4_2 TRUE)
    endif()
  endif()
  if(SDL_AVX)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -mavx")
    endif()
    check_c_source_compiles("
      #include <immintrin.h>
      void floats_add(float *dest, float *a, float *b, unsigned size) {
        for (; size >= 8; size -= 8, dest += 8, a += 8, b += 8) {
          _mm256_storeu_ps(dest, _mm256_add_ps(_mm256_loadu_ps(a), _mm256_loadu_ps(b)));
        }
      }
      int main(int argc, char **argv) {
        floats_add((float*)0, (float*)0, (float*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_AVX)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_AVX)
      set(HAVE_AVX TRUE)
    endif()
  endif()
  if(SDL_AVX2)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -mavx2")
    endif()
    check_c_source_compiles("
      #include <immintrin.h>
      void ints_add(int *dest, int *a, int *b, unsigned size) {
        for (; size >= 8; size -= 8, dest += 8, a += 8, b += 8) {
          _mm256_storeu_si256((__m256i*)dest, _mm256_add_epi32(_mm256_loadu_si256((__m256i*)a), _mm256_loadu_si256((__m256i*)b)));
        }
      }
      int main(int argc, char **argv) {
        ints_add((int*)0, (int*)0, (int*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_AVX2)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_AVX2)
      set(HAVE_AVX2 TRUE)
    endif()
  endif()
  if(SDL_AVX512F)
    cmake_push_check_state()
    if(USE_GCC OR USE_CLANG OR USE_INTELCC)
      string(APPEND CMAKE_REQUIRED_FLAGS " -mavx512f")
    endif()
    check_c_source_compiles("
      #include <immintrin.h>
      void floats_add(float *dest, float *a, float *b, unsigned size) {
        for (; size >= 16; size -= 16, dest += 16, a += 16, b += 16) {
          _mm512_storeu_ps(dest, _mm512_add_ps(_mm512_loadu_ps(a), _mm512_loadu_ps(b)));
        }
      }
      int main(int argc, char **argv) {
        floats_add((float*)0, (float*)0, (float*)0, 0);
        return 0;
      }" COMPILER_SUPPORTS_AVX512F)
    cmake_pop_check_state()
    if(COMPILER_SUPPORTS_AVX512F)
      set(HAVE_AVX512F TRUE)
    endif()
  endif()

  if(SDL_ARMNEON)
    check_c_source_compiles("
        #include <arm_neon.h>
        void floats_add(float *dest, float *a, float *b, unsigned size) {
          for (; size >= 4; size -= 4, dest += 4, a += 4, b += 4) {
            vst1q_f32(dest, vaddq_f32(vld1q_f32(a), vld1q_f32(b)));
          }
        }
        int main(int argc, char *argv[]) {
          floats_add((float*)0, (float*)0, (float*)0, 0);
          return 0;
         }" COMPILER_SUPPORTS_ARMNEON)

    if(COMPILER_SUPPORTS_ARMNEON)
      set(HAVE_ARMNEON TRUE)
    endif()
  endif()

  if(USE_GCC OR USE_CLANG)
    # TODO: Those all seem to be quite GCC specific - needs to be
    # reworked for better compiler support

    if(SDL_ALTIVEC)
      cmake_push_check_state()
      string(APPEND CMAKE_REQUIRED_FLAGS " -maltivec")
      check_c_source_compiles("
          #include <altivec.h>
          vector unsigned int vzero() {
              return vec_splat_u32(0);
          }
          int main(int argc, char **argv) { return 0; }" COMPILER_SUPPORTS_ALTIVEC)
      cmake_pop_check_state()
      if(COMPILER_SUPPORTS_ALTIVEC)
        set(HAVE_ALTIVEC TRUE)
        set(SDL_ALTIVEC_BLITTERS 1)
        sdl_compile_options(PRIVATE "-maltivec")
        set_property(SOURCE "${SDL3_SOURCE_DIR}/src/video/SDL_blit_N.c" APPEND PROPERTY COMPILE_DEFINITIONS "SDL_ENABLE_ALTIVEC")
        set_property(SOURCE "${SDL3_SOURCE_DIR}/src/video/SDL_blit_N.c" PROPERTY SKIP_PRECOMPILE_HEADERS 1)
      endif()
    endif()

    if(SDL_LSX)
      cmake_push_check_state()
      string(APPEND CMAKE_REQUIRED_FLAGS " -mlsx")
      check_c_source_compiles("
          #ifndef __loongarch_sx
          #error Assembler CPP flag not enabled
          #endif
          int main(int argc, char **argv) { return 0; }" COMPILER_SUPPORTS_LSX)
      check_include_file("lsxintrin.h" HAVE_LSXINTRIN_H)
      cmake_pop_check_state()

      if(COMPILER_SUPPORTS_LSX AND HAVE_LSXINTRIN_H)
        set_property(SOURCE "${SDL3_SOURCE_DIR}/src/video/yuv2rgb/yuv_rgb_lsx.c" APPEND PROPERTY COMPILE_OPTIONS "-mlsx")
        set(HAVE_LSX TRUE)
      endif()
    endif()

    if(SDL_LASX)
      cmake_push_check_state()
      string(APPEND CMAKE_REQUIRED_FLAGS " -mlasx")
      check_c_source_compiles("
          #ifndef __loongarch_asx
          #error Assembler CPP flag not enabled
          #endif
          int main(int argc, char **argv) { return 0; }" COMPILER_SUPPORTS_LASX)
      check_include_file("lasxintrin.h" HAVE_LASXINTRIN_H)
      cmake_pop_check_state()
      if(COMPILER_SUPPORTS_LASX AND HAVE_LASXINTRIN_H)
        set(HAVE_LASX TRUE)
      endif()
    endif()
  endif()
endif()

if(NOT HAVE_MMX)
  set(SDL_DISABLE_MMX 1)
endif()

if(NOT HAVE_SSE)
  set(SDL_DISABLE_SSE 1)
endif()

if(NOT HAVE_SSE2)
  set(SDL_DISABLE_SSE2 1)
endif()

if(NOT HAVE_SSE3)
  set(SDL_DISABLE_SSE3 1)
endif()

if(NOT HAVE_SSE4_1)
  set(SDL_DISABLE_SSE4_1 1)
endif()

if(NOT HAVE_SSE4_2)
  set(SDL_DISABLE_SSE4_2 1)
endif()

if(NOT HAVE_AVX)
  set(SDL_DISABLE_AVX 1)
endif()

if(NOT HAVE_AVX2)
  set(SDL_DISABLE_AVX2 1)
endif()

if(NOT HAVE_AVX512F)
  set(SDL_DISABLE_AVX512F 1)
endif()

if(NOT HAVE_LSX)
  set(SDL_DISABLE_LSX 1)
endif()

if(NOT HAVE_LASX)
  set(SDL_DISABLE_LASX 1)
endif()

if(NOT HAVE_ARMNEON)
  set(SDL_DISABLE_NEON 1)
endif()

set(SDL_DISABLE_ALLOCA 0)
check_include_file("alloca.h" "HAVE_ALLOCA_H")
if(MSVC)
  check_include_file("malloc.h" "HAVE_MALLOC")
  check_symbol_exists("_alloca" "malloc.h" _ALLOCA_IN_MALLOC_H)
  if(NOT HAVE_ALLOCA_H AND NOT _ALLOCA_IN_MALLOC_H)
    set(SDL_DISABLE_ALLOCA 1)
  endif()
endif()

# TODO: Can't deactivate on FreeBSD? w/o LIBC, SDL_stdinc.h can't define anything.
if(SDL_LIBC)
  set(available_headers)
  set(HAVE_LIBC TRUE)
  set(headers_to_check
    float.h
    iconv.h
    inttypes.h
    limits.h
    malloc.h
    math.h
    memory.h
    signal.h
    stdarg.h
    stdbool.h
    stddef.h
    stdint.h
    stdio.h
    stdlib.h
    string.h
    strings.h
    sys/types.h
    time.h
    wchar.h
  )
  foreach(_HEADER IN LISTS headers_to_check)
    string(TOUPPER "${_HEADER}" HEADER_IDENTIFIER)
    string(REGEX REPLACE "[./]" "_" HEADER_IDENTIFIER "${HEADER_IDENTIFIER}")
    set(LIBC_HAS_VAR "LIBC_HAS_${HEADER_IDENTIFIER}")
    check_include_file("${_HEADER}" "${LIBC_HAS_VAR}")
    set(HAVE_${HEADER_IDENTIFIER} ${${LIBC_HAS_VAR}})
    if(HAVE_${HEADER_IDENTIFIER})
      list(APPEND available_headers "${_HEADER}")
    endif()
  endforeach()

  set(symbols_to_check
    abs acos acosf asin asinf atan atan2 atan2f atanf atof atoi
    bcopy
    calloc ceil ceilf copysign copysignf cos cosf
    _Exit exp expf
    fabs fabsf floor floorf fmod fmodf fopen64 free fseeko fseeko64
    getenv
    _i64toa index itoa
    log log10 log10f logf lround lroundf _ltoa
    malloc memcmp memcpy memmove memset modf modff
    pow powf putenv
    realloc rindex round roundf
    scalbn scalbnf setenv sin sinf sqr sqrt sqrtf sscanf strchr
    strcmp strlcat strlcpy strlen strncmp strnlen strpbrk
    strrchr strstr strnstr strtod strtok_r strtol strtoll strtoul strtoull
    tan tanf trunc truncf
    unsetenv
    vsnprintf vsscanf
    wcsnlen wcscmp wcsdup wcslcat wcslcpy wcslen wcsncmp wcsstr wcstol
  )
  if(WINDOWS)
    list(APPEND symbols_to_check
      _strrev _ui64toa _uitoa _ultoa _wcsdup
    )
  else()
    list(APPEND symbols_to_check
        strcasestr
    )
  endif()
  check_library_exists(m pow "" HAVE_LIBM)
  cmake_push_check_state()
  if(HAVE_LIBM)
    sdl_link_dependency(math LIBS m)
    list(APPEND CMAKE_REQUIRED_LIBRARIES m)
  endif()
  foreach(_FN IN LISTS symbols_to_check)
    string(TOUPPER ${_FN} _UPPER)
    set(LIBC_HAS_VAR "LIBC_HAS_${_UPPER}")
    check_symbol_exists("${_FN}" "${available_headers}" ${LIBC_HAS_VAR})
    set(HAVE_${_UPPER} ${${LIBC_HAS_VAR}})
  endforeach()
  cmake_pop_check_state()

  cmake_push_check_state()
  if(MSVC)
    string(APPEND CMAKE_REQUIRED_FLAGS " -we4244 -WX")  # 'conversion' conversion from 'type1' to 'type2', possible loss of data
  else()
    string(APPEND CMAKE_REQUIRED_FLAGS " -Wfloat-conversion -Werror")
  endif()
  foreach(math_fn isinf isnan)
    string(TOUPPER "${math_fn}" MATH_FN)
    check_c_source_compiles("
        #include <math.h>
        int main() {
          double d = 3.14159;
          return ${math_fn}(d);
        }
      " LIBC_HAS_${MATH_FN})
    set(HAVE_${MATH_FN} ${LIBC_HAS_${MATH_FN}})

    check_c_source_compiles("
      #include <math.h>
      int main() {
        float f = 3.14159f;
        return ${math_fn}(f);
      }
    " LIBC_${MATH_FN}_HANDLES_FLOAT)
    set(HAVE_${MATH_FN}_FLOAT_MACRO ${LIBC_${MATH_FN}_HANDLES_FLOAT})

    check_c_source_compiles("
      #include <math.h>
      int main() {
        float f = 3.14159f;
        return ${math_fn}f(f);
      }
    " LIBC_HAS_${MATH_FN}F)
    set(HAVE_${MATH_FN}F "${LIBC_HAS_${MATH_FN}F}")
  endforeach()
  cmake_pop_check_state()

  if(NOT WINDOWS)
    check_symbol_exists(getpagesize "unistd.h" HAVE_GETPAGESIZE)
    check_symbol_exists(sigaction "signal.h" HAVE_SIGACTION)
    check_symbol_exists(setjmp "setjmp.h" HAVE_SETJMP)
    check_symbol_exists(nanosleep "time.h" HAVE_NANOSLEEP)
    check_symbol_exists(gmtime_r "time.h" HAVE_GMTIME_R)
    check_symbol_exists(localtime_r "time.h" HAVE_LOCALTIME_R)
    check_symbol_exists(nl_langinfo "langinfo.h" HAVE_NL_LANGINFO)
    check_symbol_exists(sysconf "unistd.h" HAVE_SYSCONF)
    check_symbol_exists(sysctlbyname "sys/types.h;sys/sysctl.h" HAVE_SYSCTLBYNAME)
    check_symbol_exists(getauxval "sys/auxv.h" HAVE_GETAUXVAL)
    check_symbol_exists(elf_aux_info "sys/auxv.h" HAVE_ELF_AUX_INFO)
    check_symbol_exists(poll "poll.h" HAVE_POLL)
    check_symbol_exists(memfd_create "sys/mman.h" HAVE_MEMFD_CREATE)
    check_symbol_exists(posix_fallocate "fcntl.h" HAVE_POSIX_FALLOCATE)

    if(SDL_SYSTEM_ICONV)
      check_c_source_compiles("
        #define LIBICONV_PLUG 1 /* in case libiconv header is in include path */
        #include <stddef.h>
        #include <iconv.h>
        int main(int argc, char **argv) {
            return !iconv_open(NULL,NULL);
        }" ICONV_IN_LIBC)

      cmake_push_check_state()
      list(APPEND CMAKE_REQUIRED_LIBRARIES iconv)
      check_c_source_compiles("
        #include <stddef.h>
        #include <iconv.h>
        int main(int argc, char **argv) {
            return !iconv_open(NULL,NULL);
        }" ICONV_IN_LIBICONV)
      cmake_pop_check_state()

      if(ICONV_IN_LIBC OR ICONV_IN_LIBICONV)
        set(HAVE_ICONV 1)
        set(HAVE_SYSTEM_ICONV TRUE)
        if(ICONV_IN_LIBICONV AND (SDL_LIBICONV OR (NOT ICONV_IN_LIBC)))
          sdl_link_dependency(iconv LIBS iconv)
          set(SDL_USE_LIBICONV 1)
          set(HAVE_LIBICONV TRUE)
        endif()
      endif()
    endif()

    check_struct_has_member("struct sigaction" "sa_sigaction" "signal.h" HAVE_SA_SIGACTION)
    check_struct_has_member("struct stat" "st_mtim" "sys/stat.h" HAVE_ST_MTIM)
  endif()
else()
  set(headers
    stdarg.h
    stddef.h
    stdint.h
  )
  foreach(_HEADER ${headers})
    string(TOUPPER "${_HEADER}" HEADER_IDENTIFIER)
    string(REGEX REPLACE "[./]" "_" HEADER_IDENTIFIER "${HEADER_IDENTIFIER}")
    set(LIBC_HAS_VAR "LIBC_HAS_${HEADER_IDENTIFIER}")
    check_include_file("${_HEADER}" "${LIBC_HAS_VAR}")
    set(HAVE_${HEADER_IDENTIFIER} ${${LIBC_HAS_VAR}})
  endforeach()

  if(MSVC AND USE_CLANG)
    check_c_compiler_flag("/Q_no-use-libirc" HAS_Q_NO_USE_LIBIRC)
  endif()
endif()

# General source files
sdl_glob_sources(
  "${SDL3_SOURCE_DIR}/src/*.c"
  "${SDL3_SOURCE_DIR}/src/atomic/*.c"
  "${SDL3_SOURCE_DIR}/src/audio/*.c"
  "${SDL3_SOURCE_DIR}/src/camera/*.c"
  "${SDL3_SOURCE_DIR}/src/core/*.c"
  "${SDL3_SOURCE_DIR}/src/cpuinfo/*.c"
  "${SDL3_SOURCE_DIR}/src/dynapi/*.c"
  "${SDL3_SOURCE_DIR}/src/events/*.c"
  "${SDL3_SOURCE_DIR}/src/file/*.c"
  "${SDL3_SOURCE_DIR}/src/filesystem/*.c"
  "${SDL3_SOURCE_DIR}/src/gpu/*.c"
  "${SDL3_SOURCE_DIR}/src/joystick/*.c"
  "${SDL3_SOURCE_DIR}/src/haptic/*.c"
  "${SDL3_SOURCE_DIR}/src/hidapi/*.c"
  "${SDL3_SOURCE_DIR}/src/locale/*.c"
  "${SDL3_SOURCE_DIR}/src/main/*.c"
  "${SDL3_SOURCE_DIR}/src/misc/*.c"
  "${SDL3_SOURCE_DIR}/src/power/*.c"
  "${SDL3_SOURCE_DIR}/src/render/*.c"
  "${SDL3_SOURCE_DIR}/src/render/*/*.c"
  "${SDL3_SOURCE_DIR}/src/sensor/*.c"
  "${SDL3_SOURCE_DIR}/src/stdlib/*.c"
  "${SDL3_SOURCE_DIR}/src/storage/*.c"
  "${SDL3_SOURCE_DIR}/src/thread/*.c"
  "${SDL3_SOURCE_DIR}/src/time/*.c"
  "${SDL3_SOURCE_DIR}/src/timer/*.c"
  "${SDL3_SOURCE_DIR}/src/video/*.c"
  "${SDL3_SOURCE_DIR}/src/video/yuv2rgb/*.c"
)

# Build uclibc as a static library such that non-used symbols don't end up in the SDL3 shared library.
file(GLOB SDL_UCLIBC_SOURCES "${SDL3_SOURCE_DIR}/src/libm/*.c")
add_library(SDL_uclibc STATIC "${SDL_UCLIBC_SOURCES}")
target_compile_definitions(SDL_uclibc PRIVATE USING_GENERATED_CONFIG_H)
target_include_directories(SDL_uclibc PRIVATE "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/build_config")
target_include_directories(SDL_uclibc PRIVATE "${SDL3_SOURCE_DIR}/src")
target_include_directories(SDL_uclibc PRIVATE "${SDL3_SOURCE_DIR}/include")
SDL_AddCommonCompilerFlags(SDL_uclibc)
sdl_sources(STATIC "$<TARGET_OBJECTS:SDL_uclibc>")
if(TARGET SDL3-shared)
  target_link_libraries(SDL3-shared PRIVATE SDL_uclibc)
endif()
if(HAVE_GCC_FVISIBILITY)
  set_property(TARGET SDL_uclibc PROPERTY C_VISIBILITY_PRESET "hidden")
endif()

# Enable/disable various subsystems of the SDL library
foreach(_SUB ${SDL_SUBSYSTEMS})
  string(TOUPPER ${_SUB} _OPT)
  if(NOT SDL_${_OPT})
    set(SDL_${_OPT}_DISABLED 1)
  endif()
endforeach()
if(SDL_HAPTIC)
  if(NOT SDL_JOYSTICK)
    # Haptic requires some private functions from the joystick subsystem.
    message(FATAL_ERROR "SDL_HAPTIC requires SDL_JOYSTICK, which is not enabled")
  endif()
endif()


# General SDL subsystem options, valid for all platforms
if(SDL_AUDIO)
  # CheckDummyAudio/CheckDiskAudio - valid for all platforms
  if(SDL_DUMMYAUDIO)
    set(SDL_AUDIO_DRIVER_DUMMY 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/dummy/*.c")
    set(HAVE_DUMMYAUDIO TRUE)
    set(HAVE_SDL_AUDIO TRUE)
  endif()
  if(SDL_DISKAUDIO)
    set(SDL_AUDIO_DRIVER_DISK 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/disk/*.c")
    set(HAVE_DISKAUDIO TRUE)
    set(HAVE_SDL_AUDIO TRUE)
  endif()
endif()

if(SDL_CAMERA)
  # CheckDummyCamera/CheckDiskCamera - valid for all platforms
  if(SDL_DUMMYCAMERA)
    set(SDL_CAMERA_DRIVER_DUMMY 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/dummy/*.c")
    set(HAVE_DUMMYCAMERA TRUE)
    set(HAVE_SDL_CAMERA TRUE)
  endif()
  # !!! FIXME: for later.
  #if(SDL_DISKCAMERA)
  #  set(SDL_CAMERA_DRIVER_DISK 1)
  #  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/disk/*.c")
  #  set(HAVE_DISKCAMERA TRUE)
  #  set(HAVE_SDL_CAMERA TRUE)
  #endif()
endif()

if(UNIX OR APPLE)
  # Relevant for Unix/Darwin only
  set(DYNAPI_NEEDS_DLOPEN 1)
  CheckDLOPEN()
  if(HAVE_DLOPEN)
    set(SDL_LOADSO_DLOPEN 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/loadso/dlopen/*.c")
    set(HAVE_SDL_LOADSO TRUE)
  endif()
endif()

if(UNIX OR APPLE OR HAIKU OR RISCOS)
  CheckO_CLOEXEC()
endif()

if(SDL_JOYSTICK)
  if(SDL_VIRTUAL_JOYSTICK)
    set(HAVE_VIRTUAL_JOYSTICK TRUE)
    set(SDL_JOYSTICK_VIRTUAL 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/virtual/*.c")
  endif()
endif()

if(SDL_VIDEO)
  if(SDL_DUMMYVIDEO)
    set(SDL_VIDEO_DRIVER_DUMMY 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/dummy/*.c")
    set(HAVE_DUMMYVIDEO TRUE)
    set(HAVE_SDL_VIDEO TRUE)
  endif()
endif()

# Platform-specific options and settings
if(ANDROID)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/android")

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/core/android/*.c")
  sdl_sources("${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c")
  set_property(SOURCE "${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-declaration-after-statement")

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/android/*.c")
  set(HAVE_SDL_MISC TRUE)

  # SDL_spinlock.c Needs to be compiled in ARM mode.
  # There seems to be no better way currently to set the ARM mode.
  # see: https://issuetracker.google.com/issues/62264618
  # Another option would be to set ARM mode to all compiled files
  cmake_push_check_state()
  string(APPEND CMAKE_REQUIRED_FLAGS " -Werror=unused-command-line-argument")
  check_c_compiler_flag(-marm HAVE_ARM_MODE)
  cmake_pop_check_state()
  if(HAVE_ARM_MODE)
    set_property(SOURCE "${SDL3_SOURCE_DIR}/src/atomic/SDL_spinlock.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -marm")
    set_source_files_properties(src/atomic/SDL_spinlock.c PROPERTIES SKIP_PRECOMPILE_HEADERS 1)
  endif()

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_OPENSLES 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/openslES/*.c")

    sdl_link_dependency(opensles LIBS ${ANDROID_DL_LIBRARY} OpenSLES)

    set(SDL_AUDIO_DRIVER_AAUDIO 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/aaudio/*.c")

    set(HAVE_SDL_AUDIO TRUE)
  endif()

  set(SDL_FILESYSTEM_ANDROID 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/android/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  set(SDL_FSOPS_POSIX 1)  # !!! FIXME: this might need something else for .apk data?
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  if(SDL_HAPTIC)
    set(SDL_HAPTIC_ANDROID 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/haptic/android/*.c")
    set(HAVE_SDL_HAPTIC TRUE)
  endif()

  if(SDL_HIDAPI)
    CheckHIDAPI()
  endif()

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_ANDROID 1)
    sdl_glob_sources(
      "${SDL3_SOURCE_DIR}/src/joystick/android/*.c"
      "${SDL3_SOURCE_DIR}/src/joystick/steam/*.c"
    )
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  set(SDL_LOADSO_DLOPEN 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/loadso/dlopen/*.c")
  set(HAVE_SDL_LOADSO TRUE)

  if(SDL_POWER)
    set(SDL_POWER_ANDROID 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/android/*.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/android/*.c")
  set(HAVE_SDL_LOCALE TRUE)

  set(SDL_TIME_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/unix/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_SENSOR)
    set(SDL_SENSOR_ANDROID 1)
    set(HAVE_SDL_SENSORS TRUE)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/sensor/android/*.c")
  endif()

  if(SDL_CAMERA)
    set(SDL_CAMERA_DRIVER_ANDROID 1)
    set(HAVE_CAMERA TRUE)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/android/*.c")
  endif()

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_ANDROID 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/android/*.c")
    set(HAVE_SDL_VIDEO TRUE)

    # Core stuff
    # find_library(ANDROID_DL_LIBRARY dl)
    # FIXME failing dlopen https://github.com/android-ndk/ndk/issues/929
    sdl_link_dependency(android_video LIBS dl log android)
    sdl_compile_definitions(PRIVATE "GL_GLEXT_PROTOTYPES")

    #enable gles
    if(SDL_OPENGLES)
      set(SDL_VIDEO_OPENGL_EGL 1)
      set(HAVE_OPENGLES TRUE)
      set(SDL_VIDEO_OPENGL_ES 1)
      set(SDL_VIDEO_OPENGL_ES2 1)
      set(SDL_VIDEO_RENDER_OGL_ES2 1)

      sdl_link_dependency(opengles LIBS GLESv1_CM GLESv2)
    endif()

    if(SDL_VULKAN)
      check_c_source_compiles("
      #if defined(__ARM_ARCH) && __ARM_ARCH < 7
      #error Vulkan doesn't work on this configuration
      #endif
      int main(int argc, char **argv) { return 0; }
      " VULKAN_PASSED_ANDROID_CHECKS)
      if(VULKAN_PASSED_ANDROID_CHECKS)
        set(SDL_VIDEO_VULKAN 1)
        set(HAVE_VULKAN TRUE)
        if(SDL_RENDER_VULKAN)
          set(SDL_VIDEO_RENDER_VULKAN 1)
          set(HAVE_RENDER_VULKAN TRUE)
        endif()
      endif()
    endif()
  endif()

  CheckPTHREAD()
  if(SDL_CLOCK_GETTIME)
    set(HAVE_CLOCK_GETTIME 1)
  endif()

  if(NOT SDL_DISABLE_ANDROID_JAR)
    find_package(Java)
    find_package(SdlAndroidPlatform MODULE)

    if(Java_FOUND AND SdlAndroidPlatform_FOUND AND NOT CMAKE_VERSION VERSION_LESS "3.19")
      include(UseJava)
      set(path_android_jar "${SDL_ANDROID_PLATFORM_ROOT}/android.jar")
      set(android_java_sources_root "${SDL3_SOURCE_DIR}/android-project/app/src/main/java")
      file(GLOB SDL_JAVA_SOURCES "${android_java_sources_root}/org/libsdl/app/*.java")
      set(CMAKE_JAVA_COMPILE_FLAGS "-encoding;utf-8")
      add_jar(SDL3-jar
        SOURCES ${SDL_JAVA_SOURCES}
        INCLUDE_JARS "${path_android_jar}"
        OUTPUT_NAME "SDL3"
        VERSION "${SDL3_VERSION}"
      )
      set_property(TARGET SDL3-jar PROPERTY OUTPUT "${SDL3_BINARY_DIR}/SDL3-${SDL3_VERSION}.jar")
      add_library(SDL3__Jar INTERFACE)
      add_library(SDL3::Jar ALIAS SDL3__Jar)
      get_property(sdl3_jar_location TARGET SDL3-jar PROPERTY JAR_FILE)
      set_property(TARGET SDL3__Jar PROPERTY JAR_FILE "${sdl3_jar_location}")
      set(javasourcesjar "${SDL3_BINARY_DIR}/SDL3-${SDL3_VERSION}-sources.jar")
      string(REGEX REPLACE "${android_java_sources_root}/" "" sdl_relative_java_sources "${SDL_JAVA_SOURCES}")
      add_custom_command(
        OUTPUT "${javasourcesjar}"
        COMMAND ${Java_JAR_EXECUTABLE} cf "${javasourcesjar}" ${sdl_relative_java_sources}
        WORKING_DIRECTORY "${android_java_sources_root}"
        DEPENDS ${SDL_JAVA_SOURCES}
      )
      add_custom_target(SDL3-javasources ALL DEPENDS "${javasourcesjar}")
      if(NOT SDL_DISABLE_INSTALL_DOCS)
        set(javadocdir "${SDL3_BINARY_DIR}/docs/javadoc")
        set(javadocjar "${SDL3_BINARY_DIR}/SDL3-${SDL3_VERSION}-javadoc.jar")
        set(javadoc_index_html "${javadocdir}/index.html")
        add_custom_command(
          OUTPUT "${javadoc_index_html}"
          COMMAND ${CMAKE_COMMAND} -E rm -rf "${javadocdir}"
          COMMAND ${Java_JAVADOC_EXECUTABLE} -encoding utf8 -d "${javadocdir}"
            -classpath "${path_android_jar}"
            -author -use -version ${SDL_JAVA_SOURCES}
          DEPENDS ${SDL_JAVA_SOURCES} "${path_android_jar}"
        )
        add_custom_target(SDL3-javadoc ALL DEPENDS "${javadoc_index_html}")
        set_property(TARGET SDL3-javadoc PROPERTY OUTPUT_DIR "${javadocdir}")
      endif()
    endif()
  endif()

elseif(EMSCRIPTEN)
  # Hide noisy warnings that intend to aid mostly during initial stages of porting a new
  # project. Uncomment at will for verbose cross-compiling -I/../ path info.
  sdl_compile_options(PRIVATE "-Wno-warn-absolute-paths")

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/emscripten/*.c")
  set(HAVE_SDL_MAIN_CALLBACKS TRUE)

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/emscripten/*.c")
  set(HAVE_SDL_MISC TRUE)

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_EMSCRIPTEN 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/emscripten/*.c")
    set(HAVE_SDL_AUDIO TRUE)
  endif()

  set(SDL_FILESYSTEM_EMSCRIPTEN 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/emscripten/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  set(SDL_FSOPS_POSIX 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  if(SDL_CAMERA)
    set(SDL_CAMERA_DRIVER_EMSCRIPTEN 1)
    set(HAVE_CAMERA TRUE)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/emscripten/*.c")
  endif()

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_EMSCRIPTEN 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/emscripten/*.c")
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  if(SDL_POWER)
    set(SDL_POWER_EMSCRIPTEN 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/emscripten/*.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/emscripten/*.c")
  set(HAVE_SDL_LOCALE TRUE)

  set(SDL_TIME_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/unix/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_CLOCK_GETTIME)
    set(HAVE_CLOCK_GETTIME 1)
  endif()

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_EMSCRIPTEN 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/emscripten/*.c")
    set(HAVE_SDL_VIDEO TRUE)

    #enable gles
    if(SDL_OPENGLES)
      set(SDL_VIDEO_OPENGL_EGL 1)
      set(HAVE_OPENGLES TRUE)
      set(SDL_VIDEO_OPENGL_ES2 1)
      set(SDL_VIDEO_RENDER_OGL_ES2 1)
    endif()
  endif()

  CheckPTHREAD()
  CheckLibUnwind()

elseif(UNIX AND NOT APPLE AND NOT RISCOS AND NOT HAIKU)
  if(SDL_AUDIO)
    if(NETBSD)
        set(SDL_AUDIO_DRIVER_NETBSD 1)
        sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/netbsd/*.c")
        set(HAVE_SDL_AUDIO TRUE)
    elseif(QNX)
        set(SDL_AUDIO_DRIVER_QNX 1)
        sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/qnx/*.c")
        sdl_link_dependency(asound LIBS asound)
        set(HAVE_SDL_AUDIO TRUE)
    endif()
    CheckOSS()
    CheckALSA()
    CheckJACK()
    CheckPipewire()
    CheckPulseAudio()
    CheckSNDIO()
  endif()

  if(SDL_VIDEO)
    # Need to check for Raspberry PI first and add platform specific compiler flags, otherwise the test for GLES fails!
    CheckRPI()
    # Need to check for ROCKCHIP platform and get rid of "Can't window GBM/EGL surfaces on window creation."
    CheckROCKCHIP()
    CheckX11()
    # Need to check for EGL first because KMSDRM and Wayland depend on it.
    CheckEGL()
    CheckKMSDRM()
    CheckGLX()
    CheckOpenGL()
    CheckOpenGLES()
    CheckWayland()
    CheckVivante()
    CheckVulkan()
    CheckQNXScreen()
  endif()

  if(UNIX)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/core/unix/*.c")

    check_c_source_compiles("
        #include <linux/input.h>
        #ifndef EVIOCGNAME
        #error EVIOCGNAME() ioctl not available
        #endif
        int main(int argc, char** argv) { return 0; }" HAVE_LINUX_INPUT_H)

    if(LINUX)
      check_c_source_compiles("
          #include <linux/kd.h>
          #include <linux/keyboard.h>
          #include <sys/ioctl.h>
          int main(int argc, char **argv) {
              struct kbentry kbe;
              kbe.kb_table = KG_CTRL;
              ioctl(0, KDGKBENT, &kbe);
              return 0;
          }" HAVE_INPUT_KD)
      check_c_source_compiles("
          #include <linux/videodev2.h>
          int main(int argc, char** argv) { return 0; }" HAVE_LINUX_VIDEODEV2_H)
    elseif(FREEBSD)
      check_c_source_compiles("
          #include <sys/kbio.h>
          #include <sys/ioctl.h>
          int main(int argc, char **argv) {
              accentmap_t accTable;
              ioctl(0, KDENABIO, 1);
              return 0;
          }" HAVE_INPUT_KBIO)
    elseif(OPENBSD OR NETBSD)
      check_c_source_compiles("
          #include <sys/time.h>
          #include <dev/wscons/wsconsio.h>
          #include <dev/wscons/wsksymdef.h>
          #include <dev/wscons/wsksymvar.h>
          #include <sys/ioctl.h>
          int main(int argc, char **argv) {
              struct wskbd_map_data data;
              ioctl(0, WSKBDIO_GETMAP, &data);
              return 0;
          }" HAVE_INPUT_WSCONS)
    endif()

    if(SDL_CAMERA AND HAVE_LINUX_VIDEODEV2_H)
      set(SDL_CAMERA_DRIVER_V4L2 1)
      set(HAVE_CAMERA TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/v4l2/*.c")
    endif()

    if(HAVE_LINUX_INPUT_H)
      set(SDL_INPUT_LINUXEV 1)
    endif()

    if(SDL_HAPTIC AND HAVE_LINUX_INPUT_H)
      set(SDL_HAPTIC_LINUX 1)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/haptic/linux/*.c")
      set(HAVE_SDL_HAPTIC TRUE)
    endif()

    if(HAVE_INPUT_KD)
      set(SDL_INPUT_LINUXKD 1)
    endif()

    if(HAVE_INPUT_KBIO)
      set(SDL_INPUT_FBSDKBIO 1)
    endif()

    if(HAVE_INPUT_WSCONS)
      set(SDL_INPUT_WSCONS 1)
    endif()

    CheckLibUDev()
    check_include_file("sys/inotify.h" HAVE_SYS_INOTIFY_H)
    check_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY_INIT)
    check_symbol_exists(inotify_init1 "sys/inotify.h" HAVE_INOTIFY_INIT1)

    if(HAVE_SYS_INOTIFY_H AND HAVE_INOTIFY_INIT)
      set(HAVE_INOTIFY 1)
    endif()

    if(PKG_CONFIG_FOUND)
      if(SDL_DBUS)
        pkg_search_module(DBUS dbus-1 dbus)
        if(DBUS_FOUND)
          set(HAVE_DBUS_DBUS_H TRUE)
          sdl_include_directories(PRIVATE SYSTEM ${DBUS_INCLUDE_DIRS})
          # Fcitx need only dbus.
          set(HAVE_FCITX TRUE)
          set(HAVE_DBUS TRUE)
        endif()
      endif()

      if(SDL_IBUS)
        pkg_search_module(IBUS ibus-1.0 ibus)
        find_path(HAVE_SYS_INOTIFY_H NAMES sys/inotify.h)
        if(IBUS_FOUND AND HAVE_SYS_INOTIFY_H)
          set(HAVE_IBUS_IBUS_H TRUE)
          sdl_include_directories(PRIVATE SYSTEM ${IBUS_INCLUDE_DIRS})
          set(HAVE_IBUS TRUE)
        endif()
      endif()

      if (HAVE_IBUS_IBUS_H OR HAVE_FCITX)
        set(SDL_USE_IME 1)
      endif()

      if((FREEBSD OR NETBSD) AND NOT HAVE_INOTIFY)
        set(LibInotify_PKG_CONFIG_SPEC libinotify)
        pkg_check_modules(PC_LIBINOTIFY IMPORTED_TARGET ${LibInotify_PKG_CONFIG_SPEC})
        if(PC_LIBINOTIFY_FOUND)
          set(HAVE_INOTIFY 1)
          sdl_link_dependency(libinotify LIBS PkgConfig::PC_LIBINOTIFY PKG_CONFIG_PREFIX PC_LIBINOTIFY PKG_CONFIG_SPECS ${LibInotify_PKG_CONFIG_SPEC})
        endif()
      endif()

      CheckLibUnwind()
    endif()

    if(HAVE_DBUS_DBUS_H)
      sdl_sources(
        "${SDL3_SOURCE_DIR}/src/core/linux/SDL_dbus.c"
        "${SDL3_SOURCE_DIR}/src/core/linux/SDL_system_theme.c"
      )
    endif()

    if(SDL_USE_IME)
      sdl_sources("${SDL3_SOURCE_DIR}/src/core/linux/SDL_ime.c")
    endif()

    if(HAVE_IBUS_IBUS_H)
      sdl_sources("${SDL3_SOURCE_DIR}/src/core/linux/SDL_ibus.c")
    endif()

    if(HAVE_FCITX)
      sdl_sources("${SDL3_SOURCE_DIR}/src/core/linux/SDL_fcitx.c")
    endif()

    if(HAVE_LIBUDEV_H)
      sdl_sources("${SDL3_SOURCE_DIR}/src/core/linux/SDL_udev.c")
    endif()

    if(HAVE_LINUX_INPUT_H)
      sdl_sources(
        "${SDL3_SOURCE_DIR}/src/core/linux/SDL_evdev.c"
        "${SDL3_SOURCE_DIR}/src/core/linux/SDL_evdev_kbd.c"
      )
    endif()

    if(HAVE_INPUT_KBIO)
      sdl_sources("${SDL3_SOURCE_DIR}/src/core/freebsd/SDL_evdev_kbd_freebsd.c")
    endif()

    if(HAVE_INPUT_WSCONS)
      sdl_sources(
        "${SDL3_SOURCE_DIR}/src/core/openbsd/SDL_wscons_kbd.c"
        "${SDL3_SOURCE_DIR}/src/core/openbsd/SDL_wscons_mouse.c"
      )
    endif()

    if(SDL_GPU AND SDL_GPU_DXVK)
      if(PKG_CONFIG_FOUND)
        pkg_search_module(DXVK_NATIVE dxvk-dxgi)
        if(DXVK_NATIVE_FOUND)
          set(HAVE_D3D11_H TRUE)
          sdl_include_directories(PRIVATE SYSTEM ${DXVK_NATIVE_INCLUDE_DIRS})
        endif()
      endif()
    endif()

    # Always compiled for Linux, unconditionally:
    sdl_sources(
      "${SDL3_SOURCE_DIR}/src/core/linux/SDL_evdev_capabilities.c"
      "${SDL3_SOURCE_DIR}/src/core/linux/SDL_threadprio.c"
      "${SDL3_SOURCE_DIR}/src/core/linux/SDL_sandbox.c"
    )

    # src/core/unix/*.c is included in a generic if(UNIX) section, elsewhere.
  endif()

  if(SDL_HIDAPI)
    CheckHIDAPI()
  endif()

  if(SDL_JOYSTICK)
    if(FREEBSD OR NETBSD OR OPENBSD OR BSDI)
      CheckUSBHID()
    endif()
    if((LINUX OR FREEBSD) AND HAVE_LINUX_INPUT_H AND NOT ANDROID)
      set(SDL_JOYSTICK_LINUX 1)
      sdl_glob_sources(
        "${SDL3_SOURCE_DIR}/src/joystick/linux/*.c"
        "${SDL3_SOURCE_DIR}/src/joystick/steam/*.c"
      )
      set(HAVE_SDL_JOYSTICK TRUE)
    endif()
  endif()

  CheckPTHREAD()

  if(SDL_CLOCK_GETTIME)
    check_library_exists(c clock_gettime "" FOUND_CLOCK_GETTIME_LIBC)
    if(FOUND_CLOCK_GETTIME_LIBC)
      set(HAVE_CLOCK_GETTIME 1)
    else()
      check_library_exists(rt clock_gettime "" FOUND_CLOCK_GETTIME_LIBRT)
      if(FOUND_CLOCK_GETTIME_LIBRT)
        set(HAVE_CLOCK_GETTIME 1)
        sdl_link_dependency(clock LIBS rt)
      endif()
    endif()
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/unix/*.c")
  set(HAVE_SDL_MISC TRUE)

  if(SDL_POWER)
    if(LINUX)
      set(SDL_POWER_LINUX 1)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/linux/*.c")
      set(HAVE_SDL_POWER TRUE)
    endif()
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/unix/*.c")
  set(HAVE_SDL_LOCALE TRUE)

  set(SDL_FILESYSTEM_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/unix/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  set(SDL_STORAGE_GENERIC 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/generic/*.c")
  if(LINUX)
    set(SDL_STORAGE_STEAM 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/steam/*.c")
  endif()
  set(HAVE_SDL_STORAGE 1)

  set(SDL_FSOPS_POSIX 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  set(SDL_TIME_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/unix/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  set(SDL_RLD_FLAGS "")
  if(SDL_RPATH AND SDL_SHARED)
    if(BSDI OR FREEBSD OR LINUX OR NETBSD)
      cmake_push_check_state()
      string(APPEND CMAKE_REQUIRED_FLAGS " -Wl,--enable-new-dtags")
      check_c_compiler_flag("" HAVE_ENABLE_NEW_DTAGS)
      cmake_pop_check_state()
      if(HAVE_ENABLE_NEW_DTAGS)
        set(SDL_RLD_FLAGS "-Wl,-rpath,\${libdir} -Wl,--enable-new-dtags")
      else()
        set(SDL_RLD_FLAGS "-Wl,-rpath,\${libdir}")
      endif()
      set(HAVE_RPATH TRUE)
    elseif(SOLARIS)
      set(SDL_RLD_FLAGS "-R\${libdir}")
      set(HAVE_RPATH TRUE)
    endif()
  endif()

  if(QNX)
    # QNX's *printf() family generates a SIGSEGV if NULL is passed for a string
    # specifier (on purpose), but SDL expects "(null)". Use the built-in
    # implementation.
    set (HAVE_VSNPRINTF 0)
    set (USE_POSIX_SPAWN 1)
  endif()
elseif(WINDOWS)
  check_c_source_compiles("
    #include <windows.h>
    int main(int argc, char **argv) { return 0; }" HAVE_WIN32_CC)

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/core/windows/*.c")
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/windows/*.c")

  if(TARGET SDL3-shared AND MSVC AND NOT SDL_LIBC)
    # Prevent codegen that would use the VC runtime libraries.
    target_compile_options(SDL3-shared PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/GS-> $<$<COMPILE_LANGUAGE:C,CXX>:/Gs1048576>)
    if(SDL_CPU_X86)
      target_compile_options(SDL3-shared PRIVATE "/arch:SSE")
    endif()
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/windows/*.c")

  set(HAVE_SDL_MISC TRUE)

  # Check for DirectX
  if(SDL_DIRECTX)
    cmake_push_check_state()
    if(DEFINED MSVC_VERSION AND NOT ${MSVC_VERSION} LESS 1700)
        set(USE_WINSDK_DIRECTX TRUE)
    endif()
    if(NOT MINGW AND NOT USE_WINSDK_DIRECTX)
      if("$ENV{DXSDK_DIR}" STREQUAL "")
        message(FATAL_ERROR "DIRECTX requires the \$DXSDK_DIR environment variable to be set")
      endif()
      string(APPEND CMAKE_REQUIRED_FLAGS " /I\"$ENV{DXSDK_DIR}\\Include\"")
    endif()

    check_include_file(d3d9.h HAVE_D3D9_H)
    check_include_file(d3d11_1.h HAVE_D3D11_H)
    check_include_file(ddraw.h HAVE_DDRAW_H)
    check_include_file(dsound.h HAVE_DSOUND_H)
    check_include_file(dinput.h HAVE_DINPUT_H)
    if(SDL_CPU_ARM32)  # !!! FIXME: this should probably check if we're !(x86 or x86-64) instead of arm.
      set(HAVE_DINPUT_H 0)
    endif()
    check_include_file(dxgi.h HAVE_DXGI_H)
    cmake_pop_check_state()
    if(HAVE_D3D9_H OR HAVE_D3D11_H OR HAVE_DDRAW_H OR HAVE_DSOUND_H OR HAVE_DINPUT_H)
      set(HAVE_DIRECTX TRUE)
      if(NOT MINGW AND NOT USE_WINSDK_DIRECTX)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
          set(PROCESSOR_ARCH "x64")
        else()
          set(PROCESSOR_ARCH "x86")
        endif()
        sdl_link_directories("$<BUILD_INTERFACE:$$ENV{DXSDK_DIR}\\lib\\${PROCESSOR_ARCH}>")
        sdl_include_directories(PRIVATE SYSTEM "$<BUILD_INTERFACE:$ENV{DXSDK_DIR}\\Include>")
      endif()
    endif()
  endif()

  if(SDL_XINPUT)
    # xinput.h may need windows.h, but does not include it itself.
    check_c_source_compiles("
      #include <windows.h>
      #include <xinput.h>
      int main(int argc, char **argv) { return 0; }" HAVE_XINPUT_H)
  endif()

  # headers needed elsewhere
  check_c_source_compiles("
    #define COBJMACROS
    #include <windows.gaming.input.h>
    static __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics2 *s2;
    int main(int argc, char **argv) { return 0; }" HAVE_WINDOWS_GAMING_INPUT_H
  )
  check_c_source_compiles("
    #include <stdbool.h>
    #define COBJMACROS
    #include <gameinput.h>
    int main(int argc, char **argv) { return 0; }" HAVE_GAMEINPUT_H
  )
  check_include_file(dxgi1_6.h HAVE_DXGI1_6_H)
  check_include_file(tpcshrd.h HAVE_TPCSHRD_H)
  check_include_file(roapi.h HAVE_ROAPI_H)
  check_include_file(mmdeviceapi.h HAVE_MMDEVICEAPI_H)
  check_include_file(audioclient.h HAVE_AUDIOCLIENT_H)
  check_include_file(sensorsapi.h HAVE_SENSORSAPI_H)
  check_include_file(shellscalingapi.h HAVE_SHELLSCALINGAPI_H)
  check_c_source_compiles("
    #include <windows.h>
    #include <mfapi.h>
    #include <mfidl.h>
    #include <mfreadwrite.h>
    int main(int argc, char **argv) { return 0; }
    " HAVE_MFAPI_H
  )

  if(SDL_AUDIO)
    if(HAVE_DSOUND_H)
      set(SDL_AUDIO_DRIVER_DSOUND 1)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/directsound/*.c")
      set(HAVE_SDL_AUDIO TRUE)
    endif()

    if(SDL_WASAPI AND HAVE_AUDIOCLIENT_H AND HAVE_MMDEVICEAPI_H)
      set(SDL_AUDIO_DRIVER_WASAPI 1)
      set(HAVE_WASAPI TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/wasapi/*.c")
      set(HAVE_SDL_AUDIO TRUE)
    endif()
  endif()

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_WINDOWS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/windows/*.c")

    if(SDL_RENDER_D3D AND HAVE_D3D9_H)
      set(SDL_VIDEO_RENDER_D3D 1)
      set(HAVE_RENDER_D3D TRUE)
    endif()
    if(SDL_RENDER_D3D11 AND HAVE_D3D11_H)
      set(SDL_VIDEO_RENDER_D3D11 1)
      set(HAVE_RENDER_D3D11 TRUE)
    endif()
    if(SDL_RENDER_D3D12)
      set(SDL_VIDEO_RENDER_D3D12 1)
      set(HAVE_RENDER_D3D12 TRUE)
    endif()
    set(HAVE_SDL_VIDEO TRUE)
  endif()

  set(SDL_THREAD_GENERIC_COND_SUFFIX 1)
  set(SDL_THREAD_GENERIC_RWLOCK_SUFFIX 1)
  set(SDL_THREAD_WINDOWS 1)
  sdl_sources(
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_syscond.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_sysrwlock.c"
    "${SDL3_SOURCE_DIR}/src/thread/windows/SDL_syscond_cv.c"
    "${SDL3_SOURCE_DIR}/src/thread/windows/SDL_sysmutex.c"
    "${SDL3_SOURCE_DIR}/src/thread/windows/SDL_sysrwlock_srw.c"
    "${SDL3_SOURCE_DIR}/src/thread/windows/SDL_syssem.c"
    "${SDL3_SOURCE_DIR}/src/thread/windows/SDL_systhread.c"
    "${SDL3_SOURCE_DIR}/src/thread/windows/SDL_systls.c"
  )

  set(HAVE_SDL_THREADS TRUE)

  if(SDL_SENSOR AND HAVE_SENSORSAPI_H)
    set(SDL_SENSOR_WINDOWS 1)
    set(HAVE_SDL_SENSORS TRUE)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/sensor/windows/*.c")
  endif()

  if(SDL_POWER)
    set(SDL_POWER_WINDOWS 1)
    sdl_sources("${SDL3_SOURCE_DIR}/src/power/windows/SDL_syspower.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/windows/*.c")
  set(HAVE_SDL_LOCALE TRUE)

  set(SDL_FILESYSTEM_WINDOWS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/windows/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  set(SDL_FSOPS_WINDOWS 1)
  set(HAVE_SDL_FSOPS TRUE)

  set(SDL_STORAGE_GENERIC 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/generic/*.c")
  set(SDL_STORAGE_STEAM 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/steam/*.c")
  set(HAVE_SDL_STORAGE 1)

  # Libraries for Win32 native and MinGW
  sdl_link_dependency(base LIBS kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32)

  set(SDL_TIME_WINDOWS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/windows/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_WINDOWS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/windows/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  set(SDL_LOADSO_WINDOWS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/loadso/windows/*.c")
  set(HAVE_SDL_LOADSO TRUE)

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/core/windows/*.c")

  if(SDL_VIDEO)
    if(SDL_OPENGL)
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_OPENGL_WGL 1)
      set(SDL_VIDEO_RENDER_OGL 1)
      set(HAVE_OPENGL TRUE)
    endif()

    if(SDL_OPENGLES)
      set(SDL_VIDEO_OPENGL_EGL 1)
      set(SDL_VIDEO_OPENGL_ES2 1)
      set(SDL_VIDEO_RENDER_OGL_ES2 1)
      set(HAVE_OPENGLES TRUE)
    endif()

    if(SDL_VULKAN)
      set(SDL_VIDEO_VULKAN 1)
      set(HAVE_VULKAN TRUE)
      if(SDL_RENDER_VULKAN)
        set(SDL_VIDEO_RENDER_VULKAN 1)
        set(HAVE_RENDER_VULKAN TRUE)
      endif()
    endif()
  endif()

  if(SDL_HIDAPI)
    CheckHIDAPI()
  endif()

  if(SDL_JOYSTICK)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/windows/*.c")

    set(SDL_JOYSTICK_RAWINPUT 1)
    if(HAVE_DINPUT_H)
      set(SDL_JOYSTICK_DINPUT 1)
      sdl_link_dependency(joystick LIBS dinput8)
    endif()
    if(HAVE_XINPUT_H)
      set(SDL_JOYSTICK_XINPUT 1)
      set(HAVE_XINPUT TRUE)
    endif()
    if(HAVE_WINDOWS_GAMING_INPUT_H)
      set(SDL_JOYSTICK_WGI 1)
    endif()
    if(HAVE_GAMEINPUT_H)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/gdk/*.c")
      set(SDL_JOYSTICK_GAMEINPUT 1)
    endif()
    set(HAVE_SDL_JOYSTICK TRUE)

    if(SDL_HAPTIC)
      if(HAVE_DINPUT_H)
        sdl_glob_sources("${SDL3_SOURCE_DIR}/src/haptic/windows/*.c")
        set(SDL_HAPTIC_DINPUT 1)
        set(HAVE_SDL_HAPTIC TRUE)
      endif()
    endif()
  endif()

  if(SDL_CAMERA)
    if(HAVE_MFAPI_H)
      set(HAVE_CAMERA TRUE)
      set(SDL_CAMERA_DRIVER_MEDIAFOUNDATION 1)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/mediafoundation/*.c")
    endif()
  endif()

  enable_language(RC)
  sdl_glob_sources(SHARED "${SDL3_SOURCE_DIR}/src/core/windows/*.rc")
  if(MINGW OR CYGWIN)
    sdl_pc_link_options("-mwindows")
  endif()

elseif(APPLE)
  # TODO: rework this all for proper macOS, iOS and Darwin support

  # !!! FIXME: all the `if(IOS OR TVOS OR VISIONOS)` checks should get merged into one variable, so we're ready for the next platform (or just WatchOS).

  # We always need these libs on macOS at the moment.
  # !!! FIXME: we need Carbon for some very old API calls in
  # !!! FIXME:  src/video/cocoa/SDL_cocoakeyboard.c, but we should figure out
  # !!! FIXME:  how to dump those.
  if(MACOS)
    set(SDL_FRAMEWORK_COCOA 1)
    set(SDL_FRAMEWORK_CARBON 1)
    set(SDL_FRAMEWORK_UTTYPES 1)
  endif()
  set(SDL_FRAMEWORK_FOUNDATION 1)
  set(SDL_FRAMEWORK_COREVIDEO 1)

  # iOS can use a CADisplayLink for main callbacks. macOS just uses the generic one atm.
  if(IOS OR TVOS OR VISIONOS OR WATCHOS)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/ios/*.m")
    set(HAVE_SDL_MAIN_CALLBACKS TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/file/cocoa/*.m")

  if(SDL_CAMERA)
    if(MACOS OR IOS)
      set(SDL_CAMERA_DRIVER_COREMEDIA 1)
      set(HAVE_CAMERA TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/coremedia/*.m")
    endif()
  endif()

  if(IOS OR TVOS OR VISIONOS OR WATCHOS)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/ios/*.m")
  else()
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/macos/*.m")
  endif()
  set(HAVE_SDL_MISC TRUE)

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_COREAUDIO 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/coreaudio/*.m")
    set(HAVE_SDL_AUDIO TRUE)
    set(SDL_FRAMEWORK_COREAUDIO 1)
    set(SDL_FRAMEWORK_AUDIOTOOLBOX 1)
    set(SDL_FRAMEWORK_AVFOUNDATION 1)
  endif()

  if(SDL_HIDAPI)
    CheckHIDAPI()
  endif()

  if(SDL_JOYSTICK)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/apple/*.m")
    if(IOS OR TVOS OR VISIONOS OR WATCHOS)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/steam/*.c")
      set(SDL_JOYSTICK_MFI 1)
      if(IOS OR VISIONOS OR WATCHOS)
        set(SDL_FRAMEWORK_COREMOTION 1)
      endif()
      set(SDL_FRAMEWORK_GAMECONTROLLER 1)
      set(SDL_FRAMEWORK_COREHAPTICS 1)
    else()
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/darwin/*.c")
      set_property(SOURCE ${MFI_JOYSTICK_SOURCES} APPEND_STRING PROPERTY COMPILE_FLAGS " -fobjc-weak")
      check_objc_source_compiles("
          #include <AvailabilityMacros.h>
          #include <TargetConditionals.h>
          #import <Foundation/Foundation.h>
          #import <GameController/GameController.h>
          #if MAC_OS_X_VERSION_MIN_REQUIRED < 1080
          #error GameController framework doesn't work on this configuration
          #endif
          #if TARGET_CPU_X86
          #error GameController framework doesn't work on this configuration
          #endif
          int main() { return 0; }" HAVE_FRAMEWORK_GAMECONTROLLER)
      check_objc_source_compiles("
          #include <AvailabilityMacros.h>
          #include <TargetConditionals.h>
          #import <Foundation/Foundation.h>
          #import <CoreHaptics/CoreHaptics.h>
          int main() { return 0; }" HAVE_FRAMEWORK_COREHAPTICS)
      if(HAVE_FRAMEWORK_GAMECONTROLLER AND HAVE_FRAMEWORK_COREHAPTICS)
        # Only enable MFI if we also have CoreHaptics to ensure rumble works
        set(SDL_JOYSTICK_MFI 1)
        set(SDL_FRAMEWORK_GAMECONTROLLER 1)
        set(SDL_FRAMEWORK_COREHAPTICS 1)
      endif()
      if(NOT VISIONOS)
        set(SDL_JOYSTICK_IOKIT 1)
        set(SDL_FRAMEWORK_IOKIT 1)
      endif()
      set(SDL_FRAMEWORK_FF 1)
    endif()
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  if(SDL_HAPTIC)
    if (IOS OR TVOS OR VISIONOS OR WATCHOS)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/haptic/dummy/*.c")
      set(SDL_HAPTIC_DUMMY 1)
    else()
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/haptic/darwin/*.c")
      set(SDL_HAPTIC_IOKIT 1)
      set(SDL_FRAMEWORK_IOKIT 1)
      set(SDL_FRAMEWORK_FF 1)
    endif()
    set(HAVE_SDL_HAPTIC TRUE)
  endif()

  if(SDL_POWER)
    if (IOS OR TVOS OR VISIONOS OR WATCHOS)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/uikit/*.m")
      set(SDL_POWER_UIKIT 1)
    else()
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/macos/*.c")
      set(SDL_POWER_MACOSX 1)
      set(SDL_FRAMEWORK_IOKIT 1)
    endif()
    set(HAVE_SDL_POWER TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/macos/*.m")
  set(HAVE_SDL_LOCALE TRUE)

  set(SDL_TIME_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/unix/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  set(SDL_FILESYSTEM_COCOA 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/cocoa/*.m")
  set(HAVE_SDL_FILESYSTEM TRUE)

  # TODO: SDL_STORAGE_ICLOUD
  set(SDL_STORAGE_GENERIC 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/generic/*.c")
  if(MACOS)
    set(SDL_STORAGE_STEAM 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/steam/*.c")
  endif()
  set(HAVE_SDL_STORAGE 1)

  set(SDL_FSOPS_POSIX 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  if(SDL_SENSOR)
    if(IOS OR VISIONOS OR WATCHOS)
      set(SDL_SENSOR_COREMOTION 1)
      set(HAVE_SDL_SENSORS TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/sensor/coremotion/*.m")
    endif()
  endif()

  # iOS hack needed - http://code.google.com/p/ios-cmake/ ?
  if(SDL_VIDEO)
    if(IOS OR TVOS OR VISIONOS OR WATCHOS)
      set(SDL_VIDEO_DRIVER_UIKIT 1)
      set(SDL_FRAMEWORK_COREGRAPHICS 1)
      set(SDL_FRAMEWORK_QUARTZCORE 1)
      set(SDL_FRAMEWORK_UIKIT 1)
      set(SDL_IPHONE_KEYBOARD 1)
      set(SDL_IPHONE_LAUNCHSCREEN 1)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/uikit/*.m")
      set(HAVE_SDL_VIDEO TRUE)
    else()
      CheckCOCOA()
      if(SDL_OPENGL)
        set(SDL_VIDEO_OPENGL 1)
        set(SDL_VIDEO_OPENGL_CGL 1)
        set(SDL_VIDEO_RENDER_OGL 1)
        set(HAVE_OPENGL TRUE)
      endif()
    endif()

    if(SDL_OPENGLES)
      if(IOS OR TVOS OR VISIONOS OR WATCHOS)
        set(SDL_FRAMEWORK_OPENGLES 1)
        set(SDL_VIDEO_OPENGL_ES 1)
      else()
        set(SDL_VIDEO_OPENGL_EGL 1)
      endif()
      set(SDL_VIDEO_OPENGL_ES2 1)
      set(SDL_VIDEO_RENDER_OGL_ES2 1)
      set(HAVE_OPENGLES TRUE)
    endif()

    if(SDL_VULKAN OR SDL_METAL OR SDL_RENDER_METAL)
      check_objc_source_compiles("
        #include <AvailabilityMacros.h>
        #import <Metal/Metal.h>
        #import <QuartzCore/CAMetalLayer.h>

        #if (!TARGET_CPU_X86_64 && !TARGET_CPU_ARM64)
        #error Metal doesn't work on this configuration
        #endif
        int main(int argc, char **argv) { return 0; }" HAVE_FRAMEWORK_METAL)
      if(HAVE_FRAMEWORK_METAL)
        set(SDL_FRAMEWORK_METAL 1)
        set(SDL_FRAMEWORK_QUARTZCORE 1)
        if(SDL_VULKAN)
          set(SDL_VIDEO_VULKAN 1)
          set(HAVE_VULKAN TRUE)
          if(SDL_RENDER_VULKAN)
            set(SDL_VIDEO_RENDER_VULKAN 1)
            set(HAVE_RENDER_VULKAN TRUE)
          endif()
        endif()
        if(SDL_METAL)
          set(SDL_VIDEO_METAL 1)
          set(HAVE_METAL TRUE)
        endif()
        if(SDL_RENDER_METAL)
          sdl_glob_sources("${SDL3_SOURCE_DIR}/src/render/metal/*.m")
          set(SDL_VIDEO_RENDER_METAL 1)
          set(HAVE_RENDER_METAL TRUE)
        endif()
        if (SDL_GPU)
          set(SDL_GPU_METAL 1)
          sdl_glob_sources("${SDL3_SOURCE_DIR}/src/gpu/metal/*.m")
        endif()
      endif()
    endif()
  endif()

  # Minimum version for $<LINK_LIBRARY:feature,library-list>
  cmake_minimum_required(VERSION 3.24)

  # Actually load the frameworks at the end so we don't duplicate include.
  if(SDL_FRAMEWORK_COREVIDEO)
    find_library(COREMEDIA CoreMedia)
    if(COREMEDIA)
      sdl_link_dependency(corevideo LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreMedia>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreMedia")
    endif()
    sdl_link_dependency(corevideo LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreVideo>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreVideo")
  endif()
  if(SDL_FRAMEWORK_COCOA)
    sdl_link_dependency(cocoa LIBS "$<LINK_LIBRARY:FRAMEWORK,Cocoa>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,Cocoa")
  endif()
  if(SDL_FRAMEWORK_IOKIT)
    sdl_link_dependency(iokit LIBS "$<LINK_LIBRARY:FRAMEWORK,IOKit>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,IOKit")
  endif()
  if(SDL_FRAMEWORK_FF)
    sdl_link_dependency(ff LIBS "$<LINK_LIBRARY:FRAMEWORK,ForceFeedback>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,ForceFeedback")
  endif()
  if(SDL_FRAMEWORK_CARBON)
    sdl_link_dependency(carbon LIBS "$<LINK_LIBRARY:FRAMEWORK,Carbon>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,Carbon")
  endif()
  if(SDL_FRAMEWORK_COREAUDIO)
    sdl_link_dependency(core_audio LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreAudio>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreAudio")
  endif()
  if(SDL_FRAMEWORK_AUDIOTOOLBOX)
    sdl_link_dependency(audio_toolbox LIBS "$<LINK_LIBRARY:FRAMEWORK,AudioToolbox>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,AudioToolbox")
  endif()
  if(SDL_FRAMEWORK_AVFOUNDATION)
    sdl_link_dependency(av_foundation LIBS "$<LINK_LIBRARY:FRAMEWORK,AVFoundation>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,AVFoundation")
  endif()
  if(SDL_FRAMEWORK_COREBLUETOOTH)
    sdl_link_dependency(core_bluetooth LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreBluetooth>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreBluetooth")
  endif()
  if(SDL_FRAMEWORK_COREGRAPHICS)
    sdl_link_dependency(core_graphics LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreGraphics>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreGraphics")
  endif()
  if(SDL_FRAMEWORK_COREMOTION)
    sdl_link_dependency(core_motion LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreMotion>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreMotion")
  endif()
  if(SDL_FRAMEWORK_FOUNDATION)
    sdl_link_dependency(foundation LIBS "$<LINK_LIBRARY:FRAMEWORK,Foundation>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,Foundation")
  endif()
  if(SDL_FRAMEWORK_GAMECONTROLLER)
    find_library(GAMECONTROLLER GameController)
    if(GAMECONTROLLER)
      sdl_link_dependency(game_controller LIBS "$<LINK_LIBRARY:WEAK_FRAMEWORK,GameController>" PKG_CONFIG_LINK_OPTIONS "-Wl,-weak_framework,GameController")
    endif()
  endif()
  if(SDL_FRAMEWORK_METAL)
    if(IOS OR TVOS OR VISIONOS OR WATCHOS)
      sdl_link_dependency(metal LIBS "$<LINK_LIBRARY:FRAMEWORK,Metal>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,Metal")
    else()
      sdl_link_dependency(metal LIBS "$<LINK_LIBRARY:WEAK_FRAMEWORK,Metal>" PKG_CONFIG_LINK_OPTIONS "-Wl,-weak_framework,Metal")
    endif()
  endif()
  if(SDL_FRAMEWORK_OPENGLES)
    sdl_link_dependency(opengles LIBS "$<LINK_LIBRARY:FRAMEWORK,OpenGLES>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,OpenGLES")
  endif()
  if(SDL_FRAMEWORK_QUARTZCORE)
    if(IOS OR TVOS OR VISIONOS OR WATCHOS)
      sdl_link_dependency(quartz_core LIBS "$<LINK_LIBRARY:FRAMEWORK,QuartzCore>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,QuartzCore")
    else()
      sdl_link_dependency(metal LIBS "$<LINK_LIBRARY:WEAK_FRAMEWORK,QuartzCore>" PKG_CONFIG_LINK_OPTIONS "-Wl,-weak_framework,QuartzCore")
    endif()
  endif()
  if(SDL_FRAMEWORK_UIKIT)
    sdl_link_dependency(ui_kit LIBS "$<LINK_LIBRARY:FRAMEWORK,UIKit>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,UIKit")
  endif()
  if(SDL_FRAMEWORK_COREHAPTICS)
    find_library(COREHAPTICS CoreHaptics)
    if(COREHAPTICS)
      sdl_link_dependency(core_haptics LIBS "$<LINK_LIBRARY:FRAMEWORK,CoreHaptics>" PKG_CONFIG_LINK_OPTIONS "-Wl,-framework,CoreHaptics")
    endif()
  endif()

  CheckPTHREAD()

  if(SDL_RPATH AND SDL_SHARED)
    set(SDL_RLD_FLAGS "-Wl,-rpath,\${libdir}")
    set(HAVE_RPATH TRUE)
  endif()

elseif(HAIKU)
  enable_language(CXX)
  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_HAIKU 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/haiku/*.cc")
    set(HAVE_SDL_AUDIO TRUE)
  endif()

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_HAIKU 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/haiku/*.cc")
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/haiku/*.cc")
  set(HAVE_SDL_MISC TRUE)

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_HAIKU 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/haiku/*.cc")
    set(HAVE_SDL_VIDEO TRUE)

    if(SDL_OPENGL)
      # TODO: Use FIND_PACKAGE(OpenGL) instead
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_OPENGL_HAIKU 1)
      set(SDL_VIDEO_RENDER_OGL 1)
      sdl_link_dependency(opengl LIBS GL)
      set(HAVE_OPENGL TRUE)
    endif()
  endif()

  set(SDL_FILESYSTEM_HAIKU 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/haiku/*.cc")
  set(HAVE_SDL_FILESYSTEM TRUE)

  set(SDL_FSOPS_POSIX 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  set(SDL_TIME_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_HAIKU 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/haiku/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_POWER)
    set(SDL_POWER_HAIKU 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/haiku/*.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/haiku/*.cc")
  set(HAVE_SDL_LOCALE TRUE)

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/core/haiku/*.cc")

  CheckPTHREAD()
  sdl_link_dependency(base LIBS root be media game device textencoding tracker)

elseif(RISCOS)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/riscos/*.c")
  set(HAVE_SDL_MISC TRUE)

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_RISCOS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/riscos/*.c")
    set(HAVE_SDL_VIDEO TRUE)
  endif()

  set(SDL_FILESYSTEM_RISCOS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/riscos/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  set(SDL_FSOPS_POSIX 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  set(SDL_TIME_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_UNIX 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/unix/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_CLOCK_GETTIME)
    set(HAVE_CLOCK_GETTIME 1)
  endif()

  CheckPTHREAD()

  if(SDL_AUDIO)
    CheckOSS()
  endif()

elseif(VITA)
  # SDL_spinlock.c Needs to be compiled in ARM mode.
  cmake_push_check_state()
  string(APPEND CMAKE_REQUIRED_FLAGS " -Werror=unused-command-line-argument")
  check_c_compiler_flag(-marm HAVE_ARM_MODE)
  cmake_pop_check_state()
  if(HAVE_ARM_MODE)
    set_property(SOURCE "${SDL3_SOURCE_DIR}/src/atomic/SDL_spinlock.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -marm")
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/vita/*.c")
  set(HAVE_SDL_MISC TRUE)

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_VITA 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/vita/*.c")
    set(HAVE_SDL_AUDIO TRUE)
  endif()

  set(SDL_FILESYSTEM_VITA 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/vita/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  # !!! FIXME: do we need a FSops implementation for this?

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_VITA 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/vita/*.c")
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  if(SDL_POWER)
    set(SDL_POWER_VITA 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/vita/*.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  set(SDL_THREAD_VITA 1)
  sdl_sources(
    "${SDL3_SOURCE_DIR}/src/thread/vita/SDL_sysmutex.c"
    "${SDL3_SOURCE_DIR}/src/thread/vita/SDL_syssem.c"
    "${SDL3_SOURCE_DIR}/src/thread/vita/SDL_systhread.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_syscond.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_sysrwlock.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_systls.c"
  )
  set(HAVE_SDL_THREADS TRUE)

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/vita/*.c")
  set(HAVE_SDL_LOCALE TRUE)

  set(SDL_TIME_VITA 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/vita/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_VITA 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/vita/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_SENSOR)
    set(SDL_SENSOR_VITA 1)
    set(HAVE_SDL_SENSORS TRUE)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/sensor/vita/*.c")
  endif()

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_VITA 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/vita/*.c")
    set(HAVE_SDL_VIDEO TRUE)

    if(VIDEO_VITA_PIB)
      check_include_file(pib.h HAVE_PIGS_IN_BLANKET_H)

      if(HAVE_PIGS_IN_BLANKET_H)
        set(SDL_VIDEO_OPENGL_ES2 1)
        sdl_link_dependency(pib
          LIBS
            pib
            libScePiglet_stub_weak
            taihen_stub_weak
            SceShaccCg_stub_weak
        )
        set(HAVE_VIDEO_VITA_PIB ON)
        set(SDL_VIDEO_VITA_PIB 1)
      else()
        set(HAVE_VIDEO_VITA_PIB OFF)
      endif()
    endif()

    if(VIDEO_VITA_PVR)
      check_include_file(gpu_es4/psp2_pvr_hint.h HAVE_PVR_H)
      if(HAVE_PVR_H)
        sdl_compile_definitions(PRIVATE "__psp2__")
        set(SDL_VIDEO_OPENGL_EGL 1)
        set(HAVE_OPENGLES TRUE)
        set(SDL_VIDEO_OPENGL_ES 1)
        set(SDL_VIDEO_OPENGL_ES2 1)
        set(SDL_VIDEO_RENDER_OGL_ES2 1)

        sdl_link_dependency(pvr
          LIBS
            libgpu_es4_ext_stub_weak
            libIMGEGL_stub_weak
            SceIme_stub
        )

        set(HAVE_VIDEO_VITA_PVR ON)
        set(SDL_VIDEO_VITA_PVR 1)

        if(SDL_OPENGL)
          check_include_file(gl4esinit.h HAVE_GL4ES_H)
          if(HAVE_GL4ES_H)
            set(HAVE_OPENGL TRUE)
            set(SDL_VIDEO_OPENGL 1)
            set(SDL_VIDEO_RENDER_OGL 1)
            sdl_link_dependency(opengl LIBS libGL_stub)
            set(SDL_VIDEO_VITA_PVR_OGL 1)
          endif()
        endif()

      else()
        set(HAVE_VIDEO_VITA_PVR OFF)
      endif()
    endif()

    set(SDL_VIDEO_RENDER_VITA_GXM 1)
    sdl_link_dependency(base
      LIBS
        SceGxm_stub
        SceDisplay_stub
        SceCtrl_stub
        SceAppMgr_stub
        SceAppUtil_stub
        SceAudio_stub
        SceAudioIn_stub
        SceSysmodule_stub
        SceDisplay_stub
        SceCtrl_stub
        SceIofilemgr_stub
        SceCommonDialog_stub
        SceTouch_stub
        SceHid_stub
        SceMotion_stub
        ScePower_stub
        SceProcessmgr_stub
    )
  endif()

  sdl_compile_definitions(PRIVATE "__VITA__")

elseif(PSP)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/psp/*.c")

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_PSP 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/psp/*.c")
    set(HAVE_SDL_AUDIO TRUE)
  endif()

  set(SDL_FILESYSTEM_PSP 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/psp/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  # !!! FIXME: do we need a FSops implementation for this?

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_PSP 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/psp/*.c")
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  if(SDL_POWER)
    set(SDL_POWER_PSP 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/psp/*.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  set(SDL_THREAD_PSP 1)
  sdl_glob_sources(
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_syscond.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_systls.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_sysrwlock.c"
    "${SDL3_SOURCE_DIR}/src/thread/psp/*.c"
  )
  set(HAVE_SDL_THREADS TRUE)

  set(SDL_TIME_PSP 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/psp/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_PSP 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/psp/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_PSP 1)
    set(SDL_VIDEO_RENDER_PSP 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/psp/*.c")
    set(SDL_VIDEO_OPENGL 1)
    set(HAVE_SDL_VIDEO TRUE)
  endif()

  sdl_link_dependency(base
    LIBS
      GL
      pspvram
      pspaudio
      pspvfpu
      pspdisplay
      pspgu
      pspge
      psphprm
      pspctrl
      psppower
  )

elseif(PS2)
  sdl_compile_definitions(PRIVATE "PS2" "__PS2__")
  sdl_include_directories(PRIVATE SYSTEM "$ENV{PS2SDK}/ports/include" "$ENV{PS2DEV}/gsKit/include")
  target_include_directories(SDL_uclibc PRIVATE "$ENV{PS2SDK}/ports/include" "$ENV{PS2DEV}/gsKit/include")

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/ps2/*.c")

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_PS2 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/ps2/*.c")
    set(HAVE_SDL_AUDIO TRUE)
  endif()

  set(SDL_FILESYSTEM_PS2 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/ps2/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  # !!! FIXME: do we need a FSops implementation for this?

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_PS2 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/ps2/*.c")
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  set(SDL_THREAD_PS2 1)
  sdl_glob_sources(
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_syscond.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_sysmutex.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_sysrwlock.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_systls.c"
    "${SDL3_SOURCE_DIR}/src/thread/ps2/*.c"
  )
  set(HAVE_SDL_THREADS TRUE)

  set(SDL_TIME_PS2 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/ps2/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_PS2 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/ps2/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_PS2 1)
    set(SDL_VIDEO_RENDER_PS2 1)
    sdl_glob_sources(
      "${SDL3_SOURCE_DIR}/src/video/ps2/*.c"
      "${SDL3_SOURCE_DIR}/src/render/ps2/*.c"
    )
    set(SDL_VIDEO_OPENGL 0)
    set(HAVE_SDL_VIDEO TRUE)
  endif()

  sdl_link_dependency(base
    LIBS
      patches
      gskit
      dmakit
      ps2_drivers
  )
elseif(N3DS)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/n3ds/*.c")

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_N3DS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/n3ds/*.c")
    set(HAVE_SDL_AUDIO TRUE)
  endif()

  set(SDL_FILESYSTEM_N3DS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/n3ds/*.c")
  set(HAVE_SDL_FILESYSTEM TRUE)

  # !!! FIXME: do we need a FSops implementation for this?

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_N3DS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/n3ds/*.c")
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  if(SDL_POWER)
    set(SDL_POWER_N3DS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/power/n3ds/*.c")
    set(HAVE_SDL_POWER TRUE)
  endif()

  set(SDL_THREAD_N3DS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/thread/n3ds/*.c")
  sdl_sources(
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_systls.c"
    "${SDL3_SOURCE_DIR}/src/thread/generic/SDL_sysrwlock.c"
  )
  set(HAVE_SDL_THREADS TRUE)

  set(SDL_TIME_N3DS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/n3ds/*.c")
  set(HAVE_SDL_TIME TRUE)

  set(SDL_TIMER_N3DS 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/n3ds/*.c")
  set(HAVE_SDL_TIMERS TRUE)

  set(SDL_FSOPS_POSIX 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
  set(HAVE_SDL_FSOPS TRUE)

  if(SDL_SENSOR)
    set(SDL_SENSOR_N3DS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/sensor/n3ds/*.c")
    set(HAVE_SDL_SENSORS TRUE)
  endif()

  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_N3DS 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/n3ds/*.c")
    set(HAVE_SDL_VIDEO TRUE)
  endif()

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/n3ds/*.c")
  set(HAVE_SDL_LOCALE TRUE)

  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/file/n3ds/*.c")
endif()

if (SDL_DIALOG)
  sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/SDL_dialog_utils.c)
  if(ANDROID)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/android/SDL_androiddialog.c)
    set(HAVE_SDL_DIALOG TRUE)
  elseif(UNIX AND NOT APPLE AND NOT RISCOS AND NOT HAIKU)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/unix/SDL_unixdialog.c)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/unix/SDL_portaldialog.c)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/unix/SDL_zenitydialog.c)
    set(HAVE_SDL_DIALOG TRUE)
  elseif(HAIKU)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/haiku/SDL_haikudialog.cc)
    set(HAVE_SDL_DIALOG TRUE)
  elseif(WINDOWS)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/windows/SDL_windowsdialog.c)
    set(HAVE_SDL_DIALOG TRUE)
  elseif(MACOS)
    sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/cocoa/SDL_cocoadialog.m)
    sdl_link_dependency(uniformtypeidentifiers LIBS "$<LINK_LIBRARY:WEAK_FRAMEWORK,UniformTypeIdentifiers>" PKG_CONFIG_LINK_OPTIONS "-Wl,-weak_framework,UniformTypeIdentifiers")
    set(HAVE_SDL_DIALOG TRUE)
  endif()
endif()

sdl_sources("${SDL3_SOURCE_DIR}/src/process/SDL_process.c")
if(WINDOWS)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/process/windows/*.c")
  set(SDL_PROCESS_WINDOWS 1)
  set(HAVE_SDL_PROCESS TRUE)
else()
  check_c_source_compiles("
#include <spawn.h>
#include <unistd.h>

int main(void)
{
    int pipes[2];
    int pid;

    const char * args[] = {
      \"/bin/false\",
      NULL
    };

    const char * env[] = { NULL };

    pipe(pipes);

    posix_spawnattr_t attr;
    posix_spawn_file_actions_t fa;

    posix_spawnattr_init(&attr);
    posix_spawn_file_actions_init(&fa);

    posix_spawn_file_actions_addclose(&fa, pipes[0]);
    posix_spawn_file_actions_adddup2(&fa, pipes[1], STDOUT_FILENO);

    posix_spawn(&pid, args[0], &fa, &attr, (char * const *) args, (char * const *) env);
    posix_spawnp(&pid, args[0], &fa, &attr, (char * const *) args, (char * const *) env);

    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);

    return 0;
}
" HAVE_POSIX_SPAWN)
  if(HAVE_POSIX_SPAWN)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/process/posix/*.c")
    set(SDL_PROCESS_POSIX 1)
    set(HAVE_SDL_PROCESS TRUE)
  endif()
endif()

# Platform-independent options

if(SDL_VIDEO)
  if(SDL_OFFSCREEN)
    set(SDL_VIDEO_DRIVER_OFFSCREEN 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/offscreen/*.c")
    set(HAVE_OFFSCREEN TRUE)
    set(HAVE_SDL_VIDEO TRUE)
  endif()
endif()

if(SDL_GPU)
  if(HAVE_D3D11_H)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/gpu/d3d11/*.c")
    set(SDL_GPU_D3D11 1)
    set(HAVE_SDL_GPU TRUE)
  endif()
  if(SDL_RENDER_D3D12)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/gpu/d3d12/*.c")
    set(SDL_GPU_D3D12 1)
    set(HAVE_SDL_GPU TRUE)
  endif()
  if(SDL_VIDEO_VULKAN)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/gpu/vulkan/*.c")
    set(SDL_GPU_VULKAN 1)
    set(HAVE_SDL_GPU TRUE)
  endif()
  if(SDL_RENDER_GPU)
    set(SDL_VIDEO_RENDER_GPU 1)
    set(HAVE_RENDER_GPU TRUE)
  endif()
endif()

# Dummies
# configure.ac does it differently:
# if not have X
#   if enable_X {  SDL_X_DISABLED = 1 }
#   [add dummy sources]
# so it always adds a dummy, without checking, if it was actually requested.
# This leads to missing internal references on building, since the
# src/X/*.c does not get included.
if(NOT HAVE_SDL_AUDIO)
  set(SDL_AUDIO_DRIVER_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/dummy/*.c")
endif()
if(NOT HAVE_SDL_VIDEO)
  set(SDL_VIDEO_DRIVER_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/dummy/*.c")
endif()
if(NOT HAVE_SDL_JOYSTICK)
  set(SDL_JOYSTICK_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/dummy/*.c")
endif()
if(NOT HAVE_SDL_HAPTIC)
  set(SDL_HAPTIC_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/haptic/dummy/*.c")
endif()
if(NOT HAVE_SDL_SENSORS)
  set(SDL_SENSOR_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/sensor/dummy/*.c")
endif()
if(NOT HAVE_SDL_LOADSO)
  set(SDL_LOADSO_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/loadso/dummy/*.c")
endif()
if(NOT HAVE_SDL_FILESYSTEM)
  set(SDL_FILESYSTEM_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/dummy/*.c")
endif()
if(NOT HAVE_SDL_STORAGE)
  set(SDL_STORAGE_GENERIC 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/generic/*.c")
endif()
if(NOT HAVE_SDL_FSOPS)
  set(SDL_FSOPS_DUMMY 1)
  sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/dummy/SDL_sysfsops.c")
endif()
if(NOT HAVE_SDL_LOCALE)
  set(SDL_LOCALE_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/locale/dummy/*.c")
endif()
if(NOT HAVE_SDL_MISC)
  set(SDL_MISC_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/misc/dummy/*.c")
endif()
if(NOT HAVE_SDL_DIALOG)
  set(SDL_DIALOG_DUMMY 1)
  sdl_sources(${SDL3_SOURCE_DIR}/src/dialog/dummy/SDL_dummydialog.c)
endif()
if(NOT HAVE_SDL_PROCESS)
  set(SDL_PROCESS_DUMMY 1)
  sdl_glob_sources(${SDL3_SOURCE_DIR}/src/process/dummy/*.c)
endif()
if(NOT HAVE_CAMERA)
  set(SDL_CAMERA_DRIVER_DUMMY 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/dummy/*.c")
endif()

# We always need to have threads and timers around
if(NOT HAVE_SDL_THREADS)
  # The emscripten platform has been carefully vetted to work without threads
  if(EMSCRIPTEN)
    set(SDL_THREADS_DISABLED 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/thread/generic/*.c")
  else()
    message(FATAL_ERROR "Threads are needed by many SDL subsystems and may not be disabled")
  endif()
endif()
if(NOT HAVE_SDL_TIMERS)
  message(FATAL_ERROR "Timers are needed by many SDL subsystems and may not be disabled")
endif()

# Most platforms use this.
if(NOT HAVE_SDL_MAIN_CALLBACKS)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/main/generic/*.c")
endif()

# config variables may contain generator expression, so we need to generate SDL_build_config.h in 2 steps:
# 1. replace all `#cmakedefine`'s and `@abc@`
configure_file("${SDL3_SOURCE_DIR}/include/build_config/SDL_build_config.h.cmake"
  "${SDL3_BINARY_DIR}/SDL_build_config.h.intermediate")
# 2. generate SDL_build_config.h in an build_type-dependent folder (which should be first in the include search path)
file(GENERATE
  OUTPUT "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/build_config/SDL_build_config.h"
  INPUT "${SDL3_BINARY_DIR}/SDL_build_config.h.intermediate"
)

file(GLOB SDL3_INCLUDE_FILES "${SDL3_SOURCE_DIR}/include/SDL3/*.h")
file(GLOB SDL3_TEST_INCLUDE_FILES "${SDL3_SOURCE_DIR}/include/SDL3/SDL_test*.h")
foreach(_hdr IN LISTS SDL3_INCLUDE_FILES)
  if(_hdr MATCHES ".*SDL_revision\\.h" OR _hdr MATCHES ".*SDL_test.*\\.h")
    list(REMOVE_ITEM SDL3_INCLUDE_FILES "${_hdr}")
  endif()
endforeach()

set(SDL_REVISION "" CACHE STRING "Custom SDL revision (overrides SDL_REVISION_SUFFIX)")
if(NOT SDL_REVISION)
  set(SDL_REVISION_SUFFIX "" CACHE STRING "Suffix for the SDL revision")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt")
    # If VERSION.txt exists, it contains the SDL version
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" SDL_REVISION_CENTER)
    string(STRIP "${SDL_REVISION_CENTER}" SDL_REVISION_CENTER)
  else()
    # If VERSION does not exist, use git to calculate a version
    git_describe(SDL_REVISION_CENTER)
    if(NOT SDL_REVISION_CENTER)
      set(SDL_REVISION_CENTER "${SDL3_VERSION}-no-vcs")
    endif()
  endif()
  set(SDL_REVISION "SDL-${SDL_REVISION_CENTER}${SDL_REVISION_SUFFIX}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${SDL3_BINARY_DIR}/include/SDL3")
configure_file(include/build_config/SDL_revision.h.cmake include/SDL3/SDL_revision.h @ONLY)
list(APPEND SDL3_INCLUDE_FILES "${SDL3_BINARY_DIR}/include/SDL3/SDL_revision.h")

if(SDL_FRAMEWORK)
  # With Apple frameworks, headers in the PUBLIC_HEADER property also need to be added as sources
  list(APPEND SDL3_INCLUDE_FILES ${SDL3_TEST_INCLUDE_FILES})
  sdl_sources(${SDL3_INCLUDE_FILES})
endif()

if((CMAKE_STATIC_LIBRARY_PREFIX STREQUAL "" AND CMAKE_STATIC_LIBRARY_SUFFIX STREQUAL ".lib") OR SDL_FRAMEWORK)
  # - Avoid conflict between the dll import library and the static library
  # - Create SDL3-static Apple Framework
  set(sdl_static_libname "SDL3-static")
else()
  set(sdl_static_libname "SDL3")
endif()

macro(check_add_debug_flag FLAG SUFFIX)
  check_c_compiler_flag(${FLAG} HAS_C_FLAG_${SUFFIX})
  if(HAS_C_FLAG_${SUFFIX})
    string(APPEND CMAKE_C_FLAGS_DEBUG " ${FLAG}")
  endif()

  if(CMAKE_CXX_COMPILER)
    check_cxx_compiler_flag(${FLAG} HAS_CXX_${SUFFIX})
    if(HAS_CXX_${SUFFIX})
      string(APPEND CMAKE_CXX_FLAGS_DEBUG " ${FLAG}")
    endif()
  endif()
endmacro()

macro(asan_check_add_debug_flag ASAN_FLAG)
  check_add_debug_flag("-fsanitize=${ASAN_FLAG}" "${ASAN_FLAG}")
  if(HAS_C_${ASAN_FLAG} OR HAS_CXX_${ASAN_FLAG})
    set(HAVE_ASAN ON)
  endif()
endmacro()

macro(asan_check_add_debug_flag2 ASAN_FLAG)
  # for some sanitize flags we have to manipulate the CMAKE_REQUIRED_LIBRARIES:
  # http://cmake.3232098.n2.nabble.com/CHECK-CXX-COMPILER-FLAG-doesn-t-give-correct-result-for-fsanitize-address-tp7600216p7600217.html

  set(FLAG "-fsanitize=${ASAN_FLAG}")

  cmake_push_check_state()
  list(APPEND CMAKE_REQUIRED_LIBRARIES ${FLAG} asan)

  check_c_compiler_flag (${FLAG} HAS_C_FLAG_${ASAN_FLAG})
  if (HAS_C_FLAG_${ASAN_FLAG})
    string(APPEND CMAKE_C_FLAGS_DEBUG " ${FLAG}")
  endif()

  if(CMAKE_CXX_COMPILER)
    check_cxx_compiler_flag (${FLAG} HAS_CXX_FLAG_${ASAN_FLAG})
    if (HAS_CXX_${ASAN_FLAG})
      string(APPEND CMAKE_CXX_FLAGS_DEBUG " ${FLAG}")
    endif()
  endif()

  cmake_pop_check_state()
  if(HAS_C_${ASAN_FLAG} OR HAS_CXX_${ASAN_FLAG})
    set(HAVE_ASAN ON)
  endif()
endmacro()

# enable AddressSanitizer if supported
if(SDL_ASAN)
  asan_check_add_debug_flag2("address")
  asan_check_add_debug_flag("bool")
  asan_check_add_debug_flag("bounds")
  asan_check_add_debug_flag("enum")
  asan_check_add_debug_flag("float-cast-overflow")
  asan_check_add_debug_flag("float-divide-by-zero")
  asan_check_add_debug_flag("nonnull-attribute")
  asan_check_add_debug_flag("returns-nonnull-attribute")
  asan_check_add_debug_flag("signed-integer-overflow")
  asan_check_add_debug_flag("undefined")
  asan_check_add_debug_flag("vla-bound")
  asan_check_add_debug_flag("leak")
  # The object size sanitizer has no effect on unoptimized builds on Clang,
  # but causes warnings.
  if(NOT USE_CLANG OR CMAKE_BUILD_TYPE STREQUAL "")
    asan_check_add_debug_flag("object-size")
  endif()
endif()

if(SDL_CCACHE)
  find_program(CCACHE_BINARY ccache)
  if(CCACHE_BINARY)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_BINARY})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_BINARY})
    set(CMAKE_OBJC_COMPILER_LAUNCHER ${CCACHE_BINARY})
    set(HAVE_CCACHE ON)
  else()
    set(HAVE_CCACHE OFF)
  endif()
else()
  set(HAVE_CCACHE OFF)
endif()

if(SDL_CLANG_TIDY)
  cmake_minimum_required(VERSION 3.6)
  find_program(CLANG_TIDY_BINARY clang-tidy)

  if(CLANG_TIDY_BINARY)
    set(HAVE_CLANG_TIDY ON)
    get_clang_tidy_ignored_files(CLANG_TIDY_IGNORED_FILES)
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_BINARY}" "-extra-arg=-Wno-unknown-warning-option" "--line-filter=[${CLANG_TIDY_IGNORED_FILES}]")
    if(SDL_WERROR)
      list(APPEND CLANG_TIDY_COMMAND "--warnings-as-errors=*")
    endif()
    set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY_COMMAND})
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_COMMAND})
    set(CMAKE_OBJC_CLANG_TIDY ${CLANG_TIDY_COMMAND})
    get_property(shared_sources TARGET SDL3-collector PROPERTY INTERFACE_SOURCES)
    set_source_files_properties(${shared_sources} PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
    file(GLOB STDLIB_SOURCES "${SDL3_SOURCE_DIR}/src/stdlib/*.c")
    set_property(SOURCE ${STDLIB_SOURCES} APPEND PROPERTY COMPILE_DEFINITIONS "SDL_DISABLE_ANALYZE_MACROS")
  else()
    set(HAVE_CLANG_TIDY OFF)
  endif()
endif()

if(SDL_TESTS)
  set(HAVE_TESTS ON)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH_64 TRUE)
else()
  set(ARCH_64 FALSE)
endif()

if(ANDROID)
  sdl_include_directories(PRIVATE SYSTEM "${ANDROID_NDK}/sources/android/cpufeatures")
endif()

if(APPLE)
  cmake_push_check_state(RESET)
  check_c_compiler_flag(-fobjc-arc COMPILER_SUPPORTS_FOBJC_ARC)
  cmake_pop_check_state()
  if(NOT COMPILER_SUPPORTS_FOBJC_ARC)
    message(FATAL_ERROR "Compiler does not support -fobjc-arc: this is required on Apple platforms")
  endif()
  sdl_compile_options(PRIVATE "-fobjc-arc")
endif()

if(PS2)
  sdl_compile_options(PRIVATE "-Wno-error=declaration-after-statement")
endif()

if(NOT SDL_LIBC)
  if(MSVC)
    set(saved_CMAKE_TRY_COMPILE_TARGET_TYPE "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
    cmake_push_check_state(RESET)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
    check_c_compiler_flag("/Zl" COMPILER_SUPPORTS_Zl)
    cmake_pop_check_state()
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "${saved_CMAKE_TRY_COMPILE_TARGET_TYPE}")
    if(COMPILER_SUPPORTS_Zl)
      # /Zl omits the default C runtime library name from the .obj file.
      sdl_compile_options(PRIVATE "$<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:/Zl>")
      if(TARGET SDL3_test)
        target_compile_options(SDL3_test PRIVATE "/Zl")
      endif()
    endif()
  endif()
endif()

if(APPLE)
  get_property(sources TARGET SDL3-collector PROPERTY INTERFACE_SOURCES)
  foreach(SOURCE_FILE IN LISTS sources)
    get_filename_component(FILE_EXTENSION ${SOURCE_FILE} EXT)
    if(FILE_EXTENSION STREQUAL ".m")
      set_property(SOURCE ${SOURCE_FILE} APPEND_STRING PROPERTY COMPILE_FLAGS " -x objective-c")
    endif()
    if(NOT FILE_EXTENSION STREQUAL ".c" AND NOT FILE_EXTENSION STREQUAL ".cpp")
      set_property(SOURCE ${SOURCE_FILE} PROPERTY SKIP_PRECOMPILE_HEADERS 1)
    endif()
  endforeach()
endif()

# Disable precompiled headers on SDL_dynapi.c to avoid applying dynapi overrides
set_source_files_properties(src/dynapi/SDL_dynapi.c PROPERTIES SKIP_PRECOMPILE_HEADERS 1)

set(SDL_FRAMEWORK_RESOURCES
  Xcode/SDL/pkg-support/resources/ReadMe.txt
  LICENSE.txt
)
if(SDL_FRAMEWORK)
  sdl_sources(${SDL_FRAMEWORK_RESOURCES})
endif()

add_library(SDL3_Headers INTERFACE)
add_library(SDL3::Headers ALIAS SDL3_Headers)
set_property(TARGET SDL3_Headers PROPERTY EXPORT_NAME "Headers")
target_include_directories(SDL3_Headers
  INTERFACE
    "$<BUILD_INTERFACE:${SDL3_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${SDL3_SOURCE_DIR}/include>"
)
if(SDL_FRAMEWORK)
  target_include_directories(SDL3_Headers
    INTERFACE
      "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/SDL3.framework/Headers>"
  )
  # Add `-F <parent folder of SDL3.framework>` to make sure `#include "SDL3/..."` works.
  target_compile_options(SDL3_Headers
    INTERFACE
      "$<INSTALL_INTERFACE:SHELL:-F $<INSTALL_PREFIX>>"
  )
else()
  target_include_directories(SDL3_Headers
    INTERFACE
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )
endif()

if(SDL_SHARED)
  set_target_properties(SDL3-shared PROPERTIES
    OUTPUT_NAME "SDL3"
    POSITION_INDEPENDENT_CODE TRUE
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/dynapi/SDL_dynapi.sym"
    INTERFACE_LINK_DEPENDS "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/dynapi/SDL_dynapi.sym>"
    WINDOWS_EXPORT_ALL_SYMBOLS FALSE
    DEFINE_SYMBOL "DLL_EXPORT"
  )
  if(HAVE_GCC_FVISIBILITY)
    set_target_properties(SDL3-shared PROPERTIES
      C_VISIBILITY_PRESET "hidden"
      CXX_VISIBILITY_PRESET "hidden"
      OBJC_VISIBILITY_PRESET "hidden"
    )
  endif()
  if(NOT SDL_LIBC)
    if(MSVC AND NOT MSVC_CLANG)
      # Don't try to link with the default set of libraries.
      # Note: The clang toolset for Visual Studio does not support /NODEFAULTLIB.
      target_link_options(SDL3-shared PRIVATE "/NODEFAULTLIB")
      if(SDL_CPU_ARM32)
        # linking to msvcrt.lib avoid unresolved external symbols
        # (__rt_sdiv, __rt_udiv, __rt_sdiv64, _rt_udiv64, __dtou64, __u64tod, __i64tos)
        target_link_libraries(SDL3-shared PRIVATE msvcrt.lib)
      endif()
    endif()
    if(HAS_Q_NO_USE_LIBIRC)
      target_compile_options(SDL3-shared PRIVATE /Q_no-use-libirc)
    endif()
  endif()
  if(APPLE)
    cmake_minimum_required(VERSION 3.17)
    set_target_properties(SDL3-shared PROPERTIES
      MACOSX_RPATH TRUE
      FRAMEWORK "${SDL_FRAMEWORK}"
      SOVERSION "${SDL_SO_VERSION_MAJOR}"
      MACHO_COMPATIBILITY_VERSION "${SDL_DYLIB_COMPAT_VERSION}"
      MACHO_CURRENT_VERSION "${SDL_DYLIB_CURRENT_VERSION}"
    )
    if(SDL_FRAMEWORK)
      set_target_properties(SDL3-shared PROPERTIES
        PUBLIC_HEADER "${SDL3_INCLUDE_FILES}"
        FRAMEWORK_VERSION "${SDL_FRAMEWORK_VERSION}"
        MACOSX_FRAMEWORK_IDENTIFIER "org.libsdl.SDL3"
        RESOURCE "${SDL_FRAMEWORK_RESOURCES}"
      )
    endif()
  elseif(UNIX AND NOT ANDROID)
    set_target_properties(SDL3-shared PROPERTIES
      VERSION "${SDL_SO_VERSION}"
      SOVERSION "${SDL_SO_VERSION_MAJOR}"
    )
  else()
    if(WINDOWS OR CYGWIN)
      set_target_properties(SDL3-shared PROPERTIES
        PREFIX ""
      )
    endif()
  endif()
  target_link_libraries(SDL3-shared PRIVATE ${SDL_CMAKE_DEPENDS})
  target_include_directories(SDL3-shared
    PRIVATE
      "$<BUILD_INTERFACE:${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>>/build_config"
      "$<BUILD_INTERFACE:${SDL3_SOURCE_DIR}/src>"
  )
  target_link_libraries(SDL3-shared PUBLIC $<TARGET_NAME:SDL3::Headers>)
  if(MINGW OR CYGWIN)
    target_link_options(SDL3-shared PRIVATE -static-libgcc)
  endif()
  # Use `Compatible Interface Properties` to:
  # - allow consumers to enforce a shared/static library
  # - block linking to SDL libraries of different major version
  set_property(TARGET SDL3-shared APPEND PROPERTY COMPATIBLE_INTERFACE_BOOL SDL3_SHARED)
  set_property(TARGET SDL3-shared PROPERTY INTERFACE_SDL3_SHARED TRUE)
  set_property(TARGET SDL3-shared APPEND PROPERTY COMPATIBLE_INTERFACE_STRING "SDL_VERSION")
  set_property(TARGET SDL3-shared PROPERTY INTERFACE_SDL_VERSION "SDL${SDL3_VERSION_MAJOR}")
  if(NOT CMAKE_VERSION VERSION_LESS "3.16")
    target_precompile_headers(SDL3-shared PRIVATE "$<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:${PROJECT_SOURCE_DIR}/src/SDL_internal.h>")
  endif()
endif()

if(SDL_STATIC)
  set_target_properties(SDL3-static PROPERTIES
    OUTPUT_NAME "${sdl_static_libname}"
  )
  target_compile_definitions(SDL3-static PRIVATE SDL_STATIC_LIB)
  target_link_libraries(SDL3-static PRIVATE ${SDL_CMAKE_DEPENDS})
  target_include_directories(SDL3-static
    PRIVATE
      "$<BUILD_INTERFACE:${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>>/build_config"
      "$<BUILD_INTERFACE:${SDL3_SOURCE_DIR}/src>"
  )
  target_link_libraries(SDL3-static PUBLIC $<TARGET_NAME:SDL3::Headers>)
  # Use `Compatible Interface Properties` to:
  # - allow consumers to enforce a shared/static library
  # - block linking to SDL libraries of different major version
  set_property(TARGET SDL3-static APPEND PROPERTY COMPATIBLE_INTERFACE_BOOL SDL3_SHARED)
  set_property(TARGET SDL3-static PROPERTY INTERFACE_SDL3_SHARED FALSE)
  set_property(TARGET SDL3-static APPEND PROPERTY COMPATIBLE_INTERFACE_STRING "SDL_VERSION")
  set_property(TARGET SDL3-static PROPERTY INTERFACE_SDL_VERSION "SDL${SDL3_VERSION_MAJOR}")
  if(NOT CMAKE_VERSION VERSION_LESS "3.16")
    target_precompile_headers(SDL3-static PRIVATE "$<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:${PROJECT_SOURCE_DIR}/src/SDL_internal.h>")
  endif()
endif()

sdl_compile_definitions(
  PRIVATE
    "SDL_BUILD_MAJOR_VERSION=${PROJECT_VERSION_MAJOR}"
    "SDL_BUILD_MINOR_VERSION=${PROJECT_VERSION_MINOR}"
    "SDL_BUILD_MICRO_VERSION=${PROJECT_VERSION_PATCH}"
)

##### Tests #####

if(SDL_TEST_LIBRARY)
  file(GLOB TEST_SOURCES "${SDL3_SOURCE_DIR}/src/test/*.c")
  target_sources(SDL3_test PRIVATE ${TEST_SOURCES})
  if(APPLE)
    set_target_properties(SDL3_test PROPERTIES
      FRAMEWORK "${SDL_FRAMEWORK}"
    )
    if(SDL_FRAMEWORK)
      set_target_properties(SDL3_test PROPERTIES
        FRAMEWORK_VERSION "${SDL_FRAMEWORK_VERSION}"
        MACOSX_FRAMEWORK_IDENTIFIER "org.libsdl.SDL3_test"
        RESOURCE "${SDL_FRAMEWORK_RESOURCES}"
      )
    endif()
  endif()
  target_link_libraries(SDL3_test PUBLIC $<TARGET_NAME:SDL3::Headers>)
  # FIXME: get rid of EXTRA_TEST_LIBS variable
  target_link_libraries(SDL3_test PRIVATE ${EXTRA_TEST_LIBS})
  set_property(TARGET SDL3_test APPEND PROPERTY COMPATIBLE_INTERFACE_STRING "SDL_VERSION")
  set_property(TARGET SDL3_test PROPERTY INTERFACE_SDL_VERSION "SDL${SDL3_VERSION_MAJOR}")
endif()

##### Configure installation folders #####

if(WINDOWS AND NOT MINGW)
  set(SDL_INSTALL_CMAKEDIR_ROOT_DEFAULT "cmake")
else()
  set(SDL_INSTALL_CMAKEDIR_ROOT_DEFAULT "${CMAKE_INSTALL_LIBDIR}/cmake")
endif()
set(SDL_INSTALL_CMAKEDIR_ROOT "${SDL_INSTALL_CMAKEDIR_ROOT_DEFAULT}" CACHE STRING "Root folder where to install SDL3Config.cmake related files (SDL3 subfolder for MSVC projects)")

if(FREEBSD)
  # FreeBSD uses ${PREFIX}/libdata/pkgconfig
  set(SDL_PKGCONFIG_INSTALLDIR "libdata/pkgconfig")
else()
  set(SDL_PKGCONFIG_INSTALLDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

if(WINDOWS AND NOT MINGW)
  set(SDL_INSTALL_CMAKEDIR "${SDL_INSTALL_CMAKEDIR_ROOT}")
  set(SDL_INSTALL_LICENSEDIR "licenses/SDL3")
  set(SDL_INSTALL_HEADERSDIR "${CMAKE_INSTALL_INCLUDEDIR}/SDL3")
elseif(SDL_FRAMEWORK)
  set(SDL_INSTALL_CMAKEDIR "SDL3.framework/Versions/${SDL_FRAMEWORK_VERSION}/Resources/CMake")
  set(SDL_INSTALL_LICENSEDIR "Resources")
  set(SDL_INSTALL_HEADERSDIR "Headers")
else()
  set(SDL_INSTALL_CMAKEDIR "${SDL_INSTALL_CMAKEDIR_ROOT}/SDL3")
  set(SDL_INSTALL_LICENSEDIR "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME}")
  set(SDL_INSTALL_HEADERSDIR "${CMAKE_INSTALL_INCLUDEDIR}/SDL3")
endif()

if(SDL_FRAMEWORK)
  set(SDL_SDL_INSTALL_RESOURCEDIR "SDL3.framework/Resources")
  set(SDL_SDL_INSTALL_CMAKEDIR "${SDL_SDL_INSTALL_RESOURCEDIR}/CMake")
  set(SDL_SDL_INSTALL_REAL_RESOURCEDIR "SDL3.framework/Versions/${SDL_FRAMEWORK_VERSION}/Resources")
  set(SDL_SDL_INSTALL_REAL_CMAKEDIR "${SDL_SDL_INSTALL_REAL_RESOURCEDIR}/CMake")

  set(SDL_SDLtest_INSTALL_RESOURCEDIR "SDL3_test.framework/Resources")
  set(SDL_SDLtest_INSTALL_CMAKEDIR "${SDL_SDLtest_INSTALL_RESOURCEDIR}/CMake")
  set(SDL_SDLtest_INSTALL_CMAKEFILENAME "SDL3_testConfig.cmake")
else()
  set(SDL_SDL_INSTALL_RESOURCEDIR ".")
  set(SDL_SDL_INSTALL_CMAKEDIR ${SDL_INSTALL_CMAKEDIR})
  set(SDL_SDL_INSTALL_REAL_CMAKEDIR ${SDL_INSTALL_CMAKEDIR})

  # Install SDL3*Targets.cmake files in lib/cmake/SDL3
  set(SDL_SDLstatic_INSTALL_RESOURCEDIR ".")
  set(SDL_SDLstatic_INSTALL_CMAKEDIR "${SDL_SDL_INSTALL_CMAKEDIR}")
  set(SDL_SDLstatic_INSTALL_CMAKEFILENAME "SDL3staticTargets.cmake")

  set(SDL_SDLtest_INSTALL_RESOURCEDIR ".")
  set(SDL_SDLtest_INSTALL_CMAKEDIR "${SDL_SDL_INSTALL_CMAKEDIR}")
  set(SDL_SDLtest_INSTALL_CMAKEFILENAME "SDL3testTargets.cmake")
endif()

export(TARGETS SDL3_Headers NAMESPACE "SDL3::" FILE "SDL3headersTargets.cmake")

if(SDL_SHARED)
  export(TARGETS SDL3-shared NAMESPACE "SDL3::" FILE "SDL3sharedTargets.cmake")
endif()

if(SDL_STATIC)
  export(TARGETS SDL3-static NAMESPACE "SDL3::" FILE "SDL3staticTargets.cmake")
endif()

if(SDL_TEST_LIBRARY)
  export(TARGETS SDL3_test NAMESPACE "SDL3::" FILE "SDL3testTargets.cmake")
endif()

sdl_cmake_config_find_pkg_config_commands(SDL_FIND_PKG_CONFIG_COMMANDS
  COLLECTOR SDL3-collector
  CONFIG_COMPONENT_FOUND_NAME SDL3_SDL3-static_FOUND
)
sdl_cmake_config_find_pkg_config_commands(SDL_TEST_FIND_PKG_CONFIG_COMMANDS
  COLLECTOR SDL3_test-collector
  CONFIG_COMPONENT_FOUND_NAME SDL3_SDL3_test_FOUND
)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/SDL3Config.cmake.in SDL3Config.cmake
  NO_SET_AND_CHECK_MACRO
  PATH_VARS CMAKE_INSTALL_PREFIX
  INSTALL_DESTINATION "${SDL_SDL_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/SDL3ConfigVersion.cmake"
  COMPATIBILITY AnyNewerVersion
)

sdl_cmake_config_required_modules(sdl_cmake_modules)
if(sdl_cmake_modules)
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${sdl_cmake_modules} "${SDL3_BINARY_DIR}")
endif()

if(NOT SDL_DISABLE_INSTALL)

  ##### sdl3.pc #####
  configure_sdl3_pc()
  if(NOT SDL_FRAMEWORK)
    install(FILES ${SDL3_BINARY_DIR}/sdl3.pc DESTINATION "${SDL_PKGCONFIG_INSTALLDIR}")
  endif()

  ##### Installation targets #####()

  install(TARGETS SDL3_Headers EXPORT SDL3headersTargets)

  if(SDL_SHARED)
    install(TARGETS SDL3-shared EXPORT SDL3sharedTargets
      PUBLIC_HEADER DESTINATION "${SDL_INSTALL_HEADERSDIR}"
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
      FRAMEWORK DESTINATION "."
      RESOURCE DESTINATION "${SDL_SDL_INSTALL_RESOURCEDIR}"
    )
    if(MSVC)
      SDL_install_pdb(SDL3-shared "${CMAKE_INSTALL_BINDIR}")
    endif()
  endif()

  if(SDL_STATIC)
    install(TARGETS SDL3-static EXPORT SDL3staticTargets
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      FRAMEWORK DESTINATION "."
      RESOURCE DESTINATION "${SDL_SDLstatic_INSTALL_RESOURCEDIR}"
    )
    if(MSVC)
      SDL_install_pdb(SDL3-static "${CMAKE_INSTALL_LIBDIR}")
    endif()
  endif()

  if(SDL_TEST_LIBRARY)
    install(TARGETS SDL3_test EXPORT SDL3testTargets
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      FRAMEWORK DESTINATION "."
      RESOURCE DESTINATION "${SDL_SDLtest_INSTALL_RESOURCEDIR}"
    )
    if(MSVC)
      SDL_install_pdb(SDL3_test "${CMAKE_INSTALL_LIBDIR}")
    endif()
  endif()

  ##### Install CMake Targets #####

  install(EXPORT SDL3headersTargets
    FILE "SDL3headersTargets.cmake"
    NAMESPACE SDL3::
    DESTINATION "${SDL_SDL_INSTALL_CMAKEDIR}"
  )

  if(SDL_SHARED)
    install(EXPORT SDL3sharedTargets
      FILE "SDL3sharedTargets.cmake"
      NAMESPACE SDL3::
      DESTINATION "${SDL_SDL_INSTALL_CMAKEDIR}"
    )
  endif()

  if(SDL_STATIC)
    install(EXPORT SDL3staticTargets
      FILE "${SDL_SDLstatic_INSTALL_CMAKEFILENAME}"
      NAMESPACE SDL3::
      DESTINATION "${SDL_SDLstatic_INSTALL_CMAKEDIR}"
    )
  endif()

  if(SDL_TEST_LIBRARY)
    install(EXPORT SDL3testTargets
      FILE "${SDL_SDLtest_INSTALL_CMAKEFILENAME}"
      NAMESPACE SDL3::
      DESTINATION "${SDL_SDLtest_INSTALL_CMAKEDIR}"
    )
  endif()

  install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/SDL3Config.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/SDL3ConfigVersion.cmake
      ${sdl_cmake_modules}
    DESTINATION "${SDL_SDL_INSTALL_REAL_CMAKEDIR}"
  )

  if(NOT SDL_FRAMEWORK)
    install(FILES ${SDL3_INCLUDE_FILES}
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL3"
    )
    if(SDL_TEST_LIBRARY)
      install(FILES ${SDL3_TEST_INCLUDE_FILES}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL3"
      )
    endif()

    install(FILES "LICENSE.txt" DESTINATION "${SDL_INSTALL_LICENSEDIR}")
  endif()

  if(NOT SDL_DISABLE_INSTALL_CPACK)
    if(SDL_FRAMEWORK)
      set(CPACK_GENERATOR "DragNDrop")
    elseif(MSVC)
      set(CPACK_GENERATOR "ZIP")
    else()
      set(CPACK_GENERATOR "TGZ")
    endif()
    configure_file(cmake/CPackProjectConfig.cmake.in CPackProjectConfig.cmake @ONLY)
    set(CPACK_PROJECT_CONFIG_FILE "${SDL3_BINARY_DIR}/CPackProjectConfig.cmake")
    # CPACK_SOURCE_PACKAGE_FILE_NAME must end with "-src" (so we can block creating a source archive)
    set(CPACK_SOURCE_PACKAGE_FILE_NAME "SDL${PROJECT_VERSION_MAJOR}-${PROJECT_VERSION}-src")
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
    include(CPack)
  endif()

  if(ANDROID)
    if(TARGET SDL3-jar)
      set(SDL_INSTALL_JAVADIR "${CMAKE_INSTALL_DATAROOTDIR}/java" CACHE PATH "Path where to install java clases + java sources")
      set(PROGUARD_RULES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/android-project/app/proguard-rules.pro")
      # install_jar or $<TARGET_PROPERTY:SDL3-jar,INSTALL_FILES> does not work on Windows: a SDL3.jar symlink is not generated
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SDL3-${SDL3_VERSION}.jar"
        DESTINATION "${SDL_INSTALL_JAVADIR}/SDL3")
      install(FILES "${PROGUARD_RULES_PATH}" RENAME "proguard.txt"
        DESTINATION "${SDL_INSTALL_JAVADIR}/SDL3")
      configure_package_config_file(cmake/SDL3jarTargets.cmake.in SDL3jarTargets.cmake
        INSTALL_DESTINATION "${SDL_SDL_INSTALL_CMAKEDIR}"
        PATH_VARS SDL_INSTALL_JAVADIR
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
        INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"
      )
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SDL3jarTargets.cmake"
        DESTINATION "${SDL_SDL_INSTALL_CMAKEDIR}"
      )
    endif()
    if(TARGET SDL3-javasources)
      install(FILES  "${SDL3_BINARY_DIR}/SDL3-${SDL3_VERSION}-sources.jar"
        DESTINATION "${SDL_INSTALL_JAVADIR}/SDL3")
    endif()
  endif()

  if(NOT SDL_DISABLE_INSTALL_DOCS)
    SDL_generate_manpages(
      HEADERS_DIR "${PROJECT_SOURCE_DIR}/include/SDL3"
      SYMBOL "SDL_Init"
      WIKIHEADERS_PL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-scripts/wikiheaders.pl"
      REVISION "${SDL_REVISION}"
    )
    if(TARGET SDL3-javadoc)
      set(SDL_INSTALL_JAVADOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/javadoc" CACHE PATH "Path where to install SDL3 javadoc")
      install(DIRECTORY "${SDL3_BINARY_DIR}/docs/javadoc/"
        DESTINATION "${SDL_INSTALL_JAVADOCDIR}/SDL3")
    endif()
  endif()
endif()

##### Uninstall target #####

if(NOT SDL_DISABLE_UNINSTALL)
  if(NOT TARGET uninstall)
    configure_file(cmake/cmake_uninstall.cmake.in cmake_uninstall.cmake IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
  endif()
endif()

##### Tests subproject (must appear after the install/uninstall targets) #####

if(SDL_TESTS)
  set(HAVE_TESTS ON)
  enable_testing()
  add_subdirectory(test)
endif()

##### Examples subproject (must appear after the install/uninstall targets) #####

if(SDL_EXAMPLES)
  set(HAVE_EXAMPLES ON)
  add_subdirectory(examples)
endif()

##### Fix Objective C builds #####
string(APPEND CMAKE_OBJC_FLAGS " ${CMAKE_C_FLAGS}")

SDL_PrintSummary()
debug_show_sdl_deps()
