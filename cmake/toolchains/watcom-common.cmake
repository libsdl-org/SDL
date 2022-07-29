set(WATCOM_ROOT "$ENV{WATCOM}" CACHE PATH "Root of WatCom compiler")
mark_as_advanced(WATCOM_ROOT)
if(NOT WATCOM_ROOT)
    message(FATAL_ERROR "WATCOM environment variable not set (saved in WATCOM_ROOT CMake variable)")
endif()

find_program(CMAKE_C_COMPILER NAMES wcl386)
find_program(CMAKE_CXX_COMPILER NAMES wcl386)

if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR "Could not find wcl386, make sure the directory containing wcl386 is added to your PATH")
endif()

set(WATCOM_EDPATH "${WATCOM_ROOT}/eddat")
set(WATCOM_WIPFC "${WATCOM_ROOT}/wipfc")
