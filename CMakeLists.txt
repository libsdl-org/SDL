if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the SDL source code and call cmake from there")
endif()

cmake_minimum_required(VERSION 2.6)
project(SDL2 C)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceRuns)
include(CheckCCompilerFlag)
include(CheckTypeSize)
include(CheckStructHasMember)
include(CMakeDependentOption)
include(FindPkgConfig)
set(CMAKE_MODULE_PATH "${SDL2_SOURCE_DIR}/cmake")
include(${SDL2_SOURCE_DIR}/cmake/macros.cmake)
include(${SDL2_SOURCE_DIR}/cmake/sdlchecks.cmake)

# General settings
# Edit include/SDL_version.h and change the version, then:
#   SDL_MICRO_VERSION += 1;
#   SDL_INTERFACE_AGE += 1;
#   SDL_BINARY_AGE += 1;
# if any functions have been added, set SDL_INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set SDL_BINARY_AGE and SDL_INTERFACE_AGE to 0.
set(SDL_MAJOR_VERSION 2)
set(SDL_MINOR_VERSION 0)
set(SDL_MICRO_VERSION 3)
set(SDL_INTERFACE_AGE 1)
set(SDL_BINARY_AGE 3)
set(SDL_VERSION "${SDL_MAJOR_VERSION}.${SDL_MINOR_VERSION}.${SDL_MICRO_VERSION}")

# Calculate a libtool-like version number
math(EXPR LT_CURRENT "${SDL_MICRO_VERSION} - ${SDL_INTERFACE_AGE}")
math(EXPR LT_AGE "${SDL_BINARY_AGE} - ${SDL_INTERFACE_AGE}")
math(EXPR LT_MAJOR "${LT_CURRENT}- ${LT_AGE}")
set(LT_REVISION "${SDL_INTERFACE_AGE}")
set(LT_RELEASE "${SDL_MAJOR_VERSION}.${SDL_MINOR_VERSION}")
set(LT_VERSION "${LT_MAJOR}.${LT_AGE}.${LT_REVISION}")

message(STATUS "${LT_VERSION} :: ${LT_AGE} :: ${LT_REVISION} :: ${LT_CURRENT} :: ${LT_RELEASE}")

# General settings & flags
set(LIBRARY_OUTPUT_DIRECTORY "build")
# Check for 64 or 32 bit
set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH_64 TRUE)
  set(PROCESSOR_ARCH "x64")
else()
  set(ARCH_64 FALSE)
  set(PROCESSOR_ARCH "x86")
endif()
set(LIBNAME SDL2)
if(NOT LIBTYPE)
  set(LIBTYPE SHARED)
endif()

# Get the platform
if(WIN32)
  if(NOT WINDOWS)
    set(WINDOWS TRUE)
  endif()
