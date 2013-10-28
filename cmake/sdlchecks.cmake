macro(CheckDLOPEN)
  check_function_exists(dlopen HAVE_DLOPEN)
  if(NOT HAVE_DLOPEN)
    foreach(_LIBNAME dl tdl)
      check_library_exists("${_LIBNAME}" "dlopen" "" DLOPEN_LIB)
      if(DLOPEN_LIB)
        list(APPEND EXTRA_LIBS ${_LIBNAME})
        set(_DLLIB ${_LIBNAME})
        set(HAVE_DLOPEN TRUE)
        break()
      endif(DLOPEN_LIB)
    endforeach()
  endif()

  if(HAVE_DLOPEN)
    if(_DLLIB)
      set(CMAKE_REQUIRED_LIBRARIES ${_DLLIB})
    endif()
    check_c_source_compiles("
       #include <dlfcn.h>
       int main(int argc, char **argv) {
         void *handle = dlopen(\"\", RTLD_NOW);
         const char *loaderror = (char *) dlerror();
       }" HAVE_DLOPEN)
    set(CMAKE_REQUIRED_LIBRARIES)
  endif()

  if (HAVE_DLOPEN)
    set(SDL_LOADSO_DLOPEN 1)
    set(HAVE_SDL_DLOPEN TRUE)
    file(GLOB DLOPEN_SOURCES ${SDL2_SOURCE_DIR}/src/loadso/dlopen/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${DLOPEN_SOURCES})
    set(HAVE_SDL_LOADSO TRUE)
  endif()
endmacro(CheckDLOPEN)

# Requires:
# - n/a
macro(CheckOSS)
  if(OSS)
    set(OSS_HEADER_FILE "sys/soundcard.h")
    check_c_source_compiles("
        #include <sys/soundcard.h>
        int main() { int arg = SNDCTL_DSP_SETFRAGMENT; }" OSS_FOUND)
    if(NOT OSS_FOUND)
      set(OSS_HEADER_FILE "soundcard.h")
      check_c_source_compiles("
          #include <soundcard.h>
          int main() { int arg = SNDCTL_DSP_SETFRAGMENT; }" OSS_FOUND)
    endif(NOT OSS_FOUND)

    if(OSS_FOUND)
      set(HAVE_OSS TRUE)
      file(GLOB OSS_SOURCES ${SDL2_SOURCE_DIR}/src/audio/dsp/*.c)
      if(OSS_HEADER_FILE STREQUAL "soundcard.h")
        set(SDL_AUDIO_DRIVER_OSS_SOUNDCARD_H 1)
      endif(OSS_HEADER_FILE STREQUAL "soundcard.h")
      set(SDL_AUDIO_DRIVER_OSS 1)
      set(SOURCE_FILES ${SOURCE_FILES} ${OSS_SOURCES})
      if(NETBSD OR OPENBSD)
        list(APPEND EXTRA_LIBS ossaudio)
      endif(NETBSD OR OPENBSD)
      set(HAVE_SDL_AUDIO TRUE)
    endif(OSS_FOUND)
  endif(OSS)
endmacro(CheckOSS)

# Requires:
# - n/a
# Optional:
# - ALSA_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckALSA)
  if(ALSA)
    CHECK_INCLUDE_FILE(alsa/asoundlib.h HAVE_ASOUNDLIB_H)
    if(HAVE_ASOUNDLIB_H)
      CHECK_LIBRARY_EXISTS(asound snd_pcm_open "" HAVE_LIBASOUND)
      set(HAVE_ALSA TRUE)
      file(GLOB ALSA_SOURCES ${SDL2_SOURCE_DIR}/src/audio/alsa/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${ALSA_SOURCES})
      set(SDL_AUDIO_DRIVER_ALSA 1)
      if(ALSA_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic ALSA loading")
        else()
          find_library(_ALSA_LIB asound)
          get_filename_component(F_ALSA_LIB ${_ALSA_LIB} NAME)
          set(SDL_AUDIO_DRIVER_ALSA_DYNAMIC "\"${F_ALSA_LIB}\"")
          set(HAVE_ALSA_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(ALSA_SHARED)
        list(APPEND EXTRA_LIBS asound)
      endif(ALSA_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(HAVE_ASOUNDLIB_H)
  endif(ALSA)
endmacro(CheckALSA)

# Requires:
# - PkgCheckModules
# Optional:
# - PULSEAUDIO_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckPulseAudio)
  if(PULSEAUDIO)
    pkg_check_modules(PKG_PULSEAUDIO libpulse-simple)
    if(PKG_PULSEAUDIO_FOUND)
      set(HAVE_PULSEAUDIO TRUE)
      file(GLOB PULSEAUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/pulseaudio/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${PULSEAUDIO_SOURCES})
      set(SDL_AUDIO_DRIVER_PULSEAUDIO 1)
      list(APPEND EXTRA_CFLAGS ${PKG_PULSEAUDIO_CFLAGS})
      if(PULSEAUDIO_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic PulseAudio loading")
        else()
          find_library(D_PULSE_LIB pulse-simple)
          get_filename_component(F_PULSE_LIB ${D_PULSE_LIB} NAME)
          set(SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC "\"${F_PULSE_LIB}\"")
          set(HAVE_PULSEAUDIO_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(PULSEAUDIO_SHARED)
        list(APPEND EXTRA_LDFLAGS ${PKG_PULSEAUDIO_LDFLAGS})
      endif(PULSEAUDIO_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(PKG_PULSEAUDIO_FOUND)
  endif(PULSEAUDIO)
endmacro(CheckPulseAudio)

# Requires:
# - PkgCheckModules
# Optional:
# - ESD_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckESD)
  if(ESD)
    pkg_check_modules(PKG_ESD esound)
    if(PKG_ESD_FOUND)
      set(HAVE_ESD TRUE)
      file(GLOB ESD_SOURCES ${SDL2_SOURCE_DIR}/src/audio/esd/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${ESD_SOURCES})
      set(SDL_AUDIO_DRIVER_ESD 1)
      list(APPEND EXTRA_CFLAGS ${PKG_ESD_CFLAGS})
      if(ESD_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic ESD loading")
        else()
          find_library(D_ESD_LIB esd)
          get_filename_component(F_ESD_LIB ${D_ESD_LIB} NAME)
          set(SDL_AUDIO_DRIVER_ESD_DYNAMIC "\"${F_ESD_LIB}\"")
          set(HAVE_ESD_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(ESD_SHARED)
        list(APPEND EXTRA_LDFLAGS ${PKG_ESD_LDFLAGS})
      endif(ESD_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(PKG_ESD_FOUND)
  endif(ESD)
endmacro(CheckESD)

# Requires:
# - n/a
# Optional:
# - ARTS_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckARTS)
  if(ARTS)
    find_program(ARTS_CONFIG arts-config)
    if(ARTS_CONFIG)
      execute_process(CMD_ARTSCFLAGS ${ARTS_CONFIG} --cflags
        OUTPUT_VARIABLE ARTS_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
      list(APPEND EXTRA_CFLAGS ${ARTS_CFLAGS})
      execute_process(CMD_ARTSLIBS ${ARTS_CONFIG} --libs
        OUTPUT_VARIABLE ARTS_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
      file(GLOB ARTS_SOURCES ${SDL2_SOURCE_DIR}/src/audio/arts/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${ARTS_SOURCES})
      set(SDL_AUDIO_DRIVER_ARTS 1)
      set(HAVE_ARTS TRUE)
      if(ARTS_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic ARTS loading")
        else()
          # TODO
          find_library(D_ARTS_LIB artsc)
          get_filename_component(F_ARTS_LIB ${D_ARTS_LIB} NAME)
          set(SDL_AUDIO_DRIVER_ARTS_DYNAMIC "\"${F_ARTS_LIB}\"")
          set(HAVE_ARTS_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(ARTS_SHARED)
        list(APPEND EXTRA_LDFLAGS ${ARTS_LIBS})
      endif(ARTS_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(ARTS_CONFIG)
  endif(ARTS)
endmacro(CheckARTS)

# Requires:
# - n/a
# Optional:
# - NAS_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckNAS)
  if(NAS)
    # TODO: set include paths properly, so the NAS headers are found
    check_include_file(audio/audiolib.h HAVE_NAS_H)
    find_library(D_NAS_LIB audio)
    if(HAVE_NAS_H AND D_NAS_LIB)
      set(HAVE_NAS TRUE)
      file(GLOB NAS_SOURCES ${SDL2_SOURCE_DIR}/src/audio/nas/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${NAS_SOURCES})
      set(SDL_AUDIO_DRIVER_NAS 1)
      if(NAS_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic NAS loading")
        else()
          get_filename_component(F_NAS_LIB ${D_NAS_LIB} NAME)
          set(SDL_AUDIO_DRIVER_NAS_DYNAMIC "\"${F_NAS_LIB}\"")
          set(HAVE_NAS_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(NAS_SHARED)
        list(APPEND EXTRA_LIBS ${D_NAS_LIB})
      endif(NAS_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(HAVE_NAS_H AND D_NAS_LIB)
  endif(NAS)
endmacro(CheckNAS)

# Requires:
# - n/a
# Optional:
# - SNDIO_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckSNDIO)
  if(SNDIO)
    # TODO: set include paths properly, so the sndio headers are found
    check_include_file(sndio.h HAVE_SNDIO_H)
    find_library(D_SNDIO_LIB audio)
    if(HAVE_SNDIO_H AND D_SNDIO_LIB)
      set(HAVE_SNDIO TRUE)
      file(GLOB SNDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/sndio/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${SNDIO_SOURCES})
      set(SDL_AUDIO_DRIVER_SNDIO 1)
      if(SNDIO_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic sndio loading")
        else()
          get_filename_component(F_SNDIO_LIB ${D_SNDIO_LIB} NAME)
          set(SDL_AUDIO_DRIVER_SNDIO_DYNAMIC "\"${F_SNDIO_LIB}\"")
          set(HAVE_SNDIO_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(SNDIO_SHARED)
        list(APPEND EXTRA_LIBS ${D_SNDIO_LIB})
      endif(SNDIO_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(HAVE_SNDIO_H AND D_SNDIO_LIB)
  endif(SNDIO)
endmacro(CheckSNDIO)

# Requires:
# - PkgCheckModules
# Optional:
# - FUSIONSOUND_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckFusionSound)
  if(FUSIONSOUND)
    pkg_check_modules(PKG_FUSIONSOUND fusionsound>=1.0.0)
    if(PKG_FUSIONSOUND_FOUND)
      set(HAVE_FUSIONSOUND TRUE)
      file(GLOB FUSIONSOUND_SOURCES ${SDL2_SOURCE_DIR}/src/audio/fusionsound/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${FUSIONSOUND_SOURCES})
      set(SDL_AUDIO_DRIVER_FUSIONSOUND 1)
      list(APPEND EXTRA_CFLAGS ${PKG_FUSIONSOUND_CFLAGS})
      if(FUSIONSOUND_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic FusionSound loading")
        else()
          find_library(D_FUSIONSOUND_LIB fusionsound)
          get_filename_component(F_FUSIONSOUND_LIB ${D_FUSIONSOUND_LIB} NAME)
          set(SDL_AUDIO_DRIVER_FUSIONSOUND_DYNAMIC "\"${F_FUSIONSOUND_LIB}\"")
          set(HAVE_FUSIONSOUND_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(FUSIONSOUND_SHARED)
        list(APPEND EXTRA_LDFLAGS ${PKG_FUSIONSOUND_LDFLAGS})
      endif(FUSIONSOUND_SHARED)
      set(HAVE_SDL_AUDIO TRUE)
    endif(PKG_FUSIONSOUND_FOUND)
  endif(FUSIONSOUND)
endmacro(CheckFusionSound)

# Requires:
# - n/a
# Optional:
# - X11_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckX11)
  if(VIDEO_X11)
    foreach(_LIB X11 Xext Xcursor Xinerama Xi Xrandr Xrender Xss Xxf86vm)
      string(TOUPPER ${_LIB} _LNAME)
      find_library(${_LNAME}_LIB ${_LIB})
      if(${_LNAME}_LIB)
        # reduce the library name for shared linking
        get_filename_component(_TMP ${${_LNAME}_LIB} NAME)
        set(${_LNAME}_LIB ${_TMP})
      endif()
    endforeach()

    find_path(X_INCLUDEDIR X11/Xlib.h)
    if(X_INCLUDEDIR)
      set(X_CFLAGS "-I${X_INCLUDEDIR}")
    endif()

    check_include_file(X11/Xcursor/Xcursor.h HAVE_XCURSOR_H)
    check_include_file(X11/extensions/Xinerama.h HAVE_XINERAMA_H)
    check_include_file(X11/extensions/XInput2.h HAVE_XINPUT_H)
    check_include_file(X11/extensions/Xrandr.h HAVE_XRANDR_H)
    check_include_file(X11/extensions/Xrender.h HAVE_XRENDER_H)
    check_include_file(X11/extensions/scrnsaver.h HAVE_XSS_H)
    check_include_file(X11/extensions/shape.h HAVE_XSHAPE_H)
    check_include_files("X11/Xlib.h;X11/extensions/xf86vmode.h" HAVE_XF86VM_H)
    check_include_files("X11/Xlib.h;X11/Xproto.h;X11/extensions/Xext.h" HAVE_XEXT_H)

    if(X11_LIB)
      if(NOT HAVE_XEXT_H)
        message_error("Missing Xext.h, maybe you need to install the libxext-dev package?")
      endif()

      set(HAVE_VIDEO_X11 TRUE)
      set(HAVE_SDL_VIDEO TRUE)

      file(GLOB X11_SOURCES ${SDL2_SOURCE_DIR}/src/video/x11/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${X11_SOURCES})
      set(SDL_VIDEO_DRIVER_X11 1)

      if(HAVE_GCC_FVISIBILITY)
        set(X11_SYMBOLS_PRIVATE TRUE)
      else()
        set(X11_SYMBOLS_PRIVATE FALSE)
      endif(HAVE_GCC_FVISIBILITY)

      if(APPLE)
        set(X11_SHARED OFF)
        set(X11_SYMBOLS_PRIVATE TRUE)
      endif(APPLE)

      check_function_exists("shmat" HAVE_SHMAT)
      if(NOT HAVE_SHMAT)
        check_library_exists(ipc shmat "" HAVE_SHMAT)
        if(HAVE_SHMAT)
          list(APPEND EXTRA_LIBS ipc)
        endif(HAVE_SHMAT)
        if(NOT HAVE_SHMAT)
          add_definitions(-DNO_SHARED_MEMORY)
          set(X_CFLAGS "${X_CFLAGS} -DNO_SHARED_MEMORY")
        endif(NOT HAVE_SHMAT)
      endif(NOT HAVE_SHMAT)

      if(X11_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic X11 loading")
          set(HAVE_X11_SHARED FALSE)
          if(X11_SYMBOLS_PRIVATE)
            message_warn("You must have gcc4 (-fvisibility=hidden) for dynamic X11 loading")
            set(HAVE_X11_SHARED TRUE)
          endif(X11_SYMBOLS_PRIVATE)
        else(NOT HAVE_DLOPEN)
          set(HAVE_X11_SHARED TRUE)
        endif()
        if(HAVE_X11_SHARED)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC "\"${X11_LIB}\"")
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT "\"${XEXT_LIB}\"")
        else(HAVE_X11_SHARED)
          list(APPEND EXTRA_LIBS ${X11_LIB} ${XEXT_LIB})
        endif(HAVE_X11_SHARED)
      endif(X11_SHARED)

      set(SDL_CFLAGS "${SDL_CFLAGS} ${X_CFLAGS}")

      set(CMAKE_REQUIRED_LIBRARIES ${X11_LIB} ${X11_LIB})
      check_c_source_compiles("
          #include <X11/Xlib.h>
          #include <X11/Xproto.h>
          #include <X11/extensions/Xext.h>
          #include <X11/extensions/extutil.h>
          extern XExtDisplayInfo* XextAddDisplay(XExtensionInfo* a,Display* b,_Xconst char* c,XExtensionHooks* d,int e,XPointer f);
          int main(int argc, char **argv) {}" HAVE_CONST_XEXT_ADDDISPLAY)
      if(HAVE_CONST_XEXT_ADDDISPLAY)
        set(SDL_VIDEO_DRIVER_X11_CONST_PARAM_XEXTADDDISPLAY 1)
      endif(HAVE_CONST_XEXT_ADDDISPLAY)

      check_c_source_compiles("
          #include <X11/Xlib.h>
          int main(int argc, char **argv) {
            Display *display;
            XEvent event;
            XGenericEventCookie *cookie = &event.xcookie;
            XNextEvent(display, &event);
            XGetEventData(display, cookie);
            XFreeEventData(display, cookie); }" HAVE_XGENERICEVENT)
      if(HAVE_XGENERICEVENT)
        set(SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS 1)
      endif(HAVE_XGENERICEVENT)

      check_c_source_compiles("
          #include <X11/Xlibint.h>
          extern int _XData32(Display *dpy,register _Xconst long *data,unsigned len);
          int main(int argc, char **argv) {}" HAVE_CONST_XDATA32)
      if(HAVE_CONST_XDATA32)
        set(SDL_VIDEO_DRIVER_X11_CONST_PARAM_XDATA32 1)
      endif(HAVE_CONST_XDATA32)

      check_function_exists(XkbKeycodeToKeysym SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM)

      if(VIDEO_X11_XCURSOR AND HAVE_XCURSOR_H)
        set(HAVE_VIDEO_X11_XCURSOR TRUE)
        if(HAVE_X11_SHARED AND XCURSOR_LIB)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR "\"${XCURSOR_LIB}\"")
        else(HAVE_X11_SHARED AND XCURSOR_LIB)
          list(APPEND EXTRA_LIBS ${XCURSOR_LIB})
        endif(HAVE_X11_SHARED AND XCURSOR_LIB)
        set(SDL_VIDEO_DRIVER_X11_XCURSOR 1)
      endif(VIDEO_X11_XCURSOR AND HAVE_XCURSOR_H)

      if(VIDEO_X11_XINERAMA AND HAVE_XINERAMA_H)
        set(HAVE_VIDEO_X11_XINERAMA TRUE)
        if(HAVE_X11_SHARED AND XINERAMA_LIB)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XINERAMA "\"${XINERAMA_LIB}\"")
        else(HAVE_X11_SHARED AND XINERAMA_LIB)
          list(APPEND EXTRA_LIBS ${XINERAMA_LIB})
        endif(HAVE_X11_SHARED AND XINERAMA_LIB)
        set(SDL_VIDEO_DRIVER_X11_XINERAMA 1)
      endif(VIDEO_X11_XINERAMA AND HAVE_XINERAMA_H)

      if(VIDEO_X11_XINPUT AND HAVE_XINPUT_H)
        set(HAVE_VIDEO_X11_XINPUT TRUE)
        if(HAVE_X11_SHARED AND XI_LIB)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2 "\"${XI_LIB}\"")
        else(HAVE_X11_SHARED AND XI_LIB)
          list(APPEND EXTRA_LIBS ${XI_LIB})
        endif(HAVE_X11_SHARED AND XI_LIB)
        set(SDL_VIDEO_DRIVER_X11_XINPUT2 1)

        # Check for multitouch
        check_c_source_compiles("
            #include <X11/Xlib.h>
            #include <X11/Xproto.h>
            #include <X11/extensions/XInput2.h>
            int event_type = XI_TouchBegin;
            XITouchClassInfo *t;
            Status XIAllowTouchEvents(Display *a,int b,unsigned int c,Window d,int f)
            {
              return (Status)0;
            }
            int main(int argc, char **argv) {}" HAVE_XINPUT2_MULTITOUCH)
        if(HAVE_XINPUT2_MULTITOUCH)
          set(SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH 1)
        endif(HAVE_XINPUT2_MULTITOUCH)
      endif(VIDEO_X11_XINPUT AND HAVE_XINPUT_H)

      if(VIDEO_X11_XRANDR AND HAVE_XRANDR_H)
        if(HAVE_X11_SHARED AND XRANDR_LIB)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR "\"${XRANDR_LIB}\"")
        else(HAVE_X11_SHARED AND XRANDR_LIB)
          list(APPEND EXTRA_LIBS ${XRANDR_LIB})
        endif(HAVE_X11_SHARED AND XRANDR_LIB)
        set(SDL_VIDEO_DRIVER_X11_XRANDR 1)
        set(HAVE_VIDEO_X11_XRANDR TRUE)
      endif(VIDEO_X11_XRANDR AND HAVE_XRANDR_H)

      if(VIDEO_X11_XSCRNSAVER AND HAVE_XSS_H)
        if(HAVE_X11_SHARED AND XSS_LIB)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS "\"${XSS_LIB}\"")
        else(HAVE_X11_SHARED AND XSS_LIB)
          list(APPEND EXTRA_LIBS ${XSS_LIB})
        endif(HAVE_X11_SHARED AND XSS_LIB)
        set(SDL_VIDEO_DRIVER_X11_XSCRNSAVER 1)
        set(HAVE_VIDEO_X11_XSCRNSAVER TRUE)
      endif(VIDEO_X11_XSCRNSAVER AND HAVE_XSS_H)

      if(VIDEO_X11_XSHAPE AND HAVE_XSHAPE_H)
        set(SDL_VIDEO_DRIVER_X11_XSHAPE 1)
        set(HAVE_VIDEO_X11_XSHAPE TRUE)
      endif(VIDEO_X11_XSHAPE AND HAVE_XSHAPE_H)

      if(VIDEO_X11_XVM AND HAVE_XF86VM_H)
        if(HAVE_X11_SHARED AND XF86VMODE_LIB)
          set(SDL_VIDEO_DRIVER_X11_DYNAMIC_XVIDMODE "\"${XF86VMODE_LIB}\"")
        else(HAVE_X11_SHARED AND XF86VMODE_LIB)
          list(APPEND EXTRA_LIBS ${XF86VMODE_LIB})
        endif(HAVE_X11_SHARED AND XF86VMODE_LIB)
        set(SDL_VIDEO_DRIVER_X11_XVIDMODE 1)
        set(HAVE_VIDEO_X11_XVM TRUE)
      endif(VIDEO_X11_XVM AND HAVE_XF86VM_H)

      set(CMAKE_REQUIRED_LIBRARIES)
    endif(X11_LIB)
  endif(VIDEO_X11)
endmacro(CheckX11)

# Requires:
# - n/a
#
macro(CheckCOCOA)
  if(VIDEO_COCOA)
    check_objc_source_compiles("
        #import <Cocoa/Cocoa.h>
        int main (int argc, char** argv) {}" HAVE_VIDEO_COCOA)
    if(HAVE_VIDEO_COCOA)
      file(GLOB COCOA_SOURCES ${SDL2_SOURCE_DIR}/src/video/cocoa/*.m)
      set_source_files_properties(${COCOA_SOURCES} PROPERTIES LANGUAGE C)
      set(SOURCE_FILES ${SOURCE_FILES} ${COCOA_SOURCES})
      set(SDL_VIDEO_DRIVER_COCOA 1)
      set(HAVE_SDL_VIDEO TRUE)
    endif(HAVE_VIDEO_COCOA)
  endif(VIDEO_COCOA)
endmacro(CheckCOCOA)

# Requires:
# - PkgCheckModules
# Optional:
# - DIRECTFB_SHARED opt
# - HAVE_DLOPEN opt
macro(CheckDirectFB)
  if(VIDEO_DIRECTFB)
    pkg_check_modules(PKG_DIRECTFB directfb>=1.0.0)
    if(PKG_DIRECTFB_FOUND)
      set(HAVE_VIDEO_DIRECTFB TRUE)
      file(GLOB DIRECTFB_SOURCES ${SDL2_SOURCE_DIR}/src/video/directfb/*.c)
      set(SOURCE_FILES ${SOURCE_FILES} ${DIRECTFB_SOURCES})
      set(SDL_VIDEO_DRIVER_DIRECTFB 1)
      set(SDL_VIDEO_RENDER_DIRECTFB 1)
      list(APPEND EXTRA_CFLAGS ${PKG_DIRECTFB_CFLAGS})
      if(DIRECTFB_SHARED)
        if(NOT HAVE_DLOPEN)
          message_warn("You must have SDL_LoadObject() support for dynamic DirectFB loading")
        else()
          find_library(D_DIRECTFB_LIB directfb)
          get_filename_component(F_DIRECTFB_LIB ${D_DIRECTFB_LIB} NAME)
          set(SDL_VIDEO_DRIVER_DIRECTFB_DYNAMIC "\"${F_DIRECTFB_LIB}\"")
          set(HAVE_DIRECTFB_SHARED TRUE)
        endif(NOT HAVE_DLOPEN)
      else(DIRECTFB_SHARED)
        list(APPEND EXTRA_LDFLAGS ${PKG_DIRECTFB_LDFLAGS})
      endif(DIRECTFB_SHARED)
      set(HAVE_SDL_VIDEO TRUE)
    endif(PKG_DIRECTFB_FOUND)
  endif(VIDEO_DIRECTFB)
endmacro(CheckDirectFB)

# Requires:
# - nada
macro(CheckOpenGLX11)
  if(VIDEO_OPENGL)
    check_c_source_compiles("
        #include <GL/gl.h>
        #include <GL/glx.h>
        int main(int argc, char** argv) {}" HAVE_VIDEO_OPENGL)

    if(HAVE_VIDEO_OPENGL)
      set(HAVE_VIDEO_OPENGL TRUE)
      set(SDL_VIDEO_OPENGL 1)
      set(SDL_VIDEO_OPENGL_GLX 1)
      set(SDL_VIDEO_RENDER_OGL 1)
      list(APPEND EXTRA_LIBS GL)
    endif(HAVE_VIDEO_OPENGL)
  endif(VIDEO_OPENGL)
endmacro(CheckOpenGLX11)

# Requires:
# - nada
macro(CheckOpenGLESX11)
  if(VIDEO_OPENGLES)
    check_c_source_compiles("
        #include <EGL/egl.h>
        int main (int argc, char** argv) {}" HAVE_VIDEO_OPENGL_EGL)
    if(HAVE_VIDEO_OPENGL_EGL)
        set(SDL_VIDEO_OPENGL_EGL 1)
    endif(HAVE_VIDEO_OPENGL_EGL) 
    check_c_source_compiles("
      #include <GLES/gl.h>
      #include <GLES/glext.h>
      int main (int argc, char** argv) {}" HAVE_VIDEO_OPENGLES_V1)
    if(HAVE_VIDEO_OPENGLES_V1)
        set(HAVE_VIDEO_OPENGLES TRUE)
        set(SDL_VIDEO_OPENGL_ES 1)
        set(SDL_VIDEO_RENDER_OGL_ES 1)
    endif(HAVE_VIDEO_OPENGLES_V1)
    check_c_source_compiles("
      #include <GLES2/gl2.h>
      #include <GLES2/gl2ext.h>
      int main (int argc, char** argv) {}" HAVE_VIDEO_OPENGLES_V2)
    if(HAVE_VIDEO_OPENGLES_V2)
        set(HAVE_VIDEO_OPENGLES TRUE)
        set(SDL_VIDEO_OPENGL_ES2 1)
        set(SDL_VIDEO_RENDER_OGL_ES2 1)
    endif(HAVE_VIDEO_OPENGLES_V2)

  endif(VIDEO_OPENGLES)
endmacro(CheckOpenGLESX11)

# Rquires:
# - nada
# Optional:
# - THREADS opt
# Sets:
# PTHREAD_CFLAGS
# PTHREAD_LIBS
macro(CheckPTHREAD)
  if(PTHREADS)
    if(LINUX)
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
      set(PTHREAD_LDFLAGS "-pthread")
    elseif(SOLARIS)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-pthread -lposix4")
    elseif(SYSV5)
      set(PTHREAD_CFLAGS "-D_REENTRANT -Kthread")
      set(PTHREAD_LDFLAGS "")
    elseif(AIX)
      set(PTHREAD_CFLAGS "-D_REENTRANT -mthreads")
      set(PTHREAD_LDFLAGS "-pthread")
    elseif(HPUX)
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-L/usr/lib -pthread")
    else()
      set(PTHREAD_CFLAGS "-D_REENTRANT")
      set(PTHREAD_LDFLAGS "-lpthread")
    endif(LINUX)

    # Run some tests
    set(CMAKE_REQUIRED_FLAGS "${PTHREAD_CFLAGS} ${PTHREAD_LDFLAGS}")
    check_c_source_runs("
        #include <pthread.h>
        int main(int argc, char** argv) {
          pthread_attr_t type;
          pthread_attr_init(&type);
          return 0;
        }" HAVE_PTHREADS)
    if(HAVE_PTHREADS)
      set(SDL_THREAD_PTHREAD 1)
      list(APPEND EXTRA_CFLAGS ${PTHREAD_CFLAGS})
      list(APPEND EXTRA_LDFLAGS ${PTHREAD_LDFLAGS})
      set(SDL_CFLAGS "${SDL_CFLAGS} ${PTHREAD_CFLAGS}")
      list(APPEND SDL_LIBS ${PTHREAD_LDFLAGS})

      check_c_source_compiles("
        #include <pthread.h>
        int main(int argc, char **argv) {
          pthread_mutexattr_t attr;
          pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
          return 0;
        }" HAVE_RECURSIVE_MUTEXES)
      if(HAVE_RECURSIVE_MUTEXES)
        set(SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1)
      else(HAVE_RECURSIVE_MUTEXES)
        check_c_source_compiles("
            #include <pthread.h>
            int main(int argc, char **argv) {
              pthread_mutexattr_t attr;
              pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
              return 0;
            }" HAVE_RECURSIVE_MUTEXES_NP)
        if(HAVE_RECURSIVE_MUTEXES_NP)
          set(SDL_THREAD_PTHREAD_RECURSIVE_MUTEX_NP 1)
        endif(HAVE_RECURSIVE_MUTEXES_NP)
      endif(HAVE_RECURSIVE_MUTEXES)

      if(PTHREADS_SEM)
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
              }" HAVE_SEM_TIMEDWAIT)
        endif(HAVE_PTHREADS_SEM)
      endif(PTHREADS_SEM)

      check_c_source_compiles("
          #include <pthread.h>
          int main(int argc, char** argv) {
              pthread_spin_trylock(NULL);
              return 0;
          }" HAVE_PTHREAD_SPINLOCK)

      check_c_source_compiles("
          #include <pthread.h>
          #include <pthread_np.h>
          int main(int argc, char** argv) { return 0; }" HAVE_PTHREAD_NP_H)
      check_function_exists(pthread_setname_np HAVE_PTHREAD_setNAME_NP)
      check_function_exists(pthread_set_name_np HAVE_PTHREAD_set_NAME_NP)
      set(CMAKE_REQUIRED_FLAGS)

      set(SOURCE_FILES ${SOURCE_FILES}
          ${SDL2_SOURCE_DIR}/src/thread/pthread/SDL_systhread.c
          ${SDL2_SOURCE_DIR}/src/thread/pthread/SDL_sysmutex.c   # Can be faked, if necessary
          ${SDL2_SOURCE_DIR}/src/thread/pthread/SDL_syscond.c    # Can be faked, if necessary
          ${SDL2_SOURCE_DIR}/src/thread/pthread/SDL_systls.c
          )
      if(HAVE_PTHREADS_SEM)
        set(SOURCE_FILES ${SOURCE_FILES}
            ${SDL2_SOURCE_DIR}/src/thread/pthread/SDL_syssem.c)
      else(HAVE_PTHREADS_SEM)
        set(SOURCE_FILES ${SOURCE_FILES}
            ${SDL2_SOURCE_DIR}/src/thread/generic/SDL_syssem.c)
      endif(HAVE_PTHREADS_SEM)
      set(HAVE_SDL_THREADS TRUE)
    endif(HAVE_PTHREADS)
  endif(PTHREADS)
endmacro(CheckPTHREAD)

# Requires
# - nada
# Optional:
# Sets:
# USB_LIBS
# USB_CFLAGS
macro(CheckUSBHID)
  check_library_exists(usbhid hid_init "" LIBUSBHID)
  if(LIBUSBHID)
    check_include_file(usbhid.h HAVE_USBHID_H)
    if(HAVE_USBHID_H)
      set(USB_CFLAGS "-DHAVE_USBHID_H")
    endif(HAVE_USBHID_H)

    check_include_file(libusbhid.h HAVE_LIBUSBHID_H)
    if(HAVE_LIBUSBHID_H)
      set(USB_CFLAGS "${USB_CFLAGS} -DHAVE_LIBUSBHID_H")
    endif(HAVE_LIBUSBHID_H)
    set(USB_LIBS ${USB_LIBS} usbhid)
  else(LIBUSBHID)
    check_include_file(usb.h HAVE_USB_H)
    if(HAVE_USB_H)
      set(USB_CFLAGS "-DHAVE_USB_H")
    endif(HAVE_USB_H)
    check_include_file(libusb.h HAVE_LIBUSB_H)
    if(HAVE_LIBUSB_H)
      set(USB_CFLAGS "${USB_CFLAGS} -DHAVE_LIBUSB_H")
    endif(HAVE_LIBUSB_H)
    check_library_exists(usb hid_init "" LIBUSB)
    if(LIBUSB)
      set(USB_LIBS ${USB_LIBS} usb)
    endif(LIBUSB)
  endif(LIBUSBHID)

  set(CMAKE_REQUIRED_FLAGS "${USB_CFLAGS}")
  set(CMAKE_REQUIRED_LIBRARIES "${USB_LIBS}")
  check_c_source_compiles("
       #include <sys/types.h>
        #if defined(HAVE_USB_H)
        #include <usb.h>
        #endif
        #ifdef __DragonFly__
        # include <bus/usb/usb.h>
        # include <bus/usb/usbhid.h>
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
          # include <bus/usb/usb.h>
          # include <bus/usb/usbhid.h>
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
      set(USB_CFLAGS "${USB_CFLAGS} -DUSBHID_UCR_DATA")
    endif(HAVE_USBHID_UCR_DATA)

    check_c_source_compiles("
          #include <sys/types.h>
          #if defined(HAVE_USB_H)
          #include <usb.h>
          #endif
          #ifdef __DragonFly__
          #include <bus/usb/usb.h>
          #include <bus/usb/usbhid.h>
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
      set(USB_CFLAGS "${USB_CFLAGS} -DUSBHID_NEW")
    endif(HAVE_USBHID_NEW)

    check_c_source_compiles("
        #include <machine/joystick.h>
        int main(int argc, char** argv) {
            struct joystick t;
            return 0;
        }" HAVE_MACHINE_JOYSTICK)
    if(HAVE_MACHINE_JOYSTICK)
      set(SDL_JOYSTICK_USBHID_MACHINE_JOYSTICK_H 1)
    endif(HAVE_MACHINE_JOYSTICK)
    set(SDL_JOYSTICK_USBHID 1)
    file(GLOB BSD_JOYSTICK_SOURCES ${SDL2_SOURCE_DIR}/src/joystick/bsd/*.c)
    set(SOURCE_FILES ${SOURCE_FILES} ${BSD_JOYSTICK_SOURCES})
    list(APPEND EXTRA_CFLAGS ${USB_CFLAGS})
    list(APPEND EXTRA_LIBS ${USB_LIBS})
    set(HAVE_SDL_JOYSTICK TRUE)

    set(CMAKE_REQUIRED_LIBRARIES)
    set(CMAKE_REQUIRED_FLAGS)
  endif(HAVE_USBHID)
endmacro(CheckUSBHID)
