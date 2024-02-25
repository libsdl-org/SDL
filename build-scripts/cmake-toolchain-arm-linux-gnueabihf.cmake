set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

find_program(CMAKE_C_COMPILER NAMES arm-linux-gnueabihf-gcc)
find_program(CMAKE_CXX_COMPILER NAMES arm-linux-gnueabihf-g++)
find_program(CMAKE_RC_COMPILER NAMES arm-linux-gnueabihf-windres)

if(NOT CMAKE_C_COMPILER)
	message(FATAL_ERROR "Failed to find CMAKE_C_COMPILER.")
endif()

if(NOT CMAKE_CXX_COMPILER)
	message(FATAL_ERROR "Failed to find CMAKE_CXX_COMPILER.")
endif()

if(NOT CMAKE_RC_COMPILER)
        message(FATAL_ERROR "Failed to find CMAKE_RC_COMPILER.")
endif()
