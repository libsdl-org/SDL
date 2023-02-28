include(FeatureSummary)
set_package_properties(RPi_BrcmEGL PROPERTIES
    URL "https://github.com/raspberrypi/firmware"
    DESCRIPTION "Fake brcmEGL package for RPi"
)

set(RPi_BrcmEGL_PKG_CONFIG_SPEC brcmegl)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_RPi_BrcmEGL QUIET ${RPi_BrcmEGL_PKG_CONFIG_SPEC})

find_package(RPi_BcmHost)

find_library(RPi_BrcmEGL_brcmEGL_LIBRARY
    NAMES brcmEGL
    HINTS
        ${PC_RPi_BrcmEGL_LIBRARY_DIRS}
        /opt/vc/lib
)

find_path(RPi_BrcmEGL_EGL_eglplatform_h_PATH
    NAMES EGL/eglplatform.h
    HINTS
        ${PC_RPi_BrcmEGL_INCLUDE_DIRS}
        /opt/vc/include
)

if(PC_RPi_BrcmEGL_FOUND)
    include("${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake")
    get_flags_from_pkg_config("${RPi_BrcmEGL_brcmEGL_LIBRARY}" "PC_RPi_BrcmEGL" "_RPi_BrcmEGL")
endif()

set(RPi_BrcmEGL_INCLUDE_DIRS "${_RPi_BrcmEGL_include_dirs}" CACHE STRING "Extra include dirs of brcmEGL")

set(RPi_BrcmEGL_COMPILE_OPTIONS "${_RPi_BrcmEGL_compile_options}" CACHE STRING "Extra compile options of brcmEGL")

set(RPi_BrcmEGL_LINK_LIBRARIES "${_RPi_BrcmEGL_link_libraries}" CACHE STRING "Extra link libraries of brcmEGL")

set(RPi_BrcmEGL_LINK_OPTIONS "${_RPi_BrcmEGL_link_options}" CACHE STRING "Extra link flags of brcmEGL")

set(RPi_BrcmEGL_LINK_DIRECTORIES "${_RPi_BrcmEGL_link_directories}" CACHE PATH "Extra link directories of brcmEGL")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RPi_BrcmEGL
    REQUIRED_VARS RPi_BrcmEGL_brcmEGL_LIBRARY RPi_BrcmEGL_EGL_eglext_brcm_h_PATH RPi_BcmHost_FOUND
)

if(RPi_BrcmEGL_FOUND)
    if(NOT TARGET RPi_BcmHost::RPi_BcmHost)
        add_library(RPi_BcmHost::RPi_BcmHost INTERFACE IMPORTED)
        set_target_properties(RPi_BcmHost::RPi_BcmHost PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${RPi_BrcmEGL_INCLUDE_DIRS}"
            INTERFACE_COMPILE_OPTIONS "${RPi_BrcmEGL_COMPILE_OPTIONS}"
            INTERFACE_LINK_LIBRARIES "${RPi_BrcmEGL_LINK_LIBRARIES};RPi_BcmHost::RPi_BcmHost"
            INTERFACE_LINK_OPTIONS "${RPi_BrcmEGL_LINK_OPTIONS}"
            INTERFACE_LINK_DIRECTORIES "${RPi_BrcmEGL_LINK_DIRECTORIES}"
        )
    endif()
endif()
