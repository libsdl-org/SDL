macro(FindLibraryAndSONAME _LIB)
  cmake_parse_arguments(_FLAS "" "" "LIBDIRS" ${ARGN})

  string(TOUPPER ${_LIB} _UPPERLNAME)
  string(REGEX REPLACE "\\-" "_" _LNAME "${_UPPERLNAME}")

  find_library(${_LNAME}_LIB ${_LIB} PATHS ${_FLAS_LIBDIRS})

  # FIXME: fail FindLibraryAndSONAME when library is not shared.
  if(${_LNAME}_LIB MATCHES ".*\\${CMAKE_SHARED_LIBRARY_SUFFIX}.*" AND NOT ${_LNAME}_LIB MATCHES ".*\\${CMAKE_STATIC_LIBRARY_SUFFIX}.*")
    set(${_LNAME}_SHARED TRUE)
  else()
    set(${_LNAME}_SHARED FALSE)
  endif()

  if(${_LNAME}_LIB)
    # reduce the library name for shared linking

    get_filename_component(_LIB_REALPATH ${${_LNAME}_LIB} REALPATH)  # resolves symlinks
    get_filename_component(_LIB_LIBDIR ${_LIB_REALPATH} DIRECTORY)
    get_filename_component(_LIB_JUSTNAME ${_LIB_REALPATH} NAME)

    if(APPLE)
      string(REGEX REPLACE "(\\.[0-9]*)\\.[0-9\\.]*dylib$" "\\1.dylib" _LIB_REGEXD "${_LIB_JUSTNAME}")
    else()
      string(REGEX REPLACE "(\\.[0-9]*)\\.[0-9\\.]*$" "\\1" _LIB_REGEXD "${_LIB_JUSTNAME}")
    endif()

    if(NOT EXISTS "${_LIB_LIBDIR}/${_LIB_REGEXD}")
      set(_LIB_REGEXD "${_LIB_JUSTNAME}")
    endif()
    set(${_LNAME}_LIBDIR "${_LIB_LIBDIR}")

    message(STATUS "dynamic lib${_LIB} -> ${_LIB_REGEXD}")
    set(${_LNAME}_LIB_SONAME ${_LIB_REGEXD})
  endif()

  message(DEBUG "DYNLIB OUTPUTVAR: ${_LIB} ... ${_LNAME}_LIB")
  message(DEBUG "DYNLIB ORIGINAL LIB: ${_LIB} ... ${${_LNAME}_LIB}")
  message(DEBUG "DYNLIB REALPATH LIB: ${_LIB} ... ${_LIB_REALPATH}")
  message(DEBUG "DYNLIB JUSTNAME LIB: ${_LIB} ... ${_LIB_JUSTNAME}")
  message(DEBUG "DYNLIB SONAME LIB: ${_LIB} ... ${_LIB_REGEXD}")
endmacro()

macro(CheckDLOPEN)
  check_symbol_exists(dlopen "dlfcn.h" HAVE_DLOPEN_IN_LIBC)
  if(NOT HAVE_DLOPEN_IN_LIBC)
    cmake_push_check_state()
    list(APPEND CMAKE_REQUIRED_LIBRARIES dl)
    check_symbol_exists(dlopen "dlfcn.h" HAVE_DLOPEN_IN_LIBDL)
    cmake_pop_check_state()
    if(HAVE_DLOPEN_IN_LIBDL)
      sdl_link_dependency(dl LIBS dl)
    endif()
  endif()
  if(HAVE_DLOPEN_IN_LIBC OR HAVE_DLOPEN_IN_LIBDL)
    set(HAVE_DLOPEN TRUE)
  endif()
endmacro()