elseif(UNIX AND NOT APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(LINUX TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*")
    set(FREEBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
    set(NETBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(OPENBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES ".*GNU.*")
    set(GNU TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES ".*BSDI.*")
    set(BSDI TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly.*|FreeBSD")
    set(FREEBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "SYSV5.*")
    set(SYSV5 TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Solaris.*")
    set(SOLARIS TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "HP-UX.*")
    set(HPUX TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "AIX.*")
    set(AIX TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Minix.*")
    set(MINIX TRUE)
  endif()
elseif(APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*")
    set(DARWIN TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
    set(MACOSX TRUE)
  endif()
  # TODO: iOS?
elseif(CMAKE_SYSTEM_NAME MATCHES "BeOS.*")
  message_error("BeOS support has been removed as of SDL 2.0.2.")
elseif(CMAKE_SYSTEM_NAME MATCHES "Haiku.*")
  set(HAIKU TRUE)
endif()

# Don't mistake osx for unix
if(UNIX AND NOT APPLE)
  set(UNIX_SYS ON)
else()
  set(UNIX_SYS OFF)
endif()

if(UNIX OR APPLE)
  set(UNIX_OR_MAC_SYS ON)
else()
  set(UNIX_OR_MAC_SYS OFF)
endif()

# Default option knobs
if(APPLE OR ARCH_64)
  set(OPT_DEF_SSEMATH ON)
endif()
if(UNIX OR MINGW OR MSYS)
  set(OPT_DEF_LIBC ON)
endif()

# Compiler info
if(CMAKE_COMPILER_IS_GNUCC)
  set(USE_GCC TRUE)
  set(OPT_DEF_ASM TRUE)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(USE_CLANG TRUE)
  set(OPT_DEF_ASM TRUE)
elseif(MSVC_VERSION GREATER 1400) # VisualStudio 8.0+
  set(OPT_DEF_ASM TRUE)
  #set(CMAKE_C_FLAGS "/ZI /WX- /
else()
  set(OPT_DEF_ASM FALSE)
endif()

# Default flags, if not set otherwise
if("$ENV{CFLAGS}" STREQUAL "")
  if(USE_GCC OR USE_CLANG)
    set(CMAKE_C_FLAGS "-g -O3")
  endif()
else("$ENV{CFLAGS}" STREQUAL "")
  set(CMAKE_C_FLAGS "$ENV{CFLAGS}")
  list(APPEND EXTRA_CFLAGS "$ENV{CFLAGS}")
endif()
if(NOT ("$ENV{CFLAGS}" STREQUAL "")) # Hackish, but does the trick on Win32
  list(APPEND EXTRA_LDFLAGS "$ENV{LDFLAGS}")
endif()

if(MSVC)
  option(FORCE_STATIC_VCRT "Force /MT for static VC runtimes" OFF)
  if(FORCE_STATIC_VCRT)
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach(flag_var)
  endif()
endif()

# Those are used for pkg-config and friends, so that the SDL2.pc, sdl2-config,
# etc. are created correctly.
set(SDL_LIBS "-lSDL2")
set(SDL_CFLAGS "")

if(CYGWIN)
  # We build SDL on cygwin without the UNIX emulation layer
  include_directories("-I/usr/include/mingw")
  set(CMAKE_REQUIRED_FLAGS "-mno-cygwin")
  check_c_source_compiles("int main(int argc, char **argv) {}"
    HAVE_GCC_NO_CYGWIN)
  set(CMAKE_REQUIRED_FLAGS)
  if(HAVE_GCC_NO_CYGWIN)
    list(APPEND EXTRA_LDFLAGS "-mno-cygwin")
    list(APPEND SDL_LIBS "-mno-cygwin")
  endif()
  set(SDL_CFLAGS "${SDL_CFLAGS} -I/usr/include/mingw")
endif()

add_definitions(-DUSING_GENERATED_CONFIG_H)
# General includes
include_directories(${SDL2_BINARY_DIR}/include ${SDL2_SOURCE_DIR}/include)

set(SDL_SUBSYSTEMS
    Atomic Audio Video Render Events Joystick Haptic Power Threads Timers
    File Loadso CPUinfo Filesystem)
foreach(_SUB ${SDL_SUBSYSTEMS})
  string(TOUPPER ${_SUB} _OPT)
  option(SDL_${_OPT} "Enable the ${_SUB} subsystem" ON)
endforeach()

option_string(ASSERTIONS "Enable internal sanity checks (auto/disabled/release/enabled/paranoid)" "auto")
#set_option(DEPENDENCY_TRACKING "Use gcc -MMD -MT dependency tracking" ON)
set_option(LIBC                "Use the system C library" ${OPT_DEF_LIBC})
set_option(GCC_ATOMICS         "Use gcc builtin atomics" ${USE_GCC})
set_option(ASSEMBLY            "Enable assembly routines" ${OPT_DEF_ASM})
set_option(SSEMATH             "Allow GCC to use SSE floating point math" ${OPT_DEF_SSEMATH})
set_option(MMX                 "Use MMX assembly routines" ${OPT_DEF_ASM})
set_option(3DNOW               "Use 3Dnow! MMX assembly routines" ${OPT_DEF_ASM})
set_option(SSE                 "Use SSE assembly routines" ${OPT_DEF_ASM})
set_option(SSE2                "Use SSE2 assembly routines" ${OPT_DEF_SSEMATH})
set_option(ALTIVEC             "Use Altivec assembly routines" ${OPT_DEF_ASM})
set_option(DISKAUDIO           "Support the disk writer audio driver" ON)
set_option(DUMMYAUDIO          "Support the dummy audio driver" ON)
set_option(VIDEO_DIRECTFB      "Use DirectFB video driver" OFF)
dep_option(DIRECTFB_SHARED     "Dynamically load directfb support" ON "VIDEO_DIRECTFB" OFF)
set_option(FUSIONSOUND         "Use FusionSound audio driver" OFF)
dep_option(FUSIONSOUND_SHARED  "Dynamically load fusionsound audio support" ON "FUSIONSOUND_SHARED" OFF)
set_option(VIDEO_DUMMY         "Use dummy video driver" ON)
set_option(VIDEO_OPENGL        "Include OpenGL support" ON)
set_option(VIDEO_OPENGLES      "Include OpenGL ES support" ON)
set_option(PTHREADS            "Use POSIX threads for multi-threading" ${UNIX_OR_MAC_SYS})
dep_option(PTHREADS_SEM        "Use pthread semaphores" ON "PTHREADS" OFF)
set_option(SDL_DLOPEN          "Use dlopen for shared object loading" ON)
set_option(OSS                 "Support the OSS audio API" ${UNIX_SYS})
set_option(ALSA                "Support the ALSA audio API" ${UNIX_SYS})
dep_option(ALSA_SHARED         "Dynamically load ALSA audio support" ON "ALSA" OFF)
set_option(ESD                 "Support the Enlightened Sound Daemon" ${UNIX_SYS})
dep_option(ESD_SHARED          "Dynamically load ESD audio support" ON "ESD" OFF)
set_option(PULSEAUDIO          "Use PulseAudio" ${UNIX_SYS})
dep_option(PULSEAUDIO_SHARED   "Dynamically load PulseAudio support" ON "PULSEAUDIO" OFF)
set_option(ARTS                "Support the Analog Real Time Synthesizer" ${UNIX_SYS})
dep_option(ARTS_SHARED         "Dynamically load aRts audio support" ON "ARTS" OFF)
set_option(NAS                 "Support the NAS audio API" ${UNIX_SYS})
set_option(NAS_SHARED          "Dynamically load NAS audio API" ${UNIX_SYS})
set_option(SNDIO               "Support the sndio audio API" ${UNIX_SYS})
set_option(RPATH               "Use an rpath when linking SDL" ${UNIX_SYS})
set_option(CLOCK_GETTIME       "Use clock_gettime() instead of gettimeofday()" OFF)
set_option(INPUT_TSLIB         "Use the Touchscreen library for input" ${UNIX_SYS})
set_option(VIDEO_X11           "Use X11 video driver" ${UNIX_SYS})
set_option(VIDEO_WAYLAND       "Use Wayland video driver" OFF) #${UNIX_SYS})
set_option(VIDEO_MIR           "Use Mir video driver" OFF) #${UNIX_SYS})
dep_option(X11_SHARED          "Dynamically load X11 support" ON "VIDEO_X11" OFF)
set(SDL_X11_OPTIONS Xcursor Xinerama XInput Xrandr Xscrnsaver XShape Xvm)
foreach(_SUB ${SDL_X11_OPTIONS})
  string(TOUPPER "VIDEO_X11_${_SUB}" _OPT)
  dep_option(${_OPT}           "Enable ${_SUB} support" ON "VIDEO_X11" OFF)
endforeach()
set_option(VIDEO_COCOA         "Use Cocoa video driver" ${APPLE})
set_option(DIRECTX             "Use DirectX for Windows audio/video" ${WINDOWS})
set_option(RENDER_D3D          "Enable the Direct3D render driver" ${WINDOWS})

# TODO: We should (should we?) respect cmake's ${BUILD_SHARED_LIBS} flag here
# The options below are for compatibility to configure's default behaviour.
set(SDL_SHARED ON CACHE BOOL "Build a shared version of the library")
set(SDL_STATIC ON CACHE BOOL "Build a static version of the library")

# General source files
file(GLOB SOURCE_FILES
  ${SDL2_SOURCE_DIR}/src/*.c
  ${SDL2_SOURCE_DIR}/src/atomic/*.c
  ${SDL2_SOURCE_DIR}/src/audio/*.c
  ${SDL2_SOURCE_DIR}/src/cpuinfo/*.c
  ${SDL2_SOURCE_DIR}/src/dynapi/*.c
  ${SDL2_SOURCE_DIR}/src/events/*.c
  ${SDL2_SOURCE_DIR}/src/file/*.c
  ${SDL2_SOURCE_DIR}/src/libm/*.c
  ${SDL2_SOURCE_DIR}/src/render/*.c
  ${SDL2_SOURCE_DIR}/src/render/*/*.c
  ${SDL2_SOURCE_DIR}/src/stdlib/*.c
  ${SDL2_SOURCE_DIR}/src/thread/*.c
  ${SDL2_SOURCE_DIR}/src/timer/*.c
  ${SDL2_SOURCE_DIR}/src/video/*.c)


if(ASSERTIONS STREQUAL "auto")
  # Do nada - use optimization settings to determine the assertion level
elseif(ASSERTIONS STREQUAL "disabled")
  set(SDL_DEFAULT_ASSERT_LEVEL 0)
elseif(ASSERTIONS STREQUAL "release")
  set(SDL_DEFAULT_ASSERT_LEVEL 1)
elseif(ASSERTIONS STREQUAL "enabled")
  set(SDL_DEFAULT_ASSERT_LEVEL 2)
elseif(ASSERTIONS STREQUAL "paranoid")
  set(SDL_DEFAULT_ASSERT_LEVEL 3)
else()
  message_error("unknown assertion level")
endif()
set(HAVE_ASSERTIONS ${ASSERTIONS})

# Compiler option evaluation
if(USE_GCC OR USE_CLANG)
  if(DEPENDENCY_TRACKING)
    check_c_source_compiles("
        #if !defined(__GNUC__) || __GNUC__ < 3
        #error Dependency tracking requires GCC 3.0 or newer
        #endif
        int main(int argc, char **argv) { }" HAVE_DEPENDENCY_TRACKING)
  endif()

  if(GCC_ATOMICS)
    check_c_source_compiles("int main(int argc, char **argv) {
        int a;
        void *x, *y, *z;
        __sync_lock_test_and_set(&a, 4);
        __sync_lock_test_and_set(&x, y);
        __sync_fetch_and_add(&a, 1);
        __sync_bool_compare_and_swap(&a, 5, 10);
        __sync_bool_compare_and_swap(&x, y, z); }" HAVE_GCC_ATOMICS)
    if(NOT HAVE_GCC_ATOMICS)
      check_c_source_compiles("int main(int argc, char **argv) {
          int a;
          __sync_lock_test_and_set(&a, 1);
          __sync_lock_release(&a); }" HAVE_GCC_SYNC_LOCK_TEST_AND_SET)
    endif()
  endif()

  set(CMAKE_REQUIRED_FLAGS "-mpreferred-stack-boundary=2")
  check_c_source_compiles("int x = 0; int main(int argc, char **argv) {}"
    HAVE_GCC_PREFERRED_STACK_BOUNDARY)
  set(CMAKE_REQUIRED_FLAGS)

  set(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden -Werror")
  check_c_source_compiles("
      #if !defined(__GNUC__) || __GNUC__ < 4
      #error SDL only uses visibility attributes in GCC 4 or newer
      #endif
      int main(int argc, char **argv) {}" HAVE_GCC_FVISIBILITY)
  if(HAVE_GCC_FVISIBILITY)
    list(APPEND EXTRA_CFLAGS "-fvisibility=hidden")
  endif()
  set(CMAKE_REQUIRED_FLAGS)

  check_c_compiler_flag(-Wall HAVE_GCC_WALL)
  if(HAVE_GCC_WALL)
    if(HAIKU)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-multichar")
    endif()
  endif()
endif()

if(ASSEMBLY)
  if(USE_GCC OR USE_CLANG)
    set(SDL_ASSEMBLY_ROUTINES 1)
    # TODO: Those all seem to be quite GCC specific - needs to be
    # reworked for better compiler support
    set(HAVE_ASSEMBLY TRUE)
    if(MMX)
      set(CMAKE_REQUIRED_FLAGS "-mmmx")
      check_c_source_compiles("
          #ifdef __MINGW32__
          #include <_mingw.h>
          #ifdef __MINGW64_VERSION_MAJOR
          #include <intrin.h>
          #else
          #include <mmintrin.h>
          #endif
          #else
          #include <mmintrin.h>
          #endif
          #ifndef __MMX__
          #error Assembler CPP flag not enabled
          #endif
          int main(int argc, char **argv) { }" HAVE_MMX)
      if(HAVE_MMX)
        list(APPEND EXTRA_CFLAGS "-mmmx")
      endif()
      set(CMAKE_REQUIRED_FLAGS)
    endif()

    if(3DNOW)
      set(CMAKE_REQUIRED_FLAGS "-m3dnow")
      check_c_source_compiles("
          #include <mm3dnow.h>
          #ifndef __3dNOW__
          #error Assembler CPP flag not enabled
          #endif
          int main(int argc, char **argv) {
            void *p = 0;
            _m_prefetch(p);
          }" HAVE_3DNOW)
      if(HAVE_3DNOW)
        list(APPEND EXTRA_CFLAGS "-m3dnow")
      endif()
      set(CMAKE_REQUIRED_FLAGS)
    endif()

    if(SSE)
      set(CMAKE_REQUIRED_FLAGS "-msse")
      check_c_source_compiles("
          #ifdef __MINGW32__
          #include <_mingw.h>
          #ifdef __MINGW64_VERSION_MAJOR
          #include <intrin.h>
          #else
          #include <xmmintrin.h>
          #endif
          #else
          #include <xmmintrin.h>
          #endif
          #ifndef __SSE__
          #error Assembler CPP flag not enabled
          #endif
          int main(int argc, char **argv) { }" HAVE_SSE)
      if(HAVE_SSE)
        list(APPEND EXTRA_CFLAGS "-msse")
      endif()
      set(CMAKE_REQUIRED_FLAGS)
    endif()

    if(SSE2)
      set(CMAKE_REQUIRED_FLAGS "-msse2")
      check_c_source_compiles("
          #ifdef __MINGW32__
          #include <_mingw.h>
          #ifdef __MINGW64_VERSION_MAJOR
          #include <intrin.h>
          #else
          #include <emmintrin.h>
          #endif
          #else
          #include <emmintrin.h>
          #endif
          #ifndef __SSE2__
          #error Assembler CPP flag not enabled
          #endif
          int main(int argc, char **argv) { }" HAVE_SSE2)
      if(HAVE_SSE2)
        list(APPEND EXTRA_CFLAGS "-msse2")
      endif()
      set(CMAKE_REQUIRED_FLAGS)
    endif()

    if(SSEMATH)
      if(SSE OR SSE2)
        if(USE_GCC)
          list(APPEND EXTRA_CFLAGS "-mfpmath=387")
        endif()
        set(HAVE_SSEMATH TRUE)
      endif()
    endif()

    if(ALTIVEC)
      set(CMAKE_REQUIRED_FLAGS "-maltivec")
      check_c_source_compiles("
          #include <altivec.h>
          vector unsigned int vzero() {
              return vec_splat_u32(0);
          }
          int main(int argc, char **argv) { }" HAVE_ALTIVEC_H_HDR)
      check_c_source_compiles("
          vector unsigned int vzero() {
              return vec_splat_u32(0);
          }
          int main(int argc, char **argv) { }" HAVE_ALTIVEC)
      set(CMAKE_REQUIRED_FLAGS)
      if(HAVE_ALTIVEC OR HAVE_ALTIVEC_H_HDR)
        set(HAVE_ALTIVEC TRUE) # if only HAVE_ALTIVEC_H_HDR is set
        list(APPEND EXTRA_CFLAGS "-maltivec")
        set(SDL_ALTIVEC_BLITTERS 1)
        if(HAVE_ALTIVEC_H_HDR)
          set(HAVE_ALTIVEC_H 1)
        endif()
      endif()
    endif()
  elseif(MSVC_VERSION GREATER 1500)
    # TODO: SDL_cpuinfo.h needs to support the user's configuration wish
    # for MSVC - right now it is always activated
    if(NOT ARCH_64)
      set(HAVE_MMX TRUE)
      set(HAVE_3DNOW TRUE)
    endif()
    set(HAVE_SSE TRUE)
    set(HAVE_SSE2 TRUE)
    set(SDL_ASSEMBLY_ROUTINES 1)
  endif()
# TODO:
#else(ASSEMBLY)
#  if(USE_GCC OR USE_CLANG)
#    list(APPEND EXTRA_CFLAGS "-mno-sse" "-mno-sse2" "-mno-mmx")
#  endif()
endif()

# TODO: Can't deactivate on FreeBSD? w/o LIBC, SDL_stdinc.h can't define
# anything.
if(LIBC)
  if(WINDOWS AND NOT MINGW)
    set(HAVE_LIBC TRUE)
    foreach(_HEADER stdio.h string.h ctype.h math.h)
      string(TOUPPER "HAVE_${_HEADER}" _UPPER)
      string(REPLACE "." "_" _HAVE_H ${_UPPER})
      set(${_HAVE_H} 1)
    endforeach()
    set(HAVE_SIGNAL_H 1)
    foreach(_FN
            malloc calloc realloc free qsort abs memset memcpy memmove memcmp
            strlen _strrev _strupr _strlwr strchr strrchr strstr itoa _ltoa
            _ultoa strtol strtoul strtoll strtod atoi atof strcmp strncmp
            _stricmp _strnicmp sscanf atan atan2 acos asin ceil copysign cos
            cosf fabs floor log pow scalbn sin sinf sqrt)
      string(TOUPPER ${_FN} _UPPER)
      set(HAVE_${_UPPER} 1)
    endforeach()
    if(NOT CYGWIN AND NOT MINGW)
      set(HAVE_ALLOCA 1)
    endif()
    set(HAVE_M_PI 1)
    add_definitions(-D_USE_MATH_DEFINES) # needed for M_PI
    set(STDC_HEADERS 1)
  else(WINDOWS AND NOT MINGW)
    set(HAVE_LIBC TRUE)
    check_include_file(sys/types.h HAVE_SYS_TYPES_H)
    foreach(_HEADER
            stdio.h stdlib.h stddef.h stdarg.h malloc.h memory.h string.h
            strings.h inttypes.h stdint.h ctype.h math.h iconv.h signal.h)
      string(TOUPPER "HAVE_${_HEADER}" _UPPER)
      string(REPLACE "." "_" _HAVE_H ${_UPPER})
      check_include_file("${_HEADER}" ${_HAVE_H})
    endforeach()

    check_include_files("dlfcn.h;stdint.h;stddef.h;inttypes.h;stdlib.h;strings.h;string.h;float.h" STDC_HEADERS)
    check_type_size("size_t" SIZEOF_SIZE_T)
    check_symbol_exists(M_PI math.h HAVE_M_PI)
    # TODO: refine the mprotect check
    check_c_source_compiles("#include <sys/types.h>
                             #include <sys/mman.h>
                             int main() { }" HAVE_MPROTECT)
    foreach(_FN
            strtod malloc calloc realloc free getenv setenv putenv unsetenv
            qsort abs bcopy memset memcpy memmove memcmp strlen strlcpy strlcat
            strdup _strrev _strupr _strlwr strchr strrchr strstr itoa _ltoa
            _uitoa _ultoa strtol strtoul _i64toa _ui64toa strtoll strtoull
            atoi atof strcmp strncmp _stricmp strcasecmp _strnicmp strncasecmp
            vsscanf vsnprintf fseeko fseeko64 sigaction setjmp
            nanosleep sysconf sysctlbyname
            )
      string(TOUPPER ${_FN} _UPPER)
      set(_HAVEVAR "HAVE_${_UPPER}")
      check_function_exists("${_FN}" ${_HAVEVAR})
    endforeach()

    check_library_exists(m pow "" HAVE_LIBM)
    if(HAVE_LIBM)
      set(CMAKE_REQUIRED_LIBRARIES m)
      foreach(_FN
              atan atan2 ceil copysign cos cosf fabs floor log pow scalbn sin
              sinf sqrt)
        string(TOUPPER ${_FN} _UPPER)
        set(_HAVEVAR "HAVE_${_UPPER}")
        check_function_exists("${_FN}" ${_HAVEVAR})
      endforeach()
      set(CMAKE_REQUIRED_LIBRARIES)
      list(APPEND EXTRA_LIBS m)
    endif()

    check_library_exists(iconv iconv_open "" HAVE_LIBICONV)
    if(HAVE_LIBICONV)
      list(APPEND EXTRA_LIBS iconv)
    endif()

    check_struct_has_member("struct sigaction" "sa_sigaction" "signal.h" HAVE_SA_SIGACTION)
  endif()
else(LIBC)
  if(WINDOWS)
    set(HAVE_STDARG_H 1)
    set(HAVE_STDDEF_H 1)
  endif()
endif()


# Enable/disable various subsystems of the SDL library
foreach(_SUB ${SDL_SUBSYSTEMS})
  string(TOUPPER ${_SUB} _OPT)
  if(NOT SDL_${_OPT})
    set(SDL_${_OPT}_DISABLED 1)
  endif()
endforeach()
if(SDL_JOYSTICK)
  file(GLOB JOYSTICK_SOURCES ${SDL2_SOURCE_DIR}/src/joystick/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${JOYSTICK_SOURCES})
endif()
if(SDL_HAPTIC)
  if(NOT SDL_JOYSTICK)
    # Haptic requires some private functions from the joystick subsystem.
    message_error("SDL_HAPTIC requires SDL_JOYSTICK, which is not enabled")
  endif()
  file(GLOB HAPTIC_SOURCES ${SDL2_SOURCE_DIR}/src/haptic/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${HAPTIC_SOURCES})
endif()
if(SDL_POWER)
  file(GLOB POWER_SOURCES ${SDL2_SOURCE_DIR}/src/power/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${POWER_SOURCES})
endif()
# TODO: in configure.in, the test for LOADSO and SDL_DLOPEN is a bit weird:
# if LOADSO is not wanted, SDL_LOADSO_DISABLED is set
# If however on Unix or APPLE dlopen() is detected via CheckDLOPEN(),
# SDL_LOADSO_DISABLED will not be set, regardless of the LOADSO settings

# General SDL subsystem options, valid for all platforms
if(SDL_AUDIO)
  # CheckDummyAudio/CheckDiskAudio - valid for all platforms
  if(DUMMYAUDIO)
    set(SDL_AUDIO_DRIVER_DUMMY 1)
    file(GLOB DUMMYAUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/dummy/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${DUMMYAUDIO_SOURCES})
    set(HAVE_DUMMYAUDIO TRUE)
  endif()
  if(DISKAUDIO)
    set(SDL_AUDIO_DRIVER_DISK 1)
    file(GLOB DISKAUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/disk/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${DISKAUDIO_SOURCES})
    set(HAVE_DISKAUDIO TRUE)
  endif()
endif()

if(SDL_DLOPEN)
  # Relevant for Unix/Darwin only
  if(UNIX OR APPLE)
    CheckDLOPEN()
  endif()
endif()

if(SDL_VIDEO)
  if(VIDEO_DUMMY)
    set(SDL_VIDEO_DRIVER_DUMMY 1)
    file(GLOB VIDEO_DUMMY_SOURCES ${SDL2_SOURCE_DIR}/src/video/dummy/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${VIDEO_DUMMY_SOURCES})
    set(HAVE_VIDEO_DUMMY TRUE)
    set(HAVE_SDL_VIDEO TRUE)
  endif()
endif()

# Platform-specific options and settings
if(UNIX AND NOT APPLE)
  if(SDL_AUDIO)
    if(SYSV5 OR SOLARIS OR HPUX)
        set(SDL_AUDIO_DRIVER_SUNAUDIO 1)
        file(GLOB SUN_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/sun/*.c)
        set(SOURCE_FILES ${SOURCE_FILES} ${SUN_AUDIO_SOURCES})
        set(HAVE_SDL_AUDIO TRUE)
    elseif(NETBSD OR OPENBSD)
        set(SDL_AUDIO_DRIVER_BSD 1)
        file(GLOB BSD_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/bsd/*.c)
        set(SOURCE_FILES ${SOURCE_FILES} ${BSD_AUDIO_SOURCES})
        set(HAVE_SDL_AUDIO TRUE)
    elseif(AIX)
        set(SDL_AUDIO_DRIVER_PAUDIO 1)
        file(GLOB AIX_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/paudio/*.c)
        set(SOURCE_FILES ${SOURCE_FILES} ${AIX_AUDIO_SOURCES})
        set(HAVE_SDL_AUDIO TRUE)
    endif()
    CheckOSS()
    CheckALSA()
    CheckPulseAudio()
    CheckESD()
    CheckARTS()
    CheckNAS()
    CheckSNDIO()
    CheckFusionSound()
  endif()

  if(SDL_VIDEO)
    CheckX11()
    CheckMir()
    CheckDirectFB()
    CheckOpenGLX11()
    CheckOpenGLESX11()
    CheckWayland()
  endif()

  if(LINUX)
    check_c_source_compiles("
        #include <linux/input.h>
        #ifndef EVIOCGNAME
        #error EVIOCGNAME() ioctl not available
        #endif
        int main(int argc, char** argv) {}" HAVE_INPUT_EVENTS)

    check_c_source_compiles("
        #include <linux/kd.h>
        #include <linux/keyboard.h>

        int main(int argc, char **argv) 
        {
            struct kbentry kbe;
            kbe.kb_table = KG_CTRL;
            ioctl(0, KDGKBENT, &kbe);
        }" HAVE_INPUT_KD)

    file(GLOB CORE_SOURCES ${SDL2_SOURCE_DIR}/src/core/linux/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${CORE_SOURCES})

    if(HAVE_INPUT_EVENTS)
      set(SDL_INPUT_LINUXEV 1)
    endif()

    if(SDL_HAPTIC AND HAVE_INPUT_EVENTS)
      set(SDL_HAPTIC_LINUX 1)
      file(GLOB HAPTIC_SOURCES ${SDL2_SOURCE_DIR}/src/haptic/linux/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${HAPTIC_SOURCES})
      set(HAVE_SDL_HAPTIC TRUE)
    endif()

    if(HAVE_INPUT_KD)
      set(SDL_INPUT_LINUXKD 1)
    endif()

    check_include_file("libudev.h" HAVE_LIBUDEV_H)

    # !!! FIXME: this needs pkg-config to find the include path, I think.
    check_include_file("dbus/dbus.h" HAVE_DBUS_DBUS_H)
  endif()

  if(INPUT_TSLIB)
    check_c_source_compiles("
        #include \"tslib.h\"
        int main(int argc, char** argv) { }" HAVE_INPUT_TSLIB)
    if(HAVE_INPUT_TSLIB)
      set(SDL_INPUT_TSLIB 1)
      list(APPEND EXTRA_LIBS ts)
    endif()
  endif()

  if(SDL_JOYSTICK)
    CheckUSBHID()   # seems to be BSD specific - limit the test to BSD only?
    if(LINUX)
      set(SDL_JOYSTICK_LINUX 1)
      file(GLOB JOYSTICK_SOURCES ${SDL2_SOURCE_DIR}/src/joystick/linux/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${JOYSTICK_SOURCES})
      set(HAVE_SDL_JOYSTICK TRUE)
    endif()
  endif()

  CheckPTHREAD()

  if(CLOCK_GETTIME)
    check_library_exists(rt clock_gettime "" FOUND_CLOCK_GETTIME)
    if(FOUND_CLOCK_GETTIME)
      list(APPEND EXTRA_LIBS rt)
      set(HAVE_CLOCK_GETTIME 1)
    else(FOUND_CLOCK_GETTIME)
      check_library_exists(c clock_gettime "" FOUND_CLOCK_GETTIME)
      if(FOUND_CLOCK_GETTIME)
        set(HAVE_CLOCK_GETTIME 1)
      endif()
    endif()
  endif()

  check_include_file(linux/version.h HAVE_LINUX_VERSION_H)
  if(HAVE_LINUX_VERSION_H)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_LINUX_VERSION_H")
  endif()

  if(SDL_POWER)
    if(LINUX)
      set(SDL_POWER_LINUX 1)
      file(GLOB POWER_SOURCES ${SDL2_SOURCE_DIR}/src/power/linux/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${POWER_SOURCES})
      set(HAVE_SDL_POWER TRUE)
    endif()
  endif()

  if(SDL_FILESYSTEM)
    set(SDL_FILESYSTEM_UNIX 1)
    file(GLOB FILESYSTEM_SOURCES ${SDL2_SOURCE_DIR}/src/filesystem/unix/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${FILESYSTEM_SOURCES})
    set(HAVE_SDL_FILESYSTEM TRUE)
  endif()

  if(SDL_TIMERS)
    set(SDL_TIMER_UNIX 1)
    file(GLOB TIMER_SOURCES ${SDL2_SOURCE_DIR}/src/timer/unix/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${TIMER_SOURCES})
    set(HAVE_SDL_TIMERS TRUE)
  endif()

  if(RPATH)
    set(SDL_RLD_FLAGS "")
    if(BSDI OR FREEBSD OR LINUX OR NETBSD)
      set(SDL_RLD_FLAGS "-Wl,-rpath,\${libdir}")
    elseif(SOLARIS)
      set(SDL_RLD_FLAGS "-R\${libdir}")
    endif()
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(HAVE_RPATH TRUE)
  endif()

elseif(WINDOWS)
  find_program(WINDRES windres)

  check_c_source_compiles("
    #include <windows.h>
    int main(int argc, char **argv) { }" HAVE_WIN32_CC)

  file(GLOB CORE_SOURCES ${SDL2_SOURCE_DIR}/src/core/windows/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${CORE_SOURCES})

  # Check for DirectX
  if(DIRECTX)
    if("$ENV{DXSDK_DIR}" STREQUAL "")
      message_error("DIRECTX requires the \$DXSDK_DIR environment variable to be set")
    endif()
    set(CMAKE_REQUIRED_FLAGS "/I\"$ENV{DXSDK_DIR}\\Include\"")
    check_include_file(d3d9.h HAVE_D3D_H)
    check_include_file(d3d11_1.h HAVE_D3D11_H)
    check_include_file(ddraw.h HAVE_DDRAW_H)
    check_include_file(dsound.h HAVE_DSOUND_H)
    check_include_file(dinput.h HAVE_DINPUT_H)
    check_include_file(xaudio2.h HAVE_XAUDIO2_H)
    if(HAVE_D3D_H OR HAVE_D3D11_H OR HAVE_DDRAW_H OR HAVE_DSOUND_H OR HAVE_DINPUT_H OR HAVE_XAUDIO2_H)
      set(HAVE_DIRECTX TRUE)
      # TODO: change $ENV{DXSDL_DIR} to get the path from the include checks
      link_directories($ENV{DXSDK_DIR}\\lib\\${PROCESSOR_ARCH})
      include_directories($ENV{DXSDK_DIR}\\Include)
    endif()
    set(CMAKE_REQUIRED_FLAGS)
  endif()

  if(SDL_AUDIO)
    set(SDL_AUDIO_DRIVER_WINMM 1)
    file(GLOB WINMM_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/winmm/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${WINMM_AUDIO_SOURCES})
    set(HAVE_SDL_AUDIO TRUE)

    if(HAVE_DSOUND_H)
      set(SDL_AUDIO_DRIVER_DSOUND 1)
      file(GLOB DSOUND_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/directsound/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${DSOUND_AUDIO_SOURCES})
    endif()

    if(HAVE_XAUDIO2_H)
      set(SDL_AUDIO_DRIVER_XAUDIO2 1)
      file(GLOB XAUDIO2_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/xaudio2/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${XAUDIO2_AUDIO_SOURCES})
    endif()
  endif()

  if(SDL_VIDEO)
    # requires SDL_LOADSO on Windows (IME, DX, etc.)
    if(NOT SDL_LOADSO)
      message_error("SDL_VIDEO requires SDL_LOADSO, which is not enabled")
    endif()
    set(SDL_VIDEO_DRIVER_WINDOWS 1)
    file(GLOB WIN_VIDEO_SOURCES ${SDL2_SOURCE_DIR}/src/video/windows/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${WIN_VIDEO_SOURCES})

    if(RENDER_D3D AND HAVE_D3D_H)
      set(SDL_VIDEO_RENDER_D3D 1)
      set(HAVE_RENDER_D3D TRUE)
    endif()
    if(RENDER_D3D AND HAVE_D3D11_H)
      set(SDL_VIDEO_RENDER_D3D11 1)
      set(HAVE_RENDER_D3D TRUE)
    endif()
    set(HAVE_SDL_VIDEO TRUE)
  endif()

  if(SDL_THREADS)
    set(SDL_THREAD_WINDOWS 1)
    set(SOURCE_FILES ${SOURCE_FILES}
      ${SDL2_SOURCE_DIR}/src/thread/windows/SDL_sysmutex.c
      ${SDL2_SOURCE_DIR}/src/thread/windows/SDL_syssem.c
      ${SDL2_SOURCE_DIR}/src/thread/windows/SDL_systhread.c
      ${SDL2_SOURCE_DIR}/src/thread/windows/SDL_systls.c
      ${SDL2_SOURCE_DIR}/src/thread/generic/SDL_syscond.c)
    set(HAVE_SDL_THREADS TRUE)
  endif()

  if(SDL_POWER)
    set(SDL_POWER_WINDOWS 1)
    set(SOURCE_FILES ${SOURCE_FILES} ${SDL2_SOURCE_DIR}/src/power/windows/SDL_syspower.c)
    set(HAVE_SDL_POWER TRUE)
  endif()

  if(SDL_FILESYSTEM)
    set(SDL_FILESYSTEM_WINDOWS 1)
    file(GLOB FILESYSTEM_SOURCES ${SDL2_SOURCE_DIR}/src/filesystem/windows/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${FILESYSTEM_SOURCES})
    set(HAVE_SDL_FILESYSTEM TRUE)
  endif()

  # Libraries for Win32 native and MinGW
  list(APPEND EXTRA_LIBS user32 gdi32 winmm imm32 ole32 oleaut32 version uuid)

  # TODO: in configure.in the check for timers is set on
  # cygwin | mingw32* - does this include mingw32CE?
  if(SDL_TIMERS)
    set(SDL_TIMER_WINDOWS 1)
    file(GLOB TIMER_SOURCES ${SDL2_SOURCE_DIR}/src/timer/windows/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${TIMER_SOURCES})
    set(HAVE_SDL_TIMERS TRUE)
  endif()

  if(SDL_LOADSO)
    set(SDL_LOADSO_WINDOWS 1)
    file(GLOB LOADSO_SOURCES ${SDL2_SOURCE_DIR}/src/loadso/windows/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${LOADSO_SOURCES})
    set(HAVE_SDL_LOADSO TRUE)
  endif()

  file(GLOB CORE_SOURCES ${SDL2_SOURCE_DIR}/src/core/windows/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${CORE_SOURCES})

  if(SDL_VIDEO)
    if(VIDEO_OPENGL)
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_OPENGL_WGL 1)
      set(SDL_VIDEO_RENDER_OGL 1)
      set(HAVE_VIDEO_OPENGL TRUE)
    endif()
  endif()

  if(SDL_JOYSTICK)
    if(HAVE_DINPUT_H)
      set(SDL_JOYSTICK_DINPUT 1)
      set(SOURCE_FILES ${SOURCE_FILES} ${SDL2_SOURCE_DIR}/src/joystick/windows/SDL_dxjoystick.c)
      list(APPEND EXTRA_LIBS dinput8 dxguid dxerr)
    else()
      set(SDL_JOYSTICK_WINMM 1)
      set(SOURCE_FILES ${SOURCE_FILES} ${SDL2_SOURCE_DIR}/src/joystick/windows/SDL_mmjoystick.c)
    endif()
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()

  if(SDL_HAPTIC AND HAVE_DINPUT_H)
    set(SDL_HAPTIC_DINPUT 1)
    set(SOURCE_FILES ${SOURCE_FILES} ${SDL2_SOURCE_DIR}/src/haptic/windows/SDL_syshaptic.c)
    set(HAVE_SDL_HAPTIC TRUE)
  endif()

  file(GLOB VERSION_SOURCES ${SDL2_SOURCE_DIR}/src/main/windows/*.rc)
  file(GLOB SDLMAIN_SOURCES ${SDL2_SOURCE_DIR}/src/main/windows/*.c)
  if(MINGW OR CYGWIN)
    list(APPEND EXTRA_LIBS mingw32)
    list(APPEND EXTRA_LDFLAGS "-mwindows")
    set(SDL_CFLAGS "${SDL_CFLAGS} -Dmain=SDL_main")
    list(APPEND SDL_LIBS "-lmingw32" "-lSDL2main" "-mwindows")
  endif()
elseif(APPLE)
  # TODO: rework this for proper MacOS X, iOS and Darwin support

  # Requires the darwin file implementation
  if(SDL_FILE)
    file(GLOB EXTRA_SOURCES ${PROJECT_SOURCE_DIR}/src/file/cocoa/*.m)
    set(SOURCE_FILES ${EXTRA_SOURCES} ${SOURCE_FILES})
    set_source_files_properties(${EXTRA_SOURCES} PROPERTIES LANGUAGE C)
    set(HAVE_SDL_FILE TRUE)
    set(SDL_FRAMEWORK_COCOA 1)
  else()
    message_error("SDL_FILE must be enabled to build on MacOS X")
  endif()

  if(SDL_AUDIO)
    set(MACOSX_COREAUDIO 1)
    file(GLOB AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/coreaudio/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${AUDIO_SOURCES})
    set(HAVE_SDL_AUDIO TRUE)
    set(SDL_FRAMEWORK_COREAUDIO 1)
    set(SDL_FRAMEWORK_AUDIOUNIT 1)
  endif()

  if(SDL_JOYSTICK)
    set(SDL_JOYSTICK_IOKIT 1)
    file(GLOB JOYSTICK_SOURCES ${SDL2_SOURCE_DIR}/src/joystick/darwin/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${JOYSTICK_SOURCES})
    set(HAVE_SDL_JOYSTICK TRUE)
    set(SDL_FRAMEWORK_IOKIT 1)
    set(SDL_FRAMEWORK_FF 1)
  endif()

  if(SDL_HAPTIC)
    set(SDL_HAPTIC_IOKIT 1)
    file(GLOB HAPTIC_SOURCES ${SDL2_SOURCE_DIR}/src/haptic/darwin/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${HAPTIC_SOURCES})
    set(HAVE_SDL_HAPTIC TRUE)
    set(SDL_FRAMEWORK_IOKIT 1)
    set(SDL_FRAMEWORK_FF 1)
    if(NOT SDL_JOYSTICK)
      message(FATAL_ERROR "SDL_HAPTIC requires SDL_JOYSTICK to be enabled")
    endif()
  endif()

  if(SDL_POWER)
    set(SDL_POWER_MACOSX 1)
    file(GLOB POWER_SOURCES ${SDL2_SOURCE_DIR}/src/power/macosx/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${POWER_SOURCES})
    set(HAVE_SDL_POWER TRUE)
    set(SDL_FRAMEWORK_CARBON 1)
    set(SDL_FRAMEWORK_IOKIT 1)
  endif()

  if(SDL_TIMERS)
    set(SDL_TIMER_UNIX 1)
    file(GLOB TIMER_SOURCES ${SDL2_SOURCE_DIR}/src/timer/unix/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${TIMER_SOURCES})
    set(HAVE_SDL_TIMERS TRUE)
  endif(SDL_TIMERS)

  if(SDL_FILESYSTEM)
    set(SDL_FILESYSTEM_COCOA 1)
    file(GLOB FILESYSTEM_SOURCES ${SDL2_SOURCE_DIR}/src/filesystem/cocoa/*.m)
    set_source_files_properties(${FILESYSTEM_SOURCES} PROPERTIES LANGUAGE C)
    set(SOURCE_FILES ${SOURCE_FILES} ${FILESYSTEM_SOURCES})
    set(HAVE_SDL_FILESYSTEM TRUE)
  endif()

  # Actually load the frameworks at the end so we don't duplicate include.
  if(SDL_FRAMEWORK_COCOA)
    find_library(COCOA_LIBRARY Cocoa)
    list(APPEND EXTRA_LIBS ${COCOA_LIBRARY})
  endif()
  if(SDL_FRAMEWORK_IOKIT)
    find_library(IOKIT IOKit)
    list(APPEND EXTRA_LIBS ${IOKIT})
  endif()
  if(SDL_FRAMEWORK_FF)
    find_library(FORCEFEEDBACK ForceFeedback)
    list(APPEND EXTRA_LIBS ${FORCEFEEDBACK})
  endif()
  if(SDL_FRAMEWORK_CARBON)
    find_library(CARBON_LIBRARY Carbon)
    list(APPEND EXTRA_LIBS ${CARBON_LIBRARY})
  endif()
  if(SDL_FRAMEWORK_COREAUDIO)
    find_library(COREAUDIO CoreAudio)
    list(APPEND EXTRA_LIBS ${COREAUDIO})
  endif()
  if(SDL_FRAMEWORK_AUDIOUNIT)
    find_library(AUDIOUNIT AudioUnit)
    list(APPEND EXTRA_LIBS ${AUDIOUNIT})
  endif()

  # iOS hack needed - http://code.google.com/p/ios-cmake/ ?
  if(SDL_VIDEO)
    CheckCOCOA()
    if(VIDEO_OPENGL)
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_OPENGL_CGL 1)
      set(SDL_VIDEO_RENDER_OGL 1)
      if(DARWIN)
        find_library(OpenGL_LIBRARY OpenGL)
        list(APPEND EXTRA_LIBRARIES ${OpenGL_LIBRARY})
      endif()
      set(HAVE_VIDEO_OPENGL TRUE)
    endif()
  endif()

  CheckPTHREAD()
elseif(HAIKU)
  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_HAIKU 1)
    file(GLOB HAIKUVIDEO_SOURCES ${SDL2_SOURCE_DIR}/src/video/haiku/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${HAIKUVIDEO_SOURCES})
    set(HAVE_SDL_VIDEO TRUE)

    set(SDL_FILESYSTEM_HAIKU 1)
    file(GLOB FILESYSTEM_SOURCES ${SDL2_SOURCE_DIR}/src/filesystem/haiku/*.cc)
    set(SOURCE_FILES ${SOURCE_FILES} ${FILESYSTEM_SOURCES})
    set(HAVE_SDL_FILESYSTEM TRUE)

    if(SDL_TIMERS)
      set(SDL_TIMER_HAIKU 1)
      file(GLOB TIMER_SOURCES ${SDL2_SOURCE_DIR}/src/timer/haiku/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${TIMER_SOURCES})
      set(HAVE_SDL_TIMERS TRUE)
    endif(SDL_TIMERS)

    if(VIDEO_OPENGL)
      # TODO: Use FIND_PACKAGE(OpenGL) instead
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_OPENGL_BGL 1)
      set(SDL_VIDEO_RENDER_OGL 1)
      list(APPEND EXTRA_LIBS GL)
      set(HAVE_VIDEO_OPENGL TRUE)
    endif()
  endif()

  CheckPTHREAD()
endif()

# Dummies
# configure.in does it differently:
# if not have X
#   if enable_X {  SDL_X_DISABLED = 1 }
#   [add dummy sources]
# so it always adds a dummy, without checking, if it was actually requested.
# This leads to missing internal references on building, since the
# src/X/*.c does not get included.
if(NOT HAVE_SDL_JOYSTICK)
  set(SDL_JOYSTICK_DISABLED 1)
  if(SDL_JOYSTICK AND NOT APPLE) # results in unresolved symbols on OSX

    file(GLOB JOYSTICK_SOURCES ${SDL2_SOURCE_DIR}/src/joystick/dummy/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${JOYSTICK_SOURCES})
  endif()
endif()
if(NOT HAVE_SDL_HAPTIC)
  set(SDL_HAPTIC_DISABLED 1)
  file(GLOB HAPTIC_SOURCES ${SDL2_SOURCE_DIR}/src/haptic/dummy/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${HAPTIC_SOURCES})
endif()
if(NOT HAVE_SDL_LOADSO)
  set(SDL_LOADSO_DISABLED 1)
  file(GLOB LOADSO_SOURCES ${SDL2_SOURCE_DIR}/src/loadso/dummy/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${LOADSO_SOURCES})
endif()
if(NOT HAVE_SDL_FILESYSTEM)
  set(SDL_FILESYSTEM_DISABLED 1)
  file(GLOB FILESYSTEM_SOURCES ${SDL2_SOURCE_DIR}/src/filesystem/dummy/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${FILESYSTEM_SOURCES})
endif()

# We always need to have threads and timers around
if(NOT HAVE_SDL_THREADS)
  set(SDL_THREADS_DISABLED 1)
  file(GLOB THREADS_SOURCES ${SDL2_SOURCE_DIR}/src/thread/generic/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${THREADS_SOURCES})
endif()
if(NOT HAVE_SDL_TIMERS)
  set(SDL_TIMERS_DISABLED 1)
  file(GLOB TIMER_SOURCES ${SDL2_SOURCE_DIR}/src/timer/dummy/*.c)
  set(SOURCE_FILES ${SOURCE_FILES} ${TIMER_SOURCES})
endif()

if(NOT SDLMAIN_SOURCES)
  file(GLOB SDLMAIN_SOURCES ${SDL2_SOURCE_DIR}/src/main/dummy/*.c)
endif()

# Append the -MMD -MT flags
# if(DEPENDENCY_TRACKING)
#   if(COMPILER_IS_GNUCC)
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MMD -MT \$@")
#   endif()
# endif()

configure_file("${SDL2_SOURCE_DIR}/include/SDL_config.h.cmake"
  "${SDL2_BINARY_DIR}/include/SDL_config.h")

# Prepare the flags and remove duplicates
if(EXTRA_LDFLAGS)
  list(REMOVE_DUPLICATES EXTRA_LDFLAGS)
endif()
if(EXTRA_LIBS)
  list(REMOVE_DUPLICATES EXTRA_LIBS)
endif()
if(EXTRA_CFLAGS)
  list(REMOVE_DUPLICATES EXTRA_CFLAGS)
endif()
listtostr(EXTRA_CFLAGS _EXTRA_CFLAGS)
set(EXTRA_CFLAGS ${_EXTRA_CFLAGS})

# Compat helpers for the configuration files
if(NOT WINDOWS OR CYGWIN)
  # TODO: we need a Windows script, too
  execute_process(COMMAND sh ${SDL2_SOURCE_DIR}/build-scripts/updaterev.sh)

  set(prefix ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/lib${LIB_SUFFIX}")
  set(bindir "\${exec_prefix}/bin")
  set(includedir "\${prefix}/include")
  if(SDL_STATIC)
    set(ENABLE_STATIC_TRUE "")
    set(ENABLE_STATIC_FALSE "#")
  else(SDL_STATIC)
    set(ENABLE_STATIC_TRUE "#")
    set(ENABLE_STATIC_FALSE "")
  endif()
  if(SDL_SHARED)
    set(ENABLE_SHARED_TRUE "")
    set(ENABLE_SHARED_FALSE "#")
  else(SDL_SHARED)
    set(ENABLE_SHARED_TRUE "#")
    set(ENABLE_SHARED_FALSE "")
  endif()

  # Clean up the different lists
  listtostr(EXTRA_LIBS _EXTRA_LIBS "-l")
  set(SDL_STATIC_LIBS ${SDL_LIBS} ${EXTRA_LDFLAGS} ${_EXTRA_LIBS})
  list(REMOVE_DUPLICATES SDL_STATIC_LIBS)
  listtostr(SDL_STATIC_LIBS _SDL_STATIC_LIBS)
  set(SDL_STATIC_LIBS ${_SDL_STATIC_LIBS})
  listtostr(SDL_LIBS _SDL_LIBS)
  set(SDL_LIBS ${_SDL_LIBS})

  # MESSAGE(STATUS "SDL_LIBS: ${SDL_LIBS}")
  # MESSAGE(STATUS "SDL_STATIC_LIBS: ${SDL_STATIC_LIBS}")

  configure_file("${SDL2_SOURCE_DIR}/sdl2.pc.in"
    "${SDL2_BINARY_DIR}/sdl2.pc" @ONLY)
  configure_file("${SDL2_SOURCE_DIR}/sdl2-config.in"
    "${SDL2_BINARY_DIR}/sdl2-config")
  configure_file("${SDL2_SOURCE_DIR}/sdl2-config.in"
    "${SDL2_BINARY_DIR}/sdl2-config" @ONLY)
  configure_file("${SDL2_SOURCE_DIR}/SDL2.spec.in"
    "${SDL2_BINARY_DIR}/SDL2.spec" @ONLY)
endif()

##### Info output #####
message(STATUS "")
message(STATUS "SDL2 was configured with the following options:")
message(STATUS "")
message(STATUS "Platform: ${CMAKE_SYSTEM}")
message(STATUS "64-bit:   ${ARCH_64}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "")
message(STATUS "Subsystems:")
foreach(_SUB ${SDL_SUBSYSTEMS})
  string(TOUPPER ${_SUB} _OPT)
  message_bool_option(${_SUB} SDL_${_OPT})
endforeach()
message(STATUS "")
message(STATUS "Options:")
list(SORT ALLOPTIONS)
foreach(_OPT ${ALLOPTIONS})
  # Longest option is VIDEO_X11_XSCREENSAVER = 22 characters
  # Get the padding
  string(LENGTH ${_OPT} _OPTLEN)
  math(EXPR _PADLEN "23 - ${_OPTLEN}")
  string(RANDOM LENGTH ${_PADLEN} ALPHABET " " _PADDING)
  message_tested_option(${_OPT} ${_PADDING})
endforeach()
message(STATUS "")
message(STATUS " CFLAGS:        ${CMAKE_C_FLAGS}")
message(STATUS " EXTRA_CFLAGS:  ${EXTRA_CFLAGS}")
message(STATUS " EXTRA_LDFLAGS: ${EXTRA_LDFLAGS}")
message(STATUS " EXTRA_LIBS:    ${EXTRA_LIBS}")
message(STATUS "")
message(STATUS " Build Shared Library: ${SDL_SHARED}")
message(STATUS " Build Static Library: ${SDL_STATIC}")
message(STATUS "")
if(UNIX)
  message(STATUS "If something was not detected, although the libraries")
  message(STATUS "were installed, then make sure you have set the")
  message(STATUS "CFLAGS and LDFLAGS environment variables correctly.")
  message(STATUS "")
endif()

# Ensure that the extra cflags are used at compile time
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

# Always build SDLmain
add_library(SDL2main STATIC ${SDLMAIN_SOURCES})
set(_INSTALL_LIBS "SDL2main")

if(SDL_SHARED)
  add_library(SDL2 SHARED ${SOURCE_FILES})
  if(UNIX)
    set_target_properties(SDL2 PROPERTIES
      VERSION ${LT_VERSION}
      SOVERSION ${LT_REVISION}
      OUTPUT_NAME "SDL2-${LT_RELEASE}")
  else(UNIX)
    set_target_properties(SDL2 PROPERTIES
      VERSION ${SDL_VERSION}
      SOVERSION ${LT_REVISION}
      OUTPUT_NAME "SDL2")
  endif()
 set(_INSTALL_LIBS "SDL2" ${_INSTALL_LIBS})
 target_link_libraries(SDL2 ${EXTRA_LIBS} ${EXTRA_LDFLAGS})
endif()

if(SDL_STATIC)
  set (BUILD_SHARED_LIBS FALSE)
  add_library(SDL2-static STATIC ${SOURCE_FILES})
  set_target_properties(SDL2-static PROPERTIES OUTPUT_NAME "SDL2")
  if(MSVC)
    set_target_properties(SDL2-static PROPERTIES LINK_FLAGS_RELEASE "/NODEFAULTLIB")
    set_target_properties(SDL2-static PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB")
    set_target_properties(SDL2-static PROPERTIES STATIC_LIBRARY_FLAGS "/NODEFAULTLIB")
  endif()
  # TODO: Win32 platforms keep the same suffix .lib for import and static
  # libraries - do we need to consider this?
  set(_INSTALL_LIBS "SDL2-static" ${_INSTALL_LIBS})
  target_link_libraries(SDL2-static ${EXTRA_LIBS} ${EXTRA_LDFLAGS})
endif()

##### Installation targets #####
install(TARGETS ${_INSTALL_LIBS}
  LIBRARY DESTINATION "lib${LIB_SUFFIX}"
  ARCHIVE DESTINATION "lib${LIB_SUFFIX}")

file(GLOB INCLUDE_FILES ${SDL2_SOURCE_DIR}/include/*.h)
file(GLOB BIN_INCLUDE_FILES ${SDL2_BINARY_DIR}/include/*.h)
foreach(_FNAME ${BIN_INCLUDE_FILES})
  get_filename_component(_INCNAME ${_FNAME} NAME)
  list(REMOVE_ITEM INCLUDE_FILES ${SDL2_SOURCE_DIR}/include/${_INCNAME})
endforeach()
list(APPEND INCLUDE_FILES ${BIN_INCLUDE_FILES})
install(FILES ${INCLUDE_FILES} DESTINATION include/SDL2)

if(NOT WINDOWS OR CYGWIN)
  if(SDL_SHARED)
    install(CODE "
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
      \"libSDL2-2.0.so\" \"libSDL2.so\")")
    install(FILES ${SDL2_BINARY_DIR}/libSDL2.so DESTINATION "lib${LIB_SUFFIX}")
  endif()
  if(FREEBSD)
    # FreeBSD uses ${PREFIX}/libdata/pkgconfig
    install(FILES ${SDL2_BINARY_DIR}/sdl2.pc DESTINATION "libdata/pkgconfig")
  else(FREEBSD)
    install(FILES ${SDL2_BINARY_DIR}/sdl2.pc
      DESTINATION "lib${LIB_SUFFIX}/pkgconfig")
  endif()
  install(PROGRAMS ${SDL2_BINARY_DIR}/sdl2-config DESTINATION bin)
  # TODO: what about the .spec file? Is it only needed for RPM creation?
  install(FILES "${SDL2_SOURCE_DIR}/sdl2.m4" DESTINATION "share/aclocal")
else()
  install(TARGETS SDL2 RUNTIME DESTINATION bin)
endif()

