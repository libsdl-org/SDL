# - Try to find the required ffmpeg components(default: AVFORMAT, AVUTIL, AVCODEC)
#
# Once done this will define
#  FFMPEG_FOUND         - System has the all required components.
#  FFMPEG_LIBRARIES     - Link these to use the required ffmpeg components.
#
# For each of the components it will additionally set.
#   - AVCODEC
#   - AVDEVICE
#   - AVFORMAT
#   - AVFILTER
#   - AVUTIL
#   - POSTPROC
#   - SWSCALE
# the following target will be defined
#  FFmpeg::SDL::<component>        - link to this target to
# the following variables will be defined
#  FFmpeg_<component>_FOUND        - System has <component>
#  FFmpeg_<component>_INCLUDE_DIRS - Include directory necessary for using the <component> headers
#  FFmpeg_<component>_LIBRARIES    - Link these to use <component>
#  FFmpeg_<component>_DEFINITIONS  - Compiler switches required for using <component>
#  FFmpeg_<component>_VERSION      - The components version
#
# Copyright (c) 2006, Matthias Kretz, <kretz@kde.org>
# Copyright (c) 2008, Alexander Neundorf, <neundorf@kde.org>
# Copyright (c) 2011, Michael Jansen, <kde@michael-jansen.biz>
# Copyright (c) 2023, Sam lantinga, <slouken@libsdl.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(FindPackageHandleStandardArgs)
include("${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake")

# The default components were taken from a survey over other FindFFMPEG.cmake files
if(NOT FFmpeg_FIND_COMPONENTS)
  set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL)
  foreach(_component IN LISTS FFmpeg_FIND_COMPONENTS)
    set(FFmpeg_FIND_REQUIRED_${_component} TRUE)
  endforeach()
endif()

find_package(PkgConfig QUIET)

#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
macro(find_component _component _pkgconfig _library _header)

  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_${_component} QUIET ${_pkgconfig})
  endif()

  find_path(FFmpeg_${_component}_INCLUDE_DIRS
    NAMES ${_header}
    HINTS
      ${PC_${_component}_INCLUDE_DIRS}
    PATH_SUFFIXES
      ffmpeg
  )

  find_library(FFmpeg_${_component}_LIBRARY
    NAMES ${_library}
    HINTS
      ${PC_${_component}_LIBRARY_DIRS}
  )

  if(FFmpeg_${_component}_INCLUDE_DIRS AND FFmpeg_${_component}_LIBRARY)
    set(FFmpeg_${_component}_FOUND TRUE)
  endif()

  if(PC_${_component}_FOUND)
    get_flags_from_pkg_config("${FFmpeg_${_component}_LIBRARY}" "PC_${_component}" "${_component}")
  endif()

  set(FFmpeg_${_component}_VERSION "${PC_${_component}_VERSION}")

  set(FFmpeg_${_component}_COMPILE_OPTIONS "${${_component}_options}" CACHE STRING "Extra compile options of FFmpeg ${_component}")

  set(FFmpeg_${_component}_LIBRARIES "${${_component}_link_libraries}" CACHE STRING "Extra link libraries of FFmpeg ${_component}")

  set(FFmpeg_${_component}_LINK_OPTIONS "${${_component}_link_options}" CACHE STRING "Extra link flags of FFmpeg ${_component}")

  set(FFmpeg_${_component}_LINK_DIRECTORIES "${${_component}_link_directories}" CACHE PATH "Extra link directories of FFmpeg ${_component}")

  mark_as_advanced(
    FFmpeg_${_component}_INCLUDE_DIRS
    FFmpeg_${_component}_LIBRARY
    FFmpeg_${_component}_COMPILE_OPTIONS
    FFmpeg_${_component}_LIBRARIES
    FFmpeg_${_component}_LINK_OPTIONS
    FFmpeg_${_component}_LINK_DIRECTORIES
  )
endmacro()

# Check for all possible component.
find_component(AVCODEC    libavcodec    avcodec  libavcodec/avcodec.h)
find_component(AVFORMAT   libavformat   avformat libavformat/avformat.h)
find_component(AVDEVICE   libavdevice   avdevice libavdevice/avdevice.h)
find_component(AVUTIL     libavutil     avutil   libavutil/avutil.h)
find_component(AVFILTER   libavfilter   avfilter libavfilter/avfilter.h)
find_component(SWSCALE    libswscale    swscale  libswscale/swscale.h)
find_component(POSTPROC   libpostproc   postproc libpostproc/postprocess.h)
find_component(SWRESAMPLE libswresample swresample libswresample/swresample.h)

# Compile the list of required vars
set(_FFmpeg_REQUIRED_VARS)
foreach(_component ${FFmpeg_FIND_COMPONENTS})
  list(APPEND _FFmpeg_REQUIRED_VARS FFmpeg_${_component}_INCLUDE_DIRS FFmpeg_${_component}_LIBRARY)
endforeach ()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg DEFAULT_MSG ${_FFmpeg_REQUIRED_VARS})

set(FFMPEG_LIBRARIES)
if(FFmpeg_FOUND)
  foreach(_component IN LISTS FFmpeg_FIND_COMPONENTS)
    if(FFmpeg_${_component}_FOUND)
      list(APPEND FFMPEG_LIBRARIES FFmpeg::SDL::${_component})
      if(NOT TARGET FFmpeg::SDL::${_component})
        add_library(FFmpeg::SDL::${_component} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::SDL::${_component} PROPERTIES
          IMPORTED_LOCATION "${FFmpeg_${_component}_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_${_component}_INCLUDE_DIRS}"
          INTERFACE_COMPILE_OPTIONS "${FFmpeg_${_component}_COMPILE_OPTIONS}"
          INTERFACE_LINK_LIBRARIES "${FFmpeg_${_component}_LIBRARIES}"
          INTERFACE_LINK_OPTIONS "${FFmpeg_${_component}_LINK_OPTIONS}"
          INTERFACE_LINK_DIRECTORIES "${FFmpeg_${_component}_LINK_DIRECTORIES}"
        )
      endif()
    endif()
  endforeach()
endif()
