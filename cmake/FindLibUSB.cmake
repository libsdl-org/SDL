include(FindPackageHandleStandardArgs)

set(LibUSB_PKG_CONFIG_SPEC libusb-1.0>=1.0.16)
set(LibUSB_MIN_API_VERSION 0x01000102)

find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LibUSB ${LibUSB_PKG_CONFIG_SPEC})
endif()

find_library(LibUSB_LIBRARY
  NAMES usb-1.0 libusb-1.0
  HINTS ${PC_LibUSB_LIBRARY_DIRS}
)

find_path(LibUSB_INCLUDE_PATH
  NAMES libusb.h
  PATH_SUFFIXES libusb-1.0
  HINTS ${PC_LibUSB_INCLUDE_DIRS}
)

set(LibUSB_API_VERSION "LibUSB_API_VERSION-NOTFOUND")
if(LibUSB_INCLUDE_PATH AND EXISTS "${LibUSB_INCLUDE_PATH}/libusb.h")
  file(READ "${LibUSB_INCLUDE_PATH}/libusb.h" LIBUSB_H_TEXT)
  if("${LIBUSB_H_TEXT}" MATCHES "#define[ \t]+LIBUSBX?_API_VERSION[ \t]+(0x[0-9a-fA-F]+)" )
    set(LibUSB_API_VERSION "${CMAKE_MATCH_1}")
  endif()
endif()

if(LibUSB_API_VERSION)
  math(EXPR LibUSB_MIN_API_VERSION_decimal "${LibUSB_MIN_API_VERSION}")
  math(EXPR LibUSB_API_VERSION_decimal "${LibUSB_API_VERSION}")
  if(NOT LibUSB_MIN_API_VERSION_decimal LESS_EQUAL LibUSB_API_VERSION_decimal)
    set(LibUSB_LIBRARY "LibUSB_LIBRARY-NOTFOUND")
  endif()
else()
  set(LibUSB_LIBRARY "LibUSB_LIBRARY-NOTFOUND")
endif()

set(LibUSB_COMPILE_OPTIONS "" CACHE STRING "Extra compile options of LibUSB")

set(LibUSB_LINK_LIBRARIES "" CACHE STRING "Extra link libraries of LibUSB")

set(LibUSB_LINK_FLAGS "" CACHE STRING "Extra link flags of LibUSB")

if(LibUSB_LIBRARY AND LibUSB_INCLUDE_PATH)
  if(PC_LibUSB_FOUND)
    set(LibUSB_VERSION "${PC_LibUSB_VERSION}")
  else()
    set(LibUSB_VERSION "1.0.16-or-higher")
  endif()
else()
  set(LibUSB_VERSION "LibUSB_VERSION-NOTFOUND")
endif()

find_package_handle_standard_args(LibUSB
  VERSION_VAR LibUSB_VERSION
  REQUIRED_VARS LibUSB_LIBRARY LibUSB_INCLUDE_PATH
)

if(LibUSB_FOUND AND NOT TARGET LibUSB::LibUSB)
  add_library(LibUSB::LibUSB IMPORTED UNKNOWN)
  set_target_properties(LibUSB::LibUSB
    PROPERTIES
      IMPORTED_LOCATION "${LibUSB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LibUSB_INCLUDE_PATH}"
      INTERFACE_COMPILE_OPTIONS "${LibUSB_COMPILE_OPTIONS}"
      INTERFACE_LINK_LIBRARIES "${LibUSB_LINK_LIBRARIES}"
      INTERFACE_LINK_OPTIONS "${LibUSB_LINK_OPTIONS}"
  )
endif()