macro(CheckO_CLOEXEC)
  check_c_source_compiles("
    #include <fcntl.h>
    int flag = O_CLOEXEC;
    int main(int argc, char **argv) { return 0; }" HAVE_O_CLOEXEC)
endmacro()

# Requires:
# - n/a
macro(CheckOSS)
  if(SDL_OSS)
    check_c_source_compiles("
        #include <sys/soundcard.h>
        int main(int argc, char **argv) { int arg = SNDCTL_DSP_SETFRAGMENT; return 0; }" HAVE_OSS_SYS_SOUNDCARD_H)

    if(HAVE_OSS_SYS_SOUNDCARD_H)
      set(HAVE_OSS TRUE)
      sdl_glob_sources(${SDL3_SOURCE_DIR}/src/audio/dsp/*.c)
      set(SDL_AUDIO_DRIVER_OSS 1)
      if(NETBSD)
        sdl_link_dependency(oss LIBS ossaudio)
      endif()
      set(HAVE_SDL_AUDIO TRUE)
    endif()
  endif()
endmacro()

# Requires:
# - n/a
# Optional:
# - SDL_ALSA_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckALSA)
  if(SDL_ALSA)
    set(ALSA_PKG_CONFIG_SPEC "alsa")
    find_package(ALSA MODULE)
    if(ALSA_FOUND)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/alsa/*.c")
      set(SDL_AUDIO_DRIVER_ALSA 1)
      set(HAVE_ALSA TRUE)
      set(HAVE_ALSA_SHARED FALSE)
      if(SDL_ALSA_SHARED)
        if(HAVE_SDL_LOADSO)
          FindLibraryAndSONAME("asound")
          if(ASOUND_LIB AND ASOUND_SHARED)
            sdl_link_dependency(alsa INCLUDES $<TARGET_PROPERTY:ALSA::ALSA,INTERFACE_INCLUDE_DIRECTORIES>)
            set(SDL_AUDIO_DRIVER_ALSA_DYNAMIC "\"${ASOUND_LIB_SONAME}\"")
            set(HAVE_ALSA_SHARED TRUE)
          else()
            message(WARNING "Unable to find asound shared object")
          endif()
        else()
          message(WARNING "You must have SDL_LoadObject() support for dynamic ALSA loading")
        endif()
      endif()
      if(NOT HAVE_ALSA_SHARED)
        #FIXME: remove this line and property generate sdl3.pc
        list(APPEND SDL_PC_PRIVATE_REQUIRES alsa)
        sdl_link_dependency(alsa LIBS ALSA::ALSA CMAKE_MODULE ALSA PKG_CONFIG_SPECS "${ALSA_PKG_CONFIG_SPEC}")
      endif()
      set(HAVE_SDL_AUDIO TRUE)
    else()
      message(WARNING "Unable to find the alsa development library")
    endif()
  else()
    set(HAVE_ALSA FALSE)
  endif()
endmacro()

# Requires:
# - PkgCheckModules
# Optional:
# - SDL_PIPEWIRE_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckPipewire)
  if(SDL_PIPEWIRE)
    set(PipeWire_PKG_CONFIG_SPEC libpipewire-0.3>=0.3.44)
    set(PC_PIPEWIRE_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_PIPEWIRE IMPORTED_TARGET ${PipeWire_PKG_CONFIG_SPEC})
    endif()
    if(PC_PIPEWIRE_FOUND)
      set(HAVE_PIPEWIRE TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/pipewire/*.c")
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/camera/pipewire/*.c")
      set(SDL_AUDIO_DRIVER_PIPEWIRE 1)
      set(SDL_CAMERA_DRIVER_PIPEWIRE 1)
      if(SDL_PIPEWIRE_SHARED AND NOT HAVE_SDL_LOADSO)
        message(WARNING "You must have SDL_LoadObject() support for dynamic PipeWire loading")
      endif()
      FindLibraryAndSONAME("pipewire-0.3" LIBDIRS ${PC_PIPEWIRE_LIBRARY_DIRS})
      if(SDL_PIPEWIRE_SHARED AND PIPEWIRE_0.3_LIB AND HAVE_SDL_LOADSO)
        set(SDL_AUDIO_DRIVER_PIPEWIRE_DYNAMIC "\"${PIPEWIRE_0.3_LIB_SONAME}\"")
        set(SDL_CAMERA_DRIVER_PIPEWIRE_DYNAMIC "\"${PIPEWIRE_0.3_LIB_SONAME}\"")
        set(HAVE_PIPEWIRE_SHARED TRUE)
        sdl_link_dependency(pipewire INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_PIPEWIRE,INTERFACE_INCLUDE_DIRECTORIES>)
      else()
        sdl_link_dependency(pipewire LIBS PkgConfig::PC_PIPEWIRE PKG_CONFIG_PREFIX PC_PIPEWIRE PKG_CONFIG_SPECS ${PipeWire_PKG_CONFIG_SPEC})
      endif()
      set(HAVE_SDL_AUDIO TRUE)
      endif()
    endif()
endmacro()

# Requires:
# - PkgCheckModules
# Optional:
# - SDL_PULSEAUDIO_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckPulseAudio)
  if(SDL_PULSEAUDIO)
    set(PulseAudio_PKG_CONFIG_SPEC "libpulse>=0.9.15")
    set(PC_PULSEAUDIO_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_PULSEAUDIO IMPORTED_TARGET ${PulseAudio_PKG_CONFIG_SPEC})
    endif()
    if(PC_PULSEAUDIO_FOUND)
      set(HAVE_PULSEAUDIO TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/pulseaudio/*.c")
      set(SDL_AUDIO_DRIVER_PULSEAUDIO 1)
      if(SDL_PULSEAUDIO_SHARED AND NOT HAVE_SDL_LOADSO)
        message(WARNING "You must have SDL_LoadObject() support for dynamic PulseAudio loading")
      endif()
      FindLibraryAndSONAME("pulse" LIBDIRS ${PC_PULSEAUDIO_LIBRARY_DIRS})
      if(SDL_PULSEAUDIO_SHARED AND PULSE_LIB AND HAVE_SDL_LOADSO)
        set(SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC "\"${PULSE_LIB_SONAME}\"")
        set(HAVE_PULSEAUDIO_SHARED TRUE)
        sdl_link_dependency(pulseaudio INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_PULSEAUDIO,INTERFACE_INCLUDE_DIRECTORIES>)
      else()
        sdl_link_dependency(pulseaudio LIBS PkgConfig::PC_PULSEAUDIO PKG_CONFIG_PREFIX PC_PULSEAUDIO PKG_CONFIG_SPECS "${PulseAudio_PKG_CONFIG_SPEC}")
      endif()
      set(HAVE_SDL_AUDIO TRUE)
    endif()
  endif()
endmacro()

# Requires:
# - PkgCheckModules
# Optional:
# - SDL_JACK_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckJACK)
  if(SDL_JACK)
    set(Jack_PKG_CONFIG_SPEC jack)
    set(PC_JACK_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_JACK IMPORTED_TARGET ${Jack_PKG_CONFIG_SPEC})
    endif()
    if(PC_JACK_FOUND)
      set(HAVE_JACK TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/jack/*.c")
      set(SDL_AUDIO_DRIVER_JACK 1)
      if(SDL_JACK_SHARED AND NOT HAVE_SDL_LOADSO)
        message(WARNING "You must have SDL_LoadObject() support for dynamic JACK audio loading")
      endif()
      FindLibraryAndSONAME("jack" LIBDIRS ${PC_JACK_LIBRARY_DIRS})
      if(SDL_JACK_SHARED AND JACK_LIB AND HAVE_SDL_LOADSO)
        set(SDL_AUDIO_DRIVER_JACK_DYNAMIC "\"${JACK_LIB_SONAME}\"")
        set(HAVE_JACK_SHARED TRUE)
        sdl_link_dependency(jack INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_JACK,INTERFACE_INCLUDE_DIRECTORIES>)
      else()
        sdl_link_dependency(jack LIBS PkgConfig::PC_JACK PKG_CONFIG_PREFIX PC_JACK PKG_CONFIG_SPECS ${Jack_PKG_CONFIG_SPEC})
      endif()
      set(HAVE_SDL_AUDIO TRUE)
    endif()
  endif()
endmacro()

# Requires:
# - PkgCheckModules
# Optional:
# - SDL_SNDIO_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckSNDIO)
  if(SDL_SNDIO)
    set(SndIO_PKG_CONFIG_SPEC sndio)
    set(PC_SNDIO_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_SNDIO IMPORTED_TARGET ${SndIO_PKG_CONFIG_SPEC})
    endif()
    if(PC_SNDIO_FOUND)
      set(HAVE_SNDIO TRUE)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/sndio/*.c")
      set(SDL_AUDIO_DRIVER_SNDIO 1)
      if(SDL_SNDIO_SHARED AND NOT HAVE_SDL_LOADSO)
        message(WARNING "You must have SDL_LoadObject() support for dynamic sndio loading")
      endif()
      FindLibraryAndSONAME("sndio" LIBDIRS ${PC_SNDIO_LIBRARY_DIRS})
      if(SDL_SNDIO_SHARED AND SNDIO_LIB AND HAVE_SDL_LOADSO)
        set(SDL_AUDIO_DRIVER_SNDIO_DYNAMIC "\"${SNDIO_LIB_SONAME}\"")
        set(HAVE_SNDIO_SHARED TRUE)
        sdl_include_directories(PRIVATE SYSTEM $<TARGET_PROPERTY:PkgConfig::PC_SNDIO,INTERFACE_INCLUDE_DIRECTORIES>)
      else()
        sdl_link_dependency(sndio LIBS PkgConfig::PC_SNDIO PKG_CONFIG_PREFIX PC_SNDIO PKG_CONFIG_SPECS ${SndIO_PKG_CONFIG_SPEC})
      endif()
      set(HAVE_SDL_AUDIO TRUE)
    endif()
  endif()
endmacro()

# Requires:
# - n/a
# Optional:
# - SDL_X11_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckX11)
  cmake_push_check_state()
  if(SDL_X11)
    set(X11_PKG_CONFIG_SPEC x11)
    set(Xext_PKG_CONFIG_SPEC xext)
    set(Xcursor_PKG_CONFIG_SPEC xcursor)
    set(Xi_PKG_CONFIG_SPEC xi)
    set(Xfixes_PKG_CONFIG_SPEC xfixes)
    set(Xrandr_PKG_CONFIG_SPEC xrandr)
    set(Xrender_PKG_CONFIG_SPEC xrender)
    set(Xss_PKG_CONFIG_SPEC xscrnsaver)

    find_package(X11)

    foreach(_LIB X11 Xext Xcursor Xi Xfixes Xrandr Xrender Xss)
      get_filename_component(_libdir "${X11_${_LIB}_LIB}" DIRECTORY)
      FindLibraryAndSONAME("${_LIB}" LIBDIRS ${_libdir})
    endforeach()

    find_path(X11_INCLUDEDIR
      NAMES X11/Xlib.h
      PATHS
        ${X11_INCLUDE_DIR}
        /usr/pkg/xorg/include
        /usr/X11R6/include
        /usr/X11R7/include
        /usr/local/include/X11
        /usr/include/X11
        /usr/openwin/include
        /usr/openwin/share/include
        /opt/graphics/OpenGL/include
        /opt/X11/include
    )

    if(X11_INCLUDEDIR)
      sdl_include_directories(PRIVATE SYSTEM "${X11_INCLUDEDIR}")
      list(APPEND CMAKE_REQUIRED_INCLUDES ${X11_INCLUDEDIR})
    endif()

    find_file(HAVE_XCURSOR_H NAMES "X11/Xcursor/Xcursor.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XINPUT2_H NAMES "X11/extensions/XInput2.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XRANDR_H NAMES "X11/extensions/Xrandr.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XFIXES_H_ NAMES "X11/extensions/Xfixes.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XRENDER_H NAMES "X11/extensions/Xrender.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XSYNC_H NAMES "X11/extensions/sync.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XSS_H NAMES "X11/extensions/scrnsaver.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XSHAPE_H NAMES "X11/extensions/shape.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XDBE_H NAMES "X11/extensions/Xdbe.h" HINTS "${X11_INCLUDEDIR}")
    find_file(HAVE_XEXT_H NAMES "X11/extensions/Xext.h" HINTS "${X11_INCLUDEDIR}")

    if(X11_LIB AND HAVE_XEXT_H)

      set(HAVE_X11 TRUE)
      set(HAVE_SDL_VIDEO TRUE)

      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/x11/*.c")
      set(SDL_VIDEO_DRIVER_X11 1)

      # Note: Disabled on Apple because the dynamic mode backend for X11 doesn't
      # work properly on Apple during several issues like inconsistent paths
      # among platforms. See #6778 (https://github.com/libsdl-org/SDL/issues/6778)
      if(APPLE)
        set(SDL_X11_SHARED OFF)
      endif()

      check_symbol_exists(shmat "sys/shm.h" HAVE_SHMAT_IN_LIBC)
      if(NOT HAVE_SHMAT_IN_LIBC)
        check_library_exists(ipc shmat "" HAVE_SHMAT_IN_LIBIPC)
        if(HAVE_SHMAT_IN_LIBIPC)
          sdl_link_dependency(x11_ipc LIBS ipc)
        endif()
        if(NOT HAVE_SHMAT_IN_LIBIPC)
          sdl_compile_definitions(PRIVATE "NO_SHARED_MEMORY")
        endif()
      endif()

      if(SDL_X11_SHARED)
        if(NOT HAVE_SDL_LOADSO)
          message(WARNING "You must have SDL_LoadObject() support for dynamic X11 loading")
          set(HAVE_X11_SHARED FALSE)
        else()
          set(HAVE_X11_SHARED TRUE)
        endif()
        if(X11_LIB)
          if(HAVE_X11_SHARED)
            set(SDL_VIDEO_DRIVER_X11_DYNAMIC "\"${X11_LIB_SONAME}\"")
          else()
            sdl_link_dependency(x11 LIBS X11::X11 CMAKE_MODULE X11 PKG_CONFIG_SPECS ${X11_PKG_CONFIG_SPEC})
          endif()
        endif()
        if(XEXT_LIB)
          if(HAVE_X11_SHARED)
            set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT "\"${XEXT_LIB_SONAME}\"")
          else()
            sdl_link_dependency(xext LIBS X11::Xext CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xext_PKG_CONFIG_SPEC})
          endif()
        endif()
      else()
        sdl_link_dependency(x11 LIBS X11::X11 CMAKE_MODULE X11 PKG_CONFIG_SPECS ${X11_PKG_CONFIG_SPEC})
        sdl_link_dependency(xext LIBS X11::Xext CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xext_PKG_CONFIG_SPEC})
      endif()

      list(APPEND CMAKE_REQUIRED_LIBRARIES ${X11_LIB})

      check_c_source_compiles("
          #include <X11/Xlib.h>
          int main(int argc, char **argv) {
            Display *display;
            XEvent event;
            XGenericEventCookie *cookie = &event.xcookie;
            XNextEvent(display, &event);
            XGetEventData(display, cookie);
            XFreeEventData(display, cookie);
            return 0; }" HAVE_XGENERICEVENT)
      if(HAVE_XGENERICEVENT)
        set(SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS 1)
      endif()

      check_symbol_exists(XkbLookupKeySym "X11/Xlib.h;X11/XKBlib.h" SDL_VIDEO_DRIVER_X11_HAS_XKBLOOKUPKEYSYM)

      if(SDL_X11_XCURSOR AND HAVE_XCURSOR_H AND XCURSOR_LIB)
        set(HAVE_X11_XCURSOR TRUE)
        if(HAVE_X11_SHARED)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR "\"${XCURSOR_LIB_SONAME}\"")
        else()
          sdl_link_dependency(xcursor LIBS X11::Xcursor CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xcursor_PKG_CONFIG_SPEC})
        endif()
        set(SDL_VIDEO_DRIVER_X11_XCURSOR 1)
      endif()

      if(SDL_X11_XDBE AND HAVE_XDBE_H)
        set(HAVE_X11_XDBE TRUE)
        set(SDL_VIDEO_DRIVER_X11_XDBE 1)
      endif()

      if(SDL_X11_XINPUT AND HAVE_XINPUT2_H AND XI_LIB)
        set(HAVE_X11_XINPUT TRUE)
        if(HAVE_X11_SHARED)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2 "\"${XI_LIB_SONAME}\"")
        else()
          sdl_link_dependency(xi LIBS X11::Xi CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xi_PKG_CONFIG_SPEC})
        endif()
        set(SDL_VIDEO_DRIVER_X11_XINPUT2 1)

        # Check for multitouch
        check_c_source_compiles("
            #include <X11/Xlib.h>
            #include <X11/Xproto.h>
            #include <X11/extensions/XInput2.h>
            int event_type = XI_TouchBegin;
            XITouchClassInfo *t;
            Status XIAllowTouchEvents(Display *a,int b,unsigned int c,Window d,int f) {
              return (Status)0;
            }
            int main(int argc, char **argv) { return 0; }" HAVE_XINPUT2_MULTITOUCH)
        if(HAVE_XINPUT2_MULTITOUCH)
          set(SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH 1)
        endif()
      endif()

      # check along with XInput2.h because we use Xfixes with XIBarrierReleasePointer
      if(SDL_X11_XFIXES AND HAVE_XFIXES_H_ AND HAVE_XINPUT2_H)
        check_c_source_compiles("
            #include <X11/Xlib.h>
            #include <X11/Xproto.h>
            #include <X11/extensions/XInput2.h>
            #include <X11/extensions/Xfixes.h>
            BarrierEventID b;
            int main(int argc, char **argv) { return 0; }" HAVE_XFIXES_H)
      endif()
      if(SDL_X11_XFIXES AND HAVE_XFIXES_H AND HAVE_XINPUT2_H AND XFIXES_LIB)
        if(HAVE_X11_SHARED)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XFIXES "\"${XFIXES_LIB_SONAME}\"")
        else()
          sdl_link_dependency(xfixes LIBS X11::Xfixes CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xfixes_PKG_CONFIG_SPEC})
        endif()
        set(SDL_VIDEO_DRIVER_X11_XFIXES 1)
        set(HAVE_X11_XFIXES TRUE)
      endif()

      if(SDL_X11_XSYNC AND HAVE_XSYNC_H AND XEXT_LIB)
        set(SDL_VIDEO_DRIVER_X11_XSYNC 1)
        set(HAVE_X11_XSYNC TRUE)
      endif()

      if(SDL_X11_XRANDR AND HAVE_XRANDR_H AND XRANDR_LIB)
        if(HAVE_X11_SHARED)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR "\"${XRANDR_LIB_SONAME}\"")
        else()
          sdl_link_dependency(xrandr LIBS X11::Xrandr CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xrandr_PKG_CONFIG_SPEC})
        endif()
        set(SDL_VIDEO_DRIVER_X11_XRANDR 1)
        set(HAVE_X11_XRANDR TRUE)
      endif()

      if(SDL_X11_XSCRNSAVER AND HAVE_XSS_H AND XSS_LIB)
        if(HAVE_X11_SHARED)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS "\"${XSS_LIB_SONAME}\"")
        else()
          sdl_link_dependency(xss LIBS X11::Xss CMAKE_MODULE X11 PKG_CONFIG_SPECS ${Xss_PKG_CONFIG_SPEC})
        endif()
        set(SDL_VIDEO_DRIVER_X11_XSCRNSAVER 1)
        set(HAVE_X11_XSCRNSAVER TRUE)
      endif()

      if(SDL_X11_XSHAPE AND HAVE_XSHAPE_H)
        set(SDL_VIDEO_DRIVER_X11_XSHAPE 1)
        set(HAVE_X11_XSHAPE TRUE)
      endif()
    endif()
  endif()
  if(NOT HAVE_X11)
    # Prevent Mesa from including X11 headers
    sdl_compile_definitions(PRIVATE "MESA_EGL_NO_X11_HEADERS" "EGL_NO_X11")
  endif()
  cmake_pop_check_state()
endmacro()

macro(WaylandProtocolGen _SCANNER _CODE_MODE _XML _PROTL)
    set(_WAYLAND_PROT_C_CODE "${CMAKE_CURRENT_BINARY_DIR}/wayland-generated-protocols/${_PROTL}-protocol.c")
    set(_WAYLAND_PROT_H_CODE "${CMAKE_CURRENT_BINARY_DIR}/wayland-generated-protocols/${_PROTL}-client-protocol.h")

    add_custom_command(
        OUTPUT "${_WAYLAND_PROT_H_CODE}"
        DEPENDS "${_XML}"
        COMMAND "${_SCANNER}"
        ARGS client-header "${_XML}" "${_WAYLAND_PROT_H_CODE}"
    )

    add_custom_command(
        OUTPUT "${_WAYLAND_PROT_C_CODE}"
        DEPENDS "${_WAYLAND_PROT_H_CODE}"
        COMMAND "${_SCANNER}"
        ARGS "${_CODE_MODE}" "${_XML}" "${_WAYLAND_PROT_C_CODE}"
    )

    sdl_sources("${_WAYLAND_PROT_C_CODE}")
endmacro()

# Requires:
# - EGL
# - PkgCheckModules
# Optional:
# - SDL_WAYLAND_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckWayland)
  if(SDL_WAYLAND)
    set(WAYLAND_PKG_CONFIG_SPEC "wayland-client>=1.18" wayland-egl wayland-cursor egl "xkbcommon>=0.5.0")
    set(PC_WAYLAND_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_WAYLAND IMPORTED_TARGET ${WAYLAND_PKG_CONFIG_SPEC})
    endif()
    find_program(WAYLAND_SCANNER NAMES wayland-scanner)

    set(WAYLAND_FOUND FALSE)
    if(PC_WAYLAND_FOUND AND WAYLAND_SCANNER)
      execute_process(
        COMMAND ${WAYLAND_SCANNER} --version
        RESULT_VARIABLE WAYLAND_SCANNER_VERSION_RC
        ERROR_VARIABLE WAYLAND_SCANNER_VERSION_STDERR
        ERROR_STRIP_TRAILING_WHITESPACE
      )
      if(NOT WAYLAND_SCANNER_VERSION_RC EQUAL 0)
        message(WARNING "Failed to get wayland-scanner version")
      else()
        if(WAYLAND_SCANNER_VERSION_STDERR MATCHES [[([0-9.]+)$]])
          set(WAYLAND_FOUND TRUE)
          set(WAYLAND_SCANNER_VERSION ${CMAKE_MATCH_1})
          if(WAYLAND_SCANNER_VERSION VERSION_LESS "1.15.0")
            set(WAYLAND_SCANNER_CODE_MODE "code")
          else()
            set(WAYLAND_SCANNER_CODE_MODE "private-code")
          endif()
        endif()
      endif()
    endif()

    if(WAYLAND_FOUND)
      set(HAVE_WAYLAND TRUE)
      set(HAVE_SDL_VIDEO TRUE)

      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/wayland/*.c")

      # We have to generate some protocol interface code for some unstable Wayland features.
      file(MAKE_DIRECTORY "${SDL3_BINARY_DIR}/wayland-generated-protocols")
      # Prepend to include path to make sure they override installed protocol headers
      sdl_include_directories(PRIVATE SYSTEM BEFORE "${SDL3_BINARY_DIR}/wayland-generated-protocols")

      file(GLOB WAYLAND_PROTOCOLS_XML RELATIVE "${SDL3_SOURCE_DIR}/wayland-protocols/" "${SDL3_SOURCE_DIR}/wayland-protocols/*.xml")
      foreach(_XML IN LISTS WAYLAND_PROTOCOLS_XML)
        string(REGEX REPLACE "\\.xml$" "" _PROTL "${_XML}")
        WaylandProtocolGen("${WAYLAND_SCANNER}" "${WAYLAND_SCANNER_CODE_MODE}" "${SDL3_SOURCE_DIR}/wayland-protocols/${_XML}" "${_PROTL}")
      endforeach()

      if(SDL_WAYLAND_SHARED AND NOT HAVE_SDL_LOADSO)
        message(WARNING "You must have SDL_LoadObject() support for dynamic Wayland loading")
      endif()
      FindLibraryAndSONAME(wayland-client LIBDIRS ${PC_WAYLAND_LIBRARY_DIRS})
      FindLibraryAndSONAME(wayland-egl LIBDIRS ${PC_WAYLAND_LIBRARY_DIRS})
      FindLibraryAndSONAME(wayland-cursor LIBDIRS ${PC_WAYLAND_LIBRARY_DIRS})
      FindLibraryAndSONAME(xkbcommon LIBDIRS ${PC_WAYLAND_LIBRARY_DIRS})
      if(SDL_WAYLAND_SHARED AND WAYLAND_CLIENT_LIB AND WAYLAND_EGL_LIB AND WAYLAND_CURSOR_LIB AND XKBCOMMON_LIB AND HAVE_SDL_LOADSO)
        set(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC "\"${WAYLAND_CLIENT_LIB_SONAME}\"")
        set(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_EGL "\"${WAYLAND_EGL_LIB_SONAME}\"")
        set(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_CURSOR "\"${WAYLAND_CURSOR_LIB_SONAME}\"")
        set(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_XKBCOMMON "\"${XKBCOMMON_LIB_SONAME}\"")
        set(HAVE_WAYLAND_SHARED TRUE)
        sdl_link_dependency(wayland INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_WAYLAND,INTERFACE_INCLUDE_DIRECTORIES>)
      else()
        sdl_link_dependency(wayland LIBS PkgConfig::PC_WAYLAND PKG_CONFIG_PREFIX PC_WAYLAND PKG_CONFIG_SPECS ${WAYLAND_PKG_CONFIG_SPEC})
      endif()

      if(SDL_WAYLAND_LIBDECOR)
        set(LibDecor_PKG_CONFIG_SPEC libdecor-0)
        pkg_check_modules(PC_LIBDECOR IMPORTED_TARGET ${LibDecor_PKG_CONFIG_SPEC})
        if(PC_LIBDECOR_FOUND)

          # Libdecor doesn't provide internal version defines, so generate them here.
          if (PC_LIBDECOR_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
            set(SDL_LIBDECOR_VERSION_MAJOR ${CMAKE_MATCH_1})
            set(SDL_LIBDECOR_VERSION_MINOR ${CMAKE_MATCH_2})
            set(SDL_LIBDECOR_VERSION_PATCH ${CMAKE_MATCH_3})
          else()
            message(WARNING "Failed to parse libdecor version; defaulting to lowest supported (0.1.0)")
            set(SDL_LIBDECOR_VERSION_MAJOR 0)
            set(SDL_LIBDECOR_VERSION_MINOR 1)
            set(SDL_LIBDECOR_VERSION_PATCH 0)
          endif()

          if(PC_LIBDECOR_VERSION VERSION_GREATER_EQUAL "0.2.0")
            set(LibDecor_PKG_CONFIG_SPEC "libdecor-0>=0.2.0")
          endif()
          set(HAVE_WAYLAND_LIBDECOR TRUE)
          set(HAVE_LIBDECOR_H 1)
          if(SDL_WAYLAND_LIBDECOR_SHARED AND NOT HAVE_SDL_LOADSO)
            message(WARNING "You must have SDL_LoadObject() support for dynamic libdecor loading")
          endif()
          FindLibraryAndSONAME(decor-0 LIBDIRS ${PC_LIBDECOR_LIBRARY_DIRS})
          if(SDL_WAYLAND_LIBDECOR_SHARED AND DECOR_0_LIB AND HAVE_SDL_LOADSO)
            set(HAVE_WAYLAND_LIBDECOR_SHARED TRUE)
            set(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR "\"${DECOR_0_LIB_SONAME}\"")
            sdl_link_dependency(libdecor INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_LIBDECOR,INTERFACE_INCLUDE_DIRECTORIES>)
          else()
            sdl_link_dependency(libdecor LIBS PkgConfig::PC_LIBDECOR PKG_CONFIG_PREFIX PC_LIBDECOR PKG_CONFIG_SPECS ${LibDecor_PKG_CONFIG_SPEC})
            endif()
        endif()
      endif()

      set(SDL_VIDEO_DRIVER_WAYLAND 1)
    endif()
  endif()
endmacro()

# Requires:
# - n/a
#
macro(CheckCOCOA)
  if(SDL_COCOA)
    if(APPLE) # Apple always has Cocoa.
      set(HAVE_COCOA TRUE)
    endif()
    if(HAVE_COCOA)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/cocoa/*.m")
      set(SDL_FRAMEWORK_IOKIT 1)
      set(SDL_VIDEO_DRIVER_COCOA 1)
      set(HAVE_SDL_VIDEO TRUE)
    endif()
  endif()
endmacro()

# Requires:
# - n/a
macro(CheckVivante)
  if(SDL_VIVANTE)
    check_c_source_compiles("
        #include <gc_vdk.h>
        int main(int argc, char** argv) { return 0; }" HAVE_VIVANTE_VDK)
    check_c_source_compiles("
        #define LINUX
        #define EGL_API_FB
        #include <EGL/eglvivante.h>
        int main(int argc, char** argv) { return 0; }" HAVE_VIVANTE_EGL_FB)
    if(HAVE_VIVANTE_VDK OR HAVE_VIVANTE_EGL_FB)
      set(HAVE_VIVANTE TRUE)
      set(HAVE_SDL_VIDEO TRUE)

      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/vivante/*.c")
      set(SDL_VIDEO_DRIVER_VIVANTE 1)
      # FIXME: Use Find module
      if(HAVE_VIVANTE_VDK)
        set(SDL_VIDEO_DRIVER_VIVANTE_VDK 1)
        find_library(VIVANTE_LIBRARY REQUIRED NAMES VIVANTE vivante drm_vivante)
        find_library(VIVANTE_VDK_LIBRARY VDK REQUIRED)
        sdl_link_dependency(vivante LIBS ${VIVANTE_LIBRARY} ${VIVANTE_VDK_LIBRARY})
      else()
        # these defines are needed when including the system EGL headers, which SDL does
        sdl_compile_definitions(PUBLIC "LINUX" "EGL_API_FB")
        sdl_link_dependency(vivante LIBS EGL)
      endif(HAVE_VIVANTE_VDK)
    endif()
  endif()
endmacro()

# Requires:
# - n/a
macro(CheckOpenVR)
  if(SDL_OPENVR)
    set(HAVE_OPENVR TRUE)
    set(HAVE_OPENVR_VIDEO TRUE)

    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/openvr/*.c")
    set(SDL_VIDEO_DRIVER_OPENVR 1)
    if(NOT WINDOWS)
      sdl_link_dependency(egl LIBS EGL)
    endif()
  endif()
endmacro()

# Requires:
# - nada
macro(CheckGLX)
  if(SDL_OPENGL)
    check_c_source_compiles("
        #include <GL/glx.h>
        int main(int argc, char** argv) { return 0; }" HAVE_OPENGL_GLX)
    if(HAVE_OPENGL_GLX AND NOT HAVE_ROCKCHIP)
      set(SDL_VIDEO_OPENGL_GLX 1)
    endif()
  endif()
endmacro()

# Requires:
# - PkgCheckModules
macro(CheckEGL)
  if(SDL_OPENGL OR SDL_OPENGLES)
    cmake_push_check_state()
    find_package(OpenGL MODULE)
    list(APPEND CMAKE_REQUIRED_INCLUDES ${OPENGL_EGL_INCLUDE_DIRS})
    list(APPEND CMAKE_REQUIRED_INCLUDES "${SDL3_SOURCE_DIR}/src/video/khronos")
    check_c_source_compiles("
        #define EGL_API_FB
        #define MESA_EGL_NO_X11_HEADERS
        #define EGL_NO_X11
        #include <EGL/egl.h>
        #include <EGL/eglext.h>
        int main (int argc, char** argv) { return 0; }" HAVE_OPENGL_EGL)
    cmake_pop_check_state()
    if(HAVE_OPENGL_EGL)
      set(SDL_VIDEO_OPENGL_EGL 1)
      sdl_link_dependency(egl INCLUDES ${OPENGL_EGL_INCLUDE_DIRS})
    endif()
  endif()
endmacro()

# Requires:
# - nada
macro(CheckOpenGL)
  if(SDL_OPENGL)
    check_c_source_compiles("
        #include <GL/gl.h>
        #include <GL/glext.h>
        int main(int argc, char** argv) { return 0; }" HAVE_OPENGL)
    if(HAVE_OPENGL)
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_RENDER_OGL 1)
    endif()
  endif()
endmacro()

# Requires:
# - nada
macro(CheckOpenGLES)
  if(SDL_OPENGLES)
    cmake_push_check_state()
    list(APPEND CMAKE_REQUIRED_INCLUDES "${SDL3_SOURCE_DIR}/src/video/khronos")
    check_c_source_compiles("
        #include <GLES/gl.h>
        #include <GLES/glext.h>
        int main (int argc, char** argv) { return 0; }" HAVE_OPENGLES_V1)
    check_c_source_compiles("
      #include <GLES2/gl2.h>
      #include <GLES2/gl2ext.h>
      int main (int argc, char** argv) { return 0; }" HAVE_OPENGLES_V2)
    cmake_pop_check_state()
    if(HAVE_OPENGLES_V1)
      set(HAVE_OPENGLES TRUE)
      set(SDL_VIDEO_OPENGL_ES 1)
    endif()
    if(HAVE_OPENGLES_V2)
      set(HAVE_OPENGLES TRUE)
      set(SDL_VIDEO_OPENGL_ES2 1)
      set(SDL_VIDEO_RENDER_OGL_ES2 1)
    endif()
  endif()
endmacro()

macro(CheckVulkan)
  if(SDL_VULKAN)
    set(SDL_VIDEO_VULKAN 1)
    set(HAVE_VULKAN TRUE)
    if(SDL_RENDER_VULKAN)
      set(SDL_VIDEO_RENDER_VULKAN 1)
      set(HAVE_RENDER_VULKAN TRUE)
    endif()
  endif()
endmacro()

# Requires:
# - EGL
macro(CheckQNXScreen)
  if(QNX AND HAVE_OPENGL_EGL)
    check_c_source_compiles("
        #include <screen/screen.h>
        int main (int argc, char** argv) { return 0; }" HAVE_QNX_SCREEN)
    if(HAVE_QNX_SCREEN)
      set(SDL_VIDEO_DRIVER_QNX 1)
      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/qnx/*.c")
      sdl_link_dependency(qnxscreen LIBS screen EGL)
    endif()
  endif()
endmacro()

# Requires:
# - nada
# Optional:
# - THREADS opt
# Sets:
# PTHREAD_CFLAGS
# PTHREAD_LIBS
macro(CheckPTHREAD)
  cmake_push_check_state()
  if(SDL_PTHREADS)
    if(ANDROID)
      # the android libc provides built-in support for pthreads, so no
      # additional linking or compile flags are necessary
    elseif(LINUX)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-pthread")
    elseif(BSDI)
      set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
      set(PTHREAD_LDFLAGS "")
    elseif(DARWIN)
      set(PTHREAD_CFLAGS "-D_THREAD_SAFE")
      # causes Carbon.p complaints?
      # set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
      set(PTHREAD_LDFLAGS "")
    elseif(FREEBSD)
      set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
      set(PTHREAD_LDFLAGS "-pthread")
    elseif(NETBSD)
      set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
      set(PTHREAD_LDFLAGS "-lpthread")
    elseif(OPENBSD)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-lpthread")
    elseif(SOLARIS)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      if(CMAKE_C_COMPILER_ID MATCHES "SunPro")
        set(PTHREAD_LDFLAGS "-mt -lpthread")
      else()
        set(PTHREAD_LDFLAGS "-pthread")
      endif()
    elseif(SYSV5)
      set(PTHREAD_CFLAGS "-D_REENTRANT -Kthread")
      set(PTHREAD_LDFLAGS "")
    elseif(AIX)
      set(PTHREAD_CFLAGS "-D_REENTRANT -mthreads")
      set(PTHREAD_LDFLAGS "-pthread")
    elseif(HPUX)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-L/usr/lib -pthread")
    elseif(HAIKU)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "")
    elseif(EMSCRIPTEN)
      set(PTHREAD_CFLAGS "-D_REENTRANT -pthread")
      set(PTHREAD_LDFLAGS "-pthread")
    elseif(QNX)
      # pthread support is baked in
    else()
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-lpthread")
    endif()

    # Run some tests
    string(APPEND CMAKE_REQUIRED_FLAGS " ${PTHREAD_CFLAGS} ${PTHREAD_LDFLAGS}")
    check_c_source_compiles("
        #include <pthread.h>
        int main(int argc, char** argv) {
          pthread_attr_t type;
          pthread_attr_init(&type);
          return 0;
        }" HAVE_PTHREADS)
    if(HAVE_PTHREADS)
      set(SDL_THREAD_PTHREAD 1)
      separate_arguments(PTHREAD_CFLAGS)
      sdl_compile_options(PRIVATE ${PTHREAD_CFLAGS})
      sdl_link_dependency(pthread LINK_OPTIONS ${PTHREAD_LDFLAGS})

      check_c_source_compiles("
        #include <pthread.h>
        int main(int argc, char **argv) {
          pthread_mutexattr_t attr;
          pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
          return 0;
        }" HAVE_RECURSIVE_MUTEXES)
      if(HAVE_RECURSIVE_MUTEXES)
        set(SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1)
      else()
        check_c_source_compiles("
            #include <pthread.h>
            int main(int argc, char **argv) {
              pthread_mutexattr_t attr;
              pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
              return 0;
            }" HAVE_RECURSIVE_MUTEXES_NP)
        if(HAVE_RECURSIVE_MUTEXES_NP)
          set(SDL_THREAD_PTHREAD_RECURSIVE_MUTEX_NP 1)
        endif()
      endif()

      if(SDL_PTHREADS_SEM)
        check_c_source_compiles("#include <pthread.h>
                                 #include <semaphore.h>
                                 int main(int argc, char **argv) { return 0; }" HAVE_PTHREADS_SEM)
        if(HAVE_PTHREADS_SEM)
          check_c_source_compiles("
              #include <pthread.h>
              #include <semaphore.h>
              int main(int argc, char **argv) {
                  sem_timedwait(NULL, NULL);
                  return 0;
              }" COMPILER_HAS_SEM_TIMEDWAIT)
          set(HAVE_SEM_TIMEDWAIT ${COMPILER_HAS_SEM_TIMEDWAIT})
        endif()
      endif()

      check_include_file(pthread.h HAVE_PTHREAD_H)
      check_include_file(pthread_np.h HAVE_PTHREAD_NP_H)
      if (HAVE_PTHREAD_H)
        check_c_source_compiles("
            #include <pthread.h>
            int main(int argc, char **argv) {
              #ifdef __APPLE__
              pthread_setname_np(\"\");
              #else
              pthread_setname_np(pthread_self(),\"\");
              #endif
              return 0;
            }" HAVE_PTHREAD_SETNAME_NP)
        if (HAVE_PTHREAD_NP_H)
          check_symbol_exists(pthread_set_name_np "pthread.h;pthread_np.h" HAVE_PTHREAD_SET_NAME_NP)
        endif()
      endif()

      sdl_sources(
        "${SDL3_SOURCE_DIR}/src/thread/pthread/SDL_systhread.c"
        "${SDL3_SOURCE_DIR}/src/thread/pthread/SDL_sysmutex.c"   # Can be faked, if necessary
        "${SDL3_SOURCE_DIR}/src/thread/pthread/SDL_syscond.c"    # Can be faked, if necessary
        "${SDL3_SOURCE_DIR}/src/thread/pthread/SDL_sysrwlock.c"   # Can be faked, if necessary
        "${SDL3_SOURCE_DIR}/src/thread/pthread/SDL_systls.c"
      )
      if(HAVE_PTHREADS_SEM)
        sdl_sources("${SDL3_SOURCE_DIR}/src/thread/pthread/SDL_syssem.c")
      else()
        sdl_sources("${SDL3_SOURCE_DIR}/src/thread/generic/SDL_syssem.c")
      endif()
      set(HAVE_SDL_THREADS TRUE)
    endif()
  endif()
  cmake_pop_check_state()
endmacro()

# Requires
# - nada
# Optional:
# Sets:
# USB_LIBS
# USB_CFLAGS
macro(CheckUSBHID)
  cmake_push_check_state()
  check_library_exists(usbhid hid_init "" LIBUSBHID)
  if(LIBUSBHID)
    check_include_file(usbhid.h HAVE_USBHID_H)
    if(HAVE_USBHID_H)
      set(USB_CFLAGS "-DHAVE_USBHID_H")
    endif()

    check_include_file(libusbhid.h HAVE_LIBUSBHID_H)
    if(HAVE_LIBUSBHID_H)
      string(APPEND USB_CFLAGS " -DHAVE_LIBUSBHID_H")
    endif()
    set(USB_LIBS ${USB_LIBS} usbhid)
  else()
    check_include_file(usb.h HAVE_USB_H)
    if(HAVE_USB_H)
      set(USB_CFLAGS "-DHAVE_USB_H")
    endif()
    check_include_file(libusb.h HAVE_LIBUSB_H)
    if(HAVE_LIBUSB_H)
      string(APPEND USB_CFLAGS " -DHAVE_LIBUSB_H")
    endif()
    check_library_exists(usb hid_init "" LIBUSB)
    if(LIBUSB)
      list(APPEND USB_LIBS usb)
    endif()
  endif()

  string(APPEND CMAKE_REQUIRED_FLAGS " ${USB_CFLAGS}")
  list(APPEND CMAKE_REQUIRED_LIBRARIES ${USB_LIBS})
  check_c_source_compiles("
       #include <sys/types.h>
        #if defined(HAVE_USB_H)
        #include <usb.h>
        #endif
        #ifdef __DragonFly__
        # include <bus/u4b/usb.h>
        # include <bus/u4b/usbhid.h>
        #else
        # include <dev/usb/usb.h>
        # include <dev/usb/usbhid.h>
        #endif
        #if defined(HAVE_USBHID_H)
        #include <usbhid.h>
        #elif defined(HAVE_LIBUSB_H)
        #include <libusb.h>
        #elif defined(HAVE_LIBUSBHID_H)
        #include <libusbhid.h>
        #endif
        int main(int argc, char **argv) {
          struct report_desc *repdesc;
          struct usb_ctl_report *repbuf;
          hid_kind_t hidkind;
          return 0;
        }" HAVE_USBHID)
  if(HAVE_USBHID)
    check_c_source_compiles("
          #include <sys/types.h>
          #if defined(HAVE_USB_H)
          #include <usb.h>
          #endif
          #ifdef __DragonFly__
          # include <bus/u4b/usb.h>
          # include <bus/u4b/usbhid.h>
          #else
          # include <dev/usb/usb.h>
          # include <dev/usb/usbhid.h>
          #endif
          #if defined(HAVE_USBHID_H)
          #include <usbhid.h>
          #elif defined(HAVE_LIBUSB_H)
          #include <libusb.h>
          #elif defined(HAVE_LIBUSBHID_H)
          #include <libusbhid.h>
          #endif
          int main(int argc, char** argv) {
            struct usb_ctl_report buf;
            if (buf.ucr_data) { }
            return 0;
          }" HAVE_USBHID_UCR_DATA)
    if(HAVE_USBHID_UCR_DATA)
      string(APPEND USB_CFLAGS " -DUSBHID_UCR_DATA")
    endif()

    check_c_source_compiles("
          #include <sys/types.h>
          #if defined(HAVE_USB_H)
          #include <usb.h>
          #endif
          #ifdef __DragonFly__
          #include <bus/u4b/usb.h>
          #include <bus/u4b/usbhid.h>
          #else
          #include <dev/usb/usb.h>
          #include <dev/usb/usbhid.h>
          #endif
          #if defined(HAVE_USBHID_H)
          #include <usbhid.h>
          #elif defined(HAVE_LIBUSB_H)
          #include <libusb.h>
          #elif defined(HAVE_LIBUSBHID_H)
          #include <libusbhid.h>
          #endif
          int main(int argc, char **argv) {
            report_desc_t d;
            hid_start_parse(d, 1, 1);
            return 0;
          }" HAVE_USBHID_NEW)
    if(HAVE_USBHID_NEW)
      string(APPEND USB_CFLAGS " -DUSBHID_NEW")
    endif()

    check_c_source_compiles("
        #include <machine/joystick.h>
        int main(int argc, char** argv) {
            struct joystick t;
            return 0;
        }" HAVE_MACHINE_JOYSTICK)
    if(HAVE_MACHINE_JOYSTICK)
      set(SDL_HAVE_MACHINE_JOYSTICK_H 1)
    endif()
    set(SDL_JOYSTICK_USBHID 1)
    sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/bsd/*.c")
    separate_arguments(USB_CFLAGS)
    sdl_compile_options(PRIVATE ${USB_CFLAGS})
    #FIXME: properly add usb libs with pkg-config or whatever
    sdl_link_dependency(usbhid LIBS ${USB_LIBS})
    set(HAVE_SDL_JOYSTICK TRUE)
  endif()
  cmake_pop_check_state()
endmacro()

# Check for HIDAPI support
macro(CheckHIDAPI)
  if(SDL_HIDAPI)
    set(HAVE_HIDAPI ON)
    if(SDL_HIDAPI_LIBUSB)
      set(HAVE_LIBUSB FALSE)
      find_package(LibUSB)
      if(LibUSB_FOUND)
        cmake_push_check_state()
        list(APPEND CMAKE_REQUIRED_LIBRARIES LibUSB::LibUSB)
        check_c_source_compiles("
          #include <stddef.h>
          #include <libusb.h>
          int main(int argc, char **argv) {
            libusb_close(NULL);
            return 0;
          }" HAVE_LIBUSB_H)
        cmake_pop_check_state()
        if(HAVE_LIBUSB_H)
          set(HAVE_LIBUSB TRUE)
          target_get_dynamic_library(dynamic_libusb LibUSB::LibUSB)
          if(SDL_HIDAPI_LIBUSB_SHARED AND dynamic_libusb)
            set(HAVE_HIDAPI_LIBUSB_SHARED ON)
            set(SDL_LIBUSB_DYNAMIC "\"${dynamic_libusb}\"")
            sdl_link_dependency(hidapi INCLUDES $<TARGET_PROPERTY:LibUSB::LibUSB,INTERFACE_INCLUDE_DIRECTORIES>)
          else()
            sdl_link_dependency(hidapi LIBS LibUSB::LibUSB PKG_CONFIG_SPECS "${LibUSB_PKG_CONFIG_SPEC}" CMAKE_MODULE LibUSB)
          endif()
        endif()
      endif()
      set(HAVE_HIDAPI_LIBUSB ${HAVE_LIBUSB})
    endif()

    if(HAVE_HIDAPI)
      if(ANDROID)
        enable_language(CXX)
        sdl_sources("${SDL3_SOURCE_DIR}/src/hidapi/android/hid.cpp")
      endif()
      if(IOS OR TVOS)
        sdl_sources("${SDL3_SOURCE_DIR}/src/hidapi/ios/hid.m")
        set(SDL_FRAMEWORK_COREBLUETOOTH 1)
      endif()
      set(HAVE_SDL_HIDAPI TRUE)

      if(SDL_JOYSTICK AND SDL_HIDAPI_JOYSTICK)
        set(SDL_JOYSTICK_HIDAPI 1)
        set(HAVE_SDL_JOYSTICK TRUE)
        set(HAVE_HIDAPI_JOYSTICK TRUE)
        sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/hidapi/*.c")
      endif()
    else()
      set(SDL_HIDAPI_DISABLED 1)
    endif()
  else()
    set(SDL_HIDAPI_DISABLED 1)
  endif()
endmacro()

# Requires:
# - n/a
macro(CheckRPI)
  if(SDL_RPI)
    set(BCM_HOST_PKG_CONFIG_SPEC bcm_host)
    set(BRCMEGL_PKG_CONFIG_SPEC brcmegl)

    set(original_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_PATH} "${original_PKG_CONFIG_PATH}:/opt/vc/lib/pkgconfig")
    set(PC_BCM_HOST_FOUND FALSE)
    set(PC_BRCMEGL_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_BCM_HOST IMPORTED_TARGET QUIET ${BCM_HOST_PKG_CONFIG_SPEC})
      pkg_check_modules(PC_BRCMEGL IMPORTED_TARGET QUIET ${BRCMEGL_PKG_CONFIG_SPEC})
    endif()
    set(ENV{PKG_CONFIG_PATH} "${original_PKG_CONFIG_PATH}")

    if(TARGET PkgConfig::PC_BCM_HOST AND TARGET PkgConfig::PC_BRCMEGL)
      set(HAVE_RPI TRUE)
      if(SDL_VIDEO)
        set(HAVE_SDL_VIDEO TRUE)
        set(SDL_VIDEO_DRIVER_RPI 1)
        sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/raspberry/*.c")
        sdl_link_dependency(rpi-video LIBS PkgConfig::PC_BCM_HOST PKG_CONFIG_PREFIX PC_BCM_HOST PKG_CONFIG_SPECS ${BCM_HOST_PKG_CONFIG_SPEC})
      endif()
    endif()
  endif()
endmacro()

# Requires:
# - n/a
macro(CheckROCKCHIP)
  if(SDL_ROCKCHIP)
    set(MALI_PKG_CONFIG_SPEC mali)
    set(PC_MALI_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_MALI QUIET ${MALI_PKG_CONFIG_SPEC})
    endif()
    if(PC_MALI_FOUND)
      set(HAVE_ROCKCHIP TRUE)
    endif()
    if(SDL_VIDEO AND HAVE_ROCKCHIP)
      set(HAVE_SDL_VIDEO TRUE)
      set(SDL_VIDEO_DRIVER_ROCKCHIP 1)
    endif()
  endif()
endmacro()

# Requires:
# - EGL
# - PkgCheckModules
# Optional:
# - SDL_KMSDRM_SHARED opt
# - HAVE_SDL_LOADSO opt
macro(CheckKMSDRM)
  if(SDL_KMSDRM)
    set(PKG_CONFIG_LIBDRM_SPEC libdrm)
    set(PKG_CONFIG_GBM_SPEC gbm)
    set(PC_LIBDRM_FOUND FALSE)
    set(PC_GBM_FOUND FALSE)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_LIBDRM IMPORTED_TARGET ${PKG_CONFIG_LIBDRM_SPEC})
      pkg_check_modules(PC_GBM IMPORTED_TARGET ${PKG_CONFIG_GBM_SPEC})
    endif()
    if(PC_LIBDRM_FOUND AND PC_GBM_FOUND AND HAVE_OPENGL_EGL)
      set(HAVE_KMSDRM TRUE)
      set(HAVE_SDL_VIDEO TRUE)

      sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/kmsdrm/*.c")

      set(SDL_VIDEO_DRIVER_KMSDRM 1)

      if(SDL_KMSDRM_SHARED AND NOT HAVE_SDL_LOADSO)
        message(WARNING "You must have SDL_LoadObject() support for dynamic KMS/DRM loading")
      endif()
      set(HAVE_KMSDRM_SHARED FALSE)
      if(SDL_KMSDRM_SHARED AND HAVE_SDL_LOADSO)
        FindLibraryAndSONAME(drm LIBDIRS ${PC_LIBDRM_LIBRARY_DIRS})
        FindLibraryAndSONAME(gbm LIBDIRS ${PC_GBM_LIBRARY_DIRS})
        if(DRM_LIB AND DRM_SHARED AND GBM_LIB AND GBM_SHARED)
          set(SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC "\"${DRM_LIB_SONAME}\"")
          set(SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC_GBM "\"${GBM_LIB_SONAME}\"")
          set(HAVE_KMSDRM_SHARED TRUE)
          sdl_link_dependency(kmsdrm-drm INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_LIBDRM,INTERFACE_INCLUDE_DIRECTORIES>)
          sdl_link_dependency(kmsdrm-gbm INCLUDES $<TARGET_PROPERTY:PkgConfig::PC_GBM,INTERFACE_INCLUDE_DIRECTORIES>)
        endif()
      endif()
      if(NOT HAVE_KMSDRM_SHARED)
        sdl_link_dependency(kmsdrm-libdrm LIBS PkgConfig::PC_LIBDRM PKG_CONFIG_PREFIX PC_LIBDRM PKG_CONFIG_SPECS ${PKG_CONFIG_LIBDRM_SPEC})
        sdl_link_dependency(kmsdrm-gbm LIBS PkgConfig::PC_GBM PKG_CONFIG_PREFIX PC_GBM PKG_CONFIG_SPECS ${PKG_CONFIG_GBM_SPEC})
      endif()
    endif()
  endif()
endmacro()

macro(CheckLibUDev)
  if(SDL_LIBUDEV)
    check_include_file("libudev.h" HAVE_LIBUDEV_HEADER)
    if(HAVE_LIBUDEV_HEADER)
      set(HAVE_LIBUDEV_H TRUE)
      FindLibraryAndSONAME(udev)
      if(UDEV_LIB_SONAME)
        set(SDL_UDEV_DYNAMIC "\"${UDEV_LIB_SONAME}\"")
        set(HAVE_LIBUDEV TRUE)
      endif()
    endif()
  endif()
endmacro()

macro(CheckLibUnwind)
  if(TARGET SDL3_test)
    set(found_libunwind FALSE)
    set(_libunwind_src [==[
      #include <libunwind.h>
      int main(int argc, char *argv[]) {
        (void)argc; (void)argv;
        unw_context_t context;
        unw_cursor_t cursor;
        unw_word_t pc;
        char sym[256];
        unw_word_t offset;
        unw_getcontext(&context);
        unw_step(&cursor);
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        unw_get_proc_name(&cursor, sym, sizeof(sym), &offset);
        return 0;
      }]==])

    if(NOT found_libunwind)
      cmake_push_check_state()
      check_c_source_compiles("${_libunwind_src}" LIBC_HAS_WORKING_LIBUNWIND)
      cmake_pop_check_state()
      if(LIBC_HAS_WORKING_LIBUNWIND)
        set(found_libunwind TRUE)
        target_compile_definitions(SDL3_test PRIVATE HAVE_LIBUNWIND_H)
      endif()
    endif()

    if(NOT found_libunwind)
      cmake_push_check_state()
      list(APPEND CMAKE_REQUIRED_LIBRARIES "unwind")
      check_c_source_compiles("${_libunwind_src}" LIBUNWIND_HAS_WORKINGLIBUNWIND)
      cmake_pop_check_state()
      if(LIBUNWIND_HAS_WORKINGLIBUNWIND)
        set(found_libunwind TRUE)
        sdl_test_link_dependency(UNWIND LIBS unwind)
      endif()
    endif()

    if(NOT found_libunwind)
      set(LibUnwind_PKG_CONFIG_SPEC libunwind libunwind-generic)
      set(PC_LIBUNWIND_FOUND FALSE)
      if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_LIBUNWIND IMPORTED_TARGET ${LibUnwind_PKG_CONFIG_SPEC})
      endif()
      if(PC_LIBUNWIND_FOUND)
        cmake_push_check_state()
        list(APPEND CMAKE_REQUIRED_LIBRARIES ${PC_LIBUNWIND_LIBRARIES})
        list(APPEND CMAKE_REQUIRED_INCLUDES ${PC_LIBUNWIND_INCLUDE_DIRS})
        check_c_source_compiles("${_libunwind_src}" PC_LIBUNWIND_HAS_WORKING_LIBUNWIND)
        cmake_pop_check_state()
        if(PC_LIBUNWIND_HAS_WORKING_LIBUNWIND)
          set(found_libunwind TRUE)
          sdl_test_link_dependency(UNWIND LIBS PkgConfig::PC_LIBUNWIND PKG_CONFIG_PREFIX PC_LIBUNWIND PKG_CONFIG_SPECS ${LibUnwind_PKG_CONFIG_SPEC})
        endif()
      endif()
    endif()

    if(found_libunwind)
      target_compile_definitions(SDL3_test PRIVATE HAVE_LIBUNWIND_H)
    endif()
  endif()
endmacro()
