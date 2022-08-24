include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

include(FindPackageHandleStandardArgs)

cmake_minimum_required(VERSION 3.3)

find_path(X11SDL_INCLUDE_PATH
  NAMES X11/Xlib.h
  PATHS
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

set(_X11SDL_required_vars X11SDL_INCLUDE_PATH)

function(x11sdl_find_component NAME HDRFILENAME LIBNAME MODULESPEC)
  set(header_found FALSE)
  set(lib_found FALSE)
  if(MODULESPEC)
    set(X11SDL_${NAME}_PKGCONFIG_MODULESPEC "${MODULESPEC}")
  endif()
  if(HDRFILENAME)
    find_path(X11SDL_${NAME}_INCLUDE_PATH
      NAMES "${HDRFILENAME}"
      HINTS "${X11SDL_INCLUDE_PATHS}"
    )

    if(X11SDL_${NAME}_INCLUDE_PATH)
      set(header_found TRUE)
    endif()
    set(X11SDL_${NAME}_HEADERS_FOUND ${header_found} PARENT_COPE)

    if("${NAME}_HEADERS" IN_LIST X11SDL_FIND_COMPONENTS)
      list(APPEND _X11SDL_required_vars X11SDL_${NAME}_INCLUDE_PATHS)
    endif()

    if(header_found AND NOT TARGET X11SDL::${NAME}::Headers)
      add_library(X11SDL::${NAME}::Headers INTERFACE IMPORTED)
      set_target_properties(X11SDL::${NAME}::Headers
        PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${X11SDL_${NAME}_INCLUDE_PATH}"
      )
    endif()

  endif()

  if(LIBNAME)
    set(X11SDL_${NAME}_FOUND FALSE)
    find_library(X11SDL_${NAME}_LIBRARY
      NAMES "${LIBNAME}"
    )

    if(HDRFILENAME)
      if(header_found AND X11SDL_${NAME}_LIBRARY)
        set(lib_found TRUE)
      endif()
    else()
      if(X11SDL_${NAME}_LIBRARY)
        set(lib_found TRUE)
      endif()
    endif()
    set(X11SDL_${NAME}_FOUND ${lib_found} PARENT_SCOPE)

    if("${NAME}" IN_LIST X11SDL_FIND_COMPONENTS)
      list(APPEND _X11SDL_required_vars X11SDL_${NAME}_LIBRARY)
      if(HDRFILENAME)
        list(APPEND _X11SDL_required_vars X11SDL_${NAME}_INCLUDE_PATHS)
      endif()
    endif()

    if(lib_found AND NOT TARGET X11SDL::${NAME})
      add_library(X11SDL::${NAME} SHARED IMPORTED)
      set_target_properties(X11SDL::${NAME}
        PROPERTIES
          IMPORTED_LOCATION "${X11SDL_${NAME}_LIBRARY}"
      )
      sdl_add_soname_library(X11SDL::${NAME})
      if(header_found)
        set_target_properties(X11SDL::${NAME}
          PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${X11SDL_${NAME}_INCLUDE_PATH}"
        )
      endif()
    endif()

  endif()
  set(_X11SDL_required_vars "${_X11SDL_required_vars}" PARENT_SCOPE)
endfunction()

x11sdl_find_component(X11       "X11/Xlib.h"                  "X11"       "")
x11sdl_find_component(XCURSOR   "X11/Xcursor/Xcursor.h"       "Xcursor"   "xcursor")
x11sdl_find_component(XDBE      "X11/extensions/Xdbe.h"       ""          "")
x11sdl_find_component(XEXT      "X11/extensions/Xext.h"       "Xext"      "xext")
x11sdl_find_component(XFIXES    "X11/extensions/Xfixes.h"     "Xfixes"    "xfixes")
x11sdl_find_component(XINPUT    "X11/extensions/XInput2.h"    "Xi"        "xi")
x11sdl_find_component(XRANDR    "X11/extensions/Xrandr.h"     "Xrandr"    "")
x11sdl_find_component(XRENDER   "X11/extensions/Xrender.h"    "Xrender"   "xrender")
x11sdl_find_component(XSHAPE    "X11/extensions/shape.h"      ""          "xcb-shape")
x11sdl_find_component(XSS       "X11/extensions/scrnsaver.h"  "Xss"       "xscrnsaver")

find_package_handle_standard_args(X11SDL
  REQUIRED_VARS ${_X11SDL_required_vars}
)
