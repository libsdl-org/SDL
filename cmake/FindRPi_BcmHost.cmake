include(FeatureSummary)
set_package_properties(RPi_BcmHost PROPERTIES
    URL "https://github.com/raspberrypi/firmware"
    DESCRIPTION "Broadcom VideoCore host API library"
)

set(RPi_BcmHost_PKG_CONFIG_SPEC bcm_host)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_RPi_BcmHost QUIET ${RPi_BcmHost_PKG_CONFIG_SPEC})

find_library(RPi_BcmHost_bcm_host_LIBRARY
    NAMES bcm_host
    HINTS
        ${PC_RPi_BcmHost_LIBRARY_DIRS}
        /opt/vc/lib
)

find_path(RPi_BcmHost_bcm_host_h_PATH
    NAMES bcm_host.h
    HINTS
        ${PC_RPi_BcmHost_INCLUDE_DIRS}
        /opt/vc/include
)

if(PC_RPi_BcmHost_FOUND)
    include("${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake")
    get_flags_from_pkg_config("${RPi_BcmHost_bcm_host_LIBRARY}" "PC_RPi_BcmHost" "_RPi_BcmHost")
else()
    set(_RPi_BcmHost_include_dirs
        /opt/vc/include
        /opt/vc/include/interface/vcos/pthreads
        /opt/vc/include/interface/vmcs_host/linux
    )
    set(_RPi_BcmHost_compile_options
        -DUSE_VCHIQ_ARM
    )
    set(_RPi_BcmHost_link_libraries
        -lvcos -lvchiq_arm
    )
    set(_RPi_BcmHost_link_options
        -pthread
    )
    set(_RPi_BcmHost_link_directories
        /opt/vc/lib
    )
endif()

set(RPi_BcmHost_INCLUDE_DIRS "${_RPi_BcmHost_include_dirs}" CACHE STRING "Extra include dirs of bcm_host")

set(RPi_BcmHost_COMPILE_OPTIONS "${_RPi_BcmHost_compile_options}" CACHE STRING "Extra compile options of bcm_host")

set(RPi_BcmHost_LINK_LIBRARIES "${_RPi_BcmHost_link_libraries}" CACHE STRING "Extra link libraries of bcm_host")

set(RPi_BcmHost_LINK_OPTIONS "${_RPi_BcmHost_link_options}" CACHE STRING "Extra link flags of bcm_host")

set(RPi_BcmHost_LINK_DIRECTORIES "${_RPi_BcmHost_link_directories}" CACHE PATH "Extra link directories of bcm_host")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RPi_BcmHost
    REQUIRED_VARS RPi_BcmHost_bcm_host_LIBRARY RPi_BcmHost_bcm_host_h_PATH
)

if(RPi_BcmHost_FOUND)
    if(NOT TARGET RPi_BcmHost::RPi_BcmHost)
        add_library(RPi_BcmHost::RPi_BcmHost INTERFACE IMPORTED)
        set_target_properties(RPi_BcmHost::RPi_BcmHost PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${RPi_BcmHost_INCLUDE_DIRS}"
            INTERFACE_COMPILE_OPTIONS "${RPi_BcmHost_COMPILE_OPTIONS}"
            INTERFACE_LINK_LIBRARIES "${RPi_BcmHost_LINK_LIBRARIES}"
            INTERFACE_LINK_OPTIONS "${RPi_BcmHost_LINK_OPTIONS}"
            INTERFACE_LINK_DIRECTORIES "${RPi_BcmHost_LINK_DIRECTORIES}"
          )
    endif()
endif()
